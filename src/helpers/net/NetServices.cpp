#if defined(ESP32) && defined(WITH_NET_SERVICES)

#include "NetServices.h"
#include "NetHooks.h"
#include <time.h>
#include <esp_sntp.h>
#include <esp_idf_version.h>

// ESP-IDF 5 (e.g. ESP32-C6 builds) renamed sntp_stop() to esp_sntp_stop().
#if ESP_IDF_VERSION_MAJOR >= 5
  #define NET_SNTP_STOP() esp_sntp_stop()
#else
  #define NET_SNTP_STOP() sntp_stop()
#endif

// Home Assistant discovery metric map; `field` matches the publishState() JSON keys.
// opt: optional (NAN-able) Telemetry field, or nullptr for metrics always present.
// Discovery skips entries whose opt field is NAN in a sampled Telemetry, so absent
// sensors never show up as "unavailable" entities.
struct MetricDef {
  const char* name;
  const char* field;
  const char* unit;      // nullptr => omit
  const char* dev_cla;   // nullptr => omit
  const char* stat_cla;
  float Telemetry::* opt;
};
static const MetricDef METRICS[] = {
  {"Battery Voltage", "batt_v",      "V",   "voltage",         "measurement",      nullptr},
  {"Battery",         "batt_pct",    "%",   "battery",         "measurement",      nullptr},
  {"Uptime",          "uptime_s",    "s",   "duration",        "total_increasing", nullptr},
  {"Free Heap",       "free_heap",   "B",   "data_size",       "measurement",      nullptr},
  {"Neighbours",      "neighbours",  nullptr, nullptr,         "measurement",      nullptr},
  {"RX Packets",      "rx_count",    nullptr, nullptr,         "total_increasing", nullptr},
  {"TX Packets",      "tx_count",    nullptr, nullptr,         "total_increasing", nullptr},
  {"Last RSSI",       "last_rssi",   "dBm", "signal_strength", "measurement",      nullptr},
  {"Last SNR",        "last_snr",    "dB",  nullptr,           "measurement",      nullptr},
  {"Noise Floor",     "noise_floor", "dBm", "signal_strength", "measurement",      nullptr},
  {"Temperature",     "sens_temp",   "°C",  "temperature",     "measurement",      &Telemetry::sens_temp},
  {"Humidity",        "sens_hum",    "%",   "humidity",        "measurement",      &Telemetry::sens_hum},
  {"Pressure",        "sens_pres",   "hPa", "pressure",        "measurement",      &Telemetry::sens_pres},
  {"Ch1 Voltage",     "ch1_v",       "V",   "voltage",         "measurement",      &Telemetry::ch1_v},
  {"Ch1 Current",     "ch1_a",       "A",   "current",         "measurement",      &Telemetry::ch1_a},
  {"Ch1 Power",       "ch1_w",       "W",   "power",           "measurement",      &Telemetry::ch1_w},
  {"Ch2 Voltage",     "ch2_v",       "V",   "voltage",         "measurement",      &Telemetry::ch2_v},
  {"Ch2 Current",     "ch2_a",       "A",   "current",         "measurement",      &Telemetry::ch2_a},
  {"Ch2 Power",       "ch2_w",       "W",   "power",           "measurement",      &Telemetry::ch2_w},
  {"Ch3 Voltage",     "ch3_v",       "V",   "voltage",         "measurement",      &Telemetry::ch3_v},
  {"Ch3 Current",     "ch3_a",       "A",   "current",         "measurement",      &Telemetry::ch3_a},
  {"Ch3 Power",       "ch3_w",       "W",   "power",           "measurement",      &Telemetry::ch3_w},
};

static NetServices* g_net = nullptr;

bool net_handle_cli(const char* command, char* reply, size_t n) {
  if (!g_net) return false;
  return g_net->handleCli(command, reply, n);
}

void net_observer_rx_raw(float snr, float rssi, const uint8_t* raw, int len) {
  if (g_net) g_net->queueRxPacket(raw, len, snr, rssi);
}

