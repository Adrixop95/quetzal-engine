// Mission trigger script (Maps/<name>.scn): parser only. Grammar and semantics: docs/formats.md.
#pragma once
#include <string>
#include <string_view>
#include <vector>

namespace aztec::script {

constexpr int kLocal = -2;  // side(local): the human player's side

enum class Cmp { Eq, Ne, Lt, Le, Gt, Ge };

enum class CondKind {
  Always,
  LocalTime, GlobalTime,        // localTime / globalTime <cmp> value
  Timer,                        // timer(id) <cmp> value
  SideEnemies, SideUnits, SideTowns, SideTactics, SideMoney,
  SideUnitsInRect,              // rect[4]
  SideUnitsOfUGIInRect,         // ugi + rect[4]
  SideUnitsOfId,                // ugi
  TacticValid, TacticAttacked, TacticIdle, TacticLocked,  // id = tactic number
  DefenceValid,                 // id = defence ScenarioID
  IsFriendly, IsNeutral, IsEnemy,  // id = other side (may be kLocal)
};

struct Condition {
  CondKind kind = CondKind::Always;
  int side = 0;             // side number or kLocal (side(..) conditions)
  int id = 0;               // timer / tactic / defence / other side
  int ugi = -1;
  int rect[4] = {0, 0, 0, 0};  // x1,y1,x2,y2
  Cmp cmp = Cmp::Eq;
  int value = 0;
  int line = 0;
};

enum class ActKind {
  TriggerEnable, TriggerDisable,   // id
  BoatActivate,                    // id
  JohnkaEnable, JohnkaDisable,     // id
  SubmarineGo,                     // id
  TimerSet, TimerStart, TimerStop, TimerKill,  // id (+ value for set)
  CenterView,                      // a=x, b=y
  PrintString, PrintStringR,       // text (English / Russian, cp1251)
  SideVictory, SideDefeat,
  SideBuildCommand,                // ugi + args
  SideMoveCommand,                 // a=x, b=y
  SideCameraActivate,              // a=camera index, b=value
  SideCameraDeactivate,            // a (index)
  SideSetFriendlyTo, SideSetNeutralTo, SideSetEnemyTo,  // a = other side (may be kLocal)
  SideAddMoney, SideTakeMoney,     // a
  TacticAttackTown,                // id = tactic, a = town scenario id
  TacticMoveTo,                    // a=x, b=y
  TacticJoinTactic,                // a = tactic
  TacticSetPath, TacticSetInvertedPath, TacticSetCycledPath, TacticSetCycledInvertedPath,  // a = path id
  TacticLock, TacticUnlock, TacticKill,
  TacticGiftToSide,                // a = side (may be kLocal)
};

struct Action {
  ActKind kind = ActKind::TriggerEnable;
  int side = 0;      // for side(..) actions
  int id = 0;        // trigger/boat/johnka/submarine/timer/tactic id
  int a = 0, b = 0;
  int ugi = -1;
  std::vector<int> args;  // buildCommand extra args
  std::string text;       // raw bytes of the string literal (cp1250 or cp1251)
  int line = 0;
};

struct Trigger {
  int id = 0;
  std::vector<Condition> conditions;
  std::vector<Action> actions;
};

struct Script {
  std::vector<Trigger> triggers;
  std::string error;  // empty on success, "line N: message" otherwise
  int error_line = 0;
  const Trigger* find(int id) const;
};

// Case-insensitive, whitespace-insensitive like the original sscanf-based reader.
bool parse_script(std::string_view text, Script& out);
bool load_script(const std::string& path, Script& out);

const char* cmp_text(Cmp c);

}  // namespace aztec::script
