#include <gtest/gtest.h>
#include <string.h>
#include "helpers/net/NetWifiToken.h"

static const char* token(bool sta_started, bool connected, int wl) {
  static char buf[24];
  netExternalWifiToken(buf, sizeof(buf), sta_started, connected, wl);
  return buf;
}

TEST(NetWifiToken, OffWhenStationNotStarted) {
  EXPECT_STREQ(token(false, false, 255), "off");   // core 2.x: WL_NO_SHIELD
  EXPECT_STREQ(token(false, false, 254), "off");   // core 3.x: WL_STOPPED
}

TEST(NetWifiToken, ConnectedWhenUp) {
  EXPECT_STREQ(token(true, true, 3), "connected");
}

TEST(NetWifiToken, ConnectingWhileStartedAndNotConnected) {
  EXPECT_STREQ(token(true, false, 6), "connecting(wl=6)");   // lost its AP
  EXPECT_STREQ(token(true, false, 1), "connecting(wl=1)");   // AP not in range
  EXPECT_STREQ(token(true, false, 255), "connecting(wl=255)");   // begin() called, STA_START not yet seen
}

TEST(NetWifiToken, ConnectedWinsOverModeRead) {
  EXPECT_STREQ(token(false, true, 3), "connected");
}

TEST(NetWifiToken, FitsTheStatusBuffer) {
  EXPECT_LT(strlen(token(true, false, -32768)), (size_t)24);
}

TEST(NetWifiToken, NoSpaceInToken) {
  EXPECT_EQ(strchr(token(false, false, 255), ' '), nullptr);
  EXPECT_EQ(strchr(token(true, false, 6), ' '), nullptr);
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
