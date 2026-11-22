// Generated river scenarios exercise engine policies, not original-game parity.
#include "engine/sim/game_sim.hpp"
#include "engine/sim/savegame.hpp"
#include "engine/sim/sight.hpp"
#include "core/endian.hpp"
#include "engine/ui/commands_ui.hpp"
#include "engine/ui/hud.hpp"
#include "engine/ui/selection_ui.hpp"
#include "temp_dir.hpp"
#include <cstdio>
#include <cstdlib>
#include <fstream>
using namespace aztec;
using namespace aztec::engine;
#define CHECK(x) do { if (!(x)) { std::fprintf(stderr, "Line %d: %s\n", __LINE__, #x); std::exit(1); } } while (0)
static auto rules() { auto r = std::make_shared<RulesData>(); r->init_defaults(); return r; }
static void add(World& w, int type, int x, int y, int side = 0) {
  Object o; o.id = uint32_t(w.objects.size()+1); o.type = type; o.side = side;
  o.teleport(tile_center_fixed(x), tile_center_fixed(y)); w.objects.push_back(o);
}
static World river(int count = 1) {
  World w; w.mission.file_name="transport"; w.terrain.w = 16; w.terrain.h = 16; w.terrain.cells.resize(256);
  w.passability.w = w.passability.h = 16; w.passability.v.assign(256,2);
  for (int y=0; y<16; ++y) for (int x=5; x<=10; ++x) w.passability.v[size_t(y)*16+x]=0;
  w.side_state.resize(8); add(w,84,5,7);
  for (int i=0;i<count;++i) add(w,0,2+i%2,3+i/2);
  return w;
}
static void ticks(GameSim& sim, World& w, int n) { while (n--) { w.begin_tick(); sim.tick(w); } }
static void command(GameSim& sim, Pct pct, std::vector<uint32_t> ids, int a=0, int b=0, int c=0) {
  Command cmd; cmd.type=pct; cmd.ids=ids; cmd.a=a; cmd.b=b; cmd.c=c; sim.submit(cmd);
}
static void until(GameSim& sim, World& w, const auto& predicate, int limit = 1800) {
  while (!predicate() && limit-- > 0) ticks(sim,w,1);
  if (!predicate()) for (const auto& o : w.objects)
    std::fprintf(stderr,"id=%u pos=%d,%d phase=%d cargo=%zu boarding=%u tactic=%d order=%u dest=%d,%d\n",o.id,o.cur_x/256,o.cur_y/256,o.ferry_phase,o.cargo.size(),o.boarding,o.tactic,o.order,o.dest_x,o.dest_y);
  CHECK(predicate());
}
static void crossing() {
  auto r=rules(); World w=river(3), fresh=w; GameSim sim(r); sim.init(w);
  const auto dock_x=w.objects[0].cur_x, dock_y=w.objects[0].cur_y;
  command(sim,Pct::Attack,{2,3,4},1);
  until(sim,w,[&]{ return w.objects[0].ferry_phase==3; });
  CHECK(w.objects[0].cargo.size()==3);
  CHECK(w.objects[1].removed && w.objects[1].carrier==1);
  const auto loaded_hash=sim.state_hash(w);
  std::swap(w.objects[0].cargo[0],w.objects[0].cargo[1]);
  CHECK(sim.state_hash(w)!=loaded_hash); std::swap(w.objects[0].cargo[0],w.objects[0].cargo[1]);
  for (int* field : {&w.objects[0].ferry_phase, &w.objects[0].ferry_wait, &w.objects[0].orientation,
                    &w.objects[0].boat_state, &w.objects[0].ferry_boarded,
                    &w.objects[0].ferry_direction, &w.objects[0].ferry_destination, &w.objects[0].ferry_request, &w.objects[0].ferry_group_side}) {
    ++*field; CHECK(sim.state_hash(w)!=loaded_hash); --*field;
  }
  CHECK(ferry_protected(w.objects[0])==kFerryInvulnerableWhileSailing);
  ticks(sim,w,kFerryTravelPause);
  CHECK(w.objects[0].cur_x==dock_x && w.objects[0].cur_y==dock_y);
  ticks(sim,w,1);
  CHECK(w.objects[0].cur_x==dock_x+to_fixed(5) && w.objects[0].cur_y==dock_y-to_fixed(5));
  // Save while moving, restore through the actual command-log save format, and
  // compare every later tick through unloading and docking.
  SaveData save; save.mission="transport"; save.tick=sim.tick_count(); save.hash=sim.state_hash(w); save.log=sim.log();
  SaveData decoded; CHECK(deserialize_save(serialize_save(save),decoded));
  std::string error; auto saved_sim=replay_save(decoded,fresh,r,0,"",error); CHECK(saved_sim && error.empty());
  CHECK(saved_sim->state_hash(fresh)==sim.state_hash(w));
  bool landed=false, returning=false;
  for (int i=0;i<400;++i) {
    const int previous_phase=w.objects[0].ferry_phase;
    ticks(sim,w,1); ticks(*saved_sim,fresh,1);
    CHECK(saved_sim->state_hash(fresh)==sim.state_hash(w));
    if (previous_phase==3 && w.objects[0].ferry_phase==4) {
      const int next_x=w.objects[0].cur_x/256/48+1, next_y=w.objects[0].cur_y/256/48-1;
      CHECK(w.passability.v[size_t(next_y)*16+size_t(next_x)]!=0);
    }
    if (w.objects[0].ferry_phase==5) { landed=true; returning=true; CHECK(w.objects[0].cargo.empty()); }
  }
  CHECK(landed && returning && w.objects[0].ferry_phase==0);
  CHECK(w.objects[0].cur_x==dock_x && w.objects[0].cur_y==dock_y && w.objects[0].orientation==0);
  for (size_t i=1;i<w.objects.size();++i) {
    CHECK(!w.objects[i].removed && !w.objects[i].carrier);
    CHECK(w.objects[i].tactic==w.objects[0].ferry_destination || w.objects[0].ferry_destination==-1);
    CHECK((w.objects[i].cur_x>>8)/48>=11);
  }
  // A second ordinary order starts another complete cycle at the same dock.
  for (size_t i=1;i<w.objects.size();++i) sim.teleport(w,uint32_t(i+1),3*48+24,int(5+i)*48+24);
  command(sim,Pct::Attack,{2,3,4},1);
  until(sim,w,[&]{return w.objects[0].ferry_phase==3;});
  CHECK(w.objects[0].cargo.size()==3);
  until(sim,w,[&]{return w.objects[0].ferry_phase==0;});
  CHECK(w.objects[0].cargo.empty() && w.objects[0].cur_x==dock_x);
}
static void limits() {
  auto r=rules(); World w=river(7); GameSim sim(r); sim.init(w);
  command(sim,Pct::Attack,{8,7,6,5,4,3,2},1);
  until(sim,w,[&]{return w.objects[0].ferry_phase==3;});
  CHECK(w.objects[0].cargo.size()==kTransportCapacity);
  int outside=0; for (size_t i=1;i<w.objects.size();++i) if (!w.objects[i].removed) { ++outside; CHECK(w.objects[i].boarding==1); }
  CHECK(outside==2);
  sim.script_kill(w,1); CHECK(w.objects[0].cargo.empty());
  int dead=0; for (size_t i=1;i<w.objects.size();++i) if (!w.objects[i].alive) ++dead; CHECK(dead==5);
  World far=river(); far.objects[0].teleport(tile_center_fixed(8),tile_center_fixed(7));
  GameSim fs(r); fs.init(far); command(fs,Pct::Attack,{2},1); ticks(fs,far,240);
  CHECK(far.objects[0].cargo.empty() && !far.objects[1].removed); // no teleport across water
}
static void occupied_shore() {
  World w=river(); GameSim sim(rules()); sim.init(w);
  command(sim,Pct::Board,{2},1);
  until(sim,w,[&]{return w.objects[0].ferry_phase==3 && w.objects[0].boat_state==2;});
  // Block every land tile with objects that have no mover agents. Placement
  // must check live occupants itself, then retry when they are removed.
  const size_t first_blocker=w.objects.size();
  for (int y=0;y<16;++y) for (int x=0;x<16;++x)
    if (w.passability.at(x,y)) add(w,0,x,y);
  ticks(sim,w,kFerryOpeningTicks+1);
  CHECK(w.objects[0].ferry_phase==3 && w.objects[0].cargo.size()==1);
  CHECK(w.objects[1].removed && w.objects[1].carrier==1);
  for (size_t i=first_blocker;i<w.objects.size();++i) w.objects[i].removed=true;
  ticks(sim,w,1);
  CHECK(w.objects[0].cargo.empty() && !w.objects[1].removed && !w.objects[1].carrier);
}
static void eligibility() {
  for (bool town : {false,true}) {
    World w=river(2);
    if (town) for (size_t i=1;i<w.objects.size();++i) w.objects[i].town=0;
    else for (size_t i=1;i<w.objects.size();++i) w.objects[i].type=7;
    auto start=w.objects[1].cur_x; GameSim sim(rules()); sim.init(w);
    command(sim,Pct::Attack,{2,3},1); ticks(sim,w,300);
    CHECK(w.objects[0].ferry_phase==0 && w.objects[0].cargo.empty());
    if (town) CHECK(w.objects[1].cur_x==start);
  }
  World w=river(2); w.objects[2].type=3; add(w,7,3,8); add(w,0,3,9,1);
  for (auto& row : w.relation) for (auto& relation : row) relation=kFriend;
  GameSim sim(rules()); sim.init(w);
  command(sim,Pct::Attack,{2,3,4,5},1);
  until(sim,w,[&]{return w.objects[0].ferry_phase==3;});
  CHECK(w.objects[0].cargo.size()==2 && !w.objects[3].carrier && !w.objects[4].carrier);
}
static void script_arrivals() {
  World w=river(0); w.objects[0].source=ObjectSource::Boat; w.objects[0].scenario=4; w.objects[0].orientation=1;
  add(w,0,3,7); w.objects[1].carrier=1; w.objects[1].removed=true; w.objects[0].cargo={2};
  w.objects[1].tactic=42;
  TempDir tmp; auto file=(tmp.path()/"transport.scn").string();
  std::ofstream(file)<<"trigger(1) conditions always actions boat(4).activate trigger(1).disable end";
  GameSim sim(rules()); CHECK(sim.attach_script(file)); sim.init(w); ticks(sim,w,1);
  CHECK(sim.script().unsupported()==0 && w.objects[0].ferry_phase==3);
  CHECK(w.objects[0].boat_state==5 && w.objects[0].ferry_request==-1);
  const auto remaining=w.objects[0].ferry_wait;
  CHECK(sim.activate_vessel(w,4,ObjectSource::Boat)); CHECK(w.objects[0].ferry_wait==remaining);
  until(sim,w,[&]{return w.objects[0].ferry_phase==0;});
  CHECK(w.objects[0].cargo.empty() && !w.objects[1].removed && w.objects[1].tactic!=42);
}
static void occupancy_order() {
  World w=river(3);
  w.objects[1].teleport(tile_center_fixed(4),tile_center_fixed(8));
  w.objects[2].teleport(tile_center_fixed(4),tile_center_fixed(6));
  w.objects[3].teleport(tile_center_fixed(4),tile_center_fixed(7));
  GameSim sim(rules()); sim.init(w); command(sim,Pct::Attack,{4,2,3},1);
  until(sim,w,[&]{return w.objects[0].ferry_phase==2;});
  CHECK(w.objects[0].cargo==std::vector<uint32_t>({3,4,2}));
}
static void placement_and_policy() {
  World w=river(0);
  // Small-only land is legal for infantry but not a large siege passenger.
  for (auto& value : w.passability.v) if (value==2) value=1;
  w.objects[0].teleport(tile_center_fixed(10),tile_center_fixed(7));
  add(w,0,0,0); add(w,3,0,0);
  CHECK(!rules()->type(0).large && rules()->type(3).large);
  for (size_t i=1;i<w.objects.size();++i) { w.objects[i].carrier=1; w.objects[i].removed=true; }
  w.objects[0].cargo={2,3};
  GameSim sim(rules()); sim.init(w);
  w.objects[0].ferry_phase=3; w.objects[0].boat_state=3; w.objects[0].ferry_destination=23;
  w.objects[1].idle_attack=true; // Landing must retire pursuit state from the departure shore.
  ticks(sim,w,1);
  CHECK(!w.objects[1].removed && w.objects[1].tactic==23);
  CHECK(!w.objects[1].idle_attack);
  CHECK(w.objects[1].idle_anchor_x==w.objects[1].cur_x/256);
  CHECK(w.objects[1].idle_anchor_y==w.objects[1].cur_y/256);
  CHECK(w.objects[2].removed && w.objects[0].cargo==std::vector<uint32_t>{3});
  CHECK(w.objects[0].ferry_phase==3); // blocked passenger is retained, never placed on illegal land
  for (int ph : {3,5}) {
    w.objects[0].ferry_phase=ph; w.objects[0].ferry_wait=10; w.objects[0].hp_max=w.objects[0].hp=100;
    Effect shot; shot.flight=true; shot.damage=50; shot.target=1; shot.source=0;
    shot.x=shot.end_x=w.objects[0].cur_x; shot.y=shot.end_y=w.objects[0].cur_y;
    shot.speed=256; w.effects.push_back(shot);
    ticks(sim,w,1); CHECK(w.objects[0].hp==(kFerryInvulnerableWhileSailing ? 100 : 50));
    CHECK(w.objects[2].removed && w.objects[2].carrier==1);
  }
}
static void reach_and_direction() {
  for (int mode=0;mode<4;++mode) {
    World w=river(); w.objects[1].teleport(tile_center_fixed(1),tile_center_fixed(7));
    w.objects[1].attack_mode=AttackMode(mode);
    GameSim sim(rules()); sim.init(w); command(sim,Pct::Attack,{2},1); ticks(sim,w,1);
    CHECK(w.objects[0].ferry_phase==(mode==0 ? 0 : 1)); // strict distance < reach
  }
  World w=river(0); w.objects[0].source=ObjectSource::Boat; w.objects[0].scenario=4;
  w.objects[0].orientation=0; w.objects[0].ferry_direction=-1; w.objects[0].teleport(tile_center_fixed(10),tile_center_fixed(3));
  GameSim sim(rules()); sim.init(w); CHECK(sim.activate_vessel(w,4,ObjectSource::Boat));
  const auto x=w.objects[0].cur_x,y=w.objects[0].cur_y;
  ticks(sim,w,1); CHECK(w.objects[0].cur_x==x-to_fixed(5) && w.objects[0].cur_y==y+to_fixed(5));
  until(sim,w,[&]{return w.objects[0].ferry_phase==0;});
  CHECK(w.objects[0].ferry_direction==1 && w.objects[0].cur_x==x && w.objects[0].cur_y==y);
}
static void original_motion_and_states() {
  World w=river(0);
  w.objects[0].source=ObjectSource::Boat; w.objects[0].scenario=9;
  w.objects[0].orientation=1;
  w.objects[0].teleport(tile_center_fixed(5),tile_center_fixed(10));
  // The starting cell is forbidden to the clone's Water class. It must not
  // prevent movement; only the NEXT small-walkable diagonal tile stops a boat.
  w.passability.v.assign(256,0);
  w.passability.v[10*16+5]=2;
  w.passability.v[6*16+9]=1; // small-only land is enough
  GameSim sim(rules()); sim.init(w);
  CHECK(sim.activate_vessel(w,9,ObjectSource::Boat));
  const auto x=w.objects[0].cur_x,y=w.objects[0].cur_y;
  ticks(sim,w,27);
  CHECK(w.objects[0].cur_x==x+to_fixed(135) && w.objects[0].cur_y==y-to_fixed(135));
  ticks(sim,w,1);
  CHECK(w.objects[0].cur_x==tile_center_fixed(8) && w.objects[0].cur_y==tile_center_fixed(7));
  CHECK(w.objects[0].ferry_phase==3 && w.objects[0].boat_state==2);

  World load=river(); load.objects[1].teleport(tile_center_fixed(4),tile_center_fixed(7));
  GameSim ls(rules()); ls.init(load); command(ls,Pct::Attack,{2},1); ticks(ls,load,1);
  CHECK(load.objects[0].ferry_phase==1 && load.objects[0].boat_state==2);
  ticks(ls,load,kFerryOpeningTicks-1); CHECK(load.objects[0].boat_state==2);
  ticks(ls,load,1); CHECK(load.objects[0].boat_state==3 && load.objects[0].cargo.size()==1);
  ticks(ls,load,1); CHECK(load.objects[0].ferry_phase==2 && load.objects[0].boat_state==4);
  ticks(ls,load,kFerryClosingTicks);
  CHECK(load.objects[0].ferry_phase==3 && load.objects[0].boat_state==6 && load.objects[0].orientation==0);
  const auto lx=load.objects[0].cur_x,ly=load.objects[0].cur_y;
  ticks(ls,load,kFerryTravelPause-1);
  CHECK(load.objects[0].boat_state==6 && load.objects[0].cur_x==lx);
  ticks(ls,load,1);
  CHECK(load.objects[0].boat_state==5 && load.objects[0].orientation==1 && load.objects[0].cur_x==lx);
  ticks(ls,load,1);
  CHECK(load.objects[0].cur_x==lx+to_fixed(5) && load.objects[0].cur_y==ly-to_fixed(5));
}
static void original_landing() {
  World w=river(3); w.passability.v.assign(256,2);
  w.objects[0].teleport(tile_center_fixed(8),tile_center_fixed(8));
  w.objects[0].cargo={2,3,4};
  for (size_t i=1;i<w.objects.size();++i) {
    w.objects[i].removed=true; w.objects[i].carrier=1; w.objects[i].tactic=42;
    w.objects[i].attack_mode=AttackMode::Aggressive;
  }
  GameSim sim(rules()); sim.init(w);
  w.objects[0].ferry_phase=3; w.objects[0].boat_state=3; w.objects[0].ferry_group_side=1;
  ticks(sim,w,1);
  CHECK(w.objects[0].ferry_phase==4 && w.objects[0].boat_state==4 && w.objects[0].cargo.empty());
  for (size_t i=1;i<w.objects.size();++i) {
    const auto& o=w.objects[i];
    CHECK(o.alive && !o.removed && !o.carrier && !o.boarding && o.side==1);
    CHECK(o.tactic==w.objects[1].tactic && o.tactic!=42 && o.attack_mode==AttackMode::Balanced);
    CHECK(o.cur_x==tile_center_fixed(6+int(i)) && o.cur_y==tile_center_fixed(7));
  }
  CHECK(w.selection.ids.empty());
  ticks(sim,w,kFerryClosingTicks);
  CHECK(w.objects[0].ferry_phase==5 && w.objects[0].boat_state==6);
  ticks(sim,w,kFerryTravelPause);
  CHECK(w.objects[0].boat_state==5 && w.objects[0].orientation==1);
}
static void requester_fate() {
  for (bool dies : {false,true}) {
    World w=river(2); GameSim sim(rules()); sim.init(w);
    w.objects[0].cargo={2}; w.objects[0].ferry_boarded=1;
    w.objects[0].ferry_phase=1; w.objects[0].boat_state=3;
    w.objects[0].ferry_request=12; w.objects[0].ferry_group_side=0;
    w.objects[1].carrier=1; w.objects[1].removed=true; w.objects[1].tactic=-1;
    w.objects[2].tactic=12; w.objects[2].boarding=1;
    if (dies) sim.script_kill(w,3); else sim.script_stop(w,{3});
    ticks(sim,w,1);
    CHECK(w.objects[0].ferry_phase==2 && w.objects[0].boat_state==4);
    CHECK(w.objects[1].alive && w.objects[1].carrier==1);
    until(sim,w,[&]{return w.objects[0].ferry_phase==4;});
    CHECK(w.objects[1].alive && !w.objects[1].carrier && !w.objects[1].removed);
    CHECK(w.objects[1].tactic!=12 && w.objects[1].side==0);
  }
  World w=river(); GameSim sim(rules()); sim.init(w);
  command(sim,Pct::Attack,{2},1); ticks(sim,w,1);
  command(sim,Pct::Stop,{2}); ticks(sim,w,1);
  CHECK(w.objects[0].ferry_phase==5 && w.objects[0].cargo.empty());
  until(sim,w,[&]{return w.objects[0].ferry_phase==0;});
  CHECK(w.objects[0].boat_state==1);
}
static void preloaded_request_cancellation() {
  World w=river(); GameSim sim(rules()); sim.init(w);
  w.objects[0].cargo={2}; w.objects[1].carrier=1; w.objects[1].removed=true;
  w.objects[0].ferry_phase=1; w.objects[0].boat_state=3;
  w.objects[0].ferry_request=12; w.objects[0].ferry_boarded=0;
  ticks(sim,w,1);
  CHECK(w.objects[0].ferry_phase==5 && w.objects[0].boat_state==4);
  until(sim,w,[&]{return w.objects[0].ferry_phase==0;});
  CHECK(w.objects[1].alive && w.objects[1].carrier==1 && w.objects[0].cargo.size()==1);
}
static void activate_every_phase_and_loss() {
  for (int ph=0;ph<=5;++ph) {
    World w=river(1); w.passability.v.assign(256,0);
    w.objects[0].source=ObjectSource::Boat; w.objects[0].scenario=4;
    GameSim sim(rules()); sim.init(w);
    w.objects[0].ferry_phase=ph; w.objects[0].boat_state=ph+1; w.objects[0].ferry_wait=20;
    w.objects[0].cargo={2}; w.objects[1].carrier=1; w.objects[1].removed=true;
    const auto x=w.objects[0].cur_x,y=w.objects[0].cur_y;
    CHECK(sim.activate_vessel(w,4,ObjectSource::Boat));
    CHECK(w.objects[0].ferry_phase==3 && w.objects[0].boat_state==5 && !w.objects[0].ferry_wait);
    CHECK(w.objects[0].orientation==0 && w.objects[0].cargo==std::vector<uint32_t>{2});
    ticks(sim,w,1);
    CHECK(w.objects[0].cur_x==x-to_fixed(5) && w.objects[0].cur_y==y+to_fixed(5));
    until(sim,w,[&]{return !w.objects[0].alive;});
    CHECK(!w.objects[1].alive && w.objects[0].cargo.empty() && w.objects[0].boat_state==0);
  }
  World w=river(2); GameSim sim(rules()); sim.init(w);
  w.objects[0].cargo={2}; w.objects[1].carrier=1; w.objects[1].removed=true;
  w.objects[2].boarding=1;
  w.objects[0].teleport(tile_center_fixed(-8),tile_center_fixed(7));
  ticks(sim,w,1);
  CHECK(!w.objects[0].alive && !w.objects[1].alive && w.objects[2].boarding==0);
}
static void mission_boat_and_submarine() {
  // Editor/EndPosition metadata must not control placement, direction or cargo.
  TempDir tmp;
  const auto mp=tmp.path()/"boats.mp";
  std::ofstream(mp)<<"[Globals]\nBoats=1\nSubmarines=1\n"
      "[Boat0]\nSide=1\nX=5\nY=7\nUp=1\nEditorX=1\nEditorY=2\nEndPosition=0\n"
      "Units=1\nUnit0=0,0,0,100\n[Submarine0]\nX=8\nY=9\nScenarioId=3\nUnits=1\nUnit0=0,0,0,100\n";
  World w; CHECK(load_mission(mp.string(),w.mission));
  w.terrain.w=w.terrain.h=16; w.terrain.cells.resize(256); w.side_state.resize(8);
  place_mission_objects(w);
  CHECK(w.objects.size()==3);
  CHECK(w.objects[0].cur_x==tile_center_fixed(5) && w.objects[0].cur_y==tile_center_fixed(7));
  CHECK(w.objects[0].orientation==1 && w.objects[0].cargo==std::vector<uint32_t>{2});
  CHECK(w.objects[1].removed && w.objects[1].carrier==1 && w.objects[0].ferry_boarded==0);
  GameSim sim(rules()); sim.init(w);
  const auto& sub=w.objects[2]; const auto x=sub.cur_x,y=sub.cur_y;
  CHECK(sub.type==-1 && sub.mover==-1);
  auto idle=[&]{return std::count_if(w.effects.begin(),w.effects.end(),[](const Effect& e){return e.anim==27 && e.slot==0 && e.loop;});};
  CHECK(idle()==1); ticks(sim,w,100); CHECK(idle()==1);
  CHECK(w.objects[2].cur_x==x && w.objects[2].cur_y==y);
  CHECK(sim.activate_vessel(w,3,ObjectSource::Submarine));
  CHECK(idle()==0 && w.objects[2].removed && w.objects[2].vessel_arrived);
  CHECK(w.effects.size()==1 && w.effects[0].anim==27 && w.effects[0].slot==1 && !w.effects[0].loop);
  const int duration=w.effects[0].lifetime;
  ticks(sim,w,duration-1); CHECK(w.effects.size()==1);
  ticks(sim,w,1); CHECK(w.effects.empty());
  CHECK(!sim.activate_vessel(w,3,ObjectSource::Submarine));
}
static void resource_animation_timing() {
  // Invented Anim records: one three-tick frame in every boat/submarine slot.
  std::vector<uint8_t> anim(0x208+31*32);
  auto put=[](auto& bytes,size_t offset,uint32_t value){write_le(bytes.data()+offset,value);};
  std::copy_n("Anim",4,anim.begin()); put(anim,4,3);
  for (int slot=0;slot<10;++slot) {
    const int start=1+slot*3;
    put(anim,8+slot*4,uint32_t(start));
    put(anim,0x208+start*32,1); put(anim,0x208+start*32+28,3);
  }
  std::vector<uint8_t> archive(40);
  put(archive,0,1); put(archive,4,0x515); put(archive,8,20); put(archive,12,23); put(archive,16,28);
  for (int id=23;id<=27;++id) {
    put(archive,20+(id-23)*4,uint32_t(archive.size()));
    if (id==23 || id==27) archive.insert(archive.end(),anim.begin(),anim.end());
  }
  TempDir tmp; auto file=tmp.path()/"Units.xbe";
  std::ofstream out(file,std::ios::binary); out.write(reinterpret_cast<const char*>(archive.data()),std::streamsize(archive.size())); out.close();
  auto sight=std::make_shared<SightData>();
  CHECK(sight->effects.open(file.string()) && sight->sprites.open(file.string()));
  auto r=rules(); r->set_sight(sight);
  World w=river(); w.objects[1].teleport(tile_center_fixed(4),tile_center_fixed(7));
  add(w,-1,8,9); w.objects[2].source=ObjectSource::Submarine; w.objects[2].scenario=3;
  GameSim sim(r); sim.init(w); command(sim,Pct::Attack,{2},1); ticks(sim,w,1);
  ticks(sim,w,2); CHECK(w.objects[0].boat_state==2);
  ticks(sim,w,1); CHECK(w.objects[0].boat_state==3 && w.objects[0].cargo.size()==1);
  ticks(sim,w,1); CHECK(w.objects[0].boat_state==4);
  ticks(sim,w,3); CHECK(w.objects[0].boat_state==6);
  ticks(sim,w,2); CHECK(w.objects[0].boat_state==6);
  ticks(sim,w,1); CHECK(w.objects[0].boat_state==5 && w.objects[0].orientation==1);
  CHECK(sim.activate_vessel(w,3,ObjectSource::Submarine));
  CHECK(w.effects.size()==1 && w.effects[0].lifetime==3 && !w.effects[0].loop);
  ticks(sim,w,3); CHECK(w.effects.empty());
}
static void reserved_landing_group() {
  World w=river(2); add(w,84,5,12);
  GameSim sim(rules()); sim.init(w);
  command(sim,Pct::Attack,{2},1);
  until(sim,w,[&]{return w.objects[0].ferry_phase==3;});
  const int landing=w.objects[0].ferry_destination;
  command(sim,Pct::Attack,{3},4); ticks(sim,w,1);
  CHECK(w.objects[2].tactic>landing);
  until(sim,w,[&]{return w.objects[0].ferry_phase==4;});
  CHECK(w.objects[1].tactic==landing && w.objects[1].tactic!=w.objects[2].tactic);
}
static void campaign_sites(const std::string& game) {
  GameData data; std::string error; CHECK(data.open(game,error));
  auto r=rules(); int sites=0;
  for (int mission : {2,7,9,15}) {
    World w; CHECK(load_world(data,std::to_string(mission),w,error));
    script::Script original; CHECK(script::load_script(data.map_file(w.mission.file_name,"scn"),original));
    GameSim sim(r); sim.init(w);
    for (const auto& trigger : original.triggers) for (const auto& action : trigger.actions) {
      using script::ActKind;
      if (action.kind!=ActKind::BoatActivate && action.kind!=ActKind::JohnkaEnable && action.kind!=ActKind::SubmarineGo) continue;
      ObjectSource source=action.kind==ActKind::BoatActivate ? ObjectSource::Boat :
          action.kind==ActKind::JohnkaEnable ? ObjectSource::Johnka : ObjectSource::Submarine;
      CHECK(ScriptRunner::supports(action.kind));
      // Each site has a fresh world: repeated enables and dormant gates are tested independently.
      World probe=w; GameSim instance(r); instance.init(probe);
      CHECK(instance.activate_vessel(probe,action.id,source)); ticks(instance,probe,1);
      bool running=false;
      for (const auto& vessel : probe.objects) if (vessel.source==source && vessel.scenario==action.id)
        running=source==ObjectSource::Submarine ? vessel.vessel_arrived && vessel.removed :
            source==ObjectSource::Johnka ? vessel.vessel_enabled : vessel.vessel_route || vessel.vessel_arrived;
      CHECK(running); ++sites;
      std::printf("Mission %d trigger %d line %d scenario %d: handler accepted\n",mission,trigger.id,action.line,action.id);
    }
  }
  CHECK(sites==19);
}
static void no_player_control() {
  World w=river(); GameSim sim(rules()); sim.init(w);
  CHECK(w.objects[0].mover==-1);
  const auto x=w.objects[0].cur_x, y=w.objects[0].cur_y;
  command(sim,Pct::Move,{1},10*48,3*48); command(sim,Pct::Unload,{1}); command(sim,Pct::Stop,{1}); ticks(sim,w,1);
  CHECK(w.objects[0].cur_x==x && w.objects[0].cur_y==y && w.objects[0].ferry_phase==0);
  w.selection.ids={1};
  for (auto action : {PanelAction::Move,PanelAction::Attack,PanelAction::Garrison})
    CHECK(!CommandsUi::action_enabled(action,w,&sim.rules()));
  // Both single picking and drag selection reject the ferry; picking for a right
  // click still finds it so that an ordinary group can follow it.
  Camera cam; SelectionUi ui; Input in; in.focused=true;
  in.mx=int(cam.to_screen_x(float(x/256))); in.my=int(cam.to_screen_y(float(y/256)));
  CHECK(pick_object(w,float(x/256),float(y/256),1,10)->id==1);
  in.pressed[SDL_BUTTON_LEFT]=true; in.buttons[SDL_BUTTON_LEFT]=true; ui.update(in,w,cam,1,true);
  in.pressed[SDL_BUTTON_LEFT]=false; in.buttons[SDL_BUTTON_LEFT]=false; in.released[SDL_BUTTON_LEFT]=true; ui.update(in,w,cam,1,true);
  CHECK(w.selection.ids.empty());
  command(sim,Pct::CreateGroup,{1},0); command(sim,Pct::SelectGroup,{},0); ticks(sim,w,1);
  CHECK(w.side_state[0].selected_ids.empty());
}
static void bizon_pursuit() {
  World w; w.terrain.w=64; w.terrain.h=16; w.terrain.cells.resize(64*16);
  w.passability.w=64; w.passability.h=16; w.passability.v.assign(64*16,2); w.side_state.resize(8);
  add(w,32,2,7,1); add(w,0,40,7); w.objects[1].hp=6000;
  GameSim sim(rules()); sim.init(w);
  Command attack; attack.type=Pct::Attack; attack.side=1; attack.ids={1}; attack.a=2; sim.submit(attack);
  ticks(sim,w,1); CHECK(w.objects[0].target==0 && w.objects[0].cur_x==tile_center_fixed(2)); // beyond the V Marine pursuit limit
  sim.teleport(w,2,20*48+24,7*48+24); sim.submit(attack);
  ticks(sim,w,1); CHECK(w.objects[0].target==2); // a reachable land enemy is still pursued
}
static void ferry_combat() {
  World w=river(); add(w,32,4,7,1); w.relation[0][1]=w.relation[1][0]=kFriend;
  auto r=rules(); GameSim sim(r); sim.init(w);
  // Positive health isolates ferry acquisition policy from the controller damage guard.
  w.objects[0].hp_max=w.objects[0].hp=1000;
  w.objects[0].cargo={2}; w.objects[1].carrier=1; w.objects[1].removed=true;
  CHECK(w.objects[0].alive && w.objects[0].cargo.size()==1);
  w.relation[0][1]=w.relation[1][0]=kEnemy; ticks(sim,w,120);
  CHECK(w.objects[0].alive && w.objects[0].cargo.size()==1);
  CHECK(w.objects[2].target!=1); // G: automatic acquisition excludes the ferry only.
  Command attack; attack.type=Pct::Attack; attack.side=1; attack.ids={3}; attack.a=1; sim.submit(attack);
  ticks(sim,w,120); CHECK(!w.objects[0].alive && !w.objects[1].alive);
  // A positive-health Johnka within shore firing range remains eligible.
  World johnka=river(0); add(johnka,88,5,8); add(johnka,32,4,7,1);
  johnka.objects[1].hp=johnka.objects[2].hp=100000;
  GameSim js(r); js.init(johnka);
  johnka.objects[1].hp_max=johnka.objects[1].hp=100000;
  ticks(js,johnka,8);
  CHECK(johnka.objects[2].target==2); CHECK(johnka.objects[0].alive);
}
static void zero_maximum_combat_fallback() {
  for (int type : {84, 88}) {
    World w=river(0); w.objects[0].type=type;
    add(w,27,4,7,1); add(w,0,3,7);
    w.objects[1].hp=w.objects[2].hp=10000;
    GameSim sim(rules()); sim.init(w);
    CHECK(w.objects[0].hp_max==0 && w.objects[0].hp==0);
    Command attack; attack.side=1; attack.type=Pct::Attack; attack.ids={2}; attack.a=1;
    sim.submit(attack); ticks(sim,w,12);
    CHECK(w.objects[0].alive && w.objects[0].hp==0);
    CHECK(w.objects[1].target!=1); // automatic acquisition also excludes the controller
    CHECK(w.objects[2].hp<10000); // ordinary recipients still take direct damage

    World impact=river(0); impact.objects[0].type=type;
    add(impact,0,3,7); impact.objects[1].hp=10000;
    GameSim projectiles(rules()); projectiles.init(impact);
    Effect shot; shot.flight=true; shot.target=1; shot.side=1; shot.damage=700;
    shot.x=shot.end_x=impact.objects[0].cur_x; shot.y=shot.end_y=impact.objects[0].cur_y;
    shot.speed=256; shot.splash_px=200; impact.effects.push_back(shot);
    ticks(projectiles,impact,1);
    CHECK(impact.objects[0].alive && impact.objects[0].hp==0);
    CHECK(impact.objects[1].hp==9300); // splash must not become globally harmless

    // Positive-health synthetic vessels retain ordinary combat semantics.
    impact.objects[0].hp_max=impact.objects[0].hp=1000;
    impact.effects.push_back(shot); ticks(projectiles,impact,1);
    CHECK(impact.objects[0].hp==300 && impact.objects[0].alive);
    projectiles.script_kill(impact,1); CHECK(!impact.objects[0].alive);
  }
}
static void reachable_automatic_pursuit() {
  auto r=rules();
  auto sight=std::make_shared<SightData>();
  for (auto& day:sight->daylight) day.fill(80);
  sight->masks[0].assign(404,0); sight->masks[0][0]=sight->masks[0][2]=20;
  r->set_sight(sight);
  for (bool bridge : {false,true}) {
    World w; w.terrain.w=32; w.terrain.h=16; w.terrain.cells.resize(512);
    w.passability={32,16,std::vector<uint8_t>(512,2)};
    for (int y=0;y<16;++y) for (int x=13;x<=18;++x)
      if (!bridge || y!=7) w.passability.v[size_t(y)*32+x]=0;
    // Keep the enemy inside the seven-tile scan edge, so only
    // the bridge determines whether automatic pursuit has reachable firing ground.
    add(w,0,12,7); add(w,0,19,7,1); add(w,89,23,7);
    w.objects[0].hp=w.objects[1].hp=10000;
    w.objects[0].attack_mode=AttackMode::Aggressive;
    w.side_state.resize(8);
    GameSim sim(r); sim.init(w); ticks(sim,w,23);
    CHECK(w.objects[0].target==(bridge ? 2u : 0u));
    ticks(sim,w,20);
    CHECK(sim.path_failures(w.objects[0])==0);
  }
}
int main(int argc,char** argv) {
  if (argc==3 && std::string(argv[1])=="--campaign") { campaign_sites(argv[2]); return 0; }
  bizon_pursuit(); ferry_combat(); zero_maximum_combat_fallback(); reachable_automatic_pursuit();
  original_motion_and_states(); original_landing(); requester_fate(); preloaded_request_cancellation(); activate_every_phase_and_loss(); mission_boat_and_submarine(); resource_animation_timing(); reserved_landing_group();
  crossing(); limits(); occupied_shore(); eligibility(); script_arrivals(); occupancy_order(); placement_and_policy(); reach_and_direction(); no_player_control(); std::puts("Transport scenarios passed");
}
