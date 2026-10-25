#if defined(ESP32) && defined(WITH_NET_SERVICES) && !defined(NET_WIFI_EXTERNAL)

#include "NetTls.h"
#include <SPIFFS.h>
#include <esp_idf_version.h>
#include <esp_random.h>
#include <esp_timer.h>
#include <esp_tls.h>
#include <mbedtls/version.h>
#include <mbedtls/pk.h>
#include <mbedtls/ecp.h>
#include <mbedtls/x509_crt.h>
#include <mbedtls/pem.h>
#include <mbedtls/sha256.h>
#include <mbedtls/ssl.h>
#include <lwip/sockets.h>
#include <sys/select.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

// mbedTLS 2.28 (IDF 4.4) vs 3.x (IDF 5, e.g. ESP32-C6): a few renamed calls.
#if MBEDTLS_VERSION_MAJOR >= 3
  #define NET_SHA256(in, len, out)      mbedtls_sha256((in), (len), (out), 0)
  #define NET_PK_PARSE_KEY(ctx, b, l)   mbedtls_pk_parse_key((ctx), (b), (l), NULL, 0, tlsRng, NULL)
#else
  #include <mbedtls/bignum.h>
  #define NET_SHA256(in, len, out)      mbedtls_sha256_ret((in), (len), (out), 0)
  #define NET_PK_PARSE_KEY(ctx, b, l)   mbedtls_pk_parse_key((ctx), (b), (l), NULL, 0)
#endif

#define NETTLS_KEY_FILE  "/netkey.pem"
#define NETTLS_CERT_FILE "/netcert.pem"

// ECDSA P-256 self-signed cert DER is ~450 B, its PEM ~650 B; the EC key PEM ~230 B.
static const size_t DER_MAX = 1024;
static const size_t PEM_MAX = 1536;

