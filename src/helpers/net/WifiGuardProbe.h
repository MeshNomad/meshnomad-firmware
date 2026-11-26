#pragma once
// MeshNomad #23: the board side of WifiGuard (companion only): an ARP probe of the gateway and
// a restart counter that survives the guard's own restart. No Arduino include, so NodePrefs can
// take MN_WIFI_GUARD from here.
//
// Probe: the ARP cache of the station interface is cleared and the gateway asked once (lwIP
// queues outgoing packets meanwhile, ARP_QUEUEING 1); 2 s later the gateway must have an
// answered entry. A gateway always answers ARP, so a missing answer means nothing gets
// through, which is the B84 state (no ARP reply from the node, no disconnect event).

#include <stdint.h>

#if defined(ESP32) && defined(WITH_NET_SERVICES) && defined(NET_WIFI_EXTERNAL) && defined(ENABLE_WIFI_INTERFACE)
  #define MN_WIFI_GUARD 1
#endif

#ifdef MN_WIFI_GUARD
namespace WifiGuardProbe {
  // gw: IPv4 as WiFi.gatewayIP() casts to uint32_t. Starts one probe (a running one is replaced).
  void start(uint32_t gw, uint32_t now);
  // -1 still running (or none), 0 gateway not reached, 1 reached. Reports a result once.
  int poll(uint32_t now);
  // Guard restarts since power-on (kept across esp_restart in RTC memory).
  uint32_t restarts();
  void countRestart();
  // WiFi driver off and on again, then connect with the stored station settings.
  void reconnect();
}
#endif
