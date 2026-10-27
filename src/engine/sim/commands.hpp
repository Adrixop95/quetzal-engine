// Command queue modelled on the original lockstep "frame" packets (PCT_FRAME_*, docs/object_types.md).
// UI code never changes the world directly: it submits commands, the simulation applies them at the start
// of the next tick in a fixed order (by side, then submission order), and every applied command is logged
// so a game can be replayed or restored deterministically.
#pragma once
#include <cstdint>
#include <vector>

namespace aztec::engine {

// Values equal the original PCT_* packet type ids; 0x50.. are engine extensions (the original expressed
// stop/attack-move through other packets).
enum class Pct : uint8_t {
  PauseOn = 0x13, PauseOff = 0x14, CreateGroup = 0x15, SelectGroup = 0x16, Move = 0x17, Attack = 0x18,
  AddToGroup = 0x19, DestroyHere = 0x1a, Birth = 0x1b, Build = 0x1c, Garrison = 0x1d, RepairMode = 0x1e,
  AttackMode = 0x1f, BuildMode = 0x20, PauseBuild = 0x21, QueueKill = 0x22, SelectByNo = 0x23, FollowTeam = 0x24,
  Nothing = 0x25,
  Stop = 0x50,        // ids
  AttackMove = 0x51,  // ids, a,b = target px
};

// Field use per type:
//   Move/AttackMove: ids, a,b = world px.   Attack: ids, a = target object id.   Stop: ids.
//   Garrison: b = town section number; extract its live mobile members into a fresh tactic.
//   RepairMode: a = 0 off / 1 repair / 2 destroy, b = town section number.
//   DestroyHere: b = town section number, ids = buildings to toggle in its current repairer list.
//   These are engine field conventions, not a claim about the reference packet payload layout.
//   CreateGroup/AddToGroup/SelectGroup/SelectByNo: a = group number 0..9 (+ ids for Create/Add).
//   Build: a = item type (UGI); for buildings b,c = tile x,y of the footprint centre; units ignore b,c.
//   Build/BuildMode: optional ids[0] selects an owned town member; absent = first owned town (engine convention).
//   BuildMode: a = runtime speed 0 slow / 1 normal / 2 fast.
//   QueueKill: a = queue index to cancel (refund).  PauseBuild: a = 1 pause / 0 resume.  Pause*: global.
struct Command {
  Pct type = Pct::Nothing;
  uint8_t side = 0;
  int32_t a = 0, b = 0, c = 0;
  std::vector<uint32_t> ids;
};

struct LoggedCommand { uint64_t tick = 0; Command cmd; };

// Serialises a command log (little endian) for replay files / snapshots.
std::vector<uint8_t> serialize_log(const std::vector<LoggedCommand>& log);
bool deserialize_log(const std::vector<uint8_t>& bytes, std::vector<LoggedCommand>& out);

}  // namespace aztec::engine
