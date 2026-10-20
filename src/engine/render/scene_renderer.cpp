#include "engine/render/scene_renderer.hpp"

#include <algorithm>
#include <cmath>

#include "engine/render/colors.hpp"
#include "engine/world/type_info.hpp"

namespace aztec::engine {

SceneRenderer::SceneRenderer(Window& win, const GameData& data, const World& world)
    : win_(win), data_(data), world_(world), store_(win.renderer(), data) {
  font_ok_ = font_.init(win.renderer());
}

int SceneRenderer::default_palette() const {
  if (opts.terrain_palette >= 0) return opts.terrain_palette;
  return world_.mission.tileset == 1 ? 4 : 4;  // palette 4 gives natural colours for Tiles0; see README for Tiles1
}

void SceneRenderer::queue(int page, float x0, float y0, float x1, float y1, const StoredSprite& s) {
  if (page >= int(batches_.size())) batches_.resize(page + 1);
  int ps = store_.atlas().page_size();
  batches_[page].add(x0, y0, x1, y1, SDL_Rect{s.region.x, s.region.y, s.region.w, s.region.h}, ps, ps);
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
  int margin = opts.overlays ? 3 : 0;
  int x0 = std::max(0, int(std::floor(cam.x / kTilePx)) - margin), y0 = std::max(0, int(std::floor(cam.y / kTilePx)) - margin);
  int x1 = std::min(t.w, int(std::ceil((cam.x + cam.visible_w()) / kTilePx)) + margin);
  int y1 = std::min(t.h, int(std::ceil((cam.y + cam.visible_h()) / kTilePx)) + margin);

  // Base tiles: only the exactly visible range.
  int bx0 = std::max(0, int(std::floor(cam.x / kTilePx))), by0 = std::max(0, int(std::floor(cam.y / kTilePx)));
  int bx1 = std::min(t.w, int(std::ceil((cam.x + cam.visible_w()) / kTilePx)));
  int by1 = std::min(t.h, int(std::ceil((cam.y + cam.visible_h()) / kTilePx)));
  for (int y = by0; y < by1; y++)
    for (int x = bx0; x < bx1; x++) {
      const StoredSprite& s = store_.get({arch, 0x44c, t.base(x, y), pal, shade});
      if (!s.ok()) continue;
      stats.tiles++;
      float sx0 = snap(cam.to_screen_x(float(x * kTilePx))), sy0 = snap(cam.to_screen_y(float(y * kTilePx)));
      float sx1 = snap(cam.to_screen_x(float((x + 1) * kTilePx))), sy1 = snap(cam.to_screen_y(float((y + 1) * kTilePx)));
      queue(s.region.page, sx0, sy0, sx1, sy1, s);
    }
  for (int p = 0; p < int(batches_.size()); p++) flush(p);

  if (opts.overlays) {
    for (int y = y0; y < y1; y++)
      for (int x = x0; x < x1; x++) {
        uint16_t ov = t.overlay(x, y);
        if (!ov) continue;
        const StoredSprite& s = store_.get({arch, 0x454, ov, pal, shade});
        if (!s.ok()) continue;
        stats.overlays++;
        float wx = float(x * kTilePx + s.ox), wy = float(y * kTilePx + s.oy);
        queue(s.region.page, snap(cam.to_screen_x(wx)), snap(cam.to_screen_y(wy)),
              snap(cam.to_screen_x(wx + s.region.w)), snap(cam.to_screen_y(wy + s.region.h)), s);
      }
    for (int p = 0; p < int(batches_.size()); p++) flush(p);
  }

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

void SceneRenderer::draw_objects(const Camera& cam, float alpha) {
  SDL_Renderer* r = win_.renderer();
  struct Item { float wx, wy; const Object* o; };
  std::vector<Item> items;
  items.reserve(world_.objects.size());
  float pad = 64;
  for (const Object& o : world_.objects) {
    Vec2f p = o.interpolated(alpha);
    if (p.x < cam.x - pad || p.y < cam.y - pad || p.x > cam.x + cam.visible_w() + pad || p.y > cam.y + cam.visible_h() + pad) continue;
    items.push_back({p.x, p.y, &o});
  }
  std::stable_sort(items.begin(), items.end(), [](const Item& a, const Item& b) { return a.wy < b.wy; });

  int cur_page = -1;
  int scale = std::max(1, int(std::lround(cam.zoom)));
  for (const Item& it : items) {
    const Object& o = *it.o;
    SpriteDraw sd;
    ObjectState st{o.type, o.side, world_.side_color(o.side), o.facing, o.anim_state, o.anim_tick};
    if (provider_ && o.type >= 0 && provider_->pick(st, sd)) {
      const StoredSprite& s = store_.get(sd.key);
      if (s.ok()) {
        stats.sprites++;
        if (s.region.page != cur_page) { flush(cur_page); cur_page = s.region.page; }
        float wx = it.wx + s.ox + sd.dx, wy = it.wy + s.oy + sd.dy;
        queue(s.region.page, snap(cam.to_screen_x(wx)), snap(cam.to_screen_y(wy)),
              snap(cam.to_screen_x(wx + s.region.w)), snap(cam.to_screen_y(wy + s.region.h)), s);
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
      // boats/johnkas/submarines: hollow marker so they differ from typed units
      SDL_SetRenderDrawColor(r, c.r, c.g, c.b, 255);
      SDL_RenderDrawRectF(r, &f);
    } else {
      SDL_SetRenderDrawColor(r, c.r, c.g, c.b, 235);
      SDL_RenderFillRectF(r, &f);
    }
    if (opts.labels && font_ok_) {
      std::string label = o.type >= 0 ? std::to_string(o.type) : (o.source == ObjectSource::Boat ? "BT" : o.source == ObjectSource::Johnka ? "JK" : "SUB");
      int w = font_.width(label, scale), h = font_.height(scale);
      int tx = int(sx) - w / 2, ty = int(sy) - h / 2;
      bool dark = (c.r + c.g + c.b) > 450;
      font_.draw(r, tx, ty, label, scale, dark ? SDL_Color{10, 10, 10, 255} : SDL_Color{255, 255, 255, 255}, false);
    }
  }
  flush(cur_page);
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
}

}  // namespace aztec::engine
