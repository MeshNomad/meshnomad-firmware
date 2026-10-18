#pragma once

// NetServices: network layer for MeshCore nodes on ESP32.
//  - WiFi station with non-blocking reconnect and configurable TX power
//  - password-protected TCP console that feeds the node's CLI
//  - MQTT telemetry with Home Assistant discovery and online/offline status
//  - SNTP clock sync into the node's RTC
//  - MQTT packet observer (every received packet, raw hex + RF metrics)
// Every part is off until switched on via CLI; settings persist in /netcfg.
//
// It does not include Mesh.h: the role passes plain callbacks, so the same layer
// serves the repeater, the room server and the companion.

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <PubSubClient.h>
#include <math.h>            // NAN / isnan for optional sensor fields
#include "NetConfig.h"

// Snapshot of node telemetry, filled by the role via a callback. Optional fields
// default to NAN ("not present"); state and discovery skip them while NAN.
struct Telemetry {
  float    batt_v      = 0.0f;
  float    batt_pct    = 0.0f;
  uint32_t uptime_s    = 0;
  uint32_t free_heap   = 0;
  uint16_t neighbours  = 0;
  uint32_t rx_count    = 0;
  uint32_t tx_count    = 0;
  int16_t  last_rssi   = 0;
  float    last_snr    = 0.0f;
  int      noise_floor = 0;    // dBm
  // Environment sensors
  float    sens_temp   = NAN;  // °C
  float    sens_hum    = NAN;  // %
  float    sens_pres   = NAN;  // hPa
  // Current-monitor channels (voltage / current / power), in ascending
  // sensor channel order, at most three.
  float    ch1_v = NAN, ch1_a = NAN, ch1_w = NAN;
  float    ch2_v = NAN, ch2_a = NAN, ch2_w = NAN;
  float    ch3_v = NAN, ch3_a = NAN, ch3_w = NAN;
};

// Runs the node CLI for one text line; fills reply (may stay empty).
typedef void (*CmdFn)(const char* cmd, char* reply, size_t reply_len);
// Fills a Telemetry snapshot from board/mesh state.
typedef void (*TelemFn)(Telemetry& out);
// Sets the node's RTC to an absolute UTC epoch (seconds).
typedef void (*SetTimeFn)(uint32_t epoch);
// Returns the node's current admin password (checked by the TCP console).
typedef const char* (*PasswordFn)();
// Allows (false) or forbids (true) light sleep; WiFi needs the board awake.
typedef void (*InhibitSleepFn)(bool inhibit);

class NetServices {
public:
  // node_id: short hex id used in MQTT topics and Home Assistant ids. Copied.
  void begin(const char* node_id, CmdFn cmd, TelemFn telem, SetTimeFn set_time,
             PasswordFn console_password, InhibitSleepFn inhibit_sleep);

  // Call from the main loop() only. Non-blocking; never stalls mesh routing.
  void loop();

  // Handles a network CLI line. True if consumed (reply filled).
  bool handleCli(const char* command, char* reply, size_t reply_len);

  // Observer identity: observer_id = full public key hex, origin = node name.
  void setObserverInfo(const char* observer_id, const char* origin,
                       const char* model, const char* fw_version);

  // Called from the radio path — never blocks. Copies the packet into a
  // drop-oldest ring; publishing happens later in loop().
  void queueRxPacket(const uint8_t* raw, int len, float snr, float rssi);

  bool isWifiConnected() const { return WiFi.status() == WL_CONNECTED; }

private:
  char _node_id[24] = {0};
  CmdFn          _cmd   = nullptr;
  TelemFn        _telem = nullptr;
  SetTimeFn      _set_time = nullptr;
  PasswordFn     _console_password = nullptr;
  InhibitSleepFn _inhibit_sleep = nullptr;

  NetConfig _cfg;
  bool _apply_pending = false;  // 'net apply' requested; runs at the top of loop()

  // ---- WiFi ----
  bool _wifi_started = false;
  unsigned long _last_wifi_retry = 0;
  void applyWifiState();        // bring WiFi up or fully off to match the config
  void applyWifiPower();
  void stopAll();               // MQTT offline + console closed + WiFi off
  bool observerActive() const;

  // ---- SNTP ----
  enum NtpState { NTP_IDLE, NTP_WAITING, NTP_SYNCED };
  NtpState _ntp_state = NTP_IDLE;
  unsigned long _ntp_wait_start_ms = 0;
  unsigned long _last_ntp_apply_ms = 0;
  unsigned int  _ntp_retries = 0;
  void ntpLoop();
  void ntpStop();

  // ---- TCP console ----
  WiFiServer _cli_server;
  WiFiClient _cli_client;
  bool  _cli_listening = false;
  bool  _cli_authed = false;
  char  _cli_line[160];
  int   _cli_len = 0;
  unsigned long _cli_last_activity = 0;
  void applyTcpState();         // start/stop the listener to match the config
  void cliLoop();
  void cliReset();

  // ---- MQTT ----
  WiFiClient    _mqtt_wifi;
  PubSubClient  _mqtt{_mqtt_wifi};
  unsigned long _last_mqtt_retry = 0;
  unsigned long _last_publish = 0;
  bool          _discovery_sent = false;
  void mqttLoop();
  bool mqttConnect();
  void mqttOffline();
  void publishDiscovery();
  void publishState(const Telemetry& t);
  void topicState(char* out, size_t n);
  void topicStatus(char* out, size_t n);

  // ---- Observer ----
  // Max raw packet = 255 bytes; 8 slots ≈ 2.1 KB RAM.
  struct ObsPacket {
    uint8_t len;
    float   snr;
    float   rssi;
    uint8_t raw[255];
  };
  static const int OBS_QUEUE_SLOTS = 8;
  ObsPacket _obs_q[OBS_QUEUE_SLOTS];
  int      _obs_head = 0;       // oldest entry
  int      _obs_count = 0;
  uint32_t _obs_dropped = 0;
  uint32_t _obs_published = 0;
  char _obs_id[65]     = {0};
  char _obs_origin[33] = {0};
  char _obs_model[33]  = {0};
  char _obs_fw[33]     = {0};
  void observerDrain();
  void publishObsStatus(const Telemetry& t);
};
