// Read-only mission inspector. No world creation or simulation is needed.
#include <SDL2/SDL.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include "core/map.hpp"
#include "core/visfile.hpp"
#include "engine/assets/game_data.hpp"
#include "engine/assets/mission.hpp"
#include "engine/render/font.hpp"
#include "engine/world/camera.hpp"

namespace fs = std::filesystem;
using namespace aztec;
using namespace aztec::engine;

namespace {
std::string lower(std::string s) {
  for (char& c : s) c = char(std::tolower(static_cast<unsigned char>(c)));
  return s;
}

struct Inspection {
  MissionDef mission;
  TerrainMap terrain;
  VisFile fog;
};

Inspection load(const std::string& path) {
  Inspection result;
  auto& m = result.mission;
  if (!load_mission(path, m)) throw std::runtime_error("Cannot load mission: " + path);
  std::string dir = fs::path(path).parent_path().string();
  std::string terrain = find_file_ci(dir, m.file_name + ".map");
  if (terrain.empty()) terrain = find_file_ci(dir, m.map_name + ".map");
  if (!load_terrain_map(terrain, result.terrain))
    throw std::runtime_error("Cannot load terrain for: " + path);
  // Missing fog is legitimate for custom maps. Present but invalid fog is an error.
  std::string fog = find_file_ci(dir, m.file_name + ".vis");
  if (!fog.empty() && !load_vis(fog, result.fog))
    throw std::runtime_error("Cannot load fog: " + fog);
  return result;
}

const char* source_name(ObjectSource source) {
  switch (source) {
    case ObjectSource::Town: return "town";
    case ObjectSource::Team: return "team";
    case ObjectSource::Boat: return "boat";
    case ObjectSource::Johnka: return "johnka";
    case ObjectSource::Submarine: return "submarine";
    case ObjectSource::Start: return "start";
  }
  return "unknown";
}

void dump(const Inspection& data) {
  const auto& m = data.mission;
  const auto& t = data.terrain;
  std::cout << "mission " << std::quoted(m.file_name) << "\nterrain " << t.w << ' ' << t.h
            << " tileset " << m.tileset << "\n";
  for (int y = 0; y < t.h; ++y) {
    std::cout << "terrain-row " << y;
    for (int x = 0; x < t.w; ++x) std::cout << ' ' << t.base(x, y) << ':' << t.overlay(x, y);
    std::cout << '\n';
  }
  for (const auto& s : m.sides)
    std::cout << "start side " << s.index << " tile " << s.start_x << ' ' << s.start_y << '\n';
  for (const auto& twn : m.towns)
    std::cout << "town " << twn.index << " side " << twn.side << " tile " << twn.x << ' ' << twn.y
              << " type " << twn.type << " scenario " << twn.scenario_id << '\n';
  for (const auto& o : m.objects)
    std::cout << "object " << source_name(o.source) << " side " << o.side << " tile " << o.x << ' ' << o.y
              << " type " << o.type << " hp " << o.hp << " group " << o.group
              << " scenario " << o.scenario_id << " defence " << o.defence << '\n';
  std::cout << "fog-layers " << data.fog.sides.size() << '\n';
  for (size_t i = 0; i < data.fog.sides.size(); ++i) {
    const auto& f = data.fog.sides[i];
    std::cout << "fog " << i << ' ' << f.w << ' ' << f.h << '\n';
    for (int y = 0; y < f.h; ++y) {
      std::cout << "fog-row " << i << ' ' << y;
      for (int x = 0; x < f.w; ++x) std::cout << ' ' << unsigned(f.v[size_t(y) * f.w + x]);
      std::cout << '\n';
    }
  }
  std::cout << "end-mission\n";
}

SDL_Color side_color(int side) {
  static constexpr SDL_Color colors[] = {{240, 80, 80, 255}, {80, 160, 255, 255},
    {80, 240, 120, 255}, {255, 220, 80, 255}, {220, 100, 255, 255},
    {80, 240, 240, 255}, {255, 160, 80, 255}, {240, 240, 240, 255}};
  return colors[unsigned(side) % 8];
}

int view(const std::vector<std::string>& paths, size_t index, const std::string& game, int frames) {
  Inspection data = load(paths[index]);
  if (SDL_Init(SDL_INIT_VIDEO) != 0) throw std::runtime_error(SDL_GetError());
  struct Quit { ~Quit() { SDL_Quit(); } } quit;
  std::unique_ptr<SDL_Window, decltype(&SDL_DestroyWindow)> window(
    SDL_CreateWindow("Quetzal map inspector", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                     1280, 720, SDL_WINDOW_RESIZABLE), SDL_DestroyWindow);
  if (!window) throw std::runtime_error(SDL_GetError());
  SDL_Renderer* raw = SDL_CreateRenderer(window.get(), -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
  if (!raw) raw = SDL_CreateRenderer(window.get(), -1, SDL_RENDERER_SOFTWARE);
  std::unique_ptr<SDL_Renderer, decltype(&SDL_DestroyRenderer)> renderer(raw, SDL_DestroyRenderer);
  if (!renderer) throw std::runtime_error(SDL_GetError());
  SDL_SetRenderDrawBlendMode(raw, SDL_BLENDMODE_BLEND);
  Font font;
  if (!font.init(raw)) throw std::runtime_error("Cannot initialize inspector font");
  GameData assets;
  std::string error;
  std::unique_ptr<SpriteStore> sprites;
  if (assets.open(game, error)) sprites = std::make_unique<SpriteStore>(raw, assets);
  else std::cerr << "Terrain textures unavailable; using tile ID colors. " << error << '\n';
  Camera camera;
  auto reset = [&] {
    camera = Camera{};
    camera.set_world(float(data.terrain.w * 48), float(data.terrain.h * 48));
    camera.center_on(camera.world_w / 2, camera.world_h / 2);
  };
  reset();
  bool running = true, objects = true, starts = true, overlays = true;
  int fog = -1, selected_x = -1, selected_y = -1, rendered = 0;
  Uint64 previous = SDL_GetPerformanceCounter();
  while (running) {
    int width, height;
    SDL_GetRendererOutputSize(raw, &width, &height);
    camera.set_view(float(width), float(height));
    CameraInput input;
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
      if (e.type == SDL_QUIT) running = false;
      if (e.type == SDL_KEYDOWN && !e.key.repeat) {
        auto key = e.key.keysym.sym;
        if (key == SDLK_ESCAPE) running = false;
        if (key == SDLK_o) objects = !objects;
        if (key == SDLK_s) starts = !starts;
        if (key == SDLK_t) overlays = !overlays;
        if (key == SDLK_f) fog = (fog + 2) % (int(data.fog.sides.size()) + 1) - 1;
        if (key == SDLK_HOME) reset();
        if (key == SDLK_PAGEUP || key == SDLK_PAGEDOWN) {
          size_t next = key == SDLK_PAGEDOWN ? (index + 1) % paths.size() : (index + paths.size() - 1) % paths.size();
          try {
            Inspection replacement = load(paths[next]);
            data = std::move(replacement);
            index = next;
            fog = selected_x = selected_y = -1;
            error.clear();
            reset();
          } catch (const std::exception& ex) {
            // Advance the browser even on bad entries so every mission remains reachable.
            index = next;
            error = ex.what();
          }
        }
      }
      if (e.type == SDL_MOUSEWHEEL) {
        int mx, my;
        SDL_GetMouseState(&mx, &my);
        input.zoom_steps = float(e.wheel.y * (e.wheel.direction == SDL_MOUSEWHEEL_FLIPPED ? -1 : 1));
        input.zoom_pivot_x = float(mx); input.zoom_pivot_y = float(my);
      }
      if (e.type == SDL_MOUSEMOTION && (e.motion.state & SDL_BUTTON_MMASK)) {
        input.dragging = true;
        input.drag_dx += float(e.motion.xrel); input.drag_dy += float(e.motion.yrel);
      }
      if (e.type == SDL_MOUSEBUTTONDOWN && e.button.button == SDL_BUTTON_LEFT) {
        selected_x = int(std::floor(camera.to_world_x(float(e.button.x)) / 48));
        selected_y = int(std::floor(camera.to_world_y(float(e.button.y)) / 48));
      }
    }
    const Uint8* keys = SDL_GetKeyboardState(nullptr);
    input.move_x = float(keys[SDL_SCANCODE_RIGHT] - keys[SDL_SCANCODE_LEFT]);
    input.move_y = float(keys[SDL_SCANCODE_DOWN] - keys[SDL_SCANCODE_UP]);
    Uint64 now = SDL_GetPerformanceCounter();
    camera.update(input, std::min(0.1f, float(now - previous) / float(SDL_GetPerformanceFrequency())));
    previous = now;
    SDL_SetRenderDrawColor(raw, 20, 24, 28, 255);
    SDL_RenderClear(raw);
    const auto& t = data.terrain;
    int x0 = std::max(0, int(std::floor(camera.x / 48)));
    int y0 = std::max(0, int(std::floor(camera.y / 48)));
    int x1 = std::min(t.w, int(std::ceil((camera.x + camera.visible_w()) / 48)));
    int y1 = std::min(t.h, int(std::ceil((camera.y + camera.visible_h()) / 48)));
    auto rect = [&](int x, int y) {
      return SDL_FRect{camera.to_screen_x(float(x) * 48), camera.to_screen_y(float(y) * 48), 48 * camera.zoom, 48 * camera.zoom};
    };
    ArchiveId archive = data.mission.tileset == 1 ? ArchiveId::Tiles1 : ArchiveId::Tiles0;
    for (int y = y0; y < y1; ++y) for (int x = x0; x < x1; ++x) {
      SDL_FRect dst = rect(x, y);
      uint16_t id = t.base(x, y);
      SDL_SetRenderDrawColor(raw, Uint8(40 + id * 37 % 160), Uint8(40 + id * 61 % 160), Uint8(40 + id * 83 % 160), 255);
      SDL_RenderFillRectF(raw, &dst);
      if (sprites) {
        const auto& tile = sprites->get({archive, 0x44c, id});
        if (tile.ok()) {
          SDL_Rect src = tile.frame_rect(0);
          SDL_RenderCopyF(raw, sprites->atlas().page(tile.region.page), &src, &dst);
        }
      }
      if (overlays && t.overlay(x, y)) {
        SDL_SetRenderDrawColor(raw, 190, 240, 190, 255);
        SDL_FRect marker{dst.x + dst.w / 3, dst.y + dst.h / 3, dst.w / 3, dst.h / 3};
        SDL_RenderFillRectF(raw, &marker);
      }
      if (fog >= 0) {
        const auto& layer = data.fog.sides[size_t(fog)];
        // Fog grids need not have the terrain dimensions: scale their actual extent.
        int fx = int(int64_t(x) * layer.w / t.w), fy = int(int64_t(y) * layer.h / t.h);
        Uint8 value = layer.v[size_t(fy) * layer.w + fx];
        SDL_SetRenderDrawColor(raw, 0, 0, 0, Uint8(std::min(230, int(value) * 230 / 152)));
        SDL_RenderFillRectF(raw, &dst);
      }
    }
    auto marker = [&](int x, int y, int side, const std::string& label, bool cross) {
      SDL_FRect dst = rect(x, y);
      if (dst.x + dst.w < 0 || dst.y + dst.h < 0 || dst.x >= width || dst.y >= height) return;
      SDL_Color c = side_color(side);
      SDL_SetRenderDrawColor(raw, c.r, c.g, c.b, 255);
      if (cross) {
        SDL_RenderDrawLineF(raw, dst.x, dst.y, dst.x + dst.w, dst.y + dst.h);
        SDL_RenderDrawLineF(raw, dst.x + dst.w, dst.y, dst.x, dst.y + dst.h);
      } else SDL_RenderDrawRectF(raw, &dst);
      font.draw(raw, int(dst.x + 2), int(dst.y + 2), label, 2, c);
    };
    if (objects) {
      for (const auto& town : data.mission.towns) marker(town.x, town.y, town.side, "T" + std::to_string(town.index), false);
      for (const auto& o : data.mission.objects) marker(o.x, o.y, o.side, std::to_string(o.type), false);
    }
    if (starts) for (const auto& s : data.mission.sides)
      if (s.start_x >= 0 && s.start_y >= 0) marker(s.start_x, s.start_y, s.index, "S" + std::to_string(s.index), true);
    std::vector<std::string> lines{
      "Mission " + std::to_string(index + 1) + "/" + std::to_string(paths.size()) + ": " + fs::path(paths[index]).filename().string()
        + "  Loaded: " + data.mission.file_name,
      "PageUp/PageDown missions  Arrows/Mouse middle drag scroll  Wheel zoom  Home reset  Esc quit",
      "O objects: " + std::to_string(objects) + "  S starts: " + std::to_string(starts) + "  T overlays: " + std::to_string(overlays)
        + "  F fog: " + (fog < 0 ? "off" : std::to_string(fog)) + "  Click tile to inspect",
      "Terrain " + std::to_string(t.w) + "x" + std::to_string(t.h) + "  Fog layers " + std::to_string(data.fog.sides.size())
        + (sprites ? "  Textures loaded" : "  Tile ID colors")};
    if (selected_x >= 0 && selected_y >= 0 && selected_x < t.w && selected_y < t.h) {
      marker(selected_x, selected_y, 7, "", false);
      lines.push_back("Tile " + std::to_string(selected_x) + "," + std::to_string(selected_y)
        + " base " + std::to_string(t.base(selected_x, selected_y)) + " overlay " + std::to_string(t.overlay(selected_x, selected_y)));
      for (const auto& o : data.mission.objects) if (o.x == selected_x && o.y == selected_y)
        lines.push_back(std::string(source_name(o.source)) + " type " + std::to_string(o.type) + " side " + std::to_string(o.side)
          + " hp " + std::to_string(o.hp) + " group " + std::to_string(o.group) + " scenario " + std::to_string(o.scenario_id));
      for (const auto& town : data.mission.towns) if (town.x == selected_x && town.y == selected_y)
        lines.push_back("Town " + std::to_string(town.index) + " type " + std::to_string(town.type)
          + " side " + std::to_string(town.side) + " scenario " + std::to_string(town.scenario_id));
      for (const auto& s : data.mission.sides) if (s.start_x == selected_x && s.start_y == selected_y)
        lines.push_back("Start side " + std::to_string(s.index));
      for (size_t i = 0; i < data.fog.sides.size(); ++i) {
        const auto& f = data.fog.sides[i];
        int fx = int(int64_t(selected_x) * f.w / t.w), fy = int(int64_t(selected_y) * f.h / t.h);
        lines.push_back("Fog " + std::to_string(i) + " value " + std::to_string(f.v[size_t(fy) * f.w + fx]));
      }
    }
    if (!error.empty()) lines.push_back(error);
    SDL_SetRenderDrawColor(raw, 16, 20, 24, 240);
    SDL_Rect panel{0, 0, width, int(lines.size()) * 18 + 10};
    SDL_RenderFillRect(raw, &panel);
    int y = 8;
    for (const auto& line : lines) { font.draw(raw, 8, y, line, 2, {240, 240, 240, 255}); y += 18; }
    SDL_RenderPresent(raw);
    if (frames > 0 && ++rendered >= frames) running = false;
    SDL_Delay(8);
  }
  return 0;
}
}  // namespace

