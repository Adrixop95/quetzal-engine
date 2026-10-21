#include "engine/ui/commands_ui.hpp"

#include <algorithm>
#include <cmath>

#include "engine/ui/hud.hpp"
#include "engine/ui/names.hpp"
#include "engine/world/type_info.hpp"

namespace aztec::engine {

void CommandsUi::menu_layout(const SDL_Rect& panel, int sc, int& x, int& y0, int& row_h) {
  x = panel.x + 22 * sc;
  row_h = 14 * sc;
  y0 = (214 + 8) * sc + 2 * row_h + 6 * sc;
}

void CommandsUi::issue_to_selection(const World& w, Command c) const {
  c.side = uint8_t(w.local_side);
  c.ids = w.selection.ids;
  if (!c.ids.empty()) sim_->submit(std::move(c));
}

void CommandsUi::update(const Input& in, World& w, const Camera& cam, float alpha, const Minimap& mm, bool in_world, const SDL_Rect& panel) {
  panel_ = panel;
  if (!sim_) return;
  // Drop dead / removed objects from the selection.
  w.selection.ids.erase(std::remove_if(w.selection.ids.begin(), w.selection.ids.end(), [&](uint32_t id) {
    const Object* o = w.find(id);
    return !o || o->removed || !o->alive;
  }), w.selection.ids.end());

  // ---- keyboard
  if (in.hit(SDL_SCANCODE_ESCAPE)) { attack_move_ = false; placing_ = -1; }
  if (in.hit(SDL_SCANCODE_H)) { Command c; c.type = Pct::Stop; issue_to_selection(w, c); }
  if (in.hit(SDL_SCANCODE_A) && !w.selection.ids.empty()) attack_move_ = true;
  for (int k = 0; k < 10; k++) {
    SDL_Scancode sc = SDL_Scancode(k == 0 ? SDL_SCANCODE_0 : SDL_SCANCODE_1 + (k - 1));
    if (!in.hit(sc)) continue;
    if (in.ctrl) {
      Command c; c.type = Pct::CreateGroup; c.a = k; issue_to_selection(w, c);
    } else if (w.local_side >= 0 && w.local_side < int(w.side_state.size())) {
      w.selection.ids = w.side_state[w.local_side].groups[k];   // groups only contain own objects
    }
  }
  if (in.hit(SDL_SCANCODE_P)) { Command c; c.type = sim_->paused() ? Pct::PauseOff : Pct::PauseOn; c.side = uint8_t(w.local_side); sim_->submit(c); }

  // ---- right button: click = order, drag = pan (handled by the engine)
  if (in.pressed[SDL_BUTTON_RIGHT]) { right_down_ = true; right_moved_ = false; rx_ = in.mx; ry_ = in.my; }
  if (right_down_ && in.buttons[SDL_BUTTON_RIGHT] && (std::abs(in.mx - rx_) > 5 || std::abs(in.my - ry_) > 5)) right_moved_ = true;
  if (right_down_ && in.released[SDL_BUTTON_RIGHT]) {
    right_down_ = false;
    if (!right_moved_) {
      if (placing_ >= 0 || attack_move_) { placing_ = -1; attack_move_ = false; }
      else if (mm.contains(in.mx, in.my)) {
        float wx, wy;
        mm.to_world(in.mx, in.my, wx, wy);
        Command c; c.type = Pct::Move; c.a = int(wx); c.b = int(wy);
        issue_to_selection(w, c);
      } else if (in_world) {
        float wx = cam.to_world_x(float(in.mx)), wy = cam.to_world_y(float(in.my));
        const Object* t = pick_object(w, wx, wy, alpha, 24.f / cam.zoom + 8);
        Command c;
        if (t && t->side != w.local_side && t->alive && !t->removed && t->type >= 0) { c.type = Pct::Attack; c.a = int(t->id); }
        else { c.type = Pct::Move; c.a = int(wx); c.b = int(wy); }
        issue_to_selection(w, c);
      }
    }
    right_moved_ = false;
  }

  // ---- left button: attack-move target, building placement, menu rows
  if (in.pressed[SDL_BUTTON_LEFT]) {
    if (in_world && attack_move_) {
      Command c; c.type = Pct::AttackMove;
      c.a = int(cam.to_world_x(float(in.mx))); c.b = int(cam.to_world_y(float(in.my)));
      issue_to_selection(w, c);
      attack_move_ = false;
    } else if (in_world && placing_ >= 0) {
      int tx = int(std::floor(cam.to_world_x(float(in.mx)) / kTilePx)), ty = int(std::floor(cam.to_world_y(float(in.my)) / kTilePx));
      if (sim_->can_place(w, w.local_side, placing_, tx, ty)) {
        Command c; c.type = Pct::Build; c.side = uint8_t(w.local_side); c.a = placing_; c.b = tx; c.c = ty;
        sim_->submit(c);
        if (!in.shift) placing_ = -1;
      }
    } else if (in.mx >= panel.x && panel.w > 0) {
      int x, y0, rh;
      menu_layout(panel, std::max(1, panel.w / 204), x, y0, rh);
      (void)x;
      std::vector<MenuItem> items = sim_->menu(w, w.local_side);
      int row = (in.my - y0) / std::max(1, rh);
      if (in.my >= y0 && row >= 0 && row < int(items.size()) && items[size_t(row)].enabled) {
        const MenuItem& m = items[size_t(row)];
        if (m.construct) placing_ = m.type;
        else { Command c; c.type = Pct::Build; c.side = uint8_t(w.local_side); c.a = m.type; sim_->submit(c); }
      }
    }
  }
}

std::string CommandsUi::draw_menu(SDL_Renderer* r, const Font& font, const StrBase& strings, const World& w, const SDL_Rect& panel,
                                  int sc, int mx, int my) const {
  if (!sim_) return {};
  int x, y0, rh;
  menu_layout(panel, sc, x, y0, rh);
  std::vector<MenuItem> items = sim_->menu(w, w.local_side);
  std::string tip;
  if (!items.empty()) {
    SDL_Rect bg{x - 6 * sc, y0 - 2 * sc, panel.w - 32 * sc, int(items.size()) * rh + 4 * sc};
    SDL_SetRenderDrawColor(r, 18, 14, 10, 175);
    SDL_RenderFillRect(r, &bg);
  }
  for (size_t i = 0; i < items.size(); i++) {
    const MenuItem& m = items[i];
    int y = y0 + int(i) * rh;
    bool hover = mx >= panel.x && mx < panel.x + panel.w && my >= y && my < y + rh;
    SDL_Color col = m.enabled ? SDL_Color{240, 235, 215, 255} : SDL_Color{140, 130, 120, 255};
    if (hover && m.enabled) col = {255, 230, 120, 255};
    std::string name = type_display_name(strings, m.type) + (m.construct ? " [B]" : "");
    font.draw(r, x, y, name, 2 * sc, col);
    std::string price = std::to_string(m.price);
    font.draw(r, panel.x + panel.w - 28 * sc - font.width(price, 2 * sc), y, price, 2 * sc, col);
    if (hover) {
      tip = name + "  Price: " + price + " Time: " + std::to_string(m.time);
      if (!m.enabled) tip += std::string("  (") + m.why_not + ")";
    }
  }
  // production queue
  if (w.local_side >= 0 && w.local_side < int(w.side_state.size())) {
    const SideState& ss = w.side_state[w.local_side];
    int y = panel.h - 26 * sc;
    for (size_t i = 0; i < ss.queue.size() && i < 3; i++) {
      const QueueItem& q = ss.queue[i];
      int pct = 100 - q.remaining * 100 / std::max(1, q.total);
      font.draw(r, x, y + int(i) * rh, type_display_name(strings, q.type) + " " + std::to_string(pct) + "%", 2 * sc, {180, 230, 180, 255});
    }
  }
  if (attack_move_) font.draw(r, x, y0 - rh, "ATTACK-MOVE: CLICK TARGET", 2 * sc, {255, 120, 120, 255});
  else if (placing_ >= 0) font.draw(r, x, y0 - rh, "PLACE BUILDING: CLICK (ESC)", 2 * sc, {120, 255, 120, 255});
  return tip;
}

void CommandsUi::draw_ghost(SDL_Renderer* r, const World& w, const Camera& cam, int mx, int my) const {
  if (!sim_ || placing_ < 0) return;
  int tx = int(std::floor(cam.to_world_x(float(mx)) / kTilePx)), ty = int(std::floor(cam.to_world_y(float(my)) / kTilePx));
  int x0, y0, x1, y1;
  GameSim::footprint(sim_->rules().type(placing_), tx, ty, x0, y0, x1, y1);
  bool ok = sim_->can_place(w, w.local_side, placing_, tx, ty);
  for (int y = y0; y <= y1; y++)
    for (int x = x0; x <= x1; x++) {
      SDL_FRect f{cam.to_screen_x(float(x * kTilePx)), cam.to_screen_y(float(y * kTilePx)), kTilePx * cam.zoom, kTilePx * cam.zoom};
      SDL_SetRenderDrawColor(r, ok ? 60 : 230, ok ? 220 : 50, 60, 110);
      SDL_RenderFillRectF(r, &f);
      SDL_SetRenderDrawColor(r, 255, 255, 255, 160);
      SDL_RenderDrawRectF(r, &f);
    }
}

}  // namespace aztec::engine
