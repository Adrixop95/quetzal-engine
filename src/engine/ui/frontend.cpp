#include "engine/ui/frontend.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>

#include "core/text.hpp"
#include "core/visfile.hpp"
#include "engine/assets/mission.hpp"
#include "engine/render/sprite_store.hpp"
#include "engine/render/png.hpp"
#include "engine/ui/cursor.hpp"

namespace fs = std::filesystem;

namespace aztec::engine {

std::vector<std::string> wrap_text(const Font& font, const std::string& text, int max_width, int scale) {
  std::vector<std::string> lines;
  std::string para;
  auto flush_para = [&]() {
    std::string cur, word;
    auto push_word = [&]() {
      if (word.empty()) return;
      std::string t = cur.empty() ? word : cur + " " + word;
      if (!cur.empty() && font.width(t, scale) > max_width) { lines.push_back(cur); cur = word; }
      else cur = t;
      word.clear();
    };
    for (char c : para) { if (c == ' ') push_word(); else word += c; }
    push_word();
    lines.push_back(cur);
    para.clear();
  };
  for (char c : text) {
    if (c == '\r') continue;
    if (c == '\n') flush_para();
    else para += c;
  }
  flush_para();
  return lines;
}

// ---------------------------------------------------------------------------------------------------------------------

namespace {

struct Layout {   // 800x600 virtual screen letterboxed into the window
  float s = 1;
  int ox = 0, oy = 0;
  int X(int vx) const { return ox + int(vx * s); }
  int Y(int vy) const { return oy + int(vy * s); }
  int W(int vw) const { return int(vw * s); }
  void fit(int w, int h) { s = std::min(w / 800.f, h / 600.f); ox = int((w - 800 * s) / 2); oy = int((h - 600 * s) / 2); }
  int msize(float k) const { return std::max(1, int(std::lround(s * k))); }   // font multiplier
};

struct Button { int x, y, w, h; std::string text; };

SpriteKey common_key(uint32_t id) {
  SpriteKey k;
  k.archive = ArchiveId::Common; k.group = 0x44c; k.index = id; k.shade = 0;
  return k;
}

class Screen {
 public:
  Screen(Window& win, GameData& data, Options& opts) : win_(win), data_(data), opts_(opts), store_(win.renderer(), data) {
    font_.init(win.renderer(), &store_);
    cursor_.init(win.renderer(), store_);
  }

  bool run(FrontendResult& out, const std::string& open_briefing);

 private:
  enum class Mode { Main, Campaign, Custom, Briefing, Options };

  void background(uint32_t id) {
    const StoredSprite& s = store_.get(common_key(id));
    SDL_Renderer* r = win_.renderer();
    SDL_SetRenderDrawColor(r, 0, 0, 0, 255);
    SDL_RenderClear(r);
    if (!s.ok()) return;
    SDL_Rect src = s.frame_rect(0), dst{L_.ox, L_.oy, int(800 * L_.s), int(600 * L_.s)};
    SDL_RenderCopy(r, store_.atlas().page(s.region.page), &src, &dst);
  }
  void text(int vx, int vy, const std::string& t, float size, SDL_Color c, bool center = false, bool shadow = true) {
    int m = L_.msize(size);
    int w = font_.width(t, 2 * m);
    font_.draw(win_.renderer(), center ? L_.X(vx) - w / 2 : L_.X(vx), L_.Y(vy), t, 2 * m, c, shadow);
  }
  bool hover(const Button& b) const { return mx_ >= b.x && my_ >= b.y && mx_ < b.x + b.w && my_ < b.y + b.h; }
  void draw_button(const Button& b, bool enabled = true) {
    SDL_Renderer* r = win_.renderer();
    SDL_Rect d{L_.X(b.x), L_.Y(b.y), L_.W(b.w), L_.W(b.h)};
    bool h = hover(b) && enabled;
    SDL_SetRenderDrawColor(r, h ? 70 : 30, h ? 45 : 22, 15, 210);
    SDL_RenderFillRect(r, &d);
    SDL_SetRenderDrawColor(r, h ? 255 : 190, h ? 220 : 150, h ? 120 : 80, 255);
    SDL_RenderDrawRect(r, &d);
    int m = L_.msize(1.5f);
    int tw = font_.width(b.text, 2 * m), th = font_.height(2 * m);
    font_.draw(r, d.x + (d.w - tw) / 2, d.y + (d.h - th) / 2, b.text, 2 * m, enabled ? (h ? SDL_Color{255, 240, 170, 255} : SDL_Color{235, 215, 160, 255}) : SDL_Color{130, 120, 110, 255});
  }
  bool clicked(const Button& b) const { return click_ && hover(b); }

