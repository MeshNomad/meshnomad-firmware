#pragma once

#include <stddef.h>
#include <stdio.h>
#include <string.h>

namespace mesh {

// MeshNomad: reply text for the bare `gps` CLI command.
// `setting` is SensorManager::getSettingByKey("gps"), which is NULL when the
// sensor manager lists no `gps` setting (no GPS module detected at boot).
inline void formatGpsStatus(char* reply, size_t reply_len, bool enabled, const char* setting,
                            bool fix, int sats) {
  if (reply_len == 0) return;
  if (setting == NULL) {
    snprintf(reply, reply_len, "off, no module");
  } else if (enabled) {
    bool active = strcmp(setting, "1") == 0;
    snprintf(reply, reply_len, "on, %s, %s, %d sats", active ? "active" : "deactivated",
             fix ? "fix" : "no fix", sats);
  } else {
    snprintf(reply, reply_len, "off");
  }
}

}  // namespace mesh
