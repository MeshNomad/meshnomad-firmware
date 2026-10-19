// MeshNomad: companion glue for the network layer (src/helpers/net). Compiled for
// every companion env, empty unless the env defines WITH_NET_SERVICES. WiFi itself
// is MeshCore's own (ENABLE_WIFI_INTERFACE); this adds MQTT, NTP and the observer.
#ifdef WITH_NET_SERVICES

#include "MyMesh.h"
#include <helpers/net/NetServices.h>
#include <helpers/net/NetGlue.h>

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

static void net_set_time(uint32_t epoch) {
  net_apply_epoch(rtc_clock, epoch);
}

void meshnomad_net_setup() {
  static char node_id[7];
  static char obs_id[65];
  net_make_ids(the_mesh.self_id.pub_key, node_id, obs_id);
  // Observer origin = node name at boot; a rename applies after a reboot.
  net.setObserverInfo(obs_id, the_mesh.getNodePrefs()->node_name,
                      board.getManufacturerName(), FIRMWARE_VERSION);
  // The companion has no TCP console (its WiFi is MeshCore's own), so no command bridge,
  // password or sleep callback; CLI commands reach the network layer via MyMesh::handleCommand.
  net.begin(node_id, nullptr, net_telem, net_set_time, nullptr, nullptr);
}

void meshnomad_net_loop() {
  net.loop();
}

#endif // WITH_NET_SERVICES
