#pragma once

// When an NTP time is written to the node's clock, no Arduino dependency (unit-tested
// natively). from_server: a server reply arrived in this boot, so the system time is the
// server's; without one it is only a time kept from before and may move the clock by less
// than an hour. A server time may step the clock any distance, backwards too.

#include <stdint.h>

static const uint32_t NET_EPOCH_MIN = 1600000000UL;   // earlier values are not a real time
static const uint32_t NET_EPOCH_WINDOW_S = 3600;

inline bool netEpochAccept(uint32_t now, uint32_t epoch, bool from_server) {
  if (epoch < NET_EPOCH_MIN) return false;
  if (now < NET_EPOCH_MIN || from_server) return true;
  int32_t delta = (int32_t)(epoch - now);
  return delta > -(int32_t)NET_EPOCH_WINDOW_S && delta < (int32_t)NET_EPOCH_WINDOW_S;
}

// A step back by more than the window: stored times from the old clock now lie in the future.
inline bool netSteppedBack(uint32_t before, uint32_t after) {
  return before > after && before - after > NET_EPOCH_WINDOW_S;
}

// B85: after a time from an NTP server, pull stored contact times back to now. Not only after
// a backward step: a clock corrected earlier (another build, GPS, an older step) leaves its
// future values behind, and they keep the contact sync mark in the future. Never on a clock
// that is not a real time, or every contact would be set to it.
inline bool netShouldClampContacts(bool from_server, uint32_t now) {
  return from_server && now >= NET_EPOCH_MIN;
}

inline uint32_t netClampLastmod(uint32_t lastmod, uint32_t now) {
  return lastmod > now ? now : lastmod;
}
