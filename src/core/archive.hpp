// Reader for Res/*.xbe resource archives (see docs/formats.md).
#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace aztec {

struct ArchiveGroup {
  uint32_t id = 0;
  uint32_t lo = 0, hi = 0;           // item numbers [lo, hi)
  std::vector<uint32_t> offsets;     // hi - lo start offsets
};

struct Item {
  const uint8_t* data = nullptr;
  size_t size = 0;
  explicit operator bool() const { return data != nullptr && size > 0; }
};

class Archive {
 public:
  bool open(const std::string& path);
  const std::vector<ArchiveGroup>& groups() const { return groups_; }
  // Item `index` (absolute item number) of group `group_id`; empty Item if absent.
  Item get(uint32_t group_id, uint32_t index) const;

 private:
  std::vector<uint8_t> data_;
  std::vector<ArchiveGroup> groups_;
};

}  // namespace aztec
