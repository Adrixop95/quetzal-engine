// Synthetic interface contracts. No installed resources or reference process.
#include <cstdio>
#include <memory>
#include <string>
#include <sstream>
#include "engine/app/ui_trace.hpp"
#include <vector>
#include "engine/app/options.hpp"
#include "temp_dir.hpp"
#include "test_env.hpp"
#include "engine/ui/commands_ui.hpp"
#include "engine/ui/cursor.hpp"
#include "engine/ui/panel.hpp"
#include "engine/ui/selection_ui.hpp"

using namespace aztec::engine;
static int failures = 0;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL %d: %s\n", __LINE__, #c); ++failures; } } while (0)
static bool rect(SDL_Rect a, SDL_Rect b) { return a.x == b.x && a.y == b.y && a.w == b.w && a.h == b.h; }
static bool inside(SDL_Rect r, int x, int y) { SDL_Point p{x, y}; return SDL_PointInRect(&p, &r); }

static void geometry() {
  Panel p; p.set_layout(InterfaceLayout::Original); p.layout(800, 600, 1);
  CHECK(rect(p.rect(), {596,0,204,600}));
  CHECK(rect(p.menu_rect(), {596,0,60,22}));
  CHECK(rect(p.minimap_rect(), {608,25,180,180}));
  CHECK(!p.contains(595,391)); CHECK(p.contains(596,391));
  CHECK(!p.contains(545,392)); CHECK(p.contains(546,392));
  CHECK(p.contains(799,599)); CHECK(!p.contains(800,599)); CHECK(!p.contains(600,600));
  auto c = original_panel_controls(p.rect(), 1);
  const SDL_Rect squad[] = {{560,406,59,44}, {624,405,48,44}, {672,405,38,44}, {710,405,38,44}, {748,405,38,44}};
  const SDL_Rect town[] = {{558,406,59,44}, {626,406,40,44}, {666,406,36,44}, {702,406,34,44}, {746,407,42,42}};
  for (int i = 0; i < 5; ++i) {
    CHECK(rect(c.squad[i], squad[i])); CHECK(rect(c.town[i], town[i]));
    CHECK(inside(c.squad[i], squad[i].x, squad[i].y));
    CHECK(!inside(c.squad[i], squad[i].x + squad[i].w, squad[i].y));
  }
  auto anchor = original_tooltip_anchor(p.rect(),1,true,580,480);
  CHECK(anchor.x == 570 && anchor.y == 463);
  CHECK(rect(c.build[0], {570,463,42,42})); CHECK(rect(c.build[8], {654,547,42,42}));
  CHECK(rect(c.queue[0], {738,463,42,42})); CHECK(rect(c.queue[2], {738,547,42,42}));
  CHECK(rect(c.members[0], {572,463,47,42})); CHECK(rect(c.members[11], {713,547,47,42}));
  CHECK(rect(c.towns[0], {618,218,59,44})); CHECK(rect(c.teams[3], {720,350,59,44}));
  auto layout = CommandsUi::layout(p.rect(), 1, InterfaceLayout::Original, true);
  CHECK(layout.production.size() == 9); CHECK(rect(layout.production[0], c.build[0]));
  CHECK(rect(layout.pause, c.town[4])); CHECK(layout.queue[3].w == 0);
  for (auto r : layout.commands) CHECK(r.w == 0);
  CHECK(rect(original_minimap_rect(p.minimap_rect(), 120, 90, 1), {638,70,120,90}));
  CHECK(rect(original_minimap_rect(p.minimap_rect(), 60, 30, 1), {608,70,180,90}));
  CHECK(rect(original_minimap_rect(p.minimap_rect(), 90, 120, 1), {653,55,90,120}));
  CHECK(original_minimap_rect(p.minimap_rect(), 181, 90, 1).w == 0);
  CHECK(original_minimap_rect(p.minimap_rect(), 0, 0, 1).w == 0);
  p.layout(1600,1200,2); CHECK(rect(p.minimap_rect(), {1216,50,360,360}));
  CHECK(rect(original_panel_controls(p.rect(),2).build[0], {1140,926,84,84}));
  p.set_layout(InterfaceLayout::Modern); p.layout(800,600,1);
  CHECK(rect(p.menu_rect(), {598,0,60,22})); CHECK(rect(p.minimap_rect(), {608,28,180,174}));
  CHECK(CommandsUi::layout(p.rect(),1).queue[7].w > 0);
}

