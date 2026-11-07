// Hand-written observation plumbing; no lifted functions or generated source.
#include "aztec_rt.h"
#include "l0_observer.h"
#include "l0_pool.h"
#include "../src/parity/schedule.h"
#include <pthread.h>
#include <stdatomic.h>
#include <time.h>
#include <unistd.h>

static L0Schedule schedule;
static FILE* output;
static int initialized, pending, captured;
static uint32_t last_tick;
static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t ready = PTHREAD_COND_INITIALIZER;
static _Atomic int active;
static _Atomic uint32_t frozen_ms;
static _Atomic uint32_t latest_ms;
static _Atomic uint32_t paused_ms;
static char frame_path[1024];

static int owner_of(uint32_t tactic, uint32_t sides) {
  uint32_t side = tactic ? GU32(tactic + 8) : 0;
  return sides && side >= sides && (side - sides) / 0x69c < 4 && (side - sides) % 0x69c == 0
             ? (int)((side - sides) / 0x69c) : -1;
}

static int kind_of(uint32_t object) {
  uint32_t type = GU32(object + 0x50);
  if (type >= 90) rt_fatal("Invalid L0 object type");
  int kind = (int)GU32(0x4933f8 + type * 8);
  if (kind < 0 || kind > 11) rt_fatal("Invalid L0 object kind");
  return kind;
}

// Optional diagnostics must not prevent the base state from being recorded.
static void member_probe(uint32_t tactic, uint32_t object) {
  uint32_t address = GU32(tactic + 0xc);
  L0Pool members = {0};
  const char* error = l0_span(address, 0x28) ? l0_pool_try_read(address, &members) :
                                                           "Invalid L0 member pool span";
  uint32_t record = 0;
  if (!error && members.stride < 8 + 0x28) error = "Short L0 tactic member record";
  if (!error) {
    for (uint32_t i = 0; i < members.count; ++i) {
      uint32_t payload = members.nodes[i] + 8;
      if (GU32(payload) != object) continue;
      if (record) { error = "Duplicate L0 tactic member"; break; }
      record = payload;
    }
    if (!record && !error) error = "Missing L0 tactic member";
  }
  if (error)
    fprintf(output, ",\"member\":{\"unavailable\":\"%s\",\"raw\":{\"pool\":%u}}", error, address);
  else
    // Per-member tile and route bookkeeping are distinct from the tactic anchor.
    fprintf(output, ",\"member\":{\"tile\":[%d,%d],\"active\":%s,\"route_pending\":%s,\"route_count\":%u,\"route_cursor\":%u,\"search_counter\":%u}",
            (int32_t)GU32(record + 4), (int32_t)GU32(record + 8),
            GU8(record + 0x14) ? "true" : "false", GU8(record + 0x15) ? "true" : "false",
            GU32(record + 0x1c), GU32(record + 0x20), GU32(record + 0x24));
  free(members.nodes);
}

