#include "engine/app/engine.hpp"

#include <algorithm>
#include <cmath>
#include <cctype>
#include <cstdio>
#include <filesystem>

#include "engine/assets/anim_provider.hpp"
#include "engine/assets/daylight.hpp"
#include "engine/audio/audio_system.hpp"
#include "engine/platform/clock.hpp"
#include "engine/platform/input.hpp"
#include "engine/platform/window.hpp"
#include "engine/app/playtest.hpp"
#include "engine/app/sheet.hpp"
#include "engine/render/png.hpp"
#include "engine/render/scene_renderer.hpp"
#include "engine/ui/cursor.hpp"
#include "engine/ui/frontend.hpp"
#include "engine/ui/hud.hpp"
#include "engine/ui/names.hpp"
#include "engine/ui/panel.hpp"
#include "engine/sim/game_sim.hpp"
#include "engine/sim/savegame.hpp"
#include "engine/save_import.hpp"
#include "engine/ui/commands_ui.hpp"
#include "engine/ui/selection_ui.hpp"
#include "core/strbase.hpp"
#include "engine/ui/minimap.hpp"
#include "engine/world/camera.hpp"
#include "engine/world/world.hpp"

namespace fs = std::filesystem;

namespace aztec::engine {

static bool save_screenshot(SDL_Renderer* r, const std::string& path) {
  int w, h;
  SDL_GetRendererOutputSize(r, &w, &h);
  std::vector<uint8_t> px(size_t(w) * h * 4);
  if (SDL_RenderReadPixels(r, nullptr, SDL_PIXELFORMAT_RGBA32, px.data(), w * 4) != 0) return false;  // bytes R,G,B,A
  for (size_t i = 3; i < px.size(); i += 4) px[i] = 255;
  return write_png(path, w, h, px);
}

static int list_missions(const std::string& dir) {
  std::vector<std::string> names;
  std::error_code ec;
  for (const auto& e : fs::directory_iterator(find_file_ci(dir, "Maps"), ec)) {
    std::string ext = e.path().extension().string();
    for (char& c : ext) c = char(std::tolower(static_cast<unsigned char>(c)));
    if (ext == ".mp") names.push_back(e.path().stem().string());
  }
  std::sort(names.begin(), names.end());
  for (auto& n : names) std::printf("%s\n", n.c_str());
  return names.empty() ? 1 : 0;
}

enum class PlayExit { Quit, ToMenu, Restart, Next };

// Plays the sound attached to the first tick of each animation frame (frame field [2] = sound set id).
static void emit_sounds(const World& w, const SpriteProvider* provider, AudioSystem& audio, float cx, float cy) {
  const AnimSpriteProvider* ap = dynamic_cast<const AnimSpriteProvider*>(provider);
  if (!ap) return;
  for (const Object& o : w.objects) {
    if (o.removed || o.type < 0) continue;
    float x = float(o.cur_x) / 256.f, y = float(o.cur_y) / 256.f;
    if (std::fabs(x - cx) > 900 || std::fabs(y - cy) > 900) continue;
    ObjectState st;
    st.type_id = o.type; st.side = o.side; st.facing = o.facing; st.anim_state = o.anim_state; st.anim_tick = o.anim_tick;
    st.anim_variant = o.anim_variant; st.progress = o.progress;
    data::SpriteRef now = ap->frame_for(st);
    if (!now.valid() || now.sound <= 0) continue;
    bool first = o.anim_tick == 0;
    if (!first) {
      st.anim_tick = o.anim_tick - 1;
      data::SpriteRef prev = ap->frame_for(st);
      first = prev.sprite != now.sprite;
    }
    if (first) audio.play_set_at(uint32_t(now.sound), x, y, cx, cy);
  }
}

static PlayExit play_mission(Window& win, GameData& data, const Options& o, AudioSystem* audio, std::shared_ptr<const SpriteProvider> provider,
                             std::unique_ptr<Simulation> sim, const std::string& spec, bool& playtest_failed, int& rc) {
  std::string err;
  World world;
  bool loaded = o.original_save.empty() ? load_world(data, spec, world, err)
                                       : load_original_save(data, o.original_save, world, err);
  if (!loaded) { std::fprintf(stderr, "%s\n", err.c_str()); rc = 1; return PlayExit::Quit; }
  win.set_title("Aztec Empire - " + (world.mission.title.empty() ? world.mission.file_name : world.mission.title));
  PlayExit exit_reason = PlayExit::Quit;
  {
    SceneRenderer scene(win, data, world);
    scene.opts.terrain_palette = o.terrain_palette;
    scene.fog().set_enabled(o.fog);
    scene.opts.terrain_shade = o.terrain_shade;
    if (!provider && o.sprites) {
      auto ap = std::make_shared<AnimSpriteProvider>();
      std::string up = find_file_ci(o.game_dir + "/Res", "Units.xbe");
      if (!up.empty() && ap->open(up)) {
        for (const std::string& rp : {o.rules_dir, o.game_dir + "/../docs/data", std::string("docs/data")})
          if (!rp.empty() && ap->load_visual_rules(rp)) break;
        provider = ap;
      }
      else std::fprintf(stderr, "warning: cannot load Units.xbe, using placeholders\n");
    }
    scene.set_sprite_provider(provider);
    Minimap minimap;
    if (!minimap.init(win.renderer(), data, world, scene.default_palette(), o.terrain_shade))
      std::fprintf(stderr, "warning: minimap unavailable\n");
    Cursor cursor;
    cursor.init(win.renderer(), scene.store());
    Panel panel;
    const bool panel_available = panel.init(scene.store());
    bool show_panel = o.panel && panel_available;

    auto rules = std::make_shared<RulesData>();
    bool rules_ok = false;
    for (const std::string& rp : {o.rules_dir, o.game_dir + "/../docs/data", std::string("docs/data")})
      if (!rp.empty() && (rules_ok = rules->load_dir(rp))) break;
    if (!rules_ok) { rules->init_defaults(); std::fprintf(stderr, "note: docs/data not found, using placeholder rules\n"); }
    GameSim* game = nullptr;
    if (!sim) {
      if (o.wander) sim = std::make_unique<WanderSimulation>();
      else if (o.sim) { auto gs = std::make_unique<GameSim>(rules); game = gs.get(); sim = std::move(gs); }
      else sim = std::make_unique<NullSimulation>();
    }
    CommandsUi cmdui(game);
    cmdui.open_sprites(find_file_ci(o.game_dir + "/Res", "Units.xbe"));
    std::vector<KeyBinding> bindings;
    const std::string keys_path = find_file_ci(o.game_dir, "Keys.ini");
    if (!keys_path.empty() && load_keys_ini(keys_path, bindings)) cmdui.set_bindings(std::move(bindings));
    if (game && !world.original_save) {
      std::string scn = data.map_file(world.mission.file_name, "scn");
      if (!scn.empty() && !game->attach_script(scn)) std::fprintf(stderr, "warning: cannot parse %s\n", scn.c_str());
    }
    sim->init(world);
    if (world.original_save)
      std::fprintf(stderr, "Original save imported with partial resume: orders, research execution and script state are unsupported; see docs/savegame.md\n");

    StrBase strings;
    std::string sb = find_file_ci(o.game_dir + "/Res", "strbase.bds");
    if (sb.empty() || !strings.load(sb)) std::fprintf(stderr, "note: strbase.bds not loaded, using UGI names\n");
    SelectionUi selui;
    Daylight daylight;
    bool use_light = o.light && daylight.load_from_exe(find_file_ci(o.game_dir, "Aztec.exe"));
    if (o.light && !use_light) std::fprintf(stderr, "note: no daylight tables (Aztec.exe not found), neutral light\n");
    Camera cam;
    cam.set_world(float(world.width_px()), float(world.height_px()));
    int ow, oh;
    win.output_size(ow, oh);
    cam.set_view(float(ow), float(oh));  // adjusted for the panel every frame
    cam.zoom = o.zoom > 0 ? o.zoom : (oh >= 1800 ? 2.f : 1.f);
    if (o.has_cam) {
      cam.center_on((o.cam_tx + 0.5f) * kTilePx, (o.cam_ty + 0.5f) * kTilePx);
    } else if (world.original_save) {
      cam.x = float(world.original_save->view_x); cam.y = float(world.original_save->view_y);
    } else if (!world.mission.sides.empty()) {
      const SideDef& s = world.mission.sides[0];
      cam.center_on((s.start_x + 0.5f) * kTilePx, (s.start_y + 0.5f) * kTilePx);
    }

    Input in;
    PlaytestScript playtest;
    bool pt = false;
    if (!o.script.empty()) {
      if (!playtest.load(o.script, err)) { std::fprintf(stderr, "%s\n", err.c_str()); rc = 1; return PlayExit::Quit; }
      pt = true;
    }
    auto GetMouse = [&](int* x, int* y) { if (pt) { *x = in.mx; *y = in.my; } else SDL_GetMouseState(x, y); };
    std::string pending_shot;
    FixedStepper stepper;
    HudInfo hud;
    hud.show_help = o.help;
    bool edge = o.edge_scroll, paused = false, show_hud = o.hud;
    bool drag_minimap = false;
    uint32_t seen_center = 0;
    bool end_shown = false;
    uint64_t freq = SDL_GetPerformanceFrequency(), last = SDL_GetPerformanceCounter();
    double fps_acc = 0; int fps_n = 0;
    int frame = 0, shot_no = 0;

    if (o.select_x0 >= 0) {  // --select: select own units inside a screen rectangle before the first frame
      for (const Object& ob : world.objects) {
        Vec2f p = ob.interpolated(1.f);
        float sx2 = cam.to_screen_x(p.x), sy2 = cam.to_screen_y(p.y);
        if (ob.side == world.local_side && sx2 >= o.select_x0 && sx2 <= o.select_x1 && sy2 >= o.select_y0 && sy2 <= o.select_y1) world.selection.ids.push_back(ob.id);
      }
    }
    if (game && o.cmd_x >= 0) {  // --cmd-move / --cmd-attack-move: order the selection (screenshot / smoke tests)
      Command c;
      c.type = o.cmd_attack_move ? Pct::AttackMove : Pct::Move;
      c.side = uint8_t(world.local_side); c.a = o.cmd_x * kTilePx + kTilePx / 2; c.b = o.cmd_y * kTilePx + kTilePx / 2;
      c.ids = world.selection.ids;
      game->submit(c);
    }
    for (int i = 0; i < o.pre_ticks; i++) { world.begin_tick(); sim->tick(world); }
    if (audio && audio->bank_ok()) audio->start_music(world.mission.music, world.mission.music_mask, int(std::hash<std::string>()(world.mission.file_name) & 255));
    PauseMenu pause;
    bool leave = false;
    while (!in.quit && !leave) {
      uint64_t now = SDL_GetPerformanceCounter();
      double dt = pt ? 1.0 / 12 : double(now - last) / double(freq);
      last = now;
      fps_acc += dt;
      fps_n++;
      if (fps_acc >= 0.5) { hud.fps = fps_n / fps_acc; fps_acc = 0; fps_n = 0; }

      in.begin_frame();
      float sx, sy;
      win.mouse_scale(sx, sy);
      SDL_Event e;
      if (pt) {
        sx = sy = 1;
        in.focused = true; in.mouse_inside = false;   // no edge scrolling from the synthetic mouse
        PlaytestRequest req;
        bool more = playtest.step(in, world, game, req);
        for (int i = 0; i < req.run_ticks; i++) { world.begin_tick(); sim->tick(world); }
        if (!req.shot.empty()) pending_shot = req.shot;
        if (!more || req.quit) in.quit = true;
      } else {
        while (SDL_PollEvent(&e)) in.handle(e, sx, sy);
      }
      win.output_size(ow, oh);
      int ui = std::max(2, int(std::lround(oh / 360.0)));
      int cur_scale = std::max(1, int(std::lround(oh / 600.0)));
      int pscale = std::max(1, int(std::lround(oh / 600.0)));
      int view_w = ow;
      if (show_panel) {
        panel.layout(ow, oh, pscale);
        view_w = ow - panel.width();
        minimap.layout_in(panel.minimap_rect());
      } else {
        minimap.layout(ow, oh, ui);
      }
      cam.set_view(float(view_w), float(oh));

      if (in.hit(SDL_SCANCODE_ESCAPE) && cmdui.wants_left_click()) {
        cmdui.cancel_mode();
        in.key_hit[SDL_SCANCODE_ESCAPE] = false;
      }
      if (in.hit(SDL_SCANCODE_ESCAPE)) {
        if (!o.menu_session) in.quit = true;
        else if (world.outcome == kRunning) pause.open = !pause.open;
        else { exit_reason = PlayExit::ToMenu; leave = true; }
      }
      if (world.outcome != kRunning && in.hit(SDL_SCANCODE_RETURN)) {
        leave = true;
        std::string lo = world.mission.file_name;
        for (auto& ch : lo) ch = char(std::tolower(ch));
        exit_reason = (world.outcome == kVictory && o.menu_session && lo.rfind("mission_", 0) == 0) ? PlayExit::Next : PlayExit::ToMenu;
      }
      if (o.menu_session && show_panel && in.pressed[SDL_BUTTON_LEFT] && in.mx >= ow - panel.width() && in.my < 24 * pscale && in.mx < ow - panel.width() + 100 * pscale) pause.open = true;
      {
        PauseMenu::Action pa = pause.update(in, ow, oh, pscale);
        if (pa == PauseMenu::Action::Resume) pause.open = false;
        bool do_save = pa == PauseMenu::Action::Save || (in.ctrl && in.hit(SDL_SCANCODE_S));
        bool do_load = pa == PauseMenu::Action::Load || (in.ctrl && in.hit(SDL_SCANCODE_L));
        if (game && do_save && world.original_save)
          std::fprintf(stderr, "Saving imported worlds as command replay is unsupported\n");
        if (game && do_save && !world.original_save) {
          SaveData sd;
          sd.mission = spec; sd.tick = game->tick_count(); sd.hash = game->state_hash(world);
          sd.script_hash = file_hash(data.map_file(world.mission.file_name, "scn"));
          sd.cam_x = cam.x; sd.cam_y = cam.y; sd.cam_zoom = cam.zoom; sd.log = game->log();
          std::string dir = data.root() + "/Saves";
          std::error_code sec;
          fs::create_directories(dir, sec);
          std::printf(write_save(dir + "/engine_slot1.azs", sd) ? "saved game (tick %llu)\n" : "save failed (tick %llu)\n", (unsigned long long)sd.tick);
          if (pa == PauseMenu::Action::Save) pause.open = false;
        }
        if (game && do_load && world.original_save)
          std::fprintf(stderr, "Loading an engine replay into an imported world is unsupported\n");
        if (game && do_load && !world.original_save) {
          SaveData sd;
          std::string path = data.root() + "/Saves/engine_slot1.azs";
          World fresh;
          std::string lerr;
          std::string scn;
          if (read_save(path, sd) && sd.mission == spec && load_world(data, spec, fresh, lerr) &&
              (scn = data.map_file(fresh.mission.file_name, "scn"), file_hash(scn) == sd.script_hash)) {
            auto ns = GameSim::replay(fresh, rules, sd.log, sd.tick, scn);
            if (ns->state_hash(fresh) != sd.hash) std::fprintf(stderr, "warning: replayed state differs from the saved hash\n");
            world = std::move(fresh);
            game = ns.get();
            sim = std::move(ns);
            cmdui.set_sim(game);
            cam.x = sd.cam_x; cam.y = sd.cam_y; cam.zoom = sd.cam_zoom;
            world.selection.clear();
            world.fog_version++;
            pause.open = false;
            std::printf("loaded game (tick %llu)\n", (unsigned long long)sd.tick);
          } else {
            std::fprintf(stderr, "load failed (no save for this mission?)\n");
          }
        }
        if (pa == PauseMenu::Action::Restart) { exit_reason = PlayExit::Restart; leave = true; }
        if (pa == PauseMenu::Action::Menu) { exit_reason = PlayExit::ToMenu; leave = true; }
        if (pa == PauseMenu::Action::Quit) { exit_reason = PlayExit::Quit; in.quit = true; }
        if (pause.open) {   // the world ignores input while the pause menu is up
          for (int b = 0; b < 6; b++) { in.pressed[b] = in.released[b] = in.buttons[b] = false; }
          std::fill(std::begin(in.key_hit), std::end(in.key_hit), false);
          in.wheel = 0;
        }
      }
      if (in.hit(SDL_SCANCODE_F11)) win.cycle_borderless();
      if (in.hit(SDL_SCANCODE_RETURN) && in.alt) win.toggle_fullscreen();
      if (in.hit(SDL_SCANCODE_F1)) hud.show_help = !hud.show_help;
      if (in.hit(SDL_SCANCODE_F2)) scene.opts.grid = !scene.opts.grid;
      if (in.hit(SDL_SCANCODE_F3)) scene.opts.passability = !scene.opts.passability;
      if (in.hit(SDL_SCANCODE_F4)) scene.opts.labels = !scene.opts.labels;
      if (in.hit(SDL_SCANCODE_F5)) scene.opts.markers = !scene.opts.markers;
      if (in.hit(SDL_SCANCODE_F6)) edge = !edge;
      if (in.hit(SDL_SCANCODE_F7)) scene.opts.overlays = !scene.opts.overlays;
      if (in.hit(SDL_SCANCODE_F8)) scene.fog().set_enabled(!scene.fog().enabled());
      if (in.hit(SDL_SCANCODE_F10)) show_hud = !show_hud;
      if (in.hit(SDL_SCANCODE_SPACE) && !cmdui.key_bound(SDL_SCANCODE_SPACE)) paused = !paused;
      if (panel_available && cmdui.action_hit(in, "ToggleInterface")) show_panel = !show_panel;
      if (cmdui.action_hit(in, "PrintScreen")) {
        std::string p = "aztec_shot_" + std::to_string(++shot_no) + ".png";
        std::printf(save_screenshot(win.renderer(), p) ? "saved %s\n" : "screenshot failed %s\n", p.c_str());
      }

      // Fixed 12 Hz simulation, independent of frame rate.
      if (!paused && !pause.open) {
        int n = stepper.advance(dt);
        for (int i = 0; i < n; i++) {
          world.begin_tick();
          sim->tick(world);
          if (audio && audio->bank_ok()) emit_sounds(world, provider.get(), *audio, cam.x + cam.visible_w() / 2, cam.y + cam.visible_h() / 2);
        }
      }
      float alpha = (paused || pause.open) ? 1.f : float(stepper.alpha());

      // Script requests: centerView / cameraActivate, mission end.
      if (world.center_serial != seen_center) { seen_center = world.center_serial; cam.center_on(float(world.center_x), float(world.center_y)); }
      if (world.outcome != kRunning && !end_shown) { end_shown = true; paused = true; }
      // Camera input.
      CameraInput ci;
      bool nokeys = in.ctrl;
      if (!nokeys && cmdui.action_down(in, "ScrollLeft")) ci.move_x -= 1;
      if (!nokeys && cmdui.action_down(in, "ScrollRight")) ci.move_x += 1;
      if (!nokeys && cmdui.action_down(in, "ScrollUp")) ci.move_y -= 1;
      if (!nokeys && cmdui.action_down(in, "ScrollDown")) ci.move_y += 1;
      if (in.hit(SDL_SCANCODE_EQUALS)) ci.zoom_steps += 1;
      if (in.hit(SDL_SCANCODE_KP_PLUS)) ci.zoom_steps += 1;
      if (in.hit(SDL_SCANCODE_MINUS) || in.hit(SDL_SCANCODE_KP_MINUS)) ci.zoom_steps -= 1;
      bool on_mm = minimap.contains(in.mx, in.my);
      bool on_panel = show_panel && in.mx >= ow - panel.width();
      if (!on_mm && !on_panel && in.wheel != 0) { ci.zoom_steps += in.wheel > 0 ? 1 : -1; }
      ci.zoom_pivot_x = on_mm || ci.zoom_steps == 0 || in.wheel == 0 ? view_w / 2.f : float(in.mx);
      ci.zoom_pivot_y = on_mm || ci.zoom_steps == 0 || in.wheel == 0 ? oh / 2.f : float(in.my);
      int edge_dx = 0, edge_dy = 0;
      if (edge && o.screenshot.empty() && in.focused && in.mouse_inside && !drag_minimap) {
        int z = std::max(2, ui);
        if (in.mx < z) edge_dx = -1;
        if (in.mx >= ow - z) edge_dx = 1;
        if (in.my < z) edge_dy = -1;
        if (in.my >= oh - z) edge_dy = 1;
        ci.move_x += edge_dx;
        ci.move_y += edge_dy;
      }
      ci.move_x = std::clamp(ci.move_x, -1.f, 1.f);
      ci.move_y = std::clamp(ci.move_y, -1.f, 1.f);
      bool pan = in.buttons[SDL_BUTTON_MIDDLE] || cmdui.right_dragging();
      ci.dragging = pan;
      ci.drag_dx = float(in.dx);
      ci.drag_dy = float(in.dy);

      if (in.pressed[SDL_BUTTON_LEFT] && on_mm && !cmdui.wants_left_click()) drag_minimap = true;
      if (!in.buttons[SDL_BUTTON_LEFT]) drag_minimap = false;
      // Selection (left button in the world view).
      {
        bool in_world = !on_mm && !on_panel && !drag_minimap && !pan;
        bool consumed = cmdui.update(in, world, cam, alpha, minimap, in_world, show_panel ? panel.rect() : SDL_Rect{ow, 0, 0, oh});
        Input selection_input = in;
        selection_input.shift = cmdui.action_down(in, "Add");
        selui.update(selection_input, world, cam, alpha, in_world && !consumed && !cmdui.wants_left_click());
      }
      cam.update(ci, float(std::min(dt, 0.1)));
      if (drag_minimap) {
        int mx = std::clamp(in.mx, minimap.rect().x, minimap.rect().x + minimap.rect().w - 1);
        int my = std::clamp(in.my, minimap.rect().y, minimap.rect().y + minimap.rect().h - 1);
        float wx, wy;
        minimap.to_world(mx, my, wx, wy);
        cam.center_on(wx, wy);
      }

      if (audio) audio->update();
      // Render. Day clock: mission StartTime + simulation ticks (+ interpolation).
      if (use_light) {
        double t = (o.time >= 0 ? o.time : world.mission.start_time) + (o.time >= 0 ? 0 : double(world.tick) + alpha);
        scene.set_light(daylight.sample(world.mission.tileset, t));
      }
      minimap.set_tint(scene.light_tint());
      scene.draw(cam, alpha);
      if (!show_panel) minimap.draw(win.renderer(), cam, alpha, &scene.fog());
      hud.mouse_x = in.mx; hud.mouse_y = in.my; hud.alpha = alpha;
      hud.sprites = scene.stats.sprites; hud.placeholders = scene.stats.placeholders; hud.geometry_calls = scene.stats.geometry_calls;
      selui.draw_box(win.renderer());
      pause.draw(win.renderer(), scene.font(), ow, oh, pscale);
      {  // script messages (printString) and end-of-mission banner
        int ty = 40 * ui;
        for (const Message& m : world.messages) {
          int tw = scene.font().width(m.text, ui);
          scene.font().draw(win.renderer(), (ow - panel.width() - tw) / 2, ty, m.text, ui, {255, 255, 200, 255});
          ty += 8 * ui;
        }
        if (world.outcome != kRunning) {
          std::string t = world.outcome == kVictory ? "MISSION COMPLETE" : "MISSION FAILED";
          int sc2 = ui * 2, tw = scene.font().width(t, sc2);
          scene.font().draw(win.renderer(), (ow - panel.width() - tw) / 2, oh / 3, t, sc2, world.outcome == kVictory ? SDL_Color{120, 255, 120, 255} : SDL_Color{255, 110, 110, 255});
          std::string t2 = o.menu_session ? "ENTER: CONTINUE   ESC: MENU" : "PRESS ESC TO LEAVE";
          scene.font().draw(win.renderer(), (ow - panel.width() - scene.font().width(t2, ui)) / 2, oh / 3 + 18 * ui, t2, ui, {230, 230, 230, 255});
        }
      }
      {
        int gx, gy;
        GetMouse(&gx, &gy);
        cmdui.draw_ghost(win.renderer(), world, cam, int(gx * sx), int(gy * sy));
      }
      if (show_panel) {
        int gold = 0;
        for (const SideDef& sd : world.mission.sides) if (sd.index == world.local_side) gold = sd.money;
        std::string tip;
        {
          int mx2, my2;
          GetMouse(&mx2, &my2);
          mx2 = int(mx2 * sx); my2 = int(my2 * sy);
          if (mx2 < ow - panel.width()) {
            const Object* h = pick_object(world, cam.to_world_x(float(mx2)), cam.to_world_y(float(my2)), alpha, 24.f / cam.zoom + 8);
            if (h && h->type >= 0) {
              tip = type_display_name(strings, h->type);
              const TypeRules& tr = rules->type(h->type);
              if (tr.price > 0 && strings.entry(217)) tip = format_game_string(strings.get(217), tip, std::to_string(tr.price), std::to_string(tr.time));
            }
          }
        }
        const SideState* mss = world.local_side < int(world.side_state.size()) ? &world.side_state[world.local_side] : nullptr;
        if (mss) gold = mss->money;
        panel.draw(win.renderer(), scene.store(), scene.font(), gold);
        {
          int mx3, my3;
          GetMouse(&mx3, &my3);
          std::string mt = cmdui.draw_menu(win.renderer(), scene.store(), scene.font(), strings, world, panel.rect(), pscale, int(mx3 * sx), int(my3 * sy));
          if (!mt.empty()) tip = mt;
        }
        if (!tip.empty()) panel.draw_tooltip(win.renderer(), scene.font(), tip);
        minimap.draw(win.renderer(), cam, alpha, &scene.fog());
      }
      if (show_hud) draw_hud(win.renderer(), scene.font(), world, cam, hud, ui);
      // Cursor last, at the freshest mouse position.
      int cx, cy;
      GetMouse(&cx, &cy);
      cx = int(cx * sx); cy = int(cy * sy);
      bool forced = !o.screenshot.empty() && o.cursor_x >= 0;
      if (forced) { cx = o.cursor_x; cy = o.cursor_y; }
      bool visible = in.mouse_inside || o.screenshot.empty();
      CursorKind ck = CursorKind::Arrow;
      if (pan) ck = CursorKind::Pan;
      else if (edge_dx || edge_dy) ck = Cursor::scroll_kind(edge_dx, edge_dy);
      else if (cmdui.attack_move()) ck = CursorKind::Attack;
      else if (!on_mm && !(show_panel && cx >= ow - panel.width())) {
        const Object* h = pick_object(world, cam.to_world_x(float(cx)), cam.to_world_y(float(cy)), alpha, 24.f / cam.zoom + 8);
        if (h && h->side != world.local_side) ck = CursorKind::Attack;
      }
      if (visible && (o.screenshot.empty() || forced)) cursor.draw(win.renderer(), scene.store(), cx, cy, ck, cur_scale, SDL_GetTicks());

      if (!pending_shot.empty()) {
        std::printf(save_screenshot(win.renderer(), pending_shot) ? "saved %s\n" : "screenshot failed %s\n", pending_shot.c_str());
        pending_shot.clear();
      }
      frame++;
      if (!o.screenshot.empty() && frame >= o.frames) {
        if (!save_screenshot(win.renderer(), o.screenshot)) { std::fprintf(stderr, "screenshot failed\n"); rc = 1; }
        else std::printf("saved %s (%dx%d)\n", o.screenshot.c_str(), ow, oh);
        break;
      }
      SDL_RenderPresent(win.renderer());
      int cap = o.fps_cap > 0 ? o.fps_cap : (win.vsync() ? 0 : 240);
      if (cap > 0) {
        double spent = double(SDL_GetPerformanceCounter() - now) / double(freq);
        double want = 1.0 / cap;
        if (spent < want) SDL_Delay(Uint32((want - spent) * 1000));
      }
    }
    if (pt && playtest.failures() > 0) playtest_failed = true;
  }
  if (audio) audio->stop_music();
  return exit_reason;
}

int run_engine(const Options& o, std::shared_ptr<const SpriteProvider> provider, std::unique_ptr<Simulation> sim) {
  if (o.list) return list_missions(o.game_dir);
  if (!o.sheet_spec.empty()) {
    std::string e;
    if (!write_sprite_sheet(o.game_dir, o.sheet_spec, o.sheet_out, e)) { std::fprintf(stderr, "%s\n", e.c_str()); return 1; }
    return 0;
  }
  GameData data;
  std::string err;
  if (!data.open(o.game_dir, err)) { std::fprintf(stderr, "%s\n", err.c_str()); return 1; }
  SDL_SetMainReady();
  if (SDL_Init(SDL_INIT_VIDEO) != 0) { std::fprintf(stderr, "SDL_Init: %s\n", SDL_GetError()); return 1; }
  int rc = 0;
  bool playtest_failed = false;
  {
    Window win;
    WindowOptions wo = o.window;
    wo.title = "Aztec Empire";
    if (!win.create(wo)) { SDL_Quit(); return 1; }
    Options opts = o;
    opts.menu_session = o.menu;
    std::string spec = o.mission;
    bool menu = o.menu;
    std::string open_brief;
    AudioSystem audio;
    AudioSystem* audio_ptr = nullptr;
    if (o.audio) { audio.init(o.game_dir); audio.set_volumes(o.sfx_volume, o.music_volume); audio_ptr = &audio; }
    for (;;) {
      if (menu) {
        FrontendResult fr;
        if (!run_frontend(win, data, opts, fr, open_brief)) break;
        spec = fr.mission;
        open_brief.clear();
        menu = false;
      }
      PlayExit pe = play_mission(win, data, opts, audio_ptr, provider, std::move(sim), spec, playtest_failed, rc);
      sim.reset();
      if (pe == PlayExit::Quit || rc != 0) break;
      if (pe == PlayExit::ToMenu) { opts.original_save.clear(); menu = true; continue; }
      if (pe == PlayExit::Restart) continue;
      if (pe == PlayExit::Next) {
        std::string lo = spec;
        for (auto& ch : lo) ch = char(std::tolower(ch));
        int n = std::atoi(lo.c_str() + 8);
        if (n >= 1 && n < 15) { open_brief = "mission_" + std::to_string(n + 1); menu = true; continue; }
        menu = true;
        continue;
      }
      break;
    }
  }
  SDL_Quit();
  if (playtest_failed) rc = 3;
  return rc;
}

}  // namespace aztec::engine
