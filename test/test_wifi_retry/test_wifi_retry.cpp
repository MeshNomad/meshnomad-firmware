#include <gtest/gtest.h>
#include "helpers/net/WifiRetry.h"

typedef WifiRetry W;

// Drives a fresh instance through attempts 1..n without a connection, ending right
// after the RADIO_ON of attempt n. Returns that start time.
static uint32_t toAttempt(W& w, int n) {
  uint32_t t = 0;
  EXPECT_EQ(w.step(t, true, false, false), W::RADIO_ON);
  if (n >= 2) {
    t = 20000;  EXPECT_EQ(w.step(t, true, false, false), W::RADIO_OFF);
    t = 35000;  EXPECT_EQ(w.step(t, true, false, false), W::RADIO_ON);
  }
  if (n >= 3) {
    t = 55000;  EXPECT_EQ(w.step(t, true, false, false), W::RADIO_OFF);
    t = 85000;  EXPECT_EQ(w.step(t, true, false, false), W::RADIO_ON);
  }
  EXPECT_EQ(w.phase(), W::ATTEMPT);
  EXPECT_EQ(w.attempt(), n);
  return t;
}

TEST(WifiRetry, StartsAttemptWhenWanted) {
  W w;
  EXPECT_EQ(w.step(0, true, false, false), W::RADIO_ON);
  EXPECT_EQ(w.phase(), W::ATTEMPT);
  EXPECT_EQ(w.attempt(), 1);
  EXPECT_TRUE(w.radioWanted());
}

TEST(WifiRetry, NoRadioWithoutWant) {
  W w;
  EXPECT_EQ(w.step(0, false, false, false), W::NONE);
  EXPECT_EQ(w.phase(), W::IDLE);
  EXPECT_FALSE(w.radioWanted());
}

TEST(WifiRetry, ThreeAttemptsThenFailed) {
  W w;
  EXPECT_EQ(w.step(0, true, false, false), W::RADIO_ON);
  EXPECT_EQ(w.step(20000, true, false, false), W::RADIO_OFF);
  EXPECT_EQ(w.phase(), W::OFF_WAIT);
  EXPECT_EQ(w.attempt(), 1);
  EXPECT_FALSE(w.radioWanted());
  EXPECT_EQ(w.step(34999, true, false, false), W::NONE);
  EXPECT_EQ(w.step(35000, true, false, false), W::RADIO_ON);
  EXPECT_EQ(w.attempt(), 2);
  EXPECT_EQ(w.step(55000, true, false, false), W::RADIO_OFF);
  EXPECT_EQ(w.phase(), W::OFF_WAIT);
  EXPECT_EQ(w.attempt(), 2);
  EXPECT_EQ(w.step(84999, true, false, false), W::NONE);
  EXPECT_EQ(w.step(85000, true, false, false), W::RADIO_ON);
  EXPECT_EQ(w.attempt(), 3);
  EXPECT_EQ(w.step(105000, true, false, false), W::RADIO_OFF);
  EXPECT_EQ(w.phase(), W::FAILED);
  EXPECT_EQ(w.attempt(), 3);
  EXPECT_EQ(w.step(10000000, true, false, false), W::NONE);
  EXPECT_EQ(w.phase(), W::FAILED);
}

TEST(WifiRetry, NoActionBeforeDeadline) {
  W w;
  w.step(0, true, false, false);
  EXPECT_EQ(w.step(19999, true, false, false), W::NONE);
  EXPECT_EQ(w.phase(), W::ATTEMPT);
}

TEST(WifiRetry, DropBeforeStableCountsAsNextAttempt) {
  W w;
  toAttempt(w, 2);
  EXPECT_EQ(w.step(40000, true, true, false), W::NONE);
  EXPECT_EQ(w.phase(), W::UP);
  EXPECT_EQ(w.step(69999, true, false, false), W::REBEGIN);
  EXPECT_EQ(w.phase(), W::ATTEMPT);
  EXPECT_EQ(w.attempt(), 3);
}

TEST(WifiRetry, DropAfterStableStartsOver) {
  W w;
  toAttempt(w, 2);
  w.step(40000, true, true, false);
  EXPECT_EQ(w.step(70000, true, false, false), W::REBEGIN);   // no connected step in between
  EXPECT_EQ(w.attempt(), 1);
}

