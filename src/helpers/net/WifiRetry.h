#pragma once

// WifiRetry: bounded WiFi station attempts, no Arduino dependency (unit-tested natively).
// Three attempts of at most ATTEMPT_MS each, the radio off for GAP1_MS / GAP2_MS in between;
// an attempt ends early once its scan reports no AP / a failed connect (scan_failed);
// after the third failure the radio stays off (FAILED) until retry(), reset() or reboot.
// An AP that drops within STABLE_MS of connecting counts as the next attempt, so a
// flapping AP cannot keep the radio on forever. The caller maps each returned Action
// onto the WiFi driver; times are millis() values and may wrap.

#include <stdint.h>

class WifiRetry {
public:
  enum Phase  : uint8_t { IDLE, ATTEMPT, UP, OFF_WAIT, FAILED };
  enum Action : uint8_t { NONE, RADIO_ON, REBEGIN, RADIO_OFF };
  static const uint8_t  MAX_ATTEMPTS = 3;
  static const uint32_t ATTEMPT_MS = 20000, STABLE_MS = 30000;
  static const uint32_t GAP1_MS = 15000, GAP2_MS = 30000;   // radio off after attempt 1 / 2
  static const uint32_t SCAN_MIN_MS = 1000;   // earlier failures belong to the previous scan

  // want: wifi.enabled && ssid set. connected: WL_CONNECTED. hold: an OTA session is
  // active (never RADIO_OFF while true; the attempt waits). scan_failed / failed_at: the
  // driver reported no AP / a failed connect at millis() failed_at; reports from the
  // attempt's first SCAN_MIN_MS belong to the previous scan and are ignored.
  Action step(uint32_t now, bool want, bool connected, bool hold,
              bool scan_failed = false, uint32_t failed_at = 0) {
    if (!want) {
      _retry = false;
      if (_phase == IDLE) return NONE;
      _phase = IDLE;
      _attempt = 0;
      return RADIO_OFF;
    }
    switch (_phase) {
      case IDLE:
      case OFF_WAIT:
      case FAILED:
        if (_phase == IDLE || _retry) {
          _retry = false;
          return startAttempt(now, 1, RADIO_ON);
        }
        if (_phase == OFF_WAIT && due(now)) return startAttempt(now, _attempt + 1, RADIO_ON);
        return NONE;                       // connected here = late event-task begin(): ignored
      case ATTEMPT:
        if (_retry) {
          _retry = false;
          return startAttempt(now, 1, REBEGIN);
        }
        if (connected) {
          _phase = UP;
          _up_since = now;
          return NONE;
        }
        if (hold) return NONE;
        if (!due(now) && !(scan_failed && (int32_t)(failed_at - _started) >= (int32_t)SCAN_MIN_MS)) return NONE;
        if (_attempt >= MAX_ATTEMPTS) {
          _phase = FAILED;
        } else {
          _phase = OFF_WAIT;
          _deadline = now + (_attempt == 1 ? GAP1_MS : GAP2_MS);
        }
        return RADIO_OFF;
      case UP:
        _retry = false;                    // credentials change while up still needs net apply
        if (connected) return NONE;
        if ((int32_t)(now - _up_since) >= (int32_t)STABLE_MS) return startAttempt(now, 1, REBEGIN);
        return startAttempt(now, _attempt < MAX_ATTEMPTS ? _attempt + 1 : MAX_ATTEMPTS, REBEGIN);
    }
    return NONE;
  }

  void retry() { _retry = true; }          // next step starts attempt 1/3 (ignored in UP)
  void reset() { _phase = IDLE; _attempt = 0; _retry = false; }

  Phase   phase() const { return _phase; }
  uint8_t attempt() const { return _attempt; }
  bool    radioWanted() const { return _phase == ATTEMPT || _phase == UP; }

private:
  Phase    _phase = IDLE;
  uint8_t  _attempt = 0;
  uint32_t _deadline = 0;
  uint32_t _up_since = 0;
  uint32_t _started = 0;       // start of the current attempt
  bool     _retry = false;

  bool due(uint32_t now) const { return (int32_t)(now - _deadline) >= 0; }
  Action startAttempt(uint32_t now, uint8_t n, Action a) {
    _phase = ATTEMPT;
    _attempt = n;
    _started = now;
    _deadline = now + ATTEMPT_MS;
    return a;
  }
};
