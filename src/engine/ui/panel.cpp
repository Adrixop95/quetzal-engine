#include "engine/ui/panel.hpp"

#include <algorithm>

namespace aztec::engine {

static const int kPanelId = 292;         // common.xbe 204x493 panel (palette 48)
static const int kPanelW = 204, kPanelH = 493;
static const int kStone = 445;           // first row of the repeatable stone strip at the bottom

static SpriteKey key(int id = kPanelId) {
  SpriteKey k;
  k.archive = ArchiveId::Common;
  k.group = 0x44c;
  k.index = id;
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
  mini_ = in_game_panel_layout(rect_, scale_).minimap;
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
  const auto layout = in_game_panel_layout(rect_, sc);
  auto sprite = [&](int id, SDL_Rect box) {
    const auto& image = store.get(key(id));
    if (!image.ok()) return false;
    SDL_Rect source = image.frame_rect(0);
    SDL_RenderCopy(r, store.atlas().page(image.region.page), &source, &box);
    return true;
  };
  // The short command card is a separate 254x208 resource, anchored to the bottom.
  if (!sprite(297, layout.card)) {
    SDL_SetRenderDrawColor(r, 45, 40, 32, 255);
    SDL_RenderFillRect(r, &layout.card);
  }
  int ts = font.ui_scale(sc);
  if (!sprite(282, layout.menu))
    font.draw(r, layout.menu.x + 4 * sc, layout.menu.y + 4 * sc, "MENU", ts, {235, 220, 170, 255});
  std::string g = std::to_string(gold);
  font.draw(r, rect_.x + (kPanelW - 12) * sc - font.width(g, ts),
            rect_.y + std::max(0, (22 * sc - font.height(ts)) / 2), g, ts, {250, 220, 90, 255});
  int y0 = layout.summary.y;
  for (const auto& line : info) {
    if (y0 + font.height(ts) > layout.summary.y + layout.summary.h) break;
    font.draw(r, layout.summary.x, y0, line, ts, {255, 240, 190, 255});
    y0 += font.height(ts) + 2 * sc;
  }
  if (!tooltip.empty()) draw_tooltip(r, font, tooltip);
}

bool Panel::contains(int x, int y) const {
  SDL_Point point{x, y};
  const auto card = in_game_panel_layout(rect_, scale_).card;
  return SDL_PointInRect(&point, &rect_) || SDL_PointInRect(&point, &card);
}

void Panel::draw_tooltip(SDL_Renderer* r, const Font& font, const std::string& tip) const {
  int sc = scale_;
  int ts = font.ui_scale(sc);
  int lh = font.height(ts) + 4 * sc;
  const int edge = in_game_panel_layout(rect_, sc).card.x;
  int maxw = std::max(sc, std::min(360 * sc, edge - 20 * sc));
  auto lines = font.wrap(tip, ts, maxw);
  const int limit = std::max(1, (rect_.h - 24 * sc) / lh);
  if (int(lines.size()) > limit) lines.resize(size_t(limit));
  int tw = 0;
  for (const auto& text : lines) tw = std::max(tw, font.width(text, ts));
  tw += 12 * sc;
  int th = int(lines.size()) * lh + 8 * sc;
  SDL_Rect b{std::max(0, edge - tw - 4 * sc), std::max(0, rect_.y + rect_.h - th - 8 * sc), tw, th};
  SDL_SetRenderDrawColor(r, 20, 16, 10, 240);
  SDL_RenderFillRect(r, &b);
  SDL_SetRenderDrawColor(r, 200, 170, 100, 255);
  SDL_RenderDrawRect(r, &b);
  for (size_t i = 0; i < lines.size(); i++)
    font.draw(r, b.x + 6 * sc, b.y + 4 * sc + int(i) * lh, lines[i], ts, {255, 240, 190, 255});
}

}  // namespace aztec::engine