static int order_probe(uint32_t node, uint32_t stride) {
  uint32_t o = node + 8, tactic = GU32(o + 0x30), type = GU32(o + 0x50);
  int kind = type < 90 ? (int)GU32(0x4933f8 + type * 8) : -1;
  const char* error = NULL;
  if (kind < 0 || kind > 11) error = "Invalid L0 order object type or kind";
  else if (tactic && !l0_span(tactic, 0x44)) error = "Invalid L0 order tactic span";
  else if (kind == 0 && stride < 8 + 0xa0) error = "Short L0 order unit record";
  fprintf(output, "{\"handle\":%u,\"raw\":{\"type\":%u,\"kind\":%d,\"tactic\":%u}",
          node, type, kind, tactic);
  if (error) {
    fprintf(output, ",\"unavailable\":\"%s\"}", error);
    return 1;
  }
  fprintf(output, ",\"type\":%u,\"owner\":%d,\"kind\":%d,\"alive\":%s,\"tactic\":%u,\"tile\":[%d,%d]",
          type, owner_of(tactic, GU32(0x4dd2f4)), kind, GU8(o + 0x5c) ? "true" : "false", tactic,
          (int32_t)GU32(o + 0x44), (int32_t)GU32(o + 0x48));
  if (kind == 0) {
    uint32_t state = GU32(o + 0x64), target = GU32(o + 0x90), firing = GU32(o + 0x94);
    static const char* states[] = {"inactive", "idle", "walk", "attack", "death", "idle_animation"};
    // Native execution state and current step, not a reconstructed player command.
    // Marine Think uses +0x74/+0x78 in pixels and +0x98/+0x9c in pixel/256.
    fprintf(output, ",\"unit_state\":%d,\"attack_target_handle\":%u,\"firing_target_handle\":%u,\"pursuit_target_handle\":%u,\"step_destination\":[%d,%d],\"step_velocity\":[%d,%d],\"execution_kind\":\"%s\"",
            (int32_t)state, target ? target - 8 : 0, firing ? firing - 8 : 0,
            GU32(o + 0x8c) ? GU32(o + 0x8c) - 8 : 0,
            (int32_t)GU32(o + 0x74), (int32_t)GU32(o + 0x78),
            (int32_t)GU32(o + 0x98), (int32_t)GU32(o + 0x9c),
            state < 6 ? states[state] : "unknown");
    fprintf(output, ",\"formation_marked\":%s,\"idle_animation_timer\":%d",
            GU8(o + 0x5f) ? "true" : "false", (int32_t)GU32(o + 0x88));
    uint32_t descriptor = GU32(o + 0x68);
    if (l0_span(descriptor, 0x18))
      fprintf(output, ",\"descriptor_speed_px\":%d", (int32_t)GU32(descriptor + 0x14));
    else
      fputs(",\"descriptor_speed_px\":null", output);
  }
  if (tactic)
    fprintf(output, ",\"tactic_state\":%d,\"tactic_mode\":%d,\"anchor_tile\":[%d,%d],\"route_count\":%u,\"route_cursor\":%u",
            (int32_t)GU32(tactic + 0x20), (int32_t)GU32(tactic + 0x24),
            (int32_t)GU32(tactic + 0x10), (int32_t)GU32(tactic + 0x14),
            GU32(tactic + 0x3c), GU32(tactic + 0x40));
  if (tactic && kind == 0) member_probe(tactic, o);
  fputc('}', output);
  return 0;
}

static int queue_probe(void) {
  L0Pool towns = {0};
  const char* pool_error = l0_pool_try_read(0x5156f8, &towns);
  if (pool_error) {
    fprintf(output, "[{\"unavailable\":\"%s\",\"raw\":{\"pool\":%u,\"base\":%u,\"free_head\":%u,\"stride\":%u,\"count\":%u,\"bytes\":%u,\"sentinel\":%u}}]",
            pool_error, 0x5156f8, GU32(0x5156fc), GU32(0x515700), GU32(0x515708),
            GU32(0x515710), GU32(0x515714), GU32(0x51571c));
    return 1;
  }
  int unavailable = 0;
  fputs("[", output);
  for (uint32_t i = 0; i < towns.count; ++i) {
    uint32_t town = towns.nodes[i] + 8;
    fprintf(output, "%s{\"town_handle\":%u", i ? "," : "", towns.nodes[i]);
    if (towns.stride < 8 + 0x16c) {
      fprintf(output, ",\"unavailable\":\"Short L0 town record\",\"raw\":{\"stride\":%u}}", towns.stride);
      unavailable = 1;
      continue;
    }
    // Embedded ring object, not a pointer; see the inspection note in EXPERIMENTS.md.
    uint32_t q = town + 0xf8, vt = GU32(q);
    uint32_t data = GU32(q + 4), head = GU32(q + 8), tail = GU32(q + 0xc);
    uint32_t capacity = GU32(q + 0x10), count = GU32(q + 0x14);
    const char* error = NULL;
    if (vt != 0x48ff84) error = "Invalid L0 town queue vtable";
    else if (capacity != 50 || count > capacity || head >= capacity || tail >= capacity)
      error = "Invalid L0 town queue bounds";
    else if (!l0_span(data, capacity * 4)) error = "Invalid L0 town queue data span";
    fprintf(output, ",\"hall_handle\":%u,\"owner\":%d,\"production_object_handle\":%u,\"staged_handle\":%u,\"production_words\":[%u,%u,%u],\"raw\":{\"queue\":%u,\"vtable\":%u,\"data\":%u,\"head\":%u,\"tail\":%u,\"capacity\":%u,\"count\":%u,\"queued_objects\":[",
            GU32(town + 0xf0) ? GU32(town + 0xf0) - 8 : 0,
            owner_of(town, GU32(0x4dd2f4)), GU32(town + 0x114) ? GU32(town + 0x114) - 8 : 0,
            GU32(town + 0x168) ? GU32(town + 0x168) - 8 : 0,
            GU32(town + 0x118), GU32(town + 0x11c), GU32(town + 0x120),
            q, vt, data, head, tail, capacity, count);
    uint32_t objects[50], read_count = 0;
    if (!error) {
      for (uint32_t j = 0; j < count; ++j) {
        uint32_t object = GU32(data + ((head + j) % capacity) * 4);
        objects[read_count++] = object;
        fprintf(output, "%s%u", j ? "," : "", object);
        if (!l0_span(object, 0x54)) error = "Invalid L0 queued object";
        else if (GU32(object + 0x50) >= 90) error = "Invalid L0 queued object type";
      }
    }
    fputs("]}", output);
    if (error) {
      fprintf(output, ",\"unavailable\":\"%s\"}", error);
      unavailable = 1;
      continue;
    }
    fputs(",\"queued_handles\":[", output);
    for (uint32_t j = 0; j < read_count; ++j)
      fprintf(output, "%s%u", j ? "," : "", objects[j] - 8);
    fputs("],\"queued_types\":[", output);
    for (uint32_t j = 0; j < read_count; ++j)
      fprintf(output, "%s%u", j ? "," : "", GU32(objects[j] + 0x50));
    fputs("]}", output);
  }
  fputc(']', output);
  free(towns.nodes);
  return unavailable;
}

