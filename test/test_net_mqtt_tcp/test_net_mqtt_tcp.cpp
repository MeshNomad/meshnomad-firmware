#include <gtest/gtest.h>
#include "helpers/net/NetMqttTcp.h"

TEST(NetMqttTcp, WaitsWhileNothingHappened) {
  EXPECT_EQ(netMqttTcpPoll(0, 0, 0), MQ_TCP_WAIT);
  EXPECT_EQ(netMqttTcpPoll(0, 0, MQ_TCP_TIMEOUT_MS - 1), MQ_TCP_WAIT);
}

TEST(NetMqttTcp, GivesUpAfterTheTimeout) {
  EXPECT_EQ(netMqttTcpPoll(0, 0, MQ_TCP_TIMEOUT_MS), MQ_TCP_FAIL);
  EXPECT_EQ(netMqttTcpPoll(0, 0, 60000), MQ_TCP_FAIL);
}

TEST(NetMqttTcp, ReadyWhenWritableWithoutError) {
  EXPECT_EQ(netMqttTcpPoll(1, 0, 20), MQ_TCP_READY);
  EXPECT_EQ(netMqttTcpPoll(1, 0, 60000), MQ_TCP_READY);   // a late success still counts
}

TEST(NetMqttTcp, FailsOnSocketError) {
  EXPECT_EQ(netMqttTcpPoll(1, 111, 20), MQ_TCP_FAIL);     // refused
  EXPECT_EQ(netMqttTcpPoll(-1, 0, 20), MQ_TCP_FAIL);      // select failed
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