static wifi_power_t dbmToPower(int d) {
  if (d >= 19) return WIFI_POWER_19_5dBm;
  if (d >= 18) return WIFI_POWER_18_5dBm;
  if (d >= 17) return WIFI_POWER_17dBm;
  if (d >= 15) return WIFI_POWER_15dBm;
  if (d >= 13) return WIFI_POWER_13dBm;
  if (d >= 11) return WIFI_POWER_11dBm;
  if (d >= 8)  return WIFI_POWER_8_5dBm;
  if (d >= 7)  return WIFI_POWER_7dBm;
  return WIFI_POWER_5dBm;
}

// ---- lifecycle ----
void NetServices::begin(const char* node_id, CmdFn cmd, TelemFn telem, SetTimeFn set_time,
                        PasswordFn console_password, InhibitSleepFn inhibit_sleep) {
  strlcpy(_node_id, node_id, sizeof(_node_id));
  _cmd = cmd;
  _telem = telem;
  _set_time = set_time;
  _console_password = console_password;
  _inhibit_sleep = inhibit_sleep;
  g_net = this;
  _cfg.load();
  applyWifiState();
}

void NetServices::loop() {
  if (_apply_pending) {        // 'net apply': restart everything with the stored config
    _apply_pending = false;
    stopAll();
  }

#ifndef NET_WIFI_EXTERNAL
  applyWifiState();
  if (!_wifi_started) return;
  applyTcpState();

  if (WiFi.status() != WL_CONNECTED) {
    ntpStop();                 // re-armed on reconnect
    if (_cli_client) cliReset();
    unsigned long now = millis();
    if (_cfg.wifi_ssid[0] && now - _last_wifi_retry > 10000UL) {
      _last_wifi_retry = now;
      WiFi.begin(_cfg.wifi_ssid, _cfg.wifi_pwd);
    }
    return;
  }
  cliLoop();
#else
  if (WiFi.status() != WL_CONNECTED) {
    ntpStop();
    return;
  }
#endif

  mqttLoop();
  ntpLoop();
}

// ---- WiFi ----
void NetServices::applyWifiPower() {
  if (_cfg.wifi_power > 0) WiFi.setTxPower(dbmToPower(_cfg.wifi_power));
}

void NetServices::stopAll() {
  mqttOffline();
  ntpStop();
  _discovery_sent = false;
#ifndef NET_WIFI_EXTERNAL
  if (_cli_listening) {
    _cli_server.stop();
    _cli_listening = false;
  }
  cliReset();
  if (_wifi_started) {
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    _wifi_started = false;
    if (_inhibit_sleep) _inhibit_sleep(false);
  }
#endif
}

void NetServices::applyWifiState() {
#ifndef NET_WIFI_EXTERNAL
  if (_cfg.wifi_enabled && !_wifi_started) {
    if (_inhibit_sleep) _inhibit_sleep(true);   // light sleep would drop the WiFi link
    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(true);
    if (_cfg.wifi_ssid[0]) WiFi.begin(_cfg.wifi_ssid, _cfg.wifi_pwd);
    applyWifiPower();
    _wifi_started = true;
    _last_wifi_retry = millis();
  } else if (!_cfg.wifi_enabled && _wifi_started) {
    stopAll();
  }
#else
  applyWifiPower();
#endif
}

bool NetServices::observerActive() const {
  return _cfg.obs_enabled && _cfg.obs_iata[0] && _obs_id[0];
}

// ---- SNTP (non-blocking, only while WiFi is connected) ----
void NetServices::ntpStop() {
  if (_ntp_state != NTP_IDLE) {
    NET_SNTP_STOP();
    _ntp_state = NTP_IDLE;
  }
}