static void cursors() {
  auto kind = [](bool ready, bool modal, bool placing, bool world, int x, int y, bool box, bool selected, int mode, bool hostile) {
    return Cursor::original_kind(ready,modal,placing,world,x,y,box,selected,mode,hostile);
  };
  CHECK(kind(false,true,true,false,1,1,true,true,2,true) == CursorKind::Arrow);
  CHECK(kind(true,true,false,true,1,0,false,true,0,true) == CursorKind::Menu);
  CHECK(kind(true,false,true,true,1,0,false,true,0,true) == CursorKind::Menu);
  CHECK(kind(true,false,false,false,1,0,false,true,0,true) == CursorKind::Menu);
  CHECK(kind(true,false,false,true,1,-1,true,false,2,true) == CursorKind::ScrollNE);
  CHECK(kind(true,false,false,true,0,0,true,true,2,true) == CursorKind::Menu);
  CHECK(kind(true,false,false,true,0,0,false,false,0,true) == CursorKind::Menu);
  CHECK(kind(true,false,false,true,0,0,false,true,2,true) == CursorKind::Move);
  CHECK(kind(true,false,false,true,0,0,false,true,1,true) == CursorKind::Pan);
  CHECK(kind(true,false,false,true,0,0,false,true,0,true) == CursorKind::Attack);
  CHECK(kind(true,false,false,true,0,0,false,true,0,false) == CursorKind::Menu);
  const CursorKind directions[] = {CursorKind::ScrollNW,CursorKind::ScrollN,CursorKind::ScrollNE,
    CursorKind::ScrollW,CursorKind::Menu,CursorKind::ScrollE,CursorKind::ScrollSW,CursorKind::ScrollS,CursorKind::ScrollSE};
  for (int y = -1; y <= 1; ++y) for (int x = -1; x <= 1; ++x)
    CHECK(kind(true,false,false,true,x,y,false,false,0,false) == directions[(y+1)*3+x+1]);
}

static World fixture() {
  World w; w.terrain.w = w.terrain.h = 40; w.terrain.cells.resize(1600);
  w.mission.sides.push_back(SideDef{}); w.mission.towns.push_back(TownDef{});
  w.mission.towns.back().type = 1;
  Object unit; unit.id = 1; unit.type = 0; unit.town = 0; unit.tactic = 7; unit.teleport(to_fixed(100),to_fixed(100));
  Object other = unit; other.id = 2; other.teleport(to_fixed(150),to_fixed(100));
  Object hall = unit; hall.id = 3; hall.type = 11; hall.town_hall = true; hall.teleport(to_fixed(350),to_fixed(350));
  w.objects = {unit,other,hall}; w.selection.ids = {1}; return w;
}

