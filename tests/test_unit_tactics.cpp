// Invented placements exercise tactic rules without a reference process or game installation.
#include <cstdio>
#include <fstream>
#include <memory>
#include "engine/sim/game_sim.hpp"
#include "engine/sim/path/mover.hpp"
#include "engine/world/visibility.hpp"
#include "temp_dir.hpp"

using namespace aztec;
using namespace aztec::engine;
static int failures = 0;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL %d: %s\n", __LINE__, #c); ++failures; } } while (0)
static World map() {
  World w; w.terrain.w = w.terrain.h = 70; w.terrain.cells.resize(4900);
  SideDef a; w.mission.sides.push_back(a); a.index = 1; w.mission.sides.push_back(a);
  w.relation[0][1] = w.relation[1][0] = kEnemy;
  return w;
}
static void add(World& w, int side, int x, int y, int tactic = -1, int type = 0) {
  Object o; o.id = uint32_t(w.objects.size() + 1); o.side = side; o.type = type; o.tactic = tactic;
  o.cooldown = 10000; o.teleport(tile_center_fixed(x), tile_center_fixed(y)); w.objects.push_back(o);
}
static void step(GameSim& sim, World& w, int n = 1) {
  while (n-- > 0) { w.begin_tick(); sim.tick(w); }
}
static void scan(std::shared_ptr<RulesData> rules) {
  // Box is asymmetric: low -6, high +7 inclusive. It is independent of mode reach.
  for (int offset : {-7, -6, 7, 8}) {
    auto w = map(); add(w, 0, 20, 20); add(w, 1, 20 + offset, 20);
    w.objects[1].attack_mode = AttackMode::Direct;
    GameSim sim(rules, Modifications::faithful()); sim.init(w); step(sim, w);
    CHECK((w.objects[0].target == 2) == (offset == -6 || offset == 7));
    CHECK(w.unit_tactics.size() == 2); // unassigned units have singleton tactics
  }
  // Every member widens the box; discovery lists the whole enemy tactic.
  auto w = map(); add(w, 0, 20, 20, 5); add(w, 0, 24, 20, 5);
  add(w, 1, 31, 20, 8); add(w, 1, 28, 23, 8);
  GameSim sim(rules, Modifications::faithful()); sim.init(w); step(sim, w);
  CHECK(w.objects[0].target == 4); // listed enemy's nearer member is eligible
  auto& t = w.unit_tactics.at({0, 5});
  CHECK(t.x == 20 && t.y == 20);
  CHECK(t.scan_ticks >= 15 && t.scan_ticks <= 22);
  const int delay = t.scan_ticks;
  t.enemies.clear(); sim.script_stop(w, {1, 2});
  // stop clears targets, not the periodic timer; entering enemies wait for a tactic scan
  step(sim, w, delay - 1); CHECK(w.objects[0].target == 0);
  step(sim, w); CHECK(w.objects[0].target == 4);
  CHECK(t.scan_ticks >= 15 && t.scan_ticks <= 22);

  // Same-side numeric tactic IDs are independent across sides, and allies are excluded.
  auto allied = map(); add(allied, 0, 20, 20, 5); add(allied, 1, 22, 20, 5);
  allied.relation[0][1] = kFriend;
  GameSim friends(rules, Modifications::faithful()); friends.init(allied); step(friends, allied, 30);
  CHECK(allied.objects[0].target == 0 && allied.unit_tactics.size() == 2);
}
static void fog_and_tactics(std::shared_ptr<RulesData> rules) {
  // Tactic discovery uses building corners and the strict brightness threshold;
  // remembered buildings alone must never become combat targets.
  for (int brightness : {32, 33}) for (int progress : {999, 1000}) {
    auto w = map(); add(w, 0, 20, 20); add(w, 1, 24, 20, 8, 10);
    w.objects[1].attack_mode = AttackMode::Direct;
    w.visibility_brightness = true;
    w.visibility.sides.resize(8);
    w.visibility.sides[0] = {71, 72, std::vector<uint8_t>(71 * 72, 32)};
    w.visibility_rules = rules;
    const auto bounds = visibility_bounds(w, w.objects[1]);
    auto& fog = w.visibility.sides[0].v;
    fog[(bounds[1] / kTilePx) * 71 + bounds[0] / kTilePx] = uint8_t(brightness);
    GameSim sim(rules, Modifications::faithful()); sim.init(w);
    CHECK(w.unit_tactics.size() == 2);
    CHECK(w.building_memory[0].contains(2) == (brightness == 33));
    CHECK(!position_visible(w, 0, w.objects[1].cur_x >> 8, w.objects[1].cur_y >> 8));
    w.objects[1].progress = progress;
    w.objects[1].build_total = 1000; w.objects[1].build_elapsed = progress - 1;
    step(sim, w);
    const auto& tactic = w.unit_tactics.at(tactic_key(w.objects[0]));
    CHECK(tactic.enemies.empty() == (brightness == 32));
    CHECK(w.objects[0].target == 0); // discovery does not make an ordinary building eligible
    std::fill(fog.begin(), fog.end(), 32);
    step(sim, w);
    CHECK(w.objects[0].target == 0);
    CHECK(w.building_memory[0].contains(2) == (brightness == 33));
  }
}
static void structure_targets(std::shared_ptr<RulesData> rules) {
  // Ordinary buildings are not automatic tactic targets, even when a weapon
  // can damage them. Defence structures and explicit focus orders differ.
  for (int type : {10, 14, 17}) {
    auto w = map(); add(w, 0, 20, 20, -1, 1); add(w, 1, 23, 20, 8, type);
    // A mobile member lists the shared tactic, including its Defence member.
    if (type == 17) add(w, 1, 26, 20, 8);
    GameSim sim(rules, Modifications::faithful()); sim.init(w); step(sim, w);
    CHECK(w.objects[0].target == (type == 17 ? 2u : 0u));
  }
  auto mixed = map(); add(mixed, 0, 20, 20, -1, 1); add(mixed, 1, 23, 20, 8, 14);
  add(mixed, 1, 25, 20, 8);
  GameSim pick(rules, Modifications::faithful()); pick.init(mixed); step(pick, mixed);
  CHECK(mixed.objects[0].target == 3); // a nearer building must not hide a defender

  auto focused = map(); add(focused, 0, 20, 20, -1, 1); add(focused, 1, 23, 20, 8, 14);
  focused.objects[0].cooldown = 0;
  GameSim focus(rules, Modifications::faithful()); focus.init(focused);
  const int hp = focused.objects[1].hp;
  Command attack; attack.type = Pct::Attack; attack.ids = {1}; attack.a = 2;
  focus.submit(attack); step(focus, focused, 40);
  CHECK(focused.objects[0].has_fired && focused.objects[1].hp < hp);

  // An attack-move must continue into capture range of an undefended,
  // indestructible hall instead of shooting it forever from weapon range.
  auto initial = map(); initial.mission.towns = {{0, 1, 25, 20, 0, 0, 8}};
  add(initial, 0, 20, 20, 5, 1); add(initial, 1, 25, 20, 8, 10);
  initial.objects[1].town = 0; initial.objects[1].town_hall = true;
  TempDir directory;
  const auto script = (directory.path() / "capture.scn").string();
  std::ofstream(script) << "trigger(50) conditions side(1).towns == 0 actions "
      "side(local).victory trigger(50).disable end\n";
  auto capture = initial;
  GameSim march(rules, Modifications::faithful());
  CHECK(march.attach_script(script)); march.init(capture);
  Command move; move.type = Pct::AttackMove; move.ids = {1}; move.a = 25 * 48 + 24; move.b = 20 * 48 + 24;
  march.submit(move); step(march, capture, 240);
  CHECK(capture.mission.towns[0].side == 0 && capture.objects[1].side == 0);
  CHECK(capture.objects[1].alive && capture.objects[1].hp == capture.objects[1].hp_max);
  CHECK(!capture.objects[0].has_fired);
  CHECK(capture.outcome == kVictory);
  auto replay_world = initial;
  auto replay = GameSim::replay(replay_world, rules, march.log(), 240, script, {}, Modifications::faithful());
  CHECK(replay && replay->state_hash(replay_world) == march.state_hash(capture));
}
static void leash(std::shared_ptr<RulesData> rules) {
  CHECK(tactic_norm(3, 3) == 4 && tactic_norm(4, 4) == 5 && tactic_norm(6, 6) == 8);
  for (int mode = 0; mode < 4; ++mode) {
    auto w = map(); add(w, 0, 20, 20); add(w, 1, 27, 20);
    w.objects[0].attack_mode = AttackMode(mode); w.objects[1].attack_mode = AttackMode::Direct;
    GameSim sim(rules, Modifications::faithful()); sim.init(w);
    for (int tick = 0; tick < 180; ++tick) {
      step(sim, w);
      const auto& o = w.objects[0];
      CHECK(tactic_norm((o.cur_x >> 8) / 48 - 20, (o.cur_y >> 8) / 48 - 20) <= tactic_reach(AttackMode(mode)));
    }
    if (mode == 0 || mode == 1) CHECK(w.objects[0].cur_x == tile_center_fixed(20));
    if (mode == 3) CHECK(w.objects[0].target == 2); // retain the chase beyond the step leash
  }
  // Replan with no visible hostiles clears targets and returns members to the anchor.
  auto w = map(); add(w, 0, 20, 20); add(w, 1, 25, 20);
  w.objects[1].attack_mode = AttackMode::Direct;
  GameSim sim(rules, Modifications::faithful()); sim.init(w); step(sim, w, 12);
  CHECK(w.objects[0].cur_x > tile_center_fixed(20));
  sim.teleport(w, 2, 60 * 48 + 24, 20 * 48 + 24); step(sim, w, 150);
  CHECK(w.objects[0].target == 0 && w.objects[0].cur_x == tile_center_fixed(20));
}
static void loss_and_visibility(std::shared_ptr<RulesData> rules) {
  for (int loss = 0; loss < 4; ++loss) {
    auto w = map(); add(w, 0, 20, 20); add(w, 1, 23, 20);
    w.objects[1].attack_mode = AttackMode::Direct;
    GameSim sim(rules, Modifications::faithful()); sim.init(w); step(sim, w);
    CHECK(w.objects[0].target == 2);
    if (loss == 0) sim.script_kill(w, 2);
    if (loss == 1) w.objects[1].removed = true;
    if (loss == 2) sim.teleport(w, 2, 60 * 48 + 24, 20 * 48 + 24);
    if (loss == 3) {
      w.visibility_brightness = true;
      w.visibility.sides.resize(8);
      w.visibility.sides[0] = {71, 72, std::vector<uint8_t>(71 * 72, 32)};
    }
    step(sim, w); CHECK(w.objects[0].target == 0);
  }
  // Immediate repicking from a retained table preserves the interrupted order.
  auto interrupted = map(); add(interrupted, 0, 20, 20); add(interrupted, 1, 23, 20, 8); add(interrupted, 1, 24, 20, 8);
  interrupted.objects[1].attack_mode = interrupted.objects[2].attack_mode = AttackMode::Direct;
  GameSim repick(rules, Modifications::faithful()); repick.init(interrupted);
  Command march; march.type = Pct::Move; march.ids = {1}; march.a = 30 * 48 + 24; march.b = 20 * 48 + 24;
  repick.submit(march); step(repick, interrupted);
  CHECK(interrupted.objects[0].target == 2 && interrupted.objects[0].resume == 2);
  repick.script_kill(interrupted, 2); step(repick, interrupted);
  CHECK(interrupted.objects[0].target == 3 && interrupted.objects[0].resume == 2);
  repick.script_kill(interrupted, 3); step(repick, interrupted);
  CHECK(interrupted.objects[0].order == uint8_t(Order::Move));
  // The integer pixel norm retains exactly 1152 and drops the next unit.
  for (int offset : {1198, 1199}) {
    CHECK(tactic_norm(offset, 0) == (offset == 1198 ? 1152 : 1153));
    auto boundary = map(); add(boundary, 0, 20, 20); add(boundary, 1, 45, 20);
    boundary.objects[0].attack_mode = boundary.objects[1].attack_mode = AttackMode::Direct;
    boundary.objects[1].teleport(tile_center_fixed(20) + to_fixed(offset), tile_center_fixed(20));
    GameSim cutoff(rules, Modifications::faithful()); cutoff.init(boundary);
    Command attack; attack.type = Pct::Attack; attack.ids = {1}; attack.a = 2;
    cutoff.submit(attack); step(cutoff, boundary);
    CHECK(boundary.objects[0].target == (offset == 1198 ? 2u : 0u));
  }
  // A ready Marine can use an already-listed fireable enemy without waiting
  // for the scan timer, while its independent chase target stays retained.
  auto w = map(); add(w, 0, 20, 20); add(w, 1, 24, 20, 8); add(w, 1, 25, 20, 8);
  w.objects[0].attack_mode = w.objects[1].attack_mode = w.objects[2].attack_mode = AttackMode::Direct;
  GameSim sim(rules, Modifications::faithful()); sim.init(w); step(sim, w);
  w.objects[0].order = uint8_t(Order::Attack); w.objects[0].target = 2; w.objects[0].idle_attack = true;
  sim.teleport(w, 3, 20 * 48 + 54, 20 * 48 + 24);
  w.objects[0].cooldown = 0;
  step(sim, w); // local assignment can fire on this ready standing tick
  CHECK(w.objects[0].has_fired && w.objects[0].target == 2);
}
static void local_target_preference(std::shared_ptr<RulesData> rules) {
  auto w = map(); add(w, 0, 20, 20); add(w, 1, 24, 20, 8); add(w, 1, 25, 20, 8);
  for (auto& o : w.objects) o.attack_mode = AttackMode::Direct;
  GameSim sim(rules, Modifications::faithful()); sim.init(w); step(sim, w);
  sim.teleport(w, 2, 20 * 48 + 84, 20 * 48 + 24);
  sim.teleport(w, 3, 20 * 48 + 54, 20 * 48 + 24);
  auto& shooter = w.objects[0];
  shooter.order = uint8_t(Order::Attack); shooter.target = 2; shooter.idle_attack = true; shooter.cooldown = 0;
  step(sim, w);
  CHECK(shooter.has_fired && shooter.target == 2);
  CHECK(shooter.release_target == 2 || w.objects[1].hp < w.objects[1].hp_max ||
        std::any_of(w.effects.begin(), w.effects.end(), [](const auto& e) { return e.flight && e.target == 2; }));
  CHECK(w.objects[2].hp == w.objects[2].hp_max);
}
static void retaliation(std::shared_ptr<RulesData> rules) {
  for (int mode = 0; mode < 4; ++mode) for (bool moving : {false, true}) {
    auto w = map(); add(w, 0, 20, 20, 5); add(w, 1, 29, 20, 8);
    w.objects[0].attack_mode = AttackMode(mode); w.objects[1].attack_mode = AttackMode::Direct;
    GameSim sim(rules, Modifications::faithful()); sim.init(w);
    if (moving) {
      Command move; move.type = Pct::Move; move.ids = {1}; move.a = 20 * 48 + 24; move.b = 35 * 48 + 24;
      sim.submit(move);
    }
    step(sim, w); CHECK(w.objects[0].target == 0); // attacker outside the ordinary box
    Effect hit; hit.kind = data::EffectClass::BallExpl; hit.flight = true;
    hit.source = 2; hit.payload_id = 3;
    hit.x = hit.end_x = w.objects[0].cur_x; hit.y = hit.end_y = w.objects[0].cur_y;
    w.effects.push_back(hit); step(sim, w);
    const auto& t = w.unit_tactics.at({0, 5});
    CHECK(std::find(t.enemies.begin(), t.enemies.end(), TacticKey{1, 8}) != t.enemies.end());
    // Projectile hits notify tactics after unit updates; retaliation starts next tick.
    CHECK(w.objects[0].target == 0);
    step(sim, w);
    if (mode == 0 || (mode == 1 && moving)) CHECK(w.objects[0].target == 0);
    else CHECK(w.objects[0].target == 2);
  }
}
static void walking(std::shared_ptr<RulesData> rules) {
  auto initial = map(); add(initial, 0, 10, 10, 5); add(initial, 0, 9, 10, 5, 5);
  auto w = initial;
  GameSim sim(rules, Modifications::faithful()); sim.init(w);
  Command move; move.type = Pct::Move; move.ids = {1, 2}; move.a = 20 * 48 + 24; move.b = 10 * 48 + 24;
  sim.submit(move);
  int previous = 10;
  for (int tick = 0; tick < 250; ++tick) {
    step(sim, w);
    const auto& t = w.unit_tactics.at({0, 5});
    CHECK(t.x >= previous && t.x <= previous + 1); CHECK(t.y == 10); previous = t.x;
    CHECK(t.x <= 20);
    CHECK(w.objects[0].idle_anchor_x == t.x * 48 + 24);
  }
  CHECK(previous == 20);
  CHECK(w.objects[0].cur_x > tile_center_fixed(18));
  auto fresh = initial; auto replay = GameSim::replay(fresh, rules, sim.log(), sim.tick_count());
  CHECK(replay->state_hash(fresh) == sim.state_hash(w));
  auto hash = sim.state_hash(w); ++w.unit_tactics.at({0, 5}).scan_ticks; CHECK(hash != sim.state_hash(w));
  // Internal group identities also own mode propagation; unrelated singletons
  // with the same absent script number are not attached.
  auto modes = map(); add(modes, 0, 10, 10); add(modes, 0, 11, 10); add(modes, 0, 30, 30);
  GameSim mode_sim(rules, Modifications::faithful()); mode_sim.init(modes);
  move.ids = {1, 2}; mode_sim.submit(move); step(mode_sim, modes);
  CHECK(tactic_key(modes.objects[0]) == tactic_key(modes.objects[1]));
  Command stance; stance.type = Pct::AttackMode; stance.a = 0; stance.ids = {1};
  mode_sim.submit(stance); step(mode_sim, modes);
  CHECK(modes.objects[0].attack_mode == AttackMode::Direct && modes.objects[1].attack_mode == AttackMode::Direct);
  CHECK(modes.objects[2].attack_mode == AttackMode::Balanced);


  // A removed first member never recentres the surviving tactic.
  auto stable = map(); add(stable, 0, 20, 20, 5); add(stable, 0, 22, 20, 5);
  GameSim retained(rules, Modifications::faithful()); retained.init(stable);
  retained.script_kill(stable, 1); step(retained, stable);
  CHECK(stable.unit_tactics.at({0, 5}).x == 20);

  // A disconnected member beyond reach becomes a following team on replan.
  auto lost = map(); add(lost, 0, 20, 20, 5); add(lost, 0, 32, 20, 5);
  GameSim split(rules, Modifications::faithful()); split.init(lost); step(split, lost);
  CHECK(lost.objects[1].tactic != 5);
  CHECK(lost.unit_tactics.at(tactic_key(lost.objects[1])).lost);
  step(split, lost, 250);
  CHECK(lost.objects[1].tactic == 5); // follows and merges strictly inside reach
}
static void selection_split(std::shared_ptr<RulesData> rules) {
  auto w = map(); add(w, 0, 10, 10); add(w, 0, 25, 10); add(w, 0, 30, 10);
  for (auto& o : w.objects) o.transient_tactic = -(int64_t(1) << 40) - 100;
  GameSim sim(rules, Modifications::faithful()); sim.init(w); step(sim, w);
  CHECK(w.objects[1].tactic != -1); // detached before the player's command
  Command move; move.type = Pct::Move; move.ids = {1}; move.a = 40 * 48 + 24; move.b = 10 * 48 + 24;
  sim.submit(move); step(sim, w, 700);
  CHECK(w.objects[0].cur_x == tile_center_fixed(40));
  CHECK(w.objects[1].cur_x < tile_center_fixed(35) && w.objects[2].cur_x < tile_center_fixed(35));
}
static void hierarchy(std::shared_ptr<RulesData> rules) {
  auto w = map(); add(w, 0, 10, 10, 5); add(w, 0, 30, 10, 6);
  add(w, 0, 7, 10, 7); add(w, 0, 2, 10, 8);
  GameSim sim(rules, Modifications::faithful()); sim.init(w);
  sim.script_join(w, {4}, 1);
  sim.script_join(w, {1}, 2);
  CHECK(w.objects[0].tactic == 5); // no remote membership transfer
  CHECK(w.unit_tactics.at({0, 5}).merging && !w.unit_tactics.at({0, 5}).lost);
  sim.script_join(w, {2}, 1); // a cycle leaves the destination independent
  CHECK(!w.unit_tactics.at({0, 6}).merging);
  sim.script_join(w, {3}, 1); // nearby join is immediate
  CHECK(w.objects[2].tactic == 5);
  step(sim, w, 600);
  CHECK(w.objects[0].tactic == 6 && w.objects[2].tactic == 6 && w.objects[3].tactic == 6);
  CHECK(w.unit_tactics.at({0, 6}).x == 30);

  // Equality with reach does not merge; a fresh movement order cancels a join.
  auto boundary = map(); add(boundary, 0, 20, 20, 5); add(boundary, 0, 26, 20, 6);
  GameSim edge(rules, Modifications::faithful()); edge.init(boundary);
  edge.script_join(boundary, {1}, 2);
  CHECK(boundary.objects[0].tactic == 5 && boundary.unit_tactics.at({0, 5}).merging);
  edge.script_order(boundary, {1}, 10 * 48 + 24, 20 * 48 + 24, false);
  CHECK(!boundary.unit_tactics.at({0, 5}).merging);
  step(edge, boundary, 300);
  CHECK(boundary.objects[0].tactic == 5 && boundary.unit_tactics.at({0, 5}).x == 10);

  // A disappearing destination cancels a visible join; it cannot adopt the child.
  auto cancelled = map(); add(cancelled, 0, 10, 10, 5); add(cancelled, 0, 30, 10, 6);
  GameSim cancel(rules, Modifications::faithful()); cancel.init(cancelled);
  cancel.script_join(cancelled, {1}, 2); cancel.script_kill(cancelled, 2);
  step(cancel, cancelled);
  CHECK(!cancelled.unit_tactics.at({0, 5}).merging);
  CHECK(cancelled.objects[0].order == uint8_t(Order::Idle));

  // A hidden follower receives stance changes while it is outside merge range.
  auto lost = map(); add(lost, 0, 20, 20, 5); add(lost, 0, 34, 20, 5);
  GameSim split(rules, Modifications::faithful()); split.init(lost); step(split, lost);
  CHECK(lost.objects[1].tactic != 5);
  Command stance; stance.type = Pct::AttackMode; stance.ids = {1}; stance.a = 1;
  split.submit(stance); step(split, lost);
  CHECK(lost.objects[1].attack_mode == AttackMode::Defensive);
  const auto h = split.state_hash(lost);
  auto& child = lost.unit_tactics.at(tactic_key(lost.objects[1]));
  child.merging = false; CHECK(split.state_hash(lost) != h); child.merging = true;
  split.script_kill(lost, 1); step(split, lost);
  CHECK(lost.objects[1].tactic == 5); // hidden follower repopulates an empty parent
  CHECK(lost.unit_tactics.at({0, 5}).members == std::vector<uint32_t>{2});
}
static void losses_and_return(std::shared_ptr<RulesData> rules) {
  // Losing the first member during a march retains the route and anchor.
  auto w = map(); add(w, 0, 10, 10, 5); add(w, 0, 11, 10, 5); add(w, 0, 12, 10, 5);
  GameSim sim(rules, Modifications::faithful()); sim.init(w);
  sim.script_order(w, {1, 2, 3}, 30 * 48 + 24, 10 * 48 + 24, true);
  step(sim, w, 40);
  const int anchor = w.unit_tactics.at({0, 5}).x;
  sim.script_kill(w, 1); step(sim, w);
  CHECK(w.unit_tactics.at({0, 5}).x >= anchor);
  step(sim, w, 800);
  CHECK(w.unit_tactics.at({0, 5}).x == 30);
  CHECK(w.objects[1].cur_x > tile_center_fixed(27));
  CHECK(w.objects[2].cur_x > tile_center_fixed(27));

  // Retreat here means the verified disengage/return branch, not an HP threshold.
  auto returning = map(); add(returning, 0, 20, 20, 5); add(returning, 1, 25, 20, 6);
  returning.objects[1].attack_mode = AttackMode::Direct;
  GameSim retreat(rules, Modifications::faithful()); retreat.init(returning);
  step(retreat, returning, 35);
  CHECK(returning.objects[0].cur_x > tile_center_fixed(20));
  retreat.script_kill(returning, 2); step(retreat, returning, 250);
  CHECK(returning.objects[0].target == 0);
  CHECK(returning.objects[0].cur_x == tile_center_fixed(20));
  CHECK(returning.unit_tactics.at({0, 5}).x == 20);
}
static void tile_steps() {
  path::NavGrid grid(20, 20); path::MoveSystem mover(grid);
  auto a = mover.add_agent({tile_center_fixed(5), tile_center_fixed(5)}, 14 * 256, 4 * 256, path::MoveClass::Small);
  auto enemy = mover.add_agent({tile_center_fixed(5), tile_center_fixed(6)}, 14 * 256, 4 * 256, path::MoveClass::Small, 1);
  CHECK(!mover.order_step(a, {5, 6})); // no unfinished step into a hostile occupied tile
  CHECK(mover.order_step(enemy, {6, 6}));
  CHECK(!mover.order_step(a, {6, 6})); // the hostile destination is already reserved
  mover.agents_mut()[enemy].enabled = false;
  CHECK(!mover.order_step(a, {7, 5}));
  CHECK(mover.order_step(a, {6, 5}));
  for (int i = 0; i < 40; ++i) mover.tick();
  CHECK(mover.agents()[a].pos.x == tile_center_fixed(6)); // no automatic next tile
  CHECK(mover.agents()[a].state == path::AgentState::Arrived);
  // Enhanced crowd recovery also arbitrates two allied one-tile steps meeting
  // head-on; both yield back to their tactic instead of retaining a deadlock.
  path::NavGrid passage(12, 12);
  for (int y = 0; y < 12; ++y) for (int x = 0; x < 12; ++x) if (y != 5) passage.set_terrain(x, y, 0);
  path::MoverConfig config; config.unit_unstick = true;
  path::MoveSystem crowd(passage, config);
  auto left = crowd.add_agent({tile_center_fixed(5), tile_center_fixed(5)}, 14 * 256, 8 * 256, path::MoveClass::Small);
  auto right = crowd.add_agent({tile_center_fixed(6), tile_center_fixed(5)}, 14 * 256, 8 * 256, path::MoveClass::Small);
  CHECK(crowd.order_step(left, {6, 5}) && crowd.order_step(right, {5, 5}));
  for (int i = 0; i < 100; ++i) crowd.tick();
  CHECK(crowd.agents()[left].pos.x > crowd.agents()[right].pos.x);
}
int main() {
  auto rules = std::make_shared<RulesData>(); CHECK(rules->load_dir(AZTEC_TEST_RULES_DIR));
  scan(rules); fog_and_tactics(rules); structure_targets(rules); leash(rules); loss_and_visibility(rules); local_target_preference(rules); retaliation(rules); walking(rules); selection_split(rules); hierarchy(rules); losses_and_return(rules); tile_steps();
  std::puts(failures ? "FAILED" : "OK"); return failures ? 1 : 0;
}
