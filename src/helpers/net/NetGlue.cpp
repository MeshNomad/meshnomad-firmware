#if defined(ESP32) && defined(WITH_NET_SERVICES)

#include "NetGlue.h"
#include <CayenneLPP.h>
#include <helpers/sensors/LPPDataHelpers.h>

void net_fill_sensor_telemetry(SensorManager& sensors, Telemetry& t) {
  CayenneLPP lpp(160);
  lpp.reset();
  sensors.querySensors(0xFF, lpp);
  LPPReader r(lpp.getBuffer(), lpp.getSize());

  // A current monitor reports a voltage + current + power triplet per channel.
  struct { uint8_t ch; float v, a, w; } vip[3];
  int nvip = 0;
  auto vipSlot = [&](uint8_t c) -> int {
    for (int i = 0; i < nvip; i++) if (vip[i].ch == c) return i;
    if (nvip < 3) { vip[nvip] = {c, NAN, NAN, NAN}; return nvip++; }
    return -1;                                    // more than 3 channels: ignore extras
  };

  uint8_t ch, type;
  int guard = 0;
  while (r.readHeader(ch, type) && guard++ < 32) {
    float a = 0;
    int i;
    switch (type) {
      case LPP_TEMPERATURE:         r.readTemperature(a);      t.sens_temp = a; break;
      case LPP_RELATIVE_HUMIDITY:   r.readRelativeHumidity(a); t.sens_hum  = a; break;
      case LPP_BAROMETRIC_PRESSURE: r.readPressure(a);         t.sens_pres = a; break;
      case LPP_VOLTAGE: r.readVoltage(a); if ((i = vipSlot(ch)) >= 0) vip[i].v = a; break;
      case LPP_CURRENT: r.readCurrent(a); if ((i = vipSlot(ch)) >= 0) vip[i].a = a; break;
      case LPP_POWER:   r.readPower(a);   if ((i = vipSlot(ch)) >= 0) vip[i].w = a; break;
      default:          r.skipData(type);                               break;
    }
  }

  // Ascending channel order, then ch1..ch3 by position.
  for (int p = 1; p < nvip; p++) {
    auto key = vip[p];
    int q = p - 1;
    while (q >= 0 && vip[q].ch > key.ch) { vip[q + 1] = vip[q]; q--; }
    vip[q + 1] = key;
  }
  float* dst[3][3] = {
    {&t.ch1_v, &t.ch1_a, &t.ch1_w},
    {&t.ch2_v, &t.ch2_a, &t.ch2_w},
    {&t.ch3_v, &t.ch3_a, &t.ch3_w},
  };
  for (int p = 0; p < nvip; p++) {
    *dst[p][0] = vip[p].v; *dst[p][1] = vip[p].a; *dst[p][2] = vip[p].w;
  }
}

void net_apply_epoch(mesh::RTCClock& rtc, uint32_t epoch) {
  // getCurrentTimeUnique() is monotonic, so a small backward step never reuses
  // packet timestamps; a clock that ran fast can be corrected too.
  uint32_t now = rtc.getCurrentTime();
  int32_t delta = (int32_t)(epoch - now);
  if (now < 1600000000UL || (delta > -3600 && delta < 3600)) {
    rtc.setCurrentTime(epoch);
  }
}

void net_make_ids(const uint8_t* pub_key, char node_id[7], char obs_id[65]) {
  snprintf(node_id, 7, "%02x%02x%02x", pub_key[0], pub_key[1], pub_key[2]);
  for (int i = 0; i < PUB_KEY_SIZE; i++) {
    sprintf(&obs_id[i * 2], "%02x", pub_key[i]);
  }
  obs_id[PUB_KEY_SIZE * 2] = 0;
}

#endif // ESP32 && WITH_NET_SERVICES
