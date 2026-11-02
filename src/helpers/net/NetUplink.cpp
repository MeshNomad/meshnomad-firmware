#include "NetUplink.h"

#if defined(ESP32) && defined(WITH_NET_SERVICES) && !defined(NET_WIFI_EXTERNAL)

#include <time.h>
#include <esp_heap_caps.h>
#include <esp_tls.h>
#include <Packet.h>

// Trust anchors for TLS uplinks (OD14): Let's Encrypt and Google Trust Services cover every
// broker checked on 2026-11-02. Loaded once into the esp-tls global store.
static const char UPLINK_CA_PEM[] =
// ISRG Root X1
"-----BEGIN CERTIFICATE-----\n"
"MIIFazCCA1OgAwIBAgIRAIIQz7DSQONZRGPgu2OCiwAwDQYJKoZIhvcNAQELBQAw\n"
"TzELMAkGA1UEBhMCVVMxKTAnBgNVBAoTIEludGVybmV0IFNlY3VyaXR5IFJlc2Vh\n"
"cmNoIEdyb3VwMRUwEwYDVQQDEwxJU1JHIFJvb3QgWDEwHhcNMTUwNjA0MTEwNDM4\n"
"WhcNMzUwNjA0MTEwNDM4WjBPMQswCQYDVQQGEwJVUzEpMCcGA1UEChMgSW50ZXJu\n"
"ZXQgU2VjdXJpdHkgUmVzZWFyY2ggR3JvdXAxFTATBgNVBAMTDElTUkcgUm9vdCBY\n"
"MTCCAiIwDQYJKoZIhvcNAQEBBQADggIPADCCAgoCggIBAK3oJHP0FDfzm54rVygc\n"
"h77ct984kIxuPOZXoHj3dcKi/vVqbvYATyjb3miGbESTtrFj/RQSa78f0uoxmyF+\n"
"0TM8ukj13Xnfs7j/EvEhmkvBioZxaUpmZmyPfjxwv60pIgbz5MDmgK7iS4+3mX6U\n"
"A5/TR5d8mUgjU+g4rk8Kb4Mu0UlXjIB0ttov0DiNewNwIRt18jA8+o+u3dpjq+sW\n"
"T8KOEUt+zwvo/7V3LvSye0rgTBIlDHCNAymg4VMk7BPZ7hm/ELNKjD+Jo2FR3qyH\n"
"B5T0Y3HsLuJvW5iB4YlcNHlsdu87kGJ55tukmi8mxdAQ4Q7e2RCOFvu396j3x+UC\n"
"B5iPNgiV5+I3lg02dZ77DnKxHZu8A/lJBdiB3QW0KtZB6awBdpUKD9jf1b0SHzUv\n"
"KBds0pjBqAlkd25HN7rOrFleaJ1/ctaJxQZBKT5ZPt0m9STJEadao0xAH0ahmbWn\n"
"OlFuhjuefXKnEgV4We0+UXgVCwOPjdAvBbI+e0ocS3MFEvzG6uBQE3xDk3SzynTn\n"
"jh8BCNAw1FtxNrQHusEwMFxIt4I7mKZ9YIqioymCzLq9gwQbooMDQaHWBfEbwrbw\n"
"qHyGO0aoSCqI3Haadr8faqU9GY/rOPNk3sgrDQoo//fb4hVC1CLQJ13hef4Y53CI\n"
"rU7m2Ys6xt0nUW7/vGT1M0NPAgMBAAGjQjBAMA4GA1UdDwEB/wQEAwIBBjAPBgNV\n"
"HRMBAf8EBTADAQH/MB0GA1UdDgQWBBR5tFnme7bl5AFzgAiIyBpY9umbbjANBgkq\n"
"hkiG9w0BAQsFAAOCAgEAVR9YqbyyqFDQDLHYGmkgJykIrGF1XIpu+ILlaS/V9lZL\n"
"ubhzEFnTIZd+50xx+7LSYK05qAvqFyFWhfFQDlnrzuBZ6brJFe+GnY+EgPbk6ZGQ\n"
"3BebYhtF8GaV0nxvwuo77x/Py9auJ/GpsMiu/X1+mvoiBOv/2X/qkSsisRcOj/KK\n"
"NFtY2PwByVS5uCbMiogziUwthDyC3+6WVwW6LLv3xLfHTjuCvjHIInNzktHCgKQ5\n"
"ORAzI4JMPJ+GslWYHb4phowim57iaztXOoJwTdwJx4nLCgdNbOhdjsnvzqvHu7Ur\n"
"TkXWStAmzOVyyghqpZXjFaH3pO3JLF+l+/+sKAIuvtd7u+Nxe5AW0wdeRlN8NwdC\n"
"jNPElpzVmbUq4JUagEiuTDkHzsxHpFKVK7q4+63SM1N95R1NbdWhscdCb+ZAJzVc\n"
"oyi3B43njTOQ5yOf+1CceWxG1bQVs5ZufpsMljq4Ui0/1lvh+wjChP4kqKOJ2qxq\n"
"4RgqsahDYVvTH9w7jXbyLeiNdd8XM2w9U/t7y0Ff/9yi0GE44Za4rF2LN9d11TPA\n"
"mRGunUHBcnWEvgJBQl9nJEiU0Zsnvgc/ubhPgXRR4Xq37Z0j4r7g1SgEEzwxA57d\n"
"emyPxgcYxn/eR44/KJ4EBs+lVDR3veyJm+kXQ99b21/+jh5Xos1AnX5iItreGCc=\n"
"-----END CERTIFICATE-----\n"
// GTS Root R4
"-----BEGIN CERTIFICATE-----\n"
"MIICCTCCAY6gAwIBAgINAgPlwGjvYxqccpBQUjAKBggqhkjOPQQDAzBHMQswCQYD\n"
"VQQGEwJVUzEiMCAGA1UEChMZR29vZ2xlIFRydXN0IFNlcnZpY2VzIExMQzEUMBIG\n"
"A1UEAxMLR1RTIFJvb3QgUjQwHhcNMTYwNjIyMDAwMDAwWhcNMzYwNjIyMDAwMDAw\n"
"WjBHMQswCQYDVQQGEwJVUzEiMCAGA1UEChMZR29vZ2xlIFRydXN0IFNlcnZpY2Vz\n"
"IExMQzEUMBIGA1UEAxMLR1RTIFJvb3QgUjQwdjAQBgcqhkjOPQIBBgUrgQQAIgNi\n"
"AATzdHOnaItgrkO4NcWBMHtLSZ37wWHO5t5GvWvVYRg1rkDdc/eJkTBa6zzuhXyi\n"
"QHY7qca4R9gq55KRanPpsXI5nymfopjTX15YhmUPoYRlBtHci8nHc8iMai/lxKvR\n"
"HYqjQjBAMA4GA1UdDwEB/wQEAwIBhjAPBgNVHRMBAf8EBTADAQH/MB0GA1UdDgQW\n"
"BBSATNbrdP9JNqPV2Py1PsVq8JQdjDAKBggqhkjOPQQDAwNpADBmAjEA6ED/g94D\n"
"9J+uHXqnLrmvT/aDHQ4thQEd0dlq7A/Cr8deVl5c1RxYIigL9zC2L7F8AjEA8GE8\n"
"p/SgguMh1YQdc4acLa/KNJvxn7kjNuK8YAOdgLOaVsjh4rsUecrNIdSUtUlD\n"
"-----END CERTIFICATE-----\n"
;