uint32_t rt_l0_clock(uint32_t wall_ms) {
  wall_ms -= atomic_load(&paused_ms);
  atomic_store(&latest_ms, wall_ms);
  uint32_t frozen = atomic_load(&frozen_ms);
  return frozen ? frozen : wall_ms;
}

void rt_l0_present(const void* pixels, int pitch, int width, int height, int bpp, const uint32_t* palette) {
  if (!atomic_load(&active)) return;
  pthread_mutex_lock(&lock);
  if (pending && !captured && width == 800 && height == 600 &&
      (bpp == 16 || bpp == 32 || (bpp == 8 && palette)) && pitch >= width * (bpp / 8)) {
    FILE* f = fopen(frame_path, "wb");
    if (!f) rt_fatal("L0 cannot open frame");
    fprintf(f, "P6\n800 600\n255\n");
    for (int y = 0; y < height; ++y) for (int x = 0; x < width; ++x) {
      const uint8_t* p = (const uint8_t*)pixels + y * pitch + x * (bpp / 8);
      uint32_t color = 0;
      if (bpp == 32) memcpy(&color, p, 4);
      else if (bpp == 8) color = palette[*p];
      else {
        uint16_t v;
        memcpy(&v, p, 2);
        unsigned r = (v >> 11) & 31, g = (v >> 5) & 63, b = v & 31;
        color = ((r << 3 | r >> 2) << 16) | ((g << 2 | g >> 4) << 8) | (b << 3 | b >> 2);
      }
      uint8_t rgb[3] = {color >> 16, color >> 8, color};
      if (fwrite(rgb, 1, 3, f) != 3) rt_fatal("L0 cannot write frame");
    }
    if (fclose(f)) rt_fatal("L0 cannot finish frame");
    captured = 1;
    pthread_cond_signal(&ready);
  }
  pthread_mutex_unlock(&lock);
}

