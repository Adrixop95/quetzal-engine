// Invented banks and animation records exercise the same dispatcher as the app.
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include "temp_dir.hpp"
#include "engine/audio/scene_audio.hpp"
#include "engine/sim/game_sim.hpp"
#include "core/endian.hpp"
#include <limits>
#include <array>

using namespace aztec;
using namespace aztec::engine;
using Bytes = std::vector<uint8_t>;
static int failures = 0;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL %d: %s\n", __LINE__, #c); ++failures; } } while (0)
static void put(Bytes& b, size_t pos, uint32_t v) { write_le(b.data() + pos, v); }
static void write(const std::filesystem::path& path, const Bytes& b) {
  std::ofstream f(path, std::ios::binary); f.write(reinterpret_cast<const char*>(b.data()), b.size());
}
static Bytes wav(int frames = 1) {
  Bytes b(44 + frames * 4);
  std::memcpy(b.data(), "RIFF", 4); put(b, 4, uint32_t(b.size() - 8));
  std::memcpy(b.data() + 8, "WAVEfmt ", 8); put(b, 16, 16);
  b[20] = 1; b[22] = 2; put(b, 24, 44100); put(b, 28, 176400);
  b[32] = 4; b[34] = 16;
  std::memcpy(b.data() + 36, "data", 4); put(b, 40, frames * 4);
  for (int i = 0; i < frames; ++i) { b[44 + i * 4] = 100; b[46 + i * 4] = 100; }
  return b;
}
static Bytes archive(const std::vector<std::pair<int, std::vector<Bytes>>>& groups) {
  size_t table = 4 + groups.size() * 16;
  for (const auto& g : groups) table += g.second.size() * 4;
  Bytes b(table);
  put(b, 0, groups.size()); size_t cursor = 4 + groups.size() * 16;
  for (size_t i = 0; i < groups.size(); ++i) {
    put(b, 4 + i * 16, groups[i].first); put(b, 8 + i * 16, cursor);
    put(b, 12 + i * 16, 1); put(b, 16 + i * 16, groups[i].second.size() + 1);
    for (const auto& item : groups[i].second) {
      put(b, cursor, b.size()); cursor += 4;
      b.insert(b.end(), item.begin(), item.end());
    }
  }
  return b;
}
static Bytes animation() {
  Bytes b(0x208 + 8 * 32);
  std::memcpy(b.data(), "Anim", 4); put(b, 4, 3);
  // Slot 1: two frames sharing an image, then a hold. Header loop set 2.
  put(b, 12, 1); put(b, 0x208 + 8, 2);
  for (int i = 1; i <= 3; ++i) {
    put(b, 0x208 + i * 32, 10);
    put(b, 0x208 + i * 32 + 8, 1);
    put(b, 0x208 + i * 32 + 28, i == 3 ? 0 : 2);
  }
  put(b, 8, 1); // construction slot
  // Marine attack/death slots and a separate turret attack slot.
  put(b, 8 + 9 * 4, 1); put(b, 8 + 17 * 4, 1); put(b, 8 + 2 * 4, 1);
  return b;
}
int main(int argc, char** argv) {
  CHECK(random_pitch_rate(0) == 49152 && random_pitch_rate(32767) == 81920);
  {
    Mixer pitched; pitched.sfx_volume = 1;
    auto ramp = std::make_shared<Pcm>(Pcm{0,0, 4000,4000, 8000,8000, 12000,12000});
    CHECK(pitched.play(ramp, 1, 1, false, false, true, random_pitch_rate(32767)) != 0);
    int16_t output[8]; pitched.mix(output, 4);
    CHECK(output[0] == 0 && output[2] == 5000 && output[4] == 10000 && output[6] == 12000);
    CHECK(pitched.active_voices() == 0);
    CHECK(pitched.play(ramp, 1, 1, true, false, true, random_pitch_rate(0)) != 0);
    pitched.mix(output, 4);
    CHECK(output[0] == 0 && output[2] == 3000 && output[4] == 6000 && output[6] == 9000);
    CHECK(pitched.active_voices() == 1);
  }
  TempDir directory;
  const auto& root = directory.path();
  std::filesystem::create_directory(root / "Res");
  std::filesystem::create_directory(root / "Music");
  Bytes set(4); put(set, 0, 1);
  std::vector<Bytes> sets(401);
  for (int id : {1, 2, 3, 4, 101, 103, 104, 107, 108, 150, 151, 152, 153, 200, 201, 301, 401}) sets[id - 1] = set;
  put(sets[3], 0, 3); // distinct invented speech member, long enough to stay active across updates
  write(root / "Res/Snd.xbe", archive({{0x460, {wav(), Bytes{}, wav(8)}}, {0x3e8, sets},
      {0x6, {Bytes{'R', 'e', 'a', 'd', 'y', 0}, Bytes{'N', 'o', ' ', 'W', 'A', 'V', 0}}}}));
  write(root / "Music/music.xbe", archive({{0x460, {wav(), wav(), wav(8)}}}));
  std::vector<Bytes> animations(200);
  animations[3] = animation();   // Footman anim 4
  animations[13] = animation();  // Headtruck body anim 14
  animations[50] = animation();  // Headtruck turret anim 51
  animations[115] = animation(); // destruction anim 116
  animations[199] = animation(); // building anim 200
  // Decorations use a distinct invented set so omitted cues cannot hide behind body cues.
  auto decoration = animation();
  put(decoration, 0x208 + 8, 3);
  for (int i = 1; i <= 3; ++i) put(decoration, 0x208 + i * 32 + 8, 3);
  for (int anim : {113, 152, 154, 155, 156}) animations[anim - 1] = decoration;
  // Variant 1 of the first death decoration ends before the second one holds.
  put(animations[151], 12, 5);
  put(animations[151], 0x208 + 4 * 32 + 8, 3);
  for (int i : {5, 6}) {
    put(animations[151], 0x208 + i * 32, 10);
    put(animations[151], 0x208 + i * 32 + 8, 3);
    put(animations[151], 0x208 + i * 32 + 28, 2);
  }
  write(root / "Res/Units.xbe", archive({{0x515, animations}}));
  auto timed_bytes = animation();
  put(timed_bytes, 0x208 + 3 * 32, 0); // timed sequence terminator
  data::AnimSet timed({timed_bytes.data(), timed_bytes.size()});
  CHECK(timed.at_tick(1, 0, true).sound_entry);
  CHECK(!timed.at_tick(1, 1, true).sound_entry);
  CHECK(timed.at_tick(1, 4, true).sound_entry); // wrap to same image
  CHECK(!timed.at_tick(1, 4, false).valid());
  AnimSpriteProvider provider; CHECK(provider.open((root / "Res/Units.xbe").string()));
  Archive units; CHECK(units.open((root / "Res/Units.xbe").string()));
  AudioSystem audio; audio.event_fallbacks = true; CHECK(audio.init(root.string(), false)); CHECK(!audio.device_ok());
  std::vector<AudioEvent> events;
  audio.event_sink = [&](const AudioEvent& event) {
    events.push_back(event);
    std::printf("audio %llu %d %llu %u %u\n", static_cast<unsigned long long>(event.tick), int(event.kind),
        static_cast<unsigned long long>(event.source), event.set, event.sound);
  };
  World w; Object o; o.id = 1; o.type = 0; o.anim_state = int(data::AnimState::Move);
  o.hp = o.hp_max = 100; w.objects.push_back(o);
  bool visible = true;
  auto tick = [&](int age) {
    ++w.tick;
    if (!w.objects.empty()) w.objects[0].anim_tick = age;
    emit_scene_audio(w, provider, units, audio, 0, 0,
        [&](float, float, int) { return visible; });
  };
  tick(0); CHECK(events.size() == 2);
  CHECK(events[0].kind == AudioEvent::Kind::Sound && events[0].set == 1 && events[0].sound == 1);
  CHECK(events[0].trigger == AudioEvent::Trigger::Ambient);
  CHECK(events[1].kind == AudioEvent::Kind::LoopStart && events[1].set == 2);
  tick(1); CHECK(events.size() == 2);
  tick(2); CHECK(events.size() == 3 && events.back().tick == 3);  // same sprite, different frame
  tick(4); CHECK(events.size() == 4);
  tick(5); CHECK(events.size() == 4);  // held frame does not retrigger
  visible = false; tick(6); CHECK(events.back().kind == AudioEvent::Kind::LoopStop);
  CHECK(events.back().trigger == AudioEvent::Trigger::Ambient);
  visible = true; tick(7); CHECK(events.back().kind == AudioEvent::Kind::LoopStart);
  w.objects[0].cur_x = to_fixed(900); tick(8); CHECK(events.back().kind == AudioEvent::Kind::LoopStop);
  w.objects[0].cur_x = 0;
  w.objects[0].anim_state = int(data::AnimState::Attack); tick(0);
  CHECK(events[events.size() - 2].kind == AudioEvent::Kind::Sound);
  CHECK(events[events.size() - 2].trigger == AudioEvent::Trigger::Combat);
  w.objects[0].anim_state = int(data::AnimState::Die); tick(0);
  CHECK(events.back().kind == AudioEvent::Kind::Sound);
  w.objects[0].removed = true; tick(1); CHECK(events.back().kind == AudioEvent::Kind::LoopStop);
  w.objects[0].removed = false; w.objects[0].type = 3;
  w.objects[0].anim_state = int(data::AnimState::Attack); w.objects[0].attack_tick = 0;
  tick(1); CHECK(events[events.size() - 2].kind == AudioEvent::Kind::Sound); // turret
  w.objects[0].anim_state = int(data::AnimState::Move); tick(0);
  CHECK(events.back().trigger == AudioEvent::Trigger::Combat); // independently firing turret
  audio.stop_effects(); events.clear();
  w.objects[0].type = 10; w.objects[0].attack_tick = -1;
  w.objects[0].anim_state = int(data::AnimState::Construct);
  w.objects[0].progress = 500; w.objects[0].hp = 50;
  tick(1); CHECK(events.empty()); // construction seeks do not play frame cues
  w.objects[0].hp = 100; w.objects[0].progress = 1000;
  w.objects[0].anim_state = int(data::AnimState::Idle);
  tick(0); CHECK(events.size() == 2); // built sequence and its ambient header
  CHECK(events[0].trigger == AudioEvent::Trigger::Building);
  w.objects[0].anim_state = int(data::AnimState::Die);
  w.objects[0].destruction_radius = 90;
  tick(0); // destruction variant 0 needs slot 0
  CHECK(events.back().kind == AudioEvent::Kind::Sound);
  audio.stop_effects(); w.objects.clear(); events.clear();
  Effect effect; effect.anim = 4; effect.slot = 1; w.effects.push_back(effect);
  tick(0); CHECK(events.size() == 2); // effects use their own age clock
  CHECK(events[0].trigger == AudioEvent::Trigger::Unknown);
  w.effects[0].age = 1; tick(1); CHECK(events.size() == 2);
  w.effects[0].age = 2; tick(2); CHECK(events.size() == 3);
  w.effects.clear(); tick(3); CHECK(events.back().kind == AudioEvent::Kind::LoopStop);
  CHECK(events.back().trigger == AudioEvent::Trigger::Unknown);
  audio.stop_effects();
  // Damage fire and vehicle death decorations must enter the same event stream
  // as their visible frames. Export only this invented scenario for the L6 comparator.
  events.clear(); w = World{}; o = Object{};
  o.id = 1; o.type = 10; o.hp = 50; o.hp_max = 100; o.progress = 500;
  o.anim_state = int(data::AnimState::Construct); w.objects.push_back(o);
  w.tick = 299; visible = true;
  tick(0); CHECK(events.empty());
  w.objects[0].progress = 1000; w.objects[0].anim_state = int(data::AnimState::Idle);
  tick(0); CHECK(events.size() == 4);
  tick(1); CHECK(events.size() == 4);
  tick(2); CHECK(events.size() == 6);
  tick(4); CHECK(events.size() == 8);
  tick(5); CHECK(events.size() == 8);
  w.objects[0].hp = 100; tick(6); CHECK(events.size() == 9); // healed: stop only the fire loop
  w.objects[0].hp = 50; visible = false; tick(7); CHECK(events.size() == 10);
  visible = true; tick(7); CHECK(events.size() == 12); // restart loops, no historical frame cues
  w.objects[0].cur_x = to_fixed(900); tick(7); CHECK(events.size() == 14);
  w.objects[0].cur_x = 0; tick(7); CHECK(events.size() == 16);
  w.objects[0].removed = true; tick(7); CHECK(events.size() == 18);
  w.objects[0].removed = false; w.objects[0].type = 3;
  w.objects[0].anim_state = int(data::AnimState::Die);
  tick(0); CHECK(events.size() == 22); // two independently audible death decorations
  tick(1); CHECK(events.size() == 22);
  tick(2); CHECK(events.size() == 24);
  tick(4); CHECK(events.size() == 26);
  tick(5); CHECK(events.size() == 26);
  w.objects[0].removed = true; tick(6); CHECK(events.size() == 28);
  w.objects[0].removed = false; w.objects[0].anim_variant = 1;
  tick(0); CHECK(events.size() == 32);
  tick(1); CHECK(events.size() == 32);
  tick(2); CHECK(events.size() == 34);
  tick(4); CHECK(events.size() == 36);
  CHECK(events.back().kind == AudioEvent::Kind::LoopStop && events.back().source == (uint64_t(1) << 16 | 1));
  tick(5); CHECK(events.size() == 36); // the surviving loop must keep its own channel
  w.objects[0].removed = true; tick(6); CHECK(events.size() == 37);
  CHECK(events.back().kind == AudioEvent::Kind::LoopStop && events.back().source == (uint64_t(1) << 16 | 2));
  if (argc == 3 && std::string(argv[1]) == "--event-log") {
    FILE* log = std::fopen(argv[2], "w");
    CHECK(log != nullptr);
    if (log) {
      for (size_t i = 0; i < events.size(); ++i) write_audio_event(log, events[i], i);
      std::fclose(log);
    }
  }
  // A simulation-owned fire effect replaces the provider's fallback decoration.
  // The body still plays, but the fire must be requested exactly once.
  events.clear(); w.objects[0].removed = false; w.objects[0].type = 10;
  w.objects[0].anim_state = int(data::AnimState::Idle);
  Effect fire; fire.source = 1; fire.anim = data::damage_fire_anim(50, 100); fire.loop = true;
  w.effects.push_back(fire);
  tick(0); CHECK(events.size() == 4);
  CHECK(events.back().kind == AudioEvent::Kind::LoopStart && (events.back().source >> 63) == 1);
  w.effects[0].age = 1; tick(1); CHECK(events.size() == 4);
  w.effects.clear(); w.objects[0].removed = true; tick(2);
  CHECK(events.size() == 6 && events.back().kind == AudioEvent::Kind::LoopStop);
  events.clear(); audio.event_tick = 101; audio.start_music(0, 3, 99);
  CHECK(audio.current_track() == 1); CHECK(events.size() == 1);
  CHECK(events[0].tick == 101 && events[0].trigger == AudioEvent::Trigger::Music);
  int16_t samples[16]{}; audio.mixer().mix(samples, 8); audio.update();
  CHECK(audio.current_track() == 2);
  audio.mixer().mix(samples, 8); audio.update(); CHECK(audio.current_track() == 1);
  audio.start_music(2, 1, 0); CHECK(audio.current_track() == 1);
  audio.mixer().mix(samples, 8); audio.update(); CHECK(audio.current_track() == 2);
  audio.mixer().mix(samples, 8); audio.stop_music(); audio.update();
  CHECK(audio.current_track() == 0 && !audio.mixer().music_playing());
  audio.start_music(1, 0, 0); audio.start_music(0, 0, 0);
  audio.mixer().mix(samples, 8); audio.update();
  CHECK(audio.current_track() == 0 && !audio.mixer().music_playing());
  audio.stop_effects();
  // Explicit trigger context supports future selection/order/UI dispatchers;
  // these direct calls do not claim those app dispatchers exist.
  for (auto trigger : {AudioEvent::Trigger::Selection, AudioEvent::Trigger::Orders,
                       AudioEvent::Trigger::Production, AudioEvent::Trigger::UI}) {
    audio.event_trigger = trigger; audio.event_tick = 200;
    CHECK(audio.play_sound(1) == 1);
    CHECK(events.back().trigger == trigger && events.back().tick == 200);
  }
  FILE* trace = std::tmpfile();
  CHECK(trace != nullptr);
  if (trace) {
    write_audio_event(trace, events.back(), 7);
    std::rewind(trace);
    char line[512]{};
    CHECK(std::fgets(line, sizeof line, trace) != nullptr);
    CHECK(std::strstr(line, "\"seq\":7,\"tick\":200") != nullptr);
    CHECK(std::strstr(line, "\"trigger\":\"ui\"") != nullptr);
    CHECK(std::strstr(line, "\"sound\":1") != nullptr);
    std::fclose(trace);
  }
  // A loop with unknown provenance keeps that label when another context stops it.
  audio.event_trigger = AudioEvent::Trigger::Unknown;
  audio.loop_at(999, 1, 0, 0, 0, 0);
  audio.event_trigger = AudioEvent::Trigger::Combat;
  audio.stop_effects();
  CHECK(events.back().kind == AudioEvent::Kind::LoopStop && events.back().trigger == AudioEvent::Trigger::Unknown);
  audio.event_trigger = AudioEvent::Trigger::Unknown;
  auto pcm = std::make_shared<Pcm>(Pcm{100, 200});
  Mixer mixer; mixer.sfx_volume = 1;
  auto handle = mixer.play(pcm, 1, 1, true); mixer.mix(samples, 8);
  CHECK(samples[0] == 100 && samples[15] == 200 && mixer.active_voices() == 1);
  CHECK(mixer.gain(handle, 0, 1)); mixer.mix(samples, 8); CHECK(samples[0] == 0);
  mixer.stop(handle); CHECK(mixer.active_voices() == 0 && !mixer.gain(handle, 1, 1));
  // The shared budget includes protected speech and music. Saturation steals cyclically.
  {
    Mixer budget; budget.sfx_volume = budget.music_volume = budget.speech_volume = 1;
    auto long_pcm = std::make_shared<Pcm>(200, int16_t(100));
    auto short_pcm = std::make_shared<Pcm>(20, int16_t(10));
    CHECK(budget.play_music(long_pcm, long_pcm->data(), 100));
    const auto speech = budget.play(long_pcm, 1, 1, false, true);
    std::vector<uint64_t> effects;
    for (int i = 0; i < 6; ++i) effects.push_back(budget.play(i == 5 ? short_pcm : long_pcm, 1, 1));
    CHECK(budget.active_voices() == 7);
    CHECK(budget.play(long_pcm, 1, 1) != 0);
    CHECK(!budget.playing(effects.front()) && budget.playing(effects.back()));
    for (int i = 0; i < 20; ++i) CHECK(budget.play(long_pcm, 1, 1));
    CHECK(budget.playing(speech) && budget.music_playing() && budget.active_voices() == 7);
    budget.stop_effects(); CHECK(budget.music_playing());
    for (int i = 0; i < 7; ++i) CHECK(budget.play(long_pcm, 1, 1, false, true));
    CHECK(!budget.play(long_pcm, 1, 1) && budget.active_voices() == 7);
    budget.stop_music(); CHECK(budget.play(long_pcm, 1, 1));
    budget.stop_effects();
    CHECK(budget.play_music(long_pcm, long_pcm->data(), 1));
    budget.mix(samples, 1); CHECK(budget.music_ended && !budget.music_playing());
    for (int i = 0; i < 8; ++i) CHECK(budget.play(long_pcm, 1, 1, false, true));
    CHECK(!budget.play_music(long_pcm, long_pcm->data(), 1));
    budget.stop_effects();
    const auto casual = budget.play(long_pcm, 1, 1, false, true, false);
    for (int i = 0; i < 7; ++i) CHECK(budget.play(long_pcm, 1, 1, false, true));
    CHECK(budget.play(long_pcm, 1, 1) && !budget.playing(casual));
  }
  // Speech has its own gain, with captions for missing WAVs and no audio device.
  events.clear(); audio.event_tick = 42;
  audio.set_volumes(0, 0, 1);
  CHECK(audio.play_speech(1, true) == 1);
  CHECK(audio.subtitle() == "Ready" && events.size() == 2);
  CHECK(events[0].kind == AudioEvent::Kind::Subtitle && events[0].sound == 1 && events[0].tick == 42);
  CHECK(events[1].kind == AudioEvent::Kind::Speech && events[1].tick == 42);
  trace = std::tmpfile();
  CHECK(trace != nullptr);
  if (trace) {
    write_audio_event(trace, events[0], 0);
    write_audio_event(trace, events[1], 1);
    std::rewind(trace);
    char line[512]{};
    CHECK(std::fgets(line, sizeof line, trace) != nullptr);
    CHECK(std::strstr(line, "\"kind\":\"subtitle\"") != nullptr);
    CHECK(std::strstr(line, "\"tick\":42") != nullptr);
    CHECK(std::fgets(line, sizeof line, trace) != nullptr);
    CHECK(std::strstr(line, "\"kind\":\"speech\"") != nullptr);
    CHECK(std::strstr(line, "\"tick\":42") != nullptr);
    std::fclose(trace);
  }
  audio.mixer().mix(samples, 1); CHECK(samples[0] == 100);
  audio.acknowledgement = false; const auto event_count = events.size();
  CHECK(audio.play_speech(1, true) == 0 && events.size() == event_count);
  CHECK(audio.play_speech(2) == 0 && audio.subtitle() == "No WAV");
  CHECK(events.back().kind == AudioEvent::Kind::Subtitle);
  audio.update(5.9); CHECK(!audio.subtitle().empty());
  audio.update(0.1); CHECK(audio.subtitle().empty());
  audio.set_volumes(1, 0, 0); events.clear();
  CHECK(audio.play_speech(1) == 0 && audio.subtitle() == "Ready");
  audio.subtitles = false; audio.update(); CHECK(audio.subtitle().empty());
  CHECK(audio.play_speech(2) == 0 && audio.subtitle().empty());
  audio.subtitles = true; audio.set_volumes(0, 0, 1);
  CHECK(audio.play_speech(1) == 1); CHECK(audio.play_speech(1) == 1);
  CHECK(audio.mixer().active_voices() == 1); // a new utterance replaces the previous one
  audio.stop_effects(); CHECK(audio.subtitle().empty() && audio.mixer().active_voices() == 0);
  audio.set_volumes(-1, 2, std::numeric_limits<float>::quiet_NaN());
  CHECK(audio.mixer().sfx_volume == 0 && audio.mixer().music_volume == 1 && audio.mixer().speech_volume == 0);
  // Live controls change active voices, independently, without new playback requests.
  audio.set_volumes(1, 1, 1); events.clear();
  audio.event_trigger = AudioEvent::Trigger::Ambient;
  audio.loop_at(1001, 1, 0, 0, 0, 0);
  CHECK(audio.play_speech(3) == 3);
  audio.start_music(3, 0, 0);
  const auto live_events = events.size();
  audio.mixer().mix(samples, 1); CHECK(samples[0] == 300 && samples[1] == 300);
  audio.set_volumes(0, 0, 0);
  audio.mixer().mix(samples, 1); CHECK(samples[0] == 0 && samples[1] == 0);
  CHECK(audio.mixer().active_voices() == 2 && audio.mixer().music_playing());
  audio.set_volumes(0.5f, 0, 0);
  audio.mixer().mix(samples, 1); CHECK(samples[0] == 50 && samples[1] == 50);
  audio.set_volumes(0, 0.25f, 0);
  audio.mixer().mix(samples, 1); CHECK(samples[0] == 25 && samples[1] == 25);
  audio.set_volumes(0, 0, 0.75f);
  audio.mixer().mix(samples, 1); CHECK(samples[0] == 75 && samples[1] == 75);
  audio.set_volumes(1, 1, 1);
  audio.mixer().mix(samples, 1); CHECK(samples[0] == 300 && samples[1] == 300);
  CHECK(events.size() == live_events && audio.current_track() == 3);
  audio.stop_music(); audio.stop_effects();
  // Queue priority, acknowledgement suppression, provenance and scene cleanup, with no device.
  events.clear(); audio.acknowledgement = true; audio.subtitles = false;
  audio.event_trigger = AudioEvent::Trigger::Selection;
  CHECK(audio.queue_speech_set(4, true, 0, 1) == 3);
  audio.update(); CHECK(events.size() == 1 && events[0].set == 4);
  audio.update(); audio.mixer().mix(samples, 1);
  CHECK(samples[0] == 0 && samples[1] == 100); // updates must preserve speech pan
  audio.event_trigger = AudioEvent::Trigger::Production;
  CHECK(audio.queue_speech_set(1, false) == 1);
  audio.event_trigger = AudioEvent::Trigger::Orders;
  CHECK(audio.queue_speech_set(4, true, 1, 1, false) == 3);
  CHECK(audio.queue_speech_set(3, true) == 1); // replaces only the unprotected pending tail
  audio.update(); CHECK(events.size() == 1); // active utterance is retained
  audio.mixer().mix(samples, 8); audio.event_tick = 601; audio.update();
  CHECK(events.size() == 2 && events.back().trigger == AudioEvent::Trigger::Production && events.back().tick == 601);
  audio.mixer().mix(samples, 1); audio.update();
  CHECK(events.size() == 3 && events.back().set == 3 && events.back().trigger == AudioEvent::Trigger::Orders);
  audio.mixer().mix(samples, 1);
  CHECK(audio.queue_speech_set(1, true)); audio.acknowledgement = false; audio.update();
  CHECK(events.size() == 3 && !audio.queue_speech_set(1, true));
  CHECK(audio.queue_speech_set(1, false)); audio.stop_effects(); audio.update();
  CHECK(events.size() == 3);
  // Recording through the output converter exercises every supported sampling rate without SDL devices.
  audio.acknowledgement = true; events.clear();
  audio.loop_at(9001, 1, 0, 0, 0, 0);
  const auto before_sampling = events.size();
  std::vector<int16_t> recording(4096);
  for (int rate : {11025, 22050, 44100}) {
    CHECK(audio.set_sampling_rate(rate) && audio.sampling_rate() == rate && !audio.device_ok());
    audio.render_samples(recording.data(), 2048);
    CHECK(recording[recording.size() - 2] >= 98 && recording.back() <= 102);
    CHECK(audio.mixer().active_voices() == 1 && events.size() == before_sampling);
  }
  CHECK(!audio.set_sampling_rate(12345) && audio.sampling_rate() == 44100);
  audio.stop_effects();
  // Invented bank members behind real resource namespaces; no original audio or captions.
  w.mission.sides = {{}}; w.local_side = 0; w.selection.ids = {1};
  w.objects.clear(); Object selected; selected.id = 1; selected.type = 0; w.objects.push_back(selected);
  for (const auto [type, style, expected] : {std::array<int, 3>{0, 0, 101}, {27, 0, 401}, {53, 0, 301}, {53, 1, 101}, {0, 2, 401}, {27, 3, 301}}) {
    w.objects[0].type = type; events.clear();
    emit_selection_audio(w, audio, style, 0); audio.update();
    CHECK(events.size() == 1 && events[0].set == uint32_t(expected) && events[0].trigger == AudioEvent::Trigger::Selection);
    audio.stop_effects();
  }
  events.clear(); audio.acknowledgement = false;
  emit_selection_audio(w, audio, 0, 0); audio.update(); CHECK(events.empty());
  audio.acknowledgement = true; w.objects[0].side = 1;
  emit_selection_audio(w, audio, 0, 0); audio.update(); CHECK(events.empty());
  w.objects[0].side = 0; w.objects[0].type = 10; // a structure
  emit_selection_audio(w, audio, 0, 0); audio.update(); CHECK(events.empty());
  CHECK(selection_speech_set(-1, 0) == 0 && selection_speech_set(0, 4) == 0);
  CHECK(selection_speech_set(82, 0) == 101 && selection_speech_set(83, 0) == 101);
  CHECK(selection_speech_set(85, 0) == 101 && selection_speech_set(87, 0) == 401);
  // Rejected requests must not be reported as accepted playback or retained loops.
  audio.stop_effects(); events.clear();
  auto protected_pcm = std::make_shared<Pcm>(20, int16_t(100));
  for (int i = 0; i < 8; ++i) CHECK(audio.mixer().play(protected_pcm, 1, 1, false, true));
  CHECK(audio.play_sound(1) == 0 && audio.play_set_at(1, 0, 0, 0, 0) == 0);
  audio.loop_at(99, 1, 0, 0, 0, 0); audio.start_music(1, 0, 0);
  CHECK(events.empty() && audio.current_track() == 0);
  audio.stop_effects(); audio.end_scene_tick(); CHECK(events.empty());
  // Identical commands and seed must yield identical complete state hashes at every tick.
  // Exercise real movement and commands with dispatch disabled, muted and audible, changing views and mix cadence.
  auto rules = std::make_shared<RulesData>(); rules->init_defaults();
  const auto sound_root = root / "simulation-audio";
  std::filesystem::create_directories(sound_root / "Res");
  sets[0].resize(8); put(sets[0], 0, 1); put(sets[0], 4, 3);
  write(sound_root / "Res/Snd.xbe", archive({{0x460, {wav(), Bytes{}, wav(8)}}, {0x3e8, sets}}));
  std::filesystem::copy_file(root / "Res/Units.xbe", sound_root / "Res/Units.xbe");
  CHECK(rules->load_audio(sound_root.string()));
  {
    auto combat_rules = std::make_shared<RulesData>();
    CHECK(combat_rules->load_dir(AZTEC_TEST_RULES_DIR));
    CHECK(combat_rules->load_audio(sound_root.string()));
    for (int source : {1,69,0}) {
      World initial; initial.terrain.w = initial.terrain.h = 24; initial.terrain.cells.resize(24 * 24);
      initial.relation[0][1] = initial.relation[1][0] = kEnemy;
      Object attacker; attacker.id = 1; attacker.type = source; attacker.hp = 100000;
      attacker.teleport(to_fixed(400),to_fixed(400));
      Object victim = attacker; victim.id = 2; victim.type = source == 0 ? 82 : 0;
      victim.side = 1; victim.hp = source == 0 ? 1 : 100000;
      victim.cooldown = 10000; victim.attack_mode = AttackMode::Direct;
      victim.teleport(to_fixed(410),to_fixed(400));
      initial.objects = {attacker,victim};
      World live = initial; GameSim sim(combat_rules); sim.init(live);
      Command attack; attack.type = Pct::Attack; attack.ids = {1}; attack.a = 2; sim.submit(attack);
      live.begin_tick(); sim.tick(live);
      CHECK(live.combat_sounds.size() == 1);
      const uint32_t expected = source == 1 ? 200 : source == 69 ? 201 : 150;
      if (!live.combat_sounds.empty()) {
        CHECK(live.combat_sounds[0].set == expected && live.combat_sounds[0].sound == 1);
        CHECK(live.combat_sounds[0].x == to_fixed(source == 0 ? 410 : 400));
      }
      const auto random = sim.random_state();
      const auto hash = sim.state_hash(live);
      AudioSystem backend; CHECK(backend.init(sound_root.string(),false));
      std::vector<AudioEvent> resolved;
      backend.event_sink = [&](const auto& e) { resolved.push_back(e); };
      emit_scene_audio(live,provider,units,backend,400,400,[](float,float,int) { return false; });
      CHECK(resolved.empty());
      backend.mixer().sfx_volume = 0;
      emit_scene_audio(live,provider,units,backend,400,400,[](float,float,int) { return true; });
      CHECK(resolved.empty());
      backend.mixer().sfx_volume = 1;
      emit_scene_audio(live,provider,units,backend,400,400,[](float,float,int) { return true; });
      CHECK(std::count_if(resolved.begin(),resolved.end(),[&](const auto& e) {
        return e.set == expected && e.sound == 1 && e.trigger == AudioEvent::Trigger::Combat;
      }) == 1);
      CHECK(sim.random_state() == random && sim.state_hash(live) == hash);
      auto replay = GameSim::replay(initial,combat_rules,sim.log(),1);
      CHECK(replay->state_hash(initial) == hash);
      CHECK(initial.combat_sounds.size() == live.combat_sounds.size());
      Command pause; pause.type = Pct::PauseOn; sim.submit(pause); live.begin_tick(); sim.tick(live);
      CHECK(live.combat_sounds.empty() && sim.random_state() == random);
    }
    World scene; scene.terrain.w = scene.terrain.h = 16; scene.terrain.cells.resize(256);
    Object tower; tower.id = 1; tower.type = 80; tower.hp = combat_rules->type(80).hp_max / 2;
    tower.teleport(to_fixed(200),to_fixed(200)); scene.objects = {tower};
    GameSim sim(combat_rules); sim.init(scene); scene.begin_tick(); sim.tick(scene);
    CHECK(scene.combat_sounds.size() == 1 && scene.combat_sounds[0].sound == 1 && scene.combat_sounds[0].loop);
    AudioSystem backend; CHECK(backend.init(sound_root.string(),false));
    std::vector<AudioEvent> resolved;
    backend.event_sink = [&](const auto& e) { resolved.push_back(e); };
    auto present = [&]() { emit_scene_audio(scene,provider,units,backend,200,200,[](float,float,int) { return true; }); };
    present();
    CHECK(std::count_if(resolved.begin(),resolved.end(),[](const auto& e) {
      return e.kind == AudioEvent::Kind::LoopStart && e.set == 152;
    }) == 1);
    const auto random = sim.random_state();
    scene.begin_tick(); sim.tick(scene); present();
    CHECK(sim.random_state() == random); // a persistent stage keeps its chosen member
    CHECK(std::count_if(resolved.begin(),resolved.end(),[](const auto& e) {
      return e.kind == AudioEvent::Kind::LoopStart && e.set == 152;
    }) == 1);
    scene.objects[0].hp = scene.objects[0].hp_max; scene.begin_tick(); sim.tick(scene); present();
    CHECK(scene.combat_sounds.empty());
    CHECK(std::count_if(resolved.begin(),resolved.end(),[](const auto& e) {
      return e.kind == AudioEvent::Kind::LoopStop && e.set == 152;
    }) == 1);
  }
  {
    World scene; scene.terrain.w = scene.terrain.h = 4; scene.terrain.cells.resize(16);
    GameSim choices(rules); choices.init(scene);
    Effect cue; cue.anim = 200; cue.slot = 1; cue.lifetime = 10; cue.age = -1;
    scene.effects.push_back(cue);
    scene.begin_tick(); choices.tick(scene);
    CHECK(scene.sound_choices.size() == 2);
    if (scene.sound_choices.size() == 2) {
      CHECK(scene.sound_choices[0].set == 1 && scene.sound_choices[0].sound == 1 &&
            scene.sound_choices[0].pitch == 7719 && !scene.sound_choices[0].loop);
      CHECK(scene.sound_choices[1].set == 2 && scene.sound_choices[1].pitch == 2437 && scene.sound_choices[1].loop);
    }
    CHECK(choices.random_state() == 159719620u);
    AudioSystem backend; CHECK(backend.init(sound_root.string(), false));
    std::vector<AudioEvent> resolved;
    backend.event_sink = [&](const auto& e) { resolved.push_back(e); };
    emit_scene_audio(scene, provider, units, backend, 0, 0, [](float, float, int) { return true; });
    CHECK(resolved.size() == 2 && resolved[0].sound == 1); // local seed one would choose the other member
    CHECK(choices.random_state() == 159719620u);
    scene.begin_tick(); choices.tick(scene);
    CHECK(choices.random_state() == 159719620u && scene.sound_choices.size() == 1);
    Command pause; pause.type = Pct::PauseOn; choices.submit(pause);
    scene.begin_tick(); choices.tick(scene);
    CHECK(choices.random_state() == 159719620u && scene.sound_choices.size() == 1 && scene.sound_choices[0].loop);
  }
  World initial; initial.terrain.w = initial.terrain.h = 24; initial.terrain.cells.resize(24 * 24);
  initial.mission.sides.resize(1); initial.mission.sides[0].index = 0;
  initial.selection.ids = {1};
  Object unit; unit.id = 1; unit.type = 0; unit.hp = unit.hp_max = 100;
  unit.cur_x = unit.prev_x = to_fixed(100); unit.cur_y = unit.prev_y = to_fixed(100);
  initial.objects.push_back(unit);
  Object damaged = unit; damaged.id = 2; damaged.type = 10; damaged.hp = 50;
  initial.objects.push_back(damaged);
  Object dying = unit; dying.id = 3; dying.type = 3; dying.hp = 0; dying.alive = false;
  dying.anim_state = int(data::AnimState::Die);
  initial.objects.push_back(dying);
  World silent = initial, presented = initial;
  GameSim silent_sim(rules, Modifications::faithful()), presented_sim(rules, Modifications::faithful()); silent_sim.init(silent); presented_sim.init(presented);
  audio.event_sink = {}; audio.start_music(0, 3, 0);
  for (int t = 0; t < 1200; ++t) {
    if (t % 100 == 0) {
      Command command; command.type = Pct::Move; command.side = 0; command.ids = {1};
      command.a = 100 + (t % 300); command.b = 200;
      silent_sim.submit(command); presented_sim.submit(command);
    }
    silent.begin_tick(); presented.begin_tick(); silent_sim.tick(silent); presented_sim.tick(presented);
    CHECK(silent.simulation_sounds && presented.simulation_sounds);
    for (const auto& choice : silent.sound_choices) CHECK(choice.pitch <= 32767 && choice.sound != 0);
    audio.set_volumes(t % 2 ? 0 : 1, t % 3 ? 0 : 1, t % 5 ? 0 : 1);
    audio.acknowledgement = t % 7; audio.subtitles = t % 11;
    emit_scene_audio(presented, provider, units, audio, float(t % 1000), 0,
        [&](float, float, int) { return t % 4 != 0; });
    if (t % 13 == 0) emit_selection_audio(presented, audio, t % 4, 0);
    if (t % 200 == 0) CHECK(audio.set_sampling_rate(t % 400 ? 11025 : 22050));
    audio.render_samples(samples, t % 8 + 1); audio.update(1.0 / 12);
    CHECK(silent_sim.state_hash(silent) == presented_sim.state_hash(presented));
  }
  auto replayed = initial;
  auto replay_sim = GameSim::replay(replayed, rules, presented_sim.log(), presented_sim.tick_count());
  CHECK(replay_sim->state_hash(replayed) == presented_sim.state_hash(presented));
  audio.stop_music(); audio.stop_effects();
  CHECK(audio.set_sampling_rate(44100));
  // Music works even when the effects archive is absent.
  std::filesystem::remove(root / "Res/Snd.xbe");
  AudioSystem music_only; CHECK(!music_only.init(root.string(), false));
  music_only.start_music(2, 0, 0); CHECK(music_only.current_track() == 2);
  // Skip invalid and empty tracks, decoding little-endian WAV bytes into native samples.
  Bytes empty = wav(); empty.resize(44); put(empty, 4, 36); put(empty, 40, 0);
  Bytes signed_wav = wav();
  signed_wav[44] = 0x34; signed_wav[45] = 0x12; // left = 4660, right = -4660
  signed_wav[46] = 0xcc; signed_wav[47] = 0xed;
  write(root / "Music/music.xbe", archive({{0x460, {Bytes{0}, empty, signed_wav, wav()}}}));
  AudioSystem fallback; CHECK(!fallback.init(root.string(), false));
  std::vector<AudioEvent> music_events;
  fallback.event_sink = [&](const AudioEvent& e) { music_events.push_back(e); };
  fallback.set_volumes(1, 1);
  fallback.start_music(0, 15, 99);
  CHECK(fallback.current_track() == 3 && music_events.size() == 1);
  CHECK(music_events.back().kind == AudioEvent::Kind::Music && music_events.back().sound == 3);
  fallback.mixer().mix(samples, 8);
  CHECK(samples[0] == 4660 && samples[1] == -4660 && samples[2] == 0);
  fallback.update(); CHECK(fallback.current_track() == 4);
  fallback.mixer().mix(samples, 8); fallback.update();
  CHECK(fallback.current_track() == 3); // wrap and skip both invalid tracks again
  fallback.stop_music();
  CHECK(music_events.back().kind == AudioEvent::Kind::MusicStop && music_events.back().sound == 3);
  music_events.clear();
  fallback.start_music(0, 3, 0); // every allowed track is invalid
  CHECK(fallback.current_track() == 0 && !fallback.mixer().music_playing() && music_events.empty());
  fallback.start_music(1, 15, 0); // an exact invalid track must not select another
  CHECK(fallback.current_track() == 0 && music_events.empty());
  // Output-rate conversion preserves stream duration and channel independence.
  for (int rate : {11025, 22050, 44100}) {
    Mixer conversion; conversion.sfx_volume = conversion.music_volume = 1;
    const int factor = kMixRate / rate;
    auto tone = std::make_shared<Pcm>(80 * 2);
    for (int i = 0; i < 80; ++i) { (*tone)[i * 2] = 800; (*tone)[i * 2 + 1] = -400; }
    conversion.play(tone, 1, 1);
    conversion.play_music(tone, tone->data(), 80);
    std::vector<int16_t> converted(size_t(80 / factor) * 2);
    conversion.mix_at_rate(converted.data(), 80 / factor, rate);
    for (size_t i = 0; i < converted.size(); i += 2) CHECK(converted[i] == 1600 && converted[i + 1] == -800);
    CHECK(conversion.active_voices() == 0 && conversion.music_ended);
    conversion.mix_at_rate(converted.data(), 80 / factor, rate);
    for (auto value : converted) CHECK(value == 0);
  }
  // Invented set contents test Auto/explicit families without copying game resources.
  std::vector<Bytes> families(301);
  for (int family = 0; family < 3; ++family) { const int offset = family == 1 ? 300 : family == 2 ? 200 : 0; families[offset].resize(4); put(families[offset], 0, family + 1); }
  write(root / "Res/Snd.xbe", archive({{1000, families}, {0x460, {wav(8), wav(8), wav(8)}}}));
  AudioSystem voices; CHECK(voices.init(root.string(), false));
  voices.set_volumes(1, 1, 1);
  for (int race = 0; race < 3; ++race) {
    CHECK(voices.play_speech_set(1, race, 0, true) == uint32_t(race + 1));
    for (int style = 1; style <= 3; ++style)
      CHECK(voices.play_speech_set(1, race, style, true) == uint32_t(style));
  }
  voices.acknowledgement = false;
  CHECK(voices.play_speech_set(1, 0, 1, true) == 0);
  CHECK(voices.play_speech_set(1, 0, 1, false) == 1);
  CHECK(voices.play_speech_set(1, 0, 4, false) == 0);
  CHECK(voices.set_sampling_rate(11025) && voices.sampling_rate() == 11025);
  CHECK(!voices.set_sampling_rate(8000) && voices.sampling_rate() == 11025);
  // Dummy SDL really opens each requested device rate; active stream state survives reopen.
  {
    CHECK(SDL_setenv("SDL_AUDIODRIVER", "dummy", 1) == 0);
    AudioSystem device; device.init(root.string(), true, 22050);
    CHECK(device.device_ok());
    device.start_music(3, 0, 0);
    const auto track = device.current_track();
    for (int rate : {11025, 44100, 22050}) {
      CHECK(device.set_sampling_rate(rate) && device.device_ok() && device.sampling_rate() == rate);
      CHECK(device.current_track() == track);
    }
  }
  // Request flags and generation handles survive reuse without changing other voices.
  {
    Mixer flags;
    auto pcm = std::make_shared<Pcm>(200, int16_t(100));
    std::array<uint64_t, 8> handles{};
    for (auto& handle : handles) handle = flags.play(pcm, 1, 1, true, false, false, 65536, 0);
    CHECK(flags.active_voices() == 8);
    CHECK(flags.play(pcm, 1, 1, false, false, false, 65536, 0) == 0);
    const auto replacement = flags.play(pcm, 1, 1, true, false, false, 65536, 3);
    CHECK((replacement & 7) == (handles.front() & 7));
    CHECK(replacement == handles.front() + 8 && !flags.playing(handles.front()));
    flags.stop(handles.front()); CHECK(flags.playing(replacement));
    for (int i = 0; i < 32; ++i) CHECK(flags.play(pcm, 1, 1));
    CHECK(flags.playing(replacement)); // bit 1 protects effects too
    flags.stop_effects();
    for (int i = 0; i < 8; ++i) CHECK(flags.play(pcm, 1, 1, true, false, false, 65536, 2));
    CHECK(!flags.play(pcm, 1, 1));
  }
  // Original attenuation masters mute independently and apply logarithmic gain.
  audio.stop_music(); audio.stop_effects(); audio.set_attenuations(-2000, -4000, -4000);
  CHECK(audio.play_sound(1)); audio.render_samples(samples, 1); CHECK(samples[0] == 10);
  CHECK(!audio.play_speech(1));
  audio.stop_effects(); audio.set_attenuations(-4000, -4000, -4000);
  CHECK(!audio.play_sound(1) && !audio.play_set_at(1, 0, 0, 0, 0));
  audio.loop_at(78, 4, 0, 0, 0, 0);
  CHECK(audio.mixer().active_voices() == 0);
  audio.set_volumes(1, 1, 1);
  // The documented fog threshold uses brightness, independently of ownership/UI fog switches.
  World fog_world; fog_world.local_side = 0; fog_world.visibility.sides = {{2, 1, {32, 33}}};
  fog_world.visibility_brightness = true;
  CHECK(!audio_fog_visible(fog_world, 0, 0) && audio_fog_visible(fog_world, 48, 0));
  CHECK(!audio_fog_visible(fog_world, -1, 0) && !audio_fog_visible(fog_world, 96, 0));
  fog_world.visibility_brightness = false; fog_world.visibility.sides[0].v = {120, 119};
  CHECK(!audio_fog_visible(fog_world, 0, 0) && audio_fog_visible(fog_world, 48, 0));
  // Category gates precede queueing. Positional speech remains audible beyond 800 pixels.
  audio.stop_effects(); audio.event_fallbacks = false; audio.acknowledgement = true; audio.subtitles = true;
  audio.event_sink = [&](const AudioEvent& event) { events.push_back(event); };
  audio.set_volumes(1, 1, 1); events.clear();
  CHECK(!audio.queue_speech_at(4, 4, 1, 0, 0, 0, 0, 0, 100, 100));
  CHECK(!audio.queue_speech_at(4, 6, 0, 0, 99, 99, 0, 0, 100, 100));
  CHECK(!audio.queue_speech_at(4, 5, 0, 0, 900, 0, 0, 0, 100, 100, false));
  CHECK(audio.queue_speech_at(4, 5, 0, 0, 900, 0, 0, 0, 100, 100));
  audio.update(); audio.render_samples(samples, 1);
  CHECK(samples[0] > 0 && samples[1] > samples[0]);
  audio.stop_effects();
  CHECK(audio.queue_speech_at(4, 6, 0, 0, 100, 99, 0, 0, 100, 100));
  audio.update(); CHECK(audio.mixer().active_voices() == 1);
  audio.stop_effects(); audio.acknowledgement = false;
  CHECK(!audio.queue_speech_at(4, 4, 0, 0, 0, 0, 0, 0, 100, 100));
  audio.acknowledgement = true;
  // One-shot fog rejection does not kill an already playing ambient loop.
  audio.begin_scene_tick(); audio.loop_at(77, 4, 0, 0, 0, 0); audio.end_scene_tick();
  events.clear();
  audio.begin_scene_tick(); audio.loop_at(77, 4, 0, 0, 0, 0, 0, 65536, false); audio.end_scene_tick();
  CHECK(events.empty() && audio.mixer().active_voices() == 1);
  audio.stop_effects(); events.clear();
  audio.begin_scene_tick(); audio.loop_at(77, 4, 0, 0, 0, 0, 0, 65536, false); audio.end_scene_tick();
  CHECK(events.empty() && audio.mixer().active_voices() == 0);
  World loop_world; Object looping; looping.id = 1; looping.type = 0;
  looping.anim_state = int(data::AnimState::Move); loop_world.objects.push_back(looping);
  emit_scene_audio(loop_world, provider, units, audio, 0, 0, [](float, float, int) { return true; });
  CHECK(audio.mixer().active_voices() == 2); events.clear();
  loop_world.objects[0].anim_tick = 1;
  emit_scene_audio(loop_world, provider, units, audio, 0, 0, [](float, float, int) { return false; });
  CHECK(events.empty()); // the active loop survives the dispatcher fog gate
  audio.stop_effects(); events.clear();
  // A non-replacing incoming speech request appends after an unprotected tail.
  CHECK(audio.queue_speech_set(4, false, 1, 1, false));
  CHECK(audio.queue_speech_set(1, false, 1, 1, true, false));
  audio.update(); CHECK(!events.empty() && events.back().set == 4);
  audio.render_samples(samples, 8); audio.update(); CHECK(!events.empty() && events.back().set == 1);
  audio.stop_effects(); events.clear();
  // Verified order offsets use member type rather than the owner's race.
  w.objects.clear(); selected.type = 0; selected.side = 0; selected.id = 1;
  w.objects.push_back(selected);
  Object target = selected; target.id = 2; target.side = 1; w.objects.push_back(target);
  w.relation[0][1] = kEnemy; w.selection.ids = {1};
  Command cue; cue.side = 0; cue.ids = {1}; cue.type = Pct::Attack; cue.a = 2;
  emit_order_audio(w, cue, audio, 0, 0); audio.update();
  CHECK(events.size() == 2 && events.back().set == 103 && events.back().trigger == AudioEvent::Trigger::Orders);
  audio.stop_effects(); events.clear();
  cue.type = Pct::FollowTeam; cue.b = 0;
  emit_order_audio(w, cue, audio, 0, 0); audio.update(); CHECK(!events.empty() && events.back().set == 104);
  audio.stop_effects(); events.clear(); cue.type = Pct::Move;
  emit_order_audio(w, cue, audio, 0, 0); audio.update(); CHECK(events.empty());
  cue.type = Pct::Stop;
  emit_order_audio(w, cue, audio, 0, 0); audio.update(); CHECK(events.empty());
  w.objects[0].follow_tactic = 1;
  emit_order_audio(w, cue, audio, 0, 0); audio.update(); CHECK(!events.empty() && events.back().set == 107);
  audio.stop_effects(); events.clear();
  w.objects[1].side = 0; w.objects[1].type = 27; w.selection.ids = {1, 2};
  emit_selection_audio(w, audio, 0, 0); audio.update(); CHECK(events.empty());
  emit_selection_audio(w, audio, 1, 0); audio.update(); CHECK(!events.empty() && events.back().set == 101);
  audio.stop_effects(); events.clear(); audio.event_fallbacks = true; cue.type = Pct::Move;
  emit_order_audio(w, cue, audio, 0, 0); audio.update(); CHECK(!events.empty() && events.back().set == 104);
  audio.stop_effects();
  // Playlist reconfiguration retains the cursor and never interrupts an active track.
  AudioSystem playlist; playlist.init(root.string(), false);
  playlist.start_music(3, 0, 0); CHECK(playlist.current_track() == 3);
  playlist.start_music(4, 0, 0); CHECK(playlist.current_track() == 3);
  playlist.render_samples(samples, 8); playlist.update(); CHECK(playlist.current_track() == 4);
  playlist.stop_music(); playlist.start_music(0, 12, 0); CHECK(playlist.current_track() == 3);
  playlist.stop_music(); playlist.start_music(0, 12, 0); CHECK(playlist.current_track() == 4);
  playlist.stop_music();
  auto held_pcm = std::make_shared<Pcm>(40, int16_t(100));
  uint64_t held_voice = 0;
  for (int i = 0; i < 8; ++i) held_voice = playlist.mixer().play(held_pcm, 1, 1, true, true);
  playlist.start_music(3, 0, 0); CHECK(playlist.current_track() == 0);
  playlist.mixer().stop(held_voice); playlist.update();
  CHECK(playlist.current_track() == 3 && playlist.mixer().music_playing());
  CHECK(speech_list_id(120, 1, 2) == 420 && speech_list_id(120, 2, 3) == 320);
  CHECK(!speech_list_id(0, 1, 1) && !speech_list_id(101, 0, 0));
  CHECK(silent_sim.random_state() != 0);
  return failures ? 1 : 0;
}
