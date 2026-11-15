#include <gtest/gtest.h>
#include "helpers/net/NetEpoch.h"

static const uint32_t NOW = 1794700000UL;    // Nov 2026
static const uint32_t Y2086 = 3689060000UL;

TEST(NetEpoch, FirstBootTakesAnySaneEpoch) {
  EXPECT_TRUE(netEpochAccept(0, NOW, false));
  EXPECT_TRUE(netEpochAccept(1500000000UL, NOW, false));
}

TEST(NetEpoch, GarbageEpochNeverTaken) {
  EXPECT_FALSE(netEpochAccept(0, 0, true));
  EXPECT_FALSE(netEpochAccept(NOW, 1500000000UL, true));
  EXPECT_FALSE(netEpochAccept(Y2086, 86400, true));
}

TEST(NetEpoch, WithoutServerReplyOnlyWithinOneHour) {
  EXPECT_TRUE(netEpochAccept(NOW, NOW + 3599, false));
  EXPECT_TRUE(netEpochAccept(NOW, NOW - 3599, false));
  EXPECT_FALSE(netEpochAccept(NOW, NOW + 3600, false));
  EXPECT_FALSE(netEpochAccept(NOW, NOW - 3600, false));
  EXPECT_FALSE(netEpochAccept(Y2086, NOW, false));        // system time alone never steps the clock back
}

TEST(NetEpoch, ServerReplyStepsTheClockBothWays) {
  EXPECT_TRUE(netEpochAccept(Y2086, NOW, true));          // 60 years back
  EXPECT_TRUE(netEpochAccept(NOW - 7 * 86400, NOW, true));
  EXPECT_TRUE(netEpochAccept(NOW, NOW + 5, true));
}

TEST(NetEpoch, LastmodClamp) {
  EXPECT_EQ(netClampLastmod(Y2086, NOW), NOW);
  EXPECT_EQ(netClampLastmod(NOW, NOW), NOW);
  EXPECT_EQ(netClampLastmod(NOW - 100, NOW), NOW - 100);
}

TEST(NetEpoch, BigBackStep) {
  EXPECT_TRUE(netSteppedBack(Y2086, NOW));
  EXPECT_FALSE(netSteppedBack(NOW + 3600, NOW));          // within the old window: no clamp pass
  EXPECT_FALSE(netSteppedBack(NOW, NOW + 100000));
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
