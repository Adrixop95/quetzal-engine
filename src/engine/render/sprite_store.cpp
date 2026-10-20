#include "engine/render/sprite_store.hpp"

#include "core/sprite.hpp"

namespace aztec::engine {

const Palette* SpriteStore::palette(uint8_t id, uint8_t shade) {
  uint32_t key = uint32_t(id) << 8 | shade;
  auto it = pals_.find(key);
  if (it != pals_.end()) return &it->second;
  Palette p{};
  if (!load_palette(data_.archive(ArchiveId::Pal), id, shade, p)) return nullptr;
  return &pals_.emplace(key, p).first->second;
}

const StoredSprite& SpriteStore::get(const SpriteKey& k) {
  auto it = cache_.find(k);
  if (it != cache_.end()) return it->second;
  StoredSprite out;
  Sprite s;
  if (decode_sprite(data_.archive(k.archive).get(k.group, k.index), s)) {
    const Palette* pal = s.is16() ? nullptr : palette(k.palette, k.shade);
    static const Palette kGrey = [] { Palette g{}; for (int i = 0; i < 256; i++) g[i] = 0xFF000000u | i * 0x010101u; return g; }();
    std::vector<uint32_t> px = to_argb(s, s.is16() ? kGrey : (pal ? *pal : kGrey));
    out.region = atlas_.add(s.w, s.h, px.data());
    out.ox = s.ox;
    out.oy = s.oy;
  }
  return cache_.emplace(k, out).first->second;
}

}  // namespace aztec::engine
