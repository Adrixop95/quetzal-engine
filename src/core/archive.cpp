#include "core/endian.hpp"
#include "core/loader_limits.hpp"
#include "core/archive.hpp"

#include <cstdio>
#include <cstring>

namespace aztec {

static uint32_t rd32(const uint8_t* p) { return read_le<uint32_t>(p); }

bool Archive::open(const std::string& path) {
  groups_.clear();
  data_.clear();
  FILE* f = std::fopen(path.c_str(), "rb");
  if (!f) return false;
  std::fseek(f, 0, SEEK_END);
  long n = std::ftell(f);
  std::fseek(f, 0, SEEK_SET);
  if (n < 0 || size_t(n) > detail::kFileBytes) { std::fclose(f); return false; }
  data_.resize(size_t(n));
  size_t got = data_.empty() ? 0 : std::fread(data_.data(), 1, data_.size(), f);
  std::fclose(f);
  if (got != data_.size() || data_.size() < 4) return false;

  uint32_t count = rd32(data_.data());
  if (count > 64 || 4 + size_t(count) * 16 > data_.size()) return false;
  std::vector<ArchiveGroup> groups;
  size_t total_items = 0;
  for (uint32_t i = 0; i < count; i++) {
    const uint8_t* r = data_.data() + 4 + 16 * i;
    ArchiveGroup g;
    g.id = rd32(r);
    uint32_t table = rd32(r + 4);
    g.lo = rd32(r + 8);
    g.hi = rd32(r + 12);
    if (g.hi < g.lo) return false;
    size_t n_items = g.hi - g.lo;
    total_items += n_items;
    if (total_items > detail::kFileBytes / 4) return false;
    if (table > data_.size() || n_items > (data_.size() - table) / 4) return false;
    g.offsets.resize(n_items);
    for (size_t k = 0; k < n_items; k++) g.offsets[k] = rd32(data_.data() + table + 4 * k);
    groups.push_back(std::move(g));
  }
  groups_ = std::move(groups);
  return true;
}

Item Archive::get(uint32_t group_id, uint32_t index) const {
  for (size_t gi = 0; gi < groups_.size(); gi++) {
    const ArchiveGroup& g = groups_[gi];
    if (g.id != group_id || index < g.lo || index >= g.hi) continue;
    size_t k = index - g.lo;
    size_t start = g.offsets[k];
    size_t end;
    if (k + 1 < g.offsets.size()) {
      end = g.offsets[k + 1];
    } else {
      end = data_.size();  // last item: up to the first item of the next non-empty group
      for (size_t j = gi + 1; j < groups_.size(); j++)
        if (!groups_[j].offsets.empty()) { end = groups_[j].offsets[0]; break; }
    }
    if (start > end || end > data_.size()) return {};
    return {data_.data() + start, end - start};
  }
  return {};
}

}  // namespace aztec
