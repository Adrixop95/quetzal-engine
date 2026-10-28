#include "engine/ui/campaign.hpp"

#include <algorithm>
#include <charconv>
#include <cctype>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include "core/text.hpp"
#include "core/visfile.hpp"
#include "engine/assets/mission.hpp"
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

namespace aztec::engine {
int campaign_number(const std::string& spec) {
  std::string name = std::filesystem::path(spec).filename().string();
  for (char& c : name) c = char(std::tolower(static_cast<unsigned char>(c)));
  if (name.size() > 3 && name.substr(name.size() - 3) == ".mp") name.resize(name.size() - 3);
  if (name.rfind("mission_", 0) == 0) name.erase(0, 8);
  int n = 0;
  const auto result = std::from_chars(name.data(), name.data() + name.size(), n);
  return result.ec == std::errc{} && result.ptr == name.data() + name.size() &&
      n >= 1 && n <= 15 && name == std::to_string(n) ? n : 0;
}

std::string next_campaign_mission(const GameData& data, const std::string& spec) {
  const int n = campaign_number(spec);
  if (n < 1 || n >= 15) return {};
  const std::string next = "mission_" + std::to_string(n + 1);
  return find_file_ci(data.maps_dir(), next + ".mp").empty() ? "" : next;
}

std::string mission_briefing(const GameData& data, const std::string& spec) {
  const auto mp = resolve_mission(data.maps_dir(), spec);
  const auto stem = std::filesystem::path(mp).stem().string();
  std::vector<BriefingSection> sections;
  std::string text;
  if (load_briefing(find_file_ci(data.maps_dir(), stem + ".mit"), sections)) {
    for (const auto& section : sections)
      text += cp1250_to_utf8(section.title) + "\n" + cp1250_to_utf8(section.text) + "\n";
  } else {
    MissionDef mission;
    if (load_mission(mp, mission)) for (const auto& note : mission.notes) text += note + "\n";
  }
  return text.empty() ? "No briefing or objectives are available for this mission." : text;
}

bool CampaignProgress::won(int n) const { return n >= 1 && n <= 15 && (completed & (1u << (n - 1))); }
void CampaignProgress::record(const std::string& spec, bool victory) {
  const int n = campaign_number(spec);
  if (victory && n) completed |= uint16_t(1u << (n - 1));
}
bool CampaignProgress::load(const std::string& path) {
  std::ifstream file(path, std::ios::binary);
  char bytes[64];
  file.read(bytes, sizeof bytes);
  if (file.bad()) return false;
  const std::string contents(bytes, size_t(file.gcount()));
  const std::string prefix = "QUETZAL_CAMPAIGN_1\n";
  if (contents.size() <= prefix.size() || contents.size() >= sizeof bytes || contents.rfind(prefix, 0) != 0 || contents.back() != '\n') return false;
  unsigned value = 0;
  auto result = std::from_chars(contents.data() + prefix.size(), contents.data() + contents.size() - 1, value);
  if (result.ec != std::errc{} || result.ptr != contents.data() + contents.size() - 1 || value > 32767) return false;
  completed = uint16_t(value);
  return true;
}
bool CampaignProgress::save(const std::string& path) const {
  if (completed > 32767) return false;
  const auto staging = path + ".pending";
  std::error_code ec;
  // Exclusive directory creation refuses stale output and pre-existing symlinks.
  if (!std::filesystem::create_directory(staging, ec)) return false;
  const auto temporary = staging + "/profile";
  std::ofstream file(temporary, std::ios::binary | std::ios::trunc);
  file << "QUETZAL_CAMPAIGN_1\n" << completed << '\n';
  file.close();
  bool ok = !file.fail();
#ifdef _WIN32
  if (ok) ok = MoveFileExW(std::filesystem::path(temporary).c_str(), std::filesystem::path(path).c_str(), MOVEFILE_REPLACE_EXISTING) != 0;
#else
  if (ok) ok = std::rename(temporary.c_str(), path.c_str()) == 0;
#endif
  if (!ok) std::filesystem::remove(temporary, ec);
  std::filesystem::remove(staging, ec);
  return ok;
}

void TextOverlay::update(const Input& in) {
  if (!open) return;
  if (in.hit(SDL_SCANCODE_ESCAPE) || (in.hit(SDL_SCANCODE_RETURN) && !in.alt)) open = false;
  if (in.wheel < 0 || in.hit(SDL_SCANCODE_DOWN)) ++scroll;
  if (in.wheel > 0 || in.hit(SDL_SCANCODE_UP)) scroll = std::max(0, scroll - 1);
  if (in.hit(SDL_SCANCODE_PAGEDOWN)) scroll += 8;
  if (in.hit(SDL_SCANCODE_PAGEUP)) scroll = std::max(0, scroll - 8);
  if (in.hit(SDL_SCANCODE_HOME)) scroll = 0;
  if (in.hit(SDL_SCANCODE_END)) scroll = 1000000;
}
void TextOverlay::draw(SDL_Renderer* r, const Font& font, int w, int h, int scale) {
  if (!open) return;
  const int margin = 16 * scale, ts = 2 * scale, lh = font.height(ts) + 4 * scale;
  SDL_Rect box{margin, margin, std::max(1, w - 2 * margin), std::max(1, h - 2 * margin)};
  SDL_SetRenderDrawColor(r, 20, 18, 14, 255); SDL_RenderFillRect(r, &box);
  SDL_Rect previous{}; const bool clipped = SDL_RenderIsClipEnabled(r); SDL_RenderGetClipRect(r, &previous);
  SDL_Rect clip = box;
  if (clipped) SDL_IntersectRect(&box, &previous, &clip);
  SDL_RenderSetClipRect(r, &clip);
  const int x = box.x + 8 * scale, y = box.y + 8 * scale;
  font.draw(r, x, y, title, ts, {255, 230, 150, 255});
  auto lines = font.wrap(text, ts, box.w - 16 * scale);
  const int count = std::max(1, (box.h - 16 * scale) / lh - 3);
  scroll = std::clamp(scroll, 0, std::max(0, int(lines.size()) - count));
  for (int i = 0; i < count && i + scroll < int(lines.size()); ++i)
    font.draw(r, x, y + (i + 2) * lh, lines[size_t(i + scroll)], ts, {240, 235, 210, 255});
  font.draw(r, x, box.y + box.h - lh, "UP/DOWN: SCROLL   ENTER/ESC: CLOSE", ts, {200, 200, 180, 255});
  SDL_RenderSetClipRect(r, clipped ? &previous : nullptr);
}

SDL_Rect OutcomeMenu::button(int i, int w, int h, int scale) {
  const int bw = std::min(w - 16, 280 * scale), bh = 30 * scale;
  return {(w - bw) / 2, h / 2 + i * 40 * scale, bw, bh};
}
OutcomeMenu::Action OutcomeMenu::update(const Input& in, int w, int h, int scale, bool victory) {
  if (in.hit(SDL_SCANCODE_ESCAPE)) return Action::Menu;
  if (in.hit(SDL_SCANCODE_RETURN) && !in.alt) return victory ? Action::Continue : Action::Retry;
  if (!in.pressed[SDL_BUTTON_LEFT]) return Action::None;
  const SDL_Point point{in.mx, in.my};
  for (int i = 0; i < (victory ? 3 : 2); ++i) {
    auto rect = button(i, w, h, scale);
    if (SDL_PointInRect(&point, &rect)) return i == 0 ? (victory ? Action::Continue : Action::Retry) : i == 1 && victory ? Action::Retry : Action::Menu;
  }
  return Action::None;
}
void OutcomeMenu::draw(SDL_Renderer* r, const Font& font, int w, int h, int scale, bool victory, bool final_mission, bool has_next) {
  SDL_Rect box{0, 0, w, h};
  SDL_SetRenderDrawColor(r, 10, 10, 10, 220); SDL_RenderFillRect(r, &box);
  const int ts = 2 * scale;
  const std::string heading = victory ? (final_mission ? "CAMPAIGN COMPLETE" : "MISSION COMPLETE") : "MISSION FAILED";
  font.draw(r, (w - font.width(heading, ts)) / 2, h / 3, heading, ts, {255, 230, 150, 255});
  const std::string hint = victory ? "ENTER: CONTINUE   ESC: MENU" : "ENTER: RETRY   ESC: MENU";
  font.draw(r, (w - font.width(hint, ts)) / 2, h / 3 + 20 * scale, hint, ts, {200, 200, 180, 255});
  const char* labels[] = {victory ? (has_next ? "Continue to next briefing" : "Return to menu") : "Retry mission", victory ? "Restart mission" : "Main menu", "Main menu"};
  for (int i = 0; i < (victory ? 3 : 2); ++i) {
    auto rect = button(i, w, h, scale);
    SDL_SetRenderDrawColor(r, 55, 42, 25, 255); SDL_RenderFillRect(r, &rect);
    font.draw(r, rect.x + (rect.w - font.width(labels[i], ts)) / 2, rect.y + (rect.h - font.height(ts)) / 2, labels[i], ts, {245, 230, 200, 255});
  }
}
}  // namespace aztec::engine
