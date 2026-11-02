// MeshNomad: room server glue for the network layer (src/helpers/net). Compiled for
// every room server env, empty unless the env defines WITH_NET_SERVICES.
#ifdef WITH_NET_SERVICES

#include "MyMesh.h"
#include <helpers/net/NetServices.h>
#include <helpers/net/NetGlue.h>

extern MyMesh the_mesh;

static NetServices net;

static float batt_pct_from_mv(uint16_t mv) {
  float p = (mv - 3300) / (4200.0f - 3300.0f) * 100.0f;
  if (p < 0) p = 0;
  if (p > 100) p = 100;
  return p;
}

// A TCP console line runs through the same handler as serial (admin rights,
// already password-checked by the console).
static void net_cmd(const char* cmd, char* reply, size_t reply_len) {
  (void)reply_len;
  char buf[160];
  strlcpy(buf, cmd, sizeof(buf));
  reply[0] = 0;
  the_mesh.handleCommand(0, buf, reply);
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
  net_fill_sensor_telemetry(sensors, t);   // the room server keeps no neighbour list
}

static void net_set_time(uint32_t epoch) {
  net_apply_epoch(rtc_clock, epoch);
}

static const char* net_console_password() {
  return the_mesh.getNodePrefs()->password;
}

static void net_inhibit_sleep(bool inhibit) {
  board.setInhibitSleep(inhibit);
}

void meshnomad_net_setup() {
  static char node_id[7];
  static char obs_id[65];
  net_make_ids(the_mesh.self_id.pub_key, node_id, obs_id);
  // Observer origin = the node name, re-read at every publish (a rename applies at once).
  net.setObserverInfo(obs_id, the_mesh.getNodePrefs()->node_name,
                      board.getManufacturerName(), FIRMWARE_VERSION);
  net.setSigner([](uint8_t sig[64], const uint8_t* msg, int len) {   // uplink login tokens
    the_mesh.self_id.sign(sig, msg, len);
  });
  // ADMIN_PASSWORD comes from MyMesh.h; the node name goes into the TLS certificate (O=),
  // the node id is its CN.
  net.begin(node_id, net_cmd, net_telem, net_set_time, net_console_password, net_inhibit_sleep,
            ADMIN_PASSWORD, the_mesh.getNodePrefs()->node_name);
}

void meshnomad_net_loop() {
  net.loop();
}

#endif // WITH_NET_SERVICES
