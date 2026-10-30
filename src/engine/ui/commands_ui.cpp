#include "engine/ui/commands_ui.hpp"

#include <algorithm>
#include <cmath>

#include "engine/ui/hud.hpp"
#include "engine/ui/names.hpp"
#include "engine/ui/panel_layout.hpp"
#include "engine/world/type_info.hpp"

namespace aztec::engine {
namespace {
bool contains(const SDL_Rect& r, int x, int y) {
  return r.w > 0 && r.h > 0 && x >= r.x && y >= r.y && x < r.x + r.w && y < r.y + r.h;
}
int production_town(const World& w) {
  for (uint32_t id : w.selection.ids) {
    const Object* o = w.find(id);
    if (!o || !o->alive || o->removed || o->side != w.local_side) continue;
    for (size_t i = 0; i < w.mission.towns.size(); ++i)
      if (w.mission.towns[i].side == w.local_side && w.mission.towns[i].index == o->town) return int(i);
  }
  for (size_t i = 0; i < w.mission.towns.size(); ++i)
    if (w.mission.towns[i].side == w.local_side) return int(i);
  return -1;
}
uint32_t production_member(const World& w, int town) {
  if (town < 0) return 0;
  for (const Object& o : w.objects)
    if (o.alive && !o.removed && o.side == w.local_side && o.town == w.mission.towns[size_t(town)].index)
      return o.id;
  return 0;
}
std::vector<size_t> production_queue(const World& w, int town) {
  std::vector<size_t> indices;
  if (w.local_side < 0 || w.local_side >= int(w.side_state.size())) return indices;
  const auto& queue = w.side_state[w.local_side].queue;
  for (size_t i = 0; i < queue.size(); ++i) if (queue[i].town == town) indices.push_back(i);
  return indices;
}
const char* const kLabels[] = {"Move", "Attack", "Stop", "Repair", "Garrison", "Destroy"};
// Common command resources contain twelve rotation frames. Idle uses frame zero.
constexpr int kButtons[] = {275, 257, 268, 301, 288, 269};

bool blit(SDL_Renderer* r, SpriteStore& store, SpriteKey key, const SDL_Rect& box, int frame = 0, bool enabled = true) {
  const StoredSprite& s = store.get(key);
  if (!s.ok() || box.w <= 0 || box.h <= 0) return false;
  SDL_Rect src = s.frame_rect(frame);
  float scale = std::min(float(box.w) / src.w, float(box.h) / src.h);
  SDL_Rect dst{box.x + (box.w - int(src.w * scale)) / 2, box.y + (box.h - int(src.h * scale)) / 2,
               std::max(1, int(src.w * scale)), std::max(1, int(src.h * scale))};
  SDL_Texture* tex = store.atlas().page(s.region.page);
  SDL_SetTextureColorMod(tex, enabled ? 255 : 85, enabled ? 255 : 85, enabled ? 255 : 85);
  SDL_RenderCopy(r, tex, &src, &dst);
  SDL_SetTextureColorMod(tex, 255, 255, 255);
  return true;
}
void border(SDL_Renderer* r, const SDL_Rect& box, bool highlight) {
  SDL_SetRenderDrawColor(r, highlight ? 250 : 100, highlight ? 215 : 88, highlight ? 110 : 65, 255);
  SDL_RenderDrawRect(r, &box);
}
}

CommandsUi::CommandsUi(GameSim* sim) : sim_(sim) {
  // Fallback for installations without Keys.ini. Engine extensions use M/X/H.
  parse_keys_ini("Add lshift\nAdd rshift\nGarrison `\nAutoRepair q\nRepair w\nDestroy e\n"
                 "Aggressive a\nDefencive s\nNormal d\nDirectControl f\n"
                 "ToggleInterface f12\nScrollLeft left\nScrollRight right\nScrollUp up\nScrollDown down\n"
                 "ScrollLeft pad4\nScrollRight pad6\nScrollUp pad8\nScrollDown pad2\nPrintScreen printscreen\n", bindings_);
}

SDL_Scancode CommandsUi::sdl_scancode(int dik) {
  // DirectInput uses PC set-1 codes, SDL uses USB codes. Translate explicitly.
  static constexpr SDL_Scancode letters[] = {
    SDL_SCANCODE_Q, SDL_SCANCODE_W, SDL_SCANCODE_E, SDL_SCANCODE_R, SDL_SCANCODE_T, SDL_SCANCODE_Y,
    SDL_SCANCODE_U, SDL_SCANCODE_I, SDL_SCANCODE_O, SDL_SCANCODE_P};
  if (dik >= 2 && dik <= 10) return SDL_Scancode(SDL_SCANCODE_1 + dik - 2);
  if (dik == 11) return SDL_SCANCODE_0;
  if (dik >= 16 && dik <= 25) return letters[dik - 16];
  static constexpr SDL_Scancode home[] = {SDL_SCANCODE_A, SDL_SCANCODE_S, SDL_SCANCODE_D, SDL_SCANCODE_F,
    SDL_SCANCODE_G, SDL_SCANCODE_H, SDL_SCANCODE_J, SDL_SCANCODE_K, SDL_SCANCODE_L};
  if (dik >= 30 && dik <= 38) return home[dik - 30];
  static constexpr SDL_Scancode bottom[] = {SDL_SCANCODE_Z, SDL_SCANCODE_X, SDL_SCANCODE_C, SDL_SCANCODE_V,
    SDL_SCANCODE_B, SDL_SCANCODE_N, SDL_SCANCODE_M};
  if (dik >= 44 && dik <= 50) return bottom[dik - 44];
  if (dik >= 59 && dik <= 68) return SDL_Scancode(SDL_SCANCODE_F1 + dik - 59);
  switch (dik) {
    case 1: return SDL_SCANCODE_ESCAPE;
    case 12: return SDL_SCANCODE_MINUS; case 13: return SDL_SCANCODE_EQUALS;
    case 14: return SDL_SCANCODE_BACKSPACE; case 15: return SDL_SCANCODE_TAB;
    case 26: return SDL_SCANCODE_LEFTBRACKET; case 27: return SDL_SCANCODE_RIGHTBRACKET;
    case 28: return SDL_SCANCODE_RETURN; case 29: return SDL_SCANCODE_LCTRL;
    case 39: return SDL_SCANCODE_SEMICOLON; case 40: return SDL_SCANCODE_APOSTROPHE;
    case 41: return SDL_SCANCODE_GRAVE; case 42: return SDL_SCANCODE_LSHIFT;
    case 43: return SDL_SCANCODE_BACKSLASH; case 51: return SDL_SCANCODE_COMMA;
    case 52: return SDL_SCANCODE_PERIOD; case 53: return SDL_SCANCODE_SLASH;
    case 54: return SDL_SCANCODE_RSHIFT; case 55: return SDL_SCANCODE_KP_MULTIPLY;
    case 56: return SDL_SCANCODE_LALT; case 57: return SDL_SCANCODE_SPACE; case 58: return SDL_SCANCODE_CAPSLOCK;
    case 69: return SDL_SCANCODE_NUMLOCKCLEAR; case 70: return SDL_SCANCODE_SCROLLLOCK;
    case 71: return SDL_SCANCODE_KP_7; case 72: return SDL_SCANCODE_KP_8; case 73: return SDL_SCANCODE_KP_9;
    case 74: return SDL_SCANCODE_KP_MINUS; case 75: return SDL_SCANCODE_KP_4; case 76: return SDL_SCANCODE_KP_5;
    case 77: return SDL_SCANCODE_KP_6; case 78: return SDL_SCANCODE_KP_PLUS;
    case 79: return SDL_SCANCODE_KP_1; case 80: return SDL_SCANCODE_KP_2; case 81: return SDL_SCANCODE_KP_3;
    case 82: return SDL_SCANCODE_KP_0; case 83: return SDL_SCANCODE_KP_PERIOD;
    case 87: return SDL_SCANCODE_F11; case 88: return SDL_SCANCODE_F12;
    case 156: return SDL_SCANCODE_KP_ENTER; case 157: return SDL_SCANCODE_RCTRL;
    case 181: return SDL_SCANCODE_KP_DIVIDE; case 183: return SDL_SCANCODE_PRINTSCREEN;
    case 184: return SDL_SCANCODE_RALT; case 199: return SDL_SCANCODE_HOME;
    case 200: return SDL_SCANCODE_UP; case 201: return SDL_SCANCODE_PAGEUP;
    case 203: return SDL_SCANCODE_LEFT; case 205: return SDL_SCANCODE_RIGHT;
    case 207: return SDL_SCANCODE_END; case 208: return SDL_SCANCODE_DOWN;
    case 209: return SDL_SCANCODE_PAGEDOWN; case 210: return SDL_SCANCODE_INSERT; case 211: return SDL_SCANCODE_DELETE;
    default: return SDL_SCANCODE_UNKNOWN;
  }
}

bool CommandsUi::action_hit(const Input& in, const char* action) const {
  int id = key_action_id(action);
  for (const KeyBinding& b : bindings_) if (b.action == id && sdl_scancode(b.dik) != SDL_SCANCODE_UNKNOWN && in.hit(sdl_scancode(b.dik))) return true;
  return false;
}
bool CommandsUi::action_down(const Input& in, const char* action) const {
  int id = key_action_id(action);
  for (const KeyBinding& b : bindings_) if (b.action == id && sdl_scancode(b.dik) != SDL_SCANCODE_UNKNOWN && in.down(sdl_scancode(b.dik))) return true;
  return false;
}
bool CommandsUi::key_bound(SDL_Scancode key) const {
  for (const KeyBinding& b : bindings_) if (sdl_scancode(b.dik) == key) return true;
  return false;
}
bool CommandsUi::bound_hit(const Input& in, SDL_Scancode key) const { return !in.ctrl && !in.alt && !key_bound(key) && in.hit(key); }

CommandPanelLayout CommandsUi::layout(const SDL_Rect& p, int sc) {
  sc = std::max(1, sc);
  CommandPanelLayout l;
  const auto geometry = in_game_panel_layout(p, sc);
  const auto card = geometry.card;
  auto box = [&](int x, int y, int w, int h) { return SDL_Rect{card.x + x * sc, card.y + y * sc, w * sc, h * sc}; };
  l.portrait = geometry.portrait;
  l.summary = geometry.summary;
  // Five columns follow the cells in common:297. The command/production split
  // and queue controls are engine presentation pending a live reference capture.
  auto cell = [&](int i) { return box(16 + i % 5 * 44, 70 + i / 5 * 44, 40, 40); };
  for (int i = 0; i < 6; ++i) l.commands[i] = cell(i);
  for (int i = 6; i < 15; ++i) l.production.push_back(cell(i));
  l.previous = box(14, 20, 20, 18);
  l.next = box(48, 20, 20, 18);
  l.pause = box(90, 12, 150, 18);
  for (int i = 0; i < 8; ++i) l.queue[i] = box(90 + i * 18, 34, 16, 20);
  return l;
}

bool CommandsUi::action_enabled(PanelAction action, const World& w, const RulesData* rules) {
  for (uint32_t id : w.selection.ids) {
    const Object* o = w.find(id);
    if (!o || !o->alive || o->removed || o->side != w.local_side || o->type < 0) continue;
    const TypeInfo* ti = type_info(o->type);
    if (action == PanelAction::Garrison || action == PanelAction::Repair) {
      if (o->town_hall && o->town >= 0) return true;
      continue;
    }
    if (action == PanelAction::Destroy) {
      for (const TownState& town : w.town_state)
        if (town.index == o->town && town.side == o->side && town.repair_mode != RepairMode::Off &&
            !o->town_hall && o->progress >= 1000 && ti && ti->is_structure() &&
            (town.repair_mode == RepairMode::Destroy || o->hp < o->hp_max)) return true;
      continue;
    }
    if (rules) {
      const TypeRules& tr = rules->type(o->type);
      if ((tr.mobile || tr.flier) && (action != PanelAction::Attack || tr.armed)) return true;
    } else if (ti && !ti->is_structure()) return true;
  }
  return false;
}

int CommandsUi::icon_slot(int kind) {
  if (kind == 2 || kind == 7) return 7;
  if (kind == 6) return 39;
  if (kind == 1) return 9;
  if (kind == 9) return 17;
  return 23;
}
uint32_t CommandsUi::icon_sprite(int type, bool portrait) const {
  if (type < 0 || type >= kNumTypes) return 0;
  const auto& info = data::kTypeAnims[type];
  data::AnimSet a = icons_.anim(info.anim);
  if (!a.ok()) return 0;
  int variant = 0;
  // Vehicle icon sequences contain one four-frame set per weapon variant.
  if (info.kind == 6 || info.kind == 1) {
    switch (info.anim2) {
      case 51: variant = 0; break;
      case 52: variant = 1; break;
      case 50: variant = 2; break;
      case 53: variant = 3; break;
      case 54: variant = 4; break;
    }
  }
  auto f = a.frame(icon_slot(info.kind), variant * 4 + (portrait ? 1 : 0));
  if (!f.valid()) f = a.frame(0, 0);  // types without a dedicated icon retain their original idle sprite
  if (!f.valid()) f = a.frame(1, 0);
  return uint32_t(f.sprite);
}

void CommandsUi::issue_to_selection(const World& w, Command c) const {
  c.side = uint8_t(w.local_side);
  for (uint32_t id : w.selection.ids) {
    const Object* o = w.find(id);
    if (o && o->alive && !o->removed && o->side == w.local_side) c.ids.push_back(id);
  }
  if (c.type == Pct::DestroyHere) {
    for (const TownState& town : w.town_state) {
      if (town.side != w.local_side) continue;
      Command toggle = c; toggle.b = town.index; toggle.ids.clear();
      for (uint32_t id : c.ids) {
        const Object* o = w.find(id);
        if (o && o->town == town.index) toggle.ids.push_back(id);
      }
      if (!toggle.ids.empty()) sim_->submit(std::move(toggle));
    }
  } else if (!c.ids.empty()) sim_->submit(std::move(c));
}
void CommandsUi::activate(PanelAction action, const World& w) {
  if (!action_enabled(action, w, &sim_->rules())) return;
  cancel_mode();
  if (action == PanelAction::Move) mode_ = TargetMode::Move;
  else if (action == PanelAction::Attack) mode_ = TargetMode::Attack;
  else if (action == PanelAction::Garrison || action == PanelAction::Repair) {
    for (uint32_t id : w.selection.ids) {
      const Object* o = w.find(id);
      if (!o || !o->town_hall || !o->alive || o->removed || o->side != w.local_side) continue;
      Command c; c.type = action == PanelAction::Garrison ? Pct::Garrison : Pct::RepairMode;
      c.side = uint8_t(w.local_side); c.b = o->town;
      if (action == PanelAction::Repair)
        for (const TownState& town : w.town_state)
          if (town.index == o->town && town.side == o->side) c.a = (int(town.repair_mode) + 1) % 3;
      sim_->submit(c); break;
    }
  } else { Command c; c.type = action == PanelAction::Stop ? Pct::Stop : Pct::DestroyHere; issue_to_selection(w, c); }
}

bool CommandsUi::update(const Input& in, World& w, const Camera& cam, float alpha, const Minimap& mm, bool in_world, const SDL_Rect& panel) {
  if (!sim_) return false;
  if (!in.focused) {
    pressed_control_ = 0;
    right_down_ = right_moved_ = false;
    return false;
  }
  ++ui_frame_;
  w.selection.ids.erase(std::remove_if(w.selection.ids.begin(), w.selection.ids.end(), [&](uint32_t id) {
    const Object* o = w.find(id); return !o || o->removed || !o->alive;
  }), w.selection.ids.end());
  if (selection_ != w.selection.ids) {
    cancel_mode();
    selection_ = w.selection.ids;
    page_ = 0;
  }
  if (w.selection.ids.empty() && placing_ < 0) mode_ = TargetMode::None;
  if (placing_ >= 0 && placing_member_) {
    const auto* producer = w.find(placing_member_);
    if (!producer || !producer->alive || producer->removed || producer->side != w.local_side) cancel_mode();
  }
  if (in.hit(SDL_SCANCODE_ESCAPE)) { const bool consumed = wants_left_click(); cancel_mode(); if (consumed) return true; }
  if (bound_hit(in, SDL_SCANCODE_H)) activate(PanelAction::Stop, w);
  if (bound_hit(in, SDL_SCANCODE_M)) activate(PanelAction::Move, w);
  if (bound_hit(in, SDL_SCANCODE_X)) activate(PanelAction::Attack, w);
  if (!in.ctrl && !in.alt && action_hit(in, "Repair")) activate(PanelAction::Repair, w);
  if (!in.ctrl && !in.alt && action_hit(in, "Garrison")) activate(PanelAction::Garrison, w);
  if (!in.ctrl && !in.alt && action_hit(in, "Destroy")) activate(PanelAction::Destroy, w);
  for (int k = 0; k < 10; k++) {
    SDL_Scancode sc = SDL_Scancode(k == 0 ? SDL_SCANCODE_0 : SDL_SCANCODE_1 + k - 1);
    if (!in.hit(sc)) continue;
    cancel_mode();
    if (in.ctrl) { Command c; c.type = in.shift ? Pct::AddToGroup : Pct::CreateGroup; c.a = k; issue_to_selection(w, c); }
    else if (w.local_side >= 0 && w.local_side < int(w.side_state.size())) {
      Command c; c.type = Pct::SelectGroup; c.side = uint8_t(w.local_side); c.a = k;
      sim_->submit(std::move(c));
    }
  }
  if (bound_hit(in, SDL_SCANCODE_P)) { Command c; c.type = sim_->paused() ? Pct::PauseOff : Pct::PauseOn; c.side = uint8_t(w.local_side); sim_->submit(c); }

  if (in.pressed[SDL_BUTTON_RIGHT]) { right_down_ = true; right_moved_ = false; rx_ = in.mx; ry_ = in.my; }
  if (right_down_ && in.buttons[SDL_BUTTON_RIGHT] && (std::abs(in.mx - rx_) > 5 || std::abs(in.my - ry_) > 5)) right_moved_ = true;
  if (right_down_ && in.released[SDL_BUTTON_RIGHT]) {
    right_down_ = false;
    if (!right_moved_) {
      if (wants_left_click()) cancel_mode();
      else if (in_world || mm.contains(in.mx, in.my)) {
        float wx, wy;
        if (mm.contains(in.mx, in.my)) mm.to_world(in.mx, in.my, wx, wy);
        else { wx = cam.to_world_x(float(in.mx)); wy = cam.to_world_y(float(in.my)); }
        const Object* t = in_world ? pick_object(w, wx, wy, alpha, 24.f / cam.zoom + 8) : nullptr;
        Command c;
        if (t && t->side != w.local_side && t->alive && !t->removed && t->type >= 0) { c.type = Pct::Attack; c.a = int(t->id); }
        else { c.type = Pct::Move; c.a = int(wx); c.b = int(wy); }
        issue_to_selection(w, c);
      }
    }
    right_moved_ = false;
  }
  const int selected_town = production_town(w);
  if (queue_town_ != selected_town) { queue_town_ = selected_town; queue_offset_ = 0; pressed_control_ = 0; }
  auto visible_queue = production_queue(w, selected_town);
  queue_offset_ = std::clamp(queue_offset_, 0, std::max(0, int(visible_queue.size()) - 8));
  if (in.wheel != 0 && panel.w > 0) {
    auto boxes = layout(panel, std::max(1, panel.w / 204));
    bool over_queue = contains(boxes.pause, in.mx, in.my);
    for (const auto& box : boxes.queue) over_queue |= contains(box, in.mx, in.my);
    if (over_queue) {
      pressed_control_ = 0;
      queue_offset_ = std::clamp(queue_offset_ + (in.wheel < 0 ? 8 : -8), 0,
                               std::max(0, int(visible_queue.size()) - 8));
      return true;
    }
  }
  if (in.hit(SDL_SCANCODE_ESCAPE)) pressed_control_ = 0;
  if (!in.pressed[SDL_BUTTON_LEFT] && !in.released[SDL_BUTTON_LEFT]) {
    if (!in.buttons[SDL_BUTTON_LEFT]) pressed_control_ = 0;
    return false;
  }
  if (in.pressed[SDL_BUTTON_LEFT] && (in_world || mm.contains(in.mx, in.my)) && mode_ != TargetMode::None) {
    float wx, wy;
    if (mm.contains(in.mx, in.my)) mm.to_world(in.mx, in.my, wx, wy);
    else { wx = cam.to_world_x(float(in.mx)); wy = cam.to_world_y(float(in.my)); }
    const Object* t = in_world ? pick_object(w, wx, wy, alpha, 24.f / cam.zoom + 8) : nullptr;
    Command c;
    if (mode_ == TargetMode::Attack && t && t->side != w.local_side && t->alive && !t->removed) { c.type = Pct::Attack; c.a = int(t->id); }
    else { c.type = mode_ == TargetMode::Attack ? Pct::AttackMove : Pct::Move; c.a = int(wx); c.b = int(wy); }
    issue_to_selection(w, c);
    cancel_mode();
    return true;
  }
  if (in.pressed[SDL_BUTTON_LEFT] && in_world && placing_ >= 0) {
    int tx = int(std::floor(cam.to_world_x(float(in.mx)) / kTilePx)), ty = int(std::floor(cam.to_world_y(float(in.my)) / kTilePx));
    if (sim_->can_place(w, w.local_side, placing_, tx, ty, nullptr, nullptr, placing_town_)) {
      Command c; c.type = placing_command_; c.side = uint8_t(w.local_side); c.a = placing_; c.b = tx; c.c = ty;
      if (placing_member_) c.ids = {placing_member_};
      sim_->submit(c);
      if (!in.shift && !action_down(in, "Add")) cancel_mode();
    }
    return true;
  }
  const int scale = std::max(1, panel.w / 204);
  if (panel.w <= 0 || (!contains(panel, in.mx, in.my) &&
      !contains(in_game_panel_layout(panel, scale).card, in.mx, in.my))) {
    pressed_control_ = 0;
    return false;
  }
  auto l = layout(panel, scale);
  int town = production_town(w);
  auto queue = production_queue(w, town);
  const int waiting = int(std::count_if(queue.begin(), queue.end(), [&](size_t i) {
    return !w.side_state[w.local_side].queue[i].started;
  }));
  auto items = sim_->menu(w, w.local_side, town);
  int count = int(l.production.size()), pages = std::max(1, (int(items.size()) + count - 1) / count);
  page_ = std::clamp(page_, 0, pages - 1);
  // Capture a control on press, then activate only when released over that
  // same enabled control. Recompute availability before accepting the release.
  int control = 0;
  for (size_t i = 0; i < l.commands.size(); ++i)
    if (contains(l.commands[i], in.mx, in.my) && action_enabled(PanelAction(i), w, &sim_->rules())) control = int(i) + 1;
  if (pages > 1 && contains(l.previous, in.mx, in.my)) control = 20;
  if (pages > 1 && contains(l.next, in.mx, in.my)) control = 21;
  for (int i = 0; i < count && page_ * count + i < int(items.size()); ++i) {
    const auto& item = items[size_t(page_ * count + i)];
    if (item.enabled && waiting < kProductionQueueCapacity && contains(l.production[size_t(i)], in.mx, in.my))
      control = 100 + item.type;
  }
  if (w.local_side >= 0 && w.local_side < int(w.side_state.size())) {
    if (!queue.empty() && w.side_state[w.local_side].queue[queue.front()].started && contains(l.pause, in.mx, in.my)) control = 22;
    for (size_t i = 0; i + queue_offset_ < queue.size() && i < l.queue.size(); ++i)
      if (contains(l.queue[i], in.mx, in.my)) control = 200 + int(queue[i + queue_offset_]);
  }
  if (in.pressed[SDL_BUTTON_LEFT]) {
    pressed_control_ = control;
    pressed_queue_size_ = queue.size();
  }
  if (!in.released[SDL_BUTTON_LEFT]) return true;
  const bool clicked = in.focused && control && control == pressed_control_ &&
      (control < 200 || pressed_queue_size_ == queue.size());
  pressed_control_ = 0;
  if (!clicked) return true;
  for (size_t i = 0; i < l.commands.size(); i++) if (control == int(i) + 1) activate(PanelAction(i), w);
  if (control == 20) page_ = (page_ + pages - 1) % pages;
  if (control == 21) page_ = (page_ + 1) % pages;
  for (int i = 0; i < count && page_ * count + i < int(items.size()); i++) {
    const auto& m = items[size_t(page_ * count + i)];
    if (control == 100 + m.type && m.enabled &&
        waiting < kProductionQueueCapacity) {
      cancel_mode();
      if (m.place) {
        placing_ = m.type; placing_member_ = production_member(w, town); placing_town_ = town;
        placing_command_ = m.construct ? Pct::Build : Pct::PlaceBuilding;
      } else {
        Command c; c.type = Pct::Build; c.side = uint8_t(w.local_side); c.a = m.type;
        if (uint32_t id = production_member(w, town)) c.ids = {id};
        sim_->submit(c);
      }
    }
  }
  if (w.local_side >= 0 && w.local_side < int(w.side_state.size())) {
    const SideState& ss = w.side_state[w.local_side];
    if (!queue.empty() && ss.queue[queue.front()].started && control == 22) {
      Command c; c.type = Pct::PauseBuild; c.side = uint8_t(w.local_side);
      c.a = !ss.queue[queue.front()].paused;
      c.b = int(queue.front()) + 1;
      if (auto id = production_member(w, town)) c.ids = {id};
      sim_->submit(c);
    }
    for (size_t i = 0; i + queue_offset_ < queue.size() && i < l.queue.size(); i++) if (control == 200 + int(queue[i + queue_offset_])) {
      const size_t index = queue[i + queue_offset_];
      const auto& q = ss.queue[index];
      Command c; c.type = Pct::QueueKill; c.side = uint8_t(w.local_side);
      c.a = int(index);
      if (in.shift && q.tile_x >= 0 && q.started) {
        c.type = Pct::PauseBuild; c.a = !q.paused; c.b = int(index) + 1;
        if (auto id = production_member(w, town)) c.ids = {id};
      }
      sim_->submit(c);
    }
  }
  return true;
}

std::string CommandsUi::draw_menu(SDL_Renderer* r, SpriteStore& store, const Font& font, const StrBase& strings, const World& w,
                                  const SDL_Rect& panel, int sc, int mx, int my) const {
  auto l = layout(panel, sc);
  const int ts = font.ui_scale(sc);
  std::string tip;
  auto draw_icon = [&](int type, bool portrait, const SDL_Rect& rect, bool enabled = true, int side = -1, int shade = 14) {
    SpriteKey k; k.index = icon_sprite(type, portrait); k.shade = uint8_t(shade); k.team = uint8_t(w.side_color(side < 0 ? w.local_side : side));
    if (!k.index || !blit(r, store, k, rect, 0, enabled)) font.draw(r, rect.x + 4 * sc, rect.y + 8 * sc, "?", ts, {235, 220, 190, 255});
  };
  if (!w.selection.ids.empty()) {
    const Object* o = w.find(w.selection.ids.front());
    if (o) {
      draw_icon(o->type, true, l.portrait, true, o->side);
      const auto lines = font.wrap(type_display_name(strings, o->type), ts, l.summary.w);
      int y = l.summary.y;
      for (const auto& line : lines) {
        if (y + font.height(ts) > l.summary.y + 32 * sc) break;
        font.draw(r, l.summary.x, y, line, ts, {255, 240, 190, 255});
        y += font.height(ts) + sc;
      }
      const std::string stats = "HP " + std::to_string(o->hp) + " x" + std::to_string(w.selection.ids.size());
      font.draw(r, l.summary.x, l.summary.y + 38 * sc, stats, ts, {230, 230, 210, 255});
    }
  } else font.draw(r, l.summary.x, l.summary.y + 12 * sc, "No selection", ts, {210, 200, 180, 255});
  if (!sim_) return {};
  for (size_t i = 0; i < l.commands.size(); i++) {
    bool enabled = action_enabled(PanelAction(i), w, &sim_->rules()), hover = contains(l.commands[i], mx, my);
    SpriteKey k; k.archive = ArchiveId::Common; k.index = kButtons[i]; k.shade = 0;
    bool selected = (i == 0 && mode_ == TargetMode::Move) || (i == 1 && mode_ == TargetMode::Attack);
    if (i == 3 || i == 5) {
      for (uint32_t id : w.selection.ids) {
        const Object* o = w.find(id);
        if (!o || o->side != w.local_side || !o->alive || o->removed) continue;
        for (const TownState& town : w.town_state)
          if (town.index == o->town && town.side == o->side)
            selected |= i == 3 ? o->town_hall && town.repair_mode != RepairMode::Off :
                std::find(town.marked.begin(), town.marked.end(), id) != town.marked.end();
      }
    }
    const int frame = enabled && selected ? int(ui_frame_ % 12) : 0;
    if (!blit(r, store, k, l.commands[i], frame, enabled)) font.draw(r, l.commands[i].x, l.commands[i].y, kLabels[i], ts, {220, 210, 180, 255});
    if (hover) {
      tip = kLabels[i];
      const char* action = i == 3 ? "Repair" : i == 4 ? "Garrison" : i == 5 ? "Destroy" : nullptr;
      if (action) {
        int id = key_action_id(action);
        for (const KeyBinding& b : bindings_) if (b.action == id && sdl_scancode(b.dik) != SDL_SCANCODE_UNKNOWN) {
          tip += std::string(" (") + SDL_GetScancodeName(sdl_scancode(b.dik)) + ")"; break;
        }
      } else {
        SDL_Scancode shortcut = i == 0 ? SDL_SCANCODE_M : i == 1 ? SDL_SCANCODE_X : SDL_SCANCODE_H;
        if (!key_bound(shortcut)) tip += std::string(" (") + SDL_GetScancodeName(shortcut) + ")";
      }
      if (i == 3) {
        tip += "\nCycle town service: automatic, marked repair, marked destruction";
        int selected = production_town(w);
        if (selected >= 0 && selected < int(w.town_state.size())) {
          const char* modes[] = {"Automatic", "Marked repair", "Marked destruction"};
          tip += std::string("\nCurrent service: ") + modes[int(w.town_state[size_t(selected)].repair_mode)];
        }
      }
      else if (i == 4) tip += "\nExtract the selected town's troops";
      else if (i == 5) tip += "\nToggle buildings in the town's active repairer list";
      else if (!enabled) tip += "\nSelect an owned unit";
      else if (i == 1) tip += "\nClick an enemy to attack, or terrain to attack-move";
    }
  }
  int town = production_town(w);
  auto queue = production_queue(w, town);
  const int waiting = int(std::count_if(queue.begin(), queue.end(), [&](size_t i) {
    return !w.side_state[w.local_side].queue[i].started;
  }));
  auto items = sim_->menu(w, w.local_side, town);
  int count = int(l.production.size()), pages = std::max(1, (int(items.size()) + count - 1) / count);
  int page = std::clamp(page_, 0, pages - 1);
  font.draw(r, l.previous.x, l.previous.y, "<", ts, {240, 220, 170, 255});
  std::string town_title = "Town " + (town < 0 ? std::string("?") : std::to_string(w.mission.towns[size_t(town)].index));
  font.draw(r, l.previous.x, l.previous.y - 16 * sc, town_title, ts, {240, 220, 170, 255});
  std::string title = std::to_string(page + 1) + "/" + std::to_string(pages);
  font.draw(r, l.previous.x, l.previous.y + 20 * sc, title, ts, {240, 220, 170, 255});
  font.draw(r, l.next.x, l.next.y, ">", ts, {240, 220, 170, 255});
  const SideState* ss = w.local_side >= 0 && w.local_side < int(w.side_state.size()) ? &w.side_state[w.local_side] : nullptr;
  for (int i = 0; i < count && page * count + i < int(items.size()); i++) {
    const MenuItem& m = items[size_t(page * count + i)];
    const SDL_Rect& rect = l.production[size_t(i)];
    bool full = waiting >= kProductionQueueCapacity;
    bool enabled = m.enabled && !full;
    // Production availability is encoded by the resource palette shades.
    // Hover does not add a rectangle to the original command-card cells.
    draw_icon(m.type, false, rect, true, -1, enabled ? 9 : 1);
    if (contains(rect, mx, my)) {
      tip = type_display_name(strings, m.type) + "\nCost: " + std::to_string(m.price) + "  Time: " + std::to_string(m.time);
      if (!m.enabled) tip += std::string("\nUnavailable: ") + m.why_not;
      else if (full) tip += "\nUnavailable: production queue is full";
      else if (m.construct) tip += "\nClick terrain to place; Shift repeats";
    }
  }
  if (ss) {
    const QueueItem* active = !queue.empty() && ss->queue[queue.front()].started ? &ss->queue[queue.front()] : nullptr;
    const char* label = queue.empty() ? "Queue empty" : !active ? "Waiting for production" :
                        active->paused ? (active->tile_x >= 0 ? "Resume item" : "Resume town") :
                        active->tile_x >= 0 ? "Pause item" : "Pause town";
    font.draw(r, l.pause.x, l.pause.y, label, ts, {220, 225, 185, 255});
    if (!queue.empty() && contains(l.pause, mx, my)) tip = std::string(label) + "\nScroll to view waiting orders";
    const size_t offset = queue_town_ == town ? size_t(std::clamp(queue_offset_, 0, std::max(0, int(queue.size()) - 8))) : 0;
    for (size_t i = 0; i + offset < queue.size() && i < l.queue.size(); i++) {
      const QueueItem& q = ss->queue[queue[i + offset]];
      const SDL_Rect& rect = l.queue[i];
      draw_icon(q.type, false, rect);
      int pct = std::clamp(100 - q.remaining * 100 / std::max(1, q.total), 0, 100);
      SDL_Rect bar{rect.x, rect.y + rect.h - 3 * sc, rect.w * pct / 100, 3 * sc};
      SDL_SetRenderDrawColor(r, 90, 210, 90, 255); SDL_RenderFillRect(r, &bar);
      border(r, rect, contains(rect, mx, my));
      if (q.tile_x >= 0 && q.started) {
        SDL_Rect status{rect.x + sc, rect.y + sc, 3 * sc, 3 * sc};
        SDL_SetRenderDrawColor(r, q.paused ? 230 : 90, q.paused ? 180 : 210, 90, 255);
        SDL_RenderFillRect(r, &status);
      }
      if (contains(rect, mx, my)) tip = type_display_name(strings, q.type) + " " + std::to_string(pct) + "%\nTime remaining: " + std::to_string(q.remaining) + "\nClick to cancel; refund: " + std::to_string(q.refund()) + (q.paused ? "\nPaused" : !q.started ? "\nWaiting; unpaid" : "") + "\nScroll to view waiting orders";
      if (contains(rect, mx, my) && q.tile_x >= 0 && q.started)
        tip += q.paused ? "\nShift-click to resume construction" : "\nConstruction in progress; Shift-click to pause";
    }
  }
  if (wants_left_click() && tip.empty()) tip = placing_ >= 0 ? "Place building: click terrain\nRight-click or Escape cancels" : mode_ == TargetMode::Move ? "Move: click a destination\nRight-click or Escape cancels" : "Attack: click enemy or terrain\nRight-click or Escape cancels";
  return tip;
}
void CommandsUi::draw_ghost(SDL_Renderer* r, const World& w, const Camera& cam, int mx, int my) const {
  if (!sim_ || placing_ < 0) return;
  int tx = int(std::floor(cam.to_world_x(float(mx)) / kTilePx)), ty = int(std::floor(cam.to_world_y(float(my)) / kTilePx));
  SDL_BlendMode old_blend; SDL_GetRenderDrawBlendMode(r, &old_blend);
  SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
  for (const auto& cell : sim_->placement_cells(w, w.local_side, placing_, tx, ty, placing_town_)) {
    SDL_FRect f{cam.to_screen_x(float(cell.x * kTilePx)), cam.to_screen_y(float(cell.y * kTilePx)),
                kTilePx * cam.zoom, kTilePx * cam.zoom};
    const bool ok = !cell.blocked;
    // P presentation colours until an isolated reference capture is available.
    SDL_SetRenderDrawColor(r, ok ? 60 : 230, ok ? 220 : 50, 60, 110);
    SDL_RenderFillRectF(r, &f);
    SDL_SetRenderDrawColor(r, ok ? 60 : 230, ok ? 220 : 50, 60, 230);
    SDL_RenderDrawRectF(r, &f);
  }
  SDL_SetRenderDrawBlendMode(r, old_blend);
}

}  // namespace aztec::engine
