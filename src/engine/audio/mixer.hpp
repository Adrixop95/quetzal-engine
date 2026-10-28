// Software mixer (no SDL): sound voices + one music stream mixed into interleaved stereo S16 at 44100 Hz.
// Kept free of any device code so it can be unit-tested; AudioSystem feeds it from the SDL audio callback.
#pragma once
#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>
#include "core/audio.hpp"

namespace aztec::engine {

constexpr int kMixRate = 44100;
using Pcm = std::vector<int16_t>;   // interleaved stereo frames

// Converts WAV data (8/16 bit, mono/stereo, any rate) to stereo 44100 Hz; empty result on failure.
std::shared_ptr<Pcm> to_stereo_44k(const WavInfo& w);

// Original positional rules (docs/formats.md): pan = (x - view centre x) clamped to +-4000 hundredths of dB-ish units,
// attenuation by distance from the view centre: none below 500 px, linear to -4000 at 800 px, silent beyond.
struct Positional { float left = 1, right = 1; bool audible = true; };
Positional positional_gain(float dx_px, float dist_px);

class Mixer {
 public:
  static constexpr int kMaxVoices = 32;
  uint64_t play(std::shared_ptr<const Pcm> pcm, float left, float right, bool loop = false, bool speech = false);
  bool gain(uint64_t voice, float left, float right);
  void stop(uint64_t voice);
  void stop_effects() { for (auto& voice : voices_) voice = Voice{}; }
  // Music: raw PCM stereo 44100; `holder` keeps the backing memory alive.
  void play_music(std::shared_ptr<const void> holder, const int16_t* pcm, size_t frames);
  void stop_music() { music_pcm_ = nullptr; music_holder_.reset(); music_ended = false; }
  bool music_playing() const { return music_pcm_ != nullptr; }
  void mix(int16_t* out, int frames);
  size_t active_voices() const;
  float sfx_volume = 0.8f, music_volume = 0.6f, speech_volume = 0.8f;
  bool music_ended = false;   // set by mix() when the stream finished (the owner picks the next track)

 private:
  struct Voice { std::shared_ptr<const Pcm> pcm; size_t pos = 0; float l = 1, r = 1; bool loop = false; uint64_t id = 0; bool speech = false; };
  uint64_t next_voice_ = 0;
  Voice voices_[kMaxVoices];
  std::shared_ptr<const void> music_holder_;
  const int16_t* music_pcm_ = nullptr;
  size_t music_frames_ = 0, music_pos_ = 0;
};

}  // namespace aztec::engine