namespace NetTls {

// ---- certificate state (heap copies, PEM with trailing NUL) ----
static unsigned char* _cert     = nullptr;
static size_t         _cert_len = 0;
static unsigned char* _key      = nullptr;
static size_t         _key_len  = 0;
static unsigned char  _fp[32]   = {0};        // SHA-256 of the certificate DER
static bool           _ready    = false;
static uint32_t       _keygen_ms = 0;

// Hardware RNG as the mbedTLS f_rng; no DRBG context to keep around.
static int tlsRng(void*, unsigned char* out, size_t n) {
  esp_fill_random(out, n);
  return 0;
}

static void dropMaterial() {
  if (_key) memset(_key, 0, _key_len);         // do not leave the key in freed heap
  free(_cert); free(_key);
  _cert = _key = nullptr;
  _cert_len = _key_len = 0;
  memset(_fp, 0, sizeof(_fp));
  _ready = false;
}

// Copies a NUL-terminated PEM into a fresh heap buffer; len includes the NUL.
static unsigned char* copyPem(const unsigned char* pem, size_t* len_out) {
  size_t l = strlen((const char*)pem) + 1;
  unsigned char* p = (unsigned char*)malloc(l);
  if (!p) return nullptr;
  memcpy(p, pem, l);
  *len_out = l;
  return p;
}

// Reads a whole SPIFFS file plus a trailing NUL. nullptr if missing or oversized.
static unsigned char* readFile(const char* path, size_t* len_out) {
  File f = SPIFFS.open(path, FILE_READ);
  if (!f) return nullptr;
  size_t sz = f.size();
  if (sz == 0 || sz > PEM_MAX) { f.close(); return nullptr; }
  unsigned char* p = (unsigned char*)malloc(sz + 1);
  if (!p) { f.close(); return nullptr; }
  size_t got = f.read(p, sz);
  f.close();
  p[got] = 0;
  *len_out = got + 1;
  return p;
}

static bool writeFile(const char* path, const unsigned char* data, size_t len) {
  File f = SPIFFS.open(path, FILE_WRITE);     // truncates
  if (!f) return false;
  size_t w = f.write(data, len);
  f.close();
  return w == len;
}

// Distinguished name "CN=<cn>,O=<org>". The node name is free text, so characters that
// have a meaning in an RFC 4514 string (, = + < > # ; " \) are dropped and lengths capped.
static void appendClean(char* dst, size_t n, const char* src, size_t max_chars) {
  size_t len = strlen(dst), used = 0;
  for (const char* p = src; *p && used < max_chars && len < n - 1; p++) {
    if (strchr(",=+<>#;\"\\", *p) || (unsigned char)*p < 0x20) continue;
    dst[len++] = *p;
    used++;
  }
  dst[len] = 0;
}

static void buildDn(char* dn, size_t n, const char* cn, const char* org) {
  strlcpy(dn, "CN=", n);
  appendClean(dn, n, cn && cn[0] ? cn : "meshnomad", 32);
  size_t before = strlen(dn);
  strlcat(dn, ",O=", n);
  appendClean(dn, n, org ? org : "", 48);
  if (strlen(dn) == before + 3) dn[before] = 0;   // org empty after cleaning: drop ",O="
}

// Parses cert + key together and checks that they belong to each other (a power loss
// between the two file writes of a renew leaves a mismatched pair). Fills _fp on success.
static bool parsePair(const unsigned char* cert_pem, size_t cert_len,
                      const unsigned char* key_pem, size_t key_len) {
  mbedtls_x509_crt crt;
  mbedtls_pk_context pk;
  mbedtls_x509_crt_init(&crt);
  mbedtls_pk_init(&pk);
  int ret = mbedtls_x509_crt_parse(&crt, cert_pem, cert_len);
  if (ret == 0) ret = NET_PK_PARSE_KEY(&pk, key_pem, key_len);
#if MBEDTLS_VERSION_MAJOR >= 3
  if (ret == 0) ret = mbedtls_pk_check_pair(&crt.pk, &pk, tlsRng, nullptr);
#else
  if (ret == 0) ret = mbedtls_pk_check_pair(&crt.pk, &pk);
#endif
  if (ret == 0) ret = NET_SHA256(crt.raw.p, crt.raw.len, _fp);
  mbedtls_pk_free(&pk);
  mbedtls_x509_crt_free(&crt);
  return ret == 0;
}

// Loads both files into RAM; false when either is missing, does not parse, or the
// key does not match the certificate (the caller then regenerates both).
static bool loadStored() {
  dropMaterial();
  _cert = readFile(NETTLS_CERT_FILE, &_cert_len);
  _key  = readFile(NETTLS_KEY_FILE, &_key_len);
  if (!_cert || !_key || !parsePair(_cert, _cert_len, _key, _key_len)) {
    if (_cert && _key) Serial.println("NetTls: stored key/cert do not match or do not parse, regenerating");
    dropMaterial();
    return false;
  }
  _ready = true;
  return true;
}

// Generates key + self-signed certificate, stores both files, loads them into RAM.
// DER goes through heap buffers and mbedtls_pem_write_buffer: the *_pem writers keep a
// 4 KB scratch array on the stack, too much for the 8 KB Arduino loop task.
static bool generate(const char* cn, const char* org) {
  unsigned long t0 = millis();
  bool ok = false;
  int ret = 0;
  size_t olen = 0;
  unsigned char fp[32];

  mbedtls_pk_context key;
  mbedtls_x509write_cert crt;
  mbedtls_pk_init(&key);
  mbedtls_x509write_crt_init(&crt);
  unsigned char* der = (unsigned char*)malloc(DER_MAX);
  unsigned char* pem = (unsigned char*)malloc(PEM_MAX);
  unsigned char* key_pem = nullptr;
  size_t key_pem_len = 0;
  if (!der || !pem) goto out;

  // -- key --
  ret = mbedtls_pk_setup(&key, mbedtls_pk_info_from_type(MBEDTLS_PK_ECKEY));
  if (ret) goto out;
  ret = mbedtls_ecp_gen_key(MBEDTLS_ECP_DP_SECP256R1, mbedtls_pk_ec(key), tlsRng, nullptr);
  if (ret) goto out;
  ret = mbedtls_pk_write_key_der(&key, der, DER_MAX);   // written at the END of der
  if (ret <= 0) goto out;
  ret = mbedtls_pem_write_buffer("-----BEGIN EC PRIVATE KEY-----\n", "-----END EC PRIVATE KEY-----\n",
                                 der + DER_MAX - ret, ret, pem, PEM_MAX, &olen);
  if (ret) goto out;
  key_pem = copyPem(pem, &key_pem_len);
  memset(der, 0, DER_MAX);
  memset(pem, 0, PEM_MAX);
  if (!key_pem) goto out;

  // -- certificate --
  {
    char dn[96];
    buildDn(dn, sizeof(dn), cn, org);
    mbedtls_x509write_crt_set_version(&crt, MBEDTLS_X509_CRT_VERSION_3);
    mbedtls_x509write_crt_set_md_alg(&crt, MBEDTLS_MD_SHA256);
    mbedtls_x509write_crt_set_subject_key(&crt, &key);
    mbedtls_x509write_crt_set_issuer_key(&crt, &key);
    if ((ret = mbedtls_x509write_crt_set_subject_name(&crt, dn))) goto out;
    if ((ret = mbedtls_x509write_crt_set_issuer_name(&crt, dn))) goto out;
    if ((ret = mbedtls_x509write_crt_set_validity(&crt, "20200101000000", "20991231235959"))) goto out;
    if ((ret = mbedtls_x509write_crt_set_basic_constraints(&crt, 0, -1))) goto out;

    unsigned char serial[8];                            // random positive 64-bit serial
    esp_fill_random(serial, sizeof(serial));
    serial[0] = (serial[0] & 0x7f) | 0x01;
#if MBEDTLS_VERSION_MAJOR >= 3
    ret = mbedtls_x509write_crt_set_serial_raw(&crt, serial, sizeof(serial));
#else
    mbedtls_mpi sn;
    mbedtls_mpi_init(&sn);
    ret = mbedtls_mpi_read_binary(&sn, serial, sizeof(serial));
    if (ret == 0) ret = mbedtls_x509write_crt_set_serial(&crt, &sn);
    mbedtls_mpi_free(&sn);
#endif
    if (ret) goto out;
  }
  ret = mbedtls_x509write_crt_der(&crt, der, DER_MAX, tlsRng, nullptr);   // at the END of der
  if (ret <= 0) goto out;
  {
    const unsigned char* cert_der = der + DER_MAX - ret;
    if (NET_SHA256(cert_der, ret, fp)) goto out;    // _fp changes only once installed
    ret = mbedtls_pem_write_buffer("-----BEGIN CERTIFICATE-----\n", "-----END CERTIFICATE-----\n",
                                   cert_der, ret, pem, PEM_MAX, &olen);
    if (ret) goto out;
  }

  // -- install + store --
  // Copy the new cert first: on OOM a renew keeps the old pair and stays ready().
  {
    size_t new_cert_len = 0;
    unsigned char* new_cert = copyPem(pem, &new_cert_len);
    if (!new_cert) goto out;
    if (_key) memset(_key, 0, _key_len);   // renew: old key out of the freed heap
    free(_cert); free(_key);
    _cert = new_cert;
    _cert_len = new_cert_len;
    _key = key_pem;
    _key_len = key_pem_len;
    key_pem = nullptr;
    memcpy(_fp, fp, sizeof(_fp));
  }
  _ready = true;
  _keygen_ms = millis() - t0;
  ok = true;

  if (!writeFile(NETTLS_KEY_FILE, _key, _key_len - 1) ||
      !writeFile(NETTLS_CERT_FILE, _cert, _cert_len - 1)) {
    Serial.println("NetTls: storing the key/cert files failed (kept in RAM for this boot)");
  }
  {
    char fp[80];
    fingerprint(fp, sizeof(fp));
    Serial.printf("NetTls: generated ECDSA P-256 key + self-signed cert in %lu ms, %s\n",
                  (unsigned long)_keygen_ms, fp);
  }

out:
  if (!ok) Serial.printf("NetTls: key/cert generation failed (-0x%04x)\n", (unsigned)(-ret));
  if (key_pem) { memset(key_pem, 0, key_pem_len); free(key_pem); }
  if (der) { memset(der, 0, DER_MAX); free(der); }
  if (pem) { memset(pem, 0, PEM_MAX); free(pem); }
  mbedtls_x509write_crt_free(&crt);
  mbedtls_pk_free(&key);
  return ok;
}

bool ensure(const char* cn, const char* org) {
  if (_ready) return true;
  if (loadStored()) return true;
  return generate(cn, org);
}

bool renew(const char* cn, const char* org) {
  return generate(cn, org);
}

bool ready() { return _ready; }
const unsigned char* certPem() { return _ready ? _cert : nullptr; }
size_t certPemLen()            { return _ready ? _cert_len : 0; }
const unsigned char* keyPem()  { return _ready ? _key : nullptr; }
size_t keyPemLen()             { return _ready ? _key_len : 0; }
uint32_t lastKeygenMs()        { return _keygen_ms; }

bool fingerprint(char* out, size_t n) {
  if (!_ready || n < 7 + 64 + 1) {
    if (n) out[0] = 0;
    return false;
  }
  int len = snprintf(out, n, "sha256:");
  for (int i = 0; i < 32; i++) len += snprintf(out + len, n - len, "%02x", _fp[i]);
  return true;
}

// ---- TLS server ----
// Handshake deadline: runs in the esp_timer task while loop() sits in
// esp_tls_server_session_create(). Only touches the fd, never the TLS context.
void Server::deadlineCb(void* arg) {
  Server* self = (Server*)arg;
  int fd = self->_deadline_fd;
  if (fd >= 0) shutdown(fd, SHUT_RDWR);
}

static void setNonBlocking(int fd, bool on) {
  int fl = fcntl(fd, F_GETFL, 0);
  if (fl < 0) fl = 0;
  fcntl(fd, F_SETFL, on ? (fl | O_NONBLOCK) : (fl & ~O_NONBLOCK));
}

bool Server::begin(uint16_t port) {
  if (_listen_fd >= 0) return true;
  int fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (fd < 0) return false;
  int one = 1;
  setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
  struct sockaddr_in addr = {};
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = htonl(INADDR_ANY);
  addr.sin_port = htons(port);
  if (bind(fd, (struct sockaddr*)&addr, sizeof(addr)) < 0 || listen(fd, 1) < 0) {
    ::close(fd);
    return false;
  }
  setNonBlocking(fd, true);
  _listen_fd = fd;
  return true;
}

void Server::end() {
  disconnect();
  if (_listen_fd >= 0) {
    ::close(_listen_fd);
    _listen_fd = -1;
  }
}

int Server::pollAccept(uint32_t* peer_ip) {
  if (_listen_fd < 0) return -1;
  struct sockaddr_in peer = {};
  socklen_t plen = sizeof(peer);
  int fd = accept(_listen_fd, (struct sockaddr*)&peer, &plen);
  if (fd < 0) return -1;                       // EWOULDBLOCK: nobody waiting
  if (peer_ip) *peer_ip = peer.sin_addr.s_addr;   // raw, same as (uint32_t)IPAddress
  return fd;
}

void Server::reject(int fd) {
  if (fd >= 0) ::close(fd);
}

bool Server::handshake(int fd, uint32_t timeout_ms) {
  disconnect();                                // one client at a time
  if (fd < 0 || !_ready) { reject(fd); return false; }

  // Blocking socket with timeouts: esp_tls_server_session_create() loops on the
  // handshake until it finishes or the transport fails, and a receive timeout on a
  // blocking lwIP socket is reported as a transport error, which ends the loop.
  // SO_RCVTIMEO restarts with every byte, so a peer dripping one byte per few seconds
  // would hold loop() for hours: a one-shot esp_timer enforces timeout_ms in total by
  // shutting the socket down (LWIP_NETCONN_FULLDUPLEX: shutdown() wakes the blocked
  // read, which then fails and ends the handshake loop).
  struct timeval tv = { (time_t)(timeout_ms / 1000), (suseconds_t)((timeout_ms % 1000) * 1000) };
  setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
  setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
  setNonBlocking(fd, false);

  esp_tls_cfg_server_t cfg = {};
  cfg.servercert_buf   = _cert;                // IDF 4.4 and 5.x: same field names
  cfg.servercert_bytes = _cert_len;            // PEM incl. NUL
  cfg.serverkey_buf    = _key;
  cfg.serverkey_bytes  = _key_len;

  esp_tls_t* tls = esp_tls_init();             // ~40 KB arrives inside session_create
  if (!tls) { reject(fd); return false; }

  _deadline_fd = fd;
  esp_timer_handle_t deadline = nullptr;
  esp_timer_create_args_t targs = {};
  targs.callback = &Server::deadlineCb;        // esp_timer task; shutdown() is task-safe
  targs.arg = this;
  targs.name = "nettls_hs";
  if (esp_timer_create(&targs, &deadline) == ESP_OK) {
    esp_timer_start_once(deadline, (uint64_t)timeout_ms * 1000ULL);
  }

  unsigned long t0 = millis();
  int ret = esp_tls_server_session_create(&cfg, fd, tls);
  _hs_ms = millis() - t0;

  if (deadline) {
    esp_timer_stop(deadline);                  // no-op if it already fired
    esp_timer_delete(deadline);
  }
  _deadline_fd = -1;
  if (ret != 0) {
    esp_tls_server_session_delete(tls);        // frees contexts, does not close fd
    ::close(fd);
    return false;
  }
  setNonBlocking(fd, true);                    // reads/writes from loop() never block
  _tls = tls;
  _fd = fd;
  return true;
}

int Server::readSome(char* buf, size_t n) {
  if (!_tls) return -1;
  ssize_t r = esp_tls_conn_read(_tls, buf, n);
  if (r > 0) return (int)r;
  if (r == ESP_TLS_ERR_SSL_WANT_READ || r == ESP_TLS_ERR_SSL_WANT_WRITE) return 0;
  disconnect();                                // 0 = EOF, other negatives = error/close notify
  return -1;
}

bool Server::writeAll(const void* data, size_t len, uint32_t write_timeout_ms) {
  if (!_tls) return false;
  const unsigned char* p = (const unsigned char*)data;
  unsigned long t0 = millis();
  while (len > 0) {
    ssize_t w = esp_tls_conn_write(_tls, p, len);
    if (w > 0) {
      p += w;
      len -= w;
      continue;
    }
    if (w != ESP_TLS_ERR_SSL_WANT_WRITE && w != ESP_TLS_ERR_SSL_WANT_READ) break;
    if (millis() - t0 > write_timeout_ms) break;
    fd_set ws;                                 // socket full: wait briefly for room
    FD_ZERO(&ws);
    FD_SET(_fd, &ws);
    struct timeval tv = { 0, 20000 };
    select(_fd + 1, nullptr, &ws, nullptr, &tv);
  }
  if (len > 0) {
    disconnect();
    return false;
  }
  return true;
}

void Server::disconnect() {
  if (_tls) {
    // close_notify first, so clients see a clean TLS shutdown instead of an EOF error;
    // one try on the non-blocking socket, the alert is a few bytes
#if ESP_IDF_VERSION_MAJOR >= 5
    mbedtls_ssl_context* ssl = (mbedtls_ssl_context*)esp_tls_get_ssl_context(_tls);
#else
    mbedtls_ssl_context* ssl = &_tls->ssl;
#endif
    if (ssl) mbedtls_ssl_close_notify(ssl);
    esp_tls_server_session_delete(_tls);       // once; leaves the fd to us
    _tls = nullptr;
  }
  if (_fd >= 0) {
    ::close(_fd);                              // once
    _fd = -1;
  }
}

}  // namespace NetTls

#endif  // ESP32 && WITH_NET_SERVICES && !NET_WIFI_EXTERNAL
