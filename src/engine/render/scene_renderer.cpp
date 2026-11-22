#include "engine/render/scene_renderer.hpp"

#include <algorithm>
#include <cmath>

#include "engine/data/animation_slots.hpp"
#include "engine/render/world_rules.hpp"
#include "engine/render/colors.hpp"
#include "engine/world/type_info.hpp"
#include "engine/world/visibility.hpp"
#include "engine/world/object_state.hpp"
#include "engine/data/light_masks.hpp"

namespace aztec::engine {

SceneRenderer::SceneRenderer(Window& win, const GameData& data, const World& world)
    : win_(win), data_(data), world_(world), store_(win.renderer(), data) {
  font_ok_ = font_.init(win.renderer(), &store_);
}

// 0 = per-sprite palette from the tile header (Tiles0 uses Pal 4/5/6, Tiles1 uses 8/9/10).
int SceneRenderer::default_palette() const { return opts.terrain_palette > 0 ? opts.terrain_palette : 0; }

void SceneRenderer::queue(int page, float x0, float y0, float x1, float y1, const SDL_Rect& src, const SpriteKey& key, int frame) {
  if (page >= int(batches_.size())) batches_.resize(page + 1);
  if (observe_sprite) observe_sprite(key, frame, {x0, y0, x1 - x0, y1 - y0});
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
  if (world_.visibility_brightness && fog_.requested()) { mask_lighting_ = true; return; }
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
    bool primary = true;
    const bool simulated_fire = std::any_of(world_.effects.begin(), world_.effects.end(),
        [&](const Effect& e) { return e.loop && e.source == o.id; });
    for (const auto& item : provider_->items(st)) {
      if (!primary && simulated_fire && o.alive) continue;
      primary = false;
      if (item.light >= data::kFirstGlowMask && item.light <= data::kLastGlowMask)
        lighting_.stamp(data::LightMask(archive.get(data::kLightGroup, item.light)), x + item.dx, y + item.dy, false);
    }
    const int vision = provider_->vision_mask(st);
    if (o.alive && o.side == world_.local_side && vision >= data::kFirstVisionMask && vision <= data::kLastVisionMask)
      lighting_.stamp(data::LightMask(archive.get(data::kLightGroup, vision)), x, y, true);
  }
  for (const auto& e : world_.effects) {
    const auto frame = effect_frame(e);
    if (frame.light >= data::kFirstGlowMask && frame.light <= data::kLastGlowMask)
      lighting_.stamp(data::LightMask(data_.archive(ArchiveId::Light).get(data::kLightGroup, frame.light)),
                      e.x / 256.f, e.y / 256.f, false);
  }
  lighting_.combine(fog_.enabled());
}
uint8_t SceneRenderer::shade_at(float x, float y) const {
  if (world_.visibility_brightness && fog_.requested()) {
    const auto* layer = visibility_layer(world_, world_.local_side);
    return uint8_t(data::palette_shade(layer ? sight_brightness(*layer, int(x), int(y)) / 256 : 0));
  }
  return uint8_t(mask_lighting_ ? data::palette_shade(int(lighting_.sample(x, y))) : opts.terrain_shade);
}

static float snap(float v) { return std::floor(v + 0.5f); }

int SceneRenderer::light_vertex(int x, int y) const {
  if (world_.visibility_brightness && fog_.requested()) {
    const auto* layer = visibility_layer(world_, world_.local_side);
    if (!layer || layer->w <= 0 || layer->h <= 0 || layer->v.size() != size_t(layer->w) * layer->h) return 0;
    return layer->v[size_t(std::clamp(y, 0, int(layer->h) - 1)) * layer->w + std::clamp(x, 0, int(layer->w) - 1)];
  }
  return mask_lighting_ ? int(lighting_.sample(float(x * 48), float(y * 48))) : opts.terrain_shade * 8;
}

