#pragma once
// MeshNomad: login for the companion's TCP (WiFi) link, protocol v2 (firmware B89, B91).
// Payloads inside the usual frames ('<' app -> radio, '>' radio -> app):
//   app -> radio  F0 01 02 || A                 HELLO v2, A = app's one-time X25519 key
//   radio -> app  F0 01 02 || R || N || S       CHALLENGE: R = radio's one-time X25519 key,
//                                               N = node key (Ed25519, mesh identity),
//                                               S = Ed25519(N, "MNTCP2-sig" || A || R)
//   from here on every frame in both directions is encrypted (TcpSecureChannel.h)
//   app -> radio  [F0 02 || HMAC]               LOGIN, HMAC = HMAC-SHA256(password,
//                                               "MNTCP2-login" || th)
//   radio -> app  [F0 03 || status]             RESULT 00 OK, 01 wrong password, 04 timeout;
//                                               anything but 00 closes the connection
// Refused before any crypto, as a plain RESULT followed by the close: 02 IP locked out,
// 03 no password set on the node, 05 not a v2 HELLO (protocol v1 or stock app), 04 timeout.
// With tcp.auth 0 the link is open and plain as in MeshCore (SerialWifiAuthInterface).
// Pure logic (no Arduino): the interface feeds it frames, the clock, randomness and the
// node's signature, and carries out the returned action. One login attempt per connection;
// a timeout is not a failed login.

#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include "TcpAuthConfig.h"
#include "TcpSecureChannel.h"

#define TCP_V2_HELLO_LEN     (3 + 32)
#define TCP_V2_CHALLENGE_LEN (3 + 32 + 32 + TCP_V2_SIG_LEN)   // 131
#define TCP_V2_LOGIN_LEN     (2 + TCP_V2_MAC_LEN)

typedef void (*TcpRandomFn)(uint8_t* out, size_t len);
// Ed25519 signature with the node's identity key.
typedef void (*TcpSignFn)(uint8_t sig[TCP_V2_SIG_LEN], const uint8_t* msg, size_t len);

struct TcpAuthAction {
  enum Kind : uint8_t {
    NONE,        // nothing to do
    SEND,        // send `out` as it is (CHALLENGE), keep waiting
    SEND_CLOSE,  // send `out` as it is, then close the connection
    CLOSE,       // close at once, nothing to send
    ACCEPT,      // send `out` (encrypted RESULT 00); the connection becomes the session,
                 // take its keys with TcpAuthGate::takeChannel()
  };
  Kind     kind = NONE;
  bool     failed_login = false;   // count one failure for this IP
  bool     login_ok = false;       // forget the failures of this IP
  uint8_t  out[TCP_V2_CHALLENGE_LEN];
  size_t   out_len = 0;
};

// One waiting connection. Not started = no connection waiting.
class TcpAuthGate {
  enum State : uint8_t { IDLE, WAIT_HELLO, WAIT_LOGIN };

  State        _state = IDLE;
  uint32_t     _started_ms = 0;
  bool         _hello_waiting = false;   // HELLO arrived inside the handshake gap
  uint8_t      _app_key[32];
  uint8_t      _th[TCP_V2_TH_LEN];
  TcpV2Channel _ch;
  uint32_t     _last_hs_ms = 0;          // kept across connections
  bool         _any_hs = false;

  const uint8_t* _node_pub = nullptr;
  TcpSignFn      _sign = nullptr;
  TcpRandomFn    _random = nullptr;

  static TcpAuthAction plain(TcpAuthAction::Kind kind, uint8_t status) {
    TcpAuthAction a;
    a.kind = kind;
    a.out_len = tcpAuthResultFrame(status, a.out);
    return a;
  }

  // Encrypted RESULT; CLOSE when the counter cannot seal (never in practice).
  TcpAuthAction sealed(TcpAuthAction::Kind kind, uint8_t status) {
    TcpAuthAction a;
    uint8_t r[3];
    size_t n = tcpAuthResultFrame(status, r);
    if (!_ch.tx.seal(r, n, a.out)) { a.kind = TcpAuthAction::CLOSE; return a; }
    a.kind = kind;
    a.out_len = n + TCP_V2_TAG_LEN;
    return a;
  }

  bool hsAllowed(uint32_t now) const {
    return !_any_hs || now - _last_hs_ms >= TCP_AUTH_HS_GAP_MS;
  }

  // X25519 + signature; SEND with the CHALLENGE, or CLOSE when the app key is weak.
  TcpAuthAction handshake(uint32_t now) {
    _hello_waiting = false;
    _last_hs_ms = now;
    _any_hs = true;
    uint8_t priv[32], r[32], secret[32];
    _random(priv, sizeof(priv));
    tcpV2KeyPair(priv, r);
    bool ok = tcpV2Shared(_app_key, priv, secret);   // wipes priv
    if (!ok) {
      tcpV2Wipe(secret, sizeof(secret));
      stop();
      TcpAuthAction a;
      a.kind = TcpAuthAction::CLOSE;
      return a;
    }
    tcpV2Transcript(_app_key, r, _node_pub, _th);
    _ch.derive(secret, _th, true);                   // wipes secret

    TcpAuthAction a;
    a.kind = TcpAuthAction::SEND;
    a.out[0] = TCP_AUTH_FRAME; a.out[1] = TCP_AUTH_HELLO; a.out[2] = TCP_AUTH_VERSION;
    memcpy(&a.out[3], r, 32);
    memcpy(&a.out[35], _node_pub, 32);
    uint8_t msg[74];
    size_t mlen = tcpV2SigMessage(_app_key, r, msg);
    _sign(&a.out[67], msg, mlen);
    a.out_len = TCP_V2_CHALLENGE_LEN;
    _state = WAIT_LOGIN;
    return a;
  }

public:
  void setIdentity(const uint8_t* node_pub, TcpSignFn sign, TcpRandomFn random) {
    _node_pub = node_pub; _sign = sign; _random = random;
  }

