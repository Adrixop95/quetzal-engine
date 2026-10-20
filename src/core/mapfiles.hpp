// Loaders for the per-mission files next to Maps/<name>.map (formats: docs/formats.md).
#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include "core/sprite.hpp"

namespace aztec {

bool read_file(const std::string& path, std::vector<uint8_t>& out);

template <class T>
struct Grid {
  int w = 0, h = 0;
  std::vector<T> v;
  bool in(int x, int y) const { return x >= 0 && y >= 0 && x < w && y < h; }
  T at(int x, int y) const { return v[size_t(y) * w + x]; }
};

// .dat: w*h bytes, passability (0 = water/edge, 1 = obstacle, 2 = open; meaning still being confirmed).
bool load_passability(const std::string& path, int w, int h, Grid<uint8_t>& out);
// .rm: u16 w, u16 h, w*h u16 ("resource map").
bool load_resource_map(const std::string& path, Grid<uint16_t>& out);
// .spr: single sprite = map preview (2 px per tile, palette indices).
bool load_preview(const std::string& path, Sprite& out);

// (.vis lives in core/visfile.hpp)
// Parsing helpers (pure, unit-testable).
bool parse_passability(const std::vector<uint8_t>& data, int w, int h, Grid<uint8_t>& out);
bool parse_resource_map(const std::vector<uint8_t>& data, Grid<uint16_t>& out);

}  // namespace aztec
