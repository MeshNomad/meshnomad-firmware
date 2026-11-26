// TcpAuthGate.h, protocol v2, with the real rweather Crypto and lib/ed25519
// (env native_tcp_crypto). The App struct below plays the app side from the protocol notes.
#include <gtest/gtest.h>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>
#include "helpers/wifi/TcpAuthGate.h"
extern "C" {
#include "ed_25519.h"
}

static const char* PW = "meshnomad-test";

// ---- node identity (fixed seed) and deterministic randomness ----
static uint8_t g_node_pub[32], g_node_prv[64];
static int g_random_calls = 0;
static uint8_t g_random_seed = 0x40;

static void nodeSign(uint8_t sig[TCP_V2_SIG_LEN], const uint8_t* msg, size_t len) {
  ed25519_sign(sig, msg, len, g_node_pub, g_node_prv);
}
static void detRandom(uint8_t* out, size_t len) {
  g_random_calls++;
  for (size_t i = 0; i < len; i++) out[i] = (uint8_t)(g_random_seed + i * 3);
}

class Gate : public ::testing::Test {
protected:
  void SetUp() override {
    uint8_t seed[32];
    for (int i = 0; i < 32; i++) seed[i] = (uint8_t)(0x10 + i);
    ed25519_create_keypair(g_node_pub, g_node_prv, seed);
    g_random_calls = 0;
    g_random_seed = 0x40;
    gate.setIdentity(g_node_pub, nodeSign, detRandom);
    gate.start(1000);
  }
  TcpAuthGate gate;
};

// ---- the app side ----
struct App {
  uint8_t priv[32], pub[32], th[32];
  TcpV2Channel ch;
  explicit App(uint8_t seed = 0x70) {
    for (int i = 0; i < 32; i++) priv[i] = (uint8_t)(seed + i);
    tcpV2KeyPair(priv, pub);
  }
  std::vector<uint8_t> hello() const {
    std::vector<uint8_t> f = {TCP_AUTH_FRAME, TCP_AUTH_HELLO, TCP_AUTH_VERSION};
    f.insert(f.end(), pub, pub + 32);
    return f;
  }
  // Checks the CHALLENGE as the app must (signature over A || R, node key) and derives keys.
  bool onChallenge(const uint8_t* c, size_t len, const uint8_t* expect_node = g_node_pub) {
    if (len != TCP_V2_CHALLENGE_LEN || c[0] != 0xF0 || c[1] != 0x01 || c[2] != 0x02) return false;
    const uint8_t* r = c + 3;
    const uint8_t* n = c + 35;
    const uint8_t* s = c + 67;
    if (memcmp(n, expect_node, 32) != 0) return false;
    uint8_t msg[74];
    tcpV2SigMessage(pub, r, msg);
    if (!ed25519_verify(s, msg, sizeof(msg), n)) return false;
    uint8_t secret[32];
    if (!tcpV2Shared(r, priv, secret)) return false;
    tcpV2Transcript(pub, r, n, th);
    ch.derive(secret, th, false);
    return true;
  }
  std::vector<uint8_t> login(const char* pw) {
    uint8_t p[TCP_V2_LOGIN_LEN];
    p[0] = TCP_AUTH_FRAME; p[1] = TCP_AUTH_LOGIN;
    tcpV2LoginMac(pw, th, p + 2);
    std::vector<uint8_t> w(sizeof(p) + TCP_V2_TAG_LEN);
    EXPECT_TRUE(ch.tx.seal(p, sizeof(p), w.data()));
    return w;
  }
  // Decrypts a RESULT from the radio; -1 if it does not open.
  int result(const TcpAuthAction& a) {
    uint8_t p[8];
    size_t n = 0;
    if (!ch.rx.open(a.out, a.out_len, p, &n) || n != 3 || p[0] != 0xF0 || p[1] != 0x03) return -1;
    return p[2];
  }
};

static TcpAuthAction feed(TcpAuthGate& g, const std::vector<uint8_t>& f, uint32_t now = 1100,
                          bool locked = false, const char* pw = PW) {
  return g.onFrame(f.data(), f.size(), locked, pw, now);
}

