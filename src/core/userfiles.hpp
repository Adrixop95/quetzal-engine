// Player profile (*.plr), save-game header (Saves/singleN.sav), Keys.ini and aztec_settings.ini.
// Layouts: docs/formats.md.
#pragma once
#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace aztec {

// ---- *.plr: 300 bytes. The original keeps the options in the Windows registry and copies the
// 224-byte options block into each profile file.
struct PlayerOptions {          // block at plr+0x4c; field offsets are relative to the block
  uint32_t acknowledgement = 1; // +0x08 bool (registry Sound\Acknowledgement)
  uint32_t unit_speed_mode = 0; // +0x0c enum 0..3 (cycles a button label; meaning unverified)
  uint32_t sfx_volume = 0;      // +0x94
  uint32_t music_volume = 0;    // +0x98
  uint32_t speech_volume = 0;   // +0x9c
  uint32_t frequency = 22050;   // +0xa0 mixer rate: 11025 / 22050 / other = 44100
  uint32_t display_mode = 0;    // +0xa4 0..2 (0 = 800x600, 1 = 1024x768)
  uint32_t scroll_speed = 48;   // +0xa8
  uint32_t game_speed = 83;     // +0xac
  double gamma = 1.0;           // +0xb0
  uint8_t view_all = 0;         // +0xb8
  uint8_t b9 = 1;               // +0xb9 (constant 1)
  uint8_t default_provider = 1; // +0xba network provider (registry Net\DefaultProvider)
};

struct PlayerFile {
  uint8_t flag = 0;             // +0x00 (written from the caller's parameter; 1 for "ab")
  std::string name;             // +0x01, NUL-terminated, at most 0x42 chars
  uint32_t const_one = 1;       // +0x44 (always written as 1)
  PlayerOptions options;
  std::vector<uint8_t> raw;     // the original 300 bytes (unknown parts preserved)
};
constexpr size_t kPlayerFileSize = 300;
bool parse_player_file(const std::vector<uint8_t>& data, PlayerFile& out);
std::vector<uint8_t> serialize_player_file(const PlayerFile& p);  // starts from p.raw when present
bool load_player_file(const std::string& path, PlayerFile& out);

// ---- Saves/singleN.sav and multiN.sav (header only; the rest is a raw object stream)
struct SaveHeader {
  uint32_t zero0 = 0;           // +0x00
  uint32_t zero1 = 0;           // +0x04 (read into DAT_004dd164)
  std::string map_path;         // +0x08 char[256], path of the .mp the game was started from
  std::string description;      // +0x108 char[64], the name typed by the player
  uint16_t time[8] = {};        // +0x148 SYSTEMTIME: year, month, dow, day, hour, min, sec, ms
  size_t body_offset = 0x158;   // object stream starts here
};
bool parse_save_header(const std::vector<uint8_t>& data, SaveHeader& out);
bool load_save_header(const std::string& path, SaveHeader& out);

// ---- Keys.ini: "Action key" per line; several lines may bind one action.
struct KeyBinding { int action; int dik; };  // action: id of the table below, dik: DirectInput scancode
int key_action_id(const std::string& name);  // -1 when unknown, case-insensitive ("add" = 2 ... "cheatwin" = 52)
const char* key_action_name(int id);
int dik_from_name(const std::string& name);  // "lshift" = 42, "space" = 57, ... -1 when unknown
bool parse_keys_ini(const std::string& text, std::vector<KeyBinding>& out);
bool load_keys_ini(const std::string& path, std::vector<KeyBinding>& out);

// ---- aztec_settings.ini (recompiled runtime only, not in the original game)
struct RuntimeSettings { int res_w = 0, res_h = 0, window_mode = 0, smooth = 0, video = 1, mouse_speed = 100; };
bool parse_runtime_settings(const std::string& text, RuntimeSettings& out);

}  // namespace aztec
