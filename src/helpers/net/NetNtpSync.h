#pragma once

#include <stdint.h>

// 'net ntp sync': decision and reply texts, no Arduino dependency (unit-tested natively).
// ntp_enabled: ntp.enabled is 1 and the node has a clock to set; the setting is reported
// before the link, so a node with both off is told to switch NTP on first.

enum NtpSyncCheck { NTP_SYNC_OFF, NTP_SYNC_NO_WIFI, NTP_SYNC_START };

inline NtpSyncCheck netNtpSyncCheck(bool ntp_enabled, bool wifi_connected) {
  if (!ntp_enabled) return NTP_SYNC_OFF;
  if (!wifi_connected) return NTP_SYNC_NO_WIFI;
  return NTP_SYNC_START;
}

inline const char* netNtpSyncReply(NtpSyncCheck c) {
  switch (c) {
    case NTP_SYNC_OFF:     return "Error: NTP off, set ntp.enabled 1 first";
    case NTP_SYNC_NO_WIFI: return "Error: WiFi not connected";
    default:               return "> ntp sync started";
  }
}

// B85: once the clock was applied in this boot, when to apply it again. Every boot starts
// with a clock restored from before (RTC backup or the build seed), so the first pass applies
// it without a server reply. The server's time is then applied as soon as its first reply of
// the boot is seen (first_reply), which also runs the corrections that need a server time
// (backward step, contact times); after that once per ntp.interval.
inline bool netNtpApplyDue(bool first_reply, uint32_t since_apply_ms, uint32_t interval_ms) {
  return first_reply || since_apply_ms >= interval_ms;
}
