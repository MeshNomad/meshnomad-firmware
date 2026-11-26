#include <gtest/gtest.h>
#include "helpers/SerialFrameGuard.h"

using namespace mesh;

TEST(SerialFrameGuard, LengthLimits) {
  EXPECT_FALSE(serialFrameLengthOk(0, 176));
  EXPECT_TRUE(serialFrameLengthOk(1, 176));
  EXPECT_TRUE(serialFrameLengthOk(176, 176));
  EXPECT_FALSE(serialFrameLengthOk(177, 176));
  EXPECT_FALSE(serialFrameLengthOk(65535, 176));   // the cut frame 3C FF FF
}

TEST(SerialFrameGuard, GapOfOneSecondEndsAFrame) {
  EXPECT_FALSE(serialFrameStale(10999, 10000));
  EXPECT_TRUE(serialFrameStale(11000, 10000));
  EXPECT_FALSE(serialFrameStale(10000, 10000));
}

TEST(SerialFrameGuard, MillisWrap) {
  const uint32_t before_wrap = 0xFFFFFF00u;   // 256 ms before the wrap
  EXPECT_FALSE(serialFrameStale(before_wrap + 999, before_wrap));   // wraps past 0
  EXPECT_TRUE(serialFrameStale(before_wrap + 1000, before_wrap));
  EXPECT_FALSE(serialFrameStale(0x00000010u, before_wrap));          // 272 ms later, after the wrap
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