TEST(WifiRetry, FlappingAtThirdAttemptFails) {
  W w;
  toAttempt(w, 3);
  w.step(90000, true, true, false);
  EXPECT_EQ(w.step(95000, true, false, false), W::REBEGIN);
  EXPECT_EQ(w.attempt(), 3);
  EXPECT_EQ(w.step(114999, true, false, false), W::NONE);
  EXPECT_EQ(w.step(115000, true, false, false), W::RADIO_OFF);
  EXPECT_EQ(w.phase(), W::FAILED);
}

TEST(WifiRetry, DropAtThirdAttemptWhileHold) {
  W w;
  toAttempt(w, 3);
  w.step(90000, true, true, true);
  EXPECT_EQ(w.step(95000, true, false, true), W::REBEGIN);
  EXPECT_EQ(w.step(115000, true, false, true), W::NONE);
  EXPECT_EQ(w.phase(), W::ATTEMPT);
  EXPECT_EQ(w.step(116000, true, false, false), W::RADIO_OFF);
  EXPECT_EQ(w.phase(), W::FAILED);
}

TEST(WifiRetry, HoldDelaysRadioOff) {
  W w;
  w.step(0, true, false, false);
  EXPECT_EQ(w.step(20000, true, false, true), W::NONE);
  EXPECT_EQ(w.phase(), W::ATTEMPT);
  EXPECT_EQ(w.step(25000, true, false, false), W::RADIO_OFF);
  EXPECT_EQ(w.phase(), W::OFF_WAIT);
}

TEST(WifiRetry, ConnectedIgnoredWhenRadioOff) {
  W w;
  w.step(0, true, false, false);
  w.step(20000, true, false, false);                           // OFF_WAIT
  EXPECT_EQ(w.step(21000, true, true, false), W::NONE);
  EXPECT_EQ(w.phase(), W::OFF_WAIT);
  W f;
  toAttempt(f, 3);
  f.step(105000, true, false, false);                          // FAILED
  EXPECT_EQ(f.step(106000, true, true, false), W::NONE);
  EXPECT_EQ(f.phase(), W::FAILED);
}

TEST(WifiRetry, RetryFromFailed) {
  W w;
  toAttempt(w, 3);
  w.step(105000, true, false, false);
  w.retry();
  EXPECT_EQ(w.step(200000, true, false, false), W::RADIO_ON);
  EXPECT_EQ(w.phase(), W::ATTEMPT);
  EXPECT_EQ(w.attempt(), 1);
  EXPECT_EQ(w.step(200001, true, false, false), W::NONE);      // flag consumed
}

TEST(WifiRetry, RetryFromOffWait) {
  W w;
  w.step(0, true, false, false);
  w.step(20000, true, false, false);
  w.retry();
  EXPECT_EQ(w.step(21000, true, false, false), W::RADIO_ON);
  EXPECT_EQ(w.attempt(), 1);
}

TEST(WifiRetry, RetryInAttemptRestarts) {
  W w;
  toAttempt(w, 2);                                             // started at 35000
  w.retry();
  EXPECT_EQ(w.step(36000, true, false, false), W::REBEGIN);
  EXPECT_EQ(w.attempt(), 1);
  EXPECT_EQ(w.step(55999, true, false, false), W::NONE);
  EXPECT_EQ(w.step(56000, true, false, false), W::RADIO_OFF);
}

TEST(WifiRetry, RetryIgnoredWhenUp) {
  W w;
  toAttempt(w, 2);
  w.step(40000, true, true, false);
  w.retry();
  EXPECT_EQ(w.step(41000, true, true, false), W::NONE);
  EXPECT_EQ(w.phase(), W::UP);
  EXPECT_EQ(w.step(42000, true, false, false), W::REBEGIN);   // normal drop path
  EXPECT_EQ(w.attempt(), 3);
  EXPECT_EQ(w.step(43000, true, false, false), W::NONE);      // no stale retry
}

TEST(WifiRetry, WantFalseFromAnyPhaseGoesIdle) {
  W a;
  a.step(0, true, false, false);
  a.step(20000, true, false, false);                           // OFF_WAIT
  EXPECT_EQ(a.step(21000, false, false, false), W::RADIO_OFF);
  EXPECT_EQ(a.phase(), W::IDLE);
  EXPECT_EQ(a.attempt(), 0);

  W u;
  u.step(0, true, false, false);
  u.step(1000, true, true, false);                             // UP
  EXPECT_EQ(u.step(2000, false, true, false), W::RADIO_OFF);
  EXPECT_EQ(u.phase(), W::IDLE);

  W f;
  toAttempt(f, 3);
  f.step(105000, true, false, false);                          // FAILED
  f.retry();
  EXPECT_EQ(f.step(106000, false, false, false), W::RADIO_OFF);
  EXPECT_EQ(f.phase(), W::IDLE);
  EXPECT_EQ(f.step(107000, true, false, false), W::RADIO_ON);
  EXPECT_EQ(f.attempt(), 1);
  EXPECT_EQ(f.step(108000, true, false, false), W::NONE);      // retry flag gone
}

