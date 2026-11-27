#include <gtest/gtest.h>
#include <string.h>
#include "helpers/net/NetNtpSync.h"

// The longest list an ESP32 repeater can carry.
#define WITH_NET_SERVICES 1
#define ESP32 1
#define WITH_BRIDGE 1
#define ENV_INCLUDE_GPS 1
#include "helpers/MeshNomadFeatures.h"

TEST(NetNtpSync, RefusedWhenNtpOff) {
  EXPECT_EQ(netNtpSyncCheck(false, true), NTP_SYNC_OFF);
  EXPECT_EQ(netNtpSyncCheck(false, false), NTP_SYNC_OFF);   // the setting is reported first
}

TEST(NetNtpSync, RefusedWithoutWifi) {
  EXPECT_EQ(netNtpSyncCheck(true, false), NTP_SYNC_NO_WIFI);
}

TEST(NetNtpSync, StartsWhenEnabledAndConnected) {
  EXPECT_EQ(netNtpSyncCheck(true, true), NTP_SYNC_START);
}

TEST(NetNtpSync, RefusalsStartWithError) {
  EXPECT_EQ(strncmp(netNtpSyncReply(NTP_SYNC_OFF), "Error", 5), 0);
  EXPECT_EQ(strncmp(netNtpSyncReply(NTP_SYNC_NO_WIFI), "Error", 5), 0);
}

TEST(NetNtpSync, Replies) {
  EXPECT_STREQ(netNtpSyncReply(NTP_SYNC_OFF), "Error: NTP off, set ntp.enabled 1 first");
  EXPECT_STREQ(netNtpSyncReply(NTP_SYNC_NO_WIFI), "Error: WiFi not connected");
  EXPECT_STREQ(netNtpSyncReply(NTP_SYNC_START), "> ntp sync started");
}

TEST(NetNtpSync, FeatureTokenFollowsNtp) {
  char list[96];
  meshnomadFeatures(list, sizeof(list));
  EXPECT_NE(strstr(list, " ntp ntp.sync "), nullptr) << list;
  EXPECT_STREQ(list, "wifi mem mqtt ntp ntp.sync observer tls ota.net obsup bridge gps");
  EXPECT_LT(strlen(list), sizeof(list) - 1);
}

// B85: a boot starts with a clock restored from before, so the first pass applies it without a
// server reply; the server's time must then be applied when its first reply comes, not one
// ntp.interval (25 h by default) later.
TEST(NetNtpSync, FirstServerReplyIsAppliedAtOnce) {
  EXPECT_TRUE(netNtpApplyDue(true, 0, 90000000UL));
  EXPECT_TRUE(netNtpApplyDue(true, 5000, 90000000UL));
}

TEST(NetNtpSync, WithoutANewReplyOnlyTheIntervalApplies) {
  EXPECT_FALSE(netNtpApplyDue(false, 0, 90000000UL));
  EXPECT_FALSE(netNtpApplyDue(false, 89999999UL, 90000000UL));
  EXPECT_TRUE(netNtpApplyDue(false, 90000000UL, 90000000UL));
  EXPECT_TRUE(netNtpApplyDue(false, 90000001UL, 90000000UL));
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
