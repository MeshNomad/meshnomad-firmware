#include <gtest/gtest.h>
#include "helpers/net/NetWifiPower.h"

TEST(NetWifiPower, NothingToDoWhenDriverAlreadyHasIt) {
  EXPECT_FALSE(netWifiPowerDue(0, 0));     // fresh driver, chip default wanted
  EXPECT_FALSE(netWifiPowerDue(5, 5));     // not set again on every pass
  EXPECT_FALSE(netWifiPowerDue(20, 20));
}

TEST(NetWifiPower, DueWhenSettingChanged) {
  EXPECT_TRUE(netWifiPowerDue(5, 0));      // boot with a stored value
  EXPECT_TRUE(netWifiPowerDue(17, 5));
}

TEST(NetWifiPower, WrittenAgainAtWifiStartWhenMarked) {
  EXPECT_TRUE(netWifiPowerDue(11, -1));    // repeater: a set power is written at every WiFi start
  EXPECT_FALSE(netWifiPowerDue(0, 0));     // nothing was ever set: the driver is left alone
}

TEST(NetWifiPower, BackToChipDefaultIsApplied) {
  EXPECT_TRUE(netWifiPowerDue(0, 5));      // was ignored until the next restart
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
