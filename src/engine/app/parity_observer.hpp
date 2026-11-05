// Opt-in shared L0 observations from the headless simulation driver.
#pragma once
#include "parity/schedule.h"
#include "engine/assets/anim_provider.hpp"
#include "engine/assets/daylight.hpp"
#include "engine/render/scene_renderer.hpp"
#include "engine/ui/panel.hpp"
#include "engine/ui/minimap.hpp"
#include "engine/ui/cursor.hpp"
#include "engine/ui/commands_ui.hpp"
#include "engine/ui/hud.hpp"
#include "core/strbase.hpp"
#include <filesystem>
#include <fstream>
#include <stdexcept>

namespace aztec::engine {
class ParityObserver {
 public:
  ~ParityObserver() { free(schedule_.ticks); }
  void open(const std::string& schedule, const std::string& output, const std::string& frames,
            const GameData& data, World& world, GameSim& sim, const std::string& rules) {
    if (!l0_schedule_read(schedule.c_str(), &schedule_)) throw std::runtime_error("invalid shared L0 schedule");
    frames_ = frames;
    if (!std::filesystem::is_directory(frames_)) throw std::runtime_error("L0 frame directory does not exist");
    out_.open(output);
    if (!out_) throw std::runtime_error("cannot open shared L0 observations");
    out_ << "{\"schema\":2,\"phase\":\"before_commands\",\"position_unit\":\"pixel/256\"}\n";
    if (SDL_InitSubSystem(SDL_INIT_VIDEO) != 0) throw std::runtime_error(SDL_GetError());
    WindowOptions options;
    options.width = 800; options.height = 600; options.software = true; options.vsync = false;
    if (!window_.create(options)) throw std::runtime_error("cannot create L0 renderer");
    scene_ = std::make_unique<SceneRenderer>(window_, data, world);
    auto sprites = std::make_shared<AnimSpriteProvider>();
    if (sprites->open(find_file_ci(data.root(), "Res/Units.xbe"))) {
      sprites->load_visual_rules(rules);
      scene_->set_sprite_provider(sprites);
    }
    panel_ok_ = panel_.init(scene_->store());
    panel_.layout(800, 600, 1);
    minimap_.init(window_.renderer(), data, world, scene_->default_palette(), scene_->opts.terrain_shade);
    minimap_.layout_in(panel_.minimap_rect());
    cursor_.init(window_.renderer(), scene_->store());
    commands_ = std::make_unique<CommandsUi>(&sim);
    commands_->open_sprites(find_file_ci(data.root(), "Res/Units.xbe"));
    strings_.load(find_file_ci(data.root(), "Res/strbase.bds"));
    light_ok_ = daylight_.load_from_exe(find_file_ci(data.root(), "Aztec.exe"));
  }
  uint32_t last_tick() const { return schedule_.ticks[schedule_.count - 1]; }
  void observe(uint32_t tick, World& w) {
    if (schedule_.next == schedule_.count || tick < schedule_.ticks[schedule_.next]) return;
    if (tick != schedule_.ticks[schedule_.next]) throw std::runtime_error("shared L0 missed tick");
    if (w.side_state.size() < 4) throw std::runtime_error("L0 requires four side slots");
    w.local_side = schedule_.side;
    w.selection.clear();
    Camera camera;
    camera.world_w = float(w.width_px()); camera.world_h = float(w.height_px());
    camera.view_w = float(800 - panel_.width()); camera.view_h = 600;
    camera.x = float(schedule_.x); camera.y = float(schedule_.y); camera.zoom = 1;
    if (light_ok_) scene_->set_light(daylight_.sample(w.mission.tileset, w.mission.start_time + double(w.tick)));
    scene_->draw(camera, 1.f);
    int cash = schedule_.side < int(w.side_state.size()) ? w.side_state[size_t(schedule_.side)].money : 0;
    panel_.draw(window_.renderer(), scene_->store(), scene_->font(), cash);
    commands_->draw_menu(window_.renderer(), scene_->store(), scene_->font(), strings_, w, panel_.rect(), 1, 400, 300);
    minimap_.set_tint(scene_->light_tint());
    minimap_.draw(window_.renderer(), camera, 1.f, &scene_->fog());
    draw_messages(window_.renderer(), scene_->font(), w.messages, SDL_Rect{8, 40, int(camera.view_w) - 16, 150}, 1);
    cursor_.draw(window_.renderer(), scene_->store(), 400, 300, CursorKind::Arrow, 1, uint32_t(w.tick * 1000 / 12));
    std::vector<uint8_t> pixels(800 * 600 * 3);
    if (SDL_RenderReadPixels(window_.renderer(), nullptr, SDL_PIXELFORMAT_RGB24, pixels.data(), 800 * 3))
      throw std::runtime_error("cannot read L0 frame");
    std::ofstream frame(std::filesystem::path(frames_) / ("tick_" + std::to_string(tick) + ".ppm"), std::ios::binary);
    frame << "P6\n800 600\n255\n";
    frame.write(reinterpret_cast<const char*>(pixels.data()), std::streamsize(pixels.size()));
    frame.close();
    if (!frame) throw std::runtime_error("cannot write L0 frame");
    out_ << "{\"tick\":" << tick << ",\"objects\":[";
    bool first = true;
    constexpr const char* names[] = {"idle", "move", "attack", "attack_move", "follow"};
    for (const auto& o : w.objects) if (o.alive && !o.removed) {
      if (!first) out_ << ',';
      first = false;
      if (o.order >= 5) throw std::runtime_error("unsupported L0 order");
      out_ << "{\"handle\":" << o.id << ",\"type\":" << o.type << ",\"owner\":" << o.side
           << ",\"x\":" << o.cur_x << ",\"y\":" << o.cur_y << ",\"hp\":" << o.hp
           << ",\"orders\":{\"kind\":\"" << names[o.order] << "\",\"target\":" << o.target
           << ",\"x\":" << o.dest_x << ",\"y\":" << o.dest_y << "}}";
    }
    out_ << "],\"money\":{";
    for (size_t side = 0; side < 4; ++side) {
      if (side) out_ << ',';
      out_ << '"' << side << "\":" << w.side_state[side].money;
    }
    out_ << "},\"queues\":{";
    for (size_t side = 0; side < 4; ++side) {
      if (side) out_ << ',';
      out_ << '"' << side << "\":[";
      bool first_item = true;
      for (const auto& q : w.side_state[side].queue) {
        if (!first_item) out_ << ',';
        first_item = false;
        out_ << "{\"type\":" << q.type << ",\"town\":" << q.town << ",\"remaining\":" << q.remaining
             << ",\"total\":" << q.total << ",\"paid\":" << q.paid << ",\"started\":" << (q.started ? "true" : "false")
             << ",\"paused\":" << (q.paused ? "true" : "false") << '}';
      }
      out_ << ']';
    }
    out_ << "},\"view\":{\"camera\":[" << schedule_.x << ',' << schedule_.y
         << ",1],\"resolution\":[800,600],\"side\":" << schedule_.side
         << ",\"selection\":[],\"panel\":true,\"cursor\":[400,300]},\"frame\":\"clone/tick_" << tick
         << ".ppm\",\"unavailable\":[" << (panel_ok_ ? "" : "\"panel_assets\"") << "]}\n";
    out_.flush();
    if (!out_) throw std::runtime_error("cannot write L0 observations");
    ++schedule_.next;
  }
 private:
  L0Schedule schedule_{};
  std::ofstream out_;
  std::string frames_;
  Window window_;
  std::unique_ptr<SceneRenderer> scene_;
  Panel panel_;
  Minimap minimap_;
  Cursor cursor_;
  std::unique_ptr<CommandsUi> commands_;
  StrBase strings_;
  Daylight daylight_;
  bool light_ok_ = false, panel_ok_ = false;
};
}  // namespace aztec::engine
