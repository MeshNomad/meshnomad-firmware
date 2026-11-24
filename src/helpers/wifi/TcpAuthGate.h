#pragma once
// MeshNomad: login for the companion's TCP (WiFi) link, protocol v1 (firmware B89).
// Payloads inside the usual frames ('<' app -> radio, '>' radio -> app):
//   app -> radio  F0 01 01                HELLO, protocol version 1
//   radio -> app  F0 01 01 <nonce 16>     CHALLENGE (reply to HELLO and to any other frame
//                                         before the login; that frame is dropped)
//   app -> radio  F0 02 <hmac 32>         RESPONSE = HMAC-SHA256(password, "MNTCP1" || nonce)
//   radio -> app  F0 03 <status>          RESULT: 00 OK, 01 wrong password, 02 IP locked out,
//                                         03 no password set, 04 login timeout; any status but
//                                         00 closes the connection
// With tcp.auth 0 the link is open as in MeshCore and HELLO gets RESULT 00.
// Pure logic (no Arduino): the interface feeds it frames and the clock and carries out
// the returned action. One login attempt per connection; a timeout is not a failed login.

#include <stddef.h>
#include <stdint.h>
#include <string.h>

// Companion builds that carry the gate (MeshNomad, ESP32, WiFi interface).
#if defined(MESHNOMAD_BUILD) && defined(ESP32) && defined(ENABLE_WIFI_INTERFACE)
  #define MN_TCP_AUTH 1
#endif

#define TCP_AUTH_FRAME       0xF0
#define TCP_AUTH_HELLO       0x01
#define TCP_AUTH_RESPONSE    0x02
#define TCP_AUTH_RESULT      0x03
#define TCP_AUTH_VERSION     0x01

#define TCP_AUTH_OK          0x00
#define TCP_AUTH_WRONG       0x01
#define TCP_AUTH_LOCKED      0x02
#define TCP_AUTH_NO_PASSWORD 0x03
#define TCP_AUTH_TIMEOUT     0x04

#define TCP_AUTH_NONCE_LEN   16
#define TCP_AUTH_HMAC_LEN    32
#define TCP_AUTH_DEADLINE_MS 15000UL

#define TCP_PASSWORD_MIN     8
#define TCP_PASSWORD_MAX     63

// HMAC-SHA256(key, msg) -> out[32]; the board passes the rweather Crypto one.
typedef void (*TcpAuthHmacFn)(const uint8_t* key, size_t key_len, const uint8_t* msg,
                              size_t msg_len, uint8_t out[TCP_AUTH_HMAC_LEN]);

// D4: 8-63 printable ASCII characters (0x20-0x7E).
inline bool tcpPasswordValid(const char* pw) {
  size_t n = 0;
  for (; pw[n]; n++) {
    if ((uint8_t)pw[n] < 0x20 || (uint8_t)pw[n] > 0x7E) return false;
    if (n >= TCP_PASSWORD_MAX) return false;
  }
  return n >= TCP_PASSWORD_MIN;
}

// "MNTCP1" || nonce; returns the length (22).
inline size_t tcpAuthMessage(const uint8_t nonce[TCP_AUTH_NONCE_LEN], uint8_t* out) {
  memcpy(out, "MNTCP1", 6);
  memcpy(out + 6, nonce, TCP_AUTH_NONCE_LEN);
  return 6 + TCP_AUTH_NONCE_LEN;
}

// A frame is a HELLO (any later version too).
inline bool tcpAuthIsHello(const uint8_t* f, size_t len) {
  return len >= 2 && f[0] == TCP_AUTH_FRAME && f[1] == TCP_AUTH_HELLO;
}

inline size_t tcpAuthResultFrame(uint8_t status, uint8_t* out) {
  out[0] = TCP_AUTH_FRAME; out[1] = TCP_AUTH_RESULT; out[2] = status;
  return 3;
}

