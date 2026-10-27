#include "engine/render/scene_renderer.hpp"

#include <algorithm>
#include <cmath>

#include "core/palette.hpp"
#include "engine/render/colors.hpp"
#include "engine/world/type_info.hpp"
#include "engine/data/light_masks.hpp"

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
  ambient_level_ = lv;
  daylight_tint_ = {Uint8(std::clamp(l.rgb[0], 0.f, 1.f) * 255), Uint8(std::clamp(l.rgb[1], 0.f, 1.f) * 255), Uint8(std::clamp(l.rgb[2], 0.f, 1.f) * 255), 255};
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
  uint8_t pal = uint8_t(default_palette());

  // Base tiles: only the exactly visible range.
  int bx0 = std::max(0, int(std::floor(cam.x / kTilePx))), by0 = std::max(0, int(std::floor(cam.y / kTilePx)));
  int bx1 = std::min(t.w, int(std::ceil((cam.x + cam.visible_w()) / kTilePx)));
  int by1 = std::min(t.h, int(std::ceil((cam.y + cam.visible_h()) / kTilePx)));
  for (int y = by0; y < by1; y++)
    for (int x = bx0; x < bx1; x++) {
      const StoredSprite& s = store_.get({arch, 0x44c, t.base(x, y), pal, glow_shade(float(x * 48 + 24), float(y * 48 + 24))});
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
  struct Item { float wx, wy; const Object* o; int tx, ty; uint16_t tree; const Effect* effect = nullptr; bool underlay = false; };  // tree != 0: terrain overlay sprite
  std::vector<Item> items;
  items.reserve(world_.objects.size() + 256);
  float pad = 96;
  for (const Object& o : world_.objects) {
    if (o.removed) continue;
    Vec2f p = o.interpolated(alpha);
    if (p.x < cam.x - pad || p.y < cam.y - pad || p.x > cam.x + cam.visible_w() + pad || p.y > cam.y + cam.visible_h() + pad) continue;
    if (!visible_in_fog(o, p)) continue;
    items.push_back({p.x, p.y, &o, 0, 0, 0});
  }
  for (const auto& e : world_.effects) {
    float x = (e.prev_x + (e.x - e.prev_x) * alpha) / 256.f;
    float y = (e.prev_y + (e.y - e.prev_y) * alpha) / 256.f;
    if (fog_.enabled() && e.side != world_.local_side && fog_.darkness(world_, int(x / 48), int(y / 48)) >= 120) continue;
    if (x < cam.x - pad || y < cam.y - pad || x > cam.x + cam.visible_w() + pad || y > cam.y + cam.visible_h() + pad) continue;
    if (effect_frame(e).valid()) items.push_back({x, y, nullptr, 0, 0, 0, &e, e.height == -1});
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
  std::stable_sort(items.begin(), items.end(), [](const Item& a, const Item& b) { if (a.underlay != b.underlay) return a.underlay; return a.wy != b.wy ? a.wy < b.wy : a.wx > b.wx; });

  int cur_page = -1;
  // Selection rings go on the ground, under bodies.
  mod_ = {255, 255, 255, 255};
  for (const Item& it : items) {
    if (it.effect || it.tree || !world_.selection.contains(it.o->id)) continue;
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
  mod_ = daylight_tint_;
  // Pass 2: bodies and placeholders, back to front.
  int scale = std::max(1, int(std::lround(cam.zoom)));
  for (const Item& it : items) {
    uint8_t local_shade = glow_shade(it.wx, it.wy);
    auto draw_frame = [&](data::SpriteRef frame, bool shadow, int height, int side) {
      if (!frame.valid()) return;
      SpriteKey key;
      key.archive = ArchiveId::Units; key.group = shadow ? 0x454 : 0x44c;
      key.index = uint32_t(frame.sprite); key.team = uint8_t(world_.side_color(side) & 7);
      key.shade = local_shade; key.shadow = shadow;
      const auto& sprite = store_.get(key);
      if (!sprite.ok()) return;
      blit(cam, sprite, 0, it.wx, it.wy - (shadow ? 0 : height), cur_page);
      stats.sprites++;
    };
    if (it.effect) {
      const auto& e = *it.effect;
      auto frame = effect_frame(e);
      int height = e.kind == data::EffectClass::Axe ? frame.yoff : e.height;
      draw_frame(frame, true, 0, e.side);
      draw_frame(frame, false, height, e.side);
      continue;
    }

    if (it.tree) {
      const auto& shadow = store_.get({arch, 0x454, it.tree, pal, local_shade, 0, 0, true});
      if (shadow.ok()) blit(cam, shadow, 0, it.wx, it.wy, cur_page);
      const StoredSprite& s = store_.get({arch, 0x44c, it.tree, pal, local_shade});
      if (s.ok()) { stats.overlays++; blit(cam, s, int(it.tx * 7 + it.ty * 13), it.wx, it.wy, cur_page); }
      continue;
    }
    const Object& o = *it.o;
    SpriteDraw sd;
    if (provider_ && provider_->pick_shadow(object_state(o), sd)) {
      const auto& shadow = store_.get(sd.key);
      if (shadow.ok()) blit(cam, shadow, sd.key.frame, it.wx, it.wy, cur_page);
    }
    if (provider_ && o.type >= 0 && provider_->pick(object_state(o), sd)) {
      sd.key.shade = local_shade;
      const StoredSprite& s = store_.get(sd.key);
      if (s.ok()) {
        stats.sprites++;
        blit(cam, s, sd.key.frame, it.wx + sd.dx, it.wy - sd.dy, cur_page);
        auto turret = turret_frame(o);
        draw_frame(turret, true, 0, o.side);
        draw_frame(turret, false, sd.dy, o.side);
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
  // Health bars of selected objects.
  for (const Item& it : items) {
    if (it.effect || it.tree || !world_.selection.contains(it.o->id) || it.o->type < 0) continue;
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
  return fog_.darkness(world_, int(p.x / kTilePx), int(p.y / kTilePx)) < 120;  // hide other sides' objects in the dark
}

data::SpriteRef SceneRenderer::effect_frame(const Effect& e) const {
  data::AnimSet a(data_.archive(ArchiveId::Units).get(0x515, uint32_t(e.anim)));
  return a.at_tick(e.slot, e.age, e.loop);
}

data::SpriteRef SceneRenderer::turret_frame(const Object& o) const {
  if (o.type < 0 || o.type >= 90 || !o.alive) return {};
  int anim = data::kTypeAnims[o.type].anim2;
  if (!anim) return {};
  auto sequence = data::turret_sequence(static_cast<data::AnimState>(std::min<int>(o.anim_state, 5)), o.facing * 32);
  data::AnimSet a(data_.archive(ArchiveId::Units).get(0x515, uint32_t(anim)));
  auto frame = data::SpriteDb::sprite_for_sequence(a, sequence, int(o.anim_tick));
  if (!frame.valid()) frame = data::SpriteDb::sprite_for_sequence(a, data::turret_sequence(data::AnimState::Idle, o.facing * 32), 0);
  return frame;
}

void SceneRenderer::build_glow() {
  int width = world_.terrain.w + 1, height = world_.terrain.h + 2;
  glow_.assign(size_t(width) * height, 0);
  auto add = [&](int light, int32_t fx, int32_t fy) {
    if (light < 1 || light > 16) return;
    data::LightMask mask(data_.archive(ArchiveId::Light).get(0x455, uint32_t(light)));
    if (!mask.ok()) return;
    // Bilinear translation of the mask onto the 48-pixel brightness grid.
    int x = fx / 256 - 9 * 48, y = fy / 256 - 9 * 48;
    for (int gy = std::max(0, y / 48); gy < height && gy * 48 <= y + 19 * 48; gy++)
      for (int gx = std::max(0, x / 48); gx < width && gx * 48 <= x + 19 * 48; gx++) {
        int value = mask.sample_pixels(gx * 48 - x, gy * 48 - y);
        auto& cell = glow_[size_t(gy) * width + gx];
        cell = std::min(152, cell + value);
      }
  };
  for (const auto& e : world_.effects) {
    // Light accessors use ground coordinates; altitude is not a position offset.
    auto frame = effect_frame(e);
    add(frame.light, e.x, e.y);
  }
  // A unit's light accessor uses its body frame, not its separate turret.
  for (const auto& o : world_.objects) {
    if (o.removed || !o.alive || o.type < 0 || o.type >= 90) continue;
    const auto& info = data::kTypeAnims[o.type];
    data::AnimSet a(data_.archive(ArchiveId::Units).get(0x515, info.anim));
    auto sequence = data::body_sequence(info.kind, static_cast<data::AnimState>(std::min<int>(o.anim_state, 5)), o.facing * 32, int(o.anim_variant), true, info.anim2 != 0);
    add(data::SpriteDb::sprite_for_sequence(a, sequence, int(o.anim_tick)).light, o.cur_x, o.cur_y);
  }
}

uint8_t SceneRenderer::glow_shade(float x, float y) const {
  int width = world_.terrain.w + 1;
  int gx = std::clamp(int(x / 48), 0, world_.terrain.w), gy = std::clamp(int(y / 48), 0, world_.terrain.h + 1);
  int brightness = gain_init_ ? int(ambient_level_ * 8) : std::clamp(opts.terrain_shade, 0, 19) * 8;
  if (!glow_.empty()) brightness += glow_[size_t(gy) * width + gx];
  return uint8_t(data::palette_shade(brightness));
}

void SceneRenderer::draw(const Camera& cam, float alpha) {
  stats = {};
  SDL_Renderer* r = win_.renderer();
  SDL_SetRenderDrawColor(r, 8, 10, 14, 255);
  SDL_RenderClear(r);
  build_glow();
  mod_ = daylight_tint_;
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
