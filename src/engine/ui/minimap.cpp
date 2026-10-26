#include "engine/ui/minimap.hpp"

#include <algorithm>
#include <unordered_map>
#include <vector>

#include "core/palette.hpp"
#include "core/sprite.hpp"
#include "engine/render/colors.hpp"

namespace aztec::engine {

Minimap::~Minimap() { if (tex_) SDL_DestroyTexture(tex_); }

bool Minimap::init(SDL_Renderer* r, const GameData& data, const World& world, int palette, int shade) {
  world_ = &world;
  Palette pal{};
  const Archive& pals = data.archive(ArchiveId::Pal);
  (void)palette;
  if (world.preview.w > 0 && !load_palette(pals, uint32_t(world.preview.palette), uint32_t(shade), pal)) return false;
  std::vector<uint32_t> px;
  int w, h;
  if (world.preview.w > 0 && world.preview.h > 0) {
    w = world.preview.w;
    h = world.preview.h;
    px = to_argb(world.preview, pal);
    for (auto& v : px) v |= 0xFF000000u;
    from_preview_ = true;
  } else {
    // Fallback: one pixel per tile = average colour of the base tile.
    w = world.terrain.w;
    h = world.terrain.h;
    px.assign(size_t(w) * h, 0xFF000000u);
    const Archive& tiles = data.tiles(world.mission.tileset);
    std::unordered_map<uint32_t, uint32_t> avg;
    for (int y = 0; y < h; y++)
      for (int x = 0; x < w; x++) {
        uint32_t t = world.terrain.base(x, y);
        auto it = avg.find(t);
        if (it == avg.end()) {
          uint32_t c = 0xFF000000u;
          Sprite s;
          Palette tp{};
          if (decode_sprite(tiles.get(0x44c, t), s) && load_palette(pals, uint32_t(s.palette), uint32_t(shade), tp)) {
            std::vector<uint32_t> a = to_argb(s, tp);
            uint64_t rr = 0, gg = 0, bb = 0, n = 0;
            for (uint32_t v : a) if (v >> 24) { rr += (v >> 16) & 255; gg += (v >> 8) & 255; bb += v & 255; n++; }
            if (n) c = 0xFF000000u | uint32_t(rr / n) << 16 | uint32_t(gg / n) << 8 | uint32_t(bb / n);
          }
          it = avg.emplace(t, c).first;
        }
        px[size_t(y) * w + x] = it->second;
      }
  }
  tex_ = SDL_CreateTexture(r, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STATIC, w, h);
  if (!tex_) return false;
  SDL_UpdateTexture(tex_, nullptr, px.data(), w * 4);
  return true;
}

void Minimap::layout(int out_w, int out_h, int ui_scale) {
  if (!world_) return;
  int margin = 4 * ui_scale;
  int maxw = std::max(64, std::min(out_w, out_h) * 3 / 10);
  float aspect = float(world_->terrain.w) / world_->terrain.h;
  int w = maxw, h = int(maxw / aspect);
  if (h > maxw) { h = maxw; w = int(maxw * aspect); }
  rect_ = {margin, out_h - margin - h, w, h};
}

void Minimap::layout_in(const SDL_Rect& a) {
  if (!world_) return;
  float aspect = float(world_->terrain.w) / world_->terrain.h;
  int w = a.w, h = int(a.w / aspect);
  if (h > a.h) { h = a.h; w = int(a.h * aspect); }
  rect_ = {a.x + (a.w - w) / 2, a.y + (a.h - h) / 2, w, h};
}

void Minimap::to_world(int sx, int sy, float& wx, float& wy) const {
  wx = float(sx - rect_.x) / rect_.w * world_->width_px();
  wy = float(sy - rect_.y) / rect_.h * world_->height_px();
}

void Minimap::draw(SDL_Renderer* r, const Camera& cam, float alpha, const Fog* fog) const {
  if (!tex_ || !world_) return;
  SDL_Rect b{rect_.x - 2, rect_.y - 2, rect_.w + 4, rect_.h + 4};
  SDL_SetRenderDrawColor(r, 0, 0, 0, 220);
  SDL_RenderFillRect(r, &b);
  SDL_SetTextureColorMod(tex_, tint_.r, tint_.g, tint_.b);
  SDL_RenderCopy(r, tex_, nullptr, &rect_);
  if (fog) fog->draw_minimap(r, rect_);
  float sx = float(rect_.w) / world_->width_px(), sy = float(rect_.h) / world_->height_px();
  for (const Object& o : world_->objects) {
    if (!o.alive || o.removed) continue;
    Vec2f p = o.interpolated(alpha);
    if (fog && fog->enabled() && o.side != world_->local_side && fog->darkness(*world_, int(p.x / kTilePx), int(p.y / kTilePx)) >= 120) continue;
    SDL_Color c = side_color_rgb(world_->side_color(o.side));
    SDL_SetRenderDrawColor(r, c.r, c.g, c.b, 255);
    SDL_Rect d{rect_.x + int(p.x * sx) - 1, rect_.y + int(p.y * sy) - 1, 2, 2};
    SDL_RenderFillRect(r, &d);
  }
  SDL_FRect v{rect_.x + cam.x * sx, rect_.y + cam.y * sy, cam.visible_w() * sx, cam.visible_h() * sy};
  // clip the view rectangle to the minimap
  float x0 = std::max(v.x, float(rect_.x)), y0 = std::max(v.y, float(rect_.y));
  float x1 = std::min(v.x + v.w, float(rect_.x + rect_.w)), y1 = std::min(v.y + v.h, float(rect_.y + rect_.h));
  SDL_FRect c{x0, y0, std::max(1.f, x1 - x0), std::max(1.f, y1 - y0)};
  SDL_SetRenderDrawColor(r, 255, 255, 255, 255);
  SDL_RenderDrawRectF(r, &c);
}

}  // namespace aztec::engine
