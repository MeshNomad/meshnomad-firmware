#if defined(ESP32) && defined(WITH_NET_SERVICES)

#include "NetServices.h"
#include "NetHooks.h"
#include <time.h>
#include <esp_sntp.h>
#include <esp_idf_version.h>
#include <esp_heap_caps.h>

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
                        PasswordFn console_password, InhibitSleepFn inhibit_sleep,
                        const char* default_password, const char* node_name) {
  strlcpy(_node_id, node_id, sizeof(_node_id));
  _default_password = default_password;
  _node_name = node_name;
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
#ifndef NET_WIFI_EXTERNAL
  _ota.loop();                 // first: timeout, deferred stop and restart run even with WiFi down
#endif
  if (_apply_pending) {        // 'net apply': restart everything with the stored config
    _apply_pending = false;
    stopAll();
  }

#ifndef NET_WIFI_EXTERNAL
  applyWifiState();
  updateSleepInhibit();        // releases the inhibit once a stopped session is fully off
  if (!_wifi_started) return;
  applyTcpState();

  if (WiFi.status() != WL_CONNECTED) {
    if (_ota.running()) _ota.stop();   // WiFi dropped: end the session, no reboot
    ntpStop();                 // re-armed on reconnect
    if (_cli_srv.connected()) cliReset();
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
  _ota.stop();                 // aborts an upload (no reboot); the server ends in _ota.loop()
  cliReset();
  _cli_srv.end();              // listener closed; the key/cert stay loaded
  if (_wifi_started) {
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    _wifi_started = false;
  }
  updateSleepInhibit();        // stays inhibited until the ota server is fully off
#endif
}

void NetServices::applyWifiState() {
#ifndef NET_WIFI_EXTERNAL
  if (_cfg.wifi_enabled && !_wifi_started) {
    setSleepInhibit(true);     // light sleep would drop the WiFi link
    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(true);
    if (_cfg.wifi_ssid[0]) WiFi.begin(_cfg.wifi_ssid, _cfg.wifi_pwd);
    applyWifiPower();
    _wifi_started = true;
    _last_wifi_retry = millis();
    ensureTls();               // after WIFI_STA: the radio feeds the hardware RNG
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

#ifndef NET_WIFI_EXTERNAL
// ---- TLS console ----
// TLS only (no plain fallback): `openssl s_client -quiet -connect <ip>:5000`.
// Protocol after the handshake: "Password: " -> password + "\n" -> "OK\r\n> " or
// "Denied\r\n" (closed); each reply "  -> <reply>\r\n" followed by "> "; idle timeout 120 s.
// Login rules: the password must be entered within 15 s of connecting; an empty
// node password never matches; 3 failed logins lock that client IP out for
// 5 minutes (the socket is then closed before any handshake). While the admin
// password is still the compiled default the console refuses every login (gate).
static const unsigned long LOGIN_DEADLINE_MS = 15000UL;
static const uint8_t       LOGIN_MAX_FAILS   = 3;
static const unsigned long LOGIN_LOCKOUT_MS  = 300000UL;
static const uint32_t      CLI_HANDSHAKE_MS  = 4000;      // loop() is blocked at most this long
static const size_t        CLI_MIN_HEAP_OTA  = 48 * 1024; // console refused below this while net ota runs
static const char* const   PW_GATE_MSG = "Error: change the admin password first (password <new>)";

void NetServices::ensureTls() {
  if (NetTls::ready()) return;
  if (!NetTls::ensure(_node_id, _node_name ? _node_name : "")) {
    Serial.println("NetServices: no TLS certificate, console stays closed");
  }
}

bool NetServices::adminPasswordIsDefault() const {
  const char* pw = _console_password ? _console_password() : nullptr;
  if (!pw || !pw[0]) return true;
  return _default_password && strcmp(pw, _default_password) == 0;
}

// Unlocked core; the table is shared with the httpd task (NetOta), so every entry
// point goes through the lockout*() wrappers below.
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

// Thread-safe wrappers (portMUX critical section: no blocking calls inside).
bool NetServices::lockoutCheck(uint32_t ip) {
  portENTER_CRITICAL(&_lockout_mux);
  bool locked = isLockedOut(ip);
  portEXIT_CRITICAL(&_lockout_mux);
  return locked;
}

void NetServices::lockoutFail(uint32_t ip) {
  portENTER_CRITICAL(&_lockout_mux);
  recordLoginFailure(ip);
  portEXIT_CRITICAL(&_lockout_mux);
}

void NetServices::lockoutOk(uint32_t ip) {
  portENTER_CRITICAL(&_lockout_mux);
  LoginFail* e = loginFailEntry(ip, false);
  if (e) e->count = 0;
  portEXIT_CRITICAL(&_lockout_mux);
}

void NetServices::applyTcpState() {
  // No certificate (generation failed) means no listener: there is no plain mode.
  bool want = _wifi_started && _cfg.tcp_enabled && NetTls::ready();
  if (want && !_cli_srv.listening()) {
    static unsigned long last_try = 0;         // retry a failed bind every 5 s, not per pass
    if (millis() - last_try < 5000UL && last_try) return;
    last_try = millis();
    if (!_cli_srv.begin(TCP_CLI_PORT)) Serial.println("NetServices: console listen failed");
  } else if (!want && _cli_srv.listening()) {
    cliReset();
    _cli_srv.end();
  }
}

void NetServices::cliReset() {
  _cli_srv.disconnect();       // frees the TLS session; idempotent
  _cli_authed = false;
  _cli_len = 0;
}

// Prints the peer address the way the operator sees it in the boot log.
static void fmtIp(uint32_t raw, char* out, size_t n) {
  snprintf(out, n, "%u.%u.%u.%u", (unsigned)(raw & 0xff), (unsigned)((raw >> 8) & 0xff),
           (unsigned)((raw >> 16) & 0xff), (unsigned)((raw >> 24) & 0xff));
}

void NetServices::cliAccept() {
  uint32_t ip = 0;
  int fd = _cli_srv.pollAccept(&ip);
  if (fd < 0) return;
  char ip_s[16];
  fmtIp(ip, ip_s, sizeof(ip_s));
  if (lockoutCheck(ip)) {      // locked out: no TLS work at all for this peer
    NetTls::Server::reject(fd);
    Serial.printf("NetServices: console %s locked out, closed\n", ip_s);
    return;
  }
  // A net ota session may hold the heap for its own TLS session: no handshake, no
  // lockout count, just close.
  if (_ota.active() &&
      heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT) < CLI_MIN_HEAP_OTA) {
    NetTls::Server::reject(fd);
    Serial.printf("NetServices: console %s refused, net ota session holds the heap\n", ip_s);
    return;
  }
  // Blocks loop() for the handshake (software ECDHE/ECDSA) or up to CLI_HANDSHAKE_MS
  // for a silent peer such as plain `nc`.
  if (!_cli_srv.handshake(fd, CLI_HANDSHAKE_MS)) {
    lockoutFail(ip);           // LOCKOUT-HS: a failed/aborted handshake = one failed login
    Serial.printf("NetServices: console %s TLS handshake failed after %lu ms\n",
                  ip_s, (unsigned long)_cli_srv.handshakeMs());
    return;
  }
  Serial.printf("NetServices: console %s TLS handshake %lu ms\n",
                ip_s, (unsigned long)_cli_srv.handshakeMs());
  _cli_authed = false;
  _cli_len = 0;
  _cli_last_activity = millis();
  _cli_connected_at = _cli_last_activity;
  _cli_ip = ip;
  if (adminPasswordIsDefault()) {   // gate: not a failed login, no lockout count
    _cli_srv.print(PW_GATE_MSG);
    _cli_srv.print("\r\n");
    cliReset();
    return;
  }
  _cli_srv.print("Password: ");
}

void NetServices::cliLine() {
  _cli_line[_cli_len] = 0;
  if (!_cli_authed) {
    const char* pw = _console_password ? _console_password() : nullptr;
    if (!lockoutCheck(_cli_ip) && pw && pw[0] && strcmp(_cli_line, pw) == 0) {
      lockoutOk(_cli_ip);
      _cli_authed = true;
      _cli_srv.print("OK\r\n> ");
    } else {
      lockoutFail(_cli_ip);
      _cli_srv.print("Denied\r\n");
      cliReset();
    }
  } else if (_cli_len > 0) {
    char reply[160];
    reply[0] = 0;
    if (_cmd) _cmd(_cli_line, reply, sizeof(reply));
    if (reply[0]) {
      _cli_srv.print("  -> ");
      _cli_srv.print(reply);
      _cli_srv.print("\r\n");
    }
    _cli_srv.print("> ");
  }
  _cli_len = 0;
}

void NetServices::cliLoop() {
  if (!_cli_srv.listening()) return;
  if (!_cli_srv.connected()) {
    cliAccept();
    return;
  }

  if (millis() - _cli_last_activity > 120000UL ||
      (!_cli_authed && millis() - _cli_connected_at > LOGIN_DEADLINE_MS)) {
    cliReset();
    return;
  }

  char buf[64];
  int r;
  while (_cli_srv.connected() && (r = _cli_srv.readSome(buf, sizeof(buf))) != 0) {
    if (r < 0) {               // peer closed or TLS error: the session is gone already
      cliReset();
      return;
    }
    _cli_last_activity = millis();
    for (int i = 0; i < r && _cli_srv.connected(); i++) {
      char c = buf[i];
      if (c == '\r') continue;
      if (c == '\n') cliLine();                 // may reset the session (Denied)
      else if (_cli_len < (int)sizeof(_cli_line) - 1) _cli_line[_cli_len++] = c;
    }
  }
}

// ---- net ota ----
// One HTTPS session needs two 16 KB TLS buffers plus handshake state; with the console
// session open the node holds two TLS sessions, never a third (TLS-6: 60 KB gate).
// Without an open console session, 40 KB more stay free so a console can still connect.
static const size_t   OTA_MIN_FREE_HEAP  = 60 * 1024;
static const size_t   OTA_CONSOLE_RESERVE = 40 * 1024;
static const size_t   OTA_MIN_FREE_BLOCK = 40 * 1024;
static const char* const OTA_STOPPED_SUFFIX = " (net ota stopped)";

void NetServices::setSleepInhibit(bool on) {
  if (on == _sleep_inhibited) return;
  _sleep_inhibited = on;
  if (_inhibit_sleep) _inhibit_sleep(on);
}

void NetServices::updateSleepInhibit() {
  setSleepInhibit(_wifi_started || _ota.active());
}

bool NetServices::otaStopForNet() {
  if (!_ota.running()) return false;
  _ota.stop();
  return true;
}

bool NetServices::otaIsLocked(uint32_t ip) { return g_net && g_net->lockoutCheck(ip); }
void NetServices::otaLoginFailed(uint32_t ip) { if (g_net) g_net->lockoutFail(ip); }
void NetServices::otaLoginOk(uint32_t ip) { if (g_net) g_net->lockoutOk(ip); }

// The default-password gate holds for uploads too: no password means no login.
const char* NetServices::otaPassword() {
  if (!g_net || !g_net->_console_password || g_net->adminPasswordIsDefault()) return nullptr;
  return g_net->_console_password();
}

// arg: the text after "net ota" (leading spaces allowed). Replies stay <= 120 chars.
void NetServices::otaCli(const char* arg, char* reply, size_t n) {
  while (*arg == ' ') arg++;
  char ip[16];
  strlcpy(ip, WiFi.localIP().toString().c_str(), sizeof(ip));

  if (!strcmp(arg, "start")) {
    if (!_wifi_started || WiFi.status() != WL_CONNECTED) {
      snprintf(reply, n, "Error: WiFi not connected (wl=%d)", (int)WiFi.status());
      return;
    }
    if (adminPasswordIsDefault()) {
      snprintf(reply, n, "%s", PW_GATE_MSG);
      return;
    }
    if (!_ota.active()) {      // a running session re-arms its timeout, needs no new memory
      size_t free_b  = heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
      size_t block_b = heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
      size_t need = OTA_MIN_FREE_HEAP + (_cli_srv.connected() ? 0 : OTA_CONSOLE_RESERVE);
      if (free_b < need || block_b < OTA_MIN_FREE_BLOCK) {
        snprintf(reply, n, "Error: not enough memory");
        return;
      }
    }
    if (WiFi.getMode() & WIFI_MODE_AP) {
      snprintf(reply, n, "Error: MeshCore OTA access point active, reboot first");
      return;
    }
    NetOtaHooks hooks = {otaIsLocked, otaLoginFailed, otaLoginOk, otaPassword};
    if (!_ota.start(hooks, (uint32_t)_cfg.ota_timeout_s)) {
      snprintf(reply, n, "Error: %s", _ota.lastError());
      return;
    }
    updateSleepInhibit();
    if (_cfg.ota_timeout_s > 0) snprintf(reply, n, "> https://%s/update (%d s)", ip, _cfg.ota_timeout_s);
    else snprintf(reply, n, "> https://%s/update (no limit)", ip);
    return;
  }
  if (!strcmp(arg, "stop")) {
    if (_ota.active()) {
      _ota.stop();             // no-op while already stopping
      snprintf(reply, n, "> net ota stopped");
    } else {
      snprintf(reply, n, "> off");
    }
    return;
  }
  if (!strcmp(arg, "status")) {
    if (_ota.restartPending()) snprintf(reply, n, "> update done, restarting");
    else if (!_ota.active())   snprintf(reply, n, "> off");
    else if (!_ota.running())  snprintf(reply, n, "> stopping");
    else {
      char left[12];
      if (_ota.hasLimit()) snprintf(left, sizeof(left), "%u", (unsigned)_ota.secondsLeft());
      else strcpy(left, "-");
      char up[12];
      if (_ota.uploading()) snprintf(up, sizeof(up), "%u", (unsigned)_ota.uploadedBytes());
      else strcpy(up, "idle");
      snprintf(reply, n, "> on https://%s/update left:%s upload:%s", ip, left, up);
    }
    return;
  }
  snprintf(reply, n, "Error: net ota start|stop|status");
}
#endif // !NET_WIFI_EXTERNAL

// ---- network CLI ----
// Reply format follows the companion's WiFi commands: every reply starts with "> ".
// Switches and 'net apply' take effect at the top of the next loop() pass, so a
// command typed on the TCP console still gets its reply before the link changes.
static bool isOwnedKey(const char* key) {
  static const char* const KEYS[] = {
#ifndef NET_WIFI_EXTERNAL
    "wifi.ssid", "wifi.pwd", "wifi.enabled", "wifi.status", "wifi.ip", "tcp.enabled", "net.cert",
    "ota.timeout",
#endif
    "wifi.power",
    "mqtt.enabled", "mqtt.host", "mqtt.port", "mqtt.user", "mqtt.pwd", "mqtt.topic",
    "net.telemetry", "ntp.enabled", "ntp.server", "ntp.interval", "obs.enabled", "obs.iata", "obs.topic",
  };
  for (const char* k : KEYS) {
    if (!strcmp(key, k)) return true;
  }
  return false;
}

bool NetServices::handleCli(const char* command, char* reply, size_t n) {
#ifndef NET_WIFI_EXTERNAL
  // Guards on upstream 'start ota': 9-char prefix match like CommonCLI's
  // memcmp(command, "start ota", 9), so 'start ota x' is caught too; otherwise
  // upstream handles it unchanged.
  if (!strncmp(command, "start ota", 9)) {
    if (_ota.active() || _ota.restartPending()) {
      snprintf(reply, n, "Error: net ota session active (net ota stop first)");
      return true;
    }
    if (_cfg.wifi_enabled) {
      snprintf(reply, n, "Error: WiFi on - use net ota start, or set wifi.enabled 0");
      return true;
    }
    return false;
  }
  if (!strncmp(command, "net ota", 7) && (command[7] == 0 || command[7] == ' ')) {
    otaCli(command + 7, reply, n);
    return true;
  }
#endif
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
    bool ota_stopped = otaStopForNet();   // the scan drops the link: end the session first
    size_t lim = n;                       // room for the suffix within 120 chars
    if (ota_stopped && lim > 121 - strlen(OTA_STOPPED_SUFFIX)) lim = 121 - strlen(OTA_STOPPED_SUFFIX);
    bool was_started = _wifi_started;
    if (was_started) WiFi.disconnect();   // a connect attempt in progress makes the scan fail
    else WiFi.mode(WIFI_STA);
    delay(150);
    int found = WiFi.scanNetworks();
    int len = snprintf(reply, lim, "found:%d", found < 0 ? 0 : found);
    for (int i = 0; i < found && len < (int)lim - 24; i++) {
      len += snprintf(reply + len, lim - len, " %s:%ddBm", WiFi.SSID(i).c_str(), (int)WiFi.RSSI(i));
    }
    WiFi.scanDelete();
    if (!was_started) WiFi.mode(WIFI_OFF);
    else if (_cfg.wifi_ssid[0]) WiFi.begin(_cfg.wifi_ssid, _cfg.wifi_pwd);
    if (ota_stopped) strlcat(reply, OTA_STOPPED_SUFFIX, n);
    return true;
  }
  if (!strcmp(command, "set wifi.clear")) {
    _cfg.wifi_ssid[0] = 0;
    _cfg.wifi_pwd[0] = 0;
    _cfg.save();
    snprintf(reply, n, "> wifi config cleared");
    return true;
  }
  if (!strcmp(command, "net cert renew")) {
    // Needs the radio on for the hardware RNG. Blocks for the key generation; an open
    // console session keeps running on the old certificate until it disconnects.
    if (!_wifi_started) {
      snprintf(reply, n, "Error: WiFi off, set wifi.enabled 1 first");
      return true;
    }
    if (!NetTls::renew(_node_id, _node_name ? _node_name : "")) {
      snprintf(reply, n, "Error: certificate generation failed");
      return true;
    }
    char fp[80];
    NetTls::fingerprint(fp, sizeof(fp));
    snprintf(reply, n, "> %s", fp);
    return true;
  }
#endif
  if (!strcmp(command, "net apply")) {
    _apply_pending = true;
#ifndef NET_WIFI_EXTERNAL
    if (otaStopForNet()) {     // stopAll() restarts WiFi: end the session now, no reboot
      snprintf(reply, n, "> net applied%s", OTA_STOPPED_SUFFIX);
      return true;
    }
#endif
    snprintf(reply, n, "> net applied");
    return true;
  }

  if (!strncmp(command, "get ", 4)) {
    const char* key = command + 4;
    if (!isOwnedKey(key)) return false;
    if      (!strcmp(key, "wifi.ssid"))     snprintf(reply, n, "> %s", _cfg.wifi_ssid[0] ? _cfg.wifi_ssid : "(not set)");
    else if (!strcmp(key, "wifi.pwd"))      snprintf(reply, n, "> %s", _cfg.wifi_pwd[0] ? "(set)" : "(not set)");
    else if (!strcmp(key, "wifi.enabled"))  snprintf(reply, n, "> %d", _cfg.wifi_enabled ? 1 : 0);
    else if (!strcmp(key, "wifi.status"))   snprintf(reply, n, "> %s", isWifiConnected() ? "connected" : "disconnected");
    else if (!strcmp(key, "wifi.ip")) {
      if (isWifiConnected()) snprintf(reply, n, "> %s", WiFi.localIP().toString().c_str());
      else snprintf(reply, n, "> (not connected)");
    }
    else if (!strcmp(key, "wifi.power"))    snprintf(reply, n, "> %d", _cfg.wifi_power);
    else if (!strcmp(key, "tcp.enabled"))   snprintf(reply, n, "> %d", _cfg.tcp_enabled ? 1 : 0);
#ifndef NET_WIFI_EXTERNAL
    else if (!strcmp(key, "net.cert")) {    // SHA-256 of the certificate DER, never the key
      char fp[80];
      if (NetTls::fingerprint(fp, sizeof(fp))) snprintf(reply, n, "> %s", fp);
      else snprintf(reply, n, "Error: no certificate yet, set wifi.enabled 1 first");
    }
    else if (!strcmp(key, "ota.timeout"))   snprintf(reply, n, "> %d", _cfg.ota_timeout_s);
#endif
    else if (!strcmp(key, "mqtt.enabled"))  snprintf(reply, n, "> %d", _cfg.mqtt_enabled ? 1 : 0);
    else if (!strcmp(key, "mqtt.host"))     snprintf(reply, n, "> %s", _cfg.mqtt_host);
    else if (!strcmp(key, "mqtt.port"))     snprintf(reply, n, "> %d", _cfg.mqtt_port);
    else if (!strcmp(key, "mqtt.user"))     snprintf(reply, n, "> %s", _cfg.mqtt_user);
    else if (!strcmp(key, "mqtt.pwd"))      snprintf(reply, n, "> %s", _cfg.mqtt_pwd[0] ? "(set)" : "(not set)");
    else if (!strcmp(key, "mqtt.topic"))    snprintf(reply, n, "> %s", _cfg.mqtt_topic);
    else if (!strcmp(key, "net.telemetry")) snprintf(reply, n, "> %d", _cfg.telemetry_s);
    else if (!strcmp(key, "ntp.enabled"))   snprintf(reply, n, "> %d", _cfg.ntp_enabled ? 1 : 0);
    else if (!strcmp(key, "ntp.server"))    snprintf(reply, n, "> %s", _cfg.ntp_server);
    else if (!strcmp(key, "ntp.interval"))  snprintf(reply, n, "> %d", _cfg.ntp_interval_s);
    else if (!strcmp(key, "obs.enabled"))   snprintf(reply, n, "> %d", _cfg.obs_enabled ? 1 : 0);
    else if (!strcmp(key, "obs.iata"))      snprintf(reply, n, "> %s", _cfg.obs_iata);
    else if (!strcmp(key, "obs.topic"))     snprintf(reply, n, "> %s", _cfg.obs_topic);
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
    if (!isOwnedKey(key) || !strcmp(key, "wifi.status") || !strcmp(key, "wifi.ip") ||
        !strcmp(key, "net.cert")) return false;   // read-only keys
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
    bool ota_stopped = false;
#ifndef NET_WIFI_EXTERNAL
    if (!strcmp(key, "ota.timeout")) {          // digits only: 'abc' must not mean "no limit"
      bool digits = val[0] != 0;
      for (const char* p = val; *p; p++) digits = digits && isdigit((unsigned char)*p);
      if (!digits || strlen(val) > 6 || num > NET_OTA_TIMEOUT_MAX_S) {
        snprintf(reply, n, "Error, timeout must be 0-%d s (0 = no limit)", NET_OTA_TIMEOUT_MAX_S);
        return true;
      }
    }
    // WiFi off ends a running session first (abort, no reboot).
    if (!strcmp(key, "wifi.enabled") && num == 0) ota_stopped = otaStopForNet();
#endif

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
      snprintf(reply, n, "> %s is now %s%s", key, shown[0] == '>' ? shown + 2 : shown,
               ota_stopped ? " (net ota stopped)" : "");
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
    snprintf(topic, sizeof(topic), "%s/%s/%s/rx", _cfg.obs_topic, _cfg.obs_iata, _obs_id);

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
  snprintf(topic, sizeof(topic), "%s/%s/%s/status", _cfg.obs_topic, _cfg.obs_iata, _obs_id);
  char payload[320];
  snprintf(payload, sizeof(payload),
    "{\"origin\":\"%s\",\"model\":\"%s\",\"firmware_version\":\"%s\","
    "\"stats\":{\"battery_mv\":%d,\"uptime_secs\":%u,\"noise_floor\":%d}}",
    _obs_origin, _obs_model, _obs_fw,
    (int)(t.batt_v * 1000.0f), (unsigned)t.uptime_s, t.noise_floor);
  _mqtt.publish(topic, payload, false);
}

#endif // ESP32 && WITH_NET_SERVICES
