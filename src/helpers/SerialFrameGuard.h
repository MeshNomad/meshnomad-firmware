#pragma once
// MeshNomad B90: a cut or garbled frame must not swallow the frames after it.
#include <stdint.h>

namespace mesh {

#define SERIAL_FRAME_GAP_MILLIS  1000

inline bool serialFrameLengthOk(uint16_t len, uint16_t max_len) { return len > 0 && len <= max_len; }

// true when a started frame got no byte for the gap; safe over the millis() wrap
inline bool serialFrameStale(uint32_t now, uint32_t last_byte_at) {
  return (uint32_t)(now - last_byte_at) >= SERIAL_FRAME_GAP_MILLIS;
}

}  // namespace mesh
