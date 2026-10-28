// Engine campaign presentation and completion records, independent of simulation.
#pragma once
#include <cstdint>
#include <string>
#include "engine/assets/game_data.hpp"
#include "engine/platform/input.hpp"
#include "engine/render/font.hpp"

namespace aztec::engine {
int campaign_number(const std::string& spec);
std::string next_campaign_mission(const GameData& data, const std::string& spec);
std::string mission_briefing(const GameData& data, const std::string& spec);

// A separate engine profile; original .plr files are never modified.
struct CampaignProgress {
  uint16_t completed = 0;
  bool won(int mission) const;
  void record(const std::string& spec, bool victory);
  bool load(const std::string& path);
  bool save(const std::string& path) const;
};

class TextOverlay {
 public:
  bool open = false;
  int scroll = 0;
  void show(std::string heading, std::string body) { title = std::move(heading); text = std::move(body); scroll = 0; open = true; }
  void update(const Input& in);
  void draw(SDL_Renderer* r, const Font& font, int width, int height, int scale);
 private:
  std::string title, text;
};

class OutcomeMenu {
 public:
  enum class Action { None, Continue, Retry, Menu };
  static SDL_Rect button(int index, int width, int height, int scale);
  static Action update(const Input& in, int width, int height, int scale, bool victory);
  static void draw(SDL_Renderer* r, const Font& font, int width, int height, int scale,
                   bool victory, bool final_mission, bool has_next);
};
}  // namespace aztec::engine
