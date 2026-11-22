// Logical pool slots, independent of stable command IDs and C++ vector addresses.
#pragma once
#include <cstdint>
#include <cstddef>
#include <vector>

namespace aztec::engine {
class PoolOrder {
 public:
  static constexpr uint32_t none = UINT32_MAX;
  uint32_t insert(uint32_t id) {
    if (id < by_id_.size() && by_id_[id] != none) return by_id_[id];
    uint32_t slot;
    if (free_.empty()) { slot = uint32_t(slots_.size()); slots_.push_back({}); }
    else { slot = free_.back(); free_.pop_back(); }
    if (by_id_.size() <= id) by_id_.resize(std::size_t(id) + 1, none);
    by_id_[id] = slot;
    slots_[slot] = {id, head_, none};
    if (head_ != none) slots_[head_].previous = slot;
    head_ = slot;
    return slot;
  }
  void erase(uint32_t id) {
    if (id >= by_id_.size() || by_id_[id] == none) return;
    const auto slot = by_id_[id];
    const auto entry = slots_[slot];
    if (entry.previous == none) head_ = entry.next;
    else slots_[entry.previous].next = entry.next;
    if (entry.next != none) slots_[entry.next].previous = entry.previous;
    slots_[slot] = {};
    by_id_[id] = none;
    free_.push_back(slot);
  }
  // A phase snapshots IDs before callbacks: insertion at the head waits for the next walk.
  std::vector<uint32_t> newest_first() const {
    std::vector<uint32_t> ids;
    for (auto slot = head_; slot != none; slot = slots_[slot].next) ids.push_back(slots_[slot].id);
    return ids;
  }
  uint64_t state_hash() const {
    uint64_t hash = 1469598103934665603ull;
    auto mix = [&](uint64_t value) { hash = (hash ^ value) * 1099511628211ull; };
    mix(head_); mix(slots_.size());
    for (const auto& entry : slots_) { mix(entry.id); mix(entry.next); mix(entry.previous); }
    mix(free_.size());
    for (auto slot : free_) mix(slot);
    return hash;
  }

 private:
  struct Entry { uint32_t id = 0, next = none, previous = none; };
  std::vector<Entry> slots_;
  std::vector<uint32_t> free_, by_id_;
  uint32_t head_ = none;
};
}  // namespace aztec::engine