static void expectPlain(const TcpAuthAction& a, TcpAuthAction::Kind kind, uint8_t status) {
  EXPECT_EQ(a.kind, kind);
  ASSERT_EQ(a.out_len, 3u);
  EXPECT_EQ(a.out[0], 0xF0);
  EXPECT_EQ(a.out[1], 0x03);
  EXPECT_EQ(a.out[2], status);
}

TEST_F(Gate, FullLoginThenEncryptedSession) {
  App app;
  TcpAuthAction c = feed(gate, app.hello());
  ASSERT_EQ(c.kind, TcpAuthAction::SEND);
  ASSERT_TRUE(app.onChallenge(c.out, c.out_len));

  TcpAuthAction r = feed(gate, app.login(PW), 1200);
  ASSERT_EQ(r.kind, TcpAuthAction::ACCEPT);
  EXPECT_TRUE(r.login_ok);
  EXPECT_FALSE(r.failed_login);
  EXPECT_EQ(app.result(r), TCP_AUTH_OK);

  TcpV2Channel session;
  gate.takeChannel(session);
  EXPECT_FALSE(gate.active());
  ASSERT_TRUE(session.ready());

  // app -> radio: CMD_DEVICE_QUERY
  const uint8_t q[] = {22, 3};
  uint8_t w[64], p[64];
  size_t n = 0;
  ASSERT_TRUE(app.ch.tx.seal(q, sizeof(q), w));
  ASSERT_TRUE(session.rx.open(w, sizeof(q) + TCP_V2_TAG_LEN, p, &n));
  EXPECT_EQ(n, 2u);
  EXPECT_EQ(p[0], 22);
  // radio -> app: a reply
  const uint8_t rep[] = {13, 1, 2, 3};
  ASSERT_TRUE(session.tx.seal(rep, sizeof(rep), w));
  ASSERT_TRUE(app.ch.rx.open(w, sizeof(rep) + TCP_V2_TAG_LEN, p, &n));
  EXPECT_EQ(memcmp(p, rep, sizeof(rep)), 0);
}

TEST_F(Gate, ChallengeSignatureCoversBothOneTimeKeys) {
  App app;
  TcpAuthAction c = feed(gate, app.hello());
  ASSERT_EQ(c.kind, TcpAuthAction::SEND);
  App other(0x30);   // a different app key: the radio's signature must not verify for it
  EXPECT_FALSE(other.onChallenge(c.out, c.out_len));
  TcpAuthAction t = c;
  t.out[3] ^= 1;     // R changed in transit
  App app2 = app;
  EXPECT_FALSE(app2.onChallenge(t.out, t.out_len));
  EXPECT_TRUE(app.onChallenge(c.out, c.out_len));
}

TEST_F(Gate, AppRefusesAnUnexpectedNodeKey) {
  App app;
  TcpAuthAction c = feed(gate, app.hello());
  uint8_t pinned[32];
  memset(pinned, 0x11, sizeof(pinned));
  EXPECT_FALSE(app.onChallenge(c.out, c.out_len, pinned));
}

TEST_F(Gate, WrongPasswordEncryptedResultAndCounts) {
  App app;
  TcpAuthAction c = feed(gate, app.hello());
  ASSERT_TRUE(app.onChallenge(c.out, c.out_len));
  TcpAuthAction r = feed(gate, app.login("not-the-password"), 1200);
  EXPECT_EQ(r.kind, TcpAuthAction::SEND_CLOSE);
  EXPECT_TRUE(r.failed_login);
  EXPECT_EQ(app.result(r), TCP_AUTH_WRONG);
  EXPECT_FALSE(gate.active());
}

TEST_F(Gate, PasswordClearedBeforeLoginIsWrong) {
  App app;
  TcpAuthAction c = feed(gate, app.hello());
  ASSERT_TRUE(app.onChallenge(c.out, c.out_len));
  std::vector<uint8_t> l = app.login("");
  TcpAuthAction r = gate.onFrame(l.data(), l.size(), false, "", 1200);
  EXPECT_EQ(r.kind, TcpAuthAction::SEND_CLOSE);
  EXPECT_EQ(app.result(r), TCP_AUTH_WRONG);
}

