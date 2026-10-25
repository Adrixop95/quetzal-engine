// Original singleN.sav / multiN.sav reader. See docs/savegame.md.
#pragma once
#include "core/userfiles.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace aztec {
enum class SaveValue { word, byte, short_integer, boolean, unit_ref, object_ref,
  side_ref, decoration_ref, effect_ref, script_ref, transport_ref, order_ref,
  descriptor, bytes };
// Narrow members still occupy four bytes in the file. Values retain their wire bits.
struct SaveField {
  std::string scope, name;
  int memory_offset = -1; // Relative to scope; -1 means stream metadata.
  size_t offset = 0, size = 0;
  SaveValue kind = SaveValue::word;
  uint32_t value = 0; // Only for four-byte scalar fields.
};
struct SaveRecord {
  uint32_t id = 0, type = 0, kind = 0;
  size_t offset = 0, size = 0;
  std::vector<SaveField> fields;
  const SaveField* field(int memory_offset, const std::string& scope = "object") const;
};
struct SaveSection {
  std::string name;
  size_t offset = 0, size = 0;
  std::vector<SaveRecord> records;
};
struct SaveGame {
  SaveHeader header;
  int32_t view_x = 0, view_y = 0;
  std::array<uint32_t, 5> globals{};
  std::vector<SaveSection> sections;
  std::vector<uint8_t> bytes; // Owns every field range, including opaque blocks.
  const SaveSection* section(const std::string& name) const;
};
struct SaveError { size_t offset = 0; std::string message; };
// Failure clears out; error identifies the first unsupported or malformed location.
// No executable, assets, or host pointers are needed. Trailing bytes are rejected.
bool parse_savegame(const std::vector<uint8_t>& data, SaveGame& out, SaveError* error = nullptr);
bool load_savegame(const std::string& path, SaveGame& out, SaveError* error = nullptr);
int save_unit_kind(uint32_t type);   // -1 outside the 90 UGI types
int save_weapon_kind(uint32_t type); // -1 outside the 40 weapon types
} // namespace aztec
