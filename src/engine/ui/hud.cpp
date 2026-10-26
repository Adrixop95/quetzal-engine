#include "engine/ui/hud.hpp"

#include <cmath>
#include <cstdio>

#include "engine/world/type_info.hpp"

namespace aztec::engine {

const Object* pick_object(const World& w, float wx, float wy, float alpha, float radius) {
  const Object* best = nullptr;
  float bd = radius * radius;
  for (const Object& o : w.objects) {
    if (o.removed || !o.alive) continue;
    Vec2f p = o.interpolated(alpha);
    float d = (p.x - wx) * (p.x - wx) + (p.y - wy) * (p.y - wy);
    if (d <= bd) { bd = d; best = &o; }
  }
  return best;
}

void draw_hud(SDL_Renderer* r, const Font& font, const World& w, const Camera& cam, const HudInfo& in, int scale) {
  char buf[256];
  int y = 3 * scale, x = 3 * scale, lh = 7 * scale;
  SDL_Color white{255, 255, 255, 255}, dim{200, 210, 220, 255};
  std::snprintf(buf, sizeof buf, "%s  %dX%d  FPS %.0f  TICK %llu%s", w.mission.file_name.c_str(), w.terrain.w, w.terrain.h,
                in.fps, (unsigned long long)w.tick, in.paused ? "  PAUSED" : "");
  font.draw(r, x, y, buf, scale, white);
  y += lh;
  float wx = cam.to_world_x(float(in.mouse_x)), wy = cam.to_world_y(float(in.mouse_y));
  int tx = int(std::floor(wx / kTilePx)), ty = int(std::floor(wy / kTilePx));
  std::snprintf(buf, sizeof buf, "CAM %d,%d  ZOOM %.2f  TILE %d,%d  DRAW SPR %d MARK %d GEO %d", int(cam.x / kTilePx), int(cam.y / kTilePx),
                cam.zoom, tx, ty, in.sprites, in.placeholders, in.geometry_calls);
  font.draw(r, x, y, buf, scale, dim);
  y += lh;
  if (const Object* o = pick_object(w, wx, wy, in.alpha, 24.f / cam.zoom + 8)) {
    const TypeInfo* ti = type_info(o->type);
    std::snprintf(buf, sizeof buf, "OBJ %u  TYPE %d %s  SIDE %d  HP %d", o->id, o->type, ti ? ti->name : "-", o->side, o->hp);
    font.draw(r, x, y, buf, scale, {255, 230, 120, 255});
    y += lh;
  }
  if (in.show_help) {
    const char* help[] = {"KEYS.INI SCROLL   EDGE SCROLL   MIDDLE/RIGHT DRAG PAN   WHEEL +/- ZOOM",
                          "LMB MINIMAP JUMP   F11 BORDERLESS   ALT+ENTER FULLSCREEN",
                          "M MOVE  X ATTACK  H STOP  P PAUSE   KEYS.INI: PANEL / SCREENSHOT / DESTROY"};
    for (const char* h : help) { font.draw(r, x, y, h, scale, dim); y += lh; }
  }
}

}  // namespace aztec::engine
