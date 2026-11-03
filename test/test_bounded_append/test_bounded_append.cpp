#include <gtest/gtest.h>
#include <string.h>
#include "helpers/net/BoundedAppend.h"

TEST(AppendF, AppendsAndReturnsNewLength) {
  char b[16];
  int len = appendf(b, sizeof(b), 0, "%s", "abc");
  len = appendf(b, sizeof(b), len, "%02X", 0x5a);
  EXPECT_EQ(len, 5);
  EXPECT_STREQ(b, "abc5A");
}

TEST(AppendF, ExactFitIsNotTruncated) {
  char b[6];
  int len = appendf(b, sizeof(b), 0, "%s", "abcde");   // 5 chars + NUL
  EXPECT_EQ(len, 5);
  EXPECT_STREQ(b, "abcde");
}

TEST(AppendF, OverflowReturnsCapacityAndStaysInBuffer) {
  char b[8 + 4];
  memset(b, 'Z', sizeof(b));
  int len = appendf(b, 8, 0, "%s", "abcdefghij");
  EXPECT_EQ(len, 8);                                    // truncated: len == n
  EXPECT_EQ(b[7], 0);                                   // terminated inside the buffer
  EXPECT_EQ(memcmp(b + 8, "ZZZZ", 4), 0);               // nothing written past n
}

TEST(AppendF, TruncationIsSticky) {
  char b[8 + 4];
  memset(b, 'Z', sizeof(b));
  int len = appendf(b, 8, 0, "%s", "abcdefghij");
  len = appendf(b, 8, len, "%s", "xy");
  EXPECT_EQ(len, 8);
  EXPECT_EQ(memcmp(b + 8, "ZZZZ", 4), 0);
}

TEST(AppendF, OutOfRangeStartWritesNothing) {
  char b[8 + 4];
  memset(b, 'Z', sizeof(b));
  EXPECT_EQ(appendf(b, 8, 20, "%s", "x"), 8);
  EXPECT_EQ(appendf(b, 8, -1, "%s", "x"), 8);
  EXPECT_EQ(memcmp(b, "ZZZZZZZZZZZZ", 12), 0);
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
