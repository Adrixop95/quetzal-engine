// Sound effects + music through SDL audio. Silent (but functional) when no audio device is available.
#pragma once
#include <SDL2/SDL.h>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>
#include "core/audio.hpp"
#include "engine/audio/mixer.hpp"

namespace aztec::engine {

class AudioSystem {
 public:
  ~AudioSystem();
  // game_dir holds Res/Snd.xbe and Music/*.xbe. Returns false if the sound bank cannot be read (the object stays usable, silent).
  bool init(const std::string& game_dir);
  bool device_ok() const { return dev_ != 0; }
  bool bank_ok() const { return bank_ok_; }
  void set_volumes(float sfx, float music);

  // Plays sound set `set_id` (a random member) at world position (x, y) heard from the view centre (cx, cy), all in px.
  // Returns the WAV sound id chosen, 0 if none / not audible.
  uint32_t play_set_at(uint32_t set_id, float x, float y, float cx, float cy);
  uint32_t play_sound(uint32_t sound_id, float left = 1, float right = 1);   // direct WAV id (UI)

  // Mission music: `music` = [Globals] Music (exact track when != 0), `mask` = MusicMask; `salt` varies the pick.
  void start_music(int music, uint32_t mask, int salt);
  void stop_music();
  void update();   // call once per frame: starts the next track when one ends
  int current_track() const { return track_; }
  Mixer& mixer() { return mixer_; }   // tests / tools; not thread-safe while the device runs

 private:
  static void callback(void* user, Uint8* stream, int len);
  std::shared_ptr<const Pcm> sound_pcm(uint32_t id);
  void pick_and_play_music();

  SDL_AudioDeviceID dev_ = 0;
  std::mutex mu_;
  Mixer mixer_;
  SoundBank bank_;
  bool bank_ok_ = false;
  std::map<uint32_t, std::shared_ptr<const Pcm>> cache_;
  std::string music_files_[2];
  std::vector<std::pair<int, std::pair<int, MusicTrack>>> tracks_;   // track number -> (file index, track)
  std::vector<int> allowed_;
  int track_ = 0, pick_ = 0, salt_ = 0;
  uint32_t rand_ = 7;
};

}  // namespace aztec::engine
