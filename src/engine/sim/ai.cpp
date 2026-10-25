// Computer-player skeleton. Only what the data supports is implemented; the rest of the original AI
// (Tactic.cpp / Defence.cpp decision logic) is not decoded. See docs/engine_rules_status.md.
#include <algorithm>

#include "engine/sim/game_sim.hpp"
#include "engine/world/type_info.hpp"

namespace aztec::engine {

namespace {
constexpr int kAiBuyEvery = 56;      // observed: the AI spends on a new unit every 56 ticks
constexpr int kAiIncomeEvery = 300;  // observed: passive income +60 per 300 ticks
constexpr int kAiIncome = 60;
constexpr int kAiMaxUnits = 80;      // placeholder population cap
constexpr int kAlertRadius = 10 * kTilePx;  // placeholder
}  // namespace

void GameSim::run_ai(World& w) {
  for (const SideDef& sd : w.mission.sides) {
    int side = sd.index;
    if (!sd.only_comp || sd.neutral || side < 0 || side >= int(w.side_state.size())) continue;
    SideState& ss = w.side_state[size_t(side)];
    if (tick_ % kAiIncomeEvery == 0) ss.money += kAiIncome;

    // Production: round-robin over the affordable units of the town's build list.
    if (tick_ % kAiBuyEvery == 0 && ss.queue.empty()) {
      int units = 0;
      for (const Object& o : w.objects) if (!o.removed && o.alive && o.side == side && o.type >= 0 && !rules_->type(o.type).structure) units++;
      if (units < kAiMaxUnits) {
        std::vector<int> cand;
        for (const MenuItem& m : menu(w, side)) if (m.enabled && !m.construct) cand.push_back(m.type);
        if (!cand.empty()) {
          Command c;
          c.type = Pct::Build; c.side = uint8_t(side); c.a = cand[next_rand() % cand.size()];
          apply(w, c);
        }
      }
    }

    // Defence: when a structure is hit, idle units nearby go after the attacker.
    if (tick_ % 24 == 0) {
      for (const Object& st : w.objects) {
        if (st.removed || !st.alive || st.side != side || st.type < 0 || !rules_->type(st.type).structure || !st.last_hit_by) continue;
        const Object* att = w.find(st.last_hit_by);
        if (!att || att->removed || !att->alive) continue;
        std::vector<uint32_t> help;
        for (const Object& u : w.objects) {
          if (u.removed || !u.alive || u.side != side || u.type < 0 || u.order != uint8_t(Order::Idle)) continue;
          if (!rules_->type(u.type).mobile || !rules_->type(u.type).armed) continue;
          int dx = (u.cur_x >> 8) - (st.cur_x >> 8), dy = (u.cur_y >> 8) - (st.cur_y >> 8);
          if (int64_t(dx) * dx + int64_t(dy) * dy <= int64_t(kAlertRadius) * kAlertRadius) help.push_back(u.id);
        }
        if (!help.empty()) script_order(w, help, att->cur_x >> 8, att->cur_y >> 8, true);
      }
    }
  }
}

}  // namespace aztec::engine
