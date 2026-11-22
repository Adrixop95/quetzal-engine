// Invented placements exercise tactic rules without a reference process or game installation.
#include <cstdio>
#include <memory>
#include "engine/sim/game_sim.hpp"
#include "engine/sim/path/mover.hpp"

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
  scan(rules); leash(rules); loss_and_visibility(rules); retaliation(rules); walking(rules); selection_split(rules); tile_steps();
  std::puts(failures ? "FAILED" : "OK"); return failures ? 1 : 0;
}
