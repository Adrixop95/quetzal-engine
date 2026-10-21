// Optional per-type price/build time table read from docs/data/types.csv (columns id,ugi,kind,param,unit_price,build_time,...)
// for tooltips ("Price: %i Time: %i"). Absent file = no prices shown.
#pragma once
#include <string>
#include <vector>

namespace aztec::engine {

struct TypeCost { int price = -1, time = -1; };

class RulesTable {
 public:
  bool load(const std::string& csv_path);
  TypeCost cost(int type) const { return type >= 0 && type < int(v_.size()) ? v_[type] : TypeCost{}; }
  bool loaded() const { return !v_.empty(); }

 private:
  std::vector<TypeCost> v_;
};

}  // namespace aztec::engine
