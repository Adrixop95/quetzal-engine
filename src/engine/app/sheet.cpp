#include "engine/app/sheet.hpp"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <vector>

#include "core/palette.hpp"
#include "core/sprite.hpp"
#include "core/sprite_alpha.hpp"
#include "engine/assets/game_data.hpp"
#include "engine/render/png.hpp"

namespace aztec::engine {

bool write_sprite_sheet(const std::string& game_dir, const std::string& spec, const std::string& out, std::string& err) {
  GameData data;
  if (!data.open(game_dir, err)) return false;
  char name[32] = {};
  int a = 0, b = 0, group = 0x44c;
  if (std::sscanf(spec.c_str(), "%31[a-zA-Z0-9]:%d-%d:%i", name, &a, &b, &group) < 3) { err = "bad spec"; return false; }
  std::string n = name;
  for (auto& c : n) c = char(std::tolower(c));
  ArchiveId id = n == "units" ? ArchiveId::Units : n == "tiles0" ? ArchiveId::Tiles0 : n == "tiles1" ? ArchiveId::Tiles1 : ArchiveId::Common;
  struct Img { int w, h; std::vector<uint32_t> px; };
  std::vector<Img> imgs;
  for (int i = a; i <= b; i++) {
    Sprite s;
    Item item = data.archive(id).get(uint32_t(group), uint32_t(i));
    if (item && item.size > 8 && (item.data[8] & kRle16)) {  // indexed + alpha RLE
      AlphaSprite as;
      if (!decode_alpha_sprite(item, as)) continue;
      Palette pal{};
      if (!load_palette(data.archive(ArchiveId::Pal), uint32_t(as.palette), 14, pal) && !load_palette(data.archive(ArchiveId::Pal), uint32_t(as.palette), 0, pal))
        for (int k = 0; k < 256; k++) pal[k] = 0xFF000000u | k * 0x010101u;
      Img im{as.w, as.h, alpha_to_argb(as, pal)};
      int cw = item.data[0x10] | item.data[0x11] << 8, ch = item.data[0x12] | item.data[0x13] << 8;
      if (cw == as.w && ch > 0 && as.h > ch && as.h % ch == 0 && as.h / ch <= 16) {
        int cells = as.h / ch;
        Img w2{as.w * cells, ch, std::vector<uint32_t>(size_t(as.w) * cells * ch)};
        for (int c = 0; c < cells; c++)
          for (int y = 0; y < ch; y++)
            for (int x = 0; x < as.w; x++) w2.px[size_t(y) * w2.w + c * as.w + x] = im.px[size_t(c * ch + y) * as.w + x];
        im = std::move(w2);
      }
      std::printf("%d: %dx%d alpha-rle pal=%d hot=%d,%d\n", i, as.w, as.h, as.palette, as.ox, as.oy);
      imgs.push_back(std::move(im));
      continue;
    }
    if (!decode_sprite(item, s)) continue;
    int pid = s.palette < 4 && id == ArchiveId::Units ? s.palette + 0x10 : s.palette;
    Palette pal{};
    if (!s.is16() || s.palette != 14) {
      if (!load_palette(data.archive(ArchiveId::Pal), uint32_t(pid), 14, pal) && !load_palette(data.archive(ArchiveId::Pal), uint32_t(pid), 0, pal))
        for (int k = 0; k < 256; k++) pal[k] = 0xFF000000u | k * 0x010101u;
    }
    Img im{s.w, s.h, std::vector<uint32_t>(size_t(s.w) * s.h)};
    for (size_t k = 0; k < im.px.size(); k++) {
      if (!s.opaque[k]) { im.px[k] = 0; continue; }
      if (!s.is16()) im.px[k] = pal[s.index[k]];
      else if (s.palette != 14) im.px[k] = pal[s.color[k] & 255];
      else { uint32_t v = s.color[k]; im.px[k] = 0xFF000000u | (((v >> 11) & 31) * 255 / 31) << 16 | (((v >> 5) & 63) * 255 / 63) << 8 | (v & 31) * 255 / 31; }
    }
    // cut tall stacked sprites into cells placed side by side
    if (s.cell_h > 0 && s.cell_w == s.w && s.h > s.cell_h && s.h % s.cell_h == 0 && s.h / s.cell_h <= 16) {
      int cells = s.h / s.cell_h;
      Img w2{s.w * cells, s.cell_h, std::vector<uint32_t>(size_t(s.w) * cells * s.cell_h)};
      for (int c = 0; c < cells; c++)
        for (int y = 0; y < s.cell_h; y++)
          for (int x = 0; x < s.w; x++) w2.px[size_t(y) * w2.w + c * s.w + x] = im.px[size_t(c * s.cell_h + y) * s.w + x];
      im = std::move(w2);
    }
    std::printf("%d: %dx%d flags=%d pal=%d cell=%dx%d hot=%d,%d\n", i, s.w, s.h, s.flags, s.palette, s.cell_w, s.cell_h, s.ox, s.oy);
    imgs.push_back(std::move(im));
  }
  if (imgs.empty()) { err = "no sprites"; return false; }
  const int W = 1600;
  int x = 0, y = 0, rowh = 0;
  std::vector<std::pair<int, int>> pos;
  for (auto& im : imgs) {
    int w = std::min(im.w, W), h = im.h;
    if (x + w > W) { x = 0; y += rowh + 4; rowh = 0; }
    pos.push_back({x, y});
    x += w + 4;
    rowh = std::max(rowh, h);
  }
  int H = y + rowh + 4;
  std::vector<uint8_t> out_px(size_t(W) * H * 4);
  for (size_t p = 0; p < out_px.size(); p += 4) { out_px[p] = 70; out_px[p + 1] = 70; out_px[p + 2] = 100; out_px[p + 3] = 255; }
  for (size_t i = 0; i < imgs.size(); i++)
    for (int yy = 0; yy < imgs[i].h; yy++)
      for (int xx = 0; xx < std::min(imgs[i].w, W); xx++) {
        uint32_t v = imgs[i].px[size_t(yy) * imgs[i].w + xx];
        if (!(v >> 24)) continue;
        size_t o = (size_t(pos[i].second + yy) * W + pos[i].first + xx) * 4;
        out_px[o] = uint8_t(v >> 16); out_px[o + 1] = uint8_t(v >> 8); out_px[o + 2] = uint8_t(v);
      }
  if (!write_png(out, W, H, out_px)) { err = "cannot write png"; return false; }
  return true;
}

}  // namespace aztec::engine
