#include "core/savegame.hpp"
#include "core/loader_limits.hpp"
#include "core/mapfiles.hpp"
#include <initializer_list>
#include <utility>

namespace aztec {
const SaveField* SaveRecord::field(int offset, const std::string& scope) const {
  for (const auto& f : fields) if (f.memory_offset == offset && f.scope == scope) return &f;
  return nullptr;
}
const SaveSection* SaveGame::section(const std::string& name) const {
  for (const auto& s : sections) if (s.name == name) return &s;
  return nullptr;
}
int save_unit_kind(uint32_t t) {
  // Class membership is part of the file grammar (also listed in object_types.md).
  if (t >= 90) return -1;
  if (t == 89) return 11;
  if (t == 88) return 10;
  if (t == 84) return 8;
  if (t == 87) return 9;
  if (t == 7 || t == 85) return 3;
  if (t == 33 || t == 34 || t == 60) return 5;
  if (t == 30 || t == 35 || t == 36) return 1;
  if (t == 3 || t == 4 || t == 8 || t == 9 || t == 58 || t == 59 || t == 61 || t == 62) return 6;
  if (t == 83) return 4;
  if (t == 17 || t == 23 || t == 26 || t == 42 || t == 49 || t == 86 || t == 69 || t == 80 || t == 81) return 7;
  if ((t >= 10 && t <= 26) || (t >= 37 && t <= 52) || (t >= 63 && t <= 81)) return 2;
  return 0;
}
int save_weapon_kind(uint32_t t) {
  static constexpr int kinds[] = {14,1,0,7,2,3,3,12,4,2,2,10,2,5,1,2,6,7,7,8,8,9,3,9,3,7,3,9,3,7,0,1,0,2,11,12,1,7,7,12};
  return t < 40 ? kinds[t] : -1;
}
namespace {
using V = SaveValue;
struct Invalid { size_t offset; const char* message; };
class Reader {
  const std::vector<uint8_t>& data;
  SaveGame& game;
  size_t pos = 0, field_count = 0;
  SaveRecord* record = nullptr;
  std::string scope = "object";
  void require(size_t n) { if (n > data.size() - pos) fail("Truncated save game"); }
  void add(size_t start, size_t n, int mem, V kind, uint32_t value, const char* name) {
    if (!record) return;
    if (++field_count > 1024 * 1024) fail("Too many serialized fields");
    record->fields.push_back({scope, name, mem, start, n, kind, value});
  }
public:
  Reader(const std::vector<uint8_t>& d, SaveGame& g) : data(d), game(g) {}
  [[noreturn]] void fail(const char* message) const { throw Invalid{pos, message}; }
  uint32_t word(int mem = -1, V kind = V::word, const char* name = "") {
    require(4); size_t start = pos;
    uint32_t v = uint32_t(data[pos]) | uint32_t(data[pos+1]) << 8 |
      uint32_t(data[pos+2]) << 16 | uint32_t(data[pos+3]) << 24;
    pos += 4; add(start, 4, mem, kind, v, name); return v;
  }
  void block(size_t n, int mem = -1, const char* name = "") {
    require(n); size_t start = pos; pos += n; add(start, n, mem, V::bytes, 0, name);
  }
  void members(std::initializer_list<int> offsets, V kind = V::word) {
    for (int o : offsets) word(o, kind);
  }
  size_t count(size_t stride = 4, int mem = -1, const char* name = "count") {
    size_t n = word(mem, V::word, name);
    if (n > detail::kEntries || n > (data.size() - pos) / stride) fail("Invalid count");
    return n;
  }
  void array(size_t n, size_t stride, const char* name) {
    if (n > detail::kCells || (n && stride > (data.size() - pos) / n)) fail("Invalid array size");
    block(n * stride, -1, name);
  }
  void refs(V kind) { size_t n = count(); for (size_t i=0; i<n; ++i) word(-1, kind); }
  void animation(int offset) {
    auto previous = scope; scope += ".animation" + std::to_string(offset);
    word(0, V::word, "animation_id");
    if (word(-1, V::boolean, "has_frames")) { word(8, V::word, "sequence_header_frame"); word(0x10, V::word, "current_frame"); }
    word(0xc, V::boolean, "loop"); word(0x14, V::word, "frame_tick"); word(0x18, V::boolean, "frame_changed"); scope = previous;
  }
  void visible() { members({0x10,0x14,0x18,0x1c}); word(0x20,V::boolean); members({0x24,0x28}); word(0x2c,V::boolean); }
  void world() {
    visible(); word(0x30,V::object_ref); members({0x34,0x38,0x3c,0x40,0x44,0x48}); word(0x4c,V::short_integer);
    members({0x50,0x54,0x58}); members({0x5c,0x5d,0x5e,0x5f},V::boolean);
  }
  void unit(int kind) {
    world();
    switch (kind) {
    case 0:
      word(0x60,V::byte); members({0x64,0x6c,0x70,0x74,0x78,0x80,0x84,0x88}); members({0x8c,0x90,0x94},V::unit_ref);
      members({0x98,0x9c}); word(0xa0,V::effect_ref); word(0xa4); word(0x68,V::descriptor); animation(0x7c); break;
    case 1:
      word(0x60,V::byte); members({0x64,0x68,0x78,0x7c,0x80,0x84,0x88,0x8c}); members({0x90,0x91,0x92},V::byte); word(0x94);
      members({0x98,0x9c,0xa0},V::unit_ref); word(0x6c,V::descriptor); animation(0x70); animation(0x74); break;
    case 2:
      members({0x60,0x8c},V::byte); members({0x68,0x78,0x7c}); word(0x80,V::decoration_ref); word(0x84,V::effect_ref);
      word(0x88,V::transport_ref); word(0x64,V::descriptor); animation(0x6c);
      for (int o : {0x70,0x74}) if (word(o,V::boolean,"present")) {
        scope = "object.harvest" + std::to_string(o); word(4,V::script_ref); members({8,0xc}); scope = "object";
      }
      break;
    case 3: case 5:
      members({0x60,0x68,0x6c,0x70,0x74,0x78,0x7c,0x80,0x84}); word(0x88,V::byte);
      if (kind == 5) { members({0x8c,0x90}); word(0x94,V::unit_ref); }
      else word(0x90,V::unit_ref);
      word(0x64,V::descriptor); animation(kind == 5 ? 0x98 : 0x8c); break;
    case 4:
      word(0x60,V::byte); members({0x64,0x6c,0x70,0x74,0x78,0x7c,0x80,0x84}); members({0x88,0x8c},V::unit_ref);
      members({0x90,0x91},V::byte); word(0x98); word(0x68,V::descriptor); animation(0x94); break;
    case 6:
      word(0x60,V::byte); members({0x64,0x68,0x70,0x74,0x78,0x7c,0x80,0x84,0x88,0x8c}); members({0x90,0x94},V::unit_ref);
      word(0x98,V::byte); members({0xa4,0xa8,0xac}); word(0x6c,V::descriptor); animation(0x9c); animation(0xa0); break;
    case 7:
      members({0x60,0x9c},V::byte); members({0x64,0x68}); members({0x78,0x7c},V::unit_ref); members({0x80,0x84,0x88,0x8c});
      word(0x90,V::decoration_ref); word(0x94,V::effect_ref); word(0x98,V::transport_ref); word(0x6c,V::descriptor); animation(0x70);
      if (word(0x74,V::boolean,"present")) animation(0x74);
      break;
    case 8: case 10:
      members({0x60,0x68}); if (kind == 10) { word(0x6c,V::unit_ref); word(0x70); } animation(0x64); break;
    case 9:
      word(0x60,V::byte); members({0x64,0x70,0x74,0x78,0x7c}); members({0x80,0x81},V::byte); word(0x84);
      members({0x88,0x8c,0x90},V::unit_ref); word(0x68,V::descriptor); animation(0x6c); break;
    case 11: members({0x60,0x64}); break;
    default: fail("Unknown unit kind");
    }
  }
  void tactic() {
    word(-1,V::word,"tactic_serial"); word(4); word(8,V::side_ref); members({0x10,0x14,0x1c,0x20,0x24}); word(0x28,V::short_integer);
    members({0x2c,0x30,0x34}); size_t triples = word(0x3c); members({0x40,0x44,0x48,0x4c,0x50,0x54}); word(0x58,V::unit_ref);
    uint32_t first_unit = word(0x60,V::byte); word(0x61,V::byte); members({0x64,0x68}); members({0x6c,0x6d},V::byte); word(0x70);
    size_t columns = word(0x78), rows = word(0x7c); word(0x80,V::unit_ref); members({0x84,0x88}); members({0x8c,0x94},V::byte);
    word(0x98,V::object_ref); members({0x9c,0x9d},V::byte); word(0xac); word(0xb0,V::byte); word(0xb4); word(0xb8,V::byte);
    members({0xbc,0xc0}); members({0xc4,0xc6},V::short_integer); members({0xd8,0xd9,0xe8,0xe9,0xea},V::byte);
    members({0xec,0x90}); word(0x18,V::byte);
    size_t n = count(44); auto previous = scope; scope = "tactic.member";
    for (size_t i=0;i<n;++i) { word(0,V::unit_ref); members({4,8,0xc,0x10}); members({0x14,0x15},V::byte); members({0x16,0x1c,0x20,0x24}); }
    scope = previous; array(triples,12,"points"); n = count();
    for (size_t i=0;i<n;++i) word(-1,i==0 && first_unit ? V::unit_ref : V::object_ref,"targets");
    if (word(0x74,V::boolean,"route_present")) {
      if (rows > detail::kEntries || columns > detail::kEntries) fail("Route dimensions too large");
      for(size_t i=0;i<rows;++i) { word(-1,V::unit_ref,"route_unit"); word(); word(); }
      for(size_t i=0;i<columns;++i) { word(-1,V::unit_ref,"column_unit"); word(); }
      if (rows && columns > detail::kCells / rows) fail("Route matrix too large");
      array(rows*columns,4,"route_matrix");
    }
    for (const char* name : {"pending_commands","deferred_commands"}) { n=count(8); array(n,8,name); }
    refs(V::object_ref);
  }
  void queue() { size_t n=count(4,0x14); members({0x10,0xc,8}); for(size_t i=0;i<n;++i) word(-1,V::unit_ref); }
  void town() {
    tactic(); word(0xf0,V::unit_ref); members({0x158,0x15c,0x160,0x164}); word(0x168,V::unit_ref); members({0x170,0x171},V::byte);
    members({0x174,0x17c,0x180,0x18c,0x190,0x194,0x198,0x1b0,0x1b4}); word(-1,V::byte,"town_global"); word(0x178);
    word(0x16c,V::object_ref); block(0x14,0x19c);
    scope="town.queue"; queue(); scope="town.harvest"; members({8,0xc,0x10}); word(0x14,V::byte); word(4,V::unit_ref);
    scope="town.units"; members({0x28,0x2c}); refs(V::unit_ref); scope="object";
    word(0x184,V::object_ref); word(0x188,V::order_ref);
    scope="town.build_tree"; block(0x16c,0x16c); block(0x168,4); block(0x1188,0x2e8); word(0x1470,V::descriptor);
    members({0x2e0,0x2e1,0x2e2},V::byte); word(0x2e4); scope="object";
  }
  void side() {
    members({4,8,0xc,0x10,0x14}); word(0x138,V::byte); members({0x13c,0x140,0x698,0x63c,0x640,0x644,0x648,0x64c,0x650});
    word(0x694,V::order_ref); word(0x688,V::object_ref);
    block(0x20,0x18,"name"); block(0x100,0x38); block(0x16c,0x144); block(0x5b,0x2b0,"disabled_types");
    block(0x16c,0x30c); block(0x168,0x478); block(0x5a,0x5e0);
    refs(V::object_ref); refs(V::object_ref); refs(V::unit_ref); refs(V::order_ref);
    if (word(0x68c,V::boolean,"explorer_present")) {
      size_t n=count(364); for(size_t i=0;i<n;++i) { word(-1,V::object_ref); block(0x168,-1,"explorer_state"); }
    }
    size_t w=word(-1,V::word,"fog_width"), h=word(-1,V::word,"fog_height");
    if (w > detail::kCells || h > detail::kCells || (w && h > detail::kCells/w)) fail("Fog too large");
    array(w*h,1,"fog");
    size_t n=count(20); array(n,20,"cameras");
  }
  void weapon(int kind) {
    visible(); word(0x30,V::unit_ref); word(0x34);
    switch(kind) {
    case 0: members({0x38,0x44,0x48,0x4c,0x50,0x54,0x58,0x5c,0x60,0x64}); word(0x3c,V::descriptor); animation(0x40); break;
    case 1: members({0x38,0x40,0x44,0x48,0x4c,0x50,0x54,0x58,0x5c,0x60}); word(0x64,V::unit_ref); word(0x3c,V::descriptor); animation(0x68); break;
    case 2: case 9: case 12: case 13: word(0x38,V::descriptor); break;
    case 3: members({0x38,0x40,0x44,0x48,0x4c,0x50,0x54,0x58}); word(0x3c,V::descriptor); animation(0x5c); break;
    case 4: case 5: case 6: case 10: break;
    case 7: members({0x3c,0x44}); word(0x38,V::descriptor); animation(0x40); break;
    case 8: members({0x38,0x40,0x104}); word(0x3c,V::descriptor); block(16*12,0x44,"splash_entries"); break;
    case 11: word(0x3c); animation(0x38); break;
    case 14: word(0x38,V::unit_ref); members({0x3c,0x40,0x44}); break;
    default: fail("Unknown weapon kind");
    }
  }
  void script(uint32_t kind) {
    word(4);
    if (kind == 0) { word(8); return; }
    if (kind == 4) { size_t n=count(8); array(n,8,"path_points"); return; }
    if (kind > 4) fail("Unknown script kind");
    word(8); word(0xc,V::byte);
    if (kind == 1) {
      size_t n=count(0x408); array(n,0x408,"conditions"); n=count(0x408); array(n,0x408,"actions");
    } else if (kind == 2) {
      if (word(0x14,V::byte)) { size_t n=count(4,0x20); word(0x1c); array(n,4,"timer_samples"); }
      else members({0x18,0x1c});
    }
  }
  void ui_level(const char* name) {
    scope=name; size_t n=count(12); word(0x28);
    for(size_t i=0;i<n;++i) { word(0xc,V::object_ref); word(0x14,V::byte); word(0x10); }
  }
  void parse() {
    block(0x158); game.view_x = static_cast<int32_t>(word()); game.view_y = static_cast<int32_t>(word());
    for(auto& v : game.globals) v=word();
    for(const char* name : {"decorations","towns","sides","tactics","units","weapons","effects","transports","scripts","orders","ui","daylight"}) {
      game.sections.push_back({name,pos,0,{}}); auto& section=game.sections.back();
      bool single=section.name=="ui" || section.name=="daylight";
      size_t n=single ? 1 : count();
      for(size_t i=0;i<n;++i) {
        section.records.push_back({}); record=&section.records.back(); record->offset=pos; scope="object";
        const auto& s=section.name;
        if (!single && s!="sides" && s!="weapons") record->id=word(-1,V::word,"id");
        if (s=="decorations") { visible(); word(0x34); animation(0x30); }
        else if (s=="towns") {
          if (!(record->id & 0x80000000u)) fail("Unsupported town marker");
          town();
        } else if (s=="sides") side();
        else if (s=="tactics") {
          record->kind=word(-1,V::word,"kind");
          if (record->kind==1 || record->kind>5) fail("Unknown tactic kind");
          tactic();
          if(record->kind==2) { word(0xf0); word(0xf4,V::object_ref); word(0xf8); word(0xfc,V::side_ref); }
          if(record->kind==3) word(0xf0);
          if(record->kind==4) members({0xf0,0xf1},V::byte);
        } else if (s=="units" || s=="weapons") {
          record->type=word(-1,V::word,"type"); int k=s=="units" ? save_unit_kind(record->type) : save_weapon_kind(record->type);
          if(k<0) fail("Unknown object type");
          record->kind=uint32_t(k);
          if(s=="units") unit(k); else weapon(k);
        } else if (s=="effects") { members({4,8,0xc,0x10,0x14,0x18,0x1c}); block(8,0x20,"double"); }
        else if (s=="transports") {
          visible(); word(0x30,V::unit_ref); members({0x34,0x38,0x3c,0x40,0x44,0x48,0x4c}); word(0x114,V::byte);
          members({0x118,0x11c,0x120,0x124}); members({0x128,0x129,0x12a,0x12b},V::byte); block(0xc4,0x50);
        } else if (s=="scripts") { record->kind=word(-1,V::word,"kind"); script(record->kind); }
        else if (s=="orders") {
          members({0x80c,0x808},V::object_ref); size_t entries=count(8,0x804); word(0x810,V::byte); scope="order.entry";
          for(size_t j=0;j<entries;++j) { uint32_t t=word(0); word(4,t==0 ? V::unit_ref : V::word); }
        } else if (s=="ui") {
          ui_level("ui.selection0"); ui_level("ui.selection1"); scope="ui.panel"; word(0x4c,V::object_ref); word(0x50);
          scope="ui.build"; word(0x4c,V::object_ref); word(0x11d8); block(0x1188,0x50);
        } else if (s=="daylight") { word(-1,V::word,"time"); word(-1,V::word,"stage"); }
        record->size=pos-record->offset; record=nullptr;
      }
      section.size=pos-section.offset;
    }
    if(pos!=data.size()) fail("Trailing bytes after save game");
  }
};
} // namespace
bool parse_savegame(const std::vector<uint8_t>& data, SaveGame& out, SaveError* error) {
  if(error) *error={};
  if(data.size()>detail::kFileBytes) { out={}; if(error) *error={0,"Save game exceeds file budget"}; return false; }
  SaveGame parsed;
  if(!parse_save_header(data,parsed.header)) { if(error) *error={data.size(),"Truncated save header"}; out={}; return false; }
  try { Reader(data,parsed).parse(); }
  catch(const Invalid& e) { if(error) *error={e.offset,e.message}; out={}; return false; }
  parsed.bytes=data; out=std::move(parsed); return true;
}
bool load_savegame(const std::string& path, SaveGame& out, SaveError* error) {
  std::vector<uint8_t> data;
  if(!read_file(path,data)) { out={}; if(error) *error={0,"Cannot read save game"}; return false; }
  return parse_savegame(data,out,error);
}
} // namespace aztec
