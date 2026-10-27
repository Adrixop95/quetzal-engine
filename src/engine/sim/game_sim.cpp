#include "engine/sim/game_sim.hpp"

#include <algorithm>
#include <cstdlib>

#include "engine/world/type_info.hpp"

namespace aztec::engine {

namespace {
constexpr int kAnimIdle = 0, kAnimMove = 1, kAnimAttack = 2, kAnimDie = 3;
inline int px(Fixed f) { return f >> 8; }
inline int64_t d2(const Object& a, const Object& b) {
  int64_t dx = px(a.cur_x) - px(b.cur_x), dy = px(a.cur_y) - px(b.cur_y);
  return dx * dx + dy * dy;
}
// 8-direction facing from a vector: 0 = east, counter-clockwise (2 = north), as used by the animation tables.
int facing_from(int dx, int dy) {
  if (dx == 0 && dy == 0) return -1;
  int a = path::angle_of(dx, dy);          // 256-step, 0 = +x, clockwise on screen (64 = down)
  return ((256 - a + 16) >> 5) & 7;
}
}  // namespace

GameSim::GameSim(std::shared_ptr<const RulesData> rules) : rules_(std::move(rules)) {}

void GameSim::footprint(const TypeRules& r, int tx, int ty, int& x0, int& y0, int& x1, int& y1) {
  x0 = tx - (r.foot_w - 1) / 2; x1 = x0 + r.foot_w - 1;
  y0 = ty - (r.foot_h - 1) / 2; y1 = y0 + r.foot_h - 1;
}

void GameSim::attach_agent(Object& o) {
  const TypeRules& r = rules_->type(o.type);
  if (!r.mobile || r.flier || !mover_) { o.mover = -1; return; }
  o.mover = int32_t(mover_->add_agent({o.cur_x, o.cur_y}, r.radius_px * 256, std::max(256, r.speed),
                                      r.large ? path::MoveClass::Large : path::MoveClass::Small));
}

void GameSim::init(World& w) {
  tick_ = 0;
  w.effects.clear();
  nav_ = path::NavGrid();
  if (w.passability.w == w.terrain.w && w.passability.h == w.terrain.h && !w.passability.v.empty())
    nav_.load_dat(w.passability.w, w.passability.h, w.passability.v.data());
  else
    nav_.reset(w.terrain.w, w.terrain.h);
  mover_ = std::make_unique<path::MoveSystem>(nav_);
  if (w.side_state.size() < 8) w.side_state.resize(8);
  paused_ = false;
  spawn_counter_ = 0;
  rng_ = 12345;
  if (script_.loaded()) script_.init(w);
  // largest mission hp per type = fallback maximum
  std::vector<int> seen(kNumTypes, 0);
  for (const Object& o : w.objects)
    if (o.type >= 0 && o.type < kNumTypes) seen[size_t(o.type)] = std::max(seen[size_t(o.type)], o.hp);
  for (Object& o : w.objects) {
    if (o.type < 0 || o.type >= kNumTypes) { o.type = -1; o.alive = true; continue; }   // unknown types become inert markers
    const TypeRules& r = rules_->type(o.type);
    o.hp_max = r.hp_src == Src::Verified ? r.hp_max : std::max(seen[size_t(o.type)], r.structure ? r.hp_max : 1);
    if (o.hp <= 0) o.hp = o.hp_max;
    o.secondary_hp = r.secondary_hp;
    o.progress = 1000;
    attach_agent(o);
    if (r.structure) {
      int x0, y0, x1, y1;
      footprint(r, px(o.cur_x) / kTilePx, px(o.cur_y) / kTilePx, x0, y0, x1, y1);
      nav_.add_block(x0, y0, x1 - x0 + 1, y1 - y0 + 1, +1);
    }
  }
}

int GameSim::town_index(const World& w, int side) const {
  for (size_t i = 0; i < w.mission.towns.size(); i++)
    if (w.mission.towns[i].side == side) return int(i);
  return -1;
}

uint32_t GameSim::spawn(World& w, int type, int side, int pxx, int pyy, int hp, bool constructing) {
  Object o;
  o.id = uint32_t(w.objects.size() + 1);
  o.type = type;
  o.side = side;
  o.source = ObjectSource::Team;
  o.facing = 6;
  const TypeRules& r = rules_->type(type);
  o.hp_max = r.hp_max > 0 ? r.hp_max : std::max(hp, 1);
  o.hp = hp > 0 ? hp : o.hp_max;
  o.secondary_hp = r.secondary_hp;
  o.anim_variant = next_rand();
  o.teleport(to_fixed(pxx), to_fixed(pyy));
  if (constructing) {
    o.progress = 0; o.build_elapsed = 0; o.build_total = std::max(1, r.time);
    o.hp = 1;
    o.anim_state = kAnimConstruct;
  }
  w.objects.push_back(o);
  Object& ref = w.objects.back();
  attach_agent(ref);
  if (r.structure) {
    int x0, y0, x1, y1;
    footprint(r, pxx / kTilePx, pyy / kTilePx, x0, y0, x1, y1);
    nav_.add_block(x0, y0, x1 - x0 + 1, y1 - y0 + 1, +1);
  }
  return ref.id;
}

void GameSim::kill(World& w, Object& o) {
  if (!o.alive) return;
  o.alive = false;
  o.hp = 0;
  o.order = uint8_t(Order::Idle);
  o.target = 0;
  o.corpse_ticks = 0;
  const TypeRules& r = rules_->type(o.type);
  if (r.kind == 1 || r.kind == 3 || r.kind == 4 || r.kind == 5 || r.kind == 6 || r.kind == 9) {
    decoration(w, data::kVehicleExplosionAnim, int(next_rand() % 3), o.cur_x, o.cur_y, o.render_height, o.side);
    if (!r.flier) decoration(w, data::kVehicleDebrisAnim, int(next_rand() % 20), o.cur_x, o.cur_y, -1, o.side);
    o.removed = true;
  }
  if (r.structure) {
    // Known-radius rubble is drawn by the object provider; retain the provisional
    // small-rubble effect only when its radius is unavailable.
    if (o.destruction_radius < 0 && o.progress >= 1000)
      decoration(w, 116, int(next_rand() % 20), o.cur_x, o.cur_y, -1, o.side);
    int x0, y0, x1, y1;
    footprint(r, px(o.cur_x) / kTilePx, px(o.cur_y) / kTilePx, x0, y0, x1, y1);
    nav_.add_block(x0, y0, x1 - x0 + 1, y1 - y0 + 1, -1);
    // Keep known rubble visible for its resource lifetime; unknown radii and
    // construction cancellations remain explicit missing visual inputs.
    if (o.destruction_radius < 0 || o.progress < 1000) { o.removed = true; return; }
  }
  o.anim_state = kAnimDie;
  o.anim_tick = 0;
  if (o.mover >= 0) {
    mover_->stop(uint32_t(o.mover));
    path::Agent& a = mover_->agents_mut()[size_t(o.mover)];
    a.radius = 0;   // corpses do not block
    a.hold = true;
  }
}

const Object* GameSim::nearest_enemy(const World& w, const Object& o, int radius_px) const {
  const Object* best = nullptr;
  int64_t bd = int64_t(radius_px) * radius_px;
  for (const Object& t : w.objects) {
    if (t.removed || !t.alive || t.type < 0 || !w.hostile(o.side, t.side) || t.progress < 1000) continue;
    const auto shot = rules_->attack(o.type, t.type, !w.hostile(o.side, t.side));
    if (!shot.damage_known || !shot.getters_known || shot.damage <= 0) continue;
    int64_t d = d2(o, t);
    if (d < bd || (d == bd && best && t.id < best->id)) { bd = d; best = &t; }
  }
  return best;
}

// ---- commands ----------------------------------------------------------------------------------------------------

void GameSim::apply(World& w, const Command& c) {
  auto own_ids = [&](bool need_mobile) {
    std::vector<Object*> v;
    for (uint32_t id : c.ids) {
      Object* o = w.find_mut(id);
      if (!o || o->removed || !o->alive || o->side != c.side || o->type < 0) continue;
      const TypeRules& r = rules_->type(o->type);
      if (need_mobile && !(r.mobile || r.flier)) continue;
      v.push_back(o);
    }
    return v;
  };
  SideState* ss = c.side < w.side_state.size() ? &w.side_state[c.side] : nullptr;
  switch (c.type) {
    case Pct::PauseOn: paused_ = true; break;
    case Pct::PauseOff: paused_ = false; break;
    case Pct::Move:
    case Pct::AttackMove: {
      std::vector<Object*> v = own_ids(true);
      std::vector<uint32_t> agents;
      // Coordinates come from untrusted command packets: clamp into the map before any fixed-point conversion.
      const int cx = std::clamp(c.a, 0, std::max(0, w.width_px() - 1)), cy = std::clamp(c.b, 0, std::max(0, w.height_px() - 1));
      for (Object* o : v) {
        o->order = uint8_t(c.type == Pct::Move ? Order::Move : Order::AttackMove);
        o->dest_x = cx; o->dest_y = cy; o->target = 0; o->resume = 0;
        if (o->mover >= 0) agents.push_back(uint32_t(o->mover));
      }
      if (!agents.empty()) mover_->order_move(agents.data(), int(agents.size()), {to_fixed(cx), to_fixed(cy)});
      break;
    }
    case Pct::Attack: {
      const Object* t = w.find(uint32_t(c.a));
      if (!t || t->removed || !t->alive) break;
      for (Object* o : own_ids(true)) {
        if (o->id == t->id || !rules_->type(o->type).armed) continue;
        o->order = uint8_t(Order::Attack); o->target = t->id; o->resume = 0;
      }
      break;
    }
    case Pct::Stop:
      for (Object* o : own_ids(true)) {
        o->order = uint8_t(Order::Idle); o->target = 0; o->resume = 0;
        if (o->mover >= 0) mover_->stop(uint32_t(o->mover));
      }
      break;
    case Pct::DestroyHere:
      for (Object* o : own_ids(false)) kill(w, *o);
      break;
    case Pct::CreateGroup:
    case Pct::AddToGroup:
      if (ss && c.a >= 0 && c.a < 10) {
        std::vector<uint32_t>& g = ss->groups[c.a];
        if (c.type == Pct::CreateGroup) g.clear();
        for (Object* o : own_ids(false))
          if (std::find(g.begin(), g.end(), o->id) == g.end()) g.push_back(o->id);
      }
      break;
    case Pct::PauseBuild: if (ss) ss->build_paused = c.a != 0; break;
    case Pct::QueueKill:
      if (ss && c.a >= 0 && c.a < int(ss->queue.size())) {
        ss->money += rules_->type(ss->queue[size_t(c.a)].type).price;  // refund (placeholder: full)
        ss->queue.erase(ss->queue.begin() + c.a);
      }
      break;
    case Pct::Build: {
      if (!ss) break;
      const char* why = "";
      const std::vector<MenuItem> items = menu(w, c.side);
      const MenuItem* mi = nullptr;
      for (const MenuItem& m : items) if (m.type == c.a) mi = &m;
      if (!mi || !mi->enabled) break;
      if (mi->construct) {
        if (c.b < 0 || c.c < 0 || c.b >= w.terrain.w || c.c >= w.terrain.h) break;
        if (!can_place(w, c.side, c.a, c.b, c.c, &why)) break;
        ss->money -= mi->price;
        spawn(w, c.a, c.side, c.b * kTilePx + kTilePx / 2, c.c * kTilePx + kTilePx / 2, 0, true);
      } else if (ss->queue.size() < 8) {
        ss->money -= mi->price;
        ss->queue.push_back({c.a, std::max(1, mi->time), std::max(1, mi->time)});
      }
      break;
    }
    default: break;  // Garrison, RepairMode, AttackMode, BuildMode, SelectGroup, ...: recorded but not implemented
  }
}

// ---- economy -----------------------------------------------------------------------------------------------------

bool GameSim::menu_ok(const World& w, int side, const BuildItem& it, const char** why) const {
  const SideDef* sd = nullptr;
  for (const SideDef& s : w.mission.sides) if (s.index == side) sd = &s;
  if (sd && std::find(sd->disabled.begin(), sd->disabled.end(), it.type) != sd->disabled.end()) { *why = "disabled"; return false; }
  for (int need : it.prereq) {
    bool have = false;
    for (const Object& o : w.objects)
      if (!o.removed && o.alive && o.side == side && o.type == need && o.progress >= 1000) { have = true; break; }
    if (!have) { *why = "needs building"; return false; }
  }
  const TypeRules& r = rules_->type(it.type);
  if (side < 0 || side >= int(w.side_state.size()) || w.side_state[side].money < r.price) { *why = "no gold"; return false; }
  return true;
}

std::vector<MenuItem> GameSim::menu(const World& w, int side) const {
  std::vector<MenuItem> out;
  int ti = town_index(w, side);
  if (ti < 0) return out;
  const TownDef& t = w.mission.towns[size_t(ti)];
  for (const BuildItem& it : rules_->build_list(t.race, t.type)) {
    const TypeRules& r = rules_->type(it.type);
    if (r.structure && !it.construct) continue;    // town upgrades: not implemented
    MenuItem m;
    m.type = it.type; m.construct = it.construct; m.price = r.price; m.time = r.time;
    const char* why = "";
    m.enabled = menu_ok(w, side, it, &why);
    m.why_not = why;
    out.push_back(m);
  }
  return out;
}

bool GameSim::can_place(const World& w, int side, int type, int tx, int ty, const char** why) const {
  (void)side;
  const TypeRules& r = rules_->type(type);
  int x0, y0, x1, y1;
  footprint(r, tx, ty, x0, y0, x1, y1);
  for (int y = y0; y <= y1; y++)
    for (int x = x0; x <= x1; x++)
      if (!nav_.in_bounds(x, y) || nav_.blocked(x, y, path::MoveClass::Large)) {
        if (why) *why = "blocked";
        return false;
      }
  (void)w;
  return true;
}

void GameSim::economy(World& w) {
  for (size_t s = 0; s < w.side_state.size(); s++) {
    SideState& ss = w.side_state[s];
    if (!ss.queue.empty() && !ss.build_paused) {
      QueueItem& q = ss.queue.front();
      if (--q.remaining <= 0) {
        int type = q.type;
        ss.queue.erase(ss.queue.begin());
        int ti = town_index(w, int(s));
        int bx = 0, by = 0, fh = 2;
        bool found = false;
        if (ti >= 0) {
          const TownDef& t = w.mission.towns[size_t(ti)];
          int tb = rules_->town_building(t.type, t.race);
          for (const Object& o : w.objects)
            if (!o.removed && o.alive && o.side == int(s) && o.type == tb) { bx = px(o.cur_x); by = px(o.cur_y); fh = rules_->type(tb).foot_h; found = true; break; }
        }
        if (!found) { for (const SideDef& sd : w.mission.sides) if (sd.index == int(s)) { bx = sd.start_x * kTilePx + 24; by = sd.start_y * kTilePx + 24; } }
        int slot = int(spawn_counter_++ % 5);
        spawn(w, type, int(s), bx + (slot - 2) * 26, by + fh * 24 + 40, 0, false);
      }
    }
  }
  // construction progress and placeholder income
  for (Object& o : w.objects) {
    if (o.removed || !o.alive || o.type < 0) continue;
    const TypeRules& r = rules_->type(o.type);
    if (!r.structure) continue;
    if (o.progress < 1000) {
      o.build_elapsed++;
      o.progress = std::min(1000, o.build_elapsed * 1000 / std::max(1, o.build_total));
      o.hp = std::max(1, int(int64_t(o.hp_max) * o.progress / 1000));
      if (o.progress >= 1000) { o.anim_state = kAnimIdle; o.anim_tick = 0; }
    } else if (o.side >= 0 && o.side < int(w.side_state.size()) && tick_ % kIncomeEvery == 0) {
      const char* n = type_info(o.type) ? type_info(o.type)->name : "";
      int gain = 0;
      if (std::string(n).find("_MINE") != std::string::npos) gain = 4;       // PLACEHOLDER (Harvest.cpp not decoded)
      else if (std::string(n).find("_FARM") != std::string::npos) gain = 1;  // PLACEHOLDER
      w.side_state[o.side].money += gain;
    }
  }
}

// ---- orders, combat ----------------------------------------------------------------------------------------------

void GameSim::run_orders(World& w) {
  for (size_t i = 0; i < w.objects.size(); i++) {
    Object& o = w.objects[i];
    if (o.removed || o.type < 0) continue;
    if (!o.alive) {
      if (++o.corpse_ticks > (rules_->type(o.type).structure ? 249 : kCorpseTicks)) { o.removed = true; }
      continue;
    }
    const TypeRules& r = rules_->type(o.type);
    if (o.cooldown > 0) o.cooldown--;
    if (o.progress < 1000) continue;
    o.hp = regenerated_hp(r, o.hp, o.regen_alternate);
    path::Agent* ag = o.mover >= 0 ? &mover_->agents_mut()[size_t(o.mover)] : nullptr;
    bool moving = ag && ag->state == path::AgentState::Moving;

    if (o.order == uint8_t(Order::Move) && !moving) {
      if (r.flier) {
        int dx = o.dest_x - px(o.cur_x), dy = o.dest_y - px(o.cur_y);
        int64_t dist2 = int64_t(dx) * dx + int64_t(dy) * dy;
        int step = std::max(1, r.speed >> 8);
        if (dist2 <= int64_t(step) * step) { o.cur_x = to_fixed(o.dest_x); o.cur_y = to_fixed(o.dest_y); o.order = uint8_t(Order::Idle); }
        else {
          path::Vec2 v = path::with_length({dx * 256, dy * 256}, r.speed);
          o.cur_x += v.x; o.cur_y += v.y;
          int f = facing_from(dx, dy); if (f >= 0) o.facing = uint8_t(f);
          o.set_anim(kAnimMove);
        }
      } else {
        o.order = uint8_t(Order::Idle);
      }
    }
    if (o.order == uint8_t(Order::AttackMove) && ag && !moving) o.order = uint8_t(Order::Idle);

    if (!r.armed) continue;
    // acquire targets: idle units and attack-moving units react to enemies in sight
    if (o.order == uint8_t(Order::Idle) || o.order == uint8_t(Order::AttackMove)) {
      if ((tick_ + o.id) % 4 == 0) {
        if (const Object* e = nearest_enemy(w, o, r.sight_px)) {
          o.resume = o.order == uint8_t(Order::AttackMove) ? 1 : 0;
          o.order = uint8_t(Order::Attack);
          o.target = e->id;
        }
      }
    }
    if (o.order != uint8_t(Order::Attack)) continue;
    Object* t = w.find_mut(o.target);
    if (!t || t->removed || !t->alive) {
      o.target = 0;
      if (o.resume && ag) {
        o.order = uint8_t(Order::AttackMove); o.resume = 0;
        uint32_t aid = uint32_t(o.mover);
        mover_->order_move(&aid, 1, {to_fixed(o.dest_x), to_fixed(o.dest_y)});
      } else {
        o.order = uint8_t(Order::Idle);
      }
      continue;
    }
    const TypeRules& tr = rules_->type(t->type);
    int treach = tr.structure ? std::max(tr.foot_w, tr.foot_h) * 24 : tr.radius_px;
    const auto shot = rules_->attack(o.type, t->type, !w.hostile(o.side, t->side));
    if (!shot.damage_known || !shot.getters_known || shot.damage <= 0) continue;
    int64_t reach = int64_t(shot.range_px) + treach;
    if (d2(o, *t) <= reach * reach) {
      if (ag && moving) mover_->stop(uint32_t(o.mover));
      int f = facing_from(px(t->cur_x) - px(o.cur_x), px(t->cur_y) - px(o.cur_y));
      if (f >= 0) o.facing = uint8_t(f);
      if (o.cooldown == 0) {
        // Descriptor damage/reload feed provisional projectile flight; direct weapons hit immediately.
        o.cooldown = std::max(1, shot.reload + 1);
        o.has_fired = true;
        o.anim_state = kAnimAttack; o.anim_tick = 0;
        fire(w, o, *t, shot.damage);
      }
    } else if (ag && ((tick_ + o.id) % 6 == 0 || !moving)) {
      uint32_t aid = uint32_t(o.mover);
      mover_->order_move(&aid, 1, {t->cur_x, t->cur_y});
    }
  }
}

void GameSim::decoration(World& w, int anim, int slot, Fixed x, Fixed y, int height, int side, uint32_t source) {
  Effect e;
  e.anim = anim; e.slot = slot; e.x = e.prev_x = x; e.y = e.prev_y = y;
  e.height = height; e.side = side; e.source = source;
  e.lifetime = data::effect_lifetime(anim, slot);
  for (const auto& info : data::kEffectAnims) if (info.id == anim) e.kind = info.owner;
  if (e.lifetime > 0) w.effects.push_back(e);
}

void GameSim::fire(World& w, const Object& source, Object& target, int damage) {
  fire_weapon(w, rules_->type(source.type).weapon, source, target, damage);
}

void GameSim::fire_weapon(World& w, const WeaponRules& weapon, const Object& source, Object& target, int damage) {
  if (!weapon.components.empty()) {
    // Preserve the measured/default total while per-component damage is unresolved.
    int count = int(weapon.components.size());
    for (int i = 0; i < count; i++)
      fire_weapon(w, weapon.components[size_t(i)], source, target, damage / count + (i < damage % count ? 1 : 0));
    return;
  }
  int dx = target.cur_x - source.cur_x, dy = target.cur_y - source.cur_y;
  int angle = (256 - path::angle_of(dx, dy)) & 255;
  if (weapon.muzzle) {
    auto offset = path::with_length({dx, dy}, 20 * 256);
    decoration(w, data::kBeamMuzzleAnim, 0, source.cur_x + offset.x, source.cur_y + offset.y, 0, source.side);
    int count = path::length({dx, dy}) / (30 * 256);
    for (int i = 1; i <= count; i++)
      decoration(w, data::kBeamSegmentAnim, data::direction32_from_angle(angle),
                 source.cur_x + int(int64_t(dx) * i / (count + 1)),
                 source.cur_y + int(int64_t(dy) * i / (count + 1)), 0, source.side);
    decoration(w, data::kBeamImpactAnim, 0, target.cur_x, target.cur_y, 0, source.side);
  }
  if (weapon.anim == 0) {
    target.hp -= damage; target.last_hit_by = source.id;
    if (target.hp <= 0) kill(w, target);
    return;
  }
  Effect e;
  e.kind = weapon.klass == 0 ? data::EffectClass::BallExpl : weapon.klass == 3 ? data::EffectClass::Axe :
           weapon.klass == 7 ? data::EffectClass::FireThrow : data::EffectClass::Arrow;
  e.source = source.id; e.target = target.id; e.side = source.side;
  e.anim = weapon.anim; e.angle = angle;
  auto sequence = data::projectile_sequence(e.kind, angle);
  e.slot = sequence.slot; e.loop = sequence.loop;
  e.x = e.prev_x = source.cur_x; e.y = e.prev_y = source.cur_y;
  e.end_x = target.cur_x; e.end_y = target.cur_y;
  // Snapshot aim and straight flight are provisional; no floating point enters the simulation.
  e.speed = std::max(1, weapon.speed) * 256;
  e.damage = damage; e.flight = true;
  e.impact_anim = weapon.impact_anim; e.splash_px = weapon.splash_px; e.shockwave = weapon.shockwave;
  w.effects.push_back(e);
}

void GameSim::advance_effects(World& w) {
  // Move the old list aside: impacts/deaths may append new decorations without invalidating iteration.
  auto effects = std::move(w.effects);
  w.effects.clear();
  for (auto e : effects) {
    e.prev_x = e.x; e.prev_y = e.y;
    if (!e.flight) {
      if (e.source && e.loop) {
        const auto* o = w.find(e.source);
        if (!o || !o->alive || o->removed || o->hp_max <= 0 || int64_t(o->hp) * 4 > int64_t(o->hp_max) * 3) continue;
        int stage = 3 - int((int64_t(4) * o->hp - 1) / o->hp_max);
        e.anim = 153 + stage;
        e.x = o->cur_x; e.y = o->cur_y + 256;
      }
      ++e.age;
      if (e.loop) e.age %= std::max(1, e.lifetime);
      if (e.age < e.lifetime || e.loop) w.effects.push_back(e);
      continue;
    }
    path::Vec2 delta{e.end_x - e.x, e.end_y - e.y};
    if (path::len2(delta) > int64_t(e.speed) * e.speed) {
      auto step = path::with_length(delta, e.speed);
      e.x += step.x; e.y += step.y; e.age++;
      w.effects.push_back(e);
      continue;
    }
    e.x = e.end_x; e.y = e.end_y;
    if (e.impact_anim) decoration(w, e.impact_anim, int(next_rand() % 3), e.x, e.y, 0, e.side);
    if (e.shockwave == data::kShockwaveAnim) decoration(w, e.shockwave, 0, e.x, e.y, 0, e.side);
    for (auto& o : w.objects) {
      if (!o.alive || o.removed) continue;
      int64_t dx = int64_t(o.cur_x) - e.x, dy = int64_t(o.cur_y) - e.y;
      bool splash = e.splash_px > 0 && w.hostile(e.side, o.side) &&
                    dx * dx + dy * dy <= int64_t(e.splash_px * 256) * (e.splash_px * 256);
      if (o.id != e.target && !splash) continue;
      o.hp -= e.damage; o.last_hit_by = e.source;
      if (o.hp <= 0) kill(w, o);
    }
  }
  for (const auto& o : w.objects) {
    if (!o.alive || o.removed || o.progress < 1000 || !rules_->type(o.type).structure || o.hp_max <= 0 ||
        int64_t(o.hp) * 4 > int64_t(o.hp_max) * 3) continue;
    bool exists = false;
    for (const auto& e : w.effects) if (e.loop && e.source == o.id) { exists = true; break; }
    if (!exists) {
      int stage = 3 - int((int64_t(4) * o.hp - 1) / o.hp_max);
      decoration(w, 153 + stage, 0, o.cur_x, o.cur_y + 256, 0, o.side, o.id);
      w.effects.back().loop = true;
    }
  }
}

void GameSim::sync_movement(World& w) {
  for (Object& o : w.objects) {
    if (o.removed || o.mover < 0) continue;
    const path::Agent& a = mover_->agents()[size_t(o.mover)];
    if (!o.alive) { continue; }
    o.cur_x = a.pos.x; o.cur_y = a.pos.y;
    bool mv = a.state == path::AgentState::Moving;
    if (mv) {
      int f = facing_from(a.vel.x, a.vel.y);
      if (f >= 0) o.facing = uint8_t(f);
    }
    bool shooting = o.anim_state == kAnimAttack && o.anim_tick < 8;
    if (!shooting) o.set_anim(mv ? kAnimMove : kAnimIdle);
  }
}

// Per-side fog layers (.vis): tiles within sight of a living own object become lit (0); tiles that were lit and are no
// longer seen stay "explored" (96). Unexplored = 152. Sight radius is a placeholder (docs/engine_rules_status.md).
void GameSim::update_fog(World& w) {
  int mw = w.terrain.w, mh = w.terrain.h;
  bool changed = false;
  for (size_t side = 0; side < w.visibility.sides.size(); side++) {
    VisFile::Layer& l = w.visibility.sides[side];
    if (l.w != mw || l.h != mh || l.v.size() < size_t(mw) * mh) continue;
    std::vector<uint8_t> seen(size_t(mw) * mh, 0);
    for (const Object& o : w.objects) {
      if (o.removed || !o.alive || o.type < 0 || o.side != int(side) || o.progress < 1000) continue;
      int rad = (rules_->type(o.type).structure ? 8 * kTilePx : rules_->type(o.type).sight_px) / kTilePx;
      int cx = px(o.cur_x) / kTilePx, cy = px(o.cur_y) / kTilePx;
      for (int y = std::max(0, cy - rad); y <= std::min(mh - 1, cy + rad); y++)
        for (int x = std::max(0, cx - rad); x <= std::min(mw - 1, cx + rad); x++)
          if ((x - cx) * (x - cx) + (y - cy) * (y - cy) <= rad * rad) seen[size_t(y) * mw + x] = 1;
    }
    for (size_t i = 0; i < seen.size(); i++) {
      uint8_t nv = seen[i] ? 0 : (l.v[i] == 0 ? 96 : l.v[i]);
      if (nv != l.v[i]) { l.v[i] = nv; changed = true; }
    }
  }
  if (changed) w.fog_version++;
}

void GameSim::tick(World& w) {
  w.tick++;
  tick_++;
  std::stable_sort(pending_.begin(), pending_.end(), [](const Command& a, const Command& b) { return a.side < b.side; });
  for (Command& c : pending_) {
    log_.push_back({tick_, c});
    apply(w, c);
  }
  pending_.clear();
  if (paused_) return;
  if (script_.loaded()) script_.tick(*this, w);
  economy(w);
  run_ai(w);
  advance_effects(w);
  run_orders(w);
  mover_->tick();
  sync_movement(w);
  if (tick_ % 4 == 0) update_fog(w);
}

void GameSim::script_order(World& w, const std::vector<uint32_t>& ids, int pxx, int pyy, bool attack_move) {
  pxx = std::clamp(pxx, 0, std::max(0, w.width_px() - 1));
  pyy = std::clamp(pyy, 0, std::max(0, w.height_px() - 1));
  // Script orders bypass the side check of player commands: apply per owning side directly (not logged: scripts are deterministic).
  std::vector<uint32_t> agents;
  for (uint32_t id : ids) {
    Object* o = w.find_mut(id);
    if (!o || o->removed || !o->alive) continue;
    const TypeRules& r = rules_->type(o->type);
    if (!(r.mobile || r.flier)) continue;
    o->order = uint8_t(attack_move ? Order::AttackMove : Order::Move);
    o->dest_x = pxx; o->dest_y = pyy; o->target = 0; o->resume = 0;
    if (o->mover >= 0) agents.push_back(uint32_t(o->mover));
  }
  if (!agents.empty()) mover_->order_move(agents.data(), int(agents.size()), {to_fixed(pxx), to_fixed(pyy)});
}

void GameSim::script_kill(World& w, uint32_t id) {
  Object* o = w.find_mut(id);
  if (o && o->alive) kill(w, *o);
}

void GameSim::teleport(World& w, uint32_t id, int pxx, int pyy) {
  Object* o = w.find_mut(id);
  if (!o) return;
  o->teleport(to_fixed(pxx), to_fixed(pyy));
  if (o->mover >= 0) {
    path::Agent& a = mover_->agents_mut()[size_t(o->mover)];
    a.pos = a.prev_pos = a.win_start = {to_fixed(pxx), to_fixed(pyy)};
  }
}

uint64_t GameSim::state_hash(const World& w) const {
  uint64_t h = 1469598103934665603ull;
  auto mix = [&](uint64_t v) { h = (h ^ v) * 1099511628211ull; };
  mix(tick_); mix(rng_); mix(paused_); mix(spawn_counter_); mix(w.objects.size());
  for (const Object& o : w.objects) {
    mix(o.id); mix(uint32_t(o.type)); mix(uint32_t(o.side)); mix(uint32_t(o.hp)); mix(uint32_t(o.cur_x)); mix(uint32_t(o.cur_y));
    mix(o.order); mix(o.target); mix(uint32_t(o.cooldown)); mix(o.alive); mix(o.removed); mix(uint32_t(o.progress));
    mix(uint32_t(o.render_height)); mix(uint32_t(o.destruction_radius));
    mix(uint32_t(o.transport_phase)); mix(uint32_t(o.orientation)); mix(o.construction_overlay); mix(o.has_fired);
    mix(o.facing); mix(o.anim_state); mix(uint32_t(o.corpse_ticks));
    mix(uint32_t(o.dest_x)); mix(uint32_t(o.dest_y)); mix(o.resume); mix(uint32_t(o.build_elapsed)); mix(uint32_t(o.build_total));
    mix(uint32_t(o.secondary_hp)); mix(o.regen_alternate);
    mix(uint32_t(o.hp_max)); mix(uint32_t(o.tactic)); mix(uint32_t(o.scenario)); mix(o.last_hit_by); mix(uint32_t(o.mover)); mix(o.anim_variant);
  }
  mix(w.effects.size());
  for (const auto& e : w.effects) {
    mix(uint32_t(e.kind)); mix(e.source); mix(e.target); mix(uint32_t(e.side));
    mix(e.anim); mix(e.slot); mix(e.angle); mix(uint32_t(e.x)); mix(uint32_t(e.y));
    mix(uint32_t(e.end_x)); mix(uint32_t(e.end_y)); mix(uint32_t(e.height));
    mix(e.age); mix(e.lifetime); mix(e.speed); mix(e.damage); mix(e.splash_px);
    mix(e.impact_anim); mix(e.shockwave); mix(e.flight); mix(e.loop);
  }
  for (const SideState& s : w.side_state) {
    mix(uint32_t(s.money)); mix(s.build_paused); mix(s.queue.size());
    for (const QueueItem& q : s.queue) { mix(uint32_t(q.type)); mix(uint32_t(q.remaining)); mix(uint32_t(q.total)); }
    for (const auto& g : s.groups) { mix(g.size()); for (uint32_t id : g) mix(id); }
  }
  for (const VisFile::Layer& l : w.visibility.sides) { uint64_t sum = 0; for (uint8_t v : l.v) sum = sum * 31 + v; mix(sum); }
  mix(script_.state_hash());
  for (int a = 0; a < 8; a++) for (int b = 0; b < 8; b++) mix(w.relation[a][b]);
  mix(w.outcome);
  if (mover_) mix(mover_->state_hash());
  return h;
}

std::unique_ptr<GameSim> GameSim::replay(World& fresh, std::shared_ptr<const RulesData> rules, const std::vector<LoggedCommand>& log,
                                         uint64_t until_tick, const std::string& scn_path) {
  auto sim = std::make_unique<GameSim>(std::move(rules));
  if (!scn_path.empty()) sim->attach_script(scn_path);
  sim->init(fresh);
  size_t i = 0;
  for (uint64_t t = 1; t <= until_tick; t++) {
    while (i < log.size() && log[i].tick == t) sim->submit(log[i++].cmd);
    fresh.begin_tick();
    sim->tick(fresh);
  }
  return sim;
}

}  // namespace aztec::engine
