#include "engine/render/scene_renderer.hpp"

#include <algorithm>
#include <cmath>

#include "core/palette.hpp"
#include "engine/render/colors.hpp"
#include "engine/world/type_info.hpp"

namespace aztec::engine {

SceneRenderer::SceneRenderer(Window& win, const GameData& data, const World& world)
    : win_(win), data_(data), world_(world), store_(win.renderer(), data) {
  font_ok_ = font_.init(win.renderer());
}

// 0 = per-sprite palette from the tile header (Tiles0 uses Pal 4/5/6, Tiles1 uses 8/9/10).
int SceneRenderer::default_palette() const { return opts.terrain_palette > 0 ? opts.terrain_palette : 0; }

void SceneRenderer::queue(int page, float x0, float y0, float x1, float y1, const SDL_Rect& src) {
  if (page >= int(batches_.size())) batches_.resize(page + 1);
  int ps = store_.atlas().page_size();
  batches_[page].add(x0, y0, x1, y1, src, ps, ps, mod_);
}

void SceneRenderer::set_light(const Light& l) {
  if (!gain_init_) {  // brightness of palette level k relative to level 14 (natural), measured on terrain palette 4
    gain_init_ = true;
    double sum[20] = {};
    for (int k = 0; k < 20; k++) {
      Palette p{};
      if (load_palette(data_.archive(ArchiveId::Pal), 4, uint32_t(k), p))
        for (uint32_t c : p) sum[k] += ((c >> 16) & 255) * 0.3 + ((c >> 8) & 255) * 0.59 + (c & 255) * 0.11;
    }
    for (int k = 0; k < 20; k++) gain_[k] = sum[14] > 0 ? float(sum[k] / sum[14]) : 1.f;
  }
  float lv = std::clamp(l.level, 0.f, 19.f);
  int k0 = int(lv), k1 = std::min(19, k0 + 1);
  float g = gain_[k0] + (gain_[k1] - gain_[k0]) * (lv - k0);
  auto ch = [&](float t) { return Uint8(std::clamp(t * g, 0.f, 1.f) * 255.f + 0.5f); };
  mod_ = {ch(l.rgb[0]), ch(l.rgb[1]), ch(l.rgb[2]), 255};
}

void SceneRenderer::flush(int page) {
  if (page < 0 || page >= int(batches_.size()) || batches_[page].empty()) return;
  stats.geometry_calls++;
  batches_[page].flush(win_.renderer(), store_.atlas().page(page));
}

static float snap(float v) { return std::floor(v + 0.5f); }

void SceneRenderer::draw_terrain(const Camera& cam) {
  SDL_Renderer* r = win_.renderer();
  const TerrainMap& t = world_.terrain;
  ArchiveId arch = world_.mission.tileset == 1 ? ArchiveId::Tiles1 : ArchiveId::Tiles0;
  uint8_t pal = uint8_t(default_palette()), shade = uint8_t(opts.terrain_shade);

  // Base tiles: only the exactly visible range.
  int bx0 = std::max(0, int(std::floor(cam.x / kTilePx))), by0 = std::max(0, int(std::floor(cam.y / kTilePx)));
  int bx1 = std::min(t.w, int(std::ceil((cam.x + cam.visible_w()) / kTilePx)));
  int by1 = std::min(t.h, int(std::ceil((cam.y + cam.visible_h()) / kTilePx)));
  for (int y = by0; y < by1; y++)
    for (int x = bx0; x < bx1; x++) {
      const StoredSprite& s = store_.get({arch, 0x44c, t.base(x, y), pal, shade});
      if (!s.ok()) continue;
      int frame = s.frames > 1 ? int((world_.tick / 2) % uint64_t(s.frames)) : 0;  // water/shore animation
      stats.tiles++;
      float sx0 = snap(cam.to_screen_x(float(x * kTilePx))), sy0 = snap(cam.to_screen_y(float(y * kTilePx)));
      float sx1 = snap(cam.to_screen_x(float((x + 1) * kTilePx))), sy1 = snap(cam.to_screen_y(float((y + 1) * kTilePx)));
      queue(s.region.page, sx0, sy0, sx1, sy1, {s.region.x, s.region.y + frame * kTilePx, kTilePx, kTilePx});
    }
  for (int p = 0; p < int(batches_.size()); p++) flush(p);

  if (opts.passability && world_.passability.w) {
    for (int y = by0; y < by1; y++)
      for (int x = bx0; x < bx1; x++) {
        uint8_t v = world_.passability.at(x, y);
        if (v >= 2) continue;
        SDL_SetRenderDrawColor(r, v == 0 ? 255 : 255, v == 0 ? 0 : 160, 0, 70);
        SDL_FRect f{snap(cam.to_screen_x(float(x * kTilePx))), snap(cam.to_screen_y(float(y * kTilePx))),
                    kTilePx * cam.zoom, kTilePx * cam.zoom};
        SDL_RenderFillRectF(r, &f);
      }
  }
  if (opts.grid) {
    SDL_SetRenderDrawColor(r, 0, 0, 0, 60);
    for (int x = bx0; x <= bx1; x++) {
      float sx = snap(cam.to_screen_x(float(x * kTilePx)));
      SDL_RenderDrawLineF(r, sx, cam.to_screen_y(float(by0 * kTilePx)), sx, cam.to_screen_y(float(by1 * kTilePx)));
    }
    for (int y = by0; y <= by1; y++) {
      float sy = snap(cam.to_screen_y(float(y * kTilePx)));
      SDL_RenderDrawLineF(r, cam.to_screen_x(float(bx0 * kTilePx)), sy, cam.to_screen_x(float(bx1 * kTilePx)), sy);
    }
  }
}

// Draws one image of a stored sprite with its hotspot at world position (wx, wy).
void SceneRenderer::blit(const Camera& cam, const StoredSprite& s, int frame, float wx, float wy, int& cur_page) {
  if (s.region.page != cur_page) { flush(cur_page); cur_page = s.region.page; }
  float x0 = wx - s.ox, y0 = wy - s.oy;
  queue(s.region.page, snap(cam.to_screen_x(x0)), snap(cam.to_screen_y(y0)), snap(cam.to_screen_x(x0 + s.cw)),
        snap(cam.to_screen_y(y0 + s.ch)), s.frame_rect(frame));
}

void SceneRenderer::draw_objects(const Camera& cam, float alpha) {
  SDL_Renderer* r = win_.renderer();
  ArchiveId arch = world_.mission.tileset == 1 ? ArchiveId::Tiles1 : ArchiveId::Tiles0;
  uint8_t pal = uint8_t(default_palette()), shade = uint8_t(opts.terrain_shade);
  struct Item { float wx, wy; const Object* o; int tx, ty; uint16_t tree; };  // tree != 0: terrain overlay sprite
  std::vector<Item> items;
  items.reserve(world_.objects.size() + 256);
  float pad = 96;
  for (const Object& o : world_.objects) {
    Vec2f p = o.interpolated(alpha);
    if (p.x < cam.x - pad || p.y < cam.y - pad || p.x > cam.x + cam.visible_w() + pad || p.y > cam.y + cam.visible_h() + pad) continue;
    if (!visible_in_fog(o, p)) continue;
    items.push_back({p.x, p.y, &o, 0, 0, 0});
  }
  if (opts.overlays) {  // terrain overlay layer = trees/rocks: body in 0x44c, shadow in 0x454, same id
    const TerrainMap& t = world_.terrain;
    int x0 = std::max(0, int(std::floor((cam.x - 96) / kTilePx))), y0 = std::max(0, int(std::floor((cam.y - 96) / kTilePx)));
    int x1 = std::min(t.w, int(std::ceil((cam.x + cam.visible_w() + 96) / kTilePx)));
    int y1 = std::min(t.h, int(std::ceil((cam.y + cam.visible_h() + 224) / kTilePx)));
    for (int y = y0; y < y1; y++)
      for (int x = x0; x < x1; x++)
        if (uint16_t ov = t.overlay(x, y))
          items.push_back({float(x * kTilePx + kTilePx / 2), float(y * kTilePx + kTilePx / 2), nullptr, x, y, ov});
  }
  std::stable_sort(items.begin(), items.end(), [](const Item& a, const Item& b) { return a.wy < b.wy; });

  int cur_page = -1;
  // Pass 1: shadows (translucent, under every body).
  SDL_Color body_mod = mod_;
  mod_ = {255, 255, 255, 255};
  for (const Item& it : items) {
    SpriteDraw sd;
    if (it.tree) {
      const StoredSprite& s = store_.get({arch, 0x454, it.tree, pal, shade, 0, 0, true});
      if (s.ok()) { stats.overlays++; blit(cam, s, int(it.tx * 7 + it.ty * 13), it.wx, it.wy, cur_page); }
    } else if (provider_ && provider_->pick_shadow(object_state(*it.o), sd)) {
      const StoredSprite& s = store_.get(sd.key);
      if (s.ok()) blit(cam, s, sd.key.frame, it.wx + sd.dx, it.wy + sd.dy, cur_page);
    }
  }
  flush(cur_page);
  cur_page = -1;
  mod_ = body_mod;
  // Pass 2: bodies and placeholders, back to front.
  int scale = std::max(1, int(std::lround(cam.zoom)));
  for (const Item& it : items) {
    if (it.tree) {
      const StoredSprite& s = store_.get({arch, 0x44c, it.tree, pal, shade});
      if (s.ok()) { stats.overlays++; blit(cam, s, int(it.tx * 7 + it.ty * 13), it.wx, it.wy, cur_page); }
      continue;
    }
    const Object& o = *it.o;
    SpriteDraw sd;
    if (provider_ && o.type >= 0 && provider_->pick(object_state(o), sd)) {
      const StoredSprite& s = store_.get(sd.key);
      if (s.ok()) {
        stats.sprites++;
        blit(cam, s, sd.key.frame, it.wx + sd.dx, it.wy + sd.dy, cur_page);
        continue;
      }
    }
    if (!opts.markers) continue;
    flush(cur_page);
    cur_page = -1;
    stats.placeholders++;
    const TypeInfo* ti = type_info(o.type);
    bool structure = ti && ti->is_structure();
    float half = (o.type < 0 ? 9.f : structure ? 22.f : 12.f);
    float sx = cam.to_screen_x(it.wx), sy = cam.to_screen_y(it.wy), hs = half * cam.zoom;
    SDL_Color c = side_color_rgb(world_.side_color(o.side));
    SDL_FRect f{snap(sx - hs), snap(sy - hs), snap(hs * 2), snap(hs * 2)};
    SDL_SetRenderDrawColor(r, 0, 0, 0, 255);
    SDL_FRect b{f.x - 1, f.y - 1, f.w + 2, f.h + 2};
    SDL_RenderFillRectF(r, &b);
    if (o.type < 0) {
      SDL_SetRenderDrawColor(r, c.r, c.g, c.b, 255);
      SDL_RenderDrawRectF(r, &f);
    } else {
      SDL_SetRenderDrawColor(r, c.r, c.g, c.b, 235);
      SDL_RenderFillRectF(r, &f);
    }
    if (opts.labels && font_ok_) {
      std::string label = o.type >= 0 ? std::to_string(o.type) : (o.source == ObjectSource::Boat ? "BT" : o.source == ObjectSource::Johnka ? "JK" : "SUB");
      int w = font_.width(label, scale), h = font_.height(scale);
      bool dark = (c.r + c.g + c.b) > 450;
      font_.draw(r, int(sx) - w / 2, int(sy) - h / 2, label, scale, dark ? SDL_Color{10, 10, 10, 255} : SDL_Color{255, 255, 255, 255}, false);
    }
  }
  flush(cur_page);
}

ObjectState SceneRenderer::object_state(const Object& o) const {
  ObjectState st;
  st.type_id = o.type;
  st.side = o.side;
  st.side_color = world_.side_color(o.side);
  st.facing = o.facing;
  st.anim_state = o.anim_state;
  st.anim_tick = o.anim_tick;
  st.anim_variant = o.anim_variant;
  return st;
}

bool SceneRenderer::visible_in_fog(const Object& o, Vec2f p) const {
  if (!fog_.enabled() || o.side == world_.local_side) return true;
  return fog_.darkness(world_, int(p.x / kTilePx), int(p.y / kTilePx)) < 120;  // hide other sides' objects in the dark
}

void SceneRenderer::draw(const Camera& cam, float alpha) {
  stats = {};
  SDL_Renderer* r = win_.renderer();
  SDL_SetRenderDrawColor(r, 8, 10, 14, 255);
  SDL_RenderClear(r);
  draw_terrain(cam);
  // Start positions
  if (opts.markers) {
    for (const SideDef& s : world_.mission.sides) {
      if (s.start_x <= 0 && s.start_y <= 0) continue;
      float cx = cam.to_screen_x(float(s.start_x * kTilePx + kTilePx / 2)), cy = cam.to_screen_y(float(s.start_y * kTilePx + kTilePx / 2));
      float rr = 20 * cam.zoom;
      SDL_Color c = side_color_rgb(s.color);
      SDL_SetRenderDrawColor(r, c.r, c.g, c.b, 255);
      SDL_RenderDrawLineF(r, cx - rr, cy, cx + rr, cy);
      SDL_RenderDrawLineF(r, cx, cy - rr, cx, cy + rr);
    }
  }
  draw_objects(cam, alpha);
  fog_.update(r, world_);
  fog_.draw(r, cam, world_);
}

}  // namespace aztec::engine
