// Read-only pool metadata checks for the reference observer.
#pragma once
#include "aztec_rt.h"
#include <stdlib.h>

typedef struct {
  uint32_t base, stride, capacity, count, sentinel;
  uint32_t* nodes;
} L0Pool;

static int l0_pool_index(const L0Pool* p, uint32_t node) {
  return node >= p->base && (node - p->base) / p->stride < p->capacity &&
         (node - p->base) % p->stride == 0;
}

static L0Pool l0_pool_read(uint32_t address) {
  L0Pool p = {GU32(address + 4), GU32(address + 0x10), 0,
              GU32(address + 0x18), GU32(address + 0x24), NULL};
  uint32_t bytes = GU32(address + 0x1c);
  if (!p.base || p.stride < 8 || p.stride > 0x1000 || bytes > UINT32_MAX - p.base ||
      !(p.capacity = bytes / p.stride) || p.capacity > 100000 || p.count > p.capacity || !p.sentinel)
    rt_fatal("Invalid L0 pool metadata");
  p.nodes = calloc(p.count ? p.count : 1, sizeof *p.nodes);
  uint8_t* seen = calloc(p.capacity, 1);
  if (!p.nodes || !seen) rt_fatal("Cannot allocate L0 pool audit");
  uint32_t node = GU32(p.sentinel), previous = p.sentinel;
  for (uint32_t i = 0; i < p.count; ++i) {
    if (!l0_pool_index(&p, node) || seen[(node - p.base) / p.stride] || GU32(node + 4) != previous)
      rt_fatal("Invalid L0 active pool chain");
    seen[(node - p.base) / p.stride] = 1;
    p.nodes[i] = node;
    previous = node;
    node = GU32(node);
  }
  if (node != p.sentinel || GU32(p.sentinel + 4) != previous)
    rt_fatal("L0 active pool count differs");
  // The allocator's free chain, not retained payload bytes, identifies free slots.
  node = GU32(address + 8);
  for (uint32_t i = p.count; i < p.capacity; ++i) {
    if (!l0_pool_index(&p, node) || seen[(node - p.base) / p.stride])
      rt_fatal("Invalid L0 free pool chain");
    seen[(node - p.base) / p.stride] = 1;
    node = GU32(node);
  }
  if (node) rt_fatal("L0 free pool count differs");
  free(seen);
  return p;
}