static const uint32_t CLOCK_MIN_EPOCH   = 1735689600UL;  // 2025-01-01: a token needs real time
static const uint32_t TOKEN_LIFETIME_S  = 86400;         // OD13
static const uint32_t TOKEN_RENEW_EARLY = 300;
static const size_t   HEAP_MIN_FREE     = 110 * 1024;    // handshake peak ~65 KB (O0b)
static const size_t   HEAP_MIN_BLOCK    = 40 * 1024;     // two 16.5 KB mbedTLS record buffers
static const int      OUTBOX_MAX        = 8 * 1024;
static const unsigned long CONNECT_TIMEOUT_MS = 30000;

static bool ensureCaStore() {
  static bool loaded = false;
  if (!loaded) loaded = esp_tls_set_global_ca_store((const unsigned char*)UPLINK_CA_PEM,
                                                    sizeof(UPLINK_CA_PEM)) == ESP_OK;
  return loaded;
}

static size_t b64url(const uint8_t* in, size_t len, char* out, size_t cap) {
  static const char T[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
  size_t o = 0;
  for (size_t i = 0; i < len; i += 3) {
    uint32_t v = (uint32_t)in[i] << 16;
    if (i + 1 < len) v |= (uint32_t)in[i + 1] << 8;
    if (i + 2 < len) v |= in[i + 2];
    int chars = (i + 2 < len) ? 4 : (i + 1 < len ? 3 : 2);   // no padding
    for (int k = 0; k < chars && o + 1 < cap; k++) out[o++] = T[(v >> (18 - 6 * k)) & 0x3F];
  }
  out[o] = 0;
  return o;
}

static void isoTime(uint32_t t, char* iso, size_t n_iso, char* hms, char* dmy) {
  time_t tt = t;
  struct tm tm;
  gmtime_r(&tt, &tm);
  snprintf(iso, n_iso, "%04d-%02d-%02dT%02d:%02d:%02d.000000+00:00", tm.tm_year + 1900,
           tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);
  if (hms) snprintf(hms, 9, "%02d:%02d:%02d", tm.tm_hour, tm.tm_min, tm.tm_sec);
  if (dmy) snprintf(dmy, 11, "%02d/%02d/%04d", tm.tm_mday, tm.tm_mon + 1, tm.tm_year + 1900);
}

bool NetUplink::isTls(const UplinkConfig& c) const {
  return !strncmp(c.host, "mqtts://", 8) || !strncmp(c.host, "wss://", 6);
}

const char* NetUplink::stateName() const {
  static const char* const N[] = {"off", "wait wifi", "wait clock", "wait heap", "wait turn",
                                  "retry", "connecting", "conn", "paused"};
  return N[_state];
}

void NetUplink::diag(char* out, size_t n) const {
  long next = _client ? 0 : (long)(_next_try_ms - millis()) / 1000;
  if (next < 0) next = 0;
  unsigned long ago = _last_err_ms ? (millis() - _last_err_ms) / 1000 : 0;
  snprintf(out, n, "obs%d %s pub:%u drop:%u err:%s %lus ago tries:%u next:%lds%s",
           _idx + 1, stateName(), (unsigned)_published, (unsigned)_dropped, _last_err, ago,
           (unsigned)_attempt, next, _token_exp ? " token" : "");
}

void NetUplink::onEvent(void* arg, esp_event_base_t, int32_t id, void* data) {
  NetUplink* self = (NetUplink*)arg;
  esp_mqtt_event_handle_t ev = (esp_mqtt_event_handle_t)data;
  switch ((esp_mqtt_event_id_t)id) {
    case MQTT_EVENT_CONNECTED:
      self->_ev_connected = true;
      break;
    case MQTT_EVENT_ERROR:
      if (ev && ev->error_handle) {
        self->_ev_err_type = ev->error_handle->error_type;
        self->_ev_tls_err  = ev->error_handle->esp_tls_last_esp_err;
        self->_ev_connack  = ev->error_handle->connect_return_code;
      }
      self->_ev_closed = true;
      break;
    case MQTT_EVENT_DISCONNECTED:
      self->_ev_closed = true;
      break;
    default:
      break;
  }
}

void NetUplink::buildTopic(const UplinkCtx& ctx, const char* type, char* out, size_t n) const {
  const UplinkConfig& c = ctx.cfg->obs[_idx];
  const char* tpl = c.topic[0] ? c.topic : OBS_UPLINK_TOPIC_DEFAULT;
  size_t o = 0;
  for (const char* p = tpl; *p && o + 1 < n;) {
    const char* rep = nullptr;
    size_t skip = 0;
    if      (!strncmp(p, "{iata}", 6)) { rep = ctx.cfg->obs_iata; skip = 6; }
    else if (!strncmp(p, "{key}", 5))  { rep = ctx.pub_hex;       skip = 5; }
    else if (!strncmp(p, "{type}", 6)) { rep = type;              skip = 6; }
    if (rep) {
      o += strlcpy(out + o, rep, n - o);
      if (o >= n) o = n - 1;
      p += skip;
    } else {
      out[o++] = *p++;
    }
  }
  out[o] = 0;
}

// header.payload.SIGHEX, as meshcoretomqtt auth_token.py (verified against LetsMesh, O0b).
bool NetUplink::buildToken(const UplinkCtx& ctx, uint32_t now) {
  const UplinkConfig& c = ctx.cfg->obs[_idx];
  static const char HDR[] = "{\"alg\":\"Ed25519\",\"typ\":\"JWT\"}";
  char pay[320];
  int pl = snprintf(pay, sizeof(pay), "{\"publicKey\":\"%s\",\"aud\":\"%s\",\"iat\":%u,\"exp\":%u",
                    ctx.pub_hex, c.aud, (unsigned)now, (unsigned)(now + TOKEN_LIFETIME_S));
  if (ctx.cfg->obs_owner[0] && pl < (int)sizeof(pay))
    pl += snprintf(pay + pl, sizeof(pay) - pl, ",\"owner\":\"%s\"", ctx.cfg->obs_owner);
  if (ctx.cfg->obs_email[0] && pl < (int)sizeof(pay))
    pl += snprintf(pay + pl, sizeof(pay) - pl, ",\"email\":\"%s\"", ctx.cfg->obs_email);
  if (pl >= (int)sizeof(pay) - 1) return false;
  pay[pl++] = '}';
  pay[pl] = 0;

  size_t o = b64url((const uint8_t*)HDR, strlen(HDR), _token, sizeof(_token));
  _token[o++] = '.';
  o += b64url((const uint8_t*)pay, pl, _token + o, sizeof(_token) - o);
  if (o + 1 + 128 + 1 > sizeof(_token)) return false;
  uint8_t sig[64];
  ctx.sign(sig, (const uint8_t*)_token, (int)o);
  _token[o++] = '.';
  for (int i = 0; i < 64; i++) o += snprintf(_token + o, sizeof(_token) - o, "%02X", sig[i]);
  _token_exp = now + TOKEN_LIFETIME_S;
  return true;
}

bool NetUplink::start(const UplinkCtx& ctx) {
  const UplinkConfig& c = ctx.cfg->obs[_idx];
  bool tls = isTls(c);
  if (tls && !ensureCaStore()) return false;

  if (strstr(c.host, "://")) strlcpy(_uri, c.host, sizeof(_uri));
  else snprintf(_uri, sizeof(_uri), "mqtt://%s:%d", c.host, c.port > 0 ? c.port : 1883);
  snprintf(_client_id, sizeof(_client_id), "mn_%.16s_%d", ctx.pub_hex, _idx + 1);
  buildTopic(ctx, c.fmt_rx ? "rx" : "packets", _topic_pkt, sizeof(_topic_pkt));
  buildTopic(ctx, "status", _topic_status, sizeof(_topic_status));
  snprintf(_lwt, sizeof(_lwt), "{\"status\":\"offline\",\"origin\":\"%s\",\"origin_id\":\"%s\"}",
           ctx.origin, ctx.pub_hex);

  esp_mqtt_client_config_t cfg = {};
  cfg.uri = _uri;
  cfg.client_id = _client_id;
  _token_exp = 0;
  if (c.aud[0]) {
    if (!buildToken(ctx, (uint32_t)time(nullptr))) return false;
    snprintf(_user, sizeof(_user), "v1_%s", ctx.pub_hex);
    cfg.username = _user;
    cfg.password = _token;
  } else {
    cfg.username = c.user[0] ? c.user : nullptr;
    cfg.password = c.pwd[0] ? c.pwd : nullptr;
  }
  cfg.use_global_ca_store = tls;
  cfg.keepalive = 30;
  cfg.buffer_size = 1024;
  cfg.out_buffer_size = 1280;
  cfg.network_timeout_ms = 10000;
  cfg.disable_auto_reconnect = true;
  // As NetOta httpd: idle priority, so IDLE0 keeps running through the ~7 s TLS handshake and
  // the task watchdog (IDLE0, 5 s, panic) does not reset the board (O0b).
  cfg.task_prio = tskIDLE_PRIORITY;
  cfg.lwt_topic = _topic_status;
  cfg.lwt_msg = _lwt;
  cfg.lwt_qos = 1;
  cfg.lwt_retain = 1;

  _ev_connected = _ev_closed = false;
  _ev_err_type = _ev_tls_err = _ev_connack = 0;
  _client = esp_mqtt_client_init(&cfg);
  if (!_client) return false;
  esp_mqtt_client_register_event(_client, (esp_mqtt_event_id_t)ESP_EVENT_ANY_ID, onEvent, this);
  if (esp_mqtt_client_start(_client) != ESP_OK) {
    esp_mqtt_client_destroy(_client);
    _client = nullptr;
    return false;
  }
  return true;
}

void NetUplink::stop(bool announce) {
  if (_client) {
    // A clean disconnect sends no LWT: say offline ourselves.
    if (announce && _state == CONNECTED) esp_mqtt_client_publish(_client, _topic_status, _lwt, 0, 1, 1);
    esp_mqtt_client_stop(_client);
    esp_mqtt_client_destroy(_client);
    _client = nullptr;
  }
  _token_exp = 0;
  _status_sent = false;
  if (_state == CONNECTING || _state == CONNECTED) _state = BACKOFF;
}

void NetUplink::fail(const char* why, unsigned long delay_ms) {
  strlcpy(_last_err, why, sizeof(_last_err));
  _last_err_ms = millis();
  _next_try_ms = millis() + delay_ms;
  _state = BACKOFF;
}

// Not enough heap for a handshake (operator 2026-11-02): 10 x 30 s, 10 x 5 min, 10 x 10 min,
// then every 30 min, no limit while the uplink is on. Each try only reads the heap counters.
void NetUplink::deferHeap() {
  unsigned long d = _attempt < 10 ? 30000UL : _attempt < 20 ? 300000UL
                  : _attempt < 30 ? 600000UL : 1800000UL;
  if (_attempt < 0xFFFF) _attempt++;
  strlcpy(_last_err, "heap", sizeof(_last_err));
  _last_err_ms = millis();
  _next_try_ms = millis() + d;
  _state = WAIT_HEAP;
}

void NetUplink::publishStatus(const UplinkCtx& ctx, bool online) {
  char iso[40];
  isoTime((uint32_t)time(nullptr), iso, sizeof(iso), nullptr, nullptr);
  char p[512];
  int len = snprintf(p, sizeof(p),
    "{\"status\":\"%s\",\"timestamp\":\"%s\",\"origin\":\"%s\",\"origin_id\":\"%s\","
    "\"model\":\"%s\",\"firmware_version\":\"%s\",\"client_version\":\"meshnomad/%s\","
    "\"stats\":{\"battery_mv\":%u,\"uptime_secs\":%u,\"packets_sent\":%u,\"packets_received\":%u,"
    "\"noise_floor\":%d,\"internal_heap\":%u}}",
    online ? "online" : "offline", iso, ctx.origin, ctx.pub_hex, ctx.model, ctx.fw, ctx.fw,
    (unsigned)ctx.battery_mv, (unsigned)ctx.uptime_s, (unsigned)ctx.tx_count, (unsigned)ctx.rx_count,
    ctx.noise_floor, (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
  if (len > 0 && len < (int)sizeof(p)) esp_mqtt_client_enqueue(_client, _topic_status, p, len, 1, 1, true);
}

int NetUplink::formatPacket(const UplinkCtx& ctx, const ObsPacket& p, char* out, size_t n) const {
  const UplinkConfig& c = ctx.cfg->obs[_idx];
  int len;
  if (c.fmt_rx) {                    // legacy short JSON, as the local observer
    len = snprintf(out, n, "{\"origin\":\"%s\",\"raw\":\"", ctx.origin);
    for (int i = 0; i < p.len && len < (int)n - 40; i++) len += snprintf(out + len, n - len, "%02x", p.raw[i]);
    len += snprintf(out + len, n - len, "\",\"SNR\":%.2f,\"RSSI\":%.1f}", p.snr, p.rssi);
    return len;
  }

  mesh::Packet pkt;
  bool parsed = pkt.readFrom(p.raw, p.len);
  len = snprintf(out, n, "{\"origin\":\"%s\",\"origin_id\":\"%s\"", ctx.origin, ctx.pub_hex);
  if (p.t >= CLOCK_MIN_EPOCH) {
    char iso[40], hms[9], dmy[11];
    isoTime(p.t, iso, sizeof(iso), hms, dmy);
    len += snprintf(out + len, n - len, ",\"timestamp\":\"%s\",\"time\":\"%s\",\"date\":\"%s\"", iso, hms, dmy);
  }
  len += snprintf(out + len, n - len, ",\"type\":\"PACKET\",\"direction\":\"rx\",\"len\":\"%u\"", p.len);
  if (parsed) {
    static const char ROUTE[] = {'T', 'F', 'D', 'U'};   // PH_ROUTE_MASK values 0..3
    uint8_t h[MAX_HASH_SIZE];
    pkt.calculatePacketHash(h);
    len += snprintf(out + len, n - len, ",\"packet_type\":\"%u\",\"route\":\"%c\",\"payload_len\":\"%u\",\"hash\":\"",
                    (unsigned)pkt.getPayloadType(), ROUTE[pkt.getRouteType()], (unsigned)pkt.payload_len);
    for (int i = 0; i < MAX_HASH_SIZE; i++) len += snprintf(out + len, n - len, "%02X", h[i]);
    len += snprintf(out + len, n - len, "\"");
    if (pkt.isRouteDirect() && pkt.getPathHashCount() > 0) {
      len += snprintf(out + len, n - len, ",\"path\":[");
      int sz = pkt.getPathHashSize();
      for (int hop = 0; hop < pkt.getPathHashCount(); hop++) {
        len += snprintf(out + len, n - len, hop ? ",\"" : "\"");
        for (int b = 0; b < sz; b++) len += snprintf(out + len, n - len, "%02x", pkt.path[hop * sz + b]);
        len += snprintf(out + len, n - len, "\"");
      }
      len += snprintf(out + len, n - len, "]");
    }
  }
  len += snprintf(out + len, n - len, ",\"raw\":\"");
  for (int i = 0; i < p.len && len < (int)n - 48; i++) len += snprintf(out + len, n - len, "%02X", p.raw[i]);
  len += snprintf(out + len, n - len, "\",\"SNR\":\"%.2f\",\"RSSI\":\"%.0f\"}", p.snr, p.rssi);
  return len;
}

// At most 2 packets per loop() pass, as the local observer, so a burst never stalls routing.
void NetUplink::drain(const UplinkCtx& ctx) {
  if ((int32_t)(ctx.ring_oldest - _cursor) > 0) {   // fell behind the ring: lost the oldest
    _dropped += ctx.ring_oldest - _cursor;
    _cursor = ctx.ring_oldest;
  }
  static char payload[1100];
  for (int k = 0; k < 2 && _cursor != ctx.ring_next; k++) {
    if (esp_mqtt_client_get_outbox_size(_client) > OUTBOX_MAX) break;   // broker not taking it
    const ObsPacket& p = ctx.ring[_cursor % OBS_RING_SLOTS];
    int len = formatPacket(ctx, p, payload, sizeof(payload));
    if (len > 0 && len < (int)sizeof(payload) &&
        esp_mqtt_client_enqueue(_client, _topic_pkt, payload, len, 0, 0, true) >= 0) _published++;
    else _dropped++;
    _cursor++;
  }
}

void NetUplink::loop(const UplinkCtx& ctx, bool wifi_up) {
  const UplinkConfig& c = ctx.cfg->obs[_idx];
  bool owner = *ctx.handshake_owner == _idx;

  if (_ev_connected) {
    _ev_connected = false;
    if (_state == CONNECTING) {
      _state = CONNECTED;
      _attempt = 0;
      strlcpy(_last_err, "-", sizeof(_last_err));
    }
  }
  if (_ev_closed && _client) {
    _ev_closed = false;
    bool auth = _ev_connack == 4 || _ev_connack == 5;     // bad credentials / not authorized
    const char* why = auth ? "auth" : _ev_tls_err ? "tls" : _state == CONNECTED ? "closed" : "connect";
    stop();
    unsigned long d = auth ? 300000UL : 5000UL << (_attempt < 6 ? _attempt : 6);
    if (d > 300000UL) d = 300000UL;
    if (_attempt < 0xFFFF) _attempt++;
    fail(why, d);
  }
  if (owner && _state != CONNECTING) *ctx.handshake_owner = -1;

  if (!c.on || !c.host[0]) {
    if (_client) stop(true);
    _state = OFF;
    _attempt = 0;
    _next_try_ms = 0;
    _cursor = ctx.ring_next;          // nothing old waits for a later switch-on
    return;
  }
  if (!wifi_up) {
    if (_client) stop();
    _state = WAIT_WIFI;
    _next_try_ms = 0;
    return;
  }
  bool tls = isTls(c);
  if (tls && ctx.ota_active) {        // net ota needs the heap for its own TLS session
    if (_client) stop();
    _state = PAUSED;
    _next_try_ms = 0;
    return;
  }

  if (_client) {
    if (_state == CONNECTING) {
      if (millis() - _connect_started_ms > CONNECT_TIMEOUT_MS) {
        stop();
        fail("timeout", 30000);
      }
      return;
    }
    if (_token_exp && (uint32_t)time(nullptr) + TOKEN_RENEW_EARLY > _token_exp) {
      stop();                         // renew = new connection with a fresh token
      _state = WAIT_TURN;
      _next_try_ms = 0;
      return;
    }
    // First status waits one pass for real stats: telemetry is sampled before this loop runs.
    if ((!_status_sent || ctx.status_due) && ctx.has_telem) {
      publishStatus(ctx, true);
      _status_sent = true;
    }
    drain(ctx);
    return;
  }

  if ((long)(millis() - _next_try_ms) < 0) return;
  if (c.aud[0] && (uint32_t)time(nullptr) < CLOCK_MIN_EPOCH) {
    _state = WAIT_CLOCK;
    _next_try_ms = millis() + 5000;
    return;
  }
  if (*ctx.handshake_owner >= 0) {    // one handshake at a time
    _state = WAIT_TURN;
    return;
  }
  if (tls && (heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT) < HEAP_MIN_FREE ||
              heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT) < HEAP_MIN_BLOCK)) {
    deferHeap();
    return;
  }
  if (!start(ctx)) {
    fail("init", 30000);
    return;
  }
  *ctx.handshake_owner = _idx;
  _state = CONNECTING;
  _connect_started_ms = millis();
}

#endif
