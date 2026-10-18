#pragma once
// Small entry points into the network layer for role code (MyMesh), so it does
// not need the full NetServices header (WiFi.h / PubSubClient). Defined in
// NetServices.cpp; both are safe no-ops until NetServices::begin() has run.

#include <stddef.h>
#include <stdint.h>

// Returns true if `command` belongs to the network layer and `reply` was filled;
// the caller then stops and does not pass the command to CommonCLI.
bool net_handle_cli(const char* command, char* reply, size_t reply_len);

// Every received packet (raw bytes + RF metrics) for the MQTT observer. Never
// blocks; ignored unless the observer is switched on.
void net_observer_rx_raw(float snr, float rssi, const uint8_t* raw, int len);
