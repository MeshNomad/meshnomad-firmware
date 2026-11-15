#pragma once

// wifi.power: the setting is written to the WiFi driver only when it differs from what the
// driver holds, so a change applies at once, back to 0 (chip default) too, and a loop that
// asks on every pass sets nothing. applied: the value now in the driver; 0 at boot, and
// kept across a WiFi restart because the driver keeps its power. No Arduino dependency
// (unit-tested natively).

inline bool netWifiPowerDue(int wanted, int applied) {
  return wanted != applied;
}
