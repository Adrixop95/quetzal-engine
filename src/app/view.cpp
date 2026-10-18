// Map viewer: aztec_view GAME_DIR MAP_FILE [palette] [shade]
// Arrow keys / WASD scroll, mouse wheel zooms, window resizes freely (any aspect ratio).
#include <SDL2/SDL.h>

#include <cstdio>
#include <cstdlib>
#include <map>
#include <string>

#include "core/archive.hpp"
#include "core/map.hpp"
#include "core/palette.hpp"
#include "core/sprite.hpp"

using namespace aztec;

int main(int argc, char** argv) {
  if (argc < 3) { std::fprintf(stderr, "usage: %s GAME_DIR MAP_FILE [palette=4] [shade=14]\n", argv[0]); return 2; }
  std::string game = argv[1];
  unsigned pal_id = argc > 3 ? unsigned(std::atoi(argv[3])) : 4;
  unsigned shade = argc > 4 ? unsigned(std::atoi(argv[4])) : 14;

  Archive pal_xbe, tiles;
  TerrainMap map;
  Palette pal;
  if (!pal_xbe.open(game + "/Res/Pal.xbe") || !tiles.open(game + "/Res/Tiles0.xbe") ||
      !load_terrain_map(argv[2], map) || !load_palette(pal_xbe, pal_id, shade, pal)) {
    std::fprintf(stderr, "failed to load game data\n");
    return 1;
  }

  SDL_Init(SDL_INIT_VIDEO);
  SDL_Window* win = SDL_CreateWindow("Aztec Empire viewer", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                     1280, 720, SDL_WINDOW_RESIZABLE);
  SDL_Renderer* ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
  if (!ren) ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_SOFTWARE);

  std::map<uint32_t, SDL_Texture*> cache;
  auto tile_tex = [&](uint32_t idx) -> SDL_Texture* {
    auto it = cache.find(idx);
    if (it != cache.end()) return it->second;
    SDL_Texture* t = nullptr;
    Sprite s;
    if (decode_sprite(tiles.get(0x44c, idx), s) && s.w == 48 && s.h == 48) {
      std::vector<uint32_t> px = to_argb(s, pal);
      t = SDL_CreateTexture(ren, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STATIC, s.w, s.h);
      SDL_UpdateTexture(t, nullptr, px.data(), s.w * 4);
    }
    return cache[idx] = t;
  };

  float cx = 0, cy = 0, zoom = 1.0f;  // camera in map pixels
  bool run = true;
  while (run) {
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
      if (e.type == SDL_QUIT || (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_ESCAPE)) run = false;
      if (e.type == SDL_MOUSEWHEEL) zoom = std::max(0.25f, std::min(4.0f, zoom * (e.wheel.y > 0 ? 1.25f : 0.8f)));
    }
    const Uint8* k = SDL_GetKeyboardState(nullptr);
    float speed = 12 / zoom;
    if (k[SDL_SCANCODE_LEFT] || k[SDL_SCANCODE_A]) cx -= speed;
    if (k[SDL_SCANCODE_RIGHT] || k[SDL_SCANCODE_D]) cx += speed;
    if (k[SDL_SCANCODE_UP] || k[SDL_SCANCODE_W]) cy -= speed;
    if (k[SDL_SCANCODE_DOWN] || k[SDL_SCANCODE_S]) cy += speed;

    int ww, wh;
    SDL_GetRendererOutputSize(ren, &ww, &wh);
    SDL_SetRenderDrawColor(ren, 0, 0, 0, 255);
    SDL_RenderClear(ren);
    int x0 = std::max(0, int(cx / 48)), y0 = std::max(0, int(cy / 48));
    int x1 = std::min(map.w, int((cx + ww / zoom) / 48) + 1), y1 = std::min(map.h, int((cy + wh / zoom) / 48) + 1);
    for (int y = y0; y < y1; y++)
      for (int x = x0; x < x1; x++) {
        SDL_Texture* t = tile_tex(map.base(x, y));
        if (!t) continue;
        SDL_Rect dst{int((x * 48 - cx) * zoom), int((y * 48 - cy) * zoom), int(48 * zoom) + 1, int(48 * zoom) + 1};
        SDL_RenderCopy(ren, t, nullptr, &dst);
      }
    SDL_RenderPresent(ren);
  }
  for (auto& kv : cache) if (kv.second) SDL_DestroyTexture(kv.second);
  SDL_DestroyRenderer(ren);
  SDL_DestroyWindow(win);
  SDL_Quit();
  return 0;
}
