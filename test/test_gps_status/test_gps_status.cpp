#include <gtest/gtest.h>

#include <helpers/GpsStatus.h>

TEST(GpsStatus, NoModuleDetectedDoesNotDereferenceMissingSetting) {
  char reply[64];

  mesh::formatGpsStatus(reply, sizeof(reply), true, NULL, false, 0);

  EXPECT_STREQ("off, no module", reply);
}

TEST(GpsStatus, NoModuleWinsOverEnablePin) {
  char reply[64];

  mesh::formatGpsStatus(reply, sizeof(reply), false, NULL, false, 0);

  EXPECT_STREQ("off, no module", reply);
}

TEST(GpsStatus, ActiveWithFixKeepsUpstreamFormat) {
  char reply[64];

  mesh::formatGpsStatus(reply, sizeof(reply), true, "1", true, 7);

  EXPECT_STREQ("on, active, fix, 7 sats", reply);
}

TEST(GpsStatus, DeactivatedWithoutFixKeepsUpstreamFormat) {
  char reply[64];

  mesh::formatGpsStatus(reply, sizeof(reply), true, "0", false, 0);

  EXPECT_STREQ("on, deactivated, no fix, 0 sats", reply);
}

TEST(GpsStatus, EnablePinOffWithModuleReportsOff) {
  char reply[64];

  mesh::formatGpsStatus(reply, sizeof(reply), false, "1", false, 0);

  EXPECT_STREQ("off", reply);
}

TEST(GpsStatus, TruncatesToReplyBuffer) {
  char reply[8];

  mesh::formatGpsStatus(reply, sizeof(reply), true, "1", true, 12);

  EXPECT_STREQ("on, act", reply);
}

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
