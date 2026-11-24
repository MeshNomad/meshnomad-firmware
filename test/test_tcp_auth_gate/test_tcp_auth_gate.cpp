#include <gtest/gtest.h>
#include <string>
#include <vector>
#include "helpers/wifi/TcpAuthGate.h"

// Fake HMAC: records its inputs and returns a value derived from key and message, so a
// RESPONSE built with the same fake matches and any other key or nonce does not.
static std::string g_key;
static std::vector<uint8_t> g_msg;
static void fakeHmac(const uint8_t* key, size_t key_len, const uint8_t* msg, size_t msg_len,
                     uint8_t out[TCP_AUTH_HMAC_LEN]) {
  g_key.assign((const char*)key, key_len);
  g_msg.assign(msg, msg + msg_len);
  for (size_t i = 0; i < TCP_AUTH_HMAC_LEN; i++) {
    uint8_t v = (uint8_t)(i * 7);
    for (size_t k = 0; k < key_len; k++) v ^= (uint8_t)(key[k] + i + k);
    for (size_t m = 0; m < msg_len; m++) v += (uint8_t)(msg[m] * (m + 1 + i));
    out[i] = v;
  }
}

static const char* PW = "meshnomad-test";

static void nonce16(uint8_t n[TCP_AUTH_NONCE_LEN]) {
  for (int i = 0; i < TCP_AUTH_NONCE_LEN; i++) n[i] = (uint8_t)i;
}

static std::vector<uint8_t> response(const char* pw, const uint8_t nonce[TCP_AUTH_NONCE_LEN]) {
  uint8_t msg[6 + TCP_AUTH_NONCE_LEN];
  size_t mlen = tcpAuthMessage(nonce, msg);
  std::vector<uint8_t> f(2 + TCP_AUTH_HMAC_LEN);
  f[0] = TCP_AUTH_FRAME;
  f[1] = TCP_AUTH_RESPONSE;
  fakeHmac((const uint8_t*)pw, strlen(pw), msg, mlen, &f[2]);
  return f;
}

static const uint8_t HELLO[] = {TCP_AUTH_FRAME, TCP_AUTH_HELLO, TCP_AUTH_VERSION};
static const uint8_t DEVICE_QUERY[] = {22, 3};

static TcpAuthGate started() {
  TcpAuthGate g;
  uint8_t n[TCP_AUTH_NONCE_LEN];
  nonce16(n);
  g.start(n, 1000);
  return g;
}

TEST(TcpAuthGate, HelloGetsChallengeWithTheNonce) {
  TcpAuthGate g = started();
  TcpAuthAction a = g.onFrame(HELLO, sizeof(HELLO), false, PW, fakeHmac);
  ASSERT_EQ(a.kind, TcpAuthAction::SEND);
  ASSERT_EQ(a.out_len, 19u);
  EXPECT_EQ(a.out[0], 0xF0);
  EXPECT_EQ(a.out[1], 0x01);
  EXPECT_EQ(a.out[2], 0x01);
  for (int i = 0; i < 16; i++) EXPECT_EQ(a.out[3 + i], i);
  EXPECT_FALSE(a.failed_login);
}

TEST(TcpAuthGate, OtherFrameBeforeLoginGetsChallenge) {
  TcpAuthGate g = started();
  TcpAuthAction a = g.onFrame(DEVICE_QUERY, sizeof(DEVICE_QUERY), false, PW, fakeHmac);
  EXPECT_EQ(a.kind, TcpAuthAction::SEND);
  EXPECT_EQ(a.out[1], TCP_AUTH_HELLO);
  EXPECT_TRUE(g.active());
}

TEST(TcpAuthGate, RightResponseAccepts) {
  TcpAuthGate g = started();
  g.onFrame(HELLO, sizeof(HELLO), false, PW, fakeHmac);
  uint8_t n[16];
  nonce16(n);
  std::vector<uint8_t> r = response(PW, n);
  TcpAuthAction a = g.onFrame(r.data(), r.size(), false, PW, fakeHmac);
  EXPECT_EQ(a.kind, TcpAuthAction::ACCEPT);
  ASSERT_EQ(a.out_len, 3u);
  EXPECT_EQ(a.out[2], TCP_AUTH_OK);
  EXPECT_TRUE(a.login_ok);
  EXPECT_FALSE(g.active());
  EXPECT_EQ(g_key, PW);                                   // key = password
  ASSERT_EQ(g_msg.size(), 22u);                            // "MNTCP1" || nonce
  EXPECT_EQ(std::string(g_msg.begin(), g_msg.begin() + 6), "MNTCP1");
  for (int i = 0; i < 16; i++) EXPECT_EQ(g_msg[6 + i], i);
}

TEST(TcpAuthGate, WrongPasswordClosesAndCounts) {
  TcpAuthGate g = started();
  g.onFrame(HELLO, sizeof(HELLO), false, PW, fakeHmac);
  uint8_t n[16];
  nonce16(n);
  std::vector<uint8_t> r = response("other-password", n);
  TcpAuthAction a = g.onFrame(r.data(), r.size(), false, PW, fakeHmac);
  EXPECT_EQ(a.kind, TcpAuthAction::SEND_CLOSE);
  EXPECT_EQ(a.out[2], TCP_AUTH_WRONG);
  EXPECT_TRUE(a.failed_login);
  EXPECT_FALSE(g.active());
}

