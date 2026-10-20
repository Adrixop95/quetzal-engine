// Obstacle courses used by the movement experiments (tools/make_zpath.py runs the same layouts in the original game).
// 90x60 tile maps, 2 = open, 0 = wall; units start as a 5-wide block of footmen on the left (tile spacing 1).
#pragma once
#include <string>
#include <vector>
#include "flowfield.hpp"

namespace aztec::path {

struct CourseUnit { int x, y, group; };
struct Course {
  std::string name;
  int w = 90, h = 60;
  std::vector<u8> dat;               // w*h terrain bytes
  Cell start{6, 30}, goal{80, 30};
  std::vector<CourseUnit> units;
  bool swap = false;                 // group 1 (right side) walks back to `start`
  Cell goal_of(int group) const { return group == 0 ? goal : start; }
};

bool make_course(const std::string& name, int nunits, Course& out);   // false for unknown names
const std::vector<std::string>& course_names();

}  // namespace aztec::path
