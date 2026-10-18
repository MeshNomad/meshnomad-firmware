#pragma once

// Runtime network configuration (ESP32 only), persisted to SPIFFS.
// Build flags provide defaults only; real values are set at runtime over any CLI
// transport (serial / LoRa admin / TCP console) and stored on the device, so no
// addresses or secrets end up in a build.

#include <Arduino.h>

// ---- defaults (overridable via build flags) ----
#ifndef WIFI_SSID
  #define WIFI_SSID ""
#endif
#ifndef WIFI_PWD
  #define WIFI_PWD ""
#endif
#ifndef WIFI_TX_POWER_DBM
  #define WIFI_TX_POWER_DBM 0        // 0 = core default (~19.5 dBm)
#endif
#ifndef MQTT_HOST
  #define MQTT_HOST ""
#endif
#ifndef MQTT_PORT
  #define MQTT_PORT 1883
#endif
#ifndef MQTT_USER
  #define MQTT_USER ""
#endif
#ifndef MQTT_PWD
  #define MQTT_PWD ""
#endif
#ifndef MQTT_BASE_TOPIC
  #define MQTT_BASE_TOPIC "meshcore"
#endif
#ifndef TELEMETRY_INTERVAL_SECS
  #define TELEMETRY_INTERVAL_SECS 60
#endif
#ifndef NTP_SERVER
  #define NTP_SERVER "pool.ntp.org"
#endif
#ifndef NTP_INTERVAL_SECS
  #define NTP_INTERVAL_SECS 90000    // re-apply system time to the RTC every 25 h
#endif
#ifndef OBSERVER_IATA
  #define OBSERVER_IATA ""           // area code for observer topics, e.g. WAW
#endif
#ifndef TCP_CLI_PORT
  #define TCP_CLI_PORT 5000
#endif

struct NetConfig {
  // Every switch is off on a fresh node; the user turns each one on via CLI.
  bool wifi_enabled = false;
  char wifi_ssid[33] = {0};
  char wifi_pwd[65]  = {0};
  int  wifi_power    = 0;        // dBm; 0 = core default

  bool tcp_enabled   = false;    // password-protected TCP console

  bool mqtt_enabled  = false;
  char mqtt_host[64] = {0};
  int  mqtt_port     = MQTT_PORT;
  char mqtt_user[33] = {0};
  char mqtt_pwd[65]  = {0};
  char mqtt_topic[24] = {0};

  int  telemetry_s   = TELEMETRY_INTERVAL_SECS;

  bool ntp_enabled    = false;
  char ntp_server[64] = {0};
  int  ntp_interval_s = NTP_INTERVAL_SECS;

  bool obs_enabled   = false;    // MQTT packet observer
  char obs_iata[8]   = {0};      // uppercased on set

  void loadDefaults();           // populate from the build-flag macros
  void load();                   // read /netcfg, per-key fallback to defaults
  void save();                   // write /netcfg

  // Assigns one stored key (as written by save()). False if the key is unknown.
  bool assign(const char* key, const char* val);
};