TEST_F(Gate, LockedIpRefusedPlainWithoutCrypto) {
  App app;
  TcpAuthAction a = feed(gate, app.hello(), 1100, true);
  expectPlain(a, TcpAuthAction::SEND_CLOSE, TCP_AUTH_LOCKED);
  EXPECT_FALSE(a.failed_login);
  EXPECT_EQ(g_random_calls, 0);
}

TEST_F(Gate, NoPasswordIsTheGateNotAFailure) {
  App app;
  TcpAuthAction a = feed(gate, app.hello(), 1100, false, "");
  expectPlain(a, TcpAuthAction::SEND_CLOSE, TCP_AUTH_NO_PASSWORD);
  EXPECT_FALSE(a.failed_login);
  EXPECT_EQ(g_random_calls, 0);
}

TEST_F(Gate, V1HelloAndStockFramesAreUnsupported) {
  const std::vector<uint8_t> v1 = {0xF0, 0x01, 0x01};
  expectPlain(feed(gate, v1), TcpAuthAction::SEND_CLOSE, TCP_AUTH_UNSUPPORTED);
  gate.start(2000);
  const std::vector<uint8_t> query = {22, 3};
  expectPlain(feed(gate, query, 2100), TcpAuthAction::SEND_CLOSE, TCP_AUTH_UNSUPPORTED);
  gate.start(3000);
  App app;
  std::vector<uint8_t> shortHello = app.hello();
  shortHello.pop_back();
  expectPlain(feed(gate, shortHello, 3100), TcpAuthAction::SEND_CLOSE, TCP_AUTH_UNSUPPORTED);
  EXPECT_EQ(g_random_calls, 0);
}

TEST_F(Gate, ForgedLoginClosesAndCounts) {
  App app;
  TcpAuthAction c = feed(gate, app.hello());
  ASSERT_TRUE(app.onChallenge(c.out, c.out_len));
  std::vector<uint8_t> l = app.login(PW);
  l[5] ^= 0x40;
  TcpAuthAction r = feed(gate, l, 1200);
  EXPECT_EQ(r.kind, TcpAuthAction::CLOSE);
  EXPECT_EQ(r.out_len, 0u);
  EXPECT_TRUE(r.failed_login);
}

TEST_F(Gate, PlainLoginAfterChallengeIsRefused) {
  App app;
  TcpAuthAction c = feed(gate, app.hello());
  ASSERT_TRUE(app.onChallenge(c.out, c.out_len));
  std::vector<uint8_t> plainLogin = {TCP_AUTH_FRAME, TCP_AUTH_LOGIN};
  plainLogin.resize(TCP_V2_LOGIN_LEN, 0);
  TcpAuthAction r = feed(gate, plainLogin, 1200);
  EXPECT_EQ(r.kind, TcpAuthAction::CLOSE);
  EXPECT_TRUE(r.failed_login);
}

TEST_F(Gate, EncryptedNonLoginFrameClosesWithoutCounting) {
  App app;
  TcpAuthAction c = feed(gate, app.hello());
  ASSERT_TRUE(app.onChallenge(c.out, c.out_len));
  const uint8_t q[] = {22, 3};
  std::vector<uint8_t> w(sizeof(q) + TCP_V2_TAG_LEN);
  ASSERT_TRUE(app.ch.tx.seal(q, sizeof(q), w.data()));
  TcpAuthAction r = feed(gate, w, 1200);
  EXPECT_EQ(r.kind, TcpAuthAction::CLOSE);
  EXPECT_FALSE(r.failed_login);
}

TEST_F(Gate, OversizedFrameAfterChallengeCloses) {
  App app;
  TcpAuthAction c = feed(gate, app.hello());
  ASSERT_TRUE(app.onChallenge(c.out, c.out_len));
  std::vector<uint8_t> big(192, 0x55);
  TcpAuthAction r = feed(gate, big, 1200);
  EXPECT_EQ(r.kind, TcpAuthAction::CLOSE);
  EXPECT_TRUE(r.failed_login);
}