static void selection() {
  World w = fixture(); Camera cam; cam.set_world(1920,1920); cam.set_view(596,600); cam.zoom = 1;
  SelectionUi ui; ui.set_layout(InterfaceLayout::Original);
  Input in; in.mx = 10; in.my = 10; in.buttons[SDL_BUTTON_LEFT] = in.pressed[SDL_BUTTON_LEFT] = true;
  ui.update(in,w,cam,1,true); in.pressed[SDL_BUTTON_LEFT] = false;
  in.mx = 26; in.my = 26; ui.update(in,w,cam,1,true); CHECK(!ui.dragging());
  in.mx = 27; ui.update(in,w,cam,1,true); CHECK(ui.dragging());
  in.buttons[SDL_BUTTON_LEFT] = false; in.released[SDL_BUTTON_LEFT] = true;
  ui.update(in,w,cam,1,true); CHECK(w.selection.ids == std::vector<uint32_t>{1});
  auto click = [&](int x, int y, bool shift, bool ctrl) {
    Input click; click.mx = x; click.my = y; click.shift = shift; click.ctrl = ctrl;
    click.pressed[SDL_BUTTON_LEFT] = click.buttons[SDL_BUTTON_LEFT] = true; ui.update(click,w,cam,1,true);
    click.pressed[SDL_BUTTON_LEFT] = click.buttons[SDL_BUTTON_LEFT] = false;
    click.released[SDL_BUTTON_LEFT] = true; ui.update(click,w,cam,1,true);
  };
  click(100,100,false,false); CHECK(w.selection.ids.size() == 2);
  click(100,100,false,true); CHECK(w.selection.ids == std::vector<uint32_t>{1});
  click(150,100,true,true); CHECK(w.selection.ids == std::vector<uint32_t>{1});
  click(350,350,true,false); CHECK(w.selection.ids == std::vector<uint32_t>{1});
  click(350,350,false,true); CHECK(w.selection.ids == std::vector<uint32_t>{1});
  ui.object_bounds = [](const Object& o, float alpha) { auto p = o.interpolated(alpha); return SDL_FRect{p.x-20,p.y-20,40,40}; };
  // Release coordinates must complete a drag even without an intervening held frame.
  in = {}; in.mx = 70; in.my = 70; in.pressed[SDL_BUTTON_LEFT] = in.buttons[SDL_BUTTON_LEFT] = true;
  ui.update(in,w,cam,1,true); in.mx = 85; in.my = 110;
  in.pressed[SDL_BUTTON_LEFT] = in.buttons[SDL_BUTTON_LEFT] = false; in.released[SDL_BUTTON_LEFT] = true;
  ui.update(in,w,cam,1,true); CHECK(w.selection.ids == std::vector<uint32_t>{1});
  CHECK(!SelectionUi::overlaps({0,0,10,10},{10,0,10,10}));
  CHECK(SelectionUi::overlaps({0,0,10,10},{9,9,10,10}));
  in = {}; in.mx = 10; in.my = 10; in.pressed[SDL_BUTTON_LEFT] = in.buttons[SDL_BUTTON_LEFT] = true;
  ui.update(in,w,cam,1,true); in.pressed[SDL_BUTTON_LEFT] = false; in.mx = 40; ui.update(in,w,cam,1,true); CHECK(ui.dragging());
  in.focused = false; ui.update(in,w,cam,1,true); CHECK(!ui.dragging());
}

