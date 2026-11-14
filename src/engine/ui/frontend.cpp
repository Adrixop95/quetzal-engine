#include "engine/ui/frontend.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>

#include "core/text.hpp"
#include "core/strbase.hpp"
#include "engine/ui/frontend_layout.hpp"
#include "core/visfile.hpp"
#include "engine/app/menu_script.hpp"
#include "engine/app/ui_trace.hpp"
#include "engine/assets/mission.hpp"
#include "engine/render/sprite_store.hpp"
#include "engine/render/png.hpp"
#include "engine/ui/cursor.hpp"
#include "engine/ui/campaign.hpp"
#include "engine/app/playtest.hpp"
#include "engine/sim/savegame.hpp"

namespace fs = std::filesystem;

namespace aztec::engine {

std::vector<std::string> wrap_text(const Font& font, const std::string& text, int max_width, int scale) {
  return font.wrap(text, scale, max_width);
}

// ---------------------------------------------------------------------------------------------------------------------

namespace {

struct Layout {   // 800x600 virtual screen letterboxed into the window
  float s = 1;
  int ox = 0, oy = 0;
  int X(int vx) const { return ox + int(vx * s); }
  int Y(int vy) const { return oy + int(vy * s); }
  int W(int vw) const { return int(vw * s); }
  void fit(int w, int h) { const FrontendLayout layout(w, h); s = layout.scale; ox = layout.x; oy = layout.y; }
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
    strings_.load(find_file_ci(data.root(), "Res/strbase.bds"));
  }

  bool run(FrontendResult& out, const std::string& open_briefing);

 private:
  enum class Mode { Main, Campaign, Custom, Briefing, Options, Audio, Modifications, Load, Credits, Controls };
  static const char* mode_name(Mode mode) {
    static const char* names[] = {"main", "campaign", "custom", "briefing", "options", "audio", "modifications", "load", "credits", "controls"};
    return names[int(mode)];
  }

  void background(uint32_t id) {
    background_image_ = id;
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
    observed_buttons_.push_back({b, enabled, hover(b)});
    SDL_Renderer* r = win_.renderer();
    SDL_Rect d{L_.X(b.x), L_.Y(b.y), L_.W(b.w), L_.W(b.h)};
    bool h = hover(b) && enabled;
    const bool held = h && in_.buttons[SDL_BUTTON_LEFT] && armed_x_ >= b.x && armed_y_ >= b.y &&
        armed_x_ < b.x + b.w && armed_y_ < b.y + b.h;
    int m = L_.msize(1.f);
    while (m > 1 && font_.width(b.text, 2 * m) > d.w - 8) --m;
    const int ts = 2 * m;
    auto lines = font_.wrap(b.text, ts, std::max(1, d.w - 8));
    std::string label = lines.empty() ? "" : lines.front();
    if (lines.size() > 1) {
      const auto shortened = font_.wrap(label, ts, std::max(1, d.w - 8 - font_.width("...", ts)));
      label = (shortened.empty() ? "" : shortened.front()) + "...";
    }
    int tw = font_.width(label, ts), th = font_.height(ts);
    font_.draw_state(r, d.x + (d.w - tw) / 2, d.y + (d.h - th) / 2,
                     label, ts, !enabled ? 3 : h || held ? 2 : 1);
  }
  bool clicked(const Button& b) const {
    return click_ && hover(b) && armed_x_ >= b.x && armed_y_ >= b.y &&
        armed_x_ < b.x + b.w && armed_y_ < b.y + b.h;
  }

  void load_lists();
  void enter_briefing(const std::string& spec);
  void frame_begin();
  void frame_end();

  Window& win_;
  GameData& data_;
  Options& opts_;
  SpriteStore store_;
  Font font_;
  StrBase strings_;
  std::string label(int ordinal, const char* fallback) const {
    const auto& value = strings_.get(ordinal);
    return value.empty() ? fallback : value;
  }
  Button control(SDL_Rect rect, std::string title) const { return {rect.x, rect.y, rect.w, rect.h, std::move(title)}; }
  Cursor cursor_;
  Layout L_;
  Input in_;
  int mx_ = 0, my_ = 0;       // mouse in virtual 800x600 coordinates
  bool click_ = false;
  Mode mode_ = Mode::Main;
  int ow_ = 0, oh_ = 0;