TEST(TcpAuthGate, ResponseForAnotherNonceIsWrong) {
  TcpAuthGate g = started();
  g.onFrame(HELLO, sizeof(HELLO), false, PW, fakeHmac);
  uint8_t n[16];
  nonce16(n);
  n[0] = 0x55;   // a replay from another connection
  std::vector<uint8_t> r = response(PW, n);
  EXPECT_EQ(g.onFrame(r.data(), r.size(), false, PW, fakeHmac).out[2], TCP_AUTH_WRONG);
}

TEST(TcpAuthGate, ShortResponseIsWrong) {
  TcpAuthGate g = started();
  g.onFrame(HELLO, sizeof(HELLO), false, PW, fakeHmac);
  uint8_t n[16];
  nonce16(n);
  std::vector<uint8_t> r = response(PW, n);
  TcpAuthAction a = g.onFrame(r.data(), r.size() - 1, false, PW, fakeHmac);
  EXPECT_EQ(a.out[2], TCP_AUTH_WRONG);
  EXPECT_TRUE(a.failed_login);
}

TEST(TcpAuthGate, ResponseBeforeChallengeGetsChallenge) {
  TcpAuthGate g = started();
  uint8_t n[16];
  nonce16(n);
  std::vector<uint8_t> r = response(PW, n);
  TcpAuthAction a = g.onFrame(r.data(), r.size(), false, PW, fakeHmac);
  EXPECT_EQ(a.kind, TcpAuthAction::SEND);
  EXPECT_EQ(a.out[1], TCP_AUTH_HELLO);
}

TEST(TcpAuthGate, LockedIpRefusedWithoutCounting) {
  TcpAuthGate g = started();
  TcpAuthAction a = g.onFrame(HELLO, sizeof(HELLO), true, PW, fakeHmac);
  EXPECT_EQ(a.kind, TcpAuthAction::SEND_CLOSE);
  EXPECT_EQ(a.out[2], TCP_AUTH_LOCKED);
  EXPECT_FALSE(a.failed_login);
}

TEST(TcpAuthGate, NoPasswordSetIsTheGateNotAFailure) {
  TcpAuthGate g = started();
  TcpAuthAction a = g.onFrame(HELLO, sizeof(HELLO), false, "", fakeHmac);
  EXPECT_EQ(a.kind, TcpAuthAction::SEND_CLOSE);
  EXPECT_EQ(a.out[2], TCP_AUTH_NO_PASSWORD);
  EXPECT_FALSE(a.failed_login);
  TcpAuthGate g2 = started();
  EXPECT_EQ(g2.onFrame(HELLO, sizeof(HELLO), false, nullptr, fakeHmac).out[2],
            TCP_AUTH_NO_PASSWORD);
}

TEST(TcpAuthGate, TimeoutAfter15sIsNotCounted) {
  TcpAuthGate g = started();   // started at 1000
  EXPECT_EQ(g.onTick(1000 + TCP_AUTH_DEADLINE_MS - 1).kind, TcpAuthAction::NONE);
  TcpAuthAction a = g.onTick(1000 + TCP_AUTH_DEADLINE_MS);
  EXPECT_EQ(a.kind, TcpAuthAction::SEND_CLOSE);
  EXPECT_EQ(a.out[2], TCP_AUTH_TIMEOUT);
  EXPECT_FALSE(a.failed_login);
  EXPECT_FALSE(g.active());
}

TEST(TcpAuthGate, NotStartedDoesNothing) {
  TcpAuthGate g;
  EXPECT_EQ(g.onFrame(HELLO, sizeof(HELLO), false, PW, fakeHmac).kind, TcpAuthAction::NONE);
  EXPECT_EQ(g.onTick(999999).kind, TcpAuthAction::NONE);
}

TEST(TcpAuthGate, HelloDetectionAndResultFrame) {
  EXPECT_TRUE(tcpAuthIsHello(HELLO, sizeof(HELLO)));
  EXPECT_FALSE(tcpAuthIsHello(DEVICE_QUERY, sizeof(DEVICE_QUERY)));
  uint8_t out[3];
  ASSERT_EQ(tcpAuthResultFrame(TCP_AUTH_OK, out), 3u);
  EXPECT_EQ(out[0], 0xF0);
  EXPECT_EQ(out[1], 0x03);
  EXPECT_EQ(out[2], 0x00);
}

TEST(TcpPassword, Rules) {
  EXPECT_FALSE(tcpPasswordValid("1234567"));                 // 7
  EXPECT_TRUE(tcpPasswordValid("12345678"));                 // 8
  EXPECT_TRUE(tcpPasswordValid("with space ok"));
  std::string s63(63, 'a'), s64(64, 'a');
  EXPECT_TRUE(tcpPasswordValid(s63.c_str()));
  EXPECT_FALSE(tcpPasswordValid(s64.c_str()));
  EXPECT_FALSE(tcpPasswordValid("haslo\xc5\x82zz"));         // non-ASCII (UTF-8 l-stroke)
  EXPECT_FALSE(tcpPasswordValid("tab\there1"));
  EXPECT_FALSE(tcpPasswordValid(""));
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
