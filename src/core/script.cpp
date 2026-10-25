#include "core/loader_limits.hpp"
#include "core/script.hpp"

#include <cctype>
#include <cstdio>
#include <cstdlib>

#include "core/ugi.hpp"

namespace aztec::script {

const Trigger* Script::find(int id) const {
  for (const auto& t : triggers) if (t.id == id) return &t;
  return nullptr;
}

const char* cmp_text(Cmp c) {
  switch (c) {
    case Cmp::Eq: return "==";
    case Cmp::Ne: return "!=";
    case Cmp::Lt: return "<";
    case Cmp::Le: return "<=";
    case Cmp::Gt: return ">";
    case Cmp::Ge: return ">=";
  }
  return "?";
}

namespace {

struct Fail { std::string msg; int line; };

class Reader {
 public:
  explicit Reader(std::string_view t) : t_(t) {}
  int line() const { return line_; }
  void ws() {
    while (p_ < t_.size() && (std::isspace((unsigned char)t_[p_]) || t_[p_] == 0x1a)) {  // 0x1a = DOS EOF
      if (t_[p_] == '\n') line_++;
      p_++;
    }
  }
  bool eof() { ws(); return p_ >= t_.size(); }
  [[noreturn]] void fail(const char* msg) const { throw Fail{msg, line_}; }

  // Identifier [A-Za-z_0-9]+ (the original used scanf "%[a-zA-Z_0-9]"), lower-cased copy in `lc`.
  std::string ident() {
    ws();
    size_t s = p_;
    while (p_ < t_.size() && (std::isalnum((unsigned char)t_[p_]) || t_[p_] == '_')) p_++;
    return std::string(t_.substr(s, p_ - s));
  }
  std::string peek_ident() {
    size_t sp = p_; int sl = line_;
    std::string r = ident();
    p_ = sp; line_ = sl;
    return r;
  }
  bool accept(char c) {
    ws();
    if (p_ < t_.size() && t_[p_] == c) { p_++; return true; }
    return false;
  }
  void expect(char c, const char* msg) { if (!accept(c)) fail(msg); }
  int integer() {  // "%i": decimal, 0x hex, leading sign
    ws();
    const char* b = t_.data() + p_;
    char* e = nullptr;
    errno = 0;
    long v = std::strtol(b, &e, 0);
    if (errno == ERANGE || v < INT_MIN || v > INT_MAX) fail("Constant out of range");
    if (e == b) fail("Constant expected");
    p_ += size_t(e - b);
    return int(v);
  }
  bool at_digit() {
    ws();
    return p_ < t_.size() && (std::isdigit((unsigned char)t_[p_]) || t_[p_] == '-' || t_[p_] == '+');
  }
  int side_ref() {  // "(" ("local" | int) ")" handled by caller of '(' ; this parses the inside
    ws();
    if (peek_ident() == "local" || lower(peek_ident()) == "local") { ident(); return kLocal; }
    return integer();
  }
  std::string string_literal() {  // "..." up to next quote (scanf "%[^\"]")
    expect('"', "String expected");
    size_t s = p_;
    while (p_ < t_.size() && t_[p_] != '"') { if (t_[p_] == '\n') line_++; p_++; }
    std::string r(t_.substr(s, p_ - s));
    expect('"', "String expected");
    return r;
  }
  Cmp comparison() {
    ws();
    auto two = [&](char a, char b) { return p_ + 1 < t_.size() && t_[p_] == a && t_[p_ + 1] == b; };
    if (two('=', '=')) { p_ += 2; return Cmp::Eq; }
    if (two('!', '=')) { p_ += 2; return Cmp::Ne; }
    if (two('<', '=')) { p_ += 2; return Cmp::Le; }
    if (two('>', '=')) { p_ += 2; return Cmp::Ge; }
    if (p_ < t_.size() && t_[p_] == '<') { p_++; return Cmp::Lt; }
    if (p_ < t_.size() && t_[p_] == '>') { p_++; return Cmp::Gt; }
    fail("Bad comparison operation");
  }
  static std::string lower(std::string s) {
    for (char& c : s) c = char(std::tolower((unsigned char)c));
    return s;
  }
  size_t pos() const { return p_; }

