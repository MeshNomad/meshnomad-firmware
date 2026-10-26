#pragma once

// NetServices: network layer for MeshCore nodes on ESP32.
//  - WiFi station with non-blocking reconnect and configurable TX power
//  - password-protected network console over TLS (NetTls) that feeds the node's CLI
//  - firmware update over HTTPS on request ('net ota', NetOta)
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
#ifndef NET_WIFI_EXTERNAL
  #include "NetTls.h"        // TLS console; empty in the companion build
  #include "NetOta.h"        // 'net ota' over HTTPS; empty in the companion build
#endif

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
  // default_password: the compiled admin password (ADMIN_PASSWORD); while the node still
  // uses it (or an empty one) the console and 'net ota' refuse to log in. node_name goes
  // into the certificate (O=). Both may be nullptr where there is no console (companion).
  void begin(const char* node_id, CmdFn cmd, TelemFn telem, SetTimeFn set_time,
             PasswordFn console_password, InhibitSleepFn inhibit_sleep,
             const char* default_password = nullptr, const char* node_name = nullptr);

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

#ifndef NET_WIFI_EXTERNAL
  // True while the admin password is the compiled default or empty (login gate for the
  // console and 'net ota start').
  bool adminPasswordIsDefault() const;

  // Login lockout shared by the console and the HTTPS login (3 failures -> 5 min per
  // IP). Thread-safe: callable from the httpd task as well as from loop().
  bool lockoutCheck(uint32_t ip);     // true = refuse without checking the password
  void lockoutFail(uint32_t ip);
  void lockoutOk(uint32_t ip);        // successful login: forget the failures
#endif

private:
  char _node_id[24] = {0};
  const char* _node_name = nullptr;         // not copied: points into the node prefs
  const char* _default_password = nullptr;  // compiled ADMIN_PASSWORD literal
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

#ifndef NET_WIFI_EXTERNAL
  // ---- TLS console (port TCP_CLI_PORT, one client) ----
  NetTls::Server _cli_srv;      // listener + the one TLS session (~40 KB only while connected)
  bool  _cli_authed = false;
  char  _cli_line[160];
  int   _cli_len = 0;
  unsigned long _cli_last_activity = 0;
  unsigned long _cli_connected_at = 0;
  uint32_t      _cli_ip = 0;
  // Failed logins per client IP: 3 failures lock that IP out for 5 minutes,
  // other addresses are unaffected. Oldest entry is reused when full.
  struct LoginFail {
    uint32_t      ip;
    uint8_t       count;
    unsigned long last_ms;
  };
  static const int LOGIN_FAIL_SLOTS = 16;   // 16 x 12 B
  LoginFail _login_fail[LOGIN_FAIL_SLOTS] = {};
  portMUX_TYPE _lockout_mux = portMUX_INITIALIZER_UNLOCKED;   // guards _login_fail
  // Unlocked table access; only the lockout*() wrappers may call these.
  LoginFail* loginFailEntry(uint32_t ip, bool create);
  bool isLockedOut(uint32_t ip);
  void recordLoginFailure(uint32_t ip);
  void ensureTls();             // key/cert present once WiFi is up
  void applyTcpState();         // start/stop the listener to match the config
  void cliLoop();
  void cliAccept();             // lockout check -> handshake -> gate -> prompt
  void cliLine();               // one complete console line (login or command)
  void cliReset();

  // ---- net ota (HTTPS firmware update, NetOta) ----
  NetOta _ota;
  bool   _sleep_inhibited = false;    // last value passed to _inhibit_sleep
  void setSleepInhibit(bool on);
  void updateSleepInhibit();          // inhibit = _wifi_started || ota session active
  bool otaStopForNet();               // stop a running session; true if one was running
  void otaCli(const char* arg, char* reply, size_t n);
  // NetOtaHooks trampolines to the single instance; run in the httpd task.
  static bool otaIsLocked(uint32_t ip);
  static void otaLoginFailed(uint32_t ip);
  static void otaLoginOk(uint32_t ip);
  static const char* otaPassword();
#endif

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
  const char* _obs_origin_src = nullptr;   // not copied: points into the node prefs
  void syncObsOrigin();
  char _obs_model[33]  = {0};
  char _obs_fw[33]     = {0};
  void observerDrain();
  void publishObsStatus(const Telemetry& t);
};