void NetServices::ntpLoop() {
  if (!_cfg.ntp_enabled || !_set_time) {
    ntpStop();
    return;
  }
  unsigned long now = millis();

  if (_ntp_state == NTP_IDLE) {
    configTime(0, 0, _cfg.ntp_server);            // UTC; starts SNTP
    _ntp_state = NTP_WAITING;
    _ntp_wait_start_ms = now;
    _ntp_retries = 0;
    return;
  }

  if (_ntp_state == NTP_WAITING) {
    if (sntp_get_sync_status() == SNTP_SYNC_STATUS_COMPLETED || time(nullptr) > 1600000000) {
      _set_time((uint32_t)time(nullptr));
      _ntp_state = NTP_SYNCED;
      _last_ntp_apply_ms = now;
    } else if (now - _ntp_wait_start_ms > 60000UL && _ntp_retries < 10) {
      NET_SNTP_STOP();
      configTime(0, 0, _cfg.ntp_server);          // retry (DNS may not have been ready)
      _ntp_wait_start_ms = now;
      _ntp_retries++;
    }
    return;
  }

  // NTP_SYNCED: SNTP keeps the system time fresh; copy it to the RTC periodically.
  if (now - _last_ntp_apply_ms >= (unsigned long)_cfg.ntp_interval_s * 1000UL) {
    _set_time((uint32_t)time(nullptr));
    _last_ntp_apply_ms = now;
  }
}

// ---- TCP console ----
// Protocol: "Password: " -> password + "\n" -> "OK\r\n> " or "Denied\r\n" (closed);
// each reply "  -> <reply>\r\n" followed by "> "; idle timeout 120 s.
// Login rules: the password must be entered within 15 s of connecting; an empty
// node password never matches; 3 failed logins lock that client IP out for
// 5 minutes (it keeps getting "Denied" without the password being checked).
static const unsigned long LOGIN_DEADLINE_MS = 15000UL;
static const uint8_t       LOGIN_MAX_FAILS   = 3;
static const unsigned long LOGIN_LOCKOUT_MS  = 300000UL;

NetServices::LoginFail* NetServices::loginFailEntry(uint32_t ip, bool create) {
  LoginFail* oldest = &_login_fail[0];
  for (LoginFail& e : _login_fail) {
    if (e.count > 0 && e.ip == ip) return &e;
    if (e.count == 0 || e.last_ms < oldest->last_ms) oldest = &e;
  }
  if (!create) return nullptr;
  *oldest = {ip, 0, millis()};
  return oldest;
}

bool NetServices::isLockedOut(uint32_t ip) {
  LoginFail* e = loginFailEntry(ip, false);
  if (!e) return false;
  if (millis() - e->last_ms >= LOGIN_LOCKOUT_MS) {   // expired: forget the failures
    e->count = 0;
    return false;
  }
  return e->count >= LOGIN_MAX_FAILS;
}

void NetServices::recordLoginFailure(uint32_t ip) {
  LoginFail* e = loginFailEntry(ip, true);
  if (millis() - e->last_ms >= LOGIN_LOCKOUT_MS) e->count = 0;
  if (e->count < 255) e->count++;
  e->last_ms = millis();
}

void NetServices::applyTcpState() {
  bool want = _wifi_started && _cfg.tcp_enabled;
  if (want && !_cli_listening) {
    _cli_server.begin(TCP_CLI_PORT);
    _cli_listening = true;
  } else if (!want && _cli_listening) {
    _cli_server.stop();
    _cli_listening = false;
    cliReset();
  }
}

void NetServices::cliReset() {
  if (_cli_client) _cli_client.stop();
  _cli_authed = false;
  _cli_len = 0;
}

