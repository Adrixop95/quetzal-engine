// Plug-in point for "which sprite does object X show right now". The type -> sprite/animation mapping
// is research in progress (docs/sprites_mapping.md, src/engine/data/); once known, implement
// SpriteProvider (or fill a TableSpriteProvider) and pass it to Engine::set_sprite_provider().
// Objects for which no provider answers are drawn as coloured placeholder markers.
#pragma once
#include <cstdint>
#include <functional>
#include <memory>
#include <unordered_map>
#include <vector>
#include "engine/assets/game_data.hpp"

namespace aztec::engine {

// One image in a resource archive (type 0x44c/0x454 item), with the palette used to colour 8-bit data.
struct SpriteKey {
  ArchiveId archive = ArchiveId::Units;
  uint16_t group = 0x44c;
  uint32_t index = 0;
  uint8_t palette = 0;   // Pal.xbe item for 8-bit sprites; 0 = take it from the sprite header (+0xC)
  uint8_t shade = 14;    // palette shade level 0..19
  uint8_t team = 0;      // team colour 0..7: header palette ids < 4 map to Pal item id + 0x10 + team*4
  uint8_t frame = 0;     // image index for sprites that stack several images (terrain variants)
  bool shadow = false;   // render as translucent black silhouette (group 0x454 shadow sprites)
  bool operator==(const SpriteKey&) const = default;
};
struct SpriteKeyHash {
  size_t operator()(const SpriteKey& k) const {
    uint64_t v = uint64_t(k.index) << 32 | uint64_t(k.group) << 16 | uint64_t(k.palette) << 8 | k.shade;
    uint64_t w = uint64_t(k.archive) << 24 | uint64_t(k.team) << 16 | uint64_t(k.frame) << 8 | (k.shadow ? 1 : 0);
    return std::hash<uint64_t>()(v ^ (w * 0x9E3779B97F4A7C15ull));
  }
};

// What the provider is asked about. All fields come from the simulation, never from the renderer.
struct ObjectState {
  int type_id = -1;       // UGI id 0..89; -2 Boat, -3 Johnka (render-only transport ids)
  int side = 0;
  int side_color = 0;     // [SideN] Color, for team-colour palettes
  int facing = 0;         // 0..7: east=0, counter-clockwise
  int anim_state = 0;     // data::AnimState (0 idle, 1 move, 2 attack, 3 die, 4 fidget, 5 icon, 6 construct)
  uint32_t anim_tick = 0; // simulation ticks since the animation started
  int64_t body_tick = -1, attack_tick = -1;  // optional independent simulation clocks; -1 uses anim_tick
  uint32_t anim_variant = 0;  // stable per-object random value (death/fidget sequence choice)
  int angle = -1, secondary_angle = -1;  // full 0..255 angles; -1 uses facing * 32
  bool cycle_ready = true, initial_turret = true, moving = false;
  int height = 0;             // object altitude in pixels for Bird/Zeppelin
  int hp = 0, max_hp = 0;
  int destruction_radius = -1;  // unknown until supplied by the object producer
  bool construction_overlay = false;
  int transport_phase = 0, orientation = 0;
  int progress = 1000;        // construction 0..1000 (anim_state 6 = under construction)
};

struct SpriteDraw {
  SpriteKey key;
  int dx = 0, dy = 0;     // extra pixel offset on top of the sprite's own hotspot (ox, oy)
  bool shadow = false;    // optional semantic tag; SpriteKey::shadow controls the stored image
  int layer = 0;          // reserved for custom providers; common ordering uses SpriteItem height
};

struct SpriteItem {
  int dx = 0, dy = 0, height = 0;
  std::vector<SpriteDraw> parts;
  int light = 0;
};

class SpriteProvider {
 public:
  virtual ~SpriteProvider() = default;
  // Returns true and fills `out` if a real sprite exists for this state.
  virtual bool pick(const ObjectState& st, SpriteDraw& out) const = 0;
  // Optional shadow, drawn before the body (translucent). Default: none.
  virtual bool pick_shadow(const ObjectState& /*st*/, SpriteDraw& /*out*/) const { return false; }
  // Ordered per-item layers. An empty result may intentionally hide an object.
  virtual std::vector<SpriteDraw> parts(const ObjectState& st) const {
    std::vector<SpriteDraw> result;
    SpriteDraw d;
    if (pick_shadow(st, d)) result.push_back(d);
    if (pick(st, d)) result.push_back(d);
    return result;
  }
  virtual std::vector<SpriteItem> items(const ObjectState& st) const {
    return {{0, 0, height(st), parts(st), light_mask(st)}};
  }
  virtual int light_mask(const ObjectState&) const { return 0; }
  virtual int vision_mask(const ObjectState&) const { return 0; }
  virtual int height(const ObjectState&) const { return 0; }
  virtual bool hides_missing(const ObjectState&) const { return false; }
};

// Simplest provider: fixed sprite per type id (ignores facing/animation).
class TableSpriteProvider : public SpriteProvider {
 public:
  void set(int type_id, const SpriteDraw& d) { table_[type_id] = d; }
  size_t size() const { return table_.size(); }
  bool pick(const ObjectState& st, SpriteDraw& out) const override {
    auto it = table_.find(st.type_id);
    if (it == table_.end()) return false;
    out = it->second;
    return true;
  }

 private:
  std::unordered_map<int, SpriteDraw> table_;
};

// Asks several providers in order; first hit wins.
class ChainSpriteProvider : public SpriteProvider {
 public:
  void add(std::shared_ptr<const SpriteProvider> p) { chain_.push_back(std::move(p)); }
  bool pick(const ObjectState& st, SpriteDraw& out) const override {
    for (const auto& p : chain_)
      if (p->pick(st, out)) return true;
    return false;
  }
  bool pick_shadow(const ObjectState& st, SpriteDraw& out) const override {
    for (const auto& p : chain_)
      if (p->pick_shadow(st, out)) return true;
    return false;
  }

  std::vector<SpriteDraw> parts(const ObjectState& st) const override {
    for (const auto& p : chain_) {
      auto result = p->parts(st);
      if (!result.empty() || p->hides_missing(st)) return result;
    }
    return {};
  }
  std::vector<SpriteItem> items(const ObjectState& st) const override {
    for (const auto& p : chain_) {
      if (!p->parts(st).empty() || p->hides_missing(st)) return p->items(st);
    }
    return {};
  }
  int light_mask(const ObjectState& st) const override {
    for (const auto& p : chain_) if (int id = p->light_mask(st)) return id;
    return 0;
  }
  int vision_mask(const ObjectState& st) const override {
    for (const auto& p : chain_) if (int id = p->vision_mask(st)) return id;
    return 0;
  }
  int height(const ObjectState& st) const override {
    for (const auto& p : chain_) if (p->hides_missing(st)) return p->height(st);
    return 0;
  }
  bool hides_missing(const ObjectState& st) const override {
    for (const auto& p : chain_) if (p->hides_missing(st)) return true;
    return false;
  }
 private:
  std::vector<std::shared_ptr<const SpriteProvider>> chain_;
};

}  // namespace aztec::engine
