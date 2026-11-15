#pragma once

// MQTT broker TCP connect without blocking loop(): the socket is opened non-blocking and
// polled once per pass. This is the decision for one poll, no Arduino dependency
// (unit-tested natively). select_res: result of a zero-timeout select() for writability;
// so_error: SO_ERROR of the socket once it is writable.

#include <stdint.h>

enum MqTcpPoll { MQ_TCP_WAIT, MQ_TCP_READY, MQ_TCP_FAIL };

static const uint32_t MQ_TCP_TIMEOUT_MS = 3000;   // the same limit the blocking connect had

inline MqTcpPoll netMqttTcpPoll(int select_res, int so_error, uint32_t elapsed_ms) {
  if (select_res < 0) return MQ_TCP_FAIL;
  if (select_res > 0) return so_error == 0 ? MQ_TCP_READY : MQ_TCP_FAIL;
  return elapsed_ms >= MQ_TCP_TIMEOUT_MS ? MQ_TCP_FAIL : MQ_TCP_WAIT;
}
