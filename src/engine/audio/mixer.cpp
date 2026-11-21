#include "engine/audio/mixer.hpp"

#include <algorithm>
#include <cmath>

namespace aztec::engine {

std::shared_ptr<Pcm> to_stereo_44k(const WavInfo& w) {
  if (!w.pcm || w.channels < 1 || w.channels > 2 || w.rate <= 0 || (w.bits != 8 && w.bits != 16)) return nullptr;
  size_t bps = size_t(w.bits / 8) * size_t(w.channels);
  size_t frames = w.pcm_bytes / bps;
  if (frames == 0) return nullptr;
  auto sample = [&](size_t f, int ch) -> int {
    size_t i = f * bps + size_t(std::min(ch, w.channels - 1)) * size_t(w.bits / 8);
    if (w.bits == 8) return (int(w.pcm[i]) - 128) << 8;
    return int16_t(w.pcm[i] | w.pcm[i + 1] << 8);
  };
  size_t out_frames = size_t(double(frames) * kMixRate / w.rate);
  auto out = std::make_shared<Pcm>(out_frames * 2);
  for (size_t i = 0; i < out_frames; i++) {
    double src = double(i) * w.rate / kMixRate;
    size_t a = size_t(src), b = std::min(frames - 1, a + 1);
    double f = src - double(a);
    for (int ch = 0; ch < 2; ch++) (*out)[i * 2 + size_t(ch)] = int16_t(sample(std::min(a, frames - 1), ch) * (1 - f) + sample(b, ch) * f);
  }
  return out;
}

Positional positional_gain(float dx, float dist) {
  Positional p;
  float pan = std::clamp(dx, -4000.f, 4000.f);          // hundredths of dB
  float att = 0;
  if (dist >= 800) { p.audible = false; return p; }
  if (dist > 500) att = -4000.f * (dist - 500) / 300.f;
  auto gain = [](float cdb) { return std::pow(10.f, cdb / 2000.f); };   // 100 units = 1 dB, amplitude
  p.left = gain(att + (pan > 0 ? -pan : 0));
  p.right = gain(att + (pan < 0 ? pan : 0));
  return p;
}

uint64_t Mixer::play(std::shared_ptr<const Pcm> pcm, float l, float r, bool loop, bool speech, bool protected_speech) {
  if (!pcm || pcm->size() < 2) return 0;
  const int slot = allocate_slot();
  if (slot < 0) return 0;
  voices_[slot] = {std::move(pcm), 0, l, r, loop, ++next_voice_, speech, speech && protected_speech};
  return next_voice_;
}

int Mixer::allocate_slot() {
  // V: search cyclically for a free buffer, then an unprotected victim.
  // Music and flagged speech are protected; effects and casual speech are not.
  for (int pass = 0; pass < 2; ++pass) {
    for (int offset = 1; offset <= kMaxVoices; ++offset) {
      const int slot = (last_slot_ + offset) % kMaxVoices;
      if (slot == music_slot_) continue;
      const auto& voice = voices_[slot];
      if (!voice.pcm || (pass == 1 && !voice.protected_voice)) {
        last_slot_ = slot;
        voices_[slot] = Voice{};
        return slot;
      }
    }
  }
  return -1;
}

bool Mixer::gain(uint64_t id, float l, float r) {
  for (auto& v : voices_) if (v.pcm && v.id == id) { v.l = l; v.r = r; return true; }
  return false;
}
bool Mixer::playing(uint64_t id) const {
  for (const auto& v : voices_) if (v.pcm && v.id == id) return true;
  return false;
}
void Mixer::stop(uint64_t id) {
  for (auto& v : voices_) if (v.id == id) v = Voice{};
}

bool Mixer::play_music(std::shared_ptr<const void> holder, const int16_t* pcm, size_t frames) {
  stop_music();
  if (!pcm || !frames) return false;
  music_slot_ = allocate_slot();
  if (music_slot_ < 0) return false;
  music_holder_ = std::move(holder);
  music_pcm_ = pcm;
  music_frames_ = frames;
  music_pos_ = 0;
  music_ended = false;
  return true;
}

size_t Mixer::active_voices() const {
  size_t n = 0;
  for (const Voice& v : voices_) if (v.pcm) n++;
  return n;
}

void Mixer::mix(int16_t* out, int frames) {
  std::vector<int32_t> acc(size_t(frames) * 2, 0);
  for (Voice& v : voices_) {
    if (!v.pcm) continue;
    size_t total = v.pcm->size() / 2;
    int offset = 0;
    while (offset < frames && v.pcm) {
      int n = int(std::min<size_t>(size_t(frames - offset), total - v.pos));
      const int16_t* samples = v.pcm->data() + v.pos * 2;
      const float volume = v.speech ? speech_volume : sfx_volume;
      float gl = v.l * volume, gr = v.r * volume;
      for (int i = 0; i < n; ++i) {
        acc[size_t(offset + i) * 2] += int32_t(samples[i * 2] * gl);
        acc[size_t(offset + i) * 2 + 1] += int32_t(samples[i * 2 + 1] * gr);
      }
      offset += n;
      v.pos += size_t(n);
      if (v.pos == total) { if (v.loop) v.pos = 0; else v = Voice{}; }
    }
  }
  if (music_pcm_) {
    int n = int(std::min<size_t>(size_t(frames), music_frames_ - music_pos_));
    const int16_t* s = music_pcm_ + music_pos_ * 2;
    for (int i = 0; i < n; i++) { acc[size_t(i) * 2] += int32_t(s[i * 2] * music_volume); acc[size_t(i) * 2 + 1] += int32_t(s[i * 2 + 1] * music_volume); }
    music_pos_ += size_t(n);
    if (music_pos_ >= music_frames_) { stop_music(); music_ended = true; }
  }
  for (size_t i = 0; i < acc.size(); i++) out[i] = int16_t(std::clamp(acc[i], -32768, 32767));
}

void Mixer::mix_at_rate(int16_t* out, int frames, int rate) {
  if (rate != 11025 && rate != 22050 && rate != kMixRate) return;
  const int factor = kMixRate / rate;
  if (factor == 1) { mix(out, frames); return; }
  std::vector<int16_t> source(size_t(frames) * factor * 2);
  mix(source.data(), frames * factor);
  for (int i = 0; i < frames; ++i) for (int ch = 0; ch < 2; ++ch) {
    int sum = 0;
    for (int j = 0; j < factor; ++j) sum += source[size_t(i * factor + j) * 2 + ch];
    out[size_t(i) * 2 + ch] = int16_t(sum / factor);
  }
}

}  // namespace aztec::engine
