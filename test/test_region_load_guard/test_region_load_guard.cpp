#include <gtest/gtest.h>
#include "helpers/RegionLoadGuard.h"

using namespace mesh;

TEST(RegionLoadGuard, RecognisesTheStartCommand) {
  EXPECT_TRUE(regionLoadIsStart("region load"));
  EXPECT_TRUE(regionLoadIsStart("region load nl F"));
  EXPECT_TRUE(regionLoadIsStart("region  load"));
  EXPECT_TRUE(regionLoadIsStart("  region load"));
}

TEST(RegionLoadGuard, OtherRegionCommandsAreNotTheStart) {
  EXPECT_FALSE(regionLoadIsStart("region"));
  EXPECT_FALSE(regionLoadIsStart("region save"));
  EXPECT_FALSE(regionLoadIsStart("region loadx"));
  EXPECT_FALSE(regionLoadIsStart("region put load"));
  EXPECT_FALSE(regionLoadIsStart("regions load"));
  EXPECT_FALSE(regionLoadIsStart(""));
}

TEST(RegionLoadGuard, AppPrefixIsNotARegionLine) {
  EXPECT_TRUE(regionLoadHasPrefix("01|ver"));
  EXPECT_TRUE(regionLoadHasPrefix("  7f|reboot"));
  EXPECT_TRUE(regionLoadHasPrefix("01| "));         // the app's "blank" line
  EXPECT_FALSE(regionLoadHasPrefix("pl F"));
  EXPECT_FALSE(regionLoadHasPrefix(" gdansk"));
  EXPECT_FALSE(regionLoadHasPrefix(""));
  EXPECT_TRUE(regionLoadHasPrefix("01|"));
  EXPECT_FALSE(regionLoadHasPrefix("a|"));
  EXPECT_FALSE(regionLoadHasPrefix("|"));
}

TEST(RegionLoadGuard, LocalLinesAreDefinitions) {
  EXPECT_EQ(regionLoadLine(false, false, 0), REGION_LOAD_DEFINITION);
  EXPECT_EQ(regionLoadLine(false, false, REGION_LOAD_IDLE_MILLIS - 1), REGION_LOAD_DEFINITION);
}

TEST(RegionLoadGuard, RemoteLinesAreNeverSwallowed) {
  EXPECT_EQ(regionLoadLine(true, false, 0), REGION_LOAD_PASS);
  EXPECT_EQ(regionLoadLine(true, true, 0), REGION_LOAD_PASS);
  EXPECT_EQ(regionLoadLine(true, false, REGION_LOAD_IDLE_MILLIS), REGION_LOAD_PASS);
}

TEST(RegionLoadGuard, PrefixedLineOrSilenceEndsTheLoad) {
  EXPECT_EQ(regionLoadLine(false, true, 0), REGION_LOAD_ABORT);
  EXPECT_EQ(regionLoadLine(false, false, REGION_LOAD_IDLE_MILLIS), REGION_LOAD_ABORT);
  EXPECT_EQ(regionLoadLine(false, false, 0xFFFFFFFFUL), REGION_LOAD_ABORT);
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
