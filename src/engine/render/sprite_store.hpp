// Decodes sprites from the game archives on demand and keeps them in an Atlas.
#pragma once
#include <unordered_map>
#include "core/palette.hpp"
#include "engine/assets/game_data.hpp"
#include "engine/assets/sprite_provider.hpp"
#include "engine/render/atlas.hpp"

namespace aztec::engine {

struct StoredSprite {
  AtlasRegion region;     // invalid if the sprite is absent or undecodable
  int ox = 0, oy = 0;     // hotspot offset stored in the sprite header
  bool ok() const { return region.valid(); }
};

class SpriteStore {
 public:
  SpriteStore(SDL_Renderer* r, const GameData& d) : data_(d), atlas_(r) {}
  const StoredSprite& get(const SpriteKey& k);
  const Atlas& atlas() const { return atlas_; }
  size_t cached() const { return cache_.size(); }

 private:
  const Palette* palette(uint8_t id, uint8_t shade);
  const GameData& data_;
  Atlas atlas_;
  std::unordered_map<SpriteKey, StoredSprite, SpriteKeyHash> cache_;
  std::unordered_map<uint32_t, Palette> pals_;
  std::unordered_map<uint32_t, bool> pal_ok_;
};

}  // namespace aztec::engine
