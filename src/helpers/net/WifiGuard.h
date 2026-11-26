#pragma once
// MeshNomad #23: guard against a WiFi link that stays associated but carries no traffic (B84:
// `wifi:connected`, IP and RSSI shown, nothing sent, no disconnect event, so nothing restarts
// WiFi). Pure logic, no Arduino (unit-tested natively); the companion glue runs the probe (an
// ARP request to the gateway) and carries out the action.
//
//   link up -> wait SETTLE_MS -> probe every PROBE_EVERY_MS
//   FAILS_TO_ACT failed probes in a row -> RECONNECT (WiFi off and on), up to
//   RECONNECTS_TO_RESTART times in one dead spell, then RESTART (the radio, once);
//   one good probe ends the spell.
// Off (wifi.guard 0) or link down (MeshCore's own reconnect handles that): nothing.

#include <stdint.h>

class WifiGuard {
public:
  enum class Action : uint8_t { NONE, PROBE, RECONNECT, RESTART };

  static const uint32_t SETTLE_MS = 30000;        // after the link comes up
  static const uint32_t PROBE_EVERY_MS = 30000;
  static const uint32_t PROBE_TIMEOUT_MS = 5000;  // no report from the probe = failed
  static const uint8_t  FAILS_TO_ACT = 3;
  static const uint8_t  RECONNECTS_TO_RESTART = 2;

  // Every loop pass. enabled = wifi.guard; link_up = station connected with an IP.
  Action tick(bool enabled, bool link_up, uint32_t now) {
    if (!enabled || !link_up) {
      _was_up = false;
      _probing = false;
      _fails_in_row = 0;
      _pending = Action::NONE;
      return Action::NONE;
    }
    if (!_was_up) {                 // the link (re)appeared: let it settle
      _was_up = true;
      _next_probe = now + SETTLE_MS;
      return Action::NONE;
    }
    if (_pending != Action::NONE) {
      Action a = _pending;
      _pending = Action::NONE;
      return a;
    }
    if (_probing) {
      if ((int32_t)(now - (_probe_at + PROBE_TIMEOUT_MS)) >= 0) probeResult(false, now);
      return Action::NONE;
    }
    if ((int32_t)(now - _next_probe) >= 0) {
      _probing = true;
      _probe_at = now;
      _probes++;
      return Action::PROBE;
    }
    return Action::NONE;
  }

  // The probe's answer (gateway reached or not).
  void probeResult(bool ok, uint32_t now) {
    if (!_probing) return;
    _probing = false;
    _next_probe = now + PROBE_EVERY_MS;
    if (ok) {
      _fails_in_row = 0;
      _reconnects_in_spell = 0;
      return;
    }
    _failures++;
    if (++_fails_in_row < FAILS_TO_ACT) return;
    _fails_in_row = 0;
    if (_reconnects_in_spell >= RECONNECTS_TO_RESTART) {
      _reconnects_in_spell = 0;
      _pending = Action::RESTART;
    } else {
      _reconnects_in_spell++;
      _reconnects++;
      _pending = Action::RECONNECT;
    }
  }

  uint32_t probes() const { return _probes; }
  uint32_t failures() const { return _failures; }
  uint32_t reconnects() const { return _reconnects; }

private:
  bool     _was_up = false;
  bool     _probing = false;
  uint32_t _probe_at = 0;
  uint32_t _next_probe = 0;
  uint8_t  _fails_in_row = 0;
  uint8_t  _reconnects_in_spell = 0;
  Action   _pending = Action::NONE;
  uint32_t _probes = 0, _failures = 0, _reconnects = 0;
};
