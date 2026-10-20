#include "engine/ui/cursor.hpp"

namespace aztec::engine {

static const char* kArrow[] = {
    "X..........", "XX.........", "XWX........", "XWWX.......", "XWWWX......", "XWWWWX.....", "XWWWWWX....",
    "XWWWWWWX...", "XWWWWWWWX..", "XWWWWWWWWX.", "XWWWWWXXXXX", "XWWXWWX....", "XWX.XWWX...", "XX..XWWX...",
    "X....XWWX..", ".....XWWX..", "......XX...",
};

Cursor::~Cursor() {
  for (auto& i : img_) if (i.tex) SDL_DestroyTexture(i.tex);
}

bool Cursor::set_image(SDL_Renderer* r, CursorKind k, int w, int h, const uint32_t* argb, int hx, int hy) {
  Image& im = img_[int(k)];
  if (im.tex) SDL_DestroyTexture(im.tex);
  im = {};
  SDL_Texture* t = SDL_CreateTexture(r, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STATIC, w, h);
  if (!t) return false;
  SDL_SetTextureBlendMode(t, SDL_BLENDMODE_BLEND);
  SDL_UpdateTexture(t, nullptr, argb, w * 4);
  im = {t, w, h, hx, hy};
  return true;
}

bool Cursor::init(SDL_Renderer* r) {
  const uint32_t kBlack = 0xFF000000u, kWhite = 0xFFFFFFFFu;
  {  // arrow
    int w = 11, h = 17;
    std::vector<uint32_t> px(size_t(w) * h, 0);
    for (int y = 0; y < h; y++)
      for (int x = 0; x < w; x++) {
        char c = kArrow[y][x];
        if (c == 'X') px[size_t(y) * w + x] = kBlack;
        else if (c == 'W') px[size_t(y) * w + x] = kWhite;
      }
    if (!set_image(r, CursorKind::Arrow, w, h, px.data(), 0, 0)) return false;
  }
  {  // pan: four arrows
    const int n = 15, c = 7;
    std::vector<uint32_t> px(n * n, 0);
    auto put = [&](int x, int y, uint32_t v) { if (x >= 0 && y >= 0 && x < n && y < n) px[y * n + x] = v; };
    for (int i = 1; i < n - 1; i++)
      for (int d = -1; d <= 1; d++) { put(i, c + d, kBlack); put(c + d, i, kBlack); }
    for (int i = 2; i < n - 2; i++) { put(i, c, kWhite); put(c, i, kWhite); }
    for (int k = 0; k < 3; k++)
      for (int d = -k; d <= k; d++) {
        put(c + d, 1 + k, kWhite); put(c + d, n - 2 - k, kWhite);
        put(1 + k, c + d, kWhite); put(n - 2 - k, c + d, kWhite);
      }
    if (!set_image(r, CursorKind::Pan, n, n, px.data(), c, c)) return false;
  }
  {  // crosshair
    const int n = 17, c = 8;
    std::vector<uint32_t> px(n * n, 0);
    for (int i = 0; i < n; i++) {
      if (i < c - 2 || i > c + 2) {
        px[c * n + i] = kWhite; px[i * n + c] = kWhite;
        px[(c - 1) * n + i] = kBlack; px[(c + 1) * n + i] = kBlack;
        px[i * n + c - 1] = kBlack; px[i * n + c + 1] = kBlack;
      }
    }
    for (int i = 0; i < n; i++) { if (i < c - 2 || i > c + 2) { px[c * n + i] = kWhite; px[i * n + c] = kWhite; } }
    if (!set_image(r, CursorKind::Crosshair, n, n, px.data(), c, c)) return false;
  }
  return true;
}

void Cursor::draw(SDL_Renderer* r, int x, int y, CursorKind k, int scale) const {
  const Image& im = img_[int(k)];
  if (!im.tex) return;
  SDL_Rect dst{x - im.hx * scale, y - im.hy * scale, im.w * scale, im.h * scale};
  SDL_RenderCopy(r, im.tex, nullptr, &dst);
}

}  // namespace aztec::engine
