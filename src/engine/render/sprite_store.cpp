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
  Item item = data_.archive(k.archive).get(k.group, k.index);
  if (decode_sprite(item, s)) {
    std::vector<uint32_t> px;
    if (k.shadow) {
      px.assign(size_t(s.w) * s.h, 0);
      for (size_t i = 0; i < px.size(); i++)
        if (s.opaque[i]) px[i] = 0x70000000u;
    } else {
      int pid = k.palette;
      if (!pid) pid = s.palette < 4 ? s.palette + 0x10 + k.team * 4 : s.palette;
      const Palette* pal = s.is16() ? nullptr : palette(uint8_t(pid), k.shade);
      static const Palette kGrey = [] { Palette g{}; for (int i = 0; i < 256; i++) g[i] = 0xFF000000u | i * 0x010101u; return g; }();
      px = to_argb(s, pal ? *pal : kGrey);
    }
    out.region = atlas_.add(s.w, s.h, px.data());
    out.ox = s.ox;
    out.oy = s.oy;
    // Sprites that stack several images (terrain animation, trees) have cell_w x cell_h in the header.
    out.cw = s.w;
    out.ch = s.h;
    if (s.cell_w > 0 && s.cell_h > 0 && s.w == s.cell_w && s.h > s.cell_h && s.h % s.cell_h == 0) {
      out.frames = s.h / s.cell_h;
      out.ch = s.cell_h;
    }
  }
  return cache_.emplace(k, out).first->second;
}

}  // namespace aztec::engine