TEST_F(Gate, TimeoutBeforeHelloIsPlain) {
  EXPECT_EQ(gate.onTick(1000 + TCP_AUTH_DEADLINE_MS - 1).kind, TcpAuthAction::NONE);
  TcpAuthAction a = gate.onTick(1000 + TCP_AUTH_DEADLINE_MS);
  expectPlain(a, TcpAuthAction::SEND_CLOSE, TCP_AUTH_TIMEOUT);
  EXPECT_FALSE(a.failed_login);
}

TEST_F(Gate, TimeoutAfterChallengeIsEncrypted) {
  App app;
  TcpAuthAction c = feed(gate, app.hello());
  ASSERT_TRUE(app.onChallenge(c.out, c.out_len));
  TcpAuthAction a = gate.onTick(1000 + TCP_AUTH_DEADLINE_MS);
  EXPECT_EQ(a.kind, TcpAuthAction::SEND_CLOSE);
  EXPECT_FALSE(a.failed_login);
  EXPECT_EQ(app.result(a), TCP_AUTH_TIMEOUT);
}

TEST_F(Gate, SecondHandshakeWaitsForTheGap) {
  App a1;
  ASSERT_EQ(feed(gate, a1.hello(), 1100).kind, TcpAuthAction::SEND);   // handshake at 1100
  gate.start(1500);                                                   // a new connection
  App a2(0x55);
  EXPECT_EQ(feed(gate, a2.hello(), 1600).kind, TcpAuthAction::NONE);   // held back
  EXPECT_EQ(g_random_calls, 1);
  EXPECT_EQ(gate.onTick(1100 + TCP_AUTH_HS_GAP_MS - 1).kind, TcpAuthAction::NONE);
  TcpAuthAction c = gate.onTick(1100 + TCP_AUTH_HS_GAP_MS);
  ASSERT_EQ(c.kind, TcpAuthAction::SEND);
  EXPECT_EQ(g_random_calls, 2);
  EXPECT_TRUE(a2.onChallenge(c.out, c.out_len));
}

TEST_F(Gate, WeakAppKeyCloses) {
  std::vector<uint8_t> f = {TCP_AUTH_FRAME, TCP_AUTH_HELLO, TCP_AUTH_VERSION};
  f.resize(TCP_V2_HELLO_LEN, 0);   // all-zero X25519 key
  TcpAuthAction a = feed(gate, f);
  EXPECT_EQ(a.kind, TcpAuthAction::CLOSE);
  EXPECT_FALSE(gate.active());
}

TEST_F(Gate, NoIdentityRefuses) {
  TcpAuthGate g;
  g.start(1000);
  App app;
  expectPlain(feed(g, app.hello()), TcpAuthAction::SEND_CLOSE, TCP_AUTH_UNSUPPORTED);
}

