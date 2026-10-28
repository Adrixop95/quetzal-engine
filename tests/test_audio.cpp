// Invented banks and animation records exercise the same dispatcher as the app.
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <unistd.h>
#include "engine/audio/scene_audio.hpp"

using namespace aztec;
using namespace aztec::engine;
using Bytes = std::vector<uint8_t>;
static int failures = 0;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL %d: %s\n", __LINE__, #c); ++failures; } } while (0)
static void put(Bytes& b, size_t pos, uint32_t v) { std::memcpy(b.data() + pos, &v, 4); }
static void write(const std::filesystem::path& path, const Bytes& b) {
  std::ofstream f(path, std::ios::binary); f.write(reinterpret_cast<const char*>(b.data()), b.size());
}
static Bytes wav() {
  Bytes b(48);
  std::memcpy(b.data(), "RIFF", 4); put(b, 4, 40);
  std::memcpy(b.data() + 8, "WAVEfmt ", 8); put(b, 16, 16);
  b[20] = 1; b[22] = 2; put(b, 24, 44100); put(b, 28, 176400);
  b[32] = 4; b[34] = 16;
  std::memcpy(b.data() + 36, "data", 4); put(b, 40, 4);
  b[44] = 100; b[46] = 100;
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
int main() {
  char name[] = "/tmp/aztec-audio-XXXXXX";
  char* temp = mkdtemp(name); if (!temp) return 1;
  const std::filesystem::path root(temp);
  std::filesystem::create_directory(root / "Res");
  std::filesystem::create_directory(root / "Music");
  Bytes set(4); put(set, 0, 1);
  write(root / "Res/Snd.xbe", archive({{0x460, {wav()}}, {0x3e8, {set, set}}}));
  write(root / "Music/music.xbe", archive({{0x460, {wav(), wav()}}}));
  std::vector<Bytes> animations(200);
  animations[3] = animation();   // Footman anim 4
  animations[13] = animation();  // Headtruck body anim 14
  animations[50] = animation();  // Headtruck turret anim 51
  animations[115] = animation(); // destruction anim 116
  animations[199] = animation(); // building anim 200
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
  AudioSystem audio; CHECK(audio.init(root.string(), false)); CHECK(!audio.device_ok());
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
  CHECK(events[1].kind == AudioEvent::Kind::LoopStart && events[1].set == 2);
  tick(1); CHECK(events.size() == 2);
  tick(2); CHECK(events.size() == 3 && events.back().tick == 3);  // same sprite, different frame
  tick(4); CHECK(events.size() == 4);
  tick(5); CHECK(events.size() == 4);  // held frame does not retrigger
  visible = false; tick(6); CHECK(events.back().kind == AudioEvent::Kind::LoopStop);
  visible = true; tick(7); CHECK(events.back().kind == AudioEvent::Kind::LoopStart);
  w.objects[0].cur_x = to_fixed(900); tick(8); CHECK(events.back().kind == AudioEvent::Kind::LoopStop);
  w.objects[0].cur_x = 0;
  w.objects[0].anim_state = int(data::AnimState::Attack); tick(0);
  CHECK(events[events.size() - 2].kind == AudioEvent::Kind::Sound);
  w.objects[0].anim_state = int(data::AnimState::Die); tick(0);
  CHECK(events.back().kind == AudioEvent::Kind::Sound);
  w.objects[0].removed = true; tick(1); CHECK(events.back().kind == AudioEvent::Kind::LoopStop);
  w.objects[0].removed = false; w.objects[0].type = 3;
  w.objects[0].anim_state = int(data::AnimState::Attack); w.objects[0].attack_tick = 0;
  tick(1); CHECK(events[events.size() - 2].kind == AudioEvent::Kind::Sound); // turret
  audio.stop_effects(); events.clear();
  w.objects[0].type = 10; w.objects[0].attack_tick = -1;
  w.objects[0].anim_state = int(data::AnimState::Construct);
  w.objects[0].progress = 500; w.objects[0].hp = 50;
  tick(1); CHECK(events.empty()); // construction seeks do not play frame cues
  w.objects[0].hp = 100; w.objects[0].progress = 1000;
  w.objects[0].anim_state = int(data::AnimState::Idle);
  tick(0); CHECK(events.size() == 2); // built sequence and its ambient header
  w.objects[0].anim_state = int(data::AnimState::Die);
  w.objects[0].destruction_radius = 90;
  tick(0); // destruction variant 0 needs slot 0
  CHECK(events.back().kind == AudioEvent::Kind::Sound);
  audio.stop_effects(); w.objects.clear(); events.clear();
  Effect effect; effect.anim = 4; effect.slot = 1; w.effects.push_back(effect);
  tick(0); CHECK(events.size() == 2); // effects use their own age clock
  w.effects[0].age = 1; tick(1); CHECK(events.size() == 2);
  w.effects[0].age = 2; tick(2); CHECK(events.size() == 3);
  w.effects.clear(); tick(3); CHECK(events.back().kind == AudioEvent::Kind::LoopStop);
  audio.stop_effects();
  events.clear(); audio.start_music(0, 3, 99);
  CHECK(audio.current_track() == 1); CHECK(events.size() == 1);
  int16_t samples[16]{}; audio.mixer().mix(samples, 8); audio.update();
  CHECK(audio.current_track() == 2);
  audio.mixer().mix(samples, 8); audio.update(); CHECK(audio.current_track() == 1);
  audio.start_music(2, 1, 0); CHECK(audio.current_track() == 2);
  audio.mixer().mix(samples, 8); audio.stop_music(); audio.update();
  CHECK(audio.current_track() == 0 && !audio.mixer().music_playing());
  audio.start_music(1, 0, 0); audio.start_music(0, 0, 0);
  CHECK(audio.current_track() == 0 && !audio.mixer().music_playing());
  audio.stop_effects();
  auto pcm = std::make_shared<Pcm>(Pcm{100, 200});
  Mixer mixer; mixer.sfx_volume = 1;
  auto handle = mixer.play(pcm, 1, 1, true); mixer.mix(samples, 8);
  CHECK(samples[0] == 100 && samples[15] == 200 && mixer.active_voices() == 1);
  CHECK(mixer.gain(handle, 0, 1)); mixer.mix(samples, 8); CHECK(samples[0] == 0);
  mixer.stop(handle); CHECK(mixer.active_voices() == 0 && !mixer.gain(handle, 1, 1));
  // Music works even when the effects archive is absent.
  std::filesystem::remove(root / "Res/Snd.xbe");
  AudioSystem music_only; CHECK(!music_only.init(root.string(), false));
  music_only.start_music(2, 0, 0); CHECK(music_only.current_track() == 2);
  std::filesystem::remove_all(root);
  return failures ? 1 : 0;
}
