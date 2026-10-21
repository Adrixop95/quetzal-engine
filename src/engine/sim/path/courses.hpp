// Obstacle courses used by the movement experiments (tools/make_zpath.py runs the same layouts in the original game).
// 90x60 tile maps, 2 = open, 0 = wall; units start as a 5-wide block of footmen on the left (tile spacing 1).
#pragma once
#include <string>
#include <vector>
#include "flowfield.hpp"

namespace aztec::path {

struct CourseUnit { int x, y, group, type; };
struct CourseGroup { Cell goal; int order_tick; };    // order_tick relative to the first order
struct UnitSpec { int speed_px; bool small; };          // original per-type values (Marine descriptor dword 5 / byte +0x2c)
UnitSpec unit_spec(int type);
struct Course {
  std::string name;
  int w = 90, h = 60;
  std::vector<u8> dat;               // w*h terrain bytes
  Cell start{6, 30}, goal{80, 30};
  std::vector<CourseUnit> units;
  std::vector<CourseGroup> groups;   // one order per group
  Cell goal_of(int group) const { return groups[group].goal; }
};

bool make_course(const std::string& name, int nunits, Course& out);   // false for unknown names
const std::vector<std::string>& course_names();

}  // namespace aztec::path
