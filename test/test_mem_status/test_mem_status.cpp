#include <gtest/gtest.h>
#include <string.h>
#include "helpers/MemStatus.h"

TEST(MemStatus, Format) {
  MemSample m;
  m.internal = 31208; m.low = 9732; m.block = 20468; m.psram = 2074475;
  char out[96];
  memStatusFormat(out, sizeof(out), m);
  EXPECT_STREQ(out, "internal:31208 low:9732 block:20468 psram:2074475");
}

TEST(MemStatus, NoPsramAndMaxValuesFitTheReply) {
  MemSample m;
  m.internal = m.low = m.block = 4294967295u;
  char out[96];
  int n = memStatusFormat(out, sizeof(out), m);
  EXPECT_LT(n, 90);   // "> " prefix + reply buffer of the CLI
  EXPECT_NE(strstr(out, "psram:0"), nullptr);
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
