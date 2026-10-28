#include "engine/audio/audio_system.hpp"

#include <algorithm>
#include <cstdio>

#include "engine/assets/game_data.hpp"

namespace aztec::engine {

AudioSystem::~AudioSystem() {
  if (dev_) { SDL_CloseAudioDevice(dev_); dev_ = 0; }
}

void AudioSystem::callback(void* user, Uint8* stream, int len) {
  AudioSystem* a = static_cast<AudioSystem*>(user);
  std::lock_guard<std::mutex> lock(a->mu_);
  a->mixer_.mix(reinterpret_cast<int16_t*>(stream), len / 4);
}

bool AudioSystem::init(const std::string& game_dir, bool open_device) {
  if (dev_) { SDL_CloseAudioDevice(dev_); dev_ = 0; }
  stop_effects(); stop_music(); tracks_.clear(); cache_.clear();
  std::string snd = find_file_ci(game_dir + "/Res", "Snd.xbe");
  bank_ok_ = !snd.empty() && bank_.open(snd);
  std::string mdir = game_dir + "/Music";
  music_files_[0] = find_file_ci(mdir, "music.xbe");
  music_files_[1] = find_file_ci(mdir, "Music_cd2.xbe");
  for (int f = 0; f < 2; f++) {
    std::vector<MusicTrack> t;
    if (!music_files_[f].empty() && read_music_table(music_files_[f], t))
      for (const MusicTrack& m : t) tracks_.push_back({int(m.number), {f, m}});
  }
  if (!open_device) return bank_ok_;
  if (!SDL_WasInit(SDL_INIT_AUDIO) && SDL_InitSubSystem(SDL_INIT_AUDIO) != 0) { std::fprintf(stderr, "note: audio unavailable (%s)\n", SDL_GetError()); return bank_ok_; }
  SDL_AudioSpec want{};
  want.freq = kMixRate; want.format = AUDIO_S16SYS; want.channels = 2; want.samples = 1024;
  want.callback = &AudioSystem::callback; want.userdata = this;
  dev_ = SDL_OpenAudioDevice(nullptr, 0, &want, nullptr, 0);
  if (!dev_) { std::fprintf(stderr, "note: no audio device (%s)\n", SDL_GetError()); return bank_ok_; }
  SDL_PauseAudioDevice(dev_, 0);
  return bank_ok_;
}

void AudioSystem::set_volumes(float sfx, float music) {
  std::lock_guard<std::mutex> lock(mu_);
  mixer_.sfx_volume = sfx;
  mixer_.music_volume = music;
}

std::shared_ptr<const Pcm> AudioSystem::sound_pcm(uint32_t id) {
  auto it = cache_.find(id);
  if (it != cache_.end()) return it->second;
  std::shared_ptr<const Pcm> out;
  Item item = bank_.wav(id);
  WavInfo w;
  if (item && parse_wav(item.data, item.size, w)) out = to_stereo_44k(w);
  return cache_[id] = out;
}

uint32_t AudioSystem::play_sound(uint32_t id, float l, float r) {
  if (!bank_ok_ || id == 0) return 0;
  std::shared_ptr<const Pcm> pcm = sound_pcm(id);
  if (!pcm || pcm->empty()) return 0;
  { std::lock_guard<std::mutex> lock(mu_); mixer_.play(pcm, l, r); }
  emit({AudioEvent::Kind::Sound, 0, 0, id});
  return id;
}

uint32_t AudioSystem::choose(uint32_t set_id) {
  const auto set = bank_.sound_set(set_id);
  if (set.empty()) return 0;
  rand_ = rand_ * 1664525u + 1013904223u;
  return set[(rand_ >> 8) % set.size()];
}
uint32_t AudioSystem::play_set_at(uint32_t set_id, float x, float y, float cx, float cy) {
  if (!bank_ok_ || set_id == 0) return 0;
  const float dx = x - cx, dy = y - cy;
  const auto p = positional_gain(dx, std::sqrt(dx * dx + dy * dy));
  if (!p.audible) return 0;
  const auto id = choose(set_id);
  auto pcm = sound_pcm(id);
  if (!pcm || pcm->empty()) return 0;
  { std::lock_guard<std::mutex> lock(mu_); mixer_.play(pcm, p.left, p.right); }
  emit({AudioEvent::Kind::Sound, 0, set_id, id});
  return id;
}
void AudioSystem::begin_scene_tick() {
  for (auto& [source, loop] : loops_) loop.seen = false;
}
void AudioSystem::loop_at(uint64_t source, uint32_t set, float x, float y, float cx, float cy) {
  const float dx = x - cx, dy = y - cy;
  const auto p = positional_gain(dx, std::sqrt(dx * dx + dy * dy));
  if (!set || !bank_ok_ || !p.audible) return;
  auto it = loops_.find(source);
  if (it != loops_.end() && it->second.set == set) {
    std::lock_guard<std::mutex> lock(mu_);
    if (mixer_.gain(it->second.voice, p.left, p.right)) { it->second.seen = true; return; }
  }
  if (it != loops_.end()) {
    { std::lock_guard<std::mutex> lock(mu_); mixer_.stop(it->second.voice); }
    emit({AudioEvent::Kind::LoopStop, source, it->second.set, it->second.sound});
    loops_.erase(it);
  }
  const uint32_t id = choose(set);
  auto pcm = sound_pcm(id);
  if (!pcm || pcm->empty()) return;
  uint64_t voice;
  { std::lock_guard<std::mutex> lock(mu_); voice = mixer_.play(pcm, p.left, p.right, true); }
  loops_[source] = {set, id, voice, true};
  emit({AudioEvent::Kind::LoopStart, source, set, id});
}
void AudioSystem::end_scene_tick() {
  for (auto it = loops_.begin(); it != loops_.end();) {
    if (it->second.seen) { ++it; continue; }
    { std::lock_guard<std::mutex> lock(mu_); mixer_.stop(it->second.voice); }
    emit({AudioEvent::Kind::LoopStop, it->first, it->second.set, it->second.sound});
    it = loops_.erase(it);
  }
}
void AudioSystem::stop_effects() {
  begin_scene_tick(); end_scene_tick();
  std::lock_guard<std::mutex> lock(mu_);
  mixer_.stop_effects();
}

void AudioSystem::start_music(int music, uint32_t mask, int salt) {
  stop_music();
  event_tick = 0;
  allowed_.clear();
  uint32_t available = 0;
  for (const auto& t : tracks_) if (t.first >= 1 && t.first <= 32) available |= 1u << (t.first - 1);
  uint32_t allowed = music_allowed_mask(music, mask, available);
  for (int n = 1; n <= 32; n++) if (allowed & (1u << (n - 1))) allowed_.push_back(n);
  (void)salt;
  pick_ = 0;
  if (!allowed_.empty()) pick_and_play_music();
}

void AudioSystem::stop_music() {
  { std::lock_guard<std::mutex> lock(mu_); mixer_.stop_music(); }
  allowed_.clear();
  if (track_) emit({AudioEvent::Kind::MusicStop, 0, 0, uint32_t(track_)});
  track_ = 0;
}

void AudioSystem::pick_and_play_music() {
  if (allowed_.empty()) return;
  for (size_t attempt = 0; attempt < allowed_.size(); ++attempt) {
    int n = allowed_[pick_++ % allowed_.size()];
    for (const auto& t : tracks_) {
      if (t.first != n) continue;
      auto buf = std::make_shared<std::vector<uint8_t>>();
      if (!read_music_track(music_files_[t.second.first], t.second.second, *buf)) continue;
      WavInfo w;
      if (!parse_wav(buf->data(), buf->size(), w) || w.channels != 2 || w.bits != 16 ||
          w.rate != kMixRate || w.pcm_bytes < 4) continue;
      { std::lock_guard<std::mutex> lock(mu_);
        mixer_.play_music(buf, reinterpret_cast<const int16_t*>(w.pcm), w.pcm_bytes / 4); }
      track_ = n;
      emit({AudioEvent::Kind::Music, 0, 0, uint32_t(n)});
      return;
    }
  }
  track_ = 0;
}

void AudioSystem::update() {
  bool ended;
  { std::lock_guard<std::mutex> lock(mu_); ended = mixer_.music_ended; mixer_.music_ended = false; }
  if (ended) pick_and_play_music();
}

}  // namespace aztec::engine
