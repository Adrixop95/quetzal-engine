// Generated river scenarios exercise engine policies, not original-game parity.
#include "engine/sim/game_sim.hpp"
#include "engine/sim/savegame.hpp"
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
static void crossing() {
  auto r=rules(); World w=river(2), fresh=w; GameSim sim(r); sim.init(w);
  command(sim,Pct::Board,{3,2},1); ticks(sim,w,240);
  CHECK(w.objects[0].cargo.size()==2);
  CHECK(w.objects[1].removed && w.objects[1].carrier == 1);
  auto loaded_hash=sim.state_hash(w); std::swap(w.objects[0].cargo[0],w.objects[0].cargo[1]);
  CHECK(sim.state_hash(w)!=loaded_hash); std::swap(w.objects[0].cargo[0],w.objects[0].cargo[1]);
  auto restored=GameSim::replay(fresh,r,sim.log(),sim.tick_count());
  CHECK(restored->state_hash(fresh)==sim.state_hash(w)); CHECK(fresh.objects[0].cargo==w.objects[0].cargo);
  SaveData save; save.mission="transport"; save.tick=sim.tick_count(); save.hash=sim.state_hash(w); save.log=sim.log();
  SaveData decoded; CHECK(deserialize_save(serialize_save(save),decoded));
  World saved_world=river(2); std::string error;
  auto saved_sim=replay_save(decoded,saved_world,r,0,"",error); CHECK(saved_sim && error.empty());
  CHECK(saved_world.objects[0].cargo==w.objects[0].cargo);
  command(sim,Pct::Move,{1},10*48+24,7*48+24); ticks(sim,w,180);
  CHECK((w.objects[0].cur_x>>8)/48==10); CHECK(w.objects[0].cargo.size()==2);
  command(sim,Pct::Move,{1},11*48+24,7*48+24); ticks(sim,w,1);
  CHECK(w.objects[0].cargo.empty());
  CHECK(!w.objects[1].removed && !w.objects[2].removed && !w.objects[1].carrier);
  CHECK((w.objects[1].cur_x>>8)/48>=11); CHECK(w.objects[1].cur_y != w.objects[2].cur_y || w.objects[1].cur_x != w.objects[2].cur_x);
  World replayed=river(2); auto replay=GameSim::replay(replayed,r,sim.log(),sim.tick_count());
  CHECK(replay->state_hash(replayed)==sim.state_hash(w));
}
static void limits() {
  auto r=rules(); World w=river(7); GameSim sim(r); sim.init(w);
  command(sim,Pct::Board,{2,3,4,5,6,7,8},1); ticks(sim,w,800);
  CHECK(w.objects[0].cargo.size()==kTransportCapacity);
  int outside=0; for (size_t i=1;i<w.objects.size();++i) if (!w.objects[i].removed) { ++outside; CHECK(!w.objects[i].boarding); }
  CHECK(outside==2);
  sim.script_kill(w,1); CHECK(w.objects[0].cargo.empty());
  int dead=0; for (size_t i=1;i<w.objects.size();++i) if (!w.objects[i].alive) ++dead; CHECK(dead==5);
  World far=river(); GameSim fs(r); fs.init(far); fs.teleport(far,1,8*48+24,7*48+24);
  command(fs,Pct::Board,{2},1); ticks(fs,far,80); CHECK(far.objects[0].cargo.empty()); CHECK(!far.objects[1].removed);
  command(fs,Pct::Move,{1},5*48+24,7*48+24); ticks(fs,far,240); CHECK(far.objects[0].cargo.size()==1);
  command(fs,Pct::Move,{1},8*48+24,7*48+24); ticks(fs,far,100);
  command(fs,Pct::Unload,{1},8*48+24,7*48+24); ticks(fs,far,1); CHECK(far.objects[0].cargo.size()==1);
  command(fs,Pct::Unload,{1},0,0); ticks(fs,far,1); CHECK(far.objects[0].cargo.size()==1);
}
static void occupied_shore() {
  World w=river(); GameSim sim(rules()); sim.init(w);
  command(sim,Pct::Board,{2},1); ticks(sim,w,240); CHECK(w.objects[0].cargo.size()==1);
  // Deliberate blockers have no agents: the unload occupancy query must reject them itself.
  for (int y=5;y<=9;++y) for (int x=3;x<=4;++x) add(w,0,x,y);
  command(sim,Pct::Unload,{1},4*48+24,7*48+24); ticks(sim,w,1);
  CHECK(w.objects[0].cargo.size()==1 && w.objects[1].removed);
  for (size_t i=2;i<w.objects.size();++i) w.objects[i].removed=true;
  command(sim,Pct::Unload,{1},4*48+24,7*48+24); ticks(sim,w,1);
  CHECK(w.objects[0].cargo.empty() && !w.objects[1].removed);
}
static void eligibility() {
  World w=river(); add(w,3,3,8); add(w,7,3,9); add(w,0,3,10,1); auto r=rules();
  for (auto& row : w.relation) for (auto& relation : row) relation=kFriend;
  GameSim sim(r); sim.init(w);
  command(sim,Pct::Board,{2,3,4,5},1); ticks(sim,w,500);
  CHECK(w.objects[0].cargo.size()==2); CHECK(w.objects[2].carrier==1); // siege vehicle allowed
  CHECK(!w.objects[3].carrier && !w.objects[4].carrier); // air and enemy rejected
}
static void script_arrivals() {
  Ini ini; CHECK(ini.parse("[Globals]\nMapWidth=16\nMapHeight=16\nSides=1\nBoats=1\nJohnkas=1\nSubmarines=1\n[Boat0]\nSide=0\nScenarioID=4\nX=14\nY=-2\nEditorX=10\nEditorY=7\nEndPosition=1\nUnits=1\nUnit0=11,7,0,100\n[Johnka0]\nSide=0\nScenarioID=5\nX=8\nY=8\nEnabled=0\n[Submarine0]\nScenarioID=6\nX=8\nY=8\n"));
  World w=river(0); CHECK(parse_mission(ini,"transport",w.mission)); place_mission_objects(w);
  CHECK(w.objects[0].cargo.size()==1 && w.objects[1].removed && !w.objects[2].vessel_enabled);
  TempDir tmp; auto file=(tmp.path()/"transport.scn").string();
  std::ofstream(file)<<"trigger(1) conditions always actions boat(4).activate johnka(5).enable submarine(6).go trigger(1).disable end";
  GameSim sim(rules()); CHECK(sim.attach_script(file)); sim.init(w); ticks(sim,w,250);
  CHECK(sim.script().unsupported()==0); CHECK(w.objects[0].vessel_arrived); CHECK(w.objects[0].cargo.empty());
  CHECK(w.objects[2].vessel_enabled); CHECK(w.objects[3].vessel_arrived);
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
static void unload_button() {
  World w=river(); GameSim sim(rules()); sim.init(w);
  command(sim,Pct::Board,{2},1); ticks(sim,w,240); CHECK(w.objects[0].cargo.size()==1);
  w.selection.ids={1}; CommandsUi ui(&sim); Camera cam; Minimap mm;
  SDL_Rect panel{746,0,254,700}; auto layout=CommandsUi::layout(panel,1);
  CHECK(CommandsUi::action_enabled(PanelAction::Garrison,w,&sim.rules()));
  Input in; in.focused=true; in.mx=layout.commands[4].x+4; in.my=layout.commands[4].y+4;
  in.pressed[SDL_BUTTON_LEFT]=true; in.buttons[SDL_BUTTON_LEFT]=true;
  ui.update(in,w,cam,1,mm,false,panel);
  in.pressed[SDL_BUTTON_LEFT]=false; in.buttons[SDL_BUTTON_LEFT]=false; in.released[SDL_BUTTON_LEFT]=true;
  ui.update(in,w,cam,1,mm,false,panel);
  CHECK(!sim.pending_commands().empty()); CHECK(sim.pending_commands().back().type==Pct::Unload);
  ticks(sim,w,1); CHECK(w.objects[0].cargo.empty());
}
int main(int argc,char** argv) {
  if (argc==3 && std::string(argv[1])=="--campaign") { campaign_sites(argv[2]); return 0; }
  crossing(); limits(); occupied_shore(); eligibility(); script_arrivals(); unload_button(); std::puts("Transport scenarios passed");
}
