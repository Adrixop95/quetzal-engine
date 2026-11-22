#include "engine/ui/minimap.hpp"
#include "engine/world/visibility.hpp"

#include <algorithm>
#include <unordered_map>
#include <vector>

#include "core/palette.hpp"
#include "core/sprite.hpp"
#include "engine/render/colors.hpp"

namespace aztec::engine {

Minimap::~Minimap() {
  if (tex_) SDL_DestroyTexture(tex_);
  if (fog_tex_) SDL_DestroyTexture(fog_tex_);
}

bool Minimap::init(SDL_Renderer* r, const GameData& data, const World& world, int palette, int shade) {
  if (tex_) { SDL_DestroyTexture(tex_); tex_ = nullptr; }
  if (fog_tex_) { SDL_DestroyTexture(fog_tex_); fog_tex_ = nullptr; }
  for (auto& pixels : preview_shades_) pixels.clear();
  from_preview_ = false;
  world_ = &world;
  default_shade_ = std::clamp(shade, 0, 19);
  Palette pal{};
  const Archive& pals = data.archive(ArchiveId::Pal);
  // G: grayscale fallback when the user's minimap palette is unavailable.
  for (int i = 0; i < 256; ++i) {
    uint32_t value = uint32_t(std::min(i, 19) * 255 / 19);
    fog_palette_[i] = 0xff000000u | value * 0x010101u;
  }
  load_palette(pals, 15, 0, fog_palette_);
  fog_tex_ = SDL_CreateTexture(r, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STATIC, 180, 180);
  if (!fog_tex_) return false;
  SDL_SetTextureScaleMode(fog_tex_, SDL_ScaleModeNearest);
  (void)palette;
  if (world.preview.w > 0 && !load_palette(pals, uint32_t(world.preview.palette), uint32_t(shade), pal)) return false;
  std::vector<uint32_t> px;
  int w, h;
  if (world.preview.w > 0 && world.preview.h > 0) {
    w = world.preview.w;
    h = world.preview.h;
    px = to_argb(world.preview, pal);
    for (auto& v : px) v |= 0xFF000000u;
    for (int level = 0; level < 20; ++level) {
      Palette colors{};
      if (!load_palette(pals, uint32_t(world.preview.palette), uint32_t(level), colors)) return false;
      preview_shades_[level] = to_argb(world.preview, colors);
      for (auto& color : preview_shades_[level]) color |= 0xFF000000u;
    }
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
  if (w <= 0 || h <= 0) return false;
  texture_w_ = w; texture_h_ = h;
  tex_ = SDL_CreateTexture(r, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STATIC, w, h);
  if (!tex_) return false;
  SDL_SetTextureScaleMode(tex_, SDL_ScaleModeNearest);
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

void Minimap::layout_in(const SDL_Rect& a, int ui_scale) {
  if (!world_) return;
  float aspect = float(world_->terrain.w) / world_->terrain.h;
  int w = std::min(a.w, world_->terrain.w * std::max(1, ui_scale)), h = int(w / aspect);
  if (h > a.h) { h = a.h; w = int(a.h * aspect); }
  rect_ = {a.x + (a.w - w) / 2, a.y + (a.h - h) / 2, w, h};
}

void Minimap::to_world(int sx, int sy, float& wx, float& wy) const {
  wx = wy = 0;
  if (!world_ || rect_.w <= 0 || rect_.h <= 0) return;
  wx = float(std::clamp(sx - rect_.x, 0, rect_.w - 1)) / rect_.w * world_->width_px();
  wy = float(std::clamp(sy - rect_.y, 0, rect_.h - 1)) / rect_.h * world_->height_px();
}

void Minimap::draw(SDL_Renderer* r, const Camera& cam, float alpha, const Fog* fog) const {
  if (!tex_ || !world_) return;
  SDL_Rect b{rect_.x - 2, rect_.y - 2, rect_.w + 4, rect_.h + 4};
  SDL_SetRenderDrawColor(r, 0, 0, 0, 220);
  SDL_RenderFillRect(r, &b);
  // Keep edge markers and camera outlines within the map, while respecting
  // an enclosing UI clip. The decorative frame above retains the caller's clip.
  SDL_Rect previous{};
  const bool clipped = SDL_RenderIsClipEnabled(r);
  SDL_RenderGetClipRect(r, &previous);
  SDL_Rect clip = rect_;
  if (clipped && !SDL_IntersectRect(&rect_, &previous, &clip)) return;
  SDL_RenderSetClipRect(r, &clip);
  // V: the raw fog byte >>3 is the minimap palette index.
  // The minimap does not interpolate the world view's fog edge between vertices.
  const bool raw = world_->visibility_brightness && fog && fog->requested();
  if (raw) {
    const auto* layer = visibility_layer(*world_, world_->local_side);
    std::vector<uint32_t> pixels(180 * 180, fog_palette_[0]);
    for (int y = 0; y < 180; ++y) for (int x = 0; x < 180; ++x) {
      const int tx = x * world_->terrain.w / 180, ty = y * world_->terrain.h / 180;
      const int brightness = layer ? layer->v[size_t(ty) * layer->w + tx] : 0;
      pixels[size_t(y) * 180 + x] = fog_palette_[std::min(19, brightness >> 3)];
    }
    SDL_UpdateTexture(fog_tex_, nullptr, pixels.data(), 180 * 4);
    SDL_RenderCopy(r, fog_tex_, nullptr, &rect_);
  } else {
    if (from_preview_) {
      std::vector<uint32_t> pixels = preview_shades_[default_shade_];
      SDL_UpdateTexture(tex_, nullptr, pixels.data(), texture_w_ * 4);
    }
    SDL_SetTextureColorMod(tex_, tint_.r, tint_.g, tint_.b);
    SDL_RenderCopy(r, tex_, nullptr, &rect_);
    if (fog) fog->draw_minimap(r, rect_);
  }
  float sx = float(rect_.w) / world_->width_px(), sy = float(rect_.h) / world_->height_px();
  std::vector<const Object*> displayed;
  const bool proxies = raw && world_->local_side >= 0 && world_->local_side < int(world_->building_memory.size());
  for (const auto& o : world_->objects)
    if (!proxies || !visibility_structure(*world_, o)) displayed.push_back(&o);
  if (proxies) for (const auto& [id, picture] : world_->building_memory[world_->local_side]) {
    (void)id; displayed.push_back(&picture);
  }
  for (const Object* picture : displayed) {
    const Object& o = *picture;
    if (!o.alive || o.removed || o.source == ObjectSource::Submarine) continue;
    Vec2f p = o.interpolated(alpha);
    const bool structure = visibility_structure(*world_, o);
    if (!(proxies && structure) && fog && fog->requested() && !object_visible(*world_, world_->local_side, o)) continue;
    SDL_Color c = side_color_rgb(world_->side_color(o.side));
    SDL_SetRenderDrawColor(r, c.r, c.g, c.b, 255);
    const int extent = std::max(2, rect_.w / std::max(1, world_->terrain.w));
    auto dot = [&](float x, float y) {
      SDL_Rect d{rect_.x + int(x * sx) - 1, rect_.y + int(y * sy) - 1, extent, extent};
      SDL_RenderFillRect(r, &d);
    };
    if (structure && world_->visibility_rules) {
      const auto& rule = world_->visibility_rules->type(o.type);
      const int tx = (o.cur_x >> 8) / kTilePx, ty = (o.cur_y >> 8) / kTilePx;
      if (rule.has_footprint_mask) {
        for (int i = 0; i < 49; ++i) if (rule.footprint_mask[i] == 2)
          dot(float((tx + i % 7 - 3) * kTilePx), float((ty + i / 7 - 3) * kTilePx));
      } else {
        for (int y = 0; y < rule.foot_h; ++y) for (int x = 0; x < rule.foot_w; ++x)
          dot(float((tx - rule.foot_w / 2 + x) * kTilePx), float((ty - rule.foot_h / 2 + y) * kTilePx));
      }
    } else dot(p.x, p.y);
  }
  SDL_FRect v{rect_.x + cam.x * sx, rect_.y + cam.y * sy, cam.visible_w() * sx, cam.visible_h() * sy};
  // clip the view rectangle to the minimap
  float x0 = std::max(v.x, float(rect_.x)), y0 = std::max(v.y, float(rect_.y));
  float x1 = std::min(v.x + v.w, float(rect_.x + rect_.w)), y1 = std::min(v.y + v.h, float(rect_.y + rect_.h));
  SDL_FRect c{x0, y0, std::max(1.f, x1 - x0), std::max(1.f, y1 - y0)};
  SDL_SetRenderDrawColor(r, 255, 255, 255, 255);
  SDL_RenderDrawRectF(r, &c);
  SDL_RenderSetClipRect(r, clipped ? &previous : nullptr);
}

}  // namespace aztec::engine