void NetServices::cliLoop() {
  if (!_cli_listening) return;
  if (!_cli_client || !_cli_client.connected()) {
    WiFiClient nc = _cli_server.available();
    if (nc) {
      _cli_client = nc;
      _cli_authed = false;
      _cli_len = 0;
      _cli_last_activity = millis();
      _cli_connected_at = _cli_last_activity;
      _cli_ip = (uint32_t)_cli_client.remoteIP();
      _cli_client.print("Password: ");
    }
    return;
  }

  if (millis() - _cli_last_activity > 120000UL ||
      (!_cli_authed && millis() - _cli_connected_at > LOGIN_DEADLINE_MS)) {
    cliReset();
    return;
  }

  while (_cli_client.available()) {
    char c = _cli_client.read();
    _cli_last_activity = millis();
    if (c == '\r') continue;
    if (c == '\n') {
      _cli_line[_cli_len] = 0;
      if (!_cli_authed) {
        const char* pw = _console_password ? _console_password() : nullptr;
        if (!isLockedOut(_cli_ip) && pw && pw[0] && strcmp(_cli_line, pw) == 0) {
          LoginFail* e = loginFailEntry(_cli_ip, false);
          if (e) e->count = 0;
          _cli_authed = true;
          _cli_client.print("OK\r\n> ");
        } else {
          recordLoginFailure(_cli_ip);
          _cli_client.print("Denied\r\n");
          cliReset();
          return;
        }
      } else if (_cli_len > 0) {
        char reply[160];
        reply[0] = 0;
        if (_cmd) _cmd(_cli_line, reply, sizeof(reply));
        if (reply[0]) {
          _cli_client.print("  -> ");
          _cli_client.print(reply);
          _cli_client.print("\r\n");
        }
        _cli_client.print("> ");
      }
      _cli_len = 0;
    } else if (_cli_len < (int)sizeof(_cli_line) - 1) {
      _cli_line[_cli_len++] = c;
    }
  }
}

// ---- network CLI ----
// Reply format follows the companion's WiFi commands: every reply starts with "> ".
// Switches and 'net apply' take effect at the top of the next loop() pass, so a
// command typed on the TCP console still gets its reply before the link changes.
static bool isOwnedKey(const char* key) {
  static const char* const KEYS[] = {
#ifndef NET_WIFI_EXTERNAL
    "wifi.ssid", "wifi.pwd", "wifi.enabled", "wifi.status", "wifi.ip", "tcp.enabled",
#endif
    "wifi.power",
    "mqtt.enabled", "mqtt.host", "mqtt.port", "mqtt.user", "mqtt.pwd", "mqtt.topic",
    "net.telemetry", "ntp.enabled", "ntp.server", "ntp.interval", "obs.enabled", "obs.iata",
  };
  for (const char* k : KEYS) {
    if (!strcmp(key, k)) return true;
  }
  return false;
}

