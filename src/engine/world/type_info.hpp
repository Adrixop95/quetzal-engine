// Static knowledge about object types (UGI ids), for labels, HUD and placeholder shapes.
#pragma once

namespace aztec::engine {

constexpr int kNumTypes = 90;

struct TypeInfo {
  const char* name;  // without the UGI_ prefix
  int kind;          // class kind, see type_info.cpp
  bool is_structure() const { return kind == 2 || kind == 7; }  // Building, Defence
};

const TypeInfo* type_info(int id);  // nullptr if id is out of range

}  // namespace aztec::engine
