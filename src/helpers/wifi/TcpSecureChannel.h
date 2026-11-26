#pragma once
// MeshNomad: key schedule and frame cipher of the companion's TCP link, protocol v2 (B91).
// Design: docs/superpowers/reports/2026-11-26-b91-tcp-protocol-v2.md (local).
//
//   th    = SHA-256("MNTCP2-th" || A || R || N)          A, R: X25519 one-time keys, N: node key
//   okm   = HKDF-SHA256(ikm = X25519 secret, salt = "MNTCP2", info = th, 64 B)
//   k_a2r = okm[0..32), k_r2a = okm[32..64)
//   frame = ChaCha20-Poly1305(key of the direction, nonce = 00000000 || counter u64 LE,
//           no AAD) = ciphertext || tag 16 B; the counter starts at 0 per direction and is
//           not sent (TCP keeps the order), so a dropped, repeated or reordered frame fails.
//   login = HMAC-SHA256(key = password, msg = "MNTCP2-login" || th)
//   sig   = Ed25519(node key, "MNTCP2-sig" || A || R)
// Pure C++ on rweather Crypto, no Arduino: the board and the native tests run the same code.

#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <ChaChaPoly.h>
#include <Curve25519.h>
#include <HKDF.h>
#include <SHA256.h>

#define TCP_V2_KEY_LEN    32
#define TCP_V2_TAG_LEN    16
#define TCP_V2_SIG_LEN    64
#define TCP_V2_TH_LEN     32
#define TCP_V2_MAC_LEN    32
#define TCP_V2_COUNTER_MAX 0xFFFFFFFFULL   // a session ends before the counter passes this

inline void tcpV2Wipe(void* p, size_t n) {
  volatile uint8_t* v = (volatile uint8_t*)p;
  while (n--) *v++ = 0;
}

// "MNTCP2-sig" || A || R (74 B), the message the node signs.
inline size_t tcpV2SigMessage(const uint8_t a[32], const uint8_t r[32], uint8_t out[74]) {
  memcpy(out, "MNTCP2-sig", 10);
  memcpy(out + 10, a, 32);
  memcpy(out + 42, r, 32);
  return 74;
}

inline void tcpV2Transcript(const uint8_t a[32], const uint8_t r[32], const uint8_t n[32],
                            uint8_t th[TCP_V2_TH_LEN]) {
  SHA256 sha;
  sha.update("MNTCP2-th", 9);
  sha.update(a, 32);
  sha.update(r, 32);
  sha.update(n, 32);
  sha.finalize(th, TCP_V2_TH_LEN);
}

inline void tcpV2LoginMac(const char* password, const uint8_t th[TCP_V2_TH_LEN],
                          uint8_t out[TCP_V2_MAC_LEN]) {
  size_t pl = strlen(password);
  SHA256 sha;
  sha.resetHMAC(password, pl);
  sha.update("MNTCP2-login", 12);
  sha.update(th, TCP_V2_TH_LEN);
  sha.finalizeHMAC(password, pl, out, TCP_V2_MAC_LEN);
}

// X25519 key pair from 32 random bytes (clamped here, RFC 7748).
inline void tcpV2KeyPair(uint8_t priv[32], uint8_t pub[32]) {
  priv[0] &= 0xF8;
  priv[31] = (priv[31] & 0x7F) | 0x40;
  Curve25519::eval(pub, priv, nullptr);
}

// Shared secret into `secret`; wipes priv. False for a weak or all-zero result (refuse).
inline bool tcpV2Shared(const uint8_t peer_pub[32], uint8_t priv[32], uint8_t secret[32]) {
  priv[0] &= 0xF8;                        // RFC 7748 decodeScalar25519; eval() does not clamp
  priv[31] = (priv[31] & 0x7F) | 0x40;
  memcpy(secret, peer_pub, 32);
  return Curve25519::dh2(secret, priv);   // dh2 wipes priv and checks weak points
}

// One direction: key + implicit counter.
class TcpV2Cipher {
  uint8_t  _key[TCP_V2_KEY_LEN];
  uint64_t _ctr = 0;
  bool     _set = false;

  void nonce(uint8_t iv[12]) const {
    memset(iv, 0, 4);
    for (int i = 0; i < 8; i++) iv[4 + i] = (uint8_t)(_ctr >> (8 * i));
  }

public:
  TcpV2Cipher() { memset(_key, 0, sizeof(_key)); }
  ~TcpV2Cipher() { clear(); }

  void setKey(const uint8_t key[TCP_V2_KEY_LEN]) {
    memcpy(_key, key, TCP_V2_KEY_LEN);
    _ctr = 0;
    _set = true;
  }
  void clear() {
    tcpV2Wipe(_key, sizeof(_key));
    _ctr = 0;
    _set = false;
  }
  bool ready() const { return _set; }
  uint64_t counter() const { return _ctr; }
#ifdef TCP_V2_TESTING
  void testSetCounter(uint64_t c) { _ctr = c; }
#endif

  // out = ciphertext || tag (len + 16 B). False when the counter is used up.
  bool seal(const uint8_t* in, size_t len, uint8_t* out) {
    if (!_set || _ctr > TCP_V2_COUNTER_MAX) return false;
    uint8_t iv[12];
    nonce(iv);
    ChaChaPoly c;
    c.setKey(_key, TCP_V2_KEY_LEN);
    c.setIV(iv, sizeof(iv));
    c.encrypt(out, in, len);
    c.computeTag(out + len, TCP_V2_TAG_LEN);
    c.clear();
    _ctr++;
    return true;
  }

  // Decrypts ciphertext || tag into out (len - 16 B). False on a bad tag, a short frame or a
  // used-up counter; out is wiped then and the counter does not move.
  bool open(const uint8_t* in, size_t len, uint8_t* out, size_t* out_len) {
    if (!_set || len < TCP_V2_TAG_LEN || _ctr > TCP_V2_COUNTER_MAX) return false;
    size_t n = len - TCP_V2_TAG_LEN;
    uint8_t iv[12];
    nonce(iv);
    ChaChaPoly c;
    c.setKey(_key, TCP_V2_KEY_LEN);
    c.setIV(iv, sizeof(iv));
    c.decrypt(out, in, n);
    bool ok = c.checkTag(in + n, TCP_V2_TAG_LEN);
    c.clear();
    if (!ok) {
      tcpV2Wipe(out, n);
      return false;
    }
    _ctr++;
    *out_len = n;
    return true;
  }
};

// Both directions of one connection, from the side of `radio` (true: send with k_r2a).
struct TcpV2Channel {
  TcpV2Cipher tx, rx;

  void clear() { tx.clear(); rx.clear(); }
  bool ready() const { return tx.ready() && rx.ready(); }

  // Key schedule; wipes the secret.
  void derive(uint8_t secret[32], const uint8_t th[TCP_V2_TH_LEN], bool radio) {
    uint8_t okm[2 * TCP_V2_KEY_LEN];
    hkdf<SHA256>(okm, sizeof(okm), secret, 32, "MNTCP2", 6, th, TCP_V2_TH_LEN);
    tcpV2Wipe(secret, 32);
    const uint8_t* a2r = okm;
    const uint8_t* r2a = okm + TCP_V2_KEY_LEN;
    tx.setKey(radio ? r2a : a2r);
    rx.setKey(radio ? a2r : r2a);
    tcpV2Wipe(okm, sizeof(okm));
  }

  // Moves the keys and counters to `dst`; this one is cleared.
  void moveTo(TcpV2Channel& dst) {
    dst = *this;
    clear();
  }
};