TEST(WifiRetry, ResetGoesIdle) {
  W w;
  toAttempt(w, 3);
  w.step(105000, true, false, false);
  w.retry();
  w.reset();
  EXPECT_EQ(w.phase(), W::IDLE);
  EXPECT_EQ(w.attempt(), 0);
  EXPECT_EQ(w.step(106000, true, false, false), W::RADIO_ON);
  EXPECT_EQ(w.step(107000, true, false, false), W::NONE);      // flag cleared by reset()
}

TEST(WifiRetry, ScanFailEndsAttemptEarly) {
  W w;
  w.step(0, true, false, false);
  EXPECT_EQ(w.step(3000, true, false, false, true, 3000), W::RADIO_OFF);
  EXPECT_EQ(w.phase(), W::OFF_WAIT);
  EXPECT_EQ(w.attempt(), 1);
  EXPECT_EQ(w.step(17999, true, false, false), W::NONE);       // gap counts from the early end
  EXPECT_EQ(w.step(18000, true, false, false), W::RADIO_ON);
  EXPECT_EQ(w.attempt(), 2);
}

TEST(WifiRetry, ScanFailIgnoredInFirstSecond) {
  W w;
  w.step(0, true, false, false);
  EXPECT_EQ(w.step(999, true, false, false, true, 999), W::NONE);   // stale event from the last scan
  EXPECT_EQ(w.phase(), W::ATTEMPT);
  EXPECT_EQ(w.step(1000, true, false, false, true, 1000), W::RADIO_OFF);
}

TEST(WifiRetry, StaleFailureStaysIgnored) {
  W w;
  w.step(0, true, false, false);
  EXPECT_EQ(w.step(1500, true, false, false, true, 500), W::NONE);   // reported at 0.5 s: old scan
  EXPECT_EQ(w.phase(), W::ATTEMPT);
  EXPECT_EQ(w.step(2500, true, false, false, true, 2400), W::RADIO_OFF);
}

TEST(WifiRetry, ScanFailWithHoldWaits) {
  W w;
  w.step(0, true, false, false);
  EXPECT_EQ(w.step(3000, true, false, true, true, 3000), W::NONE);
  EXPECT_EQ(w.step(4000, true, false, false, true, 4000), W::RADIO_OFF);
}

TEST(WifiRetry, ConnectedWinsOverScanFail) {
  W w;
  w.step(0, true, false, false);
  EXPECT_EQ(w.step(3000, true, true, false, true, 3000), W::NONE);
  EXPECT_EQ(w.phase(), W::UP);
}

TEST(WifiRetry, ScanFailOnEveryAttemptFailsQuickly) {
  W w;
  w.step(0, true, false, false);
  EXPECT_EQ(w.step(3000, true, false, false, true, 3000), W::RADIO_OFF);   // 1/3
  EXPECT_EQ(w.step(18000, true, false, false), W::RADIO_ON);
  EXPECT_EQ(w.step(21000, true, false, false, true, 21000), W::RADIO_OFF);  // 2/3
  EXPECT_EQ(w.step(51000, true, false, false), W::RADIO_ON);
  EXPECT_EQ(w.step(54000, true, false, false, true, 54000), W::RADIO_OFF);  // 3/3
  EXPECT_EQ(w.phase(), W::FAILED);
}

TEST(WifiRetry, ScanFailAfterRebeginNeedsItsOwnSecond) {
  W w;
  toAttempt(w, 2);                                             // started at 35000
  w.retry();
  EXPECT_EQ(w.step(36000, true, false, false), W::REBEGIN);
  EXPECT_EQ(w.step(36500, true, false, false, true, 36500), W::NONE);
  EXPECT_EQ(w.step(37000, true, false, false, true, 37000), W::RADIO_OFF);
}

TEST(WifiRetry, MillisWrap) {
  W w;
  const uint32_t t0 = 0xFFFFF000u;
  w.step(t0, true, false, false);
  EXPECT_EQ(w.step(t0 + 19999u, true, false, false), W::NONE);   // wraps past 0
  EXPECT_EQ(w.step(t0 + 20000u, true, false, false), W::RADIO_OFF);
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