 private:
  std::string_view t_;
  size_t p_ = 0;
  int line_ = 1;
};

std::string lc(std::string s) { return Reader::lower(std::move(s)); }

int paren_int(Reader& r) {
  r.expect('(', "Not enough parameters specified");
  int v = r.integer();
  r.expect(')', "Not enough parameters specified");
  return v;
}
int paren_side(Reader& r) {
  r.expect('(', "You must specify side parameters");
  int v = r.side_ref();
  r.expect(')', "You must specify side parameters");
  return v;
}
void paren_xy(Reader& r, int& x, int& y) {
  r.expect('(', "Not enough parameters specified");
  x = r.integer();
  r.expect(',', "Not enough parameters specified");
  y = r.integer();
  r.expect(')', "Not enough parameters specified");
}
void paren_rect(Reader& r, int* rc) {
  r.expect('(', "Not enough parameters specified");
  for (int i = 0; i < 4; i++) {
    rc[i] = r.integer();
    if (i < 3) r.expect(',', "Not enough parameters specified");
  }
  r.expect(')', "Not enough parameters specified");
}
int ugi_ident(Reader& r) {
  std::string n = r.ident();
  if (n.empty()) r.fail("You must specify unit id");
  int u = ugi_from_name(n);
  if (u < 0) r.fail("Bad UGI");
  return u;
}

Condition parse_condition(Reader& r) {
  Condition c;
  c.line = r.line();
  std::string kw = lc(r.ident());
  if (kw == "always") { c.kind = CondKind::Always; return c; }
  if (kw == "localtime" || kw == "globaltime") {
    c.kind = kw == "localtime" ? CondKind::LocalTime : CondKind::GlobalTime;
  } else if (kw == "timer") {
    c.kind = CondKind::Timer;
    c.id = paren_int(r);
  } else if (kw == "side") {
    c.side = paren_side(r);
    r.expect('.', "Bad method for side");
    std::string m = lc(r.ident());
    if (m == "enemies") c.kind = CondKind::SideEnemies;
    else if (m == "units") c.kind = CondKind::SideUnits;
    else if (m == "towns") c.kind = CondKind::SideTowns;
    else if (m == "tactics") c.kind = CondKind::SideTactics;
    else if (m == "money") c.kind = CondKind::SideMoney;
    else if (m == "unitsinrect") { c.kind = CondKind::SideUnitsInRect; paren_rect(r, c.rect); }
    else if (m == "unitsofugiinrect") {
      c.kind = CondKind::SideUnitsOfUGIInRect;
      r.expect('(', "Not enough parameters specified");
      c.ugi = ugi_ident(r);
      for (int i = 0; i < 4; i++) { r.expect(',', "Not enough parameters specified"); c.rect[i] = r.integer(); }
      r.expect(')', "Not enough parameters specified");
    } else if (m == "unitsofid") {
      c.kind = CondKind::SideUnitsOfId;
      r.expect('(', "You must specify unit id");
      c.ugi = ugi_ident(r);
      r.expect(')', "You must specify unit id");
    } else if (m == "tactic") {
      c.id = paren_int(r);
      r.expect('.', "You must specify operation with tactic");
      std::string t = lc(r.ident());
      if (t == "valid") c.kind = CondKind::TacticValid;
      else if (t == "attacked") c.kind = CondKind::TacticAttacked;
      else if (t == "idle") c.kind = CondKind::TacticIdle;
      else if (t == "locked") c.kind = CondKind::TacticLocked;
      else r.fail("Bad tactic method");
    } else if (m == "defence") {
      c.id = paren_int(r);
      r.expect('.', "You must specify operation with defence");
      if (lc(r.ident()) != "valid") r.fail("Bad defence method");
      c.kind = CondKind::DefenceValid;
    } else if (m == "isfriendly" || m == "isneutral" || m == "isenemy") {
      c.kind = m == "isfriendly" ? CondKind::IsFriendly : m == "isneutral" ? CondKind::IsNeutral : CondKind::IsEnemy;
      c.id = paren_side(r);
    } else {
      r.fail("Bad method for side");
    }
  } else {
    r.fail("Error loading condition.");
  }
  c.cmp = r.comparison();
  c.value = r.integer();
  return c;
}

Action parse_tactic_action(Reader& r, Action a) {
  a.id = paren_int(r);
  r.expect('.', "You must specify operation with tactic");
  std::string m = lc(r.ident());
  if (m == "attacktown") { a.kind = ActKind::TacticAttackTown; a.a = paren_int(r); }
  else if (m == "moveto") { a.kind = ActKind::TacticMoveTo; paren_xy(r, a.a, a.b); }
  else if (m == "jointactic") { a.kind = ActKind::TacticJoinTactic; a.a = paren_int(r); }
  else if (m == "setpath") { a.kind = ActKind::TacticSetPath; a.a = paren_int(r); }
  else if (m == "setinvertedpath") { a.kind = ActKind::TacticSetInvertedPath; a.a = paren_int(r); }
  else if (m == "setcycledpath") { a.kind = ActKind::TacticSetCycledPath; a.a = paren_int(r); }
  else if (m == "setcycledinvertedpath") { a.kind = ActKind::TacticSetCycledInvertedPath; a.a = paren_int(r); }
  else if (m == "lock") a.kind = ActKind::TacticLock;
  else if (m == "unlock") a.kind = ActKind::TacticUnlock;
  else if (m == "giftToSide" || m == "gifttoside") { a.kind = ActKind::TacticGiftToSide; a.a = paren_side(r); }
  else if (m == "kill") a.kind = ActKind::TacticKill;
  else r.fail("Bad tactic method");
  return a;
}

Action parse_action(Reader& r) {
  Action a;
  a.line = r.line();
  std::string kw = lc(r.ident());
  if (kw == "trigger") {
    a.id = paren_int(r);
    r.expect('.', "You must specify operation with trigger");
    std::string m = lc(r.ident());
    if (m == "enable") a.kind = ActKind::TriggerEnable;
    else if (m == "disable") a.kind = ActKind::TriggerDisable;
    else r.fail("Bad method for trigger");
  } else if (kw == "boat") {
    a.id = paren_int(r);
    r.expect('.', "You must specify operation with boat");
    if (lc(r.ident()) != "activate") r.fail("Bad boat method");
    a.kind = ActKind::BoatActivate;
  } else if (kw == "johnka") {
    a.id = paren_int(r);
    r.expect('.', "You must specify operation with johnka");
    std::string m = lc(r.ident());
    if (m == "enable") a.kind = ActKind::JohnkaEnable;
    else if (m == "disable") a.kind = ActKind::JohnkaDisable;
    else r.fail("Bad johnka method");
  } else if (kw == "submarine") {
    a.id = paren_int(r);
    r.expect('.', "You must specify operation with submarine");
    if (lc(r.ident()) != "go") r.fail("Bad submarine method");
    a.kind = ActKind::SubmarineGo;
  } else if (kw == "timer") {
    a.id = paren_int(r);
    r.expect('.', "You must specify operation with timer");
    std::string m = lc(r.ident());
    if (m == "set") { a.kind = ActKind::TimerSet; a.a = paren_int(r); }
    else if (m == "start") a.kind = ActKind::TimerStart;
    else if (m == "stop") a.kind = ActKind::TimerStop;
    else if (m == "kill") a.kind = ActKind::TimerKill;
    else r.fail("Bad method for timer");
  } else if (kw == "centerview") {
    a.kind = ActKind::CenterView;
    paren_xy(r, a.a, a.b);
  } else if (kw == "printstring" || kw == "printstringr") {
    a.kind = kw == "printstring" ? ActKind::PrintString : ActKind::PrintStringR;
    r.expect('(', "String expected");
    a.text = r.string_literal();
    r.expect(')', "String expected");
  } else if (kw == "side") {
    a.side = paren_side(r);
    r.expect('.', "You must specify operation with side");
    std::string m = lc(r.ident());
    if (m == "defeat") a.kind = ActKind::SideDefeat;
    else if (m == "victory") a.kind = ActKind::SideVictory;
    else if (m == "tactic") return parse_tactic_action(r, a);
    else if (m == "buildcommand") {
      a.kind = ActKind::SideBuildCommand;
      r.expect('(', "Not enough parameters specified");
      a.ugi = ugi_ident(r);
      while (r.accept(',')) a.args.push_back(r.integer());
      r.expect(')', "Not enough parameters specified");
    } else if (m == "movecommand") { a.kind = ActKind::SideMoveCommand; paren_xy(r, a.a, a.b); }
    else if (m == "cameraactivate") { a.kind = ActKind::SideCameraActivate; paren_xy(r, a.a, a.b); }
    else if (m == "cameradeactivate") { a.kind = ActKind::SideCameraDeactivate; a.a = paren_int(r); }
    else if (m == "setfriendlyto") { a.kind = ActKind::SideSetFriendlyTo; a.a = paren_side(r); }
    else if (m == "setneutralto") { a.kind = ActKind::SideSetNeutralTo; a.a = paren_side(r); }
    else if (m == "setenemyto") { a.kind = ActKind::SideSetEnemyTo; a.a = paren_side(r); }
    else if (m == "addmoney") { a.kind = ActKind::SideAddMoney; a.a = paren_int(r); }
    else if (m == "takemoney") { a.kind = ActKind::SideTakeMoney; a.a = paren_int(r); }
    else r.fail("Bad side method");
  } else {
    r.fail("Error loading action.");
  }
  return a;
}

bool is_keyword(const std::string& id, const char* k) { return lc(id) == k; }

}  // namespace

bool parse_script(std::string_view text, Script& out) {
  out = Script{};
  if (text.size() > detail::kTextBytes) return false;
  // Reader::integer uses strtol, which requires a terminated buffer.
  const std::string terminated(text);
  Reader r(terminated);
  try {
    while (!r.eof()) {
      std::string kw = r.ident();
      if (!is_keyword(kw, "trigger")) r.fail("Error creating trigger");
      Trigger t;
      t.id = paren_int(r);
      if (!is_keyword(r.ident(), "conditions")) r.fail("Not all parameters defined for trigger");
      for (;;) {
        std::string nx = lc(r.peek_ident());
        if (nx == "actions") { r.ident(); break; }
        if (nx.empty() || nx == "end") r.fail("Not all parameters defined for trigger");
        t.conditions.push_back(parse_condition(r));
      }
      for (;;) {
        std::string nx = lc(r.peek_ident());
        if (nx == "end") { r.ident(); break; }
        if (nx.empty()) r.fail("'}' expected.");
        t.actions.push_back(parse_action(r));
      }
      out.triggers.push_back(std::move(t));
    }
  } catch (const Fail& f) {
    out.error = "line " + std::to_string(f.line) + ": " + f.msg;
    out.error_line = f.line;
    return false;
  }
  return true;
}

bool load_script(const std::string& path, Script& out) {
  FILE* f = std::fopen(path.c_str(), "rb");
  if (!f) return false;
  std::string text;
  char buf[4096];
  size_t n;
  while ((n = std::fread(buf, 1, sizeof buf, f)) > 0) {
    if (text.size() + n > detail::kTextBytes) { std::fclose(f); return false; }
    text.append(buf, n);
  }
  std::fclose(f);
  return parse_script(text, out);
}

}  // namespace aztec::script
