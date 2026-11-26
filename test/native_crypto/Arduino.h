#pragma once
// Native stand-in for Arduino.h in the env native_tcp_crypto: only what rweather Crypto's
// RNG.cpp needs to compile. The protocol code under test never uses RNG (randomness is
// passed in), so these never run.
#include <stdint.h>
#include <string.h>
inline unsigned long millis() { return 0; }
inline unsigned long micros() { return 0; }
