#pragma once
// MeshNomad: constants of the companion's TCP (WiFi) login, protocol v2 (firmware B89, B91).
// No crypto here: `get features`, NodePrefs and the CLI include this; the handshake itself
// is in TcpAuthGate.h, the session cipher in TcpSecureChannel.h.

#include <stddef.h>
#include <stdint.h>

// Companion builds that carry the login (MeshNomad, ESP32, WiFi interface).
#if defined(MESHNOMAD_BUILD) && defined(ESP32) && defined(ENABLE_WIFI_INTERFACE)
  #define MN_TCP_AUTH 1
#endif

#define TCP_AUTH_FRAME       0xF0
#define TCP_AUTH_HELLO       0x01
#define TCP_AUTH_LOGIN       0x02
#define TCP_AUTH_RESULT      0x03
#define TCP_AUTH_VERSION     0x02

#define TCP_AUTH_OK          0x00
#define TCP_AUTH_WRONG       0x01
#define TCP_AUTH_LOCKED      0x02
#define TCP_AUTH_NO_PASSWORD 0x03
#define TCP_AUTH_TIMEOUT     0x04
#define TCP_AUTH_UNSUPPORTED 0x05   // not a v2 HELLO (protocol v1 app, stock app with tcp.auth 1)

#define TCP_AUTH_DEADLINE_MS  15000UL
#define TCP_AUTH_HS_GAP_MS    2000UL   // at most one handshake (X25519 + signature) per 2 s

#define TCP_PASSWORD_MIN     8
#define TCP_PASSWORD_MAX     63

// D4 (B89): 8-63 printable ASCII characters (0x20-0x7E).
inline bool tcpPasswordValid(const char* pw) {
  size_t n = 0;
  for (; pw[n]; n++) {
    if ((uint8_t)pw[n] < 0x20 || (uint8_t)pw[n] > 0x7E) return false;
    if (n >= TCP_PASSWORD_MAX) return false;
  }
  return n >= TCP_PASSWORD_MIN;
}

// A frame is a HELLO of any version.
inline bool tcpAuthIsHello(const uint8_t* f, size_t len) {
  return len >= 2 && f[0] == TCP_AUTH_FRAME && f[1] == TCP_AUTH_HELLO;
}

inline size_t tcpAuthResultFrame(uint8_t status, uint8_t* out) {
  out[0] = TCP_AUTH_FRAME; out[1] = TCP_AUTH_RESULT; out[2] = status;
  return 3;
}