  void start(uint32_t now) {
    stop();
    _started_ms = now;
    _state = WAIT_HELLO;
  }
  void stop() {
    _state = IDLE;
    _hello_waiting = false;
    tcpV2Wipe(_app_key, sizeof(_app_key));
    tcpV2Wipe(_th, sizeof(_th));
    _ch.clear();
  }
  bool active() const { return _state != IDLE; }

  // After ACCEPT: the session keys (counters already past LOGIN and RESULT 00).
  void takeChannel(TcpV2Channel& dst) { _ch.moveTo(dst); stop(); }

  // A frame from the waiting connection (as received: plain before the CHALLENGE, encrypted
  // after it). `locked` = its IP is locked out now; `password` = "" when not set.
  TcpAuthAction onFrame(const uint8_t* f, size_t len, bool locked, const char* password,
                        uint32_t now) {
    if (_state == IDLE) return TcpAuthAction();
    if (_state == WAIT_HELLO) {
      if (_hello_waiting) return TcpAuthAction();   // more input while waiting: ignore
      if (locked) { stop(); return plain(TcpAuthAction::SEND_CLOSE, TCP_AUTH_LOCKED); }
      if (!password || !password[0]) {            // gate: not a failed login
        stop();
        return plain(TcpAuthAction::SEND_CLOSE, TCP_AUTH_NO_PASSWORD);
      }
      if (len != TCP_V2_HELLO_LEN || f[0] != TCP_AUTH_FRAME || f[1] != TCP_AUTH_HELLO ||
          f[2] != TCP_AUTH_VERSION || !_node_pub || !_sign || !_random) {
        stop();
        return plain(TcpAuthAction::SEND_CLOSE, TCP_AUTH_UNSUPPORTED);
      }
      memcpy(_app_key, &f[3], 32);
      if (!hsAllowed(now)) { _hello_waiting = true; return TcpAuthAction(); }
      return handshake(now);
    }

    // WAIT_LOGIN: the frame must open with k_a2r
    uint8_t p[TCP_V2_LOGIN_LEN + 8];
    size_t plen = 0;
    if (len > sizeof(p) + TCP_V2_TAG_LEN || !_ch.rx.open(f, len, p, &plen)) {
      stop();                                      // forged or garbled: counts like a guess
      TcpAuthAction a;
      a.kind = TcpAuthAction::CLOSE;
      a.failed_login = true;
      return a;
    }
    if (plen != TCP_V2_LOGIN_LEN || p[0] != TCP_AUTH_FRAME || p[1] != TCP_AUTH_LOGIN) {
      tcpV2Wipe(p, sizeof(p));
      stop();                                      // the app's keys but not a LOGIN: a bug
      TcpAuthAction a;
      a.kind = TcpAuthAction::CLOSE;
      return a;
    }
    uint8_t want[TCP_V2_MAC_LEN];
    tcpV2LoginMac(password ? password : "", _th, want);
    uint8_t diff = (password && password[0]) ? 0 : 1;   // password cleared meanwhile
    for (size_t i = 0; i < TCP_V2_MAC_LEN; i++) diff |= want[i] ^ p[2 + i];   // constant time
    tcpV2Wipe(want, sizeof(want));
    tcpV2Wipe(p, sizeof(p));
    if (diff == 0) {
      TcpAuthAction a = sealed(TcpAuthAction::ACCEPT, TCP_AUTH_OK);
      a.login_ok = (a.kind == TcpAuthAction::ACCEPT);
      return a;                                    // the caller takes the keys next
    }
    TcpAuthAction a = sealed(TcpAuthAction::SEND_CLOSE, TCP_AUTH_WRONG);
    a.failed_login = true;
    stop();
    return a;
  }

  // Called every pass: runs a HELLO held back by the handshake gap; past the deadline the
  // connection gets RESULT 04 (encrypted once the keys exist) and is closed (D5: not counted).
  TcpAuthAction onTick(uint32_t now) {
    if (_state == IDLE) return TcpAuthAction();
    if (now - _started_ms >= TCP_AUTH_DEADLINE_MS) {
      TcpAuthAction a = (_state == WAIT_LOGIN)
                            ? sealed(TcpAuthAction::SEND_CLOSE, TCP_AUTH_TIMEOUT)
                            : plain(TcpAuthAction::SEND_CLOSE, TCP_AUTH_TIMEOUT);
      stop();
      return a;
    }
    if (_hello_waiting && hsAllowed(now)) return handshake(now);
    return TcpAuthAction();
  }
};