static void commands() {
  auto rules = std::make_shared<RulesData>(); rules->init_defaults();
  auto w = fixture(); GameSim sim(rules,Modifications::faithful()); sim.init(w); w.selection.ids = {1};
  CommandsUi ui(&sim); ui.set_layout(InterfaceLayout::Original);
  Camera cam; cam.set_world(1920,1920); cam.set_view(596,600); Minimap mm;
  const SDL_Rect panel{596,0,204,600}; const auto boxes = original_panel_controls(panel,1);
  auto click = [&](SDL_Rect box) {
    Input in; in.mx = box.x + 1; in.my = box.y + 1;
    in.pressed[SDL_BUTTON_LEFT] = in.buttons[SDL_BUTTON_LEFT] = true;
    ui.update(in,w,cam,1,mm,false,panel);
    in.pressed[SDL_BUTTON_LEFT] = in.buttons[SDL_BUTTON_LEFT] = false; in.released[SDL_BUTTON_LEFT] = true;
    ui.update(in,w,cam,1,mm,false,panel);
  };
  const int modes[] = {2,1,3,0};
  for (int i = 1; i < 5; ++i) {
    click(boxes.squad[i]); CHECK(sim.pending_commands().back().type == Pct::AttackMode);
    CHECK(sim.pending_commands().back().a == modes[i-1]);
  }
  click(boxes.squad[0]); CHECK(ui.center_selection(Input{},w,cam,1));
  for (int digit = 0; digit < 10; ++digit) {
    Input in; auto key = SDL_Scancode(digit ? SDL_SCANCODE_1 + digit - 1 : SDL_SCANCODE_0);
    in.key_hit[key] = true; in.key_down[SDL_SCANCODE_LCTRL] = true; in.ctrl = true; in.shift = true;
    ui.update(in,w,cam,1,mm,false,panel);
    CHECK(sim.pending_commands().back().type == Pct::CreateGroup); CHECK(sim.pending_commands().back().a == digit);
    in.key_down[SDL_SCANCODE_LCTRL] = false; in.ctrl = false;
    ui.update(in,w,cam,1,mm,false,panel); CHECK(sim.pending_commands().back().type == Pct::SelectGroup);
  }
  std::vector<aztec::KeyBinding> bindings;
  CHECK(aztec::parse_keys_ini("Inv rcontrol\nAutoRepair q\n",bindings)); ui.set_bindings(bindings);
  Input remapped; remapped.key_hit[SDL_SCANCODE_1] = true; remapped.key_down[SDL_SCANCODE_RCTRL] = true;
  ui.update(remapped,w,cam,1,mm,false,panel); CHECK(sim.pending_commands().back().type == Pct::CreateGroup);
  Input right; right.mx = 10; right.my = 10; right.pressed[SDL_BUTTON_RIGHT] = right.buttons[SDL_BUTTON_RIGHT] = true;
  ui.update(right,w,cam,1,mm,true,panel); right.pressed[SDL_BUTTON_RIGHT] = false; right.mx = right.my = 250;
  ui.update(right,w,cam,1,mm,true,panel); CHECK(!ui.right_dragging());
  right.buttons[SDL_BUTTON_RIGHT] = false; right.released[SDL_BUTTON_RIGHT] = true;
  ui.update(right,w,cam,1,mm,true,panel); CHECK(sim.pending_commands().back().type == Pct::Move);
  std::ostringstream trace;
  trace_ui(trace,0,w,cam,1,Input{},ui,nullptr,0,panel,1,true,false,CursorKind::Menu,true,0,"","",false);
  CHECK(trace.str().find("\"shape\":\"menu\"") != std::string::npos);
  CHECK(trace.str().find("\"buttons\":[]") != std::string::npos);
  w.selection.ids = {3}; click(boxes.town[1]); CHECK(sim.pending_commands().back().type == Pct::RepairMode);
  CHECK(sim.pending_commands().back().a == 1); CHECK(ui.service_target_mode() == 1);
  click(boxes.town[2]); CHECK(sim.pending_commands().back().a == 2); CHECK(ui.service_target_mode() == 2);
  Input automatic; automatic.key_hit[SDL_SCANCODE_Q] = true;
  ui.update(automatic,w,cam,1,mm,false,panel);
  CHECK(ui.service_target_mode() == 0); CHECK(sim.pending_commands().back().type == Pct::RepairMode);
  CHECK(sim.pending_commands().back().a == 0);
  click(boxes.town[2]);
  Input cancel; cancel.pressed[SDL_BUTTON_RIGHT] = true;
  const auto before = sim.pending_commands().size(); ui.update(cancel,w,cam,1,mm,true,panel);
  CHECK(ui.service_target_mode() == 0); CHECK(sim.pending_commands().size() == before);
  // Background gaps cannot dispatch an overlapping modern command.
  auto count = sim.pending_commands().size(); click({710,455,1,1}); CHECK(sim.pending_commands().size() == count);
}

static void options() {
  auto parse = [](std::vector<std::string> args, Options& options) {
    std::vector<char*> argv; for (auto& arg : args) argv.push_back(arg.data());
    std::string error; bool usage = false;
    return parse_options(int(argv.size()),argv.data(),options,error,usage);
  };
  Options o;
  CHECK(parse({"engine","--layout","original"},o)); CHECK(o.interface_layout == InterfaceLayout::Original);
  CHECK(parse({"engine","--layout","modern"},o)); CHECK(o.interface_layout == InterfaceLayout::Modern);
  CHECK(!parse({"engine","--layout","unknown"},o)); CHECK(!parse({"engine","--layout"},o));
}

int main() {
  TempDir temp;
  CHECK(set_test_env("AZTEC_USER_DIR",temp.path().string().c_str()) == 0);
  CHECK(Options{}.interface_layout == InterfaceLayout::Modern);
  options(); geometry(); cursors(); selection(); commands();
  return failures ? 1 : 0;
}
