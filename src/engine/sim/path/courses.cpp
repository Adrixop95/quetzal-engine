#include "courses.hpp"

namespace aztec::path {

namespace {
void rect(Course& c, int x0, int y0, int x1, int y1, u8 v = 0) {
  for (int y = y0; y <= y1; ++y) for (int x = x0; x <= x1; ++x) c.dat[(size_t)y * c.w + x] = v;
}
}  // namespace

const std::vector<std::string>& course_names() {
  static const std::vector<std::string> n = {"open", "gap1", "gap2", "gap4", "corr1", "corr2", "maze", "utrap", "wall", "rooms", "swap", "diag", "park", "mixed"};
  return n;
}

UnitSpec unit_spec(int type) {
  switch (type) {
    case 0: return {9, true}; case 1: return {6, true}; case 2: return {7, true}; case 5: return {5, true}; case 6: return {15, false};
    case 27: return {8, true}; case 28: return {5, true}; case 29: return {10, true}; case 31: return {16, true}; case 32: return {12, false};
    case 53: return {11, true}; case 54: return {10, true}; case 55: return {10, true}; case 56: return {4, false}; case 57: return {4, false}; case 82: return {10, true};
    default: return {9, true};
  }
}

bool make_course(const std::string& name, int n, Course& c) {
  c = Course{}; c.name = name; c.dat.assign((size_t)c.w * c.h, 2);
  for (int x = 0; x < c.w; ++x) for (int y : {0, 1, c.h - 2, c.h - 1}) c.dat[(size_t)y * c.w + x] = 0;
  for (int y = 0; y < c.h; ++y) for (int x : {0, 1, c.w - 2, c.w - 1}) c.dat[(size_t)y * c.w + x] = 0;
  const int H = c.h; bool swap = false, park = false, mixed = false;
  if (name == "open") {}
  else if (name.rfind("gap", 0) == 0) { int w = name.size() > 3 ? std::stoi(name.substr(3)) : 1; rect(c, 44, 2, 46, H - 3); rect(c, 44, 30 - w / 2, 46, 30 - w / 2 + w - 1, 2); }
  else if (name.rfind("corr", 0) == 0) { int w = name.size() > 4 ? std::stoi(name.substr(4)) : 1; rect(c, 40, 2, 60, H - 3); rect(c, 40, 30 - w / 2, 60, 30 - w / 2 + w - 1, 2); }
  else if (name == "maze") {
    for (int i = 0, x = 20; x < 70; x += 10, ++i) { rect(c, x, 2, x + 1, H - 3); if (i % 2 == 0) rect(c, x, H - 8, x + 1, H - 3, 2); else rect(c, x, 2, x + 1, 8, 2); }
  }
  else if (name == "utrap") { rect(c, 50, 14, 52, 46); rect(c, 40, 14, 52, 16); rect(c, 40, 44, 52, 46); c.goal = {62, 30}; }
  else if (name == "wall") rect(c, 44, 6, 46, H - 7);
  else if (name == "rooms") { rect(c, 44, 2, 46, H - 3); rect(c, 44, 14, 46, 15, 2); rect(c, 44, 44, 46, 45, 2); rect(c, 20, 20, 30, 40); }
  else if (name == "swap") { rect(c, 44, 2, 46, H - 3); rect(c, 44, 29, 46, 31, 2); swap = true; }
  else if (name == "park") { rect(c, 40, 2, 60, H - 3); rect(c, 40, 29, 60, 30, 2); park = true; }
  else if (name == "mixed") mixed = true;
  else if (name == "diag") { for (int i = 0; i < 44; ++i) rect(c, 20 + i, 8 + i, 21 + i, 9 + i); rect(c, 62, 50, 64, 52, 2); }
  else return false;
  static const int kMixed[5] = {0, 6, 57, 31, 32};
  c.groups.push_back({c.goal, 0});
  if (swap) c.groups.push_back({c.start, 4});
  int na = n / 3 > 1 ? n / 3 : 1;
  if (park) c.groups = {{{64, 30}, 0}, {c.goal, 200}};
  for (int i = 0; i < n; ++i) {
    int type = mixed ? kMixed[i % 5] : 0, x, y, grp = 0;
    if (swap) { grp = i >= n / 2; int k = i - (grp ? n / 2 : 0); x = c.start.x + (grp ? 66 : 0) + k % 5; y = c.start.y - 4 + k / 5; }
    else if (park) { grp = i < na ? 0 : 1; int k = grp == 0 ? i : i - na; if (grp == 0) { x = 30 + k % 5; y = 28 + k / 5; } else { x = c.start.x + k % 5; y = c.start.y - 4 + k / 5; } }
    else { x = c.start.x + i % 5; y = c.start.y - 4 + i / 5; }
    c.units.push_back({x, y, grp, type});
  }
  return true;
}

}  // namespace aztec::path