bool NetServices::handleCli(const char* command, char* reply, size_t n) {
  if (!strcmp(command, "net status")) {
    const char* ntp = !_cfg.ntp_enabled ? "off"
                    : (_ntp_state == NTP_SYNCED ? "synced"
                       : (_ntp_state == NTP_WAITING ? "waiting" : "idle"));
    char wifi_st[24];
    bool up = isWifiConnected();
#ifndef NET_WIFI_EXTERNAL
    if (!_cfg.wifi_enabled) strcpy(wifi_st, "off");
    else
#endif
    if (up) strcpy(wifi_st, "connected");
    else snprintf(wifi_st, sizeof(wifi_st), "connecting(wl=%d)", (int)WiFi.status());
    snprintf(reply, n, "wifi:%s ip:%s rssi:%ddBm mqtt:%s ntp:%s obs:%s pub:%u q:%d drop:%u",
             wifi_st,
             up ? WiFi.localIP().toString().c_str() : "-",
             up ? (int)WiFi.RSSI() : 0,
             !_cfg.mqtt_enabled ? "off" : (_mqtt.connected() ? "connected" : "down"),
             ntp,
             observerActive() ? _cfg.obs_iata : "off",
             (unsigned)_obs_published, _obs_count, (unsigned)_obs_dropped);
    return true;
  }
#ifndef NET_WIFI_EXTERNAL
  if (!strcmp(command, "net scan")) {
    // Blocking (~2-3 s): what the board itself sees with its own antenna.
    bool was_started = _wifi_started;
    if (was_started) WiFi.disconnect();   // a connect attempt in progress makes the scan fail
    else WiFi.mode(WIFI_STA);
    delay(150);
    int found = WiFi.scanNetworks();
    int len = snprintf(reply, n, "found:%d", found < 0 ? 0 : found);
    for (int i = 0; i < found && len < (int)n - 24; i++) {
      len += snprintf(reply + len, n - len, " %s:%ddBm", WiFi.SSID(i).c_str(), (int)WiFi.RSSI(i));
    }
    WiFi.scanDelete();
    if (!was_started) WiFi.mode(WIFI_OFF);
    else if (_cfg.wifi_ssid[0]) WiFi.begin(_cfg.wifi_ssid, _cfg.wifi_pwd);
    return true;
  }
  if (!strcmp(command, "set wifi.clear")) {
    _cfg.wifi_ssid[0] = 0;
    _cfg.wifi_pwd[0] = 0;
    _cfg.save();
    snprintf(reply, n, "> wifi config cleared");
    return true;
  }
#endif
  if (!strcmp(command, "net apply")) {
    _apply_pending = true;
    snprintf(reply, n, "> net applied");
    return true;
  }

  if (!strncmp(command, "get ", 4)) {
    const char* key = command + 4;
    if (!isOwnedKey(key)) return false;
    if      (!strcmp(key, "wifi.ssid"))     snprintf(reply, n, "> %s", _cfg.wifi_ssid[0] ? _cfg.wifi_ssid : "(not set)");
    else if (!strcmp(key, "wifi.pwd"))      snprintf(reply, n, "> %s", _cfg.wifi_pwd);
    else if (!strcmp(key, "wifi.enabled"))  snprintf(reply, n, "> %d", _cfg.wifi_enabled ? 1 : 0);
    else if (!strcmp(key, "wifi.status"))   snprintf(reply, n, "> %s", isWifiConnected() ? "connected" : "disconnected");
    else if (!strcmp(key, "wifi.ip")) {
      if (isWifiConnected()) snprintf(reply, n, "> %s", WiFi.localIP().toString().c_str());
      else snprintf(reply, n, "> (not connected)");
    }
    else if (!strcmp(key, "wifi.power"))    snprintf(reply, n, "> %d", _cfg.wifi_power);
    else if (!strcmp(key, "tcp.enabled"))   snprintf(reply, n, "> %d", _cfg.tcp_enabled ? 1 : 0);
    else if (!strcmp(key, "mqtt.enabled"))  snprintf(reply, n, "> %d", _cfg.mqtt_enabled ? 1 : 0);
    else if (!strcmp(key, "mqtt.host"))     snprintf(reply, n, "> %s", _cfg.mqtt_host);
    else if (!strcmp(key, "mqtt.port"))     snprintf(reply, n, "> %d", _cfg.mqtt_port);
    else if (!strcmp(key, "mqtt.user"))     snprintf(reply, n, "> %s", _cfg.mqtt_user);
    else if (!strcmp(key, "mqtt.pwd"))      snprintf(reply, n, "> %s", _cfg.mqtt_pwd);
    else if (!strcmp(key, "mqtt.topic"))    snprintf(reply, n, "> %s", _cfg.mqtt_topic);
    else if (!strcmp(key, "net.telemetry")) snprintf(reply, n, "> %d", _cfg.telemetry_s);
    else if (!strcmp(key, "ntp.enabled"))   snprintf(reply, n, "> %d", _cfg.ntp_enabled ? 1 : 0);
    else if (!strcmp(key, "ntp.server"))    snprintf(reply, n, "> %s", _cfg.ntp_server);
    else if (!strcmp(key, "ntp.interval"))  snprintf(reply, n, "> %d", _cfg.ntp_interval_s);
    else if (!strcmp(key, "obs.enabled"))   snprintf(reply, n, "> %d", _cfg.obs_enabled ? 1 : 0);
    else if (!strcmp(key, "obs.iata"))      snprintf(reply, n, "> %s", _cfg.obs_iata);
    return true;
  }

  if (!strncmp(command, "set ", 4)) {
    const char* rest = command + 4;
    const char* sp = strchr(rest, ' ');
    if (!sp) return false;                       // 'set foo' without a value: not ours
    char key[24];
    size_t klen = sp - rest;
    if (klen >= sizeof(key)) return false;
    memcpy(key, rest, klen);
    key[klen] = 0;
    if (!isOwnedKey(key) || !strcmp(key, "wifi.status") || !strcmp(key, "wifi.ip")) return false;
    const char* val = sp + 1;

    int num = atoi(val);
    if (!strcmp(key, "mqtt.port") && (num < 1 || num > 65535)) {
      snprintf(reply, n, "Error, port must be 1-65535");
      return true;
    }
    if (!strcmp(key, "wifi.power") && (num < 0 || num > 20)) {
      snprintf(reply, n, "Error, power must be 0-20 dBm");
      return true;
    }
    if (!strcmp(key, "net.telemetry") && num < 10) {
      snprintf(reply, n, "Error, minimum is 10 s");
      return true;
    }
    if (!strcmp(key, "ntp.interval") && num < 60) {
      snprintf(reply, n, "Error, minimum is 60 s");
      return true;
    }

    _cfg.assign(key, val);
    _cfg.save();
    if (!strcmp(key, "wifi.power") && _wifi_started) applyWifiPower();

    if (!strcmp(key, "wifi.pwd") || !strcmp(key, "mqtt.pwd")) {
      snprintf(reply, n, "> %s updated", key);
    } else {
      char shown[80];
      shown[0] = 0;
      char tmp[96];
      snprintf(tmp, sizeof(tmp), "get %s", key);
      handleCli(tmp, shown, sizeof(shown));      // echo the stored (normalised) value
      snprintf(reply, n, "> %s is now %s", key, shown[0] == '>' ? shown + 2 : shown);
    }
    return true;
  }

  return false;
}

