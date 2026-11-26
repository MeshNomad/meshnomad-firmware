// TcpSecureChannel.h with the real rweather Crypto (env native_tcp_crypto).
#include <gtest/gtest.h>
#include <string>
#include <vector>
#include "helpers/wifi/TcpSecureChannel.h"

static std::vector<uint8_t> hex(const char* s) {
  std::vector<uint8_t> v;
  for (size_t i = 0; s[i] && s[i + 1]; i += 2) v.push_back((uint8_t)std::stoi(std::string(s + i, 2), nullptr, 16));
  return v;
}

// RFC 7748, section 6.1
TEST(TcpV2Crypto, X25519Rfc7748) {
  auto a_priv = hex("77076d0a7318a57d3c16c17251b26645df4c2f87ebc0992ab177fba51db92c2a");
  auto a_pub  = hex("8520f0098930a754748b7ddcb43ef75a0dbf3a0d26381af4eba4a98eaa9b4e6a");
  auto b_priv = hex("5dab087e624a8a4b79e17f8b83800ee66f3bb1292618b6fd1c2f8b27ff88e0eb");
  auto b_pub  = hex("de9edb7d7b7dc1b4d35b61c2ece435373f8343c85b78674dadfc7e146f882b4f");
  auto shared = hex("4a5d9d5ba4ce2de1728e3bf480350f25e07e21c947d19e3376f09b3c1e161742");
  uint8_t pub[32];
  tcpV2KeyPair(a_priv.data(), pub);
  EXPECT_EQ(std::vector<uint8_t>(pub, pub + 32), a_pub);
  uint8_t s[32];
  ASSERT_TRUE(tcpV2Shared(b_pub.data(), a_priv.data(), s));
  EXPECT_EQ(std::vector<uint8_t>(s, s + 32), shared);
  for (int i = 0; i < 32; i++) EXPECT_EQ(a_priv[i], 0) << "private key not wiped";
  uint8_t s2[32];
  ASSERT_TRUE(tcpV2Shared(a_pub.data(), b_priv.data(), s2));
  EXPECT_EQ(std::vector<uint8_t>(s2, s2 + 32), shared);
}

TEST(TcpV2Crypto, X25519RefusesAllZeroPeerKey) {
  uint8_t priv[32], pub[32], zero[32] = {0}, s[32];
  for (int i = 0; i < 32; i++) priv[i] = (uint8_t)(i + 1);
  tcpV2KeyPair(priv, pub);
  EXPECT_FALSE(tcpV2Shared(zero, priv, s));
}

// RFC 5869, test case 1 (SHA-256)
TEST(TcpV2Crypto, HkdfRfc5869) {
  auto ikm = hex("0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b");
  auto salt = hex("000102030405060708090a0b0c");
  auto info = hex("f0f1f2f3f4f5f6f7f8f9");
  auto okm = hex("3cb25f25faacd57a90434f64d0362f2a2d2d0a90cf1a5a4c5db02d56ecc4c5bf34007208d5b887185865");
  uint8_t out[42];
  hkdf<SHA256>(out, sizeof(out), ikm.data(), ikm.size(), salt.data(), salt.size(), info.data(),
               info.size());
  EXPECT_EQ(std::vector<uint8_t>(out, out + 42), okm);
}

static void pair(TcpV2Channel& radio, TcpV2Channel& app) {
  uint8_t th[32], s1[32], s2[32];
  for (int i = 0; i < 32; i++) { th[i] = (uint8_t)i; s1[i] = s2[i] = (uint8_t)(0xA0 + i); }
  radio.derive(s1, th, true);
  app.derive(s2, th, false);
  for (int i = 0; i < 32; i++) EXPECT_EQ(s1[i], 0) << "secret not wiped";
}

TEST(TcpV2Channel, BothDirectionsRoundTrip) {
  TcpV2Channel radio, app;
  pair(radio, app);
  const uint8_t msg[] = {22, 3, 1, 2, 3};
  uint8_t w[64], p[64];
  size_t n = 0;
  ASSERT_TRUE(app.tx.seal(msg, sizeof(msg), w));
  ASSERT_TRUE(radio.rx.open(w, sizeof(msg) + TCP_V2_TAG_LEN, p, &n));
  EXPECT_EQ(n, sizeof(msg));
  EXPECT_EQ(memcmp(p, msg, n), 0);
  EXPECT_NE(memcmp(w, msg, sizeof(msg)), 0) << "not encrypted";
  ASSERT_TRUE(radio.tx.seal(msg, sizeof(msg), w));
  ASSERT_TRUE(app.rx.open(w, sizeof(msg) + TCP_V2_TAG_LEN, p, &n));
  EXPECT_EQ(memcmp(p, msg, n), 0);
}

