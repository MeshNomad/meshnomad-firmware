#pragma once
// MeshNomad: one observer uplink (obs1..obs3) to an external broker (O1,
// docs/superpowers/reports/2026-11-02-o1-observer-uplinks-design.md). esp-mqtt client
// (mqtt/mqtts/ws/wss), optional Ed25519 token, /packets with the meshcoretomqtt JSON or
// the legacy /rx. Publish only: nothing from the broker reaches the mesh.

#if defined(ESP32) && defined(WITH_NET_SERVICES)

#include <Arduino.h>
#include "NetConfig.h"

typedef void (*UplinkSignFn)(uint8_t sig[64], const uint8_t* msg, int len);

// One received packet, shared by all consumers of the observer ring (local broker + uplinks).
struct ObsPacket {
  uint8_t  len;
  float    snr;
  float    rssi;
  uint32_t t;                   // epoch at reception; 0 while the clock is not set
  uint8_t  raw[255];
};

static const int OBS_RING_SLOTS = 16;   // 16 x 268 B ≈ 4.3 KB

#endif

#if defined(ESP32) && defined(WITH_NET_SERVICES) && !defined(NET_WIFI_EXTERNAL)

#include <mqtt_client.h>

// What NetServices lends every uplink on each loop() pass.
struct UplinkCtx {
  const NetConfig* cfg;
  const ObsPacket* ring;        // OBS_RING_SLOTS entries, slot = seq % OBS_RING_SLOTS
  uint32_t ring_oldest;         // oldest sequence number still in the ring
  uint32_t ring_next;           // next sequence number to be written
  const char* pub_hex;          // 64 hex chars, uppercase
  const char* origin;           // node name, JSON-safe
  const char* model;
  const char* fw;
  UplinkSignFn sign;
  bool ota_active;              // net ota session holds the heap: TLS uplinks pause
  int* handshake_owner;         // index of the uplink in a handshake, -1 = none (one at a time)
  uint32_t battery_mv, uptime_s, rx_count, tx_count;
  int noise_floor;
  bool status_due;              // status interval elapsed this pass
  bool has_telem;               // battery/uptime/counters above were filled this pass
};

class NetUplink {
public:
  enum State : uint8_t { OFF, WAIT_WIFI, WAIT_CLOCK, WAIT_HEAP, WAIT_TURN, BACKOFF, CONNECTING, CONNECTED, PAUSED };

  void init(int index) { _idx = index; }
  void loop(const UplinkCtx& ctx, bool wifi_up);
  // Tears the client down. announce: publish "offline" first (user switched it off or changed
  // it); never on WiFi loss or token renewal, where a publish could block loop().
  void stop(bool announce = false);
  void restart() { stop(true); _attempt = 0; _next_try_ms = 0; }   // settings changed
  bool needsStatus() const { return _state == CONNECTED && !_status_sent; }

  bool isTls(const UplinkConfig& c) const;
  State state() const { return _state; }
  const char* stateName() const;
  void diag(char* out, size_t n) const;
  uint32_t published() const { return _published; }
  uint32_t dropped() const { return _dropped; }

private:
  int   _idx = 0;
  State _state = OFF;
  esp_mqtt_client_handle_t _client = nullptr;
  uint32_t _cursor = 0;              // next ring sequence to publish
  uint32_t _published = 0, _dropped = 0;
  uint16_t _attempt = 0;             // consecutive failed or deferred attempts
  unsigned long _next_try_ms = 0;
  unsigned long _connect_started_ms = 0;
  uint32_t _token_exp = 0;           // epoch; 0 = no token
  bool _status_sent = false;

  // Set by the esp-mqtt task, read in loop().
  volatile bool _ev_connected = false, _ev_closed = false;
  volatile int  _ev_err_type = 0, _ev_tls_err = 0, _ev_connack = 0;

  // Last failure for 'get obsN.diag'.
  char _last_err[24] = "-";
  unsigned long _last_err_ms = 0;

  char _uri[112], _user[72], _client_id[24];
  char _token[768];                  // with owner + email claims ~620 chars
  char _topic_pkt[112], _topic_status[112];
  char _lwt[160];

  static void onEvent(void* arg, esp_event_base_t base, int32_t id, void* data);
  bool start(const UplinkCtx& ctx);
  bool buildToken(const UplinkCtx& ctx, uint32_t now);
  void buildTopic(const UplinkCtx& ctx, const char* type, char* out, size_t n) const;
  void publishStatus(const UplinkCtx& ctx, bool online);
  void drain(const UplinkCtx& ctx);
  int  formatPacket(const UplinkCtx& ctx, const ObsPacket& p, char* out, size_t n) const;
  void fail(const char* why, unsigned long delay_ms);
  void deferHeap();
};

#endif
