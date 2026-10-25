// MeshNomad: repeater glue for the network layer (src/helpers/net). Compiled for
// every repeater env, empty unless the env defines WITH_NET_SERVICES.
#ifdef WITH_NET_SERVICES

#include "MyMesh.h"
#include <helpers/net/NetServices.h>
#include <helpers/net/NetGlue.h>

extern MyMesh the_mesh;

// Mirror of the default in MyMesh.cpp (upstream keeps it in the .cpp, so it is not
// visible here). A -D ADMIN_PASSWORD build flag reaches both places. The network
// console and 'net ota' refuse to log in while the node still uses this password.
#ifndef ADMIN_PASSWORD
  #define ADMIN_PASSWORD "password"
#endif

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
  RepeaterStats s;
  the_mesh.getRepeaterStats(s);
  t.batt_v      = s.batt_milli_volts / 1000.0f;
  t.batt_pct    = batt_pct_from_mv(s.batt_milli_volts);
  t.uptime_s    = s.total_up_time_secs;
  t.free_heap   = ESP.getFreeHeap();
  t.neighbours  = the_mesh.getNeighbourCount();
  t.rx_count    = s.n_packets_recv;
  t.tx_count    = s.n_packets_sent;
  t.last_rssi   = s.last_rssi;
  t.last_snr    = s.last_snr / 4.0f;    // RepeaterStats stores SNR x4
  t.noise_floor = s.noise_floor;
  net_fill_sensor_telemetry(sensors, t);
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
  // Observer origin = node name at boot; a rename applies after a reboot.
  net.setObserverInfo(obs_id, the_mesh.getNodePrefs()->node_name,
                      board.getManufacturerName(), FIRMWARE_VERSION);
  // The node name goes into the TLS certificate (O=); the node id is its CN.
  net.begin(node_id, net_cmd, net_telem, net_set_time, net_console_password, net_inhibit_sleep,
            ADMIN_PASSWORD, the_mesh.getNodePrefs()->node_name);
}

void meshnomad_net_loop() {
  net.loop();
}

#endif // WITH_NET_SERVICES
