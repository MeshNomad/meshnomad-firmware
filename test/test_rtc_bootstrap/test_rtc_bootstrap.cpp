#include <gtest/gtest.h>
#include "helpers/RtcBootstrap.h"

TEST(RtcBootstrap, WantedWhenClockIsAtTheSeed) {
  EXPECT_TRUE(rtcBootstrapWanted(1772323200UL));          // ESP32RTCClock seed
  EXPECT_TRUE(rtcBootstrapWanted(1772323200UL + 30));     // seed plus the boot time
  EXPECT_TRUE(rtcBootstrapWanted(1715770351UL));          // VolatileRTCClock base
  EXPECT_TRUE(rtcBootstrapWanted(946684800UL));           // hardware RTC with an empty battery (2000)
}

TEST(RtcBootstrap, NotWantedWhenClockIsSet) {
  EXPECT_FALSE(rtcBootstrapWanted(1794700000UL));         // Nov 2026
  EXPECT_FALSE(rtcBootstrapWanted(3689060000UL));         // a clock in 2086 is not replaced either
}

TEST(RtcBootstrap, Boundary) {
  EXPECT_TRUE(rtcBootstrapWanted(RTC_UNSET_BELOW - 1));
  EXPECT_FALSE(rtcBootstrapWanted(RTC_UNSET_BELOW));
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
