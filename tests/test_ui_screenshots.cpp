// Invented solid sprites and glyphs exercise UI rendering without private assets.
#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <map>
#include "temp_dir.hpp"
#include "core/endian.hpp"
#include "engine/render/png.hpp"
#include "engine/ui/commands_ui.hpp"
#include "engine/ui/hud.hpp"
#include "engine/ui/panel.hpp"

using namespace aztec;
using namespace aztec::engine;
using Bytes = std::vector<uint8_t>;
static int failures = 0;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL %d: %s\n", __LINE__, #c); ++failures; } } while (0)
static void put(Bytes& b, size_t p, uint32_t v) { write_le(b.data() + p, v); }
static Bytes sprite(int w, int h, uint16_t color) {
  Bytes b(24 + size_t(w) * h * 2);
  put(b, 0, 0xe0ac0000); put(b, 4, uint32_t(w | (h << 16)));
  put(b, 8, 0x11); put(b, 12, 14); put(b, 16, uint32_t(w | (h << 16)));
  for (size_t p = 24; p < b.size(); p += 2) write_le(b.data() + p, color);
  return b;
}
static void archive_groups(const std::filesystem::path& path, const std::map<int, std::map<int, Bytes>>& groups) {
  Bytes b(4 + groups.size() * 16);
  put(b, 0, uint32_t(groups.size()));
  size_t g = 4;
  for (const auto& [id, items] : groups) {
    const int count = items.empty() ? 0 : items.rbegin()->first + 1;
    const size_t table = b.size();
    put(b, g, id); put(b, g + 4, uint32_t(table)); put(b, g + 12, count);
    b.resize(b.size() + size_t(count) * 4);
    for (int i = 0; i < count; ++i) {
      put(b, table + i * 4, uint32_t(b.size()));
      auto it = items.find(i);
      if (it != items.end()) b.insert(b.end(), it->second.begin(), it->second.end());
    }
    g += 16;
  }
  std::ofstream file(path, std::ios::binary);
  file.write(reinterpret_cast<const char*>(b.data()), std::streamsize(b.size()));
}
static void archive(const std::filesystem::path& path, const std::map<int, Bytes>& items) {
  archive_groups(path, {{0x44c, items}});
}
static std::vector<uint32_t> capture(SDL_Renderer* r, int w, int h) {
  std::vector<uint32_t> pixels(size_t(w) * h);
  CHECK(SDL_RenderReadPixels(r, nullptr, SDL_PIXELFORMAT_ARGB8888, pixels.data(), w * 4) == 0);
  return pixels;
}
// Compare a known region, with a per-channel tolerance and a bounded bad-pixel fraction.
static bool matches(const std::vector<uint32_t>& actual, const std::vector<uint32_t>& expected,
                    int w, SDL_Rect area, int tolerance = 3, double fraction = 0.002) {
  size_t bad = 0;
  for (int y = area.y; y < area.y + area.h; ++y) for (int x = area.x; x < area.x + area.w; ++x) {
    auto a = actual[size_t(y) * w + x], e = expected[size_t(y) * w + x];
    bool different = false;
    for (int shift : {0, 8, 16}) different |= std::abs(int((a >> shift) & 255) - int((e >> shift) & 255)) > tolerance;
    bad += different;
  }
  return double(bad) <= double(area.w) * area.h * fraction;
}
static void save(const std::filesystem::path& path, const std::vector<uint32_t>& pixels, int w, int h) {
  std::vector<uint8_t> rgba;
  for (auto v : pixels) for (int shift : {16, 8, 0, 24}) rgba.push_back(uint8_t(v >> shift));
  CHECK(write_png(path.string(), w, h, rgba));
}
static void run(const std::filesystem::path& output, bool original, int sc, int width, int height) {
  TempDir temp;
  auto res = temp.path() / "Res";
  std::filesystem::create_directory(res);
  std::map<int, Bytes> common{{292, sprite(204, 493, 0xf800)}, {297, sprite(254, 208, 0x07e0)},
                              {282, sprite(60, 44, 0x001f)}};
  // Distinct command colors catch incorrect resource IDs and placement.
  for (int id : {275, 257, 268, 301, 288, 269}) {
    common[id] = sprite(40, 40 * 12, 0xffff);
    put(common[id], 16, 40 | (40 << 16));
    for (int frame = 1; frame < 12; ++frame)
      for (size_t p = 24 + size_t(frame) * 40 * 40 * 2; p < 24 + size_t(frame + 1) * 40 * 40 * 2; p += 2)
        write_le<uint16_t>(common[id].data() + p, uint16_t(frame << 11));
  }
  // Menu has two stacked frames; only the first blue frame belongs on screen.
  put(common[282], 16, 60 | (22 << 16));
  for (size_t p = 24 + 60 * 22 * 2; p < common[282].size(); p += 2)
    write_le<uint16_t>(common[282].data() + p, 0xf800);
  archive(res / "common.xbe", common);
  std::map<int, Bytes> glyphs;
  if (original) for (int c = 33; c < 127; ++c) glyphs[1200 + c] = sprite(3, 7, 0xffff);
  archive(res / "fnts.xbe", glyphs);
  for (const char* name : {"Pal", "Units", "Tiles0", "Tiles1", "light"}) archive(res / (std::string(name) + ".xbe"), {});
  // An invented indexed icon makes palette-state mistakes visible without game pixels.
  Bytes indexed(24 + 40 * 40, 1);
  put(indexed, 0, 0xe0ac0000); put(indexed, 4, 40 | (40 << 16));
  put(indexed, 8, 1); put(indexed, 12, 16); put(indexed, 16, 40 | (40 << 16)); put(indexed, 20, 0);
  Bytes anim(0x208 + 3 * 32);
  put(anim, 0, 0x6d696e41); put(anim, 4, 3);
  for (int slot = 0; slot < 128; ++slot) put(anim, 8 + slot * 4, 1);
  put(anim, 0x208 + 32, 1);
  std::map<int, Bytes> animations;
  for (const auto& type : data::kTypeAnims) animations[type.anim] = anim;
  archive_groups(res / "Units.xbe", {{0x44c, {{1, indexed}}}, {0x515, animations}});
  Bytes palette(20 * 512);
  for (int shade = 0; shade < 20; ++shade)
    write_le<uint16_t>(palette.data() + shade * 512 + 2, shade == 9 ? 0x07e0 : shade == 1 ? 0x001f : 0xffff);
  archive_groups(res / "Pal.xbe", {{0x450, {{16, palette}}}});
  SDL_Surface* surface = SDL_CreateRGBSurfaceWithFormat(0, width, height, 32, SDL_PIXELFORMAT_ARGB8888);
  CHECK(surface);
  SDL_Renderer* r = SDL_CreateSoftwareRenderer(surface); CHECK(r);
  {
    GameData data; std::string error; CHECK(data.open(temp.path().string(), error));
    SpriteStore store(r, data); Font font; CHECK(font.init(r, &store)); CHECK(font.original() == original);
    Panel panel; CHECK(panel.init(store)); panel.layout(width, height, sc);
    CHECK(font.width("", sc) == 0);
    auto wrapped = font.wrap("AB CDEFGHIJKLMNOP", font.ui_scale(sc), 16 * sc);
    CHECK(wrapped.size() >= 4);
    for (const auto& line : wrapped) CHECK(font.width(line, font.ui_scale(sc)) <= 16 * sc);
    SDL_SetRenderDrawColor(r, 0, 0, 0, 255); SDL_RenderClear(r);
    panel.draw(r, store, font, 123);
    auto actual = capture(r, width, height);
    // Independent expected silhouette: side strip, bottom card and Menu sprite.
    const int side = width - 204 * sc, card = width - 254 * sc, top = height - 208 * sc;
    std::vector<uint32_t> expected(size_t(width) * height, 0xff000000);
    auto fill = [&](SDL_Rect rect, uint32_t color) {
      for (int y = rect.y; y < rect.y + rect.h; ++y) for (int x = rect.x; x < rect.x + rect.w; ++x)
        expected[size_t(y) * width + x] = color;
    };
    fill({side, 0, 204 * sc, height}, 0xffff0000);
    fill({card, top, 254 * sc, 208 * sc}, 0xff00ff00);
    fill({side + 2 * sc, 0, 60 * sc, 22 * sc}, 0xff0000ff);
    // Exclude the dynamic gold label; compare the rest of the entire frame.
    CHECK(matches(actual, expected, width, {0, 24 * sc, width, height - 24 * sc}));
    CHECK(matches(actual, expected, width, {side + 2 * sc, 0, 60 * sc, 22 * sc}));
    auto wrong = expected;
    for (int y = top; y < height; ++y) wrong[size_t(y) * width + card] = 0xff000000;
    CHECK(!matches(actual, wrong, width, {card, top, 50 * sc, 208 * sc}));
    CHECK(panel.contains(card, top)); CHECK(!panel.contains(card, top - 1));
    CHECK(panel.menu_rect().w == 60 * sc);
    auto rules = std::make_shared<RulesData>(); CHECK(rules->load_dir(AZTEC_TEST_RULES_DIR)); GameSim sim(rules);
    World world; world.terrain.w = 120; world.terrain.h = 90; world.terrain.cells.resize(120 * 90);
    Object unit; unit.id = 1; unit.type = 0; unit.hp = 100; world.objects.push_back(unit); world.selection.ids = {1};
    CommandsUi commands(&sim); StrBase strings;
    const auto layout = commands.layout(panel.rect(), sc);
    CHECK(layout.portrait.x == width - 78 * sc && layout.portrait.y == 218 * sc);
    CHECK(layout.commands[0].x == card + 16 * sc && layout.commands[0].y == top + 70 * sc);
    auto tip = commands.draw_menu(r, store, font, strings, world, panel.rect(), sc,
                                  layout.commands[0].x + sc, layout.commands[0].y + sc);
    CHECK(tip.find("Move (M)") == 0);
    auto commands_shot = capture(r, width, height);
    // Enabled Move icon is white throughout its interior, not a fallback label.
    auto icon_expected = commands_shot;
    SDL_Rect center{layout.commands[0].x + 4 * sc, layout.commands[0].y + 4 * sc, 30 * sc, 30 * sc};
    for (int y = center.y; y < center.y + center.h; ++y) for (int x = center.x; x < center.x + center.w; ++x)
      icon_expected[size_t(y) * width + x] = 0xffffffff;
    CHECK(matches(commands_shot, icon_expected, width, center));
    Minimap minimap; CHECK(minimap.init(r, data, world, 0, 14)); minimap.layout_in(panel.minimap_rect(), sc);
    CHECK(minimap.rect().x == width - 162 * sc && minimap.rect().y == 70 * sc);
    CHECK(minimap.rect().w == 120 * sc && minimap.rect().h == 90 * sc);
    Camera camera; camera.set_world(5760, 4320); camera.set_view(float(width - 204 * sc), float(height));
    // A selected command must use the recovered rotation, while hover alone
    // must leave the resource face intact (no generic rectangle overlay).
    auto draw_commands = [&](int mx, int my) {
      panel.draw(r, store, font, 123);
      commands.draw_menu(r, store, font, strings, world, panel.rect(), sc, mx, my);
      return capture(r, width, height);
    };
    const auto idle = draw_commands(-1, -1);
    const auto hovered = draw_commands(layout.commands[0].x + sc, layout.commands[0].y + sc);
    CHECK(matches(hovered, idle, width, layout.commands[0], 0, 0));
    // Menu depression follows capture, including leave/re-enter, release and focus loss.
    Input menu_input;
    menu_input.mx = panel.menu_rect().x + sc; menu_input.my = sc;
    const auto menu_idle = capture(r, width, height);
    menu_input.buttons[SDL_BUTTON_LEFT] = menu_input.pressed[SDL_BUTTON_LEFT] = true;
    CHECK(!panel.update_menu(menu_input));
    panel.draw(r, store, font, 123);
    CHECK(!matches(capture(r, width, height), menu_idle, width, panel.menu_rect(), 0, 0));
    menu_input.pressed[SDL_BUTTON_LEFT] = false;
    menu_input.mx = 0;
    CHECK(!panel.update_menu(menu_input));
    panel.draw(r, store, font, 123);
    CHECK(matches(capture(r, width, height), menu_idle, width, panel.menu_rect(), 0, 0));
    menu_input.mx = panel.menu_rect().x + sc;
    CHECK(!panel.update_menu(menu_input));
    panel.draw(r, store, font, 123);
    CHECK(!matches(capture(r, width, height), menu_idle, width, panel.menu_rect(), 0, 0));
    menu_input.buttons[SDL_BUTTON_LEFT] = false; menu_input.released[SDL_BUTTON_LEFT] = true;
    CHECK(panel.update_menu(menu_input));
    CHECK(!panel.update_menu(menu_input));
    menu_input.released[SDL_BUTTON_LEFT] = false;
    menu_input.buttons[SDL_BUTTON_LEFT] = menu_input.pressed[SDL_BUTTON_LEFT] = true;
    CHECK(!panel.update_menu(menu_input));
    menu_input.focused = false;
    CHECK(!panel.update_menu(menu_input));
    panel.draw(r, store, font, 123);
    CHECK(matches(capture(r, width, height), menu_idle, width, panel.menu_rect(), 0, 0));
    menu_input.focused = true; menu_input.pressed[SDL_BUTTON_LEFT] = false;
    menu_input.buttons[SDL_BUTTON_LEFT] = false; menu_input.released[SDL_BUTTON_LEFT] = true;
    CHECK(!panel.update_menu(menu_input));
    Input press; press.mx = layout.commands[0].x + sc; press.my = layout.commands[0].y + sc;
    press.buttons[SDL_BUTTON_LEFT] = press.pressed[SDL_BUTTON_LEFT] = true;
    const auto ui_hash = sim.state_hash(world);
    commands.update(press, world, camera, 1, minimap, false, panel.rect());
    CHECK(commands.target_mode() == TargetMode::None);
    // Releasing over another control does not activate either control.
    Input drag_release = press;
    drag_release.mx = layout.commands[1].x + sc; drag_release.my = layout.commands[1].y + sc;
    drag_release.pressed[SDL_BUTTON_LEFT] = drag_release.buttons[SDL_BUTTON_LEFT] = false;
    drag_release.released[SDL_BUTTON_LEFT] = true;
    commands.update(drag_release, world, camera, 1, minimap, false, panel.rect());
    CHECK(commands.target_mode() == TargetMode::None);
    CHECK(sim.state_hash(world) == ui_hash);
    commands.update(press, world, camera, 1, minimap, false, panel.rect());
    press.pressed[SDL_BUTTON_LEFT] = press.buttons[SDL_BUTTON_LEFT] = false;
    press.released[SDL_BUTTON_LEFT] = true;
    commands.update(press, world, camera, 1, minimap, false, panel.rect());
    CHECK(commands.target_mode() == TargetMode::Move);
    const auto selected = draw_commands(-1, -1);
    CHECK(!matches(selected, idle, width, center, 0, 0));
    CHECK(matches(draw_commands(-1, -1), selected, width, center, 0, 0));
    Input next;
    commands.update(next, world, camera, 1, minimap, false, panel.rect());
    CHECK(!matches(draw_commands(-1, -1), selected, width, center, 0, 0));
    for (int i = 0; i < 11; ++i) commands.update(next, world, camera, 1, minimap, false, panel.rect());
    CHECK(matches(draw_commands(-1, -1), selected, width, center, 0, 0));
    CHECK(sim.state_hash(world) == ui_hash);
    next.key_hit[SDL_SCANCODE_ESCAPE] = true;
    commands.update(next, world, camera, 1, minimap, false, panel.rect());
    CHECK(matches(draw_commands(-1, -1), idle, width, layout.commands[0], 0, 0));
    World town_world = world;
    town_world.mission.sides.emplace_back(); town_world.mission.towns.emplace_back();
    town_world.mission.towns[0].type = 1;
    town_world.objects[0].town = 0;
    sim.init(town_world); town_world.side_state[0].money = 10000;
    CHECK(commands.open_sprites((res / "Units.xbe").string()));
    const auto menu = sim.menu(town_world, 0, 0);
    CHECK(!menu.empty() && menu[0].enabled);
    auto production_shot = [&] {
      panel.draw(r, store, font, 123);
      commands.draw_menu(r, store, font, strings, town_world, panel.rect(), sc, -1, -1);
      return capture(r, width, height);
    };
    auto available = production_shot();
    const SDL_Rect cell = layout.production[0];
    CHECK(available[size_t(cell.y + 10 * sc) * width + cell.x + 10 * sc] == 0xff00ff00);
    town_world.mission.sides[0].disabled.push_back(menu[0].type);
    auto unavailable = production_shot();
    CHECK(unavailable[size_t(cell.y + 10 * sc) * width + cell.x + 10 * sc] == 0xff0000ff);
    commands.draw_menu(r, store, font, strings, town_world, panel.rect(), sc, cell.x + sc, cell.y + sc);
    CHECK(matches(capture(r, width, height), unavailable, width, cell, 0, 0));
    town_world.mission.sides[0].disabled.pop_back();
    CHECK(matches(production_shot(), available, width, cell, 0, 0));
    minimap.draw(r, camera, 0);
    panel.draw_tooltip(r, font, "Move (M)\nA long unbroken token: ABCDEFGHIJKLMNOPQRSTUVWXYZABCDEFGHIJKLMNOPQRSTUVWXYZ");
    draw_messages(r, font, {{"First message with wrapping across several words", 10}, {"Second message", 10}},
                  {8 * sc, 40 * sc, 180 * sc, 100 * sc}, sc);
    const auto full = capture(r, width, height);
    // Text and tooltips must leave the world outside their allotted rectangles unchanged.
    CHECK(matches(full, actual, width, {0, height / 2, 100 * sc, 50 * sc}));
    const auto name = std::string(original ? "resource" : "fallback") + "-" + std::to_string(width) + "x" + std::to_string(height);
    save(output / (name + ".png"), full, width, height);
    // An independent text raster checks physical font scaling and message spacing.
    SDL_SetRenderDrawColor(r, 0, 0, 0, 255); SDL_RenderClear(r);
    draw_messages(r, font, {{"AA", 10}, {"AA", 10}}, {0, 0, 40 * sc, 40 * sc}, sc);
    expected.assign(size_t(width) * height, 0xff000000);
    const int text_width = (original ? 8 : 7) * sc;
    const int text_x = (40 * sc - text_width) / 2;
    const int step = (original ? 15 : 8) * sc;
    const char* letter = "010101111101101";
    for (int line = 0; line < 2; ++line) for (int ch = 0; ch < 2; ++ch)
      for (int y = 0; y < (original ? 7 : 5); ++y) for (int x = 0; x < 3; ++x)
        if (original || letter[y * 3 + x] == '1')
          fill({text_x + ch * 4 * sc + x * sc, line * step + y * sc, sc, sc}, 0xffffffc8);
    CHECK(matches(capture(r, width, height), expected, width, {0, 0, 40 * sc, 40 * sc}));
    SDL_SetRenderDrawColor(r, 0, 0, 0, 255); SDL_RenderClear(r);
    draw_messages(r, font, {{"AAAA", 10}, {"AA", 10}}, {0, 0, 40 * sc, step}, sc);
    // Only the latest message fits. Its shorter raster must replace the first.
    for (int y = step; y < 40 * sc; ++y) for (int x = 0; x < 40 * sc; ++x)
      expected[size_t(y) * width + x] = 0xff000000;
    CHECK(matches(capture(r, width, height), expected, width, {0, 0, 40 * sc, 40 * sc}));
    // Long tooltips wrap into the world area and remain inside the output.
    SDL_SetRenderDrawColor(r, 0, 0, 0, 255); SDL_RenderClear(r);
    panel.draw_tooltip(r, font, std::string(500, 'A'));
    const auto tooltip = capture(r, width, height);
    size_t changed = 0;
    for (int y = 0; y < height; ++y) for (int x = 0; x < width; ++x)
      if ((tooltip[size_t(y) * width + x] & 0xffffff) != 0) {
        ++changed; CHECK(x < card && y < height - 7 * sc);
      }
    CHECK(changed > size_t(100 * sc * sc));

  }
  SDL_DestroyRenderer(r); SDL_FreeSurface(surface);
}
int main(int argc, char** argv) {
  if (SDL_Init(SDL_INIT_VIDEO) != 0) return 1;
  std::filesystem::path output = argc > 1 ? argv[1] : "ui-screenshots";
  std::filesystem::create_directories(output);
  for (bool original : {false, true}) {
    run(output, original, 1, 800, 600);
    run(output, original, 1, 1024, 768);
    run(output, original, 2, 1600, 1200);
    run(output, original, 1, 800, 493);
  }
  SDL_Quit();
  return failures ? 1 : 0;
}
