#pragma once

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
