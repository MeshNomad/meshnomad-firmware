#pragma once
// MeshNomad: `get mem`, the board's RAM as the heap sees it, for every role (B91).
//   > internal:<free> low:<lowest free since boot> block:<largest free block> psram:<free>
// All values in bytes. internal = RAM the WiFi driver, lwIP and BLE need (the B84 dead
// link came at 2-9 KB); low is the heap's own watermark since boot; psram 0 = none or off.
// Read-only: nothing to switch on.

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

struct MemSample {
  uint32_t internal = 0;
  uint32_t low = 0;
  uint32_t block = 0;
  uint32_t psram = 0;
};

inline int memStatusFormat(char* out, size_t n, const MemSample& m) {
  return snprintf(out, n, "internal:%lu low:%lu block:%lu psram:%lu", (unsigned long)m.internal,
                  (unsigned long)m.low, (unsigned long)m.block, (unsigned long)m.psram);
}

#if defined(ESP32)
#include <esp_heap_caps.h>
inline MemSample memStatusRead() {
  const uint32_t caps = MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT;
  MemSample m;
  m.internal = heap_caps_get_free_size(caps);
  m.low = heap_caps_get_minimum_free_size(caps);
  m.block = heap_caps_get_largest_free_block(caps);
  m.psram = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
  return m;
}
#endif
