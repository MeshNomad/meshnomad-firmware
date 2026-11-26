// MeshNomad: companion glue for the network layer (src/helpers/net). Compiled for
// every companion env, empty unless the env defines WITH_NET_SERVICES. WiFi itself
// is MeshCore's own (ENABLE_WIFI_INTERFACE); this adds MQTT, NTP and the observer.
#ifdef WITH_NET_SERVICES

#include "MyMesh.h"
#include <helpers/net/NetServices.h>
#include <helpers/net/NetGlue.h>
#include <helpers/net/NetEpoch.h>
#include <helpers/net/WifiGuard.h>
#include <helpers/net/WifiGuardProbe.h>
#include <WiFi.h>

static NetServices net;

static float batt_pct_from_mv(uint16_t mv) {
  float p = (mv - 3300) / (4200.0f - 3300.0f) * 100.0f;
  if (p < 0) p = 0;
  if (p > 100) p = 100;
  return p;
}

static void net_telem(Telemetry& t) {
  uint16_t mv = board.getBattMilliVolts();
  t.batt_v      = mv / 1000.0f;
  t.batt_pct    = batt_pct_from_mv(mv);
  t.uptime_s    = millis() / 1000;
  t.free_heap   = ESP.getFreeHeap();
  t.rx_count    = radio_driver.getPacketsRecv();
  t.tx_count    = radio_driver.getPacketsSent();
  t.last_rssi   = (int16_t)radio_driver.getLastRSSI();
  t.last_snr    = radio_driver.getLastSNR();
  t.noise_floor = (int)radio_driver.getNoiseFloor();
  net_fill_sensor_telemetry(sensors, t);
}

static void net_set_time(uint32_t epoch, bool from_server) {
  net_apply_epoch(rtc_clock, epoch, from_server);
  uint32_t now = rtc_clock.getCurrentTime();
  // B85: with a server time, contact times ahead of the clock are pulled back to it (a
  // backward step leaves them in the future, and so does a clock corrected earlier). Only a
  // contact that changes marks the contacts file for a later write.
  if (netShouldClampContacts(from_server, now)) the_mesh.clampFutureContacts(now);
}

void meshnomad_net_setup() {
  static char node_id[7];
  static char obs_id[65];
  net_make_ids(the_mesh.self_id.pub_key, node_id, obs_id);
  // Observer origin = the node name, re-read at every publish (a rename applies at once).
  net.setObserverInfo(obs_id, the_mesh.getNodePrefs()->node_name,
                      board.getManufacturerName(), FIRMWARE_VERSION);
  // The companion has no TCP console (its WiFi is MeshCore's own), so no command bridge,
  // password or sleep callback; CLI commands reach the network layer via MyMesh::handleCommand.
  net.begin(node_id, nullptr, net_telem, net_set_time, nullptr, nullptr);
}

#ifdef MN_WIFI_GUARD
// #23: a link that stays associated but carries nothing gets WiFi off and on, and after two
// such tries one restart of the radio (wifi.guard 0 switches it off).
static WifiGuard wifi_guard;

static void wifi_guard_loop() {
  uint32_t now = millis();
  int r = WifiGuardProbe::poll(now);
  if (r >= 0) wifi_guard.probeResult(r == 1, now);
  bool up = WiFi.status() == WL_CONNECTED && (uint32_t)WiFi.localIP() != 0;
  switch (wifi_guard.tick(the_mesh.getNodePrefs()->wifi_guard != 0, up, now)) {
    case WifiGuard::Action::PROBE:     WifiGuardProbe::start((uint32_t)WiFi.gatewayIP(), now); break;
    case WifiGuard::Action::RECONNECT: WifiGuardProbe::reconnect(); break;
    case WifiGuard::Action::RESTART:   WifiGuardProbe::countRestart(); the_mesh.saveAndReboot(); break;
    default: break;
  }
}

void meshnomad_wifi_guard_status(char* out, size_t n) {
  snprintf(out, n, "%d probes:%lu fails:%lu reconnects:%lu restarts:%lu",
           the_mesh.getNodePrefs()->wifi_guard ? 1 : 0, (unsigned long)wifi_guard.probes(),
           (unsigned long)wifi_guard.failures(), (unsigned long)wifi_guard.reconnects(),
           (unsigned long)WifiGuardProbe::restarts());
}
#endif

void meshnomad_net_loop() {
  net.loop();
#ifdef MN_WIFI_GUARD
  wifi_guard_loop();
#endif
}

#endif // WITH_NET_SERVICES