  void load_lists();
  void enter_briefing(const std::string& spec);
  void frame_begin();
  void frame_end();

  Window& win_;
  GameData& data_;
  Options& opts_;
  SpriteStore store_;
  Font font_;
  Cursor cursor_;
  Layout L_;
  Input in_;
  int mx_ = 0, my_ = 0;       // mouse in virtual 800x600 coordinates
  bool click_ = false;
  Mode mode_ = Mode::Main;
  int ow_ = 0, oh_ = 0;

  struct Entry { std::string spec, title; };
  std::vector<Entry> campaign_, custom_;
  int custom_page_ = 0;
  // briefing
  std::string brief_spec_, brief_title_, brief_text_, brief_avi_;
  bool brief_campaign_ = false;
  int brief_scroll_ = 0;
  bool quit_ = false;
  std::string chosen_;
};

void Screen::load_lists() {
  campaign_.clear();
  custom_.clear();
  std::error_code ec;
  std::vector<std::string> stems;
  for (const auto& e : fs::directory_iterator(data_.maps_dir(), ec)) {
    std::string ext = e.path().extension().string();
    for (auto& c : ext) c = char(std::tolower(c));
    if (ext == ".mp") stems.push_back(e.path().stem().string());
  }
  std::sort(stems.begin(), stems.end(), [](const std::string& a, const std::string& b) {
    std::string la = a, lb = b;
    for (auto& c : la) c = char(std::tolower(c));
    for (auto& c : lb) c = char(std::tolower(c));
    return la < lb;
  });
  for (int n = 1; n <= 15; n++) {
    std::string spec = "mission_" + std::to_string(n);
    std::string mp = find_file_ci(data_.maps_dir(), spec + ".mp");
    if (mp.empty()) continue;
    MissionDef m;
    std::string title = load_mission(mp, m) && !m.title.empty() ? m.title : spec;
    std::vector<BriefingSection> bs;
    std::string mit = find_file_ci(data_.maps_dir(), spec + ".mit");
    if (!mit.empty() && load_briefing(mit, bs) && !bs.empty() && !bs[0].title.empty()) title = cp1250_to_utf8(bs[0].title);
    campaign_.push_back({spec, title});
  }
  for (const std::string& st : stems) {
    std::string lo = st;
    for (auto& c : lo) c = char(std::tolower(c));
    if (lo.rfind("mission_", 0) == 0) continue;
    custom_.push_back({st, st});
  }
}

void Screen::enter_briefing(const std::string& spec) {
  brief_spec_ = spec;
  brief_title_ = spec;
  brief_text_.clear();
  brief_avi_.clear();
  brief_scroll_ = 0;
  std::string lo = spec;
  for (auto& c : lo) c = char(std::tolower(c));
  brief_campaign_ = lo.rfind("mission_", 0) == 0;
  std::string mp = resolve_mission(data_.maps_dir(), spec);
  MissionDef m;
  if (!mp.empty() && load_mission(mp, m)) {
    if (!m.title.empty()) brief_title_ = m.title;
    brief_avi_ = m.avi;
    for (const std::string& n : m.notes) brief_text_ += n + "\n";
  }
  std::string mit = find_file_ci(data_.maps_dir(), (fs::path(mp).stem().string()) + ".mit");
  std::vector<BriefingSection> secs;
  if (!mit.empty() && load_briefing(mit, secs) && !secs.empty()) {
    brief_text_.clear();
    brief_title_ = cp1250_to_utf8(secs[0].title);
    for (const BriefingSection& s : secs) brief_text_ += cp1250_to_utf8(s.text) + "\n";
  }
  mode_ = Mode::Briefing;
}

void Screen::frame_begin() {
  in_.begin_frame();
  SDL_Event e;
  float sx, sy;
  win_.mouse_scale(sx, sy);
  while (SDL_PollEvent(&e)) in_.handle(e, sx, sy);
  win_.output_size(ow_, oh_);
  L_.fit(ow_, oh_);
  mx_ = int((in_.mx - L_.ox) / L_.s);
  my_ = int((in_.my - L_.oy) / L_.s);
  click_ = in_.pressed[SDL_BUTTON_LEFT];
  if (in_.quit) quit_ = true;
  if (in_.hit(SDL_SCANCODE_F11)) win_.cycle_borderless();
  if (in_.hit(SDL_SCANCODE_RETURN) && in_.alt) win_.toggle_fullscreen();
}

// Developer aid: AZTEC_MENU_SHOT=dir makes the front-end walk through its screens, saving a PNG of each, then quit.
static void save_png(SDL_Renderer* r, const std::string& path) {
  int w, h;
  SDL_GetRendererOutputSize(r, &w, &h);
  std::vector<uint8_t> px(size_t(w) * h * 4);
  if (SDL_RenderReadPixels(r, nullptr, SDL_PIXELFORMAT_ABGR8888, px.data(), w * 4) != 0) return;
  for (size_t i = 3; i < px.size(); i += 4) px[i] = 255;
  write_png(path, w, h, px);
}

void Screen::frame_end() {
  if (const char* dir = std::getenv("AZTEC_MENU_SHOT")) {
    static int frames_in_mode = 0, step = 0;
    if (++frames_in_mode == 4) {
      static const char* names[] = {"main", "campaign", "custom", "briefing", "options"};
      if (step < 5) {
        save_png(win_.renderer(), std::string(dir) + "/menu_" + names[step] + ".png");
        step++;
        frames_in_mode = 0;
        switch (step) {
          case 1: mode_ = Mode::Campaign; break;
          case 2: mode_ = Mode::Custom; break;
          case 3: enter_briefing("mission_1"); break;
          case 4: mode_ = Mode::Options; break;
          default: quit_ = true; break;
        }
      }
    }
  }
  cursor_.draw(win_.renderer(), store_, in_.mx, in_.my, CursorKind::Arrow, std::max(1, int(std::lround(L_.s))), SDL_GetTicks());
  SDL_RenderPresent(win_.renderer());
}

bool Screen::run(FrontendResult& out, const std::string& open_briefing) {
  load_lists();
  if (!open_briefing.empty()) enter_briefing(open_briefing);
  const SDL_Color gold{240, 215, 150, 255}, white{240, 240, 240, 255};
  while (!quit_ && chosen_.empty()) {
    frame_begin();
    switch (mode_) {
      case Mode::Main: {
        background(280);
        text(400, 70, "AZTEC EMPIRE", 4, {255, 225, 140, 255}, true);
        Button bs[4] = {{300, 240, 200, 40, "Campaign"}, {300, 295, 200, 40, "Custom map"}, {300, 350, 200, 40, "Options"}, {300, 405, 200, 40, "Quit"}};
        for (Button& b : bs) draw_button(b);
        if (clicked(bs[0])) mode_ = Mode::Campaign;
        if (clicked(bs[1])) { mode_ = Mode::Custom; custom_page_ = 0; }
        if (clicked(bs[2])) mode_ = Mode::Options;
        if (clicked(bs[3]) || in_.hit(SDL_SCANCODE_ESCAPE)) quit_ = true;
        break;
      }
      case Mode::Campaign: {
        background(270);
        text(400, 22, "Campaign", 2.2f, gold, true);
        for (size_t i = 0; i < campaign_.size(); i++) {
          int col = int(i) / 8, row = int(i) % 8;
          Button b{col ? 430 : 90, 100 + row * 52, 290, 44, std::to_string(i + 1) + ". " + campaign_[i].title};
          draw_button(b);
          if (clicked(b)) enter_briefing(campaign_[i].spec);
        }
        Button back{90, 535, 150, 36, "Back"};
        draw_button(back);
        if (clicked(back) || in_.hit(SDL_SCANCODE_ESCAPE)) mode_ = Mode::Main;
        break;
      }
      case Mode::Custom: {
        background(270);
        text(400, 22, "Custom maps", 2.2f, gold, true);
        int per = 14, pages = std::max(1, int((custom_.size() + per - 1) / per));
        if (in_.wheel < 0) custom_page_ = std::min(pages - 1, custom_page_ + 1);
        if (in_.wheel > 0) custom_page_ = std::max(0, custom_page_ - 1);
        for (int k = 0; k < per; k++) {
          size_t i = size_t(custom_page_ * per + k);
          if (i >= custom_.size()) break;
          int col = k / 7, row = k % 7;
          Button b{col ? 430 : 90, 100 + row * 55, 290, 44, custom_[i].title};
          draw_button(b);
          if (clicked(b)) enter_briefing(custom_[i].spec);
        }
        text(400, 500, "Page " + std::to_string(custom_page_ + 1) + "/" + std::to_string(pages) + " (wheel)", 1, {70, 40, 20, 255}, true, false);
        Button back{90, 535, 150, 36, "Back"};
        draw_button(back);
        if (clicked(back) || in_.hit(SDL_SCANCODE_ESCAPE)) mode_ = Mode::Main;
        break;
      }
      case Mode::Briefing: {
        background(290);
        std::vector<std::string> lines = wrap_text(font_, brief_text_, L_.W(330), 2 * L_.msize(1.2f));
        int lh = font_.height(2 * L_.msize(1.2f)) + 3;
        int maxl = std::max(1, (L_.W(330) * 2 / 1) / std::max(1, lh) / 2 + 12);
        (void)maxl;
        int y = 205;
        if (in_.wheel < 0) brief_scroll_++;
        if (in_.wheel > 0) brief_scroll_ = std::max(0, brief_scroll_ - 1);
        brief_scroll_ = std::min(brief_scroll_, std::max(0, int(lines.size()) - 9));
        for (size_t i = size_t(brief_scroll_); i < lines.size(); i++) {
          if (y > 470) break;
          font_.draw(win_.renderer(), L_.X(235), L_.Y(y), lines[i], 2 * L_.msize(1.2f), {40, 25, 15, 255}, false);
          y += int(lh / L_.s) + 1;
        }
        Button start{610, 300, 170, 40, "Start"}, back{610, 400, 170, 40, "Back"}, vid{610, 350, 170, 40, "Intro video"};
        draw_button(start);
        draw_button(back);
        bool have_video = !brief_avi_.empty();
        if (have_video) draw_button(vid);
        if (clicked(start) || in_.hit(SDL_SCANCODE_RETURN)) chosen_ = brief_spec_;
        if (clicked(back) || in_.hit(SDL_SCANCODE_ESCAPE)) mode_ = brief_campaign_ ? Mode::Campaign : Mode::Custom;
        if (have_video && clicked(vid)) {
          std::string vp = find_file_ci(data_.root() + "/Video", brief_avi_);
          if (vp.empty() || !play_video(win_, vp)) std::fprintf(stderr, "note: cannot play %s (needs ffmpeg and the file)\n", brief_avi_.c_str());
        }
        break;
      }
      case Mode::Options: {
        background(284);
        text(400, 60, "Options", 3, gold, true);
        auto mode_name = [&]() { switch (win_.mode()) { case DisplayMode::Windowed: return "Windowed"; case DisplayMode::Borderless: return "Borderless"; default: return "Fullscreen"; } };
        Button b1{250, 170, 300, 40, std::string("Display: ") + mode_name()};
        Button b2{250, 225, 300, 40, std::string("Edge scrolling: ") + (opts_.edge_scroll ? "on" : "off")};
        Button b3{250, 280, 300, 40, std::string("Fog of war: ") + (opts_.fog ? "on" : "off")};
        Button b4{250, 335, 300, 40, std::string("Day / night tint: ") + (opts_.light ? "on" : "off")};
        Button back{250, 420, 300, 40, "Back"};
        for (Button* b : {&b1, &b2, &b3, &b4, &back}) draw_button(*b);
        if (clicked(b1)) win_.set_mode(win_.mode() == DisplayMode::Windowed ? DisplayMode::Borderless : win_.mode() == DisplayMode::Borderless ? DisplayMode::Fullscreen : DisplayMode::Windowed);
        if (clicked(b2)) opts_.edge_scroll = !opts_.edge_scroll;
        if (clicked(b3)) opts_.fog = !opts_.fog;
        if (clicked(b4)) opts_.light = !opts_.light;
        if (clicked(back) || in_.hit(SDL_SCANCODE_ESCAPE)) mode_ = Mode::Main;
        break;
      }
    }
    (void)white;
    frame_end();
  }
  out.quit = quit_ && chosen_.empty();
  out.mission = chosen_;
  return !out.quit;
}

}  // namespace

bool run_frontend(Window& win, GameData& data, Options& opts, FrontendResult& out, const std::string& open_briefing) {
  Screen s(win, data, opts);
  return s.run(out, open_briefing);
}

// ---------------------------------------------------------------------------------------------------------------------

bool play_video(Window& win, const std::string& path) {
  if (std::system("command -v ffmpeg >/dev/null 2>&1") != 0) return false;
  std::string cmd = "ffmpeg -loglevel quiet -i '" + path + "' -vf scale=640:480 -r 25 -f rawvideo -pix_fmt rgb24 - 2>/dev/null";
  FILE* p = popen(cmd.c_str(), "r");
  if (!p) return false;
  SDL_Renderer* r = win.renderer();
  SDL_Texture* tex = SDL_CreateTexture(r, SDL_PIXELFORMAT_RGB24, SDL_TEXTUREACCESS_STREAMING, 640, 480);
  std::vector<uint8_t> buf(640 * 480 * 3);
  bool any = false, skip = false;
  while (!skip && std::fread(buf.data(), 1, buf.size(), p) == buf.size()) {
    any = true;
    SDL_Event e;
    while (SDL_PollEvent(&e))
      if (e.type == SDL_QUIT || e.type == SDL_KEYDOWN || e.type == SDL_MOUSEBUTTONDOWN) skip = true;
    int ow, oh;
    win.output_size(ow, oh);
    SDL_UpdateTexture(tex, nullptr, buf.data(), 640 * 3);
    SDL_SetRenderDrawColor(r, 0, 0, 0, 255);
    SDL_RenderClear(r);
    float s = std::min(ow / 640.f, oh / 480.f);
    SDL_Rect d{int((ow - 640 * s) / 2), int((oh - 480 * s) / 2), int(640 * s), int(480 * s)};
    SDL_RenderCopy(r, tex, nullptr, &d);
    SDL_RenderPresent(r);
    SDL_Delay(36);
  }
  SDL_DestroyTexture(tex);
  pclose(p);
  return any;
}

// ---------------------------------------------------------------------------------------------------------------------

SDL_Rect PauseMenu::button(int i, int ow, int oh, int sc) const {
  int w = 220 * sc, h = 30 * sc;
  return {(ow - w) / 2, oh / 2 - 120 * sc + i * 40 * sc, w, h};
}

PauseMenu::Action PauseMenu::update(const Input& in, int ow, int oh, int sc) {
  if (!open || !in.pressed[SDL_BUTTON_LEFT]) return Action::None;
  static const Action acts[6] = {Action::Resume, Action::Save, Action::Load, Action::Restart, Action::Menu, Action::Quit};
  for (int i = 0; i < 6; i++) {
    SDL_Rect b = button(i, ow, oh, sc);
    if (in.mx >= b.x && in.mx < b.x + b.w && in.my >= b.y && in.my < b.y + b.h) return acts[i];
  }
  return Action::None;
}

void PauseMenu::draw(SDL_Renderer* r, const Font& font, int ow, int oh, int sc) const {
  if (!open) return;
  SDL_Rect all{0, 0, ow, oh};
  SDL_SetRenderDrawColor(r, 0, 0, 0, 150);
  SDL_RenderFillRect(r, &all);
  static const char* labels[6] = {"Resume", "Save game", "Load game", "Restart mission", "Main menu", "Quit"};
  int mx, my;
  SDL_GetMouseState(&mx, &my);
  for (int i = 0; i < 6; i++) {
    SDL_Rect b = button(i, ow, oh, sc);
    SDL_SetRenderDrawColor(r, 35, 26, 16, 235);
    SDL_RenderFillRect(r, &b);
    SDL_SetRenderDrawColor(r, 200, 165, 90, 255);
    SDL_RenderDrawRect(r, &b);
    int ts = 2 * sc;
    font.draw(r, b.x + (b.w - font.width(labels[i], ts)) / 2, b.y + (b.h - font.height(ts)) / 2, labels[i], ts, {240, 220, 160, 255});
  }
}

}  // namespace aztec::engine
