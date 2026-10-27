#include "engine/render/scene_renderer.hpp"

#include <algorithm>
#include <cmath>

#include "engine/data/animation_slots.hpp"
#include "engine/render/colors.hpp"
#include "engine/world/type_info.hpp"

namespace aztec::engine {

SceneRenderer::SceneRenderer(Window& win, const GameData& data, const World& world)
    : win_(win), data_(data), world_(world), store_(win.renderer(), data) {
  font_ok_ = font_.init(win.renderer(), &store_);
}

// 0 = per-sprite palette from the tile header (Tiles0 uses Pal 4/5/6, Tiles1 uses 8/9/10).
int SceneRenderer::default_palette() const { return opts.terrain_palette > 0 ? opts.terrain_palette : 0; }

void SceneRenderer::queue(int page, float x0, float y0, float x1, float y1, const SDL_Rect& src) {
  if (page >= int(batches_.size())) batches_.resize(page + 1);
  int ps = store_.atlas().page_size();
  batches_[page].add(x0, y0, x1, y1, src, ps, ps, mod_);
}

void SceneRenderer::set_light(const Light& l) {
  if (light_.level != l.level) lighting_tick_ = uint64_t(-1);
  light_ = l;
  auto ch = [](float value) { return Uint8(std::clamp(value, 0.f, 1.f) * 255.f + 0.5f); };
  mod_ = {ch(l.rgb[0]), ch(l.rgb[1]), ch(l.rgb[2]), 255};
}

void SceneRenderer::update_lighting() {
  mask_lighting_ = data::LightMask(data_.archive(ArchiveId::Light).get(data::kLightGroup, 1)).ok();
  if (!mask_lighting_) return;
  if (lighting_side_ != world_.local_side) {
    lighting_ = LightGrid{};
    lighting_side_ = world_.local_side;
    lighting_tick_ = uint64_t(-1);
  }
  // Stamp at simulation positions, never projected body positions. Rebuilding at
  // a view frame must not count as an additional persistent exploration tick.
  if (lighting_tick_ == world_.tick && lighting_vision_ == fog_.enabled()) return;
  lighting_vision_ = fog_.enabled();
  lighting_tick_ = world_.tick;
  lighting_.reset(world_.terrain.w, world_.terrain.h, int(std::clamp(light_.level, 0.f, 19.f) * 8));
  for (const auto& o : world_.objects) {
    if (o.removed || !provider_) continue;
    const auto st = object_state(o);
    const float x = o.cur_x / 256.f, y = o.cur_y / 256.f;
    const auto& archive = data_.archive(ArchiveId::Light);
    for (const auto& item : provider_->items(st))
      if (item.light >= data::kFirstGlowMask && item.light <= data::kLastGlowMask)
        lighting_.stamp(data::LightMask(archive.get(data::kLightGroup, item.light)), x + item.dx, y + item.dy, false);
    const int vision = provider_->vision_mask(st);
    if (o.alive && o.side == world_.local_side && vision >= data::kFirstVisionMask && vision <= data::kLastVisionMask)
      lighting_.stamp(data::LightMask(archive.get(data::kLightGroup, vision)), x, y, true);
  }
  lighting_.combine(fog_.enabled());
}
uint8_t SceneRenderer::shade_at(float x, float y) const {
  return uint8_t(mask_lighting_ ? data::palette_shade(int(lighting_.sample(x, y))) : opts.terrain_shade);
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
  uint8_t pal = uint8_t(default_palette());

  // Base tiles: only the exactly visible range.
  int bx0 = std::max(0, int(std::floor(cam.x / kTilePx))), by0 = std::max(0, int(std::floor(cam.y / kTilePx)));
  int bx1 = std::min(t.w, int(std::ceil((cam.x + cam.visible_w()) / kTilePx)));
  int by1 = std::min(t.h, int(std::ceil((cam.y + cam.visible_h()) / kTilePx)));
  for (int y = by0; y < by1; y++)
    for (int x = bx0; x < bx1; x++) {
      const StoredSprite& s = store_.get({arch, 0x44c, t.base(x, y), pal, shade_at(float(x * kTilePx + 24), float(y * kTilePx + 24))});
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
  uint8_t pal = uint8_t(default_palette());
  struct Item { float wx, wy; const Object* o; int height; uint16_t tree; std::vector<SpriteDraw> parts; bool primary = true; };  // tree != 0: terrain overlay sprite
  std::vector<Item> items;
  items.reserve(world_.objects.size() + 256);
  float pad = 96;
  for (const Object& o : world_.objects) {
    if (o.removed) continue;
    Vec2f p = o.interpolated(alpha);
    if (p.x < cam.x - pad || p.y < cam.y - pad || p.x > cam.x + cam.visible_w() + pad || p.y > cam.y + cam.visible_h() + pad) continue;
    if (!visible_in_fog(o, p)) continue;
    if (provider_) {
      bool primary = true;
      for (auto item : provider_->items(object_state(o))) {
        items.push_back({p.x + item.dx, p.y + item.dy, &o, item.height, 0, std::move(item.parts), primary});
        primary = false;
      }
    } else items.push_back({p.x, p.y, &o, 0, 0, {}});
  }
  if (opts.overlays) {  // terrain overlay layer = trees/rocks: body in 0x44c, shadow in 0x454, same id
    const TerrainMap& t = world_.terrain;
    int x0 = std::max(0, int(std::floor((cam.x - 96) / kTilePx))), y0 = std::max(0, int(std::floor((cam.y - 96) / kTilePx)));
    int x1 = std::min(t.w, int(std::ceil((cam.x + cam.visible_w() + 96) / kTilePx)));
    int y1 = std::min(t.h, int(std::ceil((cam.y + cam.visible_h() + 224) / kTilePx)));
    for (int y = y0; y < y1; y++)
      for (int x = x0; x < x1; x++)
        if (uint16_t ov = t.overlay(x, y))
          items.push_back({float(x * kTilePx + kTilePx / 2), float(y * kTilePx + kTilePx / 2), nullptr, 0, ov, {}});
  }
  std::stable_sort(items.begin(), items.end(), [](const Item& a, const Item& b) { return display_before(a.wx, a.wy, a.height, b.wx, b.wy, b.height); });

  int cur_page = -1;
  SDL_Color body_mod = mod_;
  // Selection rings go on the ground, under bodies.
  mod_ = {255, 255, 255, 255};
  for (const Item& it : items) {
    if (it.tree || !it.primary || !world_.selection.contains(it.o->id)) continue;
    const TypeInfo* ti = type_info(it.o->type);
    SpriteKey k;
    k.archive = ArchiveId::Common; k.group = 0x44c; k.shade = 0;
    k.index = ti && ti->is_structure() ? 314 : 309;
    const StoredSprite& s = store_.get(k);
    if (!s.ok()) continue;
    int f = int((world_.tick / 2) % uint64_t(std::max(1, s.frames)));
    if (s.region.page != cur_page) { flush(cur_page); cur_page = s.region.page; }
    float x0 = it.wx - s.cw / 2.f, y0 = it.wy - s.ch / 2.f;
    queue(s.region.page, snap(cam.to_screen_x(x0)), snap(cam.to_screen_y(y0)), snap(cam.to_screen_x(x0 + s.cw)),
          snap(cam.to_screen_y(y0 + s.ch)), s.frame_rect(f));
  }
  flush(cur_page);
  cur_page = -1;
  mod_ = body_mod;
  // Shared ordered list: finish every item before starting the next one.
  int scale = std::max(1, int(std::lround(cam.zoom)));
  for (const Item& it : items) {
    if (it.tree) {
      for (bool shadow : {true, false}) {
        const StoredSprite& s = store_.get({arch, uint16_t(shadow ? data::kShadowGroup : data::kBodyGroup),
            it.tree, pal, shadow ? uint8_t(data::kShadowShade) : shade_at(it.wx, it.wy), 0, 0, shadow});
        mod_ = shadow ? SDL_Color{255, 255, 255, 255} : body_mod;
        if (s.ok()) { stats.overlays++; blit(cam, s, data::overlay_frame(opts.wind, opts.wind_counter), it.wx, it.wy, cur_page); }
      }
      continue;
    }
    const Object& o = *it.o;
    bool drawn = false;
    const auto st = object_state(o);
    for (auto sd : it.parts) {
      if (!sd.key.shadow) sd.key.shade = shade_at(it.wx, it.wy);
      mod_ = sd.key.shadow ? SDL_Color{255, 255, 255, 255} : body_mod;
      const StoredSprite& s = store_.get(sd.key);
      if (s.ok()) {
        drawn = true; stats.sprites++;
        blit(cam, s, sd.key.frame, it.wx + sd.dx, it.wy + sd.dy, cur_page);
      }
    }
    if (drawn || !it.primary || (provider_ && provider_->hides_missing(st))) continue;
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
  mod_ = body_mod;
  // Health bars of selected objects.
  for (const Item& it : items) {
    if (it.tree || !it.primary || !world_.selection.contains(it.o->id) || it.o->type < 0) continue;
    const TypeInfo* ti = type_info(it.o->type);
    bool structure = ti && ti->is_structure();
    int hpmax = hp_max(it.o->type);
    float frac = hpmax > 0 ? std::clamp(float(it.o->hp) / hpmax, 0.f, 1.f) : 1.f;
    float w = (structure ? 70.f : 26.f) * cam.zoom, h = std::max(3.f, 4.f * cam.zoom);
    float x = cam.to_screen_x(it.wx) - w / 2, y = cam.to_screen_y(it.wy - (structure ? 110.f : 46.f));
    SDL_FRect bg{x - 1, y - 1, w + 2, h + 2}, fg{x, y, w * frac, h};
    SDL_SetRenderDrawColor(r, 0, 0, 0, 200);
    SDL_RenderFillRectF(r, &bg);
    SDL_SetRenderDrawColor(r, frac > 0.6f ? 60 : 230, frac > 0.3f ? 210 : 50, frac > 0.6f ? 60 : 40, 255);
    SDL_RenderFillRectF(r, &fg);
  }
}

int SceneRenderer::hp_max(int type) const {
  auto it = hp_max_.find(type);
  if (it != hp_max_.end()) return it->second;
  int m = 0;
  for (const Object& o : world_.objects)
    if (o.type == type) m = std::max(m, o.hp);
  return hp_max_[type] = m;
}

ObjectState SceneRenderer::object_state(const Object& o) const {
  ObjectState st;
  st.type_id = o.type;
  if (o.type < 0 && o.source == ObjectSource::Boat) st.type_id = -2;
  if (o.type < 0 && o.source == ObjectSource::Johnka) st.type_id = -3;
  st.hp = o.hp; st.max_hp = o.hp_max;
  st.moving = o.cur_x != o.prev_x || o.cur_y != o.prev_y;
  st.initial_turret = !o.has_fired;
  st.height = o.render_height;
  st.destruction_radius = o.destruction_radius;
  st.construction_overlay = o.construction_overlay;
  st.transport_phase = o.transport_phase; st.orientation = o.orientation;
  st.side = o.side;
  st.side_color = world_.side_color(o.side);
  st.facing = o.facing;
  st.anim_state = o.anim_state;
  st.anim_tick = o.anim_tick;
  st.anim_variant = o.anim_variant;
  st.progress = o.progress;
  return st;
}

bool SceneRenderer::visible_in_fog(const Object& o, Vec2f p) const {
  if (!fog_.enabled() || o.side == world_.local_side) return true;
  if (mask_lighting_) return lighting_.sample(p.x, p.y) > 32;
  return fog_.darkness(world_, int(p.x / kTilePx), int(p.y / kTilePx)) < 120;  // hide other sides' objects in the dark
}

void SceneRenderer::draw(const Camera& cam, float alpha) {
  stats = {};
  SDL_Renderer* r = win_.renderer();
  SDL_SetRenderDrawColor(r, 8, 10, 14, 255);
  SDL_RenderClear(r);
  update_lighting();
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
  if (!mask_lighting_) fog_.draw(r, cam, world_);
}

}  // namespace aztec::engine
