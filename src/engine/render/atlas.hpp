// Texture atlas: shelf-packed pages of ARGB8888, filled lazily. Lets the renderer draw thousands of
// tiles/sprites with a handful of SDL_RenderGeometry calls (one per page).
#pragma once
#include <SDL2/SDL.h>
#include <cstdint>
#include <vector>

namespace aztec::engine {

struct AtlasRegion {
  int page = -1;
  int x = 0, y = 0, w = 0, h = 0;
  bool valid() const { return page >= 0; }
};

class Atlas {
 public:
  Atlas(SDL_Renderer* r, int page_size = 2048) : ren_(r), size_(page_size) {}
  ~Atlas();
  Atlas(const Atlas&) = delete;
  Atlas& operator=(const Atlas&) = delete;

  // Uploads w*h ARGB pixels; returns an invalid region if the image does not fit a page.
  AtlasRegion add(int w, int h, const uint32_t* argb);
  SDL_Texture* page(int i) const { return pages_[i].tex; }
  int num_pages() const { return int(pages_.size()); }
  int page_size() const { return size_; }

 private:
  struct Page { SDL_Texture* tex = nullptr; int cx = 0, cy = 0, row_h = 0; };
  bool new_page();
  SDL_Renderer* ren_;
  int size_;
  std::vector<Page> pages_;
};

}  // namespace aztec::engine