TEST(TcpV2Channel, DirectionsUseDifferentKeys) {
  TcpV2Channel radio, app;
  pair(radio, app);
  const uint8_t msg[] = {1, 2, 3};
  uint8_t w[32], p[32];
  size_t n = 0;
  ASSERT_TRUE(app.tx.seal(msg, sizeof(msg), w));
  EXPECT_FALSE(app.rx.open(w, sizeof(msg) + TCP_V2_TAG_LEN, p, &n)) << "reflected frame accepted";
}

TEST(TcpV2Channel, TamperedFrameFailsAndCounterStays) {
  TcpV2Channel radio, app;
  pair(radio, app);
  const uint8_t msg[] = {9, 9, 9, 9};
  uint8_t w[32], p[32];
  size_t n = 0;
  ASSERT_TRUE(app.tx.seal(msg, sizeof(msg), w));
  for (size_t bit = 0; bit < (sizeof(msg) + TCP_V2_TAG_LEN) * 8; bit += 7) {
    uint8_t t[32];
    memcpy(t, w, sizeof(t));
    t[bit / 8] ^= (uint8_t)(1 << (bit % 8));
    EXPECT_FALSE(radio.rx.open(t, sizeof(msg) + TCP_V2_TAG_LEN, p, &n)) << "bit " << bit;
  }
  EXPECT_EQ(radio.rx.counter(), 0u);
  EXPECT_TRUE(radio.rx.open(w, sizeof(msg) + TCP_V2_TAG_LEN, p, &n));   // original still opens
}

TEST(TcpV2Channel, ReplayAndReorderFail) {
  TcpV2Channel radio, app;
  pair(radio, app);
  const uint8_t m1[] = {1}, m2[] = {2};
  uint8_t w1[32], w2[32], p[32];
  size_t n = 0;
  ASSERT_TRUE(app.tx.seal(m1, 1, w1));
  ASSERT_TRUE(app.tx.seal(m2, 1, w2));
  EXPECT_FALSE(radio.rx.open(w2, 1 + TCP_V2_TAG_LEN, p, &n)) << "reordered frame accepted";
  ASSERT_TRUE(radio.rx.open(w1, 1 + TCP_V2_TAG_LEN, p, &n));
  EXPECT_FALSE(radio.rx.open(w1, 1 + TCP_V2_TAG_LEN, p, &n)) << "replayed frame accepted";
  EXPECT_TRUE(radio.rx.open(w2, 1 + TCP_V2_TAG_LEN, p, &n));
}

TEST(TcpV2Channel, ShortFrameFails) {
  TcpV2Channel radio, app;
  pair(radio, app);
  uint8_t w[16] = {0}, p[16];
  size_t n = 0;
  EXPECT_FALSE(radio.rx.open(w, 15, p, &n));
}

TEST(TcpV2Channel, CounterLimitEndsTheSession) {
  TcpV2Channel radio, app;
  pair(radio, app);
  const uint8_t m[] = {1};
  uint8_t w[32];
  app.tx.testSetCounter(TCP_V2_COUNTER_MAX);
  EXPECT_TRUE(app.tx.seal(m, 1, w));
  EXPECT_FALSE(app.tx.seal(m, 1, w));
}

TEST(TcpV2Channel, ClearedChannelRefuses) {
  TcpV2Channel radio, app;
  pair(radio, app);
  radio.clear();
  const uint8_t m[] = {1};
  uint8_t w[32], p[32];
  size_t n = 0;
  EXPECT_FALSE(radio.tx.seal(m, 1, w));
  ASSERT_TRUE(app.tx.seal(m, 1, w));
  EXPECT_FALSE(radio.rx.open(w, 1 + TCP_V2_TAG_LEN, p, &n));
}

TEST(TcpV2Channel, MaxFrameFitsTheWireLimit) {
  TcpV2Channel radio, app;
  pair(radio, app);
  uint8_t m[176], w[176 + TCP_V2_TAG_LEN], p[176];
  for (int i = 0; i < 176; i++) m[i] = (uint8_t)i;
  size_t n = 0;
  ASSERT_TRUE(radio.tx.seal(m, sizeof(m), w));
  ASSERT_TRUE(app.rx.open(w, sizeof(w), p, &n));
  EXPECT_EQ(n, 176u);
  EXPECT_EQ(memcmp(p, m, 176), 0);
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
