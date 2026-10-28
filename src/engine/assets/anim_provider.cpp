#include "engine/assets/anim_provider.hpp"

#include <algorithm>
#include <charconv>
#include <fstream>
#include <sstream>
#include "core/loader_limits.hpp"
#include "core/mapfiles.hpp"

namespace aztec::engine {
namespace {
int kind_of(const ObjectState& st) {
  return st.type_id >= 0 && st.type_id < 90 ? data::kTypeAnims[st.type_id].kind : -1;
}
data::AnimState state_of(const ObjectState& st) {
  return st.anim_state >= 0 && st.anim_state <= int(data::AnimState::Construct)
      ? static_cast<data::AnimState>(st.anim_state) : data::AnimState::Idle;
}
int angle_of(const ObjectState& st) { return st.angle < 0 ? (st.facing & 7) * 32 : st.angle; }
}

data::SpriteRef AnimSpriteProvider::frame_for(const ObjectState& st) const {
  const int kind = kind_of(st);
  if (kind == 8 || st.type_id == -2)
    return data::SpriteDb::sprite_for_sequence(db_.anim(23),
        data::boat_sequence(static_cast<data::BoatPhase>(std::clamp(st.transport_phase, 0, 4)), st.orientation), int64_t(st.anim_tick));
  if (kind == 10 || st.type_id == -3)
    return data::SpriteDb::sprite_for_sequence(db_.anim(29),
        data::johnka_sequence(st.orientation), int64_t(st.anim_tick));
  if (kind < 0) return {};
  const auto state = state_of(st);
  auto a = db_.anim(db_.anim_id(st.type_id));
  if (kind == 2 || kind == 7) {
    if (state == data::AnimState::Die) {
      if (st.progress < 1000 || st.destruction_radius < 0) return {};
      return db_.anim(data::destruction_anim(st.destruction_radius)).at_tick(int(st.anim_variant % 20), int64_t(st.anim_tick), false);
    }
    if (state == data::AnimState::Construct || st.progress < 1000) {
      const int frame = data::construction_frame(a.frame_count(0), st.hp, st.max_hp);
      return frame < 0 ? data::SpriteRef{} : a.frame(0, frame);
    }
  }
  // Locomotion and built loops keep their phase when a weapon fires. Older
  // producers can still supply a single state clock through anim_tick.
  const int64_t body_tick = st.body_tick >= 0 ? st.body_tick : st.anim_tick;
  const int64_t attack_tick = st.attack_tick >= 0 ? st.attack_tick : st.anim_tick;
  auto body_state = state;
  if (state == data::AnimState::Attack || (st.attack_tick >= 0 &&
      (state == data::AnimState::Idle || state == data::AnimState::Move))) {
    body_state = st.moving ? data::AnimState::Move : data::AnimState::Idle;
    // Spider/Headtruck fire with the turret; aircraft fire while flying.
    if (kind == 0 || kind == 4 || kind == 9 ||
        (kind == 7 && !data::kTypeAnims[st.type_id].anim2)) {
      auto attack = data::body_sequence(kind, data::AnimState::Attack, angle_of(st));
      auto firing = data::SpriteDb::sprite_for_sequence(a, attack, attack_tick);
      if (firing.valid()) return firing;
    }
  }
  const bool aligned = st.cycle_ready && (angle_of(st) & 31) == 0;
  const auto sequence = data::body_sequence(kind, body_state, angle_of(st), int(st.anim_variant),
                                           aligned, data::kTypeAnims[st.type_id].anim2 != 0);
  const int64_t tick = body_state == data::AnimState::Idle || body_state == data::AnimState::Move
      ? body_tick : st.anim_tick;
  auto r = data::SpriteDb::sprite_for_sequence(a, sequence, tick);
  // Only Marine corpses hold their final death image. Never resurrect a vehicle
  // through an idle fallback when its death sequence is absent.
  if (!r.valid() && state == data::AnimState::Die && kind == 0) {
    r = a.frame(sequence.slot, a.frame_count(sequence.slot) - 1);
    r.sound_entry = false;
    return r;
  }
  if (!r.valid() && (state == data::AnimState::Fidget || (kind == 9 && body_state == data::AnimState::Move)))
    return data::SpriteDb::sprite_for_sequence(a, data::body_sequence(kind, data::AnimState::Idle,
        angle_of(st), 0, aligned, data::kTypeAnims[st.type_id].anim2 != 0), body_tick);
  return r;
}

std::vector<data::SpriteRef> AnimSpriteProvider::sound_frames(const ObjectState& st) const {
  auto body = frame_for(st);
  // Construction seeks an HP frame rather than entering a timed sequence.
  if (st.progress < 1000) body.sound_entry = false;
  std::vector<data::SpriteRef> frames{body};
  const int kind = kind_of(st);
  if (kind < 0 || state_of(st) == data::AnimState::Die || st.progress < 1000) return frames;
  const int second = db_.anim_id(st.type_id, 1);
  if (second) {
    const bool firing = state_of(st) == data::AnimState::Attack || (st.attack_tick >= 0 &&
        (state_of(st) == data::AnimState::Idle || state_of(st) == data::AnimState::Move));
    auto seq = data::turret_sequence(firing ? data::AnimState::Attack : state_of(st),
        st.secondary_angle < 0 ? angle_of(st) : st.secondary_angle, st.initial_turret);
    frames.push_back(data::SpriteDb::sprite_for_sequence(db_.anim(second), seq,
        firing && st.attack_tick >= 0 ? st.attack_tick : st.anim_tick));
  }
  return frames;
}

int AnimSpriteProvider::height(const ObjectState& st) const {
  const int kind = kind_of(st);
  if (kind == 3 || kind == 5) return st.height;
  if (kind == 1 || kind == 8 || kind == 10 || st.type_id < 0) return 0;
  return frame_for(st).yoff;
}
bool AnimSpriteProvider::hides_missing(const ObjectState& st) const {
  return kind_of(st) >= 0 || st.type_id == -2 || st.type_id == -3;
}

bool AnimSpriteProvider::make(const ObjectState& st, SpriteDraw& out, bool shadow) const {
  const auto r = frame_for(st);
  if (!r.valid()) return false;
  out = SpriteDraw{};
  out.key.group = shadow ? data::kShadowGroup : data::kBodyGroup;
  out.key.index = uint32_t(r.sprite);
  out.key.team = uint8_t(st.side_color & 7);
  out.key.shadow = shadow;
  if (shadow) out.key.shade = data::kShadowShade;
  out.dy = shadow ? 0 : -height(st);
  return true;
}

std::vector<SpriteDraw> AnimSpriteProvider::parts(const ObjectState& st) const {
  std::vector<SpriteDraw> result;
  auto add = [&](data::SpriteRef frame, int h, int dy = 0) {
    if (!frame.valid()) return;
    for (bool shadow : {true, false}) {
      SpriteDraw d;
      d.key.group = shadow ? data::kShadowGroup : data::kBodyGroup;
      d.key.index = uint32_t(frame.sprite);
      d.key.team = uint8_t(st.side_color & 7);
      d.key.shadow = shadow;
      if (shadow) d.key.shade = data::kShadowShade;
      d.dy = dy - (shadow ? 0 : h);
      result.push_back(d);
    }
  };
  const int kind = kind_of(st);
  add(frame_for(st), height(st));
  if (kind < 0) return result;
  const auto state = state_of(st);
  if (state == data::AnimState::Die) return result;
  const int second = db_.anim_id(st.type_id, 1);
  if (second && st.progress >= 1000) {
    const int angle = st.secondary_angle < 0 ? angle_of(st) : st.secondary_angle;
    const bool firing = state == data::AnimState::Attack || (st.attack_tick >= 0 &&
        (state == data::AnimState::Idle || state == data::AnimState::Move));
    const auto seq = data::turret_sequence(firing ? data::AnimState::Attack : state, angle, st.initial_turret);
    const int64_t tick = st.attack_tick >= 0 ? st.attack_tick : st.anim_tick;
    auto r = data::SpriteDb::sprite_for_sequence(db_.anim(second), seq, tick);
    if (!r.valid() && firing)
      r = data::SpriteDb::sprite_for_sequence(db_.anim(second), data::turret_sequence(data::AnimState::Idle, angle), 0);
    add(r, height(st));
  }
  if ((kind == 2 || kind == 7) && st.construction_overlay) {
    const auto frame = db_.anim(db_.anim_id(st.type_id)).frame(6, 0);
    if (frame.valid()) {
      SpriteDraw d;
      d.key.index = uint32_t(frame.sprite); d.key.team = uint8_t(st.side_color & 7);
      d.dy = -height(st); result.push_back(d);
    }
  }
  return result;
}

std::vector<SpriteItem> AnimSpriteProvider::items(const ObjectState& st) const {
  std::vector<SpriteItem> result{{0, 0, height(st), parts(st), light_mask(st)}};
  auto decoration = [&](int anim, int slot, int h, int dy, bool loop) {
    const auto frame = db_.anim(anim).at_tick(slot, int64_t(st.anim_tick), loop);
    if (!frame.valid()) return;
    SpriteItem item;
    item.height = h; item.dy = dy; item.light = frame.light;
    for (bool shadow : {true, false}) {
      SpriteDraw d;
      d.key.group = shadow ? data::kShadowGroup : data::kBodyGroup;
      d.key.index = uint32_t(frame.sprite); d.key.team = uint8_t(st.side_color & 7);
      d.key.shadow = shadow;
      if (shadow) d.key.shade = data::kShadowShade;
      d.dy = shadow ? 0 : -h;
      item.parts.push_back(d);
    }
    result.push_back(item);
  };
  const int kind = kind_of(st);
  if (state_of(st) == data::AnimState::Die) {
    if (kind == 1 || kind == 3 || kind == 4 || kind == 5 || kind == 6 || kind == 9) {
      decoration(152, int(st.anim_variant % 3), st.height, 0, false);
      if (kind == 1 || kind == 4 || kind == 6 || kind == 9)
        decoration(113, int(st.anim_variant % 20), -1, 0, false);
    }
  } else if ((kind == 2 || kind == 7) && st.progress >= 1000) {
    const int fire = data::damage_fire_anim(st.hp, st.max_hp);
    if (fire) decoration(fire, 0, 0, 1, true);
  }
  return result;
}

// Descriptor columns are factual metadata (rules_types.md). No light-mask
// pixels or executable instructions are stored here.
bool AnimSpriteProvider::load_visual_rules(const std::string& directory) {
  static const char* names[] = {"marine", "spider", "building", "zeppelin", "truck", "bird", "headtruck", "defence", "", "cannon"};
  static const int columns[] = {6, 8, 55, 7, 6, 8, 8, 58, -1, 6};
  std::fill(std::begin(vision_), std::end(vision_), 0);
  bool loaded = false;
  for (int kind = 0; kind < 10; ++kind) {
    if (columns[kind] < 0) continue;
    std::vector<uint8_t> bytes;
    if (!read_file(directory + "/desc_" + names[kind] + ".csv", bytes) || bytes.size() > detail::kTextBytes) continue;
    std::istringstream input(std::string(bytes.begin(), bytes.end()));
    std::string line;
    std::getline(input, line);
    while (std::getline(input, line)) {
      std::istringstream row(line);
      std::string cell; int param = -1, mask = 0;
      for (int col = 0; std::getline(row, cell, ','); ++col) {
        int value = 0;
        auto parsed = std::from_chars(cell.data(), cell.data() + cell.size(), value);
        if (parsed.ec != std::errc{}) continue;
        if (col == 0) param = value;
        if (col == columns[kind] + 1) mask = value;
      }
      if (mask < data::kFirstVisionMask || mask > data::kLastVisionMask) continue;
      for (int type = 0; type < 90; ++type)
        if (data::kTypeAnims[type].kind == kind && data::kTypeAnims[type].param == param) {
          vision_[type] = mask; loaded = true;
        }
    }
  }
  return loaded;
}
int AnimSpriteProvider::vision_mask(const ObjectState& st) const {
  if (kind_of(st) == 8 || kind_of(st) == 10 || st.type_id == -2 || st.type_id == -3)
    return data::kTransportVisionMask;
  return st.type_id >= 0 && st.type_id < 90 ? vision_[st.type_id] : 0;
}
}  // namespace aztec::engine
