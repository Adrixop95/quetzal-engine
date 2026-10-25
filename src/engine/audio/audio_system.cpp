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

bool AudioSystem::init(const std::string& game_dir) {
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
  if (!pcm) return 0;
  std::lock_guard<std::mutex> lock(mu_);
  mixer_.play(pcm, l, r);
  return id;
}

uint32_t AudioSystem::play_set_at(uint32_t set_id, float x, float y, float cx, float cy) {
  if (!bank_ok_ || set_id == 0) return 0;
  std::vector<uint32_t> set = bank_.sound_set(set_id);
  if (set.empty()) return 0;
  rand_ = rand_ * 1664525u + 1013904223u;
  uint32_t id = set[(rand_ >> 8) % set.size()];
  float dx = x - cx, dy = y - cy;
  Positional p = positional_gain(dx, std::sqrt(dx * dx + dy * dy));
  if (!p.audible) return 0;
  return play_sound(id, p.left, p.right);
}

void AudioSystem::start_music(int music, uint32_t mask, int salt) {
  allowed_.clear();
  uint32_t available = 0;
  for (const auto& t : tracks_) if (t.first >= 1 && t.first <= 32) available |= 1u << (t.first - 1);
  uint32_t allowed = music_allowed_mask(music, mask, available);
  for (int n = 1; n <= 32; n++) if (allowed & (1u << (n - 1))) allowed_.push_back(n);
  salt_ = salt;
  pick_ = 0;
  if (!allowed_.empty()) pick_and_play_music();
}

void AudioSystem::stop_music() {
  std::lock_guard<std::mutex> lock(mu_);
  mixer_.stop_music();
  track_ = 0;
}

void AudioSystem::pick_and_play_music() {
  if (allowed_.empty()) return;
  int n = allowed_[size_t(salt_ + pick_++) % allowed_.size()];
  for (const auto& t : tracks_) {
    if (t.first != n) continue;
    auto buf = std::make_shared<std::vector<uint8_t>>();
    if (!read_music_track(music_files_[t.second.first], t.second.second, *buf)) continue;
    WavInfo w;
    if (!parse_wav(buf->data(), buf->size(), w) || w.channels != 2 || w.bits != 16 || w.rate != kMixRate) continue;
    std::lock_guard<std::mutex> lock(mu_);
    mixer_.play_music(buf, reinterpret_cast<const int16_t*>(w.pcm), w.pcm_bytes / 4);
    track_ = n;
    return;
  }
}

void AudioSystem::update() {
  bool ended;
  { std::lock_guard<std::mutex> lock(mu_); ended = mixer_.music_ended; mixer_.music_ended = false; }
  if (ended) pick_and_play_music();
}

}  // namespace aztec::engine
