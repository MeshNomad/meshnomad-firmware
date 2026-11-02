#pragma once
// MeshNomad: compiled-in feature list for the `get features` CLI command.
// The MeshNomad app shows only the command groups confirmed here.

#include <stdio.h>
#include <string.h>

// MeshNomad: the build env (`<board>_mn_<role>`) for `get build`, set by build.sh, so the app
// can tell which firmware file fits this device.
#ifndef MESHNOMAD_BUILD_ENV
  #define MESHNOMAD_BUILD_ENV "unknown"
#endif

inline void meshnomadFeatures(char* out, size_t n) {
  out[0] = 0;
  size_t len = 0;
  auto add = [&](const char* word) {
    if (len > 0 && len + 1 < n) { out[len++] = ' '; out[len] = 0; }
    len += snprintf(out + len, len < n ? n - len : 0, "%s", word);
    if (len >= n) len = n - 1;
  };
#if defined(ENABLE_WIFI_INTERFACE) || defined(WITH_NET_SERVICES)
  add("wifi");
#endif
#ifdef WITH_NET_SERVICES
  add("mqtt"); add("ntp"); add("observer");
#endif
#if defined(ESP32) && defined(WITH_NET_SERVICES) && !defined(NET_WIFI_EXTERNAL)
  add("tls");    // network console over TLS (NetTls); not in the companion
  add("ota.net");  // 'net ota': firmware update over HTTPS (NetOta); not in the companion
  add("obsup");    // observer uplinks obs1..obs3 to external brokers (NetUplink); not in the companion
#endif
#if defined(WITH_BRIDGE) || defined(WITH_RS232_BRIDGE) || defined(WITH_ESPNOW_BRIDGE)
  add("bridge");
#endif
#if defined(ENV_INCLUDE_GPS) && ENV_INCLUDE_GPS == 1
  add("gps");
#endif
#ifdef USE_LR2021
  add("extra.sf");
#endif
#ifdef NRF52_POWER_MANAGEMENT
  add("pwrmgt");
#endif
  if (len == 0) snprintf(out, n, "none");
}
