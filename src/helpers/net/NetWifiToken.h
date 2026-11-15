#pragma once

// 'wifi:' token of 'net status' where MeshCore's own code runs WiFi (NET_WIFI_EXTERNAL,
// the companion); no Arduino dependency (unit-tested natively). WiFi starts there only
// at boot, so a station that was never started stays off until the next restart.
// sta_started: the station interface is enabled (WiFi.getMode() & WIFI_MODE_STA);
// wl: WiFi.status(), printed only while a started station is not connected.

#include <stddef.h>
#include <stdio.h>

inline void netExternalWifiToken(char* out, size_t n, bool sta_started, bool connected, int wl) {
  if (connected) snprintf(out, n, "connected");
  else if (!sta_started) snprintf(out, n, "off");
  else snprintf(out, n, "connecting(wl=%d)", wl);
}