// Select palette rows at source-pixel resolution, coalescing equal horizontal
// runs. Object bodies reuse the ground row even when their artwork is elevated.
void SceneRenderer::blit_lit(const Camera& cam, SpriteKey key, int frame, float wx, float wy,
                             float ground_y, bool row, int& page) {
  const auto base = store_.get(key);
  if (!base.ok()) return;
  float left = wx - base.ox, top = wy - base.oy;
  auto shade = [&](int x, int y) {
    int px = int(std::floor(left + x)), py = int(std::floor(row ? ground_y : top + y));
    int tx = int(std::floor(px / 48.0)), ty = int(std::floor(py / 48.0));
    return light_surface_shade(light_vertex(tx, ty), light_vertex(tx + 1, ty),
        light_vertex(tx, ty + 1), light_vertex(tx + 1, ty + 1), px, py);
  };
  for (int y = 0; y < base.ch; y += row ? base.ch : 1) {
    for (int x = 0; x < base.cw;) {
      int level = shade(x, y), end = x + 1;
      while (end < base.cw && shade(end, y) == level) ++end;
      key.shade = uint8_t(level);
      const auto& image = store_.get(key);
      if (image.ok()) {
        if (image.region.page != page) { flush(page); page = image.region.page; }
        auto src = image.frame_rect(frame); src.x += x; src.y += y;
        src.w = end - x; src.h = row ? base.ch : 1;
        queue(page, snap(cam.to_screen_x(left + x)), snap(cam.to_screen_y(top + y)),
            snap(cam.to_screen_x(left + end)), snap(cam.to_screen_y(top + y + src.h)), src, key, frame);
      }
      x = end;
    }
  }
}

void SceneRenderer::flush(int page) {
  if (page < 0 || page >= int(batches_.size()) || batches_[page].empty()) return;
  stats.geometry_calls++;
  batches_[page].flush(win_.renderer(), store_.atlas().page(page));
}

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
      SpriteKey key{arch, 0x44c, t.base(x, y), pal, shade_at(float(x * kTilePx + 24), float(y * kTilePx + 24))};
      const int a = light_vertex(x, y), b = light_vertex(x + 1, y);
      const int c = light_vertex(x, y + 1), d = light_vertex(x + 1, y + 1);
      if (opts.original_look) key.shade = uint8_t(terrain_flat_shade(a));
      const StoredSprite& s = store_.get(key);
      if (!s.ok()) continue;
      int frame = s.frames > 1 ? int(opts.view_counter % unsigned(s.frames)) : 0;  // water/shore animation
      stats.tiles++;
      if (opts.original_look && !(a == b && a == c && a == d)) {
        for (int p = 0; p < int(batches_.size()); ++p) flush(p);
        int page = -1;
        blit_lit(cam, key, frame, float(x * 48 + s.ox), float(y * 48 + s.oy), 0, false, page);
        flush(page);
        continue;
      }
      float sx0 = snap(cam.to_screen_x(float(x * kTilePx))), sy0 = snap(cam.to_screen_y(float(y * kTilePx)));
      float sx1 = snap(cam.to_screen_x(float((x + 1) * kTilePx))), sy1 = snap(cam.to_screen_y(float((y + 1) * kTilePx)));
      queue(s.region.page, sx0, sy0, sx1, sy1, {s.region.x, s.region.y + frame * kTilePx, kTilePx, kTilePx}, key, frame);
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
void SceneRenderer::blit(const Camera& cam, const StoredSprite& s, const SpriteKey& key, int frame, float wx, float wy, int& cur_page) {
  if (s.region.page != cur_page) { flush(cur_page); cur_page = s.region.page; }
  const int cells = std::max(1, s.frames);
  frame = ((frame % cells) + cells) % cells;
  float x0 = wx - s.ox, y0 = wy - s.oy;
  queue(s.region.page, snap(cam.to_screen_x(x0)), snap(cam.to_screen_y(y0)), snap(cam.to_screen_x(x0 + s.cw)),
        snap(cam.to_screen_y(y0 + s.ch)), s.frame_rect(frame), key, frame);
}

