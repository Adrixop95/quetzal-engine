// Sound data: Res/Snd.xbe (effects, speech) and Music/*.xbe (CD audio tracks). See docs/formats.md.
#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include "core/archive.hpp"

namespace aztec {

struct WavInfo {
  int channels = 0, rate = 0, bits = 0;
  const uint8_t* pcm = nullptr;
  size_t pcm_bytes = 0;
};
// Plain RIFF/WAVE PCM (what both archives hold). Handles `data` size larger than the buffer (clamps).
bool parse_wav(const uint8_t* data, size_t size, WavInfo& out);

// Res/Snd.xbe
//   group 0x460, items 1..2013: WAV files (22050 Hz, 16 bit, mono). Item number = "sound id".
//   group 0x3e8, items 1..432 : sound sets: array of u32 sound ids; the game picks one of them
//                               (rand) when the set is triggered. Set id = animation frame field [2] etc.
//   group 0x6, items 1202..1731: NUL-terminated cp1250 subtitle text for speech sound id N.
class SoundBank {
 public:
  bool open(const std::string& snd_xbe_path);
  Item wav(uint32_t sound_id) const { return ar_.get(0x460, sound_id); }
  std::vector<uint32_t> sound_set(uint32_t set_id) const;
  std::string subtitle(uint32_t sound_id) const;  // cp1250 bytes, "" if none
  const Archive& archive() const { return ar_; }

 private:
  Archive ar_;
};

// Music/music.xbe and Music/Music_cd2.xbe: one group 0x460 whose items are CD-quality WAVs
// (44100 Hz stereo 16 bit, ~20-30 MB each). Empty items (zero length) are tracks stored on the other disc.
struct MusicTrack { uint32_t number; uint64_t offset, size; };  // number = item number (1-based)
bool read_music_table(const std::string& path, std::vector<MusicTrack>& out);  // only non-empty tracks
bool read_music_track(const std::string& path, const MusicTrack& t, std::vector<uint8_t>& out);

// Which tracks the mission may play: bit (n-1) = track n. `music` = [Globals] Music, `mask` = MusicMask,
// `available` = tracks present on the disc(s). Music != 0 selects exactly track `music`.
uint32_t music_allowed_mask(int music, uint32_t mask, uint32_t available);

}  // namespace aztec
