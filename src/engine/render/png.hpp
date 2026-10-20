// Minimal PNG writer (stored deflate blocks, no external library) for --screenshot.
#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace aztec::engine {

// `rgba` is w*h*4 bytes (R,G,B,A); alpha is written as-is.
bool write_png(const std::string& path, int w, int h, const std::vector<uint8_t>& rgba);

}  // namespace aztec::engine