struct TcpAuthAction {
  enum Kind : uint8_t {
    NONE,        // nothing to do
    SEND,        // send `out`, keep waiting
    SEND_CLOSE,  // send `out`, then close the connection
    ACCEPT,      // send `out` (RESULT 00), the connection becomes the session
  };
  Kind     kind = NONE;
  bool     failed_login = false;   // count one failure for this IP
  bool     login_ok = false;       // forget the failures of this IP
  uint8_t  out[3 + TCP_AUTH_NONCE_LEN];
  size_t   out_len = 0;
};

// One pending connection. Not started = no connection waiting.
class TcpAuthGate {
  uint8_t  _nonce[TCP_AUTH_NONCE_LEN];
  uint32_t _started_ms = 0;
  bool     _active = false;
  bool     _challenged = false;

  static TcpAuthAction result(TcpAuthAction::Kind kind, uint8_t status) {
    TcpAuthAction a;
    a.kind = kind;
    a.out_len = tcpAuthResultFrame(status, a.out);
    return a;
  }

  TcpAuthAction challenge() {
    _challenged = true;
    TcpAuthAction a;
    a.kind = TcpAuthAction::SEND;
    a.out[0] = TCP_AUTH_FRAME; a.out[1] = TCP_AUTH_HELLO; a.out[2] = TCP_AUTH_VERSION;
    memcpy(&a.out[3], _nonce, TCP_AUTH_NONCE_LEN);
    a.out_len = 3 + TCP_AUTH_NONCE_LEN;
    return a;
  }

public:
  void start(const uint8_t nonce[TCP_AUTH_NONCE_LEN], uint32_t now) {
    memcpy(_nonce, nonce, TCP_AUTH_NONCE_LEN);
    _started_ms = now;
    _active = true;
    _challenged = false;
  }
  void stop() {
    _active = false;
    memset(_nonce, 0, sizeof(_nonce));
  }
  bool active() const { return _active; }

  // A frame from the waiting connection. `locked` = its IP is locked out now;
  // `password` = the node's TCP password ("" = not set).
  TcpAuthAction onFrame(const uint8_t* f, size_t len, bool locked, const char* password,
                        TcpAuthHmacFn hmac) {
    if (!_active) return TcpAuthAction();
    if (locked) { stop(); return result(TcpAuthAction::SEND_CLOSE, TCP_AUTH_LOCKED); }
    if (!password || !password[0]) {   // gate: not a failed login
      stop();
      return result(TcpAuthAction::SEND_CLOSE, TCP_AUTH_NO_PASSWORD);
    }
    if (!_challenged || len < 2 || f[0] != TCP_AUTH_FRAME || f[1] != TCP_AUTH_RESPONSE) {
      return challenge();   // HELLO or any other frame: (again) the same nonce
    }
    uint8_t want[TCP_AUTH_HMAC_LEN];
    uint8_t msg[6 + TCP_AUTH_NONCE_LEN];
    size_t mlen = tcpAuthMessage(_nonce, msg);
    hmac((const uint8_t*)password, strlen(password), msg, mlen, want);
    uint8_t diff = (len == 2 + TCP_AUTH_HMAC_LEN) ? 0 : 1;
    for (size_t i = 0; i < TCP_AUTH_HMAC_LEN; i++) {   // constant time
      diff |= want[i] ^ (2 + i < len ? f[2 + i] : 0);
    }
    memset(want, 0, sizeof(want));
    stop();
    if (diff == 0) {
      TcpAuthAction a = result(TcpAuthAction::ACCEPT, TCP_AUTH_OK);
      a.login_ok = true;
      return a;
    }
    TcpAuthAction a = result(TcpAuthAction::SEND_CLOSE, TCP_AUTH_WRONG);
    a.failed_login = true;
    return a;
  }

  // Called every pass; past the deadline the connection gets RESULT 04 and is closed
  // (D5: not counted as a failed login).
  TcpAuthAction onTick(uint32_t now) {
    if (!_active || now - _started_ms < TCP_AUTH_DEADLINE_MS) return TcpAuthAction();
    stop();
    return result(TcpAuthAction::SEND_CLOSE, TCP_AUTH_TIMEOUT);
  }
};
