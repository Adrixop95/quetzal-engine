// Sound effects + music through SDL audio. Silent (but functional) when no audio device is available.
#pragma once
#include <SDL.h>
#include <map>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <vector>
#include "core/audio.hpp"
#include "engine/audio/mixer.hpp"

namespace aztec::engine {

struct AudioEvent {
  enum class Kind { Sound, LoopStart, LoopStop, Music, MusicStop, Speech, Subtitle };
  Kind kind;
  uint64_t source = 0;
  uint32_t set = 0, sound = 0;
  uint64_t tick = 0;
};

class AudioSystem {
 public:
  ~AudioSystem();
  // game_dir holds Res/Snd.xbe and Music/*.xbe. Returns whether the effects bank is readable; music loads independently.
  bool init(const std::string& game_dir, bool open_device = true);
  // Called on the main thread after successful playback requests, also without a device.
  std::function<void(const AudioEvent&)> event_sink;
  uint64_t event_tick = 0;
  void begin_scene_tick();
  void loop_at(uint64_t source, uint32_t set, float x, float y, float cx, float cy);
  void end_scene_tick();
  void stop_effects();
  bool device_ok() const { return dev_ != 0; }
  bool bank_ok() const { return bank_ok_; }
  void set_volumes(float sfx, float music, float speech = 0.8f);
  bool acknowledgement = true, subtitles = true;
  // Explicit speech WAV IDs only. Event/type/race mappings must be evidenced by callers.
  uint32_t play_speech(uint32_t sound_id, bool is_acknowledgement = false);
  const std::string& subtitle() const { return subtitle_; }

  // Plays sound set `set_id` (a random member) at world position (x, y) heard from the view centre (cx, cy), all in px.
  // Returns the WAV sound id chosen, 0 if none / not audible.
  uint32_t play_set_at(uint32_t set_id, float x, float y, float cx, float cy);
  uint32_t play_sound(uint32_t sound_id, float left = 1, float right = 1);   // direct WAV id (UI)

  // Mission music: `music` = [Globals] Music (exact track when != 0), `mask` = MusicMask; `salt` is retained for source compatibility; tracks play in ascending order.
  void start_music(int music, uint32_t mask, int salt);
  void stop_music();
  void update(double seconds = 0);   // presentation time; starts the next track when one ends
  int current_track() const { return track_; }
  Mixer& mixer() { return mixer_; }   // tests / tools; not thread-safe while the device runs

 private:
  static void callback(void* user, Uint8* stream, int len);
  void emit(AudioEvent event) { event.tick = event_tick; if (event_sink) event_sink(event); }
  struct Loop { uint32_t set, sound; uint64_t voice; bool seen; };
  std::map<uint64_t, Loop> loops_;
  uint32_t choose(uint32_t set);
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
  int track_ = 0;
  size_t pick_ = 0;
  uint32_t rand_ = 7;
  uint64_t speech_voice_ = 0;
  float speech_volume_ = 0.8f;
  std::string subtitle_;
  double subtitle_left_ = 0;
};

}  // namespace aztec::engine