// ---- MQTT ----
void NetServices::topicState(char* out, size_t n)  { snprintf(out, n, "%s/%s/state",  _cfg.mqtt_topic, _node_id); }
void NetServices::topicStatus(char* out, size_t n) { snprintf(out, n, "%s/%s/status", _cfg.mqtt_topic, _node_id); }

void NetServices::mqttOffline() {
  if (_mqtt.connected()) {
    char st[80];
    topicStatus(st, sizeof(st));
    _mqtt.publish(st, "offline", true);
    _mqtt.disconnect();
  }
  _discovery_sent = false;
}

bool NetServices::mqttConnect() {
  _mqtt.setServer(_cfg.mqtt_host, _cfg.mqtt_port);
  // Observer payloads carry up to 510 hex chars; PubSubClient silently drops any
  // publish larger than its buffer.
  _mqtt.setBufferSize(1024);
  _mqtt.setSocketTimeout(2);
  _mqtt.setKeepAlive(30);

  char status_topic[80];
  topicStatus(status_topic, sizeof(status_topic));
  char client_id[40];
  snprintf(client_id, sizeof(client_id), "mc_%s", _node_id);

  const char* user = _cfg.mqtt_user[0] ? _cfg.mqtt_user : nullptr;
  const char* pass = _cfg.mqtt_pwd[0]  ? _cfg.mqtt_pwd  : nullptr;
  bool ok = _mqtt.connect(client_id, user, pass, status_topic, 0, true, "offline");
  if (ok) {
    _mqtt.publish(status_topic, "online", true);
    _discovery_sent = false;
  }
  return ok;
}

void NetServices::mqttLoop() {
  if (!_cfg.mqtt_enabled || !_cfg.mqtt_host[0]) {
    mqttOffline();
    return;
  }

  if (!_mqtt.connected()) {
    unsigned long now = millis();
    if (now - _last_mqtt_retry < 5000UL) return;
    _last_mqtt_retry = now;
    if (!mqttConnect()) return;
  }
  _mqtt.loop();

  if (!_discovery_sent) {
    publishDiscovery();
    _discovery_sent = true;
  }

  unsigned long now = millis();
  if (now - _last_publish >= (unsigned long)_cfg.telemetry_s * 1000UL) {
    _last_publish = now;
    Telemetry t;
    if (_telem) _telem(t);
    publishState(t);
    publishObsStatus(t);
  }
  observerDrain();
}