TEST(TcpAuthGateIdle, NotStartedDoesNothing) {
  TcpAuthGate g;
  const uint8_t f[] = {0xF0, 0x01, 0x02};
  EXPECT_EQ(g.onFrame(f, sizeof(f), false, PW, 0).kind, TcpAuthAction::NONE);
  EXPECT_EQ(g.onTick(100000).kind, TcpAuthAction::NONE);
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

// ---- shared test vectors for the app (test/vectors/tcp_v2.json) ----
static std::string hx(const uint8_t* p, size_t n) {
  static const char* d = "0123456789abcdef";
  std::string s;
  for (size_t i = 0; i < n; i++) { s += d[p[i] >> 4]; s += d[p[i] & 15]; }
  return s;
}

TEST_F(Gate, VectorsForTheApp) {
  App app;   // app X25519 private = 0x70.. (clamped), radio private = 0x40, 0x43, ...
  uint8_t app_priv_raw[32], radio_priv_raw[32], seed[32];
  for (int i = 0; i < 32; i++) {
    app_priv_raw[i] = (uint8_t)(0x70 + i);
    radio_priv_raw[i] = (uint8_t)(0x40 + i * 3);
    seed[i] = (uint8_t)(0x10 + i);
  }
  std::vector<uint8_t> hello = app.hello();
  TcpAuthAction c = feed(gate, hello);
  ASSERT_TRUE(app.onChallenge(c.out, c.out_len));
  std::vector<uint8_t> login = app.login(PW);
  TcpAuthAction r = feed(gate, login, 1200);
  ASSERT_EQ(r.kind, TcpAuthAction::ACCEPT);
  TcpV2Channel s;
  gate.takeChannel(s);
  const uint8_t q[] = {22, 3};
  uint8_t qw[2 + TCP_V2_TAG_LEN];
  ASSERT_TRUE(app.ch.tx.seal(q, sizeof(q), qw));
  const uint8_t rep[] = {13, 0x0a, 0x0b};
  uint8_t rw[3 + TCP_V2_TAG_LEN];
  ASSERT_TRUE(s.tx.seal(rep, sizeof(rep), rw));

  const char* path = getenv("MN_TCP_VECTORS");
  if (path) {
    FILE* f = fopen(path, "w");
    ASSERT_NE(f, nullptr);
    fprintf(f,
            "{\n  \"note\": \"MeshNomad TCP protocol v2 (B91). Payloads only, without the '<'/'>' + u16 header. "
            "X25519 private keys are given before clamping. Ed25519 node key from seed (RFC 8032).\",\n"
            "  \"password\": \"%s\",\n  \"node_ed25519_seed\": \"%s\",\n  \"node_pub\": \"%s\",\n"
            "  \"app_x25519_priv\": \"%s\",\n  \"radio_x25519_priv\": \"%s\",\n"
            "  \"hello\": \"%s\",\n  \"challenge\": \"%s\",\n  \"th\": \"%s\",\n"
            "  \"login_encrypted\": \"%s\",\n  \"result_encrypted\": \"%s\",\n"
            "  \"app_frame_plain\": \"%s\",\n  \"app_frame_encrypted\": \"%s\",\n"
            "  \"radio_frame_plain\": \"%s\",\n  \"radio_frame_encrypted\": \"%s\"\n}\n",
            PW, hx(seed, 32).c_str(), hx(g_node_pub, 32).c_str(), hx(app_priv_raw, 32).c_str(),
            hx(radio_priv_raw, 32).c_str(), hx(hello.data(), hello.size()).c_str(),
            hx(c.out, c.out_len).c_str(), hx(app.th, 32).c_str(),
            hx(login.data(), login.size()).c_str(), hx(r.out, r.out_len).c_str(),
            hx(q, sizeof(q)).c_str(), hx(qw, sizeof(qw)).c_str(), hx(rep, sizeof(rep)).c_str(),
            hx(rw, sizeof(rw)).c_str());
    fclose(f);
  }
  // Pinned after the independent check (tool/meshnomad/check_tcp_v2_vectors.py, pyca): a
  // change here breaks the app; regenerate the JSON and recheck only for a new protocol version.
  EXPECT_EQ(hx(c.out, c.out_len), "f00102c3ceea6dc28f891d8ab39f80e16a8029179a86b21ce1e569e5392baf8b3520497776e870b93354f2a0b24c23f2a36cc4e80e223218c1b97926fdd018396a2b9bc37e32ff845d49b961222cce9f86f7ad2300125a86452c779e29a95190fab04a427fada2e58e335e32dd71f2e3a0bef9bbcb7577c64ca9e0b648a5d6f2989306");
  EXPECT_EQ(hx(login.data(), login.size()), "67df42b0aed372b9423879ed5b2f38d2c6de95ed0128ef5f895b790b82898df2dff88fe8cf5e275a562e18519af9dc781753");
  EXPECT_EQ(hx(r.out, r.out_len), "f47ac4189545eae22dc1eb0847d960fa02f301");
  EXPECT_EQ(hx(qw, sizeof(qw)), "d2da901ed6bce69f74acc533c0bf39d53475");
  EXPECT_EQ(hx(rw, sizeof(rw)), "4b052bbc9f90807553b473366d021a2ba03996");
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
