#include <gtest/gtest.h>
#include "helpers/net/WifiGuard.h"

using A = WifiGuard::Action;

// Runs the guard with a link that is up and a probe that answers `ok`, until it asks for
// something other than a probe or `for_ms` passes (wrap-safe). Returns that action.
static A runUntil(WifiGuard& g, uint32_t& now, uint32_t for_ms, bool ok) {
  for (uint32_t t = 0; t < for_ms; t += 500, now += 500) {
    A a = g.tick(true, true, now);
    if (a == A::PROBE) { g.probeResult(ok, now + 2000); continue; }
    if (a != A::NONE) return a;
  }
  return A::NONE;
}

TEST(WifiGuard, HealthyLinkOnlyProbes) {
  WifiGuard g;
  uint32_t now = 1000;
  EXPECT_EQ(runUntil(g, now, 600000, true), A::NONE);   // 10 min
  EXPECT_GE(g.probes(), 18u);
  EXPECT_EQ(g.failures(), 0u);
  EXPECT_EQ(g.reconnects(), 0u);
}

TEST(WifiGuard, FirstProbeWaitsForTheLinkToSettle) {
  WifiGuard g;
  EXPECT_EQ(g.tick(true, true, 1000), A::NONE);
  EXPECT_EQ(g.tick(true, true, 1000 + WifiGuard::SETTLE_MS - 1), A::NONE);
  EXPECT_EQ(g.tick(true, true, 1000 + WifiGuard::SETTLE_MS), A::PROBE);
}

TEST(WifiGuard, DeadLinkReconnectsTwiceThenRestarts) {
  WifiGuard g;
  uint32_t now = 1000;
  EXPECT_EQ(runUntil(g, now, 2000000, false), A::RECONNECT);
  EXPECT_EQ(g.failures(), (uint32_t)WifiGuard::FAILS_TO_ACT);
  uint32_t first = now;
  EXPECT_LE(first, 1000 + WifiGuard::SETTLE_MS + WifiGuard::FAILS_TO_ACT * WifiGuard::PROBE_EVERY_MS + 5000);
  g.tick(true, false, now);                     // the reconnect drops the link for a moment
  now += 3000;
  EXPECT_EQ(runUntil(g, now, 2000000, false), A::RECONNECT);
  g.tick(true, false, now);
  now += 3000;
  EXPECT_EQ(runUntil(g, now, 2000000, false), A::RESTART);
  EXPECT_EQ(g.reconnects(), 2u);
  EXPECT_LT(now - 1000, 6u * 60u * 1000u);      // restart within 6 minutes of a dead link
}

TEST(WifiGuard, OneGoodProbeForgetsTheFailures) {
  WifiGuard g;
  uint32_t now = 1000;
  // two failed probes, then a good one: no reconnect
  int fails = 0;
  for (; now < 600000 && fails < 2; now += 500) {
    if (g.tick(true, true, now) == A::PROBE) { g.probeResult(false, now + 2000); fails++; }
  }
  EXPECT_EQ(runUntil(g, now, 300000, true), A::NONE);
  EXPECT_EQ(g.reconnects(), 0u);
}

TEST(WifiGuard, RecoveryAfterAReconnectResetsTheEscalation) {
  WifiGuard g;
  uint32_t now = 1000;
  EXPECT_EQ(runUntil(g, now, 2000000, false), A::RECONNECT);
  g.tick(true, false, now);
  now += 3000;
  EXPECT_EQ(runUntil(g, now, 300000, true), A::NONE);   // the reconnect helped
  EXPECT_EQ(runUntil(g, now, 2000000, false), A::RECONNECT);  // a new dead spell starts over
}

TEST(WifiGuard, OffOrLinkDownDoesNothing) {
  WifiGuard g;
  for (uint32_t now = 0; now < 600000; now += 500) {
    EXPECT_EQ(g.tick(false, true, now), A::NONE);
    EXPECT_EQ(g.tick(true, false, now), A::NONE);
  }
  EXPECT_EQ(g.probes(), 0u);
}

TEST(WifiGuard, NoAnswerToAProbeCountsAsFailure) {
  WifiGuard g;
  g.tick(true, true, 1000);                       // link up, settling
  uint32_t now = 1000 + WifiGuard::SETTLE_MS;
  ASSERT_EQ(g.tick(true, true, now), A::PROBE);
  // the probe never reports back: after the probe timeout it is a failure
  EXPECT_EQ(g.tick(true, true, now + WifiGuard::PROBE_TIMEOUT_MS - 1), A::NONE);
  g.tick(true, true, now + WifiGuard::PROBE_TIMEOUT_MS);
  EXPECT_EQ(g.failures(), 1u);
}

TEST(WifiGuard, MillisWrap) {
  WifiGuard g;
  uint32_t now = 0xFFFF0000u;
  EXPECT_EQ(runUntil(g, now, 600000, true), A::NONE);   // crosses the wrap
  EXPECT_GE(g.probes(), 18u);
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