void rt_l0_observe(uint32_t tick) {
  if (!initialized) {
    initialized = 1;
    const char* path = getenv("RT_L0_SCHEDULE");
    if (!path) return;
    if (!l0_schedule_read(path, &schedule)) rt_fatal("Invalid L0 schedule");
    path = getenv("RT_L0_OUTPUT");
    if (!path || !(output = fopen(path, "w"))) rt_fatal("Cannot open L0 observations");
    if (!getenv("RT_DUMP_DIR")) rt_fatal("L0 requires RT_DUMP_DIR");
    atomic_store(&active, 1);
    fputs("{\"schema\":2,\"phase\":\"before_commands\",\"position_unit\":\"pixel/256\"}\n", output);
  }
  if (!output || tick == last_tick) return;
  last_tick = tick;
  if (schedule.next == schedule.count) { fflush(NULL); _exit(0); }
  if (tick < schedule.ticks[schedule.next]) return;
  if (tick != schedule.ticks[schedule.next]) rt_fatal("L0 skipped requested tick");
  // Stop the logic thread at its command boundary. The renderer may still present.
  uint32_t clock = atomic_load(&latest_ms);
  if (!clock) rt_fatal("L0 clock has not started");
  atomic_store(&frozen_ms, clock);
  GU32(0x4dd330) = (uint32_t)schedule.x;
  GU32(0x4dd334) = (uint32_t)schedule.y;
  pthread_mutex_lock(&lock);
  int n = snprintf(frame_path, sizeof frame_path, "%s/tick_%u.ppm", getenv("RT_DUMP_DIR"), tick);
  if (n < 0 || (size_t)n >= sizeof frame_path) rt_fatal("L0 frame path too long");
  pending = 1; captured = 0;
  struct timespec until;
  clock_gettime(CLOCK_REALTIME, &until);
  until.tv_sec += 5;
  while (!captured) {
    if (pthread_cond_timedwait(&ready, &lock, &until)) break;
  }
  pending = 0;
  int have_frame = captured;
  pthread_mutex_unlock(&lock);
  fprintf(output, "{\"tick\":%u,\"objects\":[", tick);
  L0Pool pool = l0_pool_read(0x4cc3d0);
  uint32_t sides = GU32(0x4dd2f4), inactive = 0, live = 0;
  int first = 1, fractions_known = 1;
  for (uint32_t i = 0; i < pool.count; ++i) {
    uint32_t a = pool.nodes[i];
    uint32_t o = a + 8, vt = GU32(o);
    if (pool.stride < 0xb0 || vt < 0x480000 || vt >= 0x4c4000) rt_fatal("Invalid L0 active object");
    if (!GU8(o + 0x5c)) { ++inactive; continue; }
    ++live;
    int owner = owner_of(GU32(o + 0x30), sides), kind = kind_of(o);
    long long x = (long long)(int32_t)GU32(o + 0x24) * 256;
    long long y = (long long)(int32_t)GU32(o + 0x28) * 256;
    // Class-specific current coordinates, distinct from step destinations.
    static const unsigned positions[12] = {0x6c, 0x78, 0, 0x68, 0x6c, 0x68, 0x70, 0, 0, 0, 0, 0};
    if (positions[kind]) {
      x = (int32_t)GU32(o + positions[kind]); y = (int32_t)GU32(o + positions[kind] + 4);
    } else if (kind != 2 && kind != 7) fractions_known = 0;
    fprintf(output, "%s{\"handle\":%u,\"type\":%d,\"owner\":%d,\"x\":%lld,\"y\":%lld,\"hp\":%d,\"orders\":null}",
            first ? "" : ",", a, (int)GU32(o + 0x50), owner, x, y, (int)GU32(o + 0x38));
    first = 0;
  }
  fputs("],\"money\":{", output);
  if (!sides) rt_fatal("Missing L0 side storage");
  for (int i = 0; i < 4; ++i)
    fprintf(output, "%s\"%d\":%d", i ? "," : "", i, (int)GU32(sides + i * 0x69c + 0x698));
  fprintf(output, "},\"queues\":null,\"view\":null,\"frame\":");
  if (have_frame) fprintf(output, "\"reference/tick_%u.ppm\"", tick);
  else fputs("null", output);
  fprintf(output, ",\"diagnostics\":{\"pool\":{\"capacity\":%u,\"allocated\":%u,\"free_slot_filtered\":%u,\"inactive_object_filtered\":%u,\"live_objects\":%u},\"orders\":[",
          pool.capacity, pool.count, pool.capacity - pool.count, inactive, live);
  int orders_unavailable = 0;
  for (uint32_t i = 0; i < pool.count; ++i) {
    if (i) fputc(',', output);
    orders_unavailable |= order_probe(pool.nodes[i], pool.stride);
  }
  fputs("],\"queues\":", output);
  int queues_unavailable = queue_probe();
  fprintf(output, ",\"mission_seed\":%u,\"camera\":[%d,%d],\"viewport\":[%u,%u],\"local_side_selector\":%u,\"frame_phase\":\"%s\"},",
          GU32(0x4dd15c),
          (int32_t)GU32(0x4dd330), (int32_t)GU32(0x4dd334), GU32(0x4dd340), GU32(0x4dd344), GU32(0x4cc38c),
          have_frame ? "raw_primary_present" : "unavailable");
  // A unit step/anchor is not the logical command destination. Raw presents may be partial.
  fprintf(output, "\"unavailable\":[\"orders\",\"queues\",\"view\",\"frame_phase\"%s%s%s%s]}\n",
          fractions_known ? "" : ",\"position_fraction\"", have_frame ? "" : ",\"frame\"",
          orders_unavailable ? ",\"diagnostics.orders\"" : "",
          queues_unavailable ? ",\"diagnostics.queues\"" : "");
  free(pool.nodes);
  if (fflush(output) || ferror(output)) rt_fatal("L0 observation write failed");
  ++schedule.next;
  uint32_t frozen = atomic_load(&frozen_ms);
  atomic_fetch_add(&paused_ms, atomic_load(&latest_ms) - frozen);
  atomic_store(&frozen_ms, 0);
}