void NetServices::publishState(const Telemetry& t) {
  char topic[80];
  topicState(topic, sizeof(topic));
  char payload[640];
  int len = snprintf(payload, sizeof(payload),
    "{\"batt_v\":%.2f,\"batt_pct\":%.0f,\"uptime_s\":%u,\"free_heap\":%u,"
    "\"neighbours\":%u,\"rx_count\":%u,\"tx_count\":%u,\"last_rssi\":%d,\"last_snr\":%.1f,"
    "\"noise_floor\":%d",
    t.batt_v, t.batt_pct, (unsigned)t.uptime_s, (unsigned)t.free_heap,
    (unsigned)t.neighbours, (unsigned)t.rx_count, (unsigned)t.tx_count,
    (int)t.last_rssi, t.last_snr, t.noise_floor);
  if (len < 0 || len >= (int)sizeof(payload)) return;

  // Append only the optional fields that were actually decoded (NAN = absent).
  struct { float v; const char* fmt; } opt[] = {
    {t.sens_temp, ",\"sens_temp\":%.1f"},
    {t.sens_hum,  ",\"sens_hum\":%.1f"},
    {t.sens_pres, ",\"sens_pres\":%.1f"},
    {t.ch1_v, ",\"ch1_v\":%.2f"}, {t.ch1_a, ",\"ch1_a\":%.3f"}, {t.ch1_w, ",\"ch1_w\":%.2f"},
    {t.ch2_v, ",\"ch2_v\":%.2f"}, {t.ch2_a, ",\"ch2_a\":%.3f"}, {t.ch2_w, ",\"ch2_w\":%.2f"},
    {t.ch3_v, ",\"ch3_v\":%.2f"}, {t.ch3_a, ",\"ch3_a\":%.3f"}, {t.ch3_w, ",\"ch3_w\":%.2f"},
  };
  for (auto& o : opt) {
    if (isnan(o.v) || len >= (int)sizeof(payload) - 1) continue;
    int w = snprintf(payload + len, sizeof(payload) - len, o.fmt, o.v);
    if (w > 0 && w < (int)sizeof(payload) - len) len += w;
  }

  if (len < (int)sizeof(payload) - 1) { payload[len++] = '}'; payload[len] = 0; }
  _mqtt.publish(topic, payload, true);
}

void NetServices::publishDiscovery() {
  char state_topic[80];
  topicState(state_topic, sizeof(state_topic));
  char status_topic[80];
  topicStatus(status_topic, sizeof(status_topic));

  // Sample once so only sensors present right now are advertised.
  Telemetry sample;
  if (_telem) _telem(sample);

  for (const MetricDef& m : METRICS) {
    if (m.opt && isnan(sample.*(m.opt))) continue;

    char cfg_topic[110];
    snprintf(cfg_topic, sizeof(cfg_topic),
             "homeassistant/sensor/mc_%s_%s/config", _node_id, m.field);

    char payload[640];
    int len = snprintf(payload, sizeof(payload),
      "{\"name\":\"%s\",\"uniq_id\":\"mc_%s_%s\",\"stat_t\":\"%s\","
      "\"val_tpl\":\"{{ value_json.%s }}\",\"stat_cla\":\"%s\","
      "\"avty_t\":\"%s\",\"pl_avail\":\"online\",\"pl_not_avail\":\"offline\","
      "\"dev\":{\"ids\":[\"mc_%s\"],\"name\":\"MeshCore %s\",\"mf\":\"MeshCore\",\"mdl\":\"Repeater\"}",
      m.name, _node_id, m.field, state_topic, m.field, m.stat_cla,
      status_topic, _node_id, _node_id);
    if (len < 0 || len >= (int)sizeof(payload)) continue;
    if (m.unit)
      len += snprintf(payload + len, sizeof(payload) - len, ",\"unit_of_meas\":\"%s\"", m.unit);
    if (len < (int)sizeof(payload) - 1 && m.dev_cla)
      len += snprintf(payload + len, sizeof(payload) - len, ",\"dev_cla\":\"%s\"", m.dev_cla);
    if (len < (int)sizeof(payload) - 1)
      snprintf(payload + len, sizeof(payload) - len, "}");

    _mqtt.publish(cfg_topic, payload, true);
    _mqtt.loop();
  }
}