void SceneRenderer::draw_objects(const Camera& cam, float alpha) {
  SDL_Renderer* r = win_.renderer();
  ArchiveId arch = world_.mission.tileset == 1 ? ArchiveId::Tiles1 : ArchiveId::Tiles0;
  uint8_t pal = uint8_t(default_palette());
  struct Item { float wx, wy; const Object* o; int height; uint16_t tree; std::vector<SpriteDraw> parts; bool primary = true; const Effect* effect = nullptr; int display_size = -1; bool flash = false; int marker = 0; };  // tree != 0: terrain overlay sprite
  std::vector<Item> items;
  items.reserve(world_.objects.size() + 256);
  float pad = 96;
  std::vector<const Object*> displayed;
  const bool proxies = fog_.requested() && world_.visibility_brightness &&
      world_.local_side >= 0 && world_.local_side < int(world_.building_memory.size());
  for (const auto& o : world_.objects)
    if (!proxies || !visibility_structure(world_, o)) displayed.push_back(&o);
  if (proxies) for (const auto& [id, picture] : world_.building_memory[world_.local_side]) {
    (void)id; displayed.push_back(&picture);
  }
  int collection_page = -1;
  for (const Object* picture : displayed) {
    const Object& o = *picture;
    if (o.removed || o.source == ObjectSource::Submarine) continue;
    Vec2f p = o.interpolated(alpha);
    if (p.x < cam.x - pad || p.y < cam.y - pad || p.x > cam.x + cam.visible_w() + pad || p.y > cam.y + cam.visible_h() + pad) continue;
    if (!(proxies && visibility_structure(world_, o)) &&
        !visible_in_fog(o, {o.cur_x / 256.f, o.cur_y / 256.f})) continue;
    if (provider_) {
      const bool frozen = proxies && visibility_structure(world_, o) &&
          !object_visible(world_, world_.local_side, o);
      const bool simulated_fire = !frozen && std::any_of(world_.effects.begin(), world_.effects.end(),
          [&](const Effect& e) { return e.loop && e.source == o.id; });
      bool primary = true;
      for (auto item : provider_->items(object_state(o))) {
        if (!primary && simulated_fire && o.alive) continue;
        for (auto& part : item.parts) if (part.underlay) {
          part.key.shade = shade_at(p.x, p.y);
          const auto image = store_.get(part.key);
          if (!image.ok()) continue;
          if (opts.original_look) blit_lit(cam, part.key, part.key.frame, p.x + item.dx + part.dx, p.y + item.dy + part.dy, p.y + item.dy, false, collection_page);
          else blit(cam, image, part.key, part.key.frame, p.x + item.dx + part.dx, p.y + item.dy + part.dy, collection_page);
        }
        std::erase_if(item.parts, [](const auto& part) { return part.underlay; });
        items.push_back({p.x + item.dx, p.y + item.dy, &o, item.height, 0, std::move(item.parts), primary,
            nullptr, item.display_size, item.flash, item.marker});
        primary = false;
      }
    } else items.push_back({p.x, p.y, &o, 0, 0, {}});
  }
  flush(collection_page);
  for (const auto& e : world_.effects) {
    float x = (e.prev_x + (e.x - e.prev_x) * alpha) / 256.f;
    float y = (e.prev_y + (e.y - e.prev_y) * alpha) / 256.f;
    if (fog_.requested() &&
        !position_visible(world_, world_.local_side, e.x >> 8, e.y >> 8)) continue;
    if (x < cam.x - pad || y < cam.y - pad || x > cam.x + cam.visible_w() + pad || y > cam.y + cam.visible_h() + pad) continue;
    if (effect_frame(e).valid()) items.push_back({x, y, nullptr, e.height, 0, {}, false, &e});
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
  // V: each sorted item draws its own ground selection before its body.
  auto draw_selection = [&](const Item& it) {
    if (it.effect || it.tree || !it.primary || is_ferry(*it.o) || !world_.selection.contains(it.o->id)) return;
    const TypeInfo* ti = type_info(it.o->type);
    SpriteKey k;
    k.archive = ArchiveId::Common; k.group = 0x44c; k.shade = 0;
    const int size = it.display_size >= 0 ? it.display_size : ti && ti->is_structure() ? 77 : 24;
    k.index = status_art[status_art_index(size)].ellipse;
    k.palette = 3;
    const StoredSprite& s = store_.get(k);
    if (!s.ok()) return;
    mod_ = {255, 255, 255, 255};
    int f = int(opts.view_counter % unsigned(std::max(1, s.frames)));
    if (s.region.page != cur_page) { flush(cur_page); cur_page = s.region.page; }
    float x0 = it.wx - s.cw / 2.f, y0 = it.wy - it.height - s.ch / 2.f;
    queue(s.region.page, snap(cam.to_screen_x(x0)), snap(cam.to_screen_y(y0)), snap(cam.to_screen_x(x0 + s.cw)),
          snap(cam.to_screen_y(y0 + s.ch)), s.frame_rect(f), k, f);
    mod_ = body_mod;
  };
  auto draw_health = [&](const Item& it) {
    if (it.effect || it.tree || !it.primary || is_ferry(*it.o) || (!opts.all_status && !world_.selection.contains(it.o->id)) || it.o->type < 0) return;
    const TypeInfo* ti = type_info(it.o->type);
    bool structure = ti && ti->is_structure();
    int hpmax = it.o->hp_max;
    if (hpmax <= 0 || it.o->hp <= 0) return;
    int ratio = int(std::clamp(int64_t(it.o->hp) * 256 / hpmax, int64_t(0), int64_t(256)));
    if (!ratio) return;
    int size = it.display_size >= 0 ? it.display_size : structure ? 77 : 24;
    int art = status_art_index(size);
    SpriteKey key{ArchiveId::Common, 0x44c, uint32_t(status_art[std::max(0, art - 1)].health), 3, 0};
    const auto image = store_.get(key);
    if (image.ok()) {
      flush(cur_page); cur_page = -1;
      auto src = image.frame_rect(health_frame(ratio));
      src.w = health_width(src.w, ratio);
      float x = cam.to_screen_x(it.wx - image.cw / 2.f);
      // G: the producer does not yet supply the original bar rectangle top.
      float y = cam.to_screen_y(it.wy - std::max(0, it.height) - (structure ? 110.f : 46.f));
      mod_ = {255, 255, 255, 255};
      if (src.w) queue(image.region.page, x, y, x + src.w * cam.zoom, y + src.h * cam.zoom, src, key, health_frame(ratio));
      flush(image.region.page); mod_ = body_mod;
      return;
    }
    // G: diagnostic fallback when the user's health artwork is unavailable.
    flush(cur_page); cur_page = -1;
    float frac = hpmax > 0 ? std::clamp(float(it.o->hp) / hpmax, 0.f, 1.f) : 1.f;
    float w = (structure ? 70.f : 26.f) * cam.zoom, h = std::max(3.f, 4.f * cam.zoom);
    float x = cam.to_screen_x(it.wx) - w / 2, y = cam.to_screen_y(it.wy - (structure ? 110.f : 46.f));
    SDL_FRect bg{x - 1, y - 1, w + 2, h + 2}, fg{x, y, w * frac, h};
    SDL_SetRenderDrawColor(r, 0, 0, 0, 200);
    SDL_RenderFillRectF(r, &bg);
    SDL_SetRenderDrawColor(r, frac > 0.6f ? 60 : 230, frac > 0.3f ? 210 : 50, frac > 0.6f ? 60 : 40, 255);
    SDL_RenderFillRectF(r, &fg);
  };
  // Shared ordered list: finish every item before starting the next one.
  int scale = std::max(1, int(std::lround(cam.zoom)));
  for (const Item& it : items) {
    draw_selection(it);
    uint8_t local_shade = shade_at(it.wx, it.wy);
    auto draw_frame = [&](data::SpriteRef frame, bool shadow, int height, int side) {
      if (!frame.valid()) return;
      SpriteKey key;
      key.archive = ArchiveId::Units; key.group = shadow ? 0x454 : 0x44c;
      key.index = uint32_t(frame.sprite); key.team = uint8_t(world_.side_color(side) & 7);
      key.shade = shadow ? uint8_t(data::kShadowShade) : local_shade; key.shadow = shadow;
      mod_ = shadow ? SDL_Color{255, 255, 255, 255} : body_mod;
      const auto& sprite = store_.get(key);
      if (!sprite.ok()) return;
      if (opts.original_look && !shadow) blit_lit(cam, key, 0, it.wx, it.wy - height, it.wy, true, cur_page);
      else blit(cam, sprite, key, 0, it.wx, it.wy - (shadow ? 0 : height), cur_page);
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
      for (bool shadow : {true, false}) {
        const SpriteKey key{arch, uint16_t(shadow ? data::kShadowGroup : data::kBodyGroup),
            it.tree, pal, shadow ? uint8_t(data::kShadowShade) : shade_at(it.wx, it.wy), 0, 0, shadow};
        const StoredSprite& s = store_.get(key);
        mod_ = shadow ? SDL_Color{255, 255, 255, 255} : body_mod;
        if (s.ok()) {
          stats.overlays++;
          int frame = data::overlay_frame(opts.wind, opts.wind_counter);
          if (opts.original_look && !shadow) blit_lit(cam, key, frame, it.wx, it.wy, it.wy, true, cur_page);
          else blit(cam, s, key, frame, it.wx, it.wy, cur_page);
        }
      }
      continue;
    }
    const Object& o = *it.o;
    bool drawn = false;
    const auto st = object_state(o);
    for (auto sd : it.parts) {
      if (!sd.key.shadow) sd.key.shade = flash_tick(it.flash, opts.view_counter) ? 19 : shade_at(it.wx, it.wy);
      else sd.key.shade = data::kShadowShade;
      mod_ = sd.key.shadow ? SDL_Color{255, 255, 255, 255} : body_mod;
      const StoredSprite& s = store_.get(sd.key);
      if (s.ok()) {
        drawn = true; stats.sprites++;
        if (opts.original_look && !sd.key.shadow && !flash_tick(it.flash, opts.view_counter))
          blit_lit(cam, sd.key, sd.key.frame, it.wx + sd.dx, it.wy + sd.dy, it.wy, it.height != -1, cur_page);
        else blit(cam, s, sd.key, sd.key.frame, it.wx + sd.dx, it.wy + sd.dy, cur_page);
      }
    }
    draw_health(it);
    if (it.marker) {
      SpriteKey key{ArchiveId::Common, 0x44c, uint32_t(it.marker == 1 ? 5 : 2), 0, 0};
      const auto image = store_.get(key);
      mod_ = {255, 255, 255, 255};
      if (image.ok()) blit(cam, image, key, int(opts.view_counter), it.wx, it.wy, cur_page);
      mod_ = body_mod;
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
}

ObjectState SceneRenderer::object_state(const Object& o) const {
  return object_sprite_state(o, world_.side_color(o.side));
}

bool SceneRenderer::visible_in_fog(const Object& o, Vec2f p) const {
  if (!fog_.requested()) return true;
  if (visibility_structure(world_, o)) return object_visible(world_, world_.local_side, o);
  if (!world_.visibility_brightness && o.side == world_.local_side) return true;
  return position_visible(world_, world_.local_side, int(std::floor(p.x)), int(std::floor(p.y)));
}

data::SpriteRef SceneRenderer::effect_frame(const Effect& e) const {
  data::AnimSet a(data_.archive(ArchiveId::Units).get(0x515, uint32_t(e.anim)));
  return a.at_tick(e.slot, e.age, e.loop);
}

void SceneRenderer::draw(const Camera& cam, float alpha, const std::function<void()>& ground_overlay) {
  stats = {};
  if (opts.original_look) alpha = 1;
  SDL_Renderer* r = win_.renderer();
  SDL_SetRenderDrawColor(r, 8, 10, 14, 255);
  SDL_RenderClear(r);
  fog_.update(r, world_);
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
  if (!mask_lighting_) fog_.draw(r, cam, world_);
  if (ground_overlay) ground_overlay();
}

}  // namespace aztec::engine
