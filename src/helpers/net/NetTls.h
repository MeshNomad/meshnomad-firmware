#pragma once

// NetTls: per-node TLS material and a one-client TLS server for the network console.
//  - ECDSA P-256 key + self-signed certificate, generated on the board once and kept
//    in SPIFFS (/netkey.pem, /netcert.pem); the key is never served or echoed.
//  - NetTls::Server wraps a raw lwIP listener + one esp-tls server session so the
//    console (port 5000) and, later, the OTA server share the same certificate.
// Compiled only where the fork owns the WiFi link: the companion build sets
// NET_WIFI_EXTERNAL and must not pull any TLS code in.

#if defined(ESP32) && defined(WITH_NET_SERVICES) && !defined(NET_WIFI_EXTERNAL)

#include <Arduino.h>
#include <stddef.h>
#include <stdint.h>

struct esp_tls;                      // esp_tls_t; the header stays out of NetServices.h

namespace NetTls {

// ---- certificate ----
// Load /netkey.pem + /netcert.pem from SPIFFS into RAM; if either is missing or does not
// parse, generate an ECDSA P-256 key + self-signed cert (CN = cn, O = org, valid
// 20200101000000..20991231235959, SHA-256) and store both. Blocks for the key generation
// (hundreds of ms on ESP32 classic); call from setup() or loop(), never from the radio path.
bool ensure(const char* cn, const char* org);
bool renew(const char* cn, const char* org);   // regenerate, store, reload
bool ready();

// PEM buffers for esp_tls_cfg_server_t / httpd_ssl_config_t. Lengths include the trailing
// NUL, as the mbedTLS PEM parser requires. nullptr / 0 until ready().
const unsigned char* certPem();
size_t certPemLen();
const unsigned char* keyPem();
size_t keyPemLen();

// Writes "sha256:<64 lowercase hex>" = SHA-256 of the certificate DER (n >= 72).
bool fingerprint(char* out, size_t n);

// Milliseconds the last key + certificate generation took; 0 if nothing was generated
// since boot (files were already present).
uint32_t lastKeygenMs();

// ---- TLS server (one client) ----
// Lifecycle per client, all driven from loop():
//   int fd = srv.pollAccept(&ip);       // non-blocking; -1 when nobody is waiting
//   if (locked_out(ip)) srv.reject(fd); // closes the socket, no TLS work done
//   else if (!srv.handshake(fd)) ...;   // blocks up to the socket timeout; owns fd now
//   srv.readSome()/srv.print() ...      // non-blocking after the handshake
//   srv.disconnect();                   // frees the session and closes the socket once
// Session memory (~40 KB: 16 KB in + 16 KB out mbedTLS buffers + contexts) exists only
// between a successful handshake() and disconnect().
// Method names avoid lwIP's socket macros (listen/accept/socket/send/recv/select).
class Server {
public:
  // Opens a non-blocking lwIP listener on port (backlog 1). False if bind/listen failed.
  bool begin(uint16_t port);
  // Closes the client session (if any) and the listener.
  void end();
  bool listening() const { return _listen_fd >= 0; }

  // Polls accept(). Returns the new socket fd (>= 0) and the peer IPv4 address in the
  // same raw form WiFiClient::remoteIP() casts to uint32_t, so it can be compared with
  // the console lockout table; -1 when no connection is pending. The fd belongs to the
  // caller until reject() or handshake() takes it.
  int pollAccept(uint32_t* peer_ip);
  // Drops a just-accepted socket without any TLS exchange (lockout, busy).
  static void reject(int fd);

  // Runs the TLS handshake on fd. Blocking, at most timeout_ms in total: SO_RCVTIMEO/
  // SO_SNDTIMEO cover a silent peer (plain `nc`), a one-shot esp_timer that shuts the
  // socket down covers a slow-drip peer (one byte per few seconds).
  // On success the socket is switched to non-blocking and owned by this Server; on failure
  // the session is freed and fd closed. handshakeMs() has the duration either way.
  bool handshake(int fd, uint32_t timeout_ms = 5000);
  bool connected() const { return _tls != nullptr; }
  uint32_t handshakeMs() const { return _hs_ms; }

  // Non-blocking read of decrypted bytes: > 0 count, 0 nothing available yet,
  // < 0 peer closed / error (the session is already closed by then).
  int readSome(char* buf, size_t n);
  // Writes all bytes (short waits on a full socket, bounded by write_timeout_ms).
  // False on error; the session is closed then.
  bool writeAll(const void* data, size_t len, uint32_t write_timeout_ms = 2000);
  bool print(const char* s) { return writeAll(s, strlen(s)); }
  // Frees the session (esp_tls_server_session_delete, once) and closes the socket (once).
  void disconnect();

private:
  int       _listen_fd = -1;
  int       _fd        = -1;
  esp_tls*  _tls       = nullptr;
  uint32_t  _hs_ms     = 0;
  volatile int _deadline_fd = -1;    // fd under handshake, read by the deadline timer
  static void deadlineCb(void* arg);
};

}  // namespace NetTls

#endif  // ESP32 && WITH_NET_SERVICES && !NET_WIFI_EXTERNAL
