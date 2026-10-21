#include "engine/ui/panel.hpp"

#include <algorithm>

namespace aztec::engine {

static const int kPanelId = 292;         // common.xbe 204x493 panel (palette 48)
static const int kPanelW = 204, kPanelH = 493;
static const int kBar = 24;              // top bar (menu button / gold)
static const int kStone = 445;           // first row of the repeatable stone strip at the bottom

static SpriteKey key() {
  SpriteKey k;
  k.archive = ArchiveId::Common;
  k.group = 0x44c;
  k.index = kPanelId;
  k.shade = 0;
  return k;
}

bool Panel::init(SpriteStore& store) {
  const StoredSprite& s = store.get(key());
  ok_ = s.ok() && s.cw == kPanelW && s.ch == kPanelH;
  return ok_;
}

void Panel::layout(int out_w, int out_h, int scale) {
  scale_ = std::max(1, scale);
  rect_ = {out_w - kPanelW * scale_, 0, kPanelW * scale_, out_h};
  mini_ = {rect_.x + 12 * scale_, 28 * scale_, 180 * scale_, 176 * scale_};
}

void Panel::draw(SDL_Renderer* r, SpriteStore& store, const Font& font, int gold, const std::vector<std::string>& info,
                 const std::string& tooltip) const {
  if (!ok_) return;
  const StoredSprite& s = store.get(key());
  SDL_Texture* tex = store.atlas().page(s.region.page);
  int sc = scale_;
  // Top part: the whole sprite (or as much of it as fits).
  int rows = std::min(kPanelH, (rect_.h + sc - 1) / sc);
  SDL_Rect src{s.region.x, s.region.y, kPanelW, rows};
  SDL_Rect dst{rect_.x, 0, kPanelW * sc, rows * sc};
  SDL_RenderCopy(r, tex, &src, &dst);
  // Taller windows: repeat the bottom stone strip.
  int y = kPanelH * sc;
  while (y < rect_.h) {
    int h = std::min((kPanelH - kStone) * sc, rect_.h - y);
    SDL_Rect ss{s.region.x, s.region.y + kStone, kPanelW, (h + sc - 1) / sc};
    SDL_Rect dd{rect_.x, y, kPanelW * sc, ss.h * sc};
    SDL_RenderCopy(r, tex, &ss, &dd);
    y += h;
  }
  int ts = std::max(1, sc * 2 / 2);
  font.draw(r, rect_.x + 14 * sc, (kBar - 5 * ts) / 2 * sc, "MENU", ts, {235, 220, 170, 255});
  std::string g = std::to_string(gold);
  font.draw(r, rect_.x + (kPanelW - 24) * sc - font.width(g, ts), (kBar - 5 * ts) / 2 * sc, g, ts, {250, 220, 90, 255});
  int lh = std::max(12, font.height(ts * 2)) + 2 * sc;
  int y0 = 214 * sc + 8 * sc;
  for (size_t i = 0; i < info.size(); i++)
    font.draw(r, rect_.x + 24 * sc, y0 + int(i) * lh, info[i], ts * 2, i == 0 ? SDL_Color{255, 240, 190, 255} : SDL_Color{230, 230, 230, 255});
  if (!tooltip.empty()) draw_tooltip(r, font, tooltip);
}

void Panel::draw_tooltip(SDL_Renderer* r, const Font& font, const std::string& tip) const {
  std::string tooltip = tip;
  for (char& c : tooltip) if (c == '\n') c = ' ';
  int sc = scale_;
  int ts = std::max(1, sc);
  int lh = std::max(12, font.height(ts * 2)) + 2 * sc;
  int tw = font.width(tooltip, ts * 2) + 8 * sc, th = lh + 4 * sc;
  SDL_Rect b{rect_.x - tw - 4 * sc, rect_.h - th - 8 * sc, tw, th};
  SDL_SetRenderDrawColor(r, 20, 16, 10, 230);
  SDL_RenderFillRect(r, &b);
  SDL_SetRenderDrawColor(r, 200, 170, 100, 255);
  SDL_RenderDrawRect(r, &b);
  font.draw(r, b.x + 4 * sc, b.y + 2 * sc, tooltip, ts * 2, {255, 240, 190, 255});
}

}  // namespace aztec::engine
