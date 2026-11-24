#include <gtest/gtest.h>
#include "helpers/LoginLockout.h"

static const uint32_t IP_A = 0x0A01A8C0;   // 192.168.1.10
static const uint32_t IP_B = 0x0B01A8C0;

TEST(LoginLockout, ThreeFailuresLockTheIp) {
  LoginLockout l;
  EXPECT_FALSE(l.isLocked(IP_A, 1000));
  l.fail(IP_A, 1000);
  l.fail(IP_A, 2000);
  EXPECT_FALSE(l.isLocked(IP_A, 2000));
  l.fail(IP_A, 3000);
  EXPECT_TRUE(l.isLocked(IP_A, 3000));
  EXPECT_FALSE(l.isLocked(IP_B, 3000));   // other addresses unaffected
  EXPECT_EQ(l.lockedCount(3000), 1);
  EXPECT_EQ(l.failsSinceBoot(), 3u);
}

TEST(LoginLockout, ExpiresAfterFiveMinutes) {
  LoginLockout l;
  for (int i = 0; i < 3; i++) l.fail(IP_A, 1000);
  EXPECT_TRUE(l.isLocked(IP_A, 1000 + LoginLockout::LOCKOUT_MS - 1));
  EXPECT_FALSE(l.isLocked(IP_A, 1000 + LoginLockout::LOCKOUT_MS));
  EXPECT_EQ(l.lockedCount(1000 + LoginLockout::LOCKOUT_MS), 0);
  l.fail(IP_A, 1000 + LoginLockout::LOCKOUT_MS + 1);   // counting starts again
  EXPECT_FALSE(l.isLocked(IP_A, 1000 + LoginLockout::LOCKOUT_MS + 1));
}

TEST(LoginLockout, SuccessForgetsFailures) {
  LoginLockout l;
  l.fail(IP_A, 1000);
  l.fail(IP_A, 1000);
  l.ok(IP_A);
  l.fail(IP_A, 1000);
  EXPECT_FALSE(l.isLocked(IP_A, 1000));
}

TEST(LoginLockout, FullTableReusesASlot) {
  LoginLockout l;
  for (uint32_t i = 0; i < (uint32_t)LoginLockout::SLOTS; i++) l.fail(100 + i, 1000 + i);
  for (int i = 0; i < 3; i++) l.fail(IP_B, 5000);
  EXPECT_TRUE(l.isLocked(IP_B, 5000));
}

TEST(LoginLockout, ClockWrapStillExpires) {
  LoginLockout l;
  uint32_t t = 0xFFFFFF00UL;
  for (int i = 0; i < 3; i++) l.fail(IP_A, t);
  EXPECT_TRUE(l.isLocked(IP_A, t + 100));                     // wrapped past 0
  EXPECT_FALSE(l.isLocked(IP_A, t + LoginLockout::LOCKOUT_MS));
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
