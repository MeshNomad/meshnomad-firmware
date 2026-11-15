#pragma once
// Role-independent helpers shared by every role that uses NetServices.

#include <MeshCore.h>
#include <helpers/SensorManager.h>
#include "NetServices.h"

// Live-reads all telemetry sensors and fills the optional Telemetry fields:
// temperature / humidity / pressure by type; voltage / current / power grouped
// per sensor channel and mapped to ch1..ch3 in ascending channel order.
void net_fill_sensor_telemetry(SensorManager& sensors, Telemetry& t);

// Applies an NTP epoch to the RTC (rules: NetEpoch.h): always on a first boot (clock not
// set yet) and when a server reply arrived in this boot (from_server), then backwards
// too; otherwise only within ±3600 s of the current time.
void net_apply_epoch(mesh::RTCClock& rtc, uint32_t epoch, bool from_server);

// node_id = first 3 public-key bytes as hex (MQTT topics); obs_id = full key hex.
void net_make_ids(const uint8_t* pub_key, char node_id[7], char obs_id[65]);