int main(int argc, char** argv) {
  try {
    std::string game = "game", maps, spec;
    bool text = false, list = false;
    int frames = 0;
    for (int i = 1; i < argc; ++i) {
      std::string arg = argv[i];
      if (arg == "--help") {
        std::cout << "Usage: aztec_map_viewer [--game DIR] [--maps DIR] [--dump | --list] [MISSION]\n"
                     "MISSION: number, basename, or .mp path. Omit to browse/dump all missions.\n"
                     "--frames N: exit the SDL view after N frames (smoke checks).\n";
        return 0;
      }
      if (arg == "--dump") text = true;
      else if (arg == "--list") list = true;
      else if (arg == "--game" || arg == "--maps" || arg == "--frames") {
        if (++i == argc) throw std::runtime_error("Missing value for " + arg);
        if (arg == "--game") game = argv[i];
        else if (arg == "--maps") maps = argv[i];
        else {
          std::string value = argv[i];
          size_t used = 0;
          frames = std::stoi(value, &used);
          if (used != value.size() || frames <= 0) throw std::runtime_error("Invalid frame count");
        }
      } else if (!arg.empty() && arg[0] == '-') throw std::runtime_error("Unknown option: " + arg);
      else if (spec.empty()) spec = arg;
      else throw std::runtime_error("Only one mission may be selected");
    }
    if (text && list) throw std::runtime_error("Choose either --dump or --list");
    if (maps.empty()) {
      maps = fs::is_regular_file(spec) ? fs::absolute(spec).parent_path().string() : game + "/Maps";
    }
    std::vector<std::string> paths;
    for (const auto& entry : fs::directory_iterator(maps))
      if (entry.is_regular_file() && lower(entry.path().extension().string()) == ".mp") paths.push_back(entry.path().string());
    std::sort(paths.begin(), paths.end(), [](const auto& a, const auto& b) {
      auto la = lower(a), lb = lower(b);
      return la == lb ? a < b : la < lb;
    });
    size_t index = 0;
    if (!spec.empty()) {
      std::string resolved = resolve_mission(maps, spec);
      if (resolved.empty()) throw std::runtime_error("Mission not found: " + spec);
      auto it = std::find_if(paths.begin(), paths.end(), [&](const auto& p) { return fs::equivalent(p, resolved); });
      if (it == paths.end()) { paths.push_back(resolved); index = paths.size() - 1; }
      else index = size_t(it - paths.begin());
    }
    if (paths.empty()) throw std::runtime_error("No missions in: " + maps);
    if (list) { for (const auto& p : paths) std::cout << fs::path(p).filename().string() << '\n'; return 0; }
    if (!text) return view(paths, index, game, frames);
    bool failed = false;
    for (size_t i = spec.empty() ? 0 : index; i < (spec.empty() ? paths.size() : index + 1); ++i) {
      try { dump(load(paths[i])); }
      catch (const std::exception& ex) { std::cerr << ex.what() << '\n'; failed = true; }
    }
    return failed ? 1 : 0;
  } catch (const std::exception& ex) {
    std::cerr << "Map inspector: " << ex.what() << '\n';
    return 1;
  }
}
