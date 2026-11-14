#include "engine/sim/game_sim.hpp"

#include <algorithm>
#include <cstdlib>
#include <limits>
#include <map>
#include <stdexcept>

#include "engine/world/type_info.hpp"
#include "engine/world/object_state.hpp"
#include "engine/sim/sight.hpp"
#include "engine/world/visibility.hpp"

namespace aztec::engine {

namespace {
constexpr int kAnimIdle = 0, kAnimMove = 1, kAnimAttack = 2, kAnimDie = 3;
inline int px(Fixed f) { return f >> 8; }
inline int64_t d2(const Object& a, const Object& b) {
  int64_t dx = px(a.cur_x) - px(b.cur_x), dy = px(a.cur_y) - px(b.cur_y);
  return dx * dx + dy * dy;
}
// Mobile weapon reach starts at the source collision disc, not its centre.
// Omitting that radius can make melee reach shorter than non-overlapping discs.
int64_t weapon_reach(const TypeRules& source, const TypeRules& target, int range) {
  const int target_extent = target.structure ? std::max(target.foot_w, target.foot_h) * 24 : target.radius_px;
  return int64_t(range) + target_extent + (source.mobile ? source.radius_px : 0);
}
// 8-direction facing from a vector: 0 = east, counter-clockwise (2 = north), as used by the animation tables.
int facing_from(int dx, int dy) {
  if (dx == 0 && dy == 0) return -1;
  int a = path::angle_of(dx, dy);          // 256-step, 0 = +x, clockwise on screen (64 = down)
  return ((256 - a + 16) >> 5) & 7;
}
Order resumed_order(uint8_t resume) {
  return resume == 3 ? Order::Follow : resume == 2 ? Order::Move : resume == 1 ? Order::AttackMove : Order::Idle;
}
}  // namespace

GameSim::GameSim(std::shared_ptr<const RulesData> rules, Modifications modifications)
    : rules_(std::move(rules)), modifications_(modifications), initial_modifications_(modifications) {}

void GameSim::footprint(const TypeRules& r, int tx, int ty, int& x0, int& y0, int& x1, int& y1) {
  if (r.has_footprint_mask) {
    x0 = tx + 3; y0 = ty + 3; x1 = tx - 3; y1 = ty - 3;
    for (int i = 0; i < 49; ++i) if (r.footprint_mask[i] == 2) {
      x0 = std::min(x0, tx + i % 7 - 3); x1 = std::max(x1, tx + i % 7 - 3);
      y0 = std::min(y0, ty + i / 7 - 3); y1 = std::max(y1, ty + i / 7 - 3);
    }
    return;
  }
  x0 = tx - (r.foot_w - 1) / 2; x1 = x0 + r.foot_w - 1;
  y0 = ty - (r.foot_h - 1) / 2; y1 = y0 + r.foot_h - 1;
}

bool GameSim::occupies(const TypeRules& r, int tx, int ty, int x, int y) {
  if (r.has_footprint_mask) {
    int dx = x - tx + 3, dy = y - ty + 3;
    return dx >= 0 && dy >= 0 && dx < 7 && dy < 7 && r.footprint_mask[dy * 7 + dx] == 2;
  }
  int x0, y0, x1, y1; footprint(r, tx, ty, x0, y0, x1, y1);
  return x >= x0 && x <= x1 && y >= y0 && y <= y1;
}

void GameSim::block_footprint(const TypeRules& r, int tx, int ty, int delta) {
  int x0, y0, x1, y1; footprint(r, tx, ty, x0, y0, x1, y1);
  for (int y = std::max(0, y0); y <= std::min(nav_.height() - 1, y1); ++y)
    for (int x = std::max(0, x0); x <= std::min(nav_.width() - 1, x1); ++x)
      if (occupies(r, tx, ty, x, y)) nav_.add_block(x, y, 1, 1, delta);
}

void GameSim::attach_agent(Object& o) {
  const TypeRules& r = rules_->type(o.type);
  if (!r.mobile || r.flier || !mover_) { o.mover = -1; return; }
  o.mover = int32_t(mover_->add_agent({o.cur_x, o.cur_y}, r.radius_px * 256, std::max(256, r.speed),
                                      is_transport(o.type) ? path::MoveClass::Water : r.large ? path::MoveClass::Large : path::MoveClass::Small, o.side));
}

void GameSim::init(World& w) {
  modifications_ = initial_modifications_;
  tick_ = w.original_save ? w.tick : 0;
  w.tick = tick_;
  pending_.clear();
  log_.clear();
  next_trace_id_ = 0;
  if (!w.original_save) w.day_clock = (w.mission.start_time % kDayTicks + kDayTicks) % kDayTicks;
  w.ai_towns.assign(w.mission.towns.size(), {});
  w.town_speed.assign(w.mission.towns.size(), 1);
  w.repair_credits.assign(w.mission.towns.size(), 0);
  w.effects.clear();
  // Reference computer intelligence starts with visibility value 24 (rules_ai.md).
  // Imported saves retain their recorded fog instead of restarting intelligence.
  if (!w.original_save) {
    for (const auto& side : w.mission.sides)
      if (side.only_comp && side.index >= 0 && side.index < int(w.visibility.sides.size()))
        std::fill(w.visibility.sides[side.index].v.begin(), w.visibility.sides[side.index].v.end(), uint8_t(w.visibility_brightness ? 128 : 24));
  }
  w.town_state.clear();
  for (const TownDef& t : w.mission.towns) {
    TownState state;
    state.index = t.index; state.side = t.side;
    for (const Object& o : w.objects)
      if (o.town_hall && o.town == t.index && o.side == t.side) { state.hall = o.id; break; }
    w.town_state.push_back(state);
  }
  for (SideState& s : w.side_state) { s.selected_tactic = -1; s.selected_ids.clear(); }
  nav_ = path::NavGrid();
  if (w.passability.w == w.terrain.w && w.passability.h == w.terrain.h && !w.passability.v.empty())
    nav_.load_dat(w.passability.w, w.passability.h, w.passability.v.data());
  else
    nav_.reset(w.terrain.w, w.terrain.h);
  mover_ = std::make_unique<path::MoveSystem>(nav_);
  mover_->set_unit_unstick(modifications_.unit_unstick);
  if (w.side_state.size() < 8) w.side_state.resize(8);
  paused_ = false;
  spawn_counter_ = 0;
  std::fill(std::begin(income_total_), std::end(income_total_), 0);
  visual_rng_ = 12345;
  if (script_.loaded() && !w.original_save) script_.init(w);
  for (Object& o : w.objects) {
    if (o.type < 0 || o.type >= kNumTypes) { o.type = -1; o.alive = true; continue; }   // unknown types become inert markers
    const TypeRules& r = rules_->type(o.type);
    o.initialize_health(r, bool(w.original_save));
    if (!w.original_save) {
      o.progress = 1000;
      o.gold_harvest = {}; o.repair_harvest = {}; o.harvest_ready = false;
      o.body_angle = o.facing * 32; o.turret_angle = o.body_angle;
    }
    if (o.removed || !o.alive) continue;
    attach_agent(o);
    if (o.mover < 0) mover_->block_formation_tile({px(o.cur_x) / kTilePx, px(o.cur_y) / kTilePx});
    if (r.structure) {
      block_footprint(r, px(o.cur_x) / kTilePx, px(o.cur_y) / kTilePx, +1);
    }
  }
  if (!w.original_save) {
    // Placement ids include submarine controllers and each vessel's passengers.
    // Keep the source group, including teams without a script ScenarioID.
    struct StartupGroup { path::Cell anchor; std::vector<uint32_t> agents; };
    std::map<std::pair<int, int>, StartupGroup> groups;
    uint32_t id = 0;
    for (const PlacedObject& p : w.mission.objects) {
      const auto placed_id = id + 1;
      id += uint32_t(1 + p.passengers.size());
      Object* o = w.find_mut(placed_id);
      if (!o || o->type != p.type || o->side != p.side ||
          !o->alive || o->removed || p.defence ||
          (p.source != ObjectSource::Team && p.source != ObjectSource::Town)) continue;
      auto key = std::make_pair(int(p.source), p.group);
      auto [it, inserted] = groups.try_emplace(key, StartupGroup{{p.x, p.y}, {}});
      if (inserted && p.source == ObjectSource::Town) {
        for (const auto& town : w.mission.towns)
          if (town.index == p.group) it->second.anchor = {town.x, town.y};
      }
      if (o->mover >= 0) it->second.agents.push_back(uint32_t(o->mover));
    }
    for (auto& [key, group] : groups) {
      (void)key;
      mover_->order_formation(group.agents.data(), int(group.agents.size()), group.anchor);
    }
  }
}

int GameSim::town_index(const World& w, int side) const {
  for (size_t i = 0; i < w.mission.towns.size(); i++)
    if (w.mission.towns[i].side == side) return int(i);
  return -1;
}

int GameSim::producing_town(const World& w, int side, const std::vector<uint32_t>& ids) const {
  if (ids.empty()) return town_index(w, side);
  const Object* o = w.find(ids.front());
  if (!o || !o->alive || o->removed || o->side != side) return -1;
  for (size_t i = 0; i < w.mission.towns.size(); ++i)
    if (w.mission.towns[i].side == side && w.mission.towns[i].index == o->town) return int(i);
  return -1;
}

const Object* GameSim::town_hall(const World& w, int town) const {
  if (town < 0 || town >= int(w.mission.towns.size())) return nullptr;
  const TownDef& t = w.mission.towns[size_t(town)];
  for (const Object& o : w.objects)
    if (o.alive && !o.removed && o.side == t.side && o.town == t.index &&
        (o.town_hall || o.type == rules_->town_building(t.type, t.race))) return &o;
  return nullptr;
}

int GameSim::upgrade_level(int race, int type) const {
  for (int level = 1; level <= 3; ++level)
    if (rules_->town_building(level, race) == type) return level;
  return -1;
}

bool GameSim::automatic_site(const World& w, int side, int type, int tx, int ty, int town) const {
  if (!can_place(w, side, type, tx, ty, nullptr, nullptr, town)) return false;
  const auto& candidate = rules_->type(type);
  int x0, y0, x1, y1; footprint(candidate, tx, ty, x0, y0, x1, y1);
  // Automatic towns need walkable gaps between structures. Exact mask overlap
  // alone can pack irregular buildings into a ring that seals the producer.
  // This is an engine planner policy, independent of manual placement rules.
  for (const auto& object : w.objects) {
    if (!object.alive || object.removed || !rules_->type(object.type).structure) continue;
    const auto& other = rules_->type(object.type);
    const int ox = px(object.cur_x) / kTilePx, oy = px(object.cur_y) / kTilePx;
    int ox0, oy0, ox1, oy1; footprint(other, ox, oy, ox0, oy0, ox1, oy1);
    if (x1 + 1 < ox0 || ox1 + 1 < x0 || y1 + 1 < oy0 || oy1 + 1 < y0) continue;
    for (int y = y0; y <= y1; ++y) for (int x = x0; x <= x1; ++x) {
      if (!occupies(candidate, tx, ty, x, y)) continue;
      for (int dy = -1; dy <= 1; ++dy) for (int dx = -1; dx <= 1; ++dx)
        if (occupies(other, ox, oy, x + dx, y + dy)) return false;
    }
  }
  return true;
}

// Automatic placement is an engine policy pending reference placement evidence.
// Search nearest square rings, with stable row/column tie breaking.
bool GameSim::finish_structure(World& w, int side, const QueueItem& item) {
  const Object* hall = town_hall(w, item.town);
  if (!hall || hall->side != side || hall->progress < 1000) return false;
  TownDef& town = w.mission.towns[size_t(item.town)];
  const TypeRules& r = rules_->type(item.type);
  int tx = px(hall->cur_x) / kTilePx, ty = px(hall->cur_y) / kTilePx;
  int level = upgrade_level(town.race, item.type);
  if (level >= 0) {
    Object* o = w.find_mut(hall->id);
    const TypeRules& old = rules_->type(o->type);
    block_footprint(old, tx, ty, -1);
    if (!can_place(w, side, item.type, tx, ty, nullptr, nullptr, item.town)) {
      block_footprint(old, tx, ty, +1);
      return false;
    }
    o->type = item.type; o->town_hall = true;
    o->hp_max = r.hp_max; o->hp = r.hp_max; o->secondary_hp = r.secondary_hp;
    o->harvest_ready = false; o->gold_harvest = {}; o->repair_harvest = {};
    o->anim_state = kAnimIdle; o->anim_tick = 0; o->body_tick = -1;
    block_footprint(r, tx, ty, +1);
    town.type = level;
    return true;
  }
  auto place = [&](int x, int y) {
    if (!automatic_site(w, side, item.type, x, y, item.town)) return false;
    uint32_t id = spawn(w, item.type, side, x * kTilePx + kTilePx / 2, y * kTilePx + kTilePx / 2, 0, false);
    w.find_mut(id)->town = town.index;
    w.find_mut(id)->tactic = town.scenario_id;
    return true;
  };
  int limit = std::max(w.terrain.w, w.terrain.h);
  for (int radius = 0; radius < limit; ++radius)
    for (int y = std::max(0, ty - radius); y <= std::min(w.terrain.h - 1, ty + radius); ++y) {
      if (y == ty - radius || y == ty + radius) {
        for (int x = std::max(0, tx - radius); x <= std::min(w.terrain.w - 1, tx + radius); ++x)
          if (place(x, y)) return true;
      } else {
        if (tx - radius >= 0 && place(tx - radius, y)) return true;
        if (tx + radius < w.terrain.w && place(tx + radius, y)) return true;
      }
    }
  return false;
}

uint32_t GameSim::spawn(World& w, int type, int side, int pxx, int pyy, int hp, bool constructing) {
  Object o;
  o.id = uint32_t(w.objects.size() + 1);
  o.type = type;
  o.side = side;
  o.source = ObjectSource::Team;
  o.facing = 6;
  o.body_angle = o.turret_angle = o.facing * 32;
  const TypeRules& r = rules_->type(type);
  o.hp = hp;
  o.initialize_health(r);
  o.anim_variant = next_visual_rand();
  o.teleport(to_fixed(pxx), to_fixed(pyy));
  if (constructing) {
    o.progress = 0; o.build_elapsed = 0; o.build_total = std::max(1, r.time);
    o.hp = 1;
    o.anim_state = kAnimConstruct;
    o.construction_overlay = r.kind == 7;
  }
  w.objects.push_back(o);
  Object& ref = w.objects.back();
  attach_agent(ref);
  if (r.structure) {
    block_footprint(r, pxx / kTilePx, pyy / kTilePx, +1);
  }
  return ref.id;
}

void GameSim::kill(World& w, Object& o) {
  if (!o.alive) return;
  const auto passengers = o.cargo;
  o.cargo.clear();
  for (auto id : passengers) if (auto* passenger = w.find_mut(id)) {
    passenger->carrier = 0; kill(w, *passenger); passenger->removed = true;
  }
  o.alive = false;
  o.hp = 0;
  o.order = uint8_t(Order::Idle);
  o.target = 0;
  o.corpse_ticks = 0;
  const TypeRules& r = rules_->type(o.type);
  if (r.kind == 1 || r.kind == 3 || r.kind == 4 || r.kind == 5 || r.kind == 6 || r.kind == 9) {
    decoration(w, data::kVehicleExplosionAnim, int(next_visual_rand() % 3), o.cur_x, o.cur_y, o.render_height, o.side);
    if (!r.flier) decoration(w, data::kVehicleDebrisAnim, int(next_visual_rand() % 20), o.cur_x, o.cur_y, -1, o.side);
    o.removed = true;
  }
  if (r.structure) {
    // Known-radius rubble is drawn by the object provider; retain the provisional
    // small-rubble effect only when its radius is unavailable.
    if (o.destruction_radius < 0 && o.progress >= 1000)
      decoration(w, 116, int(next_visual_rand() % 20), o.cur_x, o.cur_y, -1, o.side);
    block_footprint(r, px(o.cur_x) / kTilePx, px(o.cur_y) / kTilePx, -1);
    // Keep known rubble visible for its resource lifetime; unknown radii and
    // construction cancellations remain explicit missing visual inputs.
    if (o.destruction_radius < 0 || o.progress < 1000) { o.removed = true; return; }
  }
  o.anim_state = kAnimDie;
  o.anim_tick = 0;
  if (o.mover >= 0) {
    mover_->stop(uint32_t(o.mover));
    path::Agent& a = mover_->agents_mut()[size_t(o.mover)];
    if (is_transport(o.type)) a.enabled = false;
    a.radius = 0;   // corpses do not block
    a.hold = true;
  }
}

const Object* GameSim::nearest_enemy(const World& w, const Object& o) const {
  const bool mask_sight = rules_->sight() != nullptr;
  if (mask_sight && (o.side < 0 || o.side >= int(w.visibility.sides.size()))) return nullptr;
  const Object* best = nullptr;
  // Preserve asset-free simulation combat when the user's lighting inputs are
  // unavailable. This six-tile fallback remains a placeholder, not a sight fact.
  constexpr int64_t fallback_radius = 6 * kTilePx;
  int64_t bd = mask_sight ? std::numeric_limits<int64_t>::max() : fallback_radius * fallback_radius;
  for (const Object& t : w.objects) {
    if (t.removed || !t.alive || t.type < 0 || !w.hostile(o.side, t.side) || t.progress < 1000) continue;
    if (mask_sight && !position_visible(w, o.side, px(t.cur_x), px(t.cur_y))) continue;
    const auto shot = rules_->attack(o.type, t.type, !w.hostile(o.side, t.side));
    if (!shot.damage_known || !shot.getters_known || shot.damage <= 0) continue;
    int64_t d = d2(o, t);
    // An idle unit holds its ground: it engages what its weapon reaches plus a short leash. The original keeps
    // idle units within one or two tiles of their tactic anchor (live mission 1 to 3 measurement); chasing any
    // enemy the side can see made the clone's own units walk to the enemy base and start the scripted counterattack.
    const bool idle_hold = o.order == uint8_t(Order::Idle) && o.attack_mode != AttackMode::Aggressive;
    if (o.attack_mode == AttackMode::Direct || idle_hold) {
      const auto& tr = rules_->type(t.type);
      int64_t reach = weapon_reach(rules_->type(o.type), tr, shot.range_px);
      if (idle_hold && o.attack_mode != AttackMode::Direct) reach += 2 * kTilePx;  // Balanced and Defensive hold their ground
      if (d > reach * reach) continue;
    }
    if (d < bd || (d == bd && best && t.id < best->id)) { bd = d; best = &t; }
  }
  return best;
}

// ---- commands ----------------------------------------------------------------------------------------------------

void GameSim::apply(World& w, const Command& c) {
  SideState* ss = c.side < w.side_state.size() ? &w.side_state[c.side] : nullptr;
  const auto& command_ids = c.ids.empty() && ss ? ss->selected_ids : c.ids;
  auto own_ids = [&](bool need_mobile) {
    std::vector<Object*> v;
    for (uint32_t id : command_ids) {
      Object* o = w.find_mut(id);
      if (!o || o->removed || !o->alive || o->side != c.side || o->type < 0 || (is_transport(o->type) && !o->vessel_enabled)) continue;
      const TypeRules& r = rules_->type(o->type);
      if (need_mobile && !(r.mobile || r.flier)) continue;
      if (std::find(v.begin(), v.end(), o) == v.end()) v.push_back(o);
    }
    return v;
  };
  auto select = [&](std::vector<uint32_t> ids) {
    ss->selected_ids = std::move(ids);
    ss->selected_tactic = -1;
    if (!ss->selected_ids.empty()) {
      int tactic = w.find(ss->selected_ids.front())->tactic;
      if (tactic >= 0 && std::all_of(ss->selected_ids.begin(), ss->selected_ids.end(),
          [&](uint32_t id) { return w.find(id)->tactic == tactic; })) ss->selected_tactic = tactic;
    }
    if (w.local_side == c.side) w.selection.ids = ss->selected_ids;
  };
  switch (c.type) {
    case Pct::PauseOn: paused_ = true; break;
    case Pct::PauseOff: paused_ = false; break;
    case Pct::Board: {
      Object* vessel = w.find_mut(uint32_t(c.a));
      if (!vessel || !vessel->alive || vessel->removed || !is_transport(vessel->type) ||
          vessel->side != c.side || !vessel->vessel_enabled) break;
      for (auto* o : own_ids(true)) {
        const auto& r = rules_->type(o->type);
        if (r.flier || is_transport(o->type) || o->carrier) continue;
        script_stop(w, {o->id}); o->boarding = vessel->id;
      }
      break;
    }
    case Pct::Unload:
      for (auto* o : own_ids(true)) if (is_transport(o->type)) {
        script_stop(w, {o->id}); o->vessel_route = false;
        unload(w, *o, c.c == 1 ? px(o->cur_x) / kTilePx : std::clamp(c.a, 0, w.width_px()-1) / kTilePx,
                        c.c == 1 ? px(o->cur_y) / kTilePx : std::clamp(c.b, 0, w.height_px()-1) / kTilePx);
      }
      break;
    case Pct::Move:
    case Pct::AttackMove: {
      std::vector<Object*> v = own_ids(true);
      std::vector<uint32_t> agents;
      // Coordinates come from untrusted command packets: clamp into the map before any fixed-point conversion.
      const int cx = std::clamp(c.a, 0, std::max(0, w.width_px() - 1)), cy = std::clamp(c.b, 0, std::max(0, w.height_px() - 1));
      for (Object* o : v) {
        if (is_transport(o->type) && !nav_.blocked(cx / kTilePx, cy / kTilePx, path::MoveClass::Small)) {
          script_stop(w, {o->id}); o->vessel_route = false;
          unload(w, *o, cx / kTilePx, cy / kTilePx); continue;
        }
        if (is_transport(o->type)) script_stop(w, {o->id});
        o->boarding = 0; o->vessel_route = false;
        o->order = uint8_t(c.type == Pct::Move ? Order::Move : Order::AttackMove);
        o->dest_x = cx; o->dest_y = cy; o->target = 0; o->resume = 0;
        o->follow_tactic = -1;
        if (o->mover >= 0) agents.push_back(uint32_t(o->mover));
      }
      if (!agents.empty()) mover_->order_move(agents.data(), int(agents.size()), {to_fixed(cx), to_fixed(cy)});
      break;
    }
    case Pct::Attack: {
      const Object* t = w.find(uint32_t(c.a));
      if (!t || t->removed || !t->alive ||
          (w.visibility_brightness && !object_visible(w, c.side, *t))) break;
      for (Object* o : own_ids(true)) {
        if (o->id == t->id || !rules_->type(o->type).armed) continue;
        o->boarding = 0;
        o->order = uint8_t(Order::Attack); o->target = t->id; o->resume = 0;
        o->follow_tactic = -1;
        if (o->attack_mode == AttackMode::Direct && o->mover >= 0) mover_->stop(uint32_t(o->mover));
      }
      break;
    }
    case Pct::AttackMode: {
      if (!ss || c.a < 0 || c.a > 3) break;
      const auto members = own_ids(true);
      std::vector<int> tactics;
      for (const Object* o : members) if (o->tactic >= 0) tactics.push_back(o->tactic);
      for (Object& o : w.objects) {
        if (o.removed || !o.alive || o.side != c.side || o.type < 0) continue;
        const auto& r = rules_->type(o.type);
        if (!(r.mobile || r.flier)) continue;
        const bool selected = std::find(command_ids.begin(), command_ids.end(), o.id) != command_ids.end();
        const bool attached = o.tactic >= 0 && std::find(tactics.begin(), tactics.end(), o.tactic) != tactics.end();
        if (!selected && !attached) continue;
        o.attack_mode = AttackMode(c.a);
        // Clear combat assignment on a mode change; preserve the movement destination.
        if (o.order == uint8_t(Order::Attack)) {
          o.order = uint8_t(resumed_order(o.resume));
          o.target = 0; o.resume = 0;
          if (o.mover >= 0) {
            uint32_t aid = uint32_t(o.mover);
            if (o.order == uint8_t(Order::Idle)) mover_->stop(aid);
            else mover_->order_move(&aid, 1, {to_fixed(o.dest_x), to_fixed(o.dest_y)});
          }
        }
      }
      break;
    }
    case Pct::Stop:
      for (Object* o : own_ids(true)) script_stop(w, {o->id});
      break;
    case Pct::Garrison:
    case Pct::RepairMode:
    case Pct::DestroyHere: {
      if (!ss) break;
      auto ti = std::find_if(w.town_state.begin(), w.town_state.end(), [&](const TownState& t) {
        return t.index == c.b && t.side == c.side;
      });
      if (ti == w.town_state.end()) break;
      TownState& town = *ti;
      const Object* hall = w.find(town.hall);
      if (!hall || !hall->alive || hall->removed || hall->side != c.side) break;
      if (c.type == Pct::RepairMode) {
        if (c.a < 0 || c.a > 2) break;
        const auto mode = RepairMode(c.a);
        if (town.repair_mode != mode) { town.marked.clear(); town.repair_mode = mode; }
      } else if (c.type == Pct::DestroyHere) {
        if (town.repair_mode == RepairMode::Off) break;
        // A duplicated id in one packet is one toggle, just like a selection set.
        std::vector<uint32_t> seen;
        for (Object* o : own_ids(false)) {
          if (std::find(seen.begin(), seen.end(), o->id) != seen.end()) continue;
          seen.push_back(o->id);
          auto mark = std::find(town.marked.begin(), town.marked.end(), o->id);
          if (mark != town.marked.end()) town.marked.erase(mark);
          else if (mark_eligible(*o, town)) town.marked.push_back(o->id);
        }
      } else {
        // Allocate independently of object iteration order and avoid existing script tactic ids.
        int32_t tactic = 0;
        for (const Object& o : w.objects) {
          if (o.tactic == std::numeric_limits<int32_t>::max()) { tactic = -1; break; }
          tactic = std::max(tactic, o.tactic + 1);
        }
        if (tactic < 0) break;
        std::vector<uint32_t> extracted;
        for (Object& o : w.objects) {
          const TypeRules& r = rules_->type(o.type);
          if (o.town != town.index || o.side != c.side || !o.alive || o.removed || o.hp <= 0 ||
              r.structure || !(r.mobile || r.flier)) continue;
          o.town = -1; o.tactic = tactic;
          extracted.push_back(o.id);
        }
        if (!extracted.empty()) {
          ss->selected_tactic = tactic;
          ss->selected_ids = extracted;
          if (w.local_side == c.side) w.selection.ids = std::move(extracted);
        }
      }
      break;
    }
    case Pct::SelectGroup: {
      if (!ss || c.a < 0 || c.a >= 10 || c.b != 0) break;
      std::vector<uint32_t> ids;
      for (uint32_t id : ss->groups[c.a]) {
        const Object* o = w.find(id);
        if (o && o->alive && !o->removed && o->type >= 0 && o->side == c.side) ids.push_back(id);
      }
      select(std::move(ids));
      break;
    }
    case Pct::SelectByNo: {
      if (!ss || c.a < 0 || c.b != 0) break;
      std::vector<uint32_t> ids;
      for (const Object& o : w.objects)
        if (o.alive && !o.removed && o.type >= 0 && o.side == c.side && o.tactic == c.a) ids.push_back(o.id);
      if (!ids.empty()) select(std::move(ids));
      break;
    }
    case Pct::FollowTeam: {
      if (!ss || c.a < 0 || c.b != 0) break;
      auto mobile = [&](const Object& o) {
        return o.alive && !o.removed && o.type >= 0 && o.side == c.side &&
            (rules_->type(o.type).mobile || rules_->type(o.type).flier);
      };
      if (std::none_of(w.objects.begin(), w.objects.end(), [&](const Object& o) {
            return mobile(o) && o.tactic == c.a;
          })) break;
      std::vector<int> sources;
      for (const Object* o : own_ids(true))
        if (o->tactic >= 0 && std::find(sources.begin(), sources.end(), o->tactic) == sources.end()) sources.push_back(o->tactic);
      // Reject the entire order if any leader path reaches a source tactic.
      std::vector<int> reachable{c.a};
      for (size_t i = 0; i < reachable.size(); ++i)
        for (const Object& o : w.objects)
          if (mobile(o) && o.tactic == reachable[i] && o.follow_tactic >= 0 &&
              std::find(reachable.begin(), reachable.end(), o.follow_tactic) == reachable.end()) reachable.push_back(o.follow_tactic);
      if (std::any_of(sources.begin(), sources.end(), [&](int t) {
            return std::find(reachable.begin(), reachable.end(), t) != reachable.end();
          })) break;
      for (Object& o : w.objects) {
        if (!mobile(o) || std::find(sources.begin(), sources.end(), o.tactic) == sources.end()) continue;
        o.follow_tactic = c.a; o.order = uint8_t(Order::Follow); o.target = 0; o.resume = 0;
        o.dest_x = o.dest_y = -1;
        if (o.mover >= 0) mover_->stop(uint32_t(o.mover));
      }
      break;
    }
    case Pct::CreateGroup:
    case Pct::AddToGroup:
      if (ss && c.a >= 0 && c.a < 10) {
        std::vector<uint32_t>& g = ss->groups[c.a];
        if (c.type == Pct::CreateGroup) g.clear();
        for (Object* o : own_ids(false))
          if (std::find(g.begin(), g.end(), o->id) == g.end()) g.push_back(o->id);
      }
      break;
    case Pct::SetModifications:
      if (c.a >= 0 && Modifications::valid(uint32_t(c.a))) {
        modifications_ = Modifications::from_bits(uint32_t(c.a));
        mover_->set_unit_unstick(modifications_.unit_unstick);
      }
      break;
    case Pct::PauseBuild: {
      if (!ss || c.a < 0 || c.a > 2 || c.b < 0) break;
      int town = producing_town(w, c.side, c.ids);
      for (size_t i = 0; i < ss->queue.size(); ++i) {
        auto& q = ss->queue[i];
        if (c.b > 0 && size_t(c.b - 1) != i) continue;
        if (q.town != town || !q.started) continue;
        if (c.a == 2) cancel_production(w, *ss, i);
        else q.paused = c.a == 1;
        break;
      }
      break;
    }
    case Pct::BuildMode: {
      int ti = producing_town(w, c.side, c.ids);
      if (ti >= 0 && ti < int(w.town_speed.size()) && c.a >= 0 && c.a <= 2) w.town_speed[ti] = c.a;
      break;
    }
    case Pct::QueueKill:
      if (ss && c.a >= 0 && c.a < int(ss->queue.size())) {
        cancel_production(w, *ss, size_t(c.a));
      }
      break;
    case Pct::PlaceBuilding:
    case Pct::Build: {
      if (!ss) break;
      const char* why = "";
      int ti = producing_town(w, c.side, c.ids);
      if (ti < 0) break;
      const std::vector<MenuItem> items = menu(w, c.side, ti);
      const MenuItem* mi = nullptr;
      for (const MenuItem& m : items) if (m.type == c.a) mi = &m;
      if (!mi || !mi->enabled) break;
      if (std::count_if(ss->queue.begin(), ss->queue.end(),
                        [ti](const QueueItem& q) { return q.town == ti && !q.started; }) >= kProductionQueueCapacity) break;
      const bool place = mi->construct || c.type == Pct::PlaceBuilding;
      if (c.type == Pct::PlaceBuilding && !mi->place) break;
      if (place && (c.b < 0 || c.c < 0 || c.b >= w.terrain.w || c.c >= w.terrain.h ||
                            !can_place(w, c.side, c.a, c.b, c.c, &why, nullptr, ti))) break;
      QueueItem q{c.a, std::max(1, mi->time), std::max(1, mi->time), ti, 0};
      if (place) {
        q.tile_x = c.b; q.tile_y = c.c;
        q.object = spawn(w, q.type, c.side, c.b * kTilePx + kTilePx / 2,
                         c.c * kTilePx + kTilePx / 2, 0, true);
        auto* object = w.find_mut(q.object);
        object->town = w.mission.towns[size_t(ti)].index;
        object->tactic = w.mission.towns[size_t(ti)].scenario_id;
        object->build_total = q.total;
      }
      ss->queue.push_back(q);
      break;
    }
    default: break;
  }
}

bool GameSim::mark_eligible(const Object& o, const TownState& town) const {
  if (o.removed || !o.alive || o.hp <= 0 || o.side != town.side || o.town != town.index ||
      o.town_hall || o.id == town.hall || o.progress < 1000 || !rules_->type(o.type).structure) return false;
  return town.repair_mode == RepairMode::Destroy || o.hp < o.hp_max;
}

void GameSim::maintain_town_marks(World& w) {
  for (TownState& town : w.town_state) {
    const Object* hall = w.find(town.hall);
    if (!hall || !hall->alive || hall->removed || hall->side != town.side) { town.marked.clear(); continue; }
    town.marked.erase(std::remove_if(town.marked.begin(), town.marked.end(), [&](uint32_t id) {
      const Object* o = w.find(id);
      return !o || !mark_eligible(*o, town);
    }), town.marked.end());
    if (town.repair_mode != RepairMode::Off) continue;
    for (const Object& o : w.objects)
      if (rules_->type(o.type).kind == 2 && mark_eligible(o, town) &&
          std::find(town.marked.begin(), town.marked.end(), o.id) == town.marked.end()) town.marked.push_back(o.id);
  }
}

// Credits are a per-tick work budget, independent of side cash. Groups are
// snapshotted before service so healing never changes priority within this tick.
void GameSim::service_towns(World& w) {
  for (size_t ti = 0; ti < w.town_state.size(); ++ti) {
    TownState& town = w.town_state[ti];
    int64_t budget = w.repair_credits[ti];
    w.repair_credits[ti] = 0;
    if (budget <= 0) continue;
    std::array<std::vector<uint32_t>, 4> groups;
    for (uint32_t id : town.marked) {
      const Object* o = w.find(id);
      if (!o || !mark_eligible(*o, town)) continue;
      int group = rules_->type(o->type).kind == 7 ? 3 :
                  o->hp_max / o->hp > 3 ? 0 : o->hp_max / o->hp > 2 ? 1 : 2;
      groups[size_t(group)].push_back(id);
    }
    const bool destroy = town.repair_mode == RepairMode::Destroy;
    const std::array<int, 4> order = destroy ? std::array<int, 4>{0, 1, 2, 3} : std::array<int, 4>{3, 0, 1, 2};
    for (int group : order) {
      const auto& ids = groups[size_t(group)];
      if (ids.empty() || budget == 0) continue;
      if (destroy) budget *= 3;
      const int64_t share = budget / int64_t(ids.size());
      for (uint32_t id : ids) {
        if (budget == 0) break;
        Object* o = w.find_mut(id);
        int64_t work = std::min({share, budget, int64_t(destroy ? o->hp : o->hp_max - o->hp)});
        if (work == 0) {
          town.marked.erase(std::remove(town.marked.begin(), town.marked.end(), id), town.marked.end());
          continue;
        }
        if (destroy) {
          // Weapon 10 supplies attribution; the service passes explicit damage,
          // rather than calculating another descriptor weapon hit.
          if (!rules_->type(o->type).service_damage_protected) {
            o->hp = int(std::max<int64_t>(std::numeric_limits<int>::min(), int64_t(o->hp) - work * 3));
            o->last_hit_by = ids.front();
            if (o->hp <= 0) kill(w, *o);
          }
        } else o->hp += int(work);
        budget -= work;
      }
      if (destroy) budget /= 3;
    }
  }
}

// ---- economy -----------------------------------------------------------------------------------------------------

// The first town-local structure prerequisite is the producer; side-wide
// prerequisites unlock technology. Units without one use the town hall.
int GameSim::training_producer(const World& w, int town, int type) const {
  const auto& td = w.mission.towns[size_t(town)];
  for (const auto& recipe : rules_->build_list(td.race, td.type)) if (recipe.type == type)
    for (const auto& need : recipe.prereq)
      if (!need.side_wide && rules_->type(need.type).structure) return need.type;
  return rules_->town_building(td.type, td.race);
}

int GameSim::training_capacity(const World& w, int town, int producer) const {
  if (modifications_.parallel_training != 0) return modifications_.parallel_training;
  const auto& td = w.mission.towns[size_t(town)];
  return int(std::count_if(w.objects.begin(), w.objects.end(), [&](const auto& o) {
    return o.alive && !o.removed && o.side == td.side && o.town == td.index &&
        o.type == producer && o.progress >= 1000;
  }));
}

bool GameSim::menu_ok(const World& w, int side, const BuildItem& it, int town, const char** why, const QueueItem* reservation) const {
  const SideDef* sd = nullptr;
  for (const SideDef& s : w.mission.sides) if (s.index == side) sd = &s;
  if (sd && std::find(sd->disabled.begin(), sd->disabled.end(), it.type) != sd->disabled.end()) { *why = "disabled"; return false; }
  for (const Prerequisite& need : it.prereq) {
    bool have = false;
    for (const Object& o : w.objects)
      if (!o.removed && o.alive && o.side == side && o.type == need.type && o.progress >= 1000 && (need.side_wide || o.town == w.mission.towns[size_t(town)].index)) { have = true; break; }
    if (!have) { *why = "needs building"; return false; }
  }
  const TownDef& td = w.mission.towns[size_t(town)];
  const int level = upgrade_level(td.race, it.type);
  if (rules_->type(it.type).structure && !it.construct) {
    const Object* hall = town_hall(w, town);
    if (!hall || hall->progress < 1000) { *why = "needs town hall"; return false; }
    if (level >= 0) {
      if (level <= td.type) { *why = "already developed"; return false; }
      for (const auto& q : w.side_state[side].queue)
        if (&q != reservation && q.town == town && upgrade_level(td.race, q.type) >= 0) { *why = "upgrade queued"; return false; }
    }
  }
  // Only map-construction entries reserve a single structure of their type.
  // Queued farms, mines and defences remain repeatable; upgrades have their own guard above.
  bool repeat_producer = false;
  if (modifications_.parallel_training == 0 && it.construct)
    for (const auto& recipe : rules_->build_list(td.race, td.type))
      if (!rules_->type(recipe.type).structure && training_producer(w, town, recipe.type) == it.type)
        repeat_producer = true;
  if (it.construct && !repeat_producer) {
    for (const Object& o : w.objects)
      if ((!reservation || o.id != reservation->object) && !o.removed && o.alive && o.side == side && o.town == td.index && o.type == it.type) {
        *why = "already built"; return false;
      }
    for (const auto& q : w.side_state[side].queue)
      if (&q != reservation && q.town == town && q.type == it.type) { *why = "already queued"; return false; }
  }
  return true;
}

std::vector<MenuItem> GameSim::menu(const World& w, int side, int town) const {
  std::vector<MenuItem> out;
  int ti = town < 0 ? town_index(w, side) : town;
  if (side < 0 || side >= int(w.side_state.size()) || ti < 0 || ti >= int(w.mission.towns.size()) || w.mission.towns[ti].side != side) return out;
  const TownDef& t = w.mission.towns[size_t(ti)];
  for (const BuildItem& it : rules_->build_list(t.race, t.type)) {
    const TypeRules& r = rules_->type(it.type);
    MenuItem m;
    bool computer = false;
    for (const auto& sd : w.mission.sides) if (sd.index == side) computer = sd.only_comp;
    const bool hostile_all = w.enemy_all(side);
    int speed = ti < int(w.town_speed.size()) ? w.town_speed[ti] : 1;
    m.type = it.type; m.construct = it.construct;
    m.place = r.structure && upgrade_level(t.race, it.type) < 0;
    m.price = production_value(r.price, t.type, speed, computer, hostile_all, false);
    const int nominal = std::max(1, production_value(r.time, t.type, speed, computer, hostile_all, true));
    m.work_rate = r.hp_max / nominal;
    m.time = m.work_rate > 0 ? int((int64_t(r.hp_max) + m.work_rate - 1) / m.work_rate) : nominal;
    const char* why = "";
    m.enabled = menu_ok(w, side, it, ti, &why);
    m.why_not = why;
    out.push_back(m);
  }
  return out;
}

std::vector<PlacementCell> GameSim::placement_cells(const World& w, int side, int type, int tx, int ty,
    int town, const QueueItem* reservation) const {
  const auto& r = rules_->type(type);
  if (!r.structure || !nav_.in_bounds(tx, ty)) return {{tx, ty, "outside map"}};
  if (town < 0) town = town_index(w, side);
  const Object* hall = town_hall(w, town);
  const char* global = nullptr;
  if (!hall || hall->side != side || hall->progress < 1000) global = "needs town hall";
  // P: bounded town-local build area until the original distance metric is measured.
  else if (std::max(std::abs(tx - px(hall->cur_x) / kTilePx),
                    std::abs(ty - px(hall->cur_y) / kTilePx)) > 16) global = "too far from town hall";
  const bool mine = type == 13 || type == 48 || type == 66;
  const bool strict = !modifications_.relaxed_placement;
  // P: value 1 is treated as clearance until task 66 measures its exact role.
  // Keep navigation occupancy (value 2) separate from placement requirements.
  auto required = [strict](const TypeRules& rule, int cx, int cy, int x, int y) {
    if (strict && rule.has_footprint_mask) {
      const int dx = x - cx + 3, dy = y - cy + 3;
      if (dx >= 0 && dx < 7 && dy >= 0 && dy < 7 && rule.footprint_mask[dy * 7 + dx] == 1) return true;
    }
    return occupies(rule, cx, cy, x, y);
  };
  int x0, y0, x1, y1; footprint(r, tx, ty, x0, y0, x1, y1);
  if (strict && r.has_footprint_mask) {
    for (int i = 0; i < 49; ++i) if (r.footprint_mask[i] == 1) {
      x0 = std::min(x0, tx + i % 7 - 3); x1 = std::max(x1, tx + i % 7 - 3);
      y0 = std::min(y0, ty + i / 7 - 3); y1 = std::max(y1, ty + i / 7 - 3);
    }
  }
  // Reject pathological fallback descriptors without allocating an unbounded preview.
  if (int64_t(x1) - x0 > 7 || int64_t(y1) - y0 > 7) return {{tx, ty, "invalid footprint"}};
  bool deposit = false;
  std::vector<PlacementCell> cells;
  for (int y = y0; y <= y1; ++y) for (int x = x0; x <= x1; ++x) {
    if (!required(r, tx, ty, x, y)) continue;
    const bool body = occupies(r, tx, ty, x, y);
    const bool resource = w.resources.in(x, y) && (w.resources.at(x, y) >> 8) > 0;
    const bool mineral = mine && body && resource;
    deposit |= mineral;
    const char* reason = nullptr;
    if (!nav_.in_bounds(x, y)) reason = "outside map";
    else {
      // P: mineral-bearing rock may host mines; ordinary structures need clear terrain.
      if (nav_.terrain(x, y) < 2 && (!mineral || (strict && nav_.terrain(x, y) == 1))) reason = "terrain blocked";
      else if ((!reservation || !reservation->object) && nav_.blocked(x, y, path::MoveClass::Large) && nav_.terrain(x, y) >= 2) reason = "occupied";
      // P: conservatively reject map overlays, including objects on open .dat
      // cells. Overlay-specific exceptions still need original-game controls.
      else if (strict && size_t(y) * w.terrain.w + x < w.terrain.cells.size() && w.terrain.overlay(x, y)) reason = "map object";
      else if (strict && resource && !mine) reason = "mineral resource";
      // A known terrain/navigation rejection needs no further collision scans.
      if (reason) { cells.push_back({x, y, reason}); continue; }
      // Query structure masks as well: deposits must never bypass a dynamic blocker.
      for (const auto& object : w.objects) {
        if (!object.alive || object.removed || object.type < 0 || (reservation && object.id == reservation->object)) continue;
        const auto& other = rules_->type(object.type);
        if (other.structure) {
          // In-place upgrades temporarily remove the old hall's navigation footprint.
          if (&object == hall && upgrade_level(w.mission.towns[size_t(town)].race, type) >= 0) continue;
          const int ox = px(object.cur_x) / kTilePx, oy = px(object.cur_y) / kTilePx;
          // Clearance rings may overlap each other, but never a building body.
          if (occupies(other, ox, oy, x, y) || (body && required(other, ox, oy, x, y))) reason = "occupied";
        } else if (!other.flier) {
          const int64_t ox = px(object.cur_x), oy = px(object.cur_y);
          const int64_t cx = std::clamp(ox, int64_t(x) * kTilePx, int64_t(x + 1) * kTilePx);
          const int64_t cy = std::clamp(oy, int64_t(y) * kTilePx, int64_t(y + 1) * kTilePx);
          if ((ox - cx) * (ox - cx) + (oy - cy) * (oy - cy) < int64_t(other.radius_px) * other.radius_px)
            reason = "unit in the way";
        }
        if (reason) break;
      }
      if (!reason) for (const auto& state : w.side_state) for (const auto& q : state.queue) {
        if (&q == reservation || q.tile_x < 0 || q.started) continue;
        const auto& queued = rules_->type(q.type);
        if (occupies(queued, q.tile_x, q.tile_y, x, y) || (body && required(queued, q.tile_x, q.tile_y, x, y))) reason = "reserved";
      }
    }
    cells.push_back({x, y, reason ? reason : global});
  }
  if (mine && !deposit) for (auto& cell : cells) if (!cell.blocked) cell.blocked = "needs mineral deposit";
  return cells;
}

bool GameSim::can_place(const World& w, int side, int type, int tx, int ty, const char** why,
    const QueueItem* reservation, int town) const {
  const auto cells = placement_cells(w, side, type, tx, ty, town, reservation);
  for (const auto& cell : cells) if (cell.blocked) { if (why) *why = cell.blocked; return false; }
  if (why) *why = "";
  return !cells.empty();
}

void GameSim::cancel_production(World& w, SideState& side, size_t index) {
  const auto q = side.queue[index];
  const Object* construction = w.find(q.object);
  if (!q.object || (construction && construction->alive && !construction->removed)) side.money += q.refund();
  if (Object* o = w.find_mut(q.object); o && o->alive && !o->removed) {
    block_footprint(rules_->type(o->type), px(o->cur_x) / kTilePx, px(o->cur_y) / kTilePx, -1);
    o->alive = false; o->removed = true; o->hp = 0;
  }
  side.queue.erase(side.queue.begin() + index);
}

void GameSim::economy(World& w) {
  std::vector<uint32_t> completed;
  for (size_t s = 0; s < w.side_state.size(); s++) {
    SideState& ss = w.side_state[s];
    // Stable insertion order arbitrates cash and capacity. Keep the original
    // serial branch intact when training is off, including same-tick handover.
    std::vector<int> advanced;
    std::map<int, int> trainees;
    std::map<std::pair<int, int>, int> producers;
    for (size_t i = 0; i < ss.queue.size();) {
      QueueItem& q = ss.queue[i];
      if (q.object) {
        const auto* object = w.find(q.object);
        if (!object || !object->alive || object->removed) { ss.queue.erase(ss.queue.begin() + i); continue; }
      }
      const bool construction = modifications_.parallel_construction && rules_->type(q.type).structure &&
          upgrade_level(w.mission.towns[size_t(q.town)].race, q.type) < 0;
      const bool training = modifications_.parallel_training != 1 && !rules_->type(q.type).structure;
      const int producer = training ? training_producer(w, q.town, q.type) : -1;
      const auto group = std::make_pair(q.town, producer);
      if (training) {
        if (std::find(advanced.begin(), advanced.end(), q.town) != advanced.end() ||
            (modifications_.parallel_training == 0 ? producers[group] : trainees[q.town]) >=
                training_capacity(w, q.town, producer)) { ++i; continue; }
        ++trainees[q.town]; ++producers[group];
      } else if (!construction) {
        if (trainees[q.town] > 0 || std::find(advanced.begin(), advanced.end(), q.town) != advanced.end()) {
          // An ordinary building is a FIFO barrier between training batches.
          if (trainees[q.town] > 0) advanced.push_back(q.town);
          ++i; continue;
        }
        advanced.push_back(q.town);
      }
      const Object* hall = town_hall(w, q.town);
      // A missing producer suspends production rather than spawning at the side start.
      if (!hall || hall->side != int(s) || hall->progress < 1000) { ++i; continue; }
      const bool starting = !q.started;
      if (!q.started) {
        const auto& town = w.mission.towns[size_t(q.town)];
        const char* why = "";
        bool available = false;
        for (const auto& recipe : rules_->build_list(town.race, town.type))
          if (recipe.type == q.type) available = menu_ok(w, int(s), recipe, q.town, &why, &q);
        if (!available) { ++i; continue; }
        MenuItem offer;
        for (const auto& m : menu(w, int(s), q.town)) if (m.type == q.type) offer = m;
        if (ss.money < offer.price) { ++i; continue; }
        if (q.tile_x >= 0 && !can_place(w, int(s), q.type, q.tile_x, q.tile_y, nullptr, &q, q.town)) {
          ++i; continue;
        }
        q.started = true;
        q.paid = offer.price;
        // Production advances by an integer fraction of the target's maximum
        // health each tick. Truncation can extend the nominal duration.
        q.work_rate = offer.work_rate;
        q.total = q.remaining = std::max(1, offer.time);
        ss.money -= q.paid;
        if (q.town < int(w.ai_towns.size())) w.ai_towns[q.town].spent += q.paid;
        if (auto* object = w.find_mut(q.object)) object->build_total = q.total;
        if (ai_observer_ && std::any_of(w.mission.sides.begin(), w.mission.sides.end(),
            [s](const auto& side) { return side.index == int(s) && side.only_comp; })) {
          AiEvent event; event.kind = "production"; event.side = int(s); event.town = q.town;
          event.x = q.tile_x; event.y = q.tile_y; event.composition.at(size_t(q.type)) = 1;
          ai_observer_(tick_, event);
        }
      }
      if (q.paused) { ++i; continue; }
      if (q.remaining > 0 && q.work_rate > 0) --q.remaining;
      if (auto* o = w.find_mut(q.object)) {
        // Buildings use their actual health as the work clock. Damage delays
        // completion; it never causes another payment for the accepted item.
        o->hp = int(std::min(int64_t(o->hp_max), int64_t(o->hp) + q.work_rate));
        if (q.work_rate > 0)
          q.remaining = int((int64_t(o->hp_max - o->hp) + q.work_rate - 1) / q.work_rate);
        o->build_elapsed = q.total - q.remaining;
        o->progress = q.remaining == 0 ? 1000 : std::min(999, int(int64_t(o->hp) * 1000 / std::max(1, o->hp_max)));
        if (q.remaining == 0) { o->set_anim(kAnimIdle); o->body_tick = -1; }
      }
      bool finished = false;
      if (q.remaining <= 0 && !starting) {
        if (q.object) { finished = true; completed.push_back(q.object); }
        else if (rules_->type(q.type).structure) finished = finish_structure(w, int(s), q);
        else {
          const auto& unit = rules_->type(q.type);
          const int bx = px(hall->cur_x), by = px(hall->cur_y);
          const int hx = bx / kTilePx, hy = by / kTilePx;
          const auto move_class = unit.large ? path::MoveClass::Large : path::MoveClass::Small;
          auto birth = [&](int x, int y) {
            if (x < 0 || y < 0 || x >= w.terrain.w * kTilePx || y >= w.terrain.h * kTilePx) return false;
            if (unit.mobile && nav_.disc_hits({to_fixed(x), to_fixed(y)}, unit.radius_px * 256, move_class)) return false;
            uint32_t id = spawn(w, q.type, int(s), x, y, 0, false);
            w.find_mut(id)->town = w.mission.towns[size_t(q.town)].index;
            w.find_mut(id)->tactic = w.mission.towns[size_t(q.town)].scenario_id;
            q.object = id;
            ++spawn_counter_; return true;
          };
          const int slot = int(spawn_counter_ % 5);
          finished = birth(bx + (slot - 2) * 26, by + rules_->type(hall->type).foot_h * 24 + 40);
          // The preferred exit can be covered by another construction mask.
          // Search deterministically for a collision-free exit; if none fits,
          // keep the paid item at zero rather than embedding its movement agent.
          for (int radius = 1; radius <= 16 && !finished; ++radius)
            for (int y = hy - radius; y <= hy + radius && !finished; ++y)
              for (int x = hx - radius; x <= hx + radius && !finished; ++x)
                if (std::max(std::abs(x - hx), std::abs(y - hy)) == radius)
                  finished = birth(x * kTilePx + kTilePx / 2, y * kTilePx + kTilePx / 2);
        }
      }
      if (finished) {
        if (q.plan_town >= 0 && q.plan_town < int(w.ai_towns.size())) {
          auto& owner = w.ai_towns[size_t(q.plan_town)];
          if (owner.plan_generation == q.plan_generation && q.plan_entry >= 0 && q.plan_entry < int(owner.plan.size())) {
            auto& entry = owner.plan[size_t(q.plan_entry)];
            if (entry.type == q.type && entry.producer == q.town) entry.object = q.object;
          }
        }
        const int town = q.town;
        ss.queue.erase(ss.queue.begin() + i);
        // Completing the current object frees this town's lane immediately.
        if (training) { --trainees[town]; --producers[group]; }
        else if (!construction)
          advanced.erase(std::remove(advanced.begin(), advanced.end(), town), advanced.end());
      } else ++i;
    }
  }
  std::fill(w.repair_credits.begin(), w.repair_credits.end(), 0);
  // Construction starts its independent harvest clocks only after completion.
  for (Object& o : w.objects) {
    if (o.removed || !o.alive || o.type < 0) continue;
    const TypeRules& r = rules_->type(o.type);
    if (!r.structure || std::find(completed.begin(), completed.end(), o.id) != completed.end()) continue;
    if (o.progress < 1000) {
      bool queued = false;
      for (const auto& state : w.side_state) for (const auto& q : state.queue) queued |= q.object == o.id;
      if (queued) continue;
      const int total = std::max(1, o.build_total);
      const int before = std::max(1, int(int64_t(o.hp_max) * o.build_elapsed / total));
      o.build_elapsed = std::min(total, o.build_elapsed + 1);
      o.progress = int(int64_t(o.build_elapsed) * 1000 / total);
      o.hp = std::min(o.hp_max, o.hp + std::max(1, int(int64_t(o.hp_max) * o.build_elapsed / total)) - before);
      if (o.progress >= 1000) { o.anim_state = kAnimIdle; o.anim_tick = 0; o.body_tick = -1; }
    } else if (o.side >= 0 && o.side < int(w.side_state.size())) {
      if (!o.harvest_ready) {
        // The first active tick follows creation/completion of the accumulator.
        o.gold_harvest.last_clock = o.repair_harvest.last_clock = (w.day_clock + kDayTicks - 1) % kDayTicks;
        o.harvest_ready = true;
      }
      const auto& h = r.harvest;
      int rate = int(((int64_t(h.rate_a) * 65536 / 100) + 128) >> 8);
      if (h.terrain) {
        int64_t sum = 0;
        int count = 0;
        int ca = int(((int64_t(h.rate_a) * 65536 / 100) * 2 + 256) >> 8);
        int cb = int(((int64_t(h.rate_b) * 65536 / 100) * 2 + 256) >> 8);
        for (int cell = 0; cell < 49; ++cell) if (h.occupancy[cell] == 2) {
          int x = px(o.cur_x) / kTilePx + cell % 7 - 3;
          int y = px(o.cur_y) / kTilePx + cell / 7 - 3;
          uint16_t v = w.resources.in(x, y) ? w.resources.at(x, y) : 0;
          sum += 2 * (v & 255) * ca + 2 * (v >> 8) * cb;
          ++count;
        }
        rate = count ? int((sum / count) >> 8) : 0;
      }
      int gain = harvest_tick(o.gold_harvest, rate, w.day_clock, h.period, h.scheduled ? &h.dates : nullptr);
      if (o.town >= 0 && (o.type == 12 || o.type == 39 || o.type == 65)) {
        bool warehouse = false;
        for (const auto& other : w.objects)
          if (!other.removed && other.alive && other.side == o.side && other.town == o.town && other.progress >= 1000 &&
              (other.type == 18 || other.type == 40 || other.type == 70)) warehouse = true;
        // Exact rational form of the specified binary32 multiplier; no host long-double precision.
        if (warehouse) gain = int(std::min(int64_t(std::numeric_limits<int>::max()),
                                          int64_t(gain) * 11184811 / 8388608));
      }
      w.side_state[o.side].money += gain;
      if (o.side < 8) income_total_[o.side] += gain;
      int repair_rate = int(((int64_t(h.repair_rate) * 65536 / 100) + 128) >> 8);
      int credits = harvest_tick(o.repair_harvest, repair_rate, w.day_clock, h.repair_period);
      for (size_t ti = 0; ti < w.mission.towns.size(); ++ti)
        if (w.mission.towns[ti].index == o.town && w.mission.towns[ti].side == o.side) {
          w.repair_credits[ti] += credits * 100;
          w.ai_towns[ti].harvested += gain;
        }
    }
  }
}

// ---- orders, combat ----------------------------------------------------------------------------------------------

void GameSim::follow_tactics(World& w) {
  // Engine policy: track the live mobile leader centroid, replanning at six-tick
  // intervals. Reference formation anchors and attachment flags remain unresolved.
  for (Object& o : w.objects) {
    if (!o.alive || o.removed || o.follow_tactic < 0) continue;
    int64_t x = 0, y = 0, count = 0;
    for (const Object& leader : w.objects) {
      if (!leader.alive || leader.removed || leader.type < 0 || leader.side != o.side ||
          leader.tactic != o.follow_tactic) continue;
      const auto& r = rules_->type(leader.type);
      if (!(r.mobile || r.flier)) continue;
      x += px(leader.cur_x); y += px(leader.cur_y); ++count;
    }
    if (!count || o.tactic == o.follow_tactic) {
      o.follow_tactic = -1;
      if (o.order == uint8_t(Order::Follow) || o.resume == 3) {
        o.order = uint8_t(Order::Idle); o.target = 0; o.resume = 0;
        if (o.mover >= 0) mover_->stop(uint32_t(o.mover));
      }
      continue;
    }
    if (o.order != uint8_t(Order::Follow)) continue;
    int dx = int(x / count), dy = int(y / count);
    if ((tick_ + o.id) % 6 != 0 && o.dest_x >= 0) continue;
    const bool changed = o.dest_x != dx || o.dest_y != dy;
    o.dest_x = dx; o.dest_y = dy;
    if (o.mover >= 0 && changed) {
      const path::Vec2 goal{to_fixed(dx), to_fixed(dy)};
      uint32_t aid = uint32_t(o.mover);
      mover_->order_move(&aid, 1, goal);
    }
  }
}

void GameSim::run_orders(World& w) {
  follow_tactics(w);
  for (size_t i = 0; i < w.objects.size(); i++) {
    Object& o = w.objects[i];
    if (o.removed || o.type < 0) continue;
    if (!o.alive) {
      if (o.regen_service_ticks > 0) --o.regen_service_ticks;
      if (++o.corpse_ticks > (rules_->type(o.type).structure ? 249 : kCorpseTicks)) { o.removed = true; }
      continue;
    }
    const TypeRules& r = rules_->type(o.type);
    if (o.cooldown > 0) o.cooldown--;
    if (o.progress < 1000) {
      if (o.regen_service_ticks > 0) --o.regen_service_ticks;
      continue;
    }
    o.hp = regenerated_hp(r, o.hp, o.regen_service_ticks > 0);
    if (o.regen_service_ticks > 0) --o.regen_service_ticks;
    path::Agent* ag = o.mover >= 0 ? &mover_->agents_mut()[size_t(o.mover)] : nullptr;
    bool moving = ag && ag->state == path::AgentState::Moving;

    if ((o.order == uint8_t(Order::Move) ||
         (r.flier && (o.order == uint8_t(Order::AttackMove) || o.order == uint8_t(Order::Follow)))) && !moving) {
      if (r.flier) {
        path::Vec2 delta{to_fixed(o.dest_x) - o.cur_x, to_fixed(o.dest_y) - o.cur_y};
        int step = std::max(1, r.speed);
        if (path::len2(delta) <= int64_t(step) * step) {
          o.cur_x = to_fixed(o.dest_x); o.cur_y = to_fixed(o.dest_y);
          if (o.order != uint8_t(Order::Follow)) o.order = uint8_t(Order::Idle);
          o.set_anim(kAnimIdle);
        }
        else {
          path::Vec2 v = path::with_length(delta, step);
          o.cur_x += v.x; o.cur_y += v.y;
          int f = facing_from(delta.x, delta.y); if (f >= 0) o.facing = uint8_t(f);
          o.set_anim(kAnimMove);
        }
      } else {
        o.order = uint8_t(Order::Idle);
      }
    }
    if (o.order == uint8_t(Order::AttackMove) && ag && !moving) o.order = uint8_t(Order::Idle);

    if (!r.armed || o.boarding || o.type == 84 || (o.type == 88 && !o.vessel_enabled)) continue;
    // Mode gates are independent of the provisional nearest-visible-enemy policy.
    const bool engage_moving = o.attack_mode == AttackMode::Aggressive || o.attack_mode == AttackMode::Balanced;
    if (o.order == uint8_t(Order::Idle) ||
        (engage_moving && (o.order == uint8_t(Order::AttackMove) || o.order == uint8_t(Order::Move) ||
                           o.order == uint8_t(Order::Follow)))) {
      if ((tick_ + o.id) % 4 == 0) {
        if (const Object* e = nearest_enemy(w, o)) {
          o.resume = o.order == uint8_t(Order::Follow) ? 3 :
              o.order == uint8_t(Order::Move) ? 2 : o.order == uint8_t(Order::AttackMove) ? 1 : 0;
          o.order = uint8_t(Order::Attack);
          o.target = e->id;
        }
      }
    }
    if (o.order != uint8_t(Order::Attack)) continue;
    Object* t = w.find_mut(o.target);
    if (!t || t->removed || !t->alive) {
      o.target = 0;
      if (o.resume) {
        o.order = uint8_t(resumed_order(o.resume)); o.resume = 0;
        if (ag) {
          uint32_t aid = uint32_t(o.mover);
          mover_->order_move(&aid, 1, {to_fixed(o.dest_x), to_fixed(o.dest_y)});
        }
      } else {
        o.order = uint8_t(Order::Idle);
      }
      continue;
    }
    const TypeRules& tr = rules_->type(t->type);
    const auto shot = rules_->attack(o.type, t->type, !w.hostile(o.side, t->side));
    if (!shot.damage_known || !shot.getters_known || shot.damage <= 0) continue;
    const int64_t reach = weapon_reach(r, tr, shot.range_px);
    if (d2(o, *t) <= reach * reach) {
      if (ag && moving) mover_->stop(uint32_t(o.mover));
      int f = facing_from(px(t->cur_x) - px(o.cur_x), px(t->cur_y) - px(o.cur_y));
      if (f >= 0) o.facing = uint8_t(f);
      const int ax = t->cur_x - o.cur_x, ay = t->cur_y - o.cur_y;
      if (ax || ay) {
        o.turret_angle = (256 - path::angle_of(ax, ay)) & 255;
        if (!data::kTypeAnims[o.type].anim2) o.body_angle = o.turret_angle;
      }
      if (o.cooldown == 0) {
        // Descriptor damage/reload feed provisional projectile flight; direct weapons hit immediately.
        const bool siege = (r.kind == 4 || r.kind == 6) && (r.weapon.klass == 0 || r.weapon.klass == 3);
        o.cooldown = std::max(1, shot.reload + (siege ? 0 : 1));
        o.has_fired = true;
        o.anim_state = kAnimAttack; o.anim_tick = 0; o.attack_tick = 0;
        fire(w, o, *t, shot.damage);
      }
    } else if (o.attack_mode != AttackMode::Direct && ag && ((tick_ + o.id) % 6 == 0 || !moving)) {
      uint32_t aid = uint32_t(o.mover);
      path::Vec2 goal{t->cur_x, t->cur_y};
      if (r.weapon.klass == 0 || r.weapon.klass == 3) {
        // Choose a reachable firing tile even when the target's tile is blocked.
        path::FlowField field;
        path::Cell start{px(o.cur_x) / kTilePx, px(o.cur_y) / kTilePx};
        field.build(nav_, ag->cls, &start, 1);
        uint32_t best = path::FlowField::kInf;
        bool found = false;
        const int tx = px(t->cur_x), ty = px(t->cur_y);
        for (int y = std::max(0, int((ty - reach) / kTilePx)); y <= std::min(nav_.height() - 1, int((ty + reach) / kTilePx)); ++y)
          for (int x = std::max(0, int((tx - reach) / kTilePx)); x <= std::min(nav_.width() - 1, int((tx + reach) / kTilePx)); ++x) {
            path::Vec2 candidate{tile_center_fixed(x), tile_center_fixed(y)};
            const int64_t dx = px(candidate.x) - tx, dy = px(candidate.y) - ty;
            if (dx * dx + dy * dy > reach * reach || !field.reachable(x, y) ||
                nav_.disc_hits(candidate, ag->radius, ag->cls)) continue;
            if (field.cost(x, y) < best) { best = field.cost(x, y); goal = candidate; found = true; }
          }
        if (!found) { mover_->stop(aid); continue; }
      }
      mover_->order_move(&aid, 1, goal);
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
  if (weapon.klass == 7) {
    // V: a standalone area hit resolves at the target position plus two pixels
    // vertically. It has no travelling projectile or hostile-only victim gate.
    Effect hit;
    hit.trace_id = ++next_trace_id_; hit.source = source.id; hit.target = target.id; hit.side = source.side;
    hit.kind = data::EffectClass::SplashDamage; hit.payload_id = weapon.payload_id;
    hit.x = target.cur_x; hit.y = target.cur_y + to_fixed(2);
    trace_combat("launch", hit); trace_combat("impact", hit);
    if (weapon.impact_anim) decoration(w, weapon.impact_anim, int(next_visual_rand() % 3), hit.x, hit.y, 0, hit.side);
    apply_blast(w, hit);
    return;
  }
  if (!weapon.components.empty()) {
    const auto offset = path::with_length({target.cur_y - source.cur_y, source.cur_x - target.cur_x},
                                          weapon.spread_px * 256);
    for (size_t i = 0; i < weapon.components.size(); ++i) {
      const auto& child = weapon.components[i];
      auto shot = rules_->weapon(child.id, rules_->type(target.type), !w.hostile(source.side, target.side));
      Object muzzle = source;
      const int sign = i == 0 ? 1 : -1;
      muzzle.cur_x += sign * offset.x; muzzle.cur_y += sign * offset.y;
      if (shot.damage_known) fire_weapon(w, child, muzzle, target, shot.damage);
    }
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
    Effect hit;
    hit.trace_id = ++next_trace_id_; hit.source = source.id; hit.target = target.id; hit.side = source.side;
    hit.x = source.cur_x; hit.y = source.cur_y;
    trace_combat("launch", hit);
    hit.x = target.cur_x; hit.y = target.cur_y;
    trace_combat("impact", hit);
    trace_combat("damage", hit, target.id, target.hp, target.hp - damage);
    target.hp -= damage; target.last_hit_by = source.id;
    if (target.hp <= 0) kill(w, target);
    if (weapon.klass == 10) {
      // V: life drain uses one third of calculated damage, including overkill,
      // then the ordinary capped-healing operation on the attacker.
      if (auto* attacker = w.find_mut(source.id))
        attacker->hp = int(std::min(int64_t(attacker->hp_max), int64_t(attacker->hp) + damage / 3));
    }
    if (weapon.klass == 6) {
      // V: the secondary reaction adds eighteen to the recipient's attack
      // countdown. The spatial query is a 96-pixel square about the source.
      // G: stable engine object order supplies the original's bounded query.
      int candidates = 0;
      for (auto& victim : w.objects) {
        if (!victim.alive || victim.removed || std::abs(px(victim.cur_x) - px(source.cur_x)) > 48 ||
            std::abs(px(victim.cur_y) - px(source.cur_y)) > 48) continue;
        if (++candidates > 128) break;
        const auto& type = rules_->type(victim.type);
        if (victim.side == source.side || (source.side >= 0 && source.side < 8 && victim.side >= 0 &&
            victim.side < 8 && w.relation[source.side][victim.side] == kFriend) || type.flier ||
            !(type.category_e8 || type.category_f4)) continue;
        victim.cooldown = int(std::min(int64_t(std::numeric_limits<int>::max()), int64_t(victim.cooldown) + 18));
      }
    }
    return;
  }
  Effect e;
  e.trace_id = ++next_trace_id_;
  e.kind = weapon.klass == 0 ? data::EffectClass::BallExpl : weapon.klass == 3 ? data::EffectClass::Axe :
           data::EffectClass::Arrow;
  e.source = source.id; e.target = target.id; e.side = source.side;
  e.anim = weapon.anim; e.angle = angle;
  auto sequence = data::projectile_sequence(e.kind, angle);
  e.slot = sequence.slot; e.loop = sequence.loop;
  e.x = e.prev_x = source.cur_x; e.y = e.prev_y = source.cur_y;
  e.end_x = target.cur_x; e.end_y = target.cur_y;
  // Snapshot aim; ball projectiles retain a separate vertical fall.
  e.speed = std::max(1, weapon.speed) * 256;
  e.damage = damage; e.flight = true;
  if (weapon.klass == 0) {
    e.payload_id = weapon.payload_id;
    e.end_y += to_fixed(2);
    const path::Vec2 delta{e.end_x - e.x, e.end_y - e.y};
    const auto velocity = path::with_length(delta, std::max(0, weapon.speed) * 256);
    e.velocity_x = velocity.x; e.velocity_y = velocity.y;
    e.altitude = to_fixed(24); e.height = 24;
    const int duration = weapon.speed > 0 ? path::length(delta) / (weapon.speed * 256) : 0;
    e.gravity = duration > 0 ? int(int64_t(e.altitude) * 2 / (int64_t(duration) * duration)) : 2 * 256;
  }
  e.impact_anim = weapon.impact_anim; e.splash_px = weapon.splash_px; e.shockwave = weapon.shockwave;
  trace_combat("launch", e);
  w.effects.push_back(e);
}

void GameSim::apply_blast(World& w, const Effect& e) {
  for (auto& o : w.objects) {
    if (!o.alive || o.removed) continue;
    const int64_t dx = int64_t(o.cur_x) - e.x, dy = int64_t(o.cur_y) - e.y;
    const int distance = path::length({int32_t(dx), int32_t(dy)}) / 256;
    const int damage = rules_->blast_damage(e.payload_id, rules_->type(o.type), distance);
    if (damage <= 0) { trace_combat("miss", e, o.id, o.hp, o.hp); continue; }
    trace_combat("damage", e, o.id, o.hp, o.hp - damage);
    o.hp -= damage; o.last_hit_by = e.source;
    if (o.hp <= 0) kill(w, o);
  }
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
    const bool ball = e.kind == data::EffectClass::BallExpl;
    path::Vec2 delta{e.end_x - e.x, e.end_y - e.y};
    if (ball) {
      e.x += e.velocity_x; e.y += e.velocity_y;
      e.altitude -= e.vertical_speed; e.vertical_speed += e.gravity;
      e.height = std::max(0, e.altitude / 256); ++e.age;
      trace_combat("position", e);
      if (e.altitude > 0) { w.effects.push_back(e); continue; }
    } else if (path::len2(delta) > int64_t(e.speed) * e.speed) {
      auto step = path::with_length(delta, e.speed);
      e.x += step.x; e.y += step.y; e.age++;
      trace_combat("position", e);
      w.effects.push_back(e);
      continue;
    }
    if (!ball) { e.x = e.end_x; e.y = e.end_y; }
    trace_combat("impact", e);
    if (e.impact_anim) decoration(w, e.impact_anim, int(next_visual_rand() % 3), e.x, e.y, 0, e.side);
    if (e.shockwave == data::kShockwaveAnim) decoration(w, e.shockwave, 0, e.x, e.y, 0, e.side);
    if (e.payload_id >= 0) { apply_blast(w, e); continue; }
    for (auto& o : w.objects) {
      if (!o.alive || o.removed) continue;
      int64_t dx = int64_t(o.cur_x) - e.x, dy = int64_t(o.cur_y) - e.y;
      bool splash = e.splash_px > 0 && w.hostile(e.side, o.side) &&
                    dx * dx + dy * dy <= int64_t(e.splash_px * 256) * (e.splash_px * 256);
      if (o.id != e.target && !splash) {
        trace_combat("miss", e, o.id, o.hp, o.hp);
        continue;
      }
      trace_combat("damage", e, o.id, o.hp, o.hp - e.damage);
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
    if (!o.alive || !a.enabled) { continue; }
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

void GameSim::update_animation_state(World& w) {
  for (Object& o : w.objects) {
    if (o.removed || !o.alive) continue;
    const auto& r = rules_->type(o.type);
    const bool turret = o.type >= 0 && o.type < 90 && data::kTypeAnims[o.type].anim2 != 0;
    if (o.body_angle < 0) o.body_angle = o.facing * 32;
    if (o.turret_angle < 0) o.turret_angle = o.body_angle;
    // Use actual displacement, including the final flight step. This is an
    // engine steering policy; turn rates and Bird cooldown remain unmeasured.
    const int dx = o.cur_x - o.prev_x, dy = o.cur_y - o.prev_y;
    if (dx || dy) o.body_angle = (256 - path::angle_of(dx, dy)) & 255;
    if (r.flier && !(o.anim_state == kAnimAttack && o.anim_tick < 8))
      o.set_anim(o.body_moving ? kAnimMove : kAnimIdle);
    if (o.order == uint8_t(Order::Attack)) {
      const Object* target = w.find(o.target);
      if (target && target->alive && !target->removed) {
        const int tx = target->cur_x - o.cur_x, ty = target->cur_y - o.cur_y;
        if (tx || ty) {
          const int aim = (256 - path::angle_of(tx, ty)) & 255;
          o.turret_angle = aim;
          if (!turret && !o.body_moving) o.body_angle = aim;
        }
      }
    } else if (!turret) o.turret_angle = o.body_angle;
    o.facing = uint8_t(data::direction_from_angle(o.body_angle));
    if (r.kind == 7) o.construction_overlay = o.progress < 1000;
    // V12 supplies phase initiation and travel. Here only established finite
    // resource phases complete; absent resources never imply an instant phase.
    const bool boat = r.kind == 8 || o.source == ObjectSource::Boat;
    const auto* inputs = rules_->sight();
    if (boat && inputs && inputs->sprites.boat_phase_complete(o.transport_phase, o.orientation, o.anim_tick)) {
      if (o.transport_phase == int(data::BoatPhase::Opening)) o.transport_phase = int(data::BoatPhase::Open);
      else {
        if (o.transport_phase == int(data::BoatPhase::Turning)) o.orientation ^= 1;
        o.transport_phase = int(data::BoatPhase::Closed);
      }
      o.anim_tick = 0;
    }
  }
}

// The persistent side grid stores brightness, with an explored ceiling of 32.
void GameSim::update_fog(World& w) {
  const SightData* inputs = rules_->sight();
  if (!inputs || w.terrain.w <= 0 || w.terrain.h <= 0) return;
  int mw = w.terrain.w + 1, mh = w.terrain.h + 2;
  std::vector<uint8_t> glow(size_t(mw) * mh, inputs->daylight[w.mission.tileset != 0][w.day_clock]);
  for (const Object& o : w.objects) {
    if (o.removed || !o.alive) continue;
    const auto st = object_sprite_state(o, w.side_color(o.side));
    int mask = inputs->sprites.light_mask(st);
    if (mask >= 1 && mask <= 16)
      stamp_sight(glow, mw, mh, inputs->mask(mask), px(o.cur_x), px(o.cur_y), false);
  }
  for (const Effect& e : w.effects) {
    auto frame = inputs->effects.anim(e.anim).at_tick(e.slot, e.age, e.loop);
    if (frame.light >= 1 && frame.light <= 16)
      stamp_sight(glow, mw, mh, inputs->mask(frame.light), px(e.x), px(e.y), false);
  }
  if (w.visibility.sides.empty()) w.visibility.sides.resize(w.side_state.size());
  bool changed = false;
  for (size_t side = 0; side < w.visibility.sides.size(); side++) {
    VisFile::Layer& l = w.visibility.sides[side];
    if (l.w != mw || l.h != mh || l.v.size() != size_t(mw) * mh) {
      VisFile::Layer expanded;
      expanded.w = mw; expanded.h = mh; expanded.v.assign(size_t(mw) * mh, 0);
      if (l.w == w.terrain.w && l.h == w.terrain.h && l.v.size() == size_t(l.w) * l.h)
        for (int y = 0; y < l.h; ++y)
          for (int x = 0; x < l.w; ++x)
            expanded.v[size_t(y) * mw + x] = w.visibility_brightness ? l.v[size_t(y) * l.w + x] :
                uint8_t(152 - std::min(152, int(l.v[size_t(y) * l.w + x])));
      l = std::move(expanded); changed = true;
    }
    std::vector<uint8_t> vision(glow.size(), 152);
    for (const Object& o : w.objects) {
      if (o.removed || !o.alive || o.side != int(side)) continue;
      int mask = rules_->type(o.type).vision_mask;
      if (o.source == ObjectSource::Boat || o.source == ObjectSource::Johnka) mask = 71;
      stamp_sight(vision, mw, mh, inputs->mask(mask), px(o.cur_x), px(o.cur_y), true);
    }
    for (size_t i = 0; i < glow.size(); i++) {
      uint8_t nv = uint8_t(std::max(std::min(int(l.v[i]), 32), std::max(0, int(glow[i]) - vision[i])));
      if (nv != l.v[i]) { l.v[i] = nv; changed = true; }
    }
  }
  if (!w.visibility_brightness) changed = true;
  w.visibility_brightness = true;
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
  if (paused_) {
    for (const auto& e : w.effects) if (e.flight) trace_combat("position", e);
    return;
  }
  for (auto& o : w.objects) {
    if (o.removed) continue;
    ++o.anim_tick;
    if (o.attack_tick >= 0) ++o.attack_tick;
  }
  if (script_.loaded() && !w.original_save) script_.tick(*this, w);
  w.day_clock = (w.day_clock + 1) % kDayTicks;
  economy(w);
  run_ai(w);
  advance_effects(w);
  run_orders(w);
  maintain_town_marks(w);
  service_towns(w);
  capture_towns(w);
  // Ownership may change during hypnosis; cooperation follows current sides.
  if (modifications_.unit_unstick) for (const auto& object : w.objects)
    if (object.mover >= 0) mover_->agents_mut()[size_t(object.mover)].team = object.side;
  mover_->tick();
  sync_movement(w);
  transport_tick(w);
  for (auto& o : w.objects) {
    if (o.removed || !o.alive) continue;
    const bool moving = o.cur_x != o.prev_x || o.cur_y != o.prev_y;
    o.body_tick = moving == o.body_moving ? o.body_tick + 1 : 0;
    o.body_moving = moving;
  }
  update_animation_state(w);
  update_fog(w);
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
    o->follow_tactic = -1;
    if (o->mover >= 0) agents.push_back(uint32_t(o->mover));
  }
  if (!agents.empty()) mover_->order_move(agents.data(), int(agents.size()), {to_fixed(pxx), to_fixed(pyy)});
}

void GameSim::script_stop(World& w, const std::vector<uint32_t>& ids) {
  for (uint32_t id : ids) {
    Object* o = w.find_mut(id);
    if (!o || o->removed || !o->alive) continue;
    if (o->vessel_route && o->mover >= 0) {
      auto& agent = mover_->agents_mut()[size_t(o->mover)];
      agent.enabled = true; agent.pos = agent.prev_pos = {o->cur_x, o->cur_y};
    }
    o->vessel_route = false;
    o->boarding = 0;
    o->order = uint8_t(Order::Idle); o->target = 0; o->resume = 0;
    o->follow_tactic = -1;
    if (o->mover >= 0) mover_->stop(uint32_t(o->mover));
  }
}

void GameSim::capture_towns(World& w) {
  // V: the reference checks nearby ground attackers and nearby friendly defenders.
  // G: deterministic nearest attacker, two-tile capture and four-tile defence
  // radii; exact eligibility, scan ordering and expanded alert radius need a live probe.
  for (size_t i = 0; i < w.mission.towns.size(); ++i) {
    const auto* hall = town_hall(w, int(i));
    if (!hall || hall->progress < 1000) continue;
    const int owner = hall->side;
    uint32_t candidate = 0;
    int64_t nearest = int64_t(2 * kTilePx) * (2 * kTilePx) + 1;
    bool defended = false;
    for (const auto& object : w.objects) {
      if (!object.alive || object.removed || object.type < 0) continue;
      const auto& type = rules_->type(object.type);
      if (!type.mobile || type.flier || is_transport(object.type)) continue;
      const auto distance = d2(*hall, object);
      if ((object.side == owner || w.relation[owner][object.side] == kFriend) &&
          distance <= int64_t(4 * kTilePx) * (4 * kTilePx))
        defended = true;
      if (w.hostile(owner, object.side) && distance < nearest) {
        nearest = distance;
        candidate = object.id;
      }
    }
    if (candidate && !defended) transfer_town(w, int(i), w.find(candidate)->side);
  }
}

void GameSim::transfer_town(World& w, int town, int recipient) {
  if (town < 0 || town >= int(w.mission.towns.size()) || recipient < 0 ||
      recipient >= int(w.side_state.size())) return;
  auto& definition = w.mission.towns[size_t(town)];
  const int previous = definition.side;
  if (previous == recipient) return;
  // G: cancel/refund the donor's pending production before transferring the town.
  if (previous >= 0 && previous < int(w.side_state.size())) {
    auto& state = w.side_state[size_t(previous)];
    for (size_t i = state.queue.size(); i > 0; --i)
      if (state.queue[i - 1].town == town) cancel_production(w, state, i - 1);
  }
  std::vector<uint32_t> members;
  for (const auto& object : w.objects)
    if (object.alive && object.side == previous && object.town == definition.index)
      members.push_back(object.id);
  script_stop(w, members);
  for (auto id : members) {
    auto* object = w.find_mut(id);
    object->side = recipient;
    if (object->mover >= 0) mover_->agents_mut()[size_t(object->mover)].team = recipient;
  }
  definition.side = recipient;
  if (size_t(town) < w.town_state.size()) {
    auto& state = w.town_state[size_t(town)];
    state.side = recipient;
    state.marked.clear();
    state.repair_mode = RepairMode::Off;
  }
  if (size_t(town) < w.ai_towns.size()) w.ai_towns[size_t(town)] = {};
}

void GameSim::script_kill(World& w, uint32_t id) {
  Object* o = w.find_mut(id);
  if (o && o->alive) kill(w, *o);
}

void GameSim::teleport(World& w, uint32_t id, int pxx, int pyy) {
  Object* o = w.find_mut(id);
  if (!o) return;
  const auto& r = rules_->type(o->type);
  if (r.structure && o->alive && !o->removed) block_footprint(r, px(o->cur_x) / kTilePx, px(o->cur_y) / kTilePx, -1);
  o->teleport(to_fixed(pxx), to_fixed(pyy));
  if (r.structure && o->alive && !o->removed) block_footprint(r, pxx / kTilePx, pyy / kTilePx, +1);
  if (o->mover >= 0) {
    path::Agent& a = mover_->agents_mut()[size_t(o->mover)];
    a.pos = a.prev_pos = a.win_start = {to_fixed(pxx), to_fixed(pyy)};
  }
}

uint64_t GameSim::state_hash(const World& w) const {
  uint64_t h = 1469598103934665603ull;
  auto mix = [&](uint64_t v) { h = (h ^ v) * 1099511628211ull; };
  // Hash contract 2: fixed-width integer values, ordered containers and explicit sizes.
  // Rules and loaded sight resources must be identical (content compatibility is separate).
  mix(2); mix(initial_modifications_.bits()); mix(modifications_.bits()); mix(w.tick); mix(w.script_side); mix(bool(w.original_save));
  mix(w.terrain.w); mix(w.terrain.h); mix(w.mission.tileset);
  mix(w.resources.w); mix(w.resources.h); mix(w.resources.v.size());
  for (auto value : w.resources.v) mix(value);
  mix(w.mission.sides.size());
  for (const auto& s : w.mission.sides) {
    mix(s.index); mix(s.race); mix(s.start_x); mix(s.start_y); mix(s.neutral); mix(s.only_comp);
    mix(s.disabled.size()); for (int type : s.disabled) mix(type);
  }
  mix(w.mission.paths.size());
  for (const auto& path : w.mission.paths) {
    mix(path.size()); for (const auto& point : path) { mix(point.first); mix(point.second); }
  }
  mix(pending_.size());
  for (const auto& c : pending_) {
    mix(uint8_t(c.type)); mix(c.side); mix(uint32_t(c.a)); mix(uint32_t(c.b)); mix(uint32_t(c.c));
    mix(c.ids.size()); for (uint32_t id : c.ids) mix(id);
  }
  mix(w.day_clock);
  mix(w.mission.towns.size());
  for (const TownDef& t : w.mission.towns) { mix(uint32_t(t.index)); mix(uint32_t(t.type)); mix(uint32_t(t.race)); mix(t.side); mix(t.x); mix(t.y); mix(t.scenario_id); }
  mix(w.ai_towns.size());
  for (const auto& s : w.ai_towns) {
    mix(s.phase); mix(s.science_timer); mix(s.target_town); mix(s.remaining);
    mix(s.defence_enabled); mix(s.elapsed); mix(s.harvested); mix(s.spent); mix(s.last_alert);
    mix(s.random); mix(s.plan_generation);
    mix(s.knowledge.size());
    for (const auto& record : s.knowledge) { mix(record.town); for (int n : record.counts) mix(n); }
    mix(s.plan.size());
    for (const auto& entry : s.plan) { mix(entry.type); mix(entry.producer); mix(entry.object); }
    mix(s.known_towns.size()); for (int town : s.known_towns) mix(town);
  }
  mix(w.town_speed.size()); for (int speed : w.town_speed) mix(speed);
  mix(w.repair_credits.size()); for (int credits : w.repair_credits) mix(credits);
  mix(tick_); mix(visual_rng_); mix(paused_); mix(spawn_counter_); mix(w.objects.size());
  for (const Object& o : w.objects) {
    mix(uint8_t(o.source)); mix(uint32_t(o.prev_x)); mix(uint32_t(o.prev_y));
    mix(uint32_t(o.town)); mix(o.harvest_ready);
    for (const auto* h : {&o.gold_harvest, &o.repair_harvest}) { mix(h->accumulated); mix(h->last_clock); mix(h->cursor); }
    mix(o.id); mix(uint32_t(o.type)); mix(uint32_t(o.side)); mix(uint32_t(o.hp)); mix(uint32_t(o.cur_x)); mix(uint32_t(o.cur_y));
    // Transport state joins the digest only when present, preserving pinned hashes of non-transport worlds.
    const bool vessel_state = is_transport(o.type) || o.carrier || o.boarding || !o.vessel_enabled ||
                              o.vessel_route || o.vessel_arrived || o.landing_x || o.landing_y || !o.cargo.empty();
    if (vessel_state) {
      mix(o.carrier); mix(o.boarding); mix(o.vessel_enabled); mix(o.vessel_route); mix(o.vessel_arrived);
      mix(o.landing_x); mix(o.landing_y); mix(o.cargo.size());
      for (auto id : o.cargo) mix(id);
    }
    mix(uint8_t(o.attack_mode)); mix(uint32_t(o.follow_tactic)); mix(o.order); mix(o.target); mix(uint32_t(o.cooldown)); mix(o.alive); mix(o.removed); mix(uint32_t(o.progress));
    mix(uint32_t(o.render_height)); mix(uint32_t(o.destruction_radius));
    mix(uint32_t(o.transport_phase)); mix(uint32_t(o.orientation)); mix(o.construction_overlay); mix(o.has_fired);
    mix(o.body_tick); mix(o.attack_tick); mix(o.body_moving); mix(o.anim_tick);
    mix(uint32_t(o.body_angle)); mix(uint32_t(o.turret_angle));
    mix(o.facing); mix(o.anim_state); mix(uint32_t(o.corpse_ticks));
    mix(uint32_t(o.dest_x)); mix(uint32_t(o.dest_y)); mix(o.resume); mix(uint32_t(o.build_elapsed)); mix(uint32_t(o.build_total));
    mix(uint32_t(o.town)); mix(o.town_hall); mix(o.defence);
    mix(uint32_t(o.secondary_hp)); mix(uint32_t(o.regen_service_ticks));
    mix(uint32_t(o.hp_max)); mix(uint32_t(o.tactic)); mix(uint32_t(o.scenario)); mix(o.last_hit_by); mix(uint32_t(o.mover)); mix(o.anim_variant);
  }
  mix(w.effects.size());
  for (const auto& e : w.effects) {
    mix(uint32_t(e.kind)); mix(e.source); mix(e.target); mix(uint32_t(e.side));
    mix(e.anim); mix(e.slot); mix(e.angle); mix(uint32_t(e.x)); mix(uint32_t(e.y));
    mix(uint32_t(e.end_x)); mix(uint32_t(e.end_y)); mix(uint32_t(e.height));
    mix(uint32_t(e.prev_x)); mix(uint32_t(e.prev_y));
    mix(e.age); mix(e.lifetime); mix(e.speed); mix(e.damage); mix(e.splash_px);
    mix(e.impact_anim); mix(e.shockwave); mix(e.payload_id); mix(e.flight); mix(e.loop);
    mix(e.velocity_x); mix(e.velocity_y); mix(e.altitude); mix(e.vertical_speed); mix(e.gravity);
  }
  mix(w.town_state.size());
  for (const TownState& t : w.town_state) {
    mix(uint32_t(t.index)); mix(uint32_t(t.side)); mix(t.hall); mix(uint8_t(t.repair_mode)); mix(t.marked.size());
    for (uint32_t id : t.marked) mix(id);
  }
  mix(w.side_state.size());
  for (const SideState& s : w.side_state) {
    mix(uint32_t(s.selected_tactic));
    mix(s.selected_ids.size()); for (uint32_t id : s.selected_ids) mix(id);
    mix(uint32_t(s.money)); mix(s.queue.size());
    mix(s.enemy_mask);
    for (const QueueItem& q : s.queue) { mix(uint32_t(q.type)); mix(uint32_t(q.remaining)); mix(uint32_t(q.total)); mix(uint32_t(q.town)); mix(uint32_t(q.paid)); mix(q.plan_town); mix(q.plan_entry); mix(q.plan_generation);
      mix(q.started); mix(q.paused); mix(uint32_t(q.tile_x)); mix(uint32_t(q.tile_y)); mix(q.object); mix(uint32_t(q.work_rate)); }
    for (const auto& g : s.groups) { mix(g.size()); for (uint32_t id : g) mix(id); }
  }
  mix(w.visibility_brightness);
  mix(w.visibility.sides.size());
  for (const VisFile::Layer& l : w.visibility.sides) {
    mix(l.w); mix(l.h); mix(l.v.size()); for (uint8_t value : l.v) mix(value);
  }
  mix(script_.state_hash());
  for (int a = 0; a < 8; a++) for (int b = 0; b < 8; b++) mix(w.relation[a][b]);
  mix(w.outcome);
  for (uint8_t outcome : w.side_outcome) mix(outcome);
  if (mover_) mix(mover_->state_hash());
  return h;
}

std::unique_ptr<GameSim> GameSim::replay(World& fresh, std::shared_ptr<const RulesData> rules, const std::vector<LoggedCommand>& log,
                                         uint64_t until_tick, const std::string& scn_path, ReplayObserver before_commands, Modifications modifications) {
  uint64_t previous = 0;
  for (const auto& entry : log) {
    if (!entry.tick || entry.tick < previous || entry.tick > until_tick)
      throw std::invalid_argument("command log is unordered or outside replay horizon");
    previous = entry.tick;
  }
  auto sim = std::make_unique<GameSim>(std::move(rules), modifications);
  if (!scn_path.empty() && !sim->attach_script(scn_path)) throw std::invalid_argument("cannot load replay script");
  sim->init(fresh);
  size_t i = 0;
  for (uint64_t t = 1; t <= until_tick; t++) {
    if (before_commands) before_commands(t, fresh, *sim);
    while (i < log.size() && log[i].tick == t) sim->submit(log[i++].cmd);
    fresh.begin_tick();
    sim->tick(fresh);
  }
  return sim;
}

}  // namespace aztec::engine
