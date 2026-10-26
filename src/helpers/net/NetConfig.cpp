#if defined(ESP32) && defined(WITH_NET_SERVICES)

#include "NetConfig.h"
#include <SPIFFS.h>

#define NETCFG_FILE "/netcfg"

int NetConfig::maxLen(const char* k) {
  if (!strcmp(k, "wifi.ssid"))   return sizeof(NetConfig::wifi_ssid) - 1;
  if (!strcmp(k, "wifi.pwd"))    return sizeof(NetConfig::wifi_pwd) - 1;
  if (!strcmp(k, "mqtt.host"))   return sizeof(NetConfig::mqtt_host) - 1;
  if (!strcmp(k, "mqtt.user"))   return sizeof(NetConfig::mqtt_user) - 1;
  if (!strcmp(k, "mqtt.pwd"))    return sizeof(NetConfig::mqtt_pwd) - 1;
  if (!strcmp(k, "mqtt.topic"))  return sizeof(NetConfig::mqtt_topic) - 1;
  if (!strcmp(k, "ntp.server"))  return sizeof(NetConfig::ntp_server) - 1;
  if (!strcmp(k, "obs.iata"))    return sizeof(NetConfig::obs_iata) - 1;
  if (!strcmp(k, "obs.topic"))   return sizeof(NetConfig::obs_topic) - 1;
  return -1;
}

bool NetConfig::assign(const char* k, const char* v) {
  if      (!strcmp(k, "wifi.enabled"))  wifi_enabled = (atoi(v) != 0);
  else if (!strcmp(k, "wifi.ssid"))     strlcpy(wifi_ssid, v, sizeof(wifi_ssid));
  else if (!strcmp(k, "wifi.pwd"))      strlcpy(wifi_pwd, v, sizeof(wifi_pwd));
  else if (!strcmp(k, "wifi.power"))    wifi_power = atoi(v);
  else if (!strcmp(k, "tcp.enabled"))   tcp_enabled = (atoi(v) != 0);
  else if (!strcmp(k, "mqtt.enabled"))  mqtt_enabled = (atoi(v) != 0);
  else if (!strcmp(k, "mqtt.host"))     strlcpy(mqtt_host, v, sizeof(mqtt_host));
  else if (!strcmp(k, "mqtt.port"))     mqtt_port = atoi(v);
  else if (!strcmp(k, "mqtt.user"))     strlcpy(mqtt_user, v, sizeof(mqtt_user));
  else if (!strcmp(k, "mqtt.pwd"))      strlcpy(mqtt_pwd, v, sizeof(mqtt_pwd));
  else if (!strcmp(k, "mqtt.topic"))    strlcpy(mqtt_topic, v, sizeof(mqtt_topic));
  else if (!strcmp(k, "net.telemetry")) telemetry_s = atoi(v);
  else if (!strcmp(k, "ntp.enabled"))   ntp_enabled = (atoi(v) != 0);
  else if (!strcmp(k, "ntp.server"))    strlcpy(ntp_server, v, sizeof(ntp_server));
  else if (!strcmp(k, "ntp.interval"))  ntp_interval_s = atoi(v);
  else if (!strcmp(k, "obs.enabled"))   obs_enabled = (atoi(v) != 0);
  else if (!strcmp(k, "obs.iata")) {
    strlcpy(obs_iata, v, sizeof(obs_iata));
    for (char* p = obs_iata; *p; p++) *p = toupper(*p);
  }
  else if (!strcmp(k, "obs.topic"))     strlcpy(obs_topic, v, sizeof(obs_topic));
  else if (!strcmp(k, "ota.timeout")) {
    ota_timeout_s = atoi(v);
    if (ota_timeout_s < 0) ota_timeout_s = 0;
    if (ota_timeout_s > NET_OTA_TIMEOUT_MAX_S) ota_timeout_s = NET_OTA_TIMEOUT_MAX_S;
  }
  else return false;
  return true;
}

void NetConfig::loadDefaults() {
  wifi_enabled = false;
  strlcpy(wifi_ssid, WIFI_SSID, sizeof(wifi_ssid));
  strlcpy(wifi_pwd,  WIFI_PWD,  sizeof(wifi_pwd));
  wifi_power = WIFI_TX_POWER_DBM;
  tcp_enabled = false;
  mqtt_enabled = false;
  strlcpy(mqtt_host, MQTT_HOST, sizeof(mqtt_host));
  mqtt_port = MQTT_PORT;
  strlcpy(mqtt_user, MQTT_USER, sizeof(mqtt_user));
  strlcpy(mqtt_pwd,  MQTT_PWD,  sizeof(mqtt_pwd));
  strlcpy(mqtt_topic, MQTT_BASE_TOPIC, sizeof(mqtt_topic));
  telemetry_s = TELEMETRY_INTERVAL_SECS;
  ntp_enabled = false;
  strlcpy(ntp_server, NTP_SERVER, sizeof(ntp_server));
  ntp_interval_s = NTP_INTERVAL_SECS;
  obs_enabled = false;
  strlcpy(obs_iata, OBSERVER_IATA, sizeof(obs_iata));
  strlcpy(obs_topic, OBSERVER_TOPIC, sizeof(obs_topic));
  ota_timeout_s = NET_OTA_TIMEOUT_S;
}

void NetConfig::load() {
  loadDefaults();                       // keys missing from the file keep their default
  File f = SPIFFS.open(NETCFG_FILE, FILE_READ);
  if (!f) return;
  while (f.available()) {
    String line = f.readStringUntil('\n');
    line.trim();
    if (line.length() == 0 || line[0] == '#') continue;
    int eq = line.indexOf('=');
    if (eq < 1) continue;
    String k = line.substring(0, eq);
    String v = line.substring(eq + 1);
    assign(k.c_str(), v.c_str());
  }
  f.close();
}

void NetConfig::save() {
  File f = SPIFFS.open(NETCFG_FILE, FILE_WRITE);
  if (!f) return;
  f.printf("wifi.enabled=%d\n", wifi_enabled ? 1 : 0);
  f.printf("wifi.ssid=%s\n", wifi_ssid);
  f.printf("wifi.pwd=%s\n", wifi_pwd);
  f.printf("wifi.power=%d\n", wifi_power);
  f.printf("tcp.enabled=%d\n", tcp_enabled ? 1 : 0);
  f.printf("mqtt.enabled=%d\n", mqtt_enabled ? 1 : 0);
  f.printf("mqtt.host=%s\n", mqtt_host);
  f.printf("mqtt.port=%d\n", mqtt_port);
  f.printf("mqtt.user=%s\n", mqtt_user);
  f.printf("mqtt.pwd=%s\n", mqtt_pwd);
  f.printf("mqtt.topic=%s\n", mqtt_topic);
  f.printf("net.telemetry=%d\n", telemetry_s);
  f.printf("ntp.enabled=%d\n", ntp_enabled ? 1 : 0);
  f.printf("ntp.server=%s\n", ntp_server);
  f.printf("ntp.interval=%d\n", ntp_interval_s);
  f.printf("obs.enabled=%d\n", obs_enabled ? 1 : 0);
  f.printf("obs.iata=%s\n", obs_iata);
  f.printf("obs.topic=%s\n", obs_topic);
  f.printf("ota.timeout=%d\n", ota_timeout_s);
  f.close();
}

#endif // ESP32 && WITH_NET_SERVICES