  struct Entry { std::string spec, title; };
  std::vector<Entry> campaign_, custom_;
  int custom_page_ = 0, campaign_page_ = 0, campaign_selected_ = -1;
  int armed_x_ = -1, armed_y_ = -1;
  CampaignProgress progress_;
  AudioOptions audio_options_;
  ModificationOptions modification_options_;
  SaveLoadMenu slots_;
  TextOverlay credits_;
  std::string resume_save_;
  // briefing
  std::string brief_spec_, brief_text_, brief_avi_;
  std::array<std::string, 3> brief_sections_;
  int brief_section_ = 1;
  bool brief_campaign_ = false;
  int brief_scroll_ = 0;
  bool brief_reading_ = false;
  bool quit_ = false;
  std::string chosen_;
  PlaytestScript capture_script_;
  World capture_world_;
  bool scripted_ = false;
  uint64_t capture_frame_ = 0;
  std::string capture_shot_;
  MenuScript script_;
  MenuScript::Request request_;
  std::ofstream trace_;
  struct ObservedButton { Button button; bool enabled, hover; };
  std::vector<ObservedButton> observed_buttons_;
  uint32_t background_image_ = 0;
  int frame_ = 0;
  Mode rendered_mode_ = Mode::Main;
  const char* rendered_load_id_ = "load-empty";
  bool failed_ = false;
};

void Screen::load_lists() {
  progress_.load(user_file("Saves/engine_campaign.txt"));
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
    std::string title = load_mission(mp, m) && !m.title.empty() ? m.title : label(34, "Mission") + " " + std::to_string(n);
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
  brief_text_.clear();
  brief_avi_.clear();
  brief_scroll_ = 0;
  brief_reading_ = false;
  brief_campaign_ = campaign_number(spec) != 0;
  std::string mp = resolve_mission(data_.maps_dir(), spec);
  MissionDef m;
  if (!mp.empty() && load_mission(mp, m)) {
    brief_avi_ = m.avi;
  }
  brief_text_ = mission_briefing(data_, spec);
  brief_sections_.fill(brief_text_);
  std::vector<BriefingSection> sections;
  const auto stem = fs::path(mp).stem().string();
  if (load_briefing(find_file_ci(data_.maps_dir(), stem + ".mit"), sections)) {
    for (size_t i = 0; i < std::min(sections.size(), brief_sections_.size()); ++i)
      brief_sections_[i] = cp1250_to_utf8(sections[i].title) + "\n" + cp1250_to_utf8(sections[i].text);
  }
  mode_ = Mode::Briefing;
}

void Screen::frame_begin() {
  in_.begin_frame();
  SDL_Event e;
  float sx, sy;
  win_.mouse_scale(sx, sy);
  while (!scripted_ && SDL_PollEvent(&e)) {
    win_.handle_event(e);
    if (opts_.menu_script.empty() || e.type == SDL_QUIT) in_.handle(e, sx, sy);
  }
  if (scripted_) {
    PlaytestRequest request;
    if (!capture_script_.step(in_, capture_world_, nullptr, request) || request.quit) quit_ = true;
    capture_shot_ = request.shot;
  }
  if (!opts_.menu_script.empty()) {
    request_ = script_.step(frame_, in_);
    if (request_.quit) quit_ = true;
  }
  observed_buttons_.clear();
  rendered_mode_ = mode_;
  rendered_load_id_ = slots_.screen_id();
  win_.output_size(ow_, oh_);
  L_.fit(ow_, oh_);
  const auto mouse = FrontendLayout(ow_, oh_).virtual_point(in_.mx, in_.my);
  mx_ = mouse.x; my_ = mouse.y;
  if (!in_.focused) { armed_x_ = armed_y_ = -1; }
  else if (in_.pressed[SDL_BUTTON_LEFT]) { armed_x_ = mx_; armed_y_ = my_; }
  click_ = in_.focused && in_.released[SDL_BUTTON_LEFT];
  if (in_.quit) quit_ = true;
  if (in_.hit(SDL_SCANCODE_F11)) win_.cycle_borderless();
  if (in_.hit(SDL_SCANCODE_RETURN) && in_.alt) win_.toggle_fullscreen();
}

// Developer aid: AZTEC_MENU_SHOT=dir makes the front-end walk through its screens, saving a PNG of each, then quit.
static bool save_png(SDL_Renderer* r, const std::string& path) {
  int w, h;
  SDL_GetRendererOutputSize(r, &w, &h);
  std::vector<uint8_t> px(size_t(w) * h * 4);
  if (SDL_RenderReadPixels(r, nullptr, SDL_PIXELFORMAT_RGBA32, px.data(), w * 4) != 0) return false;
  for (size_t i = 3; i < px.size(); i += 4) px[i] = 255;
  return write_png(path, w, h, px);
}

void Screen::frame_end() {
  if (const char* dir = std::getenv("AZTEC_MENU_SHOT"); dir && *dir) {
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
  const uint32_t cursor_ms = scripted_ ? uint32_t(capture_frame_ * 1000 / 12)
      : opts_.menu_script.empty() ? SDL_GetTicks() : uint32_t(uint64_t(frame_) * 1000 / 60);
  cursor_.draw(win_.renderer(), store_, in_.mx, in_.my, CursorKind::Menu, std::max(1, int(std::lround(L_.s))), cursor_ms);
  if (!capture_shot_.empty()) save_png(win_.renderer(), capture_shot_);
  ++capture_frame_;
  if (!request_.shot.empty() && !save_png(win_.renderer(), request_.shot)) {
    std::fprintf(stderr, "Cannot capture menu frame: %s\n", request_.shot.c_str());
    failed_ = quit_ = true;
  }
  if (trace_.is_open()) {
    trace_ << "{\"schema\":1,\"contract\":\"engine-menu-raw-v1\",\"frame\":" << frame_
           << ",\"phase\":\"after_input_before_present\",\"rendered_screen\":\"" << (rendered_mode_ == Mode::Load ? rendered_load_id_ : mode_name(rendered_mode_))
           << "\",\"next_screen\":\"" << mode_name(mode_) << "\",\"mouse\":[" << in_.mx << ',' << in_.my
           << "],\"left_down\":" << (in_.buttons[SDL_BUTTON_LEFT] ? "true" : "false")
           << ",\"pressed\":" << (in_.pressed[SDL_BUTTON_LEFT] ? "true" : "false")
           << ",\"released\":" << (in_.released[SDL_BUTTON_LEFT] ? "true" : "false")
           << ",\"cursor\":\"mace\",\"cursor_ms\":" << cursor_ms << ",\"buttons\":[";
    for (size_t i = 0; i < observed_buttons_.size(); ++i) {
      const auto& b = observed_buttons_[i];
      trace_ << (i ? "," : "") << "{\"label\":"; trace_string(trace_, b.button.text);
      trace_ << ",\"rect\":[" << b.button.x << ',' << b.button.y << ',' << b.button.w << ',' << b.button.h
             << "],\"enabled\":" << (b.enabled ? "true" : "false")
             << ",\"hover\":" << (b.hover ? "true" : "false")
             << ",\"held\":" << (b.hover && in_.buttons[SDL_BUTTON_LEFT] ? "true" : "false") << '}';
    }
    trace_ << "],\"mission\":"; trace_string(trace_, chosen_);
    trace_ << ",\"keys_hit\":[";
    bool first_key = true;
    for (int k = 0; k < SDL_NUM_SCANCODES; ++k) if (in_.key_hit[k]) {
      trace_ << (first_key ? "" : ",") << k; first_key = false;
    }
    trace_ << "],\"options\":{\"edge_scroll\":" << (opts_.edge_scroll ? "true" : "false")
           << ",\"fog\":" << (opts_.fog ? "true" : "false") << ",\"light\":" << (opts_.light ? "true" : "false")
           << ",\"sfx\":" << opts_.sfx_volume << ",\"music\":" << opts_.music_volume << ",\"speech\":" << opts_.speech_volume
           << ",\"acknowledgement\":" << (opts_.acknowledgement ? "true" : "false")
           << ",\"subtitles\":" << (opts_.subtitles ? "true" : "false") << '}';
    trace_ << ",\"quit\":" << (quit_ ? "true" : "false") << ",\"capture\":";
    trace_string(trace_, request_.shot);
    trace_ << ",\"background_image\":" << background_image_ << ",\"audio_observed\":false}\n";
    trace_.flush();
    if (!trace_) { std::fprintf(stderr, "Cannot write menu trace\n"); failed_ = quit_ = true; }
  }
  SDL_RenderPresent(win_.renderer());
  if (in_.released[SDL_BUTTON_LEFT] || mode_ != rendered_mode_) armed_x_ = armed_y_ = -1;
  ++frame_;
}

bool Screen::run(FrontendResult& out, const std::string& open_briefing) {
  std::string error;
  if (const char* path = std::getenv("AZTEC_UI_SCRIPT"); path && *path) {
    if (!capture_script_.load(path, error)) {
      std::fprintf(stderr, "%s\n", error.c_str());
      out.quit = true;
      return false;
    }
    scripted_ = true;
  }
  if (!opts_.menu_script.empty() && !script_.load(opts_.menu_script, error)) {
    std::fprintf(stderr, "%s\n", error.c_str()); out.quit = out.failed = true; return false;
  }
  if (!opts_.menu_trace.empty()) {
    trace_.open(opts_.menu_trace);
    if (!trace_) { std::fprintf(stderr, "Cannot open menu trace\n"); out.quit = out.failed = true; return false; }
  }
  load_lists();
  if (!open_briefing.empty()) enter_briefing(open_briefing);
  const SDL_Color gold{240, 215, 150, 255}, white{240, 240, 240, 255};
  while (!quit_ && chosen_.empty()) {
    frame_begin();
    if (in_.hit(SDL_SCANCODE_F9) && !in_.ctrl) {
      modification_options_.open = mode_ != Mode::Modifications;
      mode_ = modification_options_.open ? Mode::Modifications : Mode::Options;
    }
    rendered_mode_ = mode_;
    switch (mode_) {
      case Mode::Main: {
        background(284);
        text(400, 125, label(30, "Choose Player"), 1, gold, true);
        // G: one engine profile; importing and editing original players is unavailable.
        Button player{140, 230, 520, 20, "Player"}; draw_button(player);
        Button create = control(FrontendLayout::top_left, label(132, "Create Player"));
        Button remove = control(FrontendLayout::top_middle, label(133, "Remove Player"));
        Button credits = control(FrontendLayout::top_right, label(134, "Credits"));
        Button accept = control(FrontendLayout::bottom_left, label(136, "Accept Player"));
        Button quit = control(FrontendLayout::bottom_right, label(137, "Quit The Game"));
        draw_button(create, false); draw_button(remove, false);
        for (Button* button : {&credits, &accept, &quit}) draw_button(*button);
        if (!opts_.frontend_notice.empty()) text(400, 480, opts_.frontend_notice, 1, gold, true);
        if (clicked(accept) || (in_.hit(SDL_SCANCODE_RETURN) && !in_.alt)) mode_ = Mode::Campaign;
        if (clicked(credits)) {
          auto body = installed_credits(data_);
          credits_.show(label(134, "Credits"), body.empty() ? "Credits unavailable" : body);
          mode_ = Mode::Credits;
        }
        if (clicked(quit) || in_.hit(SDL_SCANCODE_ESCAPE)) quit_ = true;
        break;
      }
      case Mode::Campaign: {
        background(284);
        text(400, 192, label(33, "Campaign"), 1, gold, true);
        const int pages = std::max(1, int((campaign_.size() + 8) / 9));
        Button load = control(FrontendLayout::top_left, label(138, "Load Game"));
        Button options = control(FrontendLayout::top_middle, label(48, "Options")); // G: shortcut
        Button custom = control(FrontendLayout::top_right, label(37, "Custom Mission"));
        Button play = control(FrontendLayout::bottom_left, label(139, "Play Mission"));
        Button begin = control(FrontendLayout::bottom_middle, label(140, "Begin Campaign"));
        Button back = control(FrontendLayout::bottom_right, label(148, "Cancel"));
        Button previous = control(FrontendLayout::previous, "<"), next = control(FrontendLayout::next, ">");
        for (int row = 0; row < 9; ++row) {
          const int i = campaign_page_ * 9 + row;
          if (i >= int(campaign_.size())) break;
          const auto& entry = campaign_[size_t(i)];
          Button item = control(FrontendLayout::campaign_row(row),
              (campaign_selected_ == i ? "> " : "") + entry.title);
          const bool unlocked = progress_.unlocked(campaign_number(entry.spec));
          draw_button(item, unlocked);
          if (unlocked && clicked(item)) campaign_selected_ = i;
        }
        for (Button* button : {&load, &options, &custom, &begin, &back}) draw_button(*button);
        draw_button(play, campaign_selected_ >= 0);
        draw_button(previous, campaign_page_ > 0); draw_button(next, campaign_page_ + 1 < pages);
        if (clicked(previous) && campaign_page_ > 0) --campaign_page_;
        if (clicked(next) && campaign_page_ + 1 < pages) ++campaign_page_;
        if (clicked(load)) { mode_ = Mode::Load; slots_.show(false); }
        if (clicked(options)) mode_ = Mode::Options;
        if (clicked(custom)) { mode_ = Mode::Custom; custom_page_ = 0; }
        if (clicked(begin) && !campaign_.empty()) enter_briefing(campaign_.front().spec);
        if (campaign_selected_ >= 0 && (clicked(play) || (in_.hit(SDL_SCANCODE_RETURN) && !in_.alt)))
          enter_briefing(campaign_[size_t(campaign_selected_)].spec);
        if (clicked(back) || in_.hit(SDL_SCANCODE_ESCAPE)) mode_ = Mode::Main;
        break;
      }
      case Mode::Load: {
        background_image_ = 284;
        slots_.draw(win_.renderer(), font_, ow_, oh_, std::max(1, int(L_.s)), &store_);
        const int slot = slots_.update(in_, ow_, oh_, std::max(1, int(L_.s)));
        if (slot >= 0) {
          SaveData save;
          resume_save_ = SaveLoadMenu::slot_path(slot);
          if (read_save(resume_save_, save)) chosen_ = save.mission;
          else { resume_save_.clear(); slots_.show(false); }
        }
        if (!slots_.open && chosen_.empty()) mode_ = Mode::Campaign;
        break;
      }
      case Mode::Custom: {
        background(284);
        text(400, 192, label(37, "Custom Mission"), 1, gold, true);
        const int pages = std::max(1, int((custom_.size() + 8) / 9));
        if (in_.wheel < 0) custom_page_ = std::min(pages - 1, custom_page_ + 1);
        if (in_.wheel > 0) custom_page_ = std::max(0, custom_page_ - 1);
        for (int row = 0; row < 9; ++row) {
          const int i = custom_page_ * 9 + row;
          if (i >= int(custom_.size())) break;
          Button item = control(FrontendLayout::campaign_row(row), custom_[size_t(i)].title);
          draw_button(item);
          if (clicked(item)) enter_briefing(custom_[size_t(i)].spec);
        }
        Button previous = control(FrontendLayout::previous, "<"), next = control(FrontendLayout::next, ">");
        Button back = control(FrontendLayout::bottom_right, label(148, "Cancel"));
        draw_button(previous, custom_page_ > 0); draw_button(next, custom_page_ + 1 < pages); draw_button(back);
        if (clicked(previous) && custom_page_ > 0) --custom_page_;
        if (clicked(next) && custom_page_ + 1 < pages) ++custom_page_;
        if (clicked(back) || in_.hit(SDL_SCANCODE_ESCAPE)) mode_ = Mode::Campaign;
        break;
      }
      case Mode::Briefing: {
        const uint32_t backgrounds[] = {270, 290, 264};
        background(brief_reading_ ? backgrounds[brief_section_] : 285);
        // G: text margins require native captures; section art and text are loaded independently.
        const SDL_Rect text_boxes[] = {{140, 130, 520, 340}, {235, 205, 330, 270}, {265, 170, 230, 290}};
        const auto box = text_boxes[brief_section_];
        std::vector<std::string> lines = wrap_text(font_, brief_sections_[brief_section_], L_.W(box.w), 2 * L_.msize(1.2f));
        int lh = font_.height(2 * L_.msize(1.2f)) + 3;
        const int count = std::max(1, L_.W(box.h) / lh);
        if (in_.wheel < 0 || in_.hit(SDL_SCANCODE_DOWN)) brief_scroll_++;
        if (in_.wheel > 0 || in_.hit(SDL_SCANCODE_UP)) brief_scroll_--;
        if (in_.hit(SDL_SCANCODE_PAGEDOWN)) brief_scroll_ += count;
        if (in_.hit(SDL_SCANCODE_PAGEUP)) brief_scroll_ -= count;
        if (in_.hit(SDL_SCANCODE_HOME)) brief_scroll_ = 0;
        if (in_.hit(SDL_SCANCODE_END)) brief_scroll_ = int(lines.size());
        brief_scroll_ = std::clamp(brief_scroll_, 0, std::max(0, int(lines.size()) - count));
        for (int i = 0; brief_reading_ && i < count && i + brief_scroll_ < int(lines.size()); ++i)
          font_.draw(win_.renderer(), L_.X(box.x), L_.Y(box.y) + i * lh, lines[size_t(i + brief_scroll_)], 2 * L_.msize(1.2f), {40, 25, 15, 255}, false);
        Button start = control(FrontendLayout::briefing_start, label(150, "Start Game"));
        Button back = control(FrontendLayout::briefing_back, label(141, "Back"));
        Button diary = control(FrontendLayout::briefing_diary, "Diary");
        Button order = control(FrontendLayout::briefing_order, label(34, "Mission"));
        Button objectives = control(FrontendLayout::briefing_objectives, label(55, "Mission objectives"));
        // V: common 256/271/272 are full-width transparent illustrated controls.
        auto illustrated = [&](const Button& button, uint32_t image, int y) {
          observed_buttons_.push_back({button, true, hover(button)});
          if (!hover(button)) return;
          const auto& sprite = store_.get(common_key(image));
          if (!sprite.ok()) return;
          auto source = sprite.frame_rect(0);
          SDL_Rect dest{L_.X(0), L_.Y(y), L_.W(sprite.cw), L_.W(sprite.ch)};
          SDL_RenderCopy(win_.renderer(), store_.atlas().page(sprite.region.page), &source, &dest);
        };
        illustrated(start, 256, 330); illustrated(back, 272, 0); illustrated(diary, 271, 0);
        if (!brief_reading_) {
          observed_buttons_.push_back({order, true, hover(order)});
          observed_buttons_.push_back({objectives, true, hover(objectives)});
        }
        if (clicked(start) || (in_.hit(SDL_SCANCODE_RETURN) && !in_.alt)) chosen_ = brief_spec_;
        if (clicked(diary) || (!brief_reading_ && (clicked(order) || clicked(objectives)))) {
          brief_section_ = clicked(diary) ? 0 : clicked(order) ? 1 : 2;
          brief_scroll_ = 0;
          brief_reading_ = true;
        }
        if (clicked(back) || in_.hit(SDL_SCANCODE_ESCAPE)) {
          if (brief_reading_) brief_reading_ = false;
          else mode_ = brief_campaign_ ? Mode::Campaign : Mode::Custom;
        }
        break;
      }
      case Mode::Credits: {
        background(284);
        credits_.update(in_);
        credits_.draw(win_.renderer(), font_, ow_, oh_, std::max(1, int(L_.s)));
        if (!credits_.open) mode_ = Mode::Main;
        break;
      }
      case Mode::Controls: {
        background(284);
        text(400, 125, "Controls", 1, gold, true);
        const char* lines[] = {"Arrow keys: scroll", "Mouse wheel: zoom", "Left mouse: select", "Right mouse: order",
            "Escape: mission menu", "F11: borderless display", "Alt+Enter: fullscreen"};
        for (int i = 0; i < 7; ++i) text(220, 200 + i * 30, lines[i], 1, gold);
        Button edge{220, 420, 360, 30, std::string("Edge scrolling: ") + (opts_.edge_scroll ? "on" : "off")};
        Button back = control(FrontendLayout::bottom_right, label(141, "Back"));
        draw_button(edge); draw_button(back);
        if (clicked(edge)) {
          opts_.edge_scroll = !opts_.edge_scroll;
          save_frontend_options(user_file("frontend-options.txt"), opts_);
        }
        if (clicked(back) || in_.hit(SDL_SCANCODE_ESCAPE)) mode_ = Mode::Options;
        break;
      }
      case Mode::Audio: {
        background(284);
        audio_options_.update(in_, opts_, ow_, oh_, std::max(1, int(L_.s)));
        audio_options_.draw(win_.renderer(), font_, opts_, ow_, oh_, std::max(1, int(L_.s)), false);
        if (!audio_options_.open) mode_ = Mode::Options;
        break;
      }
      case Mode::Modifications: {
        modification_options_.update(in_, opts_, ow_, oh_, std::max(1, int(L_.s)));
        modification_options_.draw(win_.renderer(), font_, opts_, ow_, oh_, std::max(1, int(L_.s)));
        if (!modification_options_.open) mode_ = Mode::Options;
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
        Button sound{250, 390, 300, 40, "Audio"};
        Button mods{250, 440, 300, 40, "Game modifications"};
        Button controls{560, 495, 190, 40, "Controls"}; draw_button(controls);
        if (clicked(controls)) mode_ = Mode::Controls;
        Button back{250, 495, 300, 40, "Back"};
        for (Button* b : {&b1, &b2, &b3, &b4, &sound, &mods, &back}) draw_button(*b);
        if (clicked(mods)) { mode_ = Mode::Modifications; modification_options_.open = true; }
        if (clicked(sound)) { mode_ = Mode::Audio; audio_options_.open = true; }
        if (clicked(b1)) win_.set_mode(win_.mode() == DisplayMode::Windowed ? DisplayMode::Borderless : win_.mode() == DisplayMode::Borderless ? DisplayMode::Fullscreen : DisplayMode::Windowed);
        if (clicked(b2)) opts_.edge_scroll = !opts_.edge_scroll;
        if (clicked(b3)) opts_.fog = !opts_.fog;
        if (clicked(b4)) opts_.light = !opts_.light;
        if (clicked(b1) || clicked(b2) || clicked(b3) || clicked(b4)) {
          opts_.window.mode = win_.mode();
          if (!save_frontend_options(user_file("frontend-options.txt"), opts_))
            std::fprintf(stderr, "Cannot persist front-end options\n");
        }
        if (clicked(back) || in_.hit(SDL_SCANCODE_ESCAPE)) mode_ = Mode::Campaign;
        break;
      }
    }
    (void)white;
    frame_end();
  }
  out.quit = quit_ && chosen_.empty();
  out.failed = failed_;
  out.mission = chosen_;
  out.resume_save = resume_save_;
  return !out.quit;
}

}  // namespace

bool run_frontend(Window& win, GameData& data, Options& opts, FrontendResult& out, const std::string& open_briefing) {
  Screen s(win, data, opts);
  return s.run(out, open_briefing);
}

SDL_Rect AudioOptions::row(int i, int w, int h, int sc) {
  const int width = std::min(w - 16, 400 * sc);
  return {(w - width) / 2, (h - 6 * 48 * sc) / 2 + i * 48 * sc, width, 36 * sc};
}
void AudioOptions::update(const Input& in, Options& o, int w, int h, int sc) {
  if (!open) return;
  if (in.hit(SDL_SCANCODE_ESCAPE) || (in.hit(SDL_SCANCODE_RETURN) && !in.alt)) open = false;
  if (!in.pressed[SDL_BUTTON_LEFT]) return;
  const SDL_Point point{in.mx, in.my};
  for (int i = 0; i < 6; ++i) {
    const auto rect = row(i, w, h, sc);
    if (!SDL_PointInRect(&point, &rect)) continue;
    if (i < 3) {
      float* volumes[] = {&o.sfx_volume, &o.music_volume, &o.speech_volume};
      *volumes[i] = std::clamp(*volumes[i] + (in.mx < rect.x + rect.w / 2 ? -0.1f : 0.1f), 0.f, 1.f);
    } else if (i == 3) o.acknowledgement = !o.acknowledgement;
    else if (i == 4) o.subtitles = !o.subtitles;
    else open = false;
    if (i < 5 && !save_frontend_options(user_file("frontend-options.txt"), o))
      std::fprintf(stderr, "Cannot persist audio options\n");
  }
}
void AudioOptions::draw(SDL_Renderer* r, const Font& font, const Options& o, int w, int h, int sc, bool backdrop) const {
  if (!open) return;
  SDL_Rect all{0, 0, w, h};
  if (backdrop) { SDL_SetRenderDrawColor(r, 20, 18, 14, 255); SDL_RenderFillRect(r, &all); }
  std::string labels[] = {"-  Effects: " + std::to_string(int(std::lround(o.sfx_volume * 100))) + "%  +",
      "-  Music: " + std::to_string(int(std::lround(o.music_volume * 100))) + "%  +",
      "-  Speech: " + std::to_string(int(std::lround(o.speech_volume * 100))) + "%  +",
      std::string("Acknowledgements: ") + (o.acknowledgement ? "on" : "off"),
      std::string("Subtitles: ") + (o.subtitles ? "on" : "off"), "Back"};
  for (int i = 0; i < 6; ++i) {
    const auto rect = row(i, w, h, sc);
    SDL_SetRenderDrawColor(r, 55, 42, 25, 255); SDL_RenderFillRect(r, &rect);
    const int ts = 2 * sc;
    font.draw(r, rect.x + (rect.w - font.width(labels[i], ts)) / 2,
              rect.y + (rect.h - font.height(ts)) / 2, labels[i], ts, {245, 230, 200, 255});
  }
}

SDL_Rect ModificationOptions::row(int i, int w, int h, int sc) {
  return {(w - std::min(w - 16, 600 * sc)) / 2, h / 2 - 110 * sc + i * 44 * sc,
          std::min(w - 16, 600 * sc), 36 * sc};
}
void ModificationOptions::update(const Input& in, Options& options, int w, int h, int sc) {
  if (!open) return;
  if (in.hit(SDL_SCANCODE_ESCAPE)) open = false;
  if (!in.pressed[SDL_BUTTON_LEFT]) return;
  SDL_Point point{in.mx, in.my};
  for (int i = 0; i < 6; ++i) {
    const auto rect = row(i, w, h, sc);
    if (!SDL_PointInRect(&point, &rect)) continue;
    if (i == 5) { open = false; return; }
    if (i == 1) options.modifications.set_profile(options.modifications.profile == PlayProfile::Faithful ? "enhanced" : "faithful");
    else {
      const size_t index = i == 0 ? 0 : size_t(i - 1);
      const auto training = (options.modifications.parallel_training + 1) % 9;
      options.modifications.set(std::string(Modifications::names[index]) + "=" +
          (index == 1 ? (training == 0 ? "per-building" : std::to_string(training)) :
           options.modifications.value(index) ? "off" : "on"));
    }
    if (!save_modifications(options.modifications_file, options.modifications))
      std::fprintf(stderr, "Cannot persist game modifications\n");
    return;
  }
}
void ModificationOptions::draw(SDL_Renderer* r, const Font& font, const Options& options, int w, int h, int sc) const {
  if (!open) return;
  SDL_SetRenderDrawColor(r, 20, 18, 14, 255); SDL_RenderClear(r);
  const auto top = row(0, w, h, sc);
  font.draw(r, top.x, top.y - 44 * sc, "Game modifications (F9)", 2 * sc, {245, 230, 200, 255});
  for (int i = 0; i < 6; ++i) {
    const auto rect = row(i, w, h, sc);
    std::string label;
    if (i == 5) label = "Back";
    else if (i == 1) label = std::string("Profile: ") + options.modifications.profile_name();
    else {
      const size_t index = i == 0 ? 0 : size_t(i - 1);
      label = options.modifications.setting(index);
      if (options.modifications.overrides & (1u << index)) label += " (override)";
    }
    SDL_SetRenderDrawColor(r, 55, 42, 25, 255); SDL_RenderFillRect(r, &rect);
    font.draw(r, rect.x + 8 * sc, rect.y + 8 * sc, label, sc, {245, 230, 200, 255});
  }
  font.draw(r, top.x, top.y + 272 * sc, "Profile resets overrides. Training: 1-8 or per building.", sc, {245, 230, 200, 255});
  font.draw(r, top.x, top.y + 288 * sc, "Unit unstick: repath, yield and relocate stalled units.", sc, {245, 230, 200, 255});
}

// ---------------------------------------------------------------------------------------------------------------------

bool play_video(Window& win, const std::string& path) {
#ifdef _WIN32
  // cmd.exe expands these even inside quotes. Decline such paths rather than interpreting them.
  if (path.find_first_of("\"%!\r\n") != std::string::npos) return false;
  std::string input = "\"" + path + "\"";
  const char* redirect = " 2>NUL";
#else
  std::string input = "'";
  for (char c : path) input += c == '\'' ? "'\\''" : std::string(1, c);
  input += "'";
  const char* redirect = " 2>/dev/null";
#endif
  std::string cmd = "ffmpeg -loglevel quiet -i " + input +
      " -vf scale=576:420 -r 12 -f rawvideo -pix_fmt rgb24 -" + redirect;
#ifdef _WIN32
  FILE* p = _popen(cmd.c_str(), "rb");  // Binary mode preserves raw pixel bytes.
#else
  FILE* p = popen(cmd.c_str(), "r");
#endif
  if (!p) return false;
  SDL_Renderer* r = win.renderer();
  SDL_Texture* tex = SDL_CreateTexture(r, SDL_PIXELFORMAT_RGB24, SDL_TEXTUREACCESS_STREAMING, 576, 420);
  std::vector<uint8_t> buf(576 * 420 * 3);
  if (!tex) {
#ifdef _WIN32
    _pclose(p);
#else
    pclose(p);
#endif
    return false;
  }
  bool any = false, skip = false, closed = false;
  while (!skip && std::fread(buf.data(), 1, buf.size(), p) == buf.size()) {
    any = true;
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
      win.handle_event(e);
      if (e.type == SDL_QUIT) closed = true;
      if (e.type == SDL_QUIT || e.type == SDL_KEYDOWN || e.type == SDL_MOUSEBUTTONDOWN) skip = true;
    }
    int ow, oh;
    win.output_size(ow, oh);
    SDL_UpdateTexture(tex, nullptr, buf.data(), 576 * 3);
    SDL_SetRenderDrawColor(r, 0, 0, 0, 255);
    SDL_RenderClear(r);
    float s = std::min(ow / 576.f, oh / 420.f);
    SDL_Rect d{int((ow - 576 * s) / 2), int((oh - 420 * s) / 2), int(576 * s), int(420 * s)};
    SDL_RenderCopy(r, tex, nullptr, &d);
    SDL_RenderPresent(r);
    SDL_Delay(83);
  }
  SDL_DestroyTexture(tex);
#ifdef _WIN32
  _pclose(p);
#else
  pclose(p);
#endif
  if (closed) { SDL_Event event{}; event.type = SDL_QUIT; SDL_PushEvent(&event); }
  return any;
}

// ---------------------------------------------------------------------------------------------------------------------

SDL_Rect PauseMenu::button(int i, int ow, int oh, int sc) const {
  int w = std::min(ow - 16, 280 * sc), h = 30 * sc;
  return {(ow - w) / 2, (oh - 8 * 38 * sc) / 2 + i * 38 * sc, w, h};
}

PauseMenu::Action PauseMenu::update(const Input& in, int ow, int oh, int sc) {
  if (!open || !in.pressed[SDL_BUTTON_LEFT]) return Action::None;
  static const Action acts[9] = {Action::Resume, Action::Briefing, Action::Audio, Action::Save, Action::Load, Action::Restart, Action::Menu, Action::Quit, Action::Modifications};
  for (int i = 0; i < 9; i++) {
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
  static const char* labels[9] = {"Resume", "Briefing / objectives", "Audio", "Save game", "Load game", "Restart mission", "Main menu", "Quit", "Game modifications"};
  int mx, my;
  SDL_GetMouseState(&mx, &my);
  for (int i = 0; i < 9; i++) {
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
