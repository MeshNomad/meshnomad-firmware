#pragma once
// MeshNomad: failed logins per client IP, shared by the repeater/room server console,
// 'net ota' HTTPS login and the companion TCP login. 3 failures lock that IP out for
// 5 minutes; other addresses are unaffected; the oldest entry is reused when full.
// Not thread-safe: a caller used from two tasks wraps every call in its own lock.
// The clock is passed in (millis() on the board), so the logic runs in native tests.

#include <stdint.h>

class LoginLockout {
public:
  static const uint8_t  MAX_FAILS  = 3;
  static const uint32_t LOCKOUT_MS = 300000UL;
  static const int      SLOTS      = 16;     // 16 x 12 B

  // true = refuse without checking the password
  bool isLocked(uint32_t ip, uint32_t now) {
    Entry* e = find(ip, false, now);
    if (!e) return false;
    if (now - e->last_ms >= LOCKOUT_MS) {    // expired: forget the failures
      e->count = 0;
      return false;
    }
    return e->count >= MAX_FAILS;
  }

  void fail(uint32_t ip, uint32_t now) {
    Entry* e = find(ip, true, now);
    if (now - e->last_ms >= LOCKOUT_MS) e->count = 0;
    if (e->count < 255) e->count++;
    e->last_ms = now;
    if (_fails < 0xFFFFFFFFUL) _fails++;
  }

  void ok(uint32_t ip) {                      // successful login: forget the failures
    Entry* e = find(ip, false, 0);
    if (e) e->count = 0;
  }

  uint32_t failsSinceBoot() const { return _fails; }

  int lockedCount(uint32_t now) const {
    int n = 0;
    for (const Entry& e : _e) {
      if (e.count >= MAX_FAILS && now - e.last_ms < LOCKOUT_MS) n++;
    }
    return n;
  }

private:
  struct Entry {
    uint32_t ip;
    uint8_t  count;
    uint32_t last_ms;
  };
  Entry _e[SLOTS] = {};
  uint32_t _fails = 0;

  Entry* find(uint32_t ip, bool create, uint32_t now) {
    Entry* oldest = &_e[0];
    for (Entry& e : _e) {
      if (e.count > 0 && e.ip == ip) return &e;
      if (e.count == 0 || e.last_ms < oldest->last_ms) oldest = &e;
    }
    if (!create) return nullptr;
    *oldest = {ip, 0, now};
    return oldest;
  }
};
