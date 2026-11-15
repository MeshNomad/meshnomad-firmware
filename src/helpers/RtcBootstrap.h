#pragma once

// MeshNomad: when the boot may take the clock from stored contact data (lastmod). Only a
// clock that was never set is replaced: one that already holds a time (hardware RTC,
// time kept across a restart) stays, because contact data can carry another node's wrong
// clock. No Arduino dependency (unit-tested natively).

#include <stdint.h>

#ifndef RTC_UNSET_BELOW
  // ESP32RTCClock's power-on seed (RTC_TIME_MIN, 1 Mar 2026) plus one day of running unset.
  #define RTC_UNSET_BELOW (1772323200UL + 86400UL)
#endif

inline bool rtcBootstrapWanted(uint32_t now) {
  return now < RTC_UNSET_BELOW;
}