// ---- Observer (liteScope / letsmesh schema) ----
void NetServices::setObserverInfo(const char* observer_id, const char* origin,
                                  const char* model, const char* fw_version) {
  strlcpy(_obs_id, observer_id, sizeof(_obs_id));
  strlcpy(_obs_model, model, sizeof(_obs_model));
  strlcpy(_obs_fw, fw_version, sizeof(_obs_fw));
  // origin goes inside JSON string literals: drop characters that would break them
  int j = 0;
  for (const char* p = origin; *p && j < (int)sizeof(_obs_origin) - 1; p++) {
    if (*p != '"' && *p != '\\') _obs_origin[j++] = *p;
  }
  _obs_origin[j] = 0;
}

void NetServices::queueRxPacket(const uint8_t* raw, int len, float snr, float rssi) {
  if (!observerActive() || len <= 0) return;
  if (len > (int)sizeof(ObsPacket::raw)) len = sizeof(ObsPacket::raw);

  int slot;
  if (_obs_count < OBS_QUEUE_SLOTS) {
    slot = (_obs_head + _obs_count) % OBS_QUEUE_SLOTS;
    _obs_count++;
  } else {                                   // full: drop the oldest
    slot = _obs_head;
    _obs_head = (_obs_head + 1) % OBS_QUEUE_SLOTS;
    _obs_dropped++;
  }
  ObsPacket& p = _obs_q[slot];
  p.len = (uint8_t)len;
  p.snr = snr;
  p.rssi = rssi;
  memcpy(p.raw, raw, len);
}

// At most 2 packets per loop() pass, so a burst never stalls mesh routing. No
// timestamp field: the receiving side stamps the time.
void NetServices::observerDrain() {
  if (!observerActive()) return;
  for (int n = 0; n < 2 && _obs_count > 0; n++) {
    if (!_mqtt.connected()) return;          // keep the queue for the reconnect
    ObsPacket& p = _obs_q[_obs_head];

    char topic[112];
    snprintf(topic, sizeof(topic), "%s/%s/%s/rx", _cfg.mqtt_topic, _cfg.obs_iata, _obs_id);

    static char payload[640];                // 510 hex + JSON wrapper
    int len = snprintf(payload, sizeof(payload), "{\"origin\":\"%s\",\"raw\":\"", _obs_origin);
    for (int i = 0; i < p.len && len < (int)sizeof(payload) - 40; i++) {
      len += snprintf(payload + len, sizeof(payload) - len, "%02x", p.raw[i]);
    }
    len += snprintf(payload + len, sizeof(payload) - len,
                    "\",\"SNR\":%.2f,\"RSSI\":%.1f}", p.snr, p.rssi);
    if (len > 0 && len < (int)sizeof(payload)) {
      if (_mqtt.publish(topic, payload, false)) _obs_published++;
    }
    _obs_head = (_obs_head + 1) % OBS_QUEUE_SLOTS;
    _obs_count--;
  }
}

void NetServices::publishObsStatus(const Telemetry& t) {
  if (!observerActive()) return;
  char topic[120];
  snprintf(topic, sizeof(topic), "%s/%s/%s/status", _cfg.mqtt_topic, _cfg.obs_iata, _obs_id);
  char payload[320];
  snprintf(payload, sizeof(payload),
    "{\"origin\":\"%s\",\"model\":\"%s\",\"firmware_version\":\"%s\","
    "\"stats\":{\"battery_mv\":%d,\"uptime_secs\":%u,\"noise_floor\":%d}}",
    _obs_origin, _obs_model, _obs_fw,
    (int)(t.batt_v * 1000.0f), (unsigned)t.uptime_s, t.noise_floor);
  _mqtt.publish(topic, payload, false);
}

#endif // ESP32 && WITH_NET_SERVICES
