#include "TcpAuthConfig.h"
#ifdef MN_TCP_AUTH

#include "SerialWifiAuthInterface.h"
#include <esp_random.h>

SerialWifiAuthInterface* SerialWifiAuthInterface::_instance = nullptr;

// Hardware RNG; true random while the radio (WiFi) is on, which a TCP link implies.
static void tcpRandom(uint8_t* out, size_t len) {
  esp_fill_random(out, len);
}

void SerialWifiAuthInterface::begin(int port) {
  // WiFi itself is set up outside of this class; this only starts the server
  server.begin(port);
}

void SerialWifiAuthInterface::setIdentity(const uint8_t pub[32], TcpSignFn sign) {
  gate.setIdentity(pub, sign, tcpRandom);
}

void SerialWifiAuthInterface::enable() {
  if (_isEnabled) return;
  _isEnabled = true;
  send_queue_len = 0;
}

void SerialWifiAuthInterface::disable() {
  _isEnabled = false;
}

size_t SerialWifiAuthInterface::writeFrame(const uint8_t src[], size_t len) {
  if (len > MAX_FRAME_SIZE) return 0;
  if (deviceConnected && len > 0) {   // the session only, never a waiting connection
    if (send_queue_len >= MN_WIFI_FRAME_QUEUE_SIZE) return 0;
    send_queue[send_queue_len].len = len;
    memcpy(send_queue[send_queue_len].buf, src, len);
    send_queue_len++;
    return len;
  }
  return 0;
}

void SerialWifiAuthInterface::writeRaw(WiFiClient& c, const uint8_t* payload, size_t len) {
  uint8_t pkt[3 + TCP_WIRE_MAX];
  if (len > TCP_WIRE_MAX) return;
  pkt[0] = '>';
  pkt[1] = (len & 0xFF);
  pkt[2] = (len >> 8);
  memcpy(&pkt[3], payload, len);
  c.write(pkt, 3 + len);
}

// Same rules as SerialWifiInterface::checkRecvFrame(): 3-byte header ('<', u16 LE length),
// frames of another type or longer than the limit (here MAX_FRAME_SIZE + tag) are skipped.
size_t SerialWifiAuthInterface::FrameReader::read(WiFiClient& c, uint8_t dest[]) {
  if (type == 0 || length == 0) {
    if (c.available() < 3) return 0;
    c.readBytes(&type, 1);
    c.readBytes((uint8_t*)&length, 2);
    if (type == 0 || length == 0) { reset(); return 0; }
  }
  int frame_length = length;
  if (frame_length > c.available()) return 0;   // wait for the rest
  if (frame_length > (int)TCP_WIRE_MAX || type != '<') {
    while (frame_length > 0) {
      uint8_t skip[1];
      int skipped = c.read(skip, 1);
      if (skipped <= 0) break;
      frame_length -= skipped;
    }
    reset();
    return 0;
  }
  c.readBytes(dest, frame_length);
  reset();
  return frame_length;
}

void SerialWifiAuthInterface::dropPending() {
  gate.stop();
  pending.stop();
  pending = WiFiClient();
  pending_rx.reset();
  pending_ip = 0;
}

void SerialWifiAuthInterface::endSession() {
  deviceConnected = false;
  client.stop();
  client_rx.reset();
  client_ip = 0;
  session.clear();
  send_queue_len = 0;
}

void SerialWifiAuthInterface::acceptNew() {
  auto newClient = server.available();
  if (!newClient) return;
  if (!authOn()) {             // MeshCore behaviour: the newest connection is the session
    dropPending();
    endSession();
    client = newClient;
    client_ip = (uint32_t)client.remoteIP();
    return;
  }
  // A newer waiting connection replaces an older one; the session is not touched.
  dropPending();
  pending = newClient;
  pending_ip = (uint32_t)pending.remoteIP();
  gate.start(millis());
}

void SerialWifiAuthInterface::servicePending() {
  if (!gate.active()) return;
  if (!pending.connected()) { dropPending(); return; }

  TcpAuthAction a;
  uint8_t frame[TCP_WIRE_MAX];
  size_t len = pending_rx.read(pending, frame);
  uint32_t now = millis();
  if (len > 0) {
    a = gate.onFrame(frame, len, lockout.isLocked(pending_ip, now), password(), now);
  } else {
    a = gate.onTick(now);
  }
  if (a.failed_login) lockout.fail(pending_ip, now);
  if (a.login_ok) lockout.ok(pending_ip);
  if (a.out_len > 0) writeRaw(pending, a.out, a.out_len);

  if (a.kind == TcpAuthAction::SEND_CLOSE || a.kind == TcpAuthAction::CLOSE) {
    WIFI_DEBUG_PRINTLN("login refused (%d)", (int)a.kind);
    // Unread input at close makes lwIP send a RST, which can drop the RESULT just
    // written (an old app sends several frames at once): read what has arrived first.
    uint8_t skip[32];
    for (int i = 0; i < 16 && pending.available() > 0; i++) {
      if (pending.read(skip, sizeof(skip)) <= 0) break;
    }
    dropPending();
  } else if (a.kind == TcpAuthAction::ACCEPT) {
    WIFI_DEBUG_PRINTLN("login ok");
    endSession();              // the old session ends here
    client = pending;
    client_ip = pending_ip;
    gate.takeChannel(session);
    pending = WiFiClient();
    pending_rx.reset();
    pending_ip = 0;
  }
}

size_t SerialWifiAuthInterface::checkRecvFrame(uint8_t dest[]) {
  acceptNew();
  servicePending();

  if (client.connected()) {
    if (!deviceConnected) {
      WIFI_DEBUG_PRINTLN("Got connection");
      deviceConnected = true;
    }
  } else if (deviceConnected || session.ready()) {
    endSession();
    WIFI_DEBUG_PRINTLN("Disconnected");
  }
  if (!deviceConnected) return 0;

  if (send_queue_len > 0) {    // first, the send queue
    Frame& q = send_queue[0];
    if (session.ready()) {
      uint8_t sealed[TCP_WIRE_MAX];
      if (!session.tx.seal(q.buf, q.len, sealed)) { endSession(); return 0; }
      writeRaw(client, sealed, q.len + TCP_V2_TAG_LEN);
    } else {
      writeRaw(client, q.buf, q.len);
    }
    send_queue_len--;
    for (int i = 0; i < send_queue_len; i++) send_queue[i] = send_queue[i + 1];
    return 0;
  }

  size_t len = 0;
  uint8_t wire[TCP_WIRE_MAX];                       // dest holds MAX_FRAME_SIZE only
  size_t wlen = client_rx.read(client, wire);
  if (wlen == 0) return 0;
  if (session.ready()) {
    if (!session.rx.open(wire, wlen, dest, &len)) {   // forged, replayed or out of order
      WIFI_DEBUG_PRINTLN("bad frame, session closed");
      endSession();
      return 0;
    }
  } else {
    if (wlen > MAX_FRAME_SIZE) return 0;              // plain session keeps the old limit
    memcpy(dest, wire, wlen);
    len = wlen;
  }
  if (len > 0 && dest[0] == TCP_AUTH_FRAME) {   // login frames never reach the companion
    if (tcpAuthIsHello(dest, len) && !session.ready()) {   // tcp.auth 0: plain link
      uint8_t out[3];
      writeRaw(client, out, tcpAuthResultFrame(TCP_AUTH_OK, out));
    }
    return 0;
  }
  return len;
}

bool SerialWifiAuthInterface::status(char* out, size_t n) {
  SerialWifiAuthInterface* s = _instance;
  if (!s) return false;
  char ip[16] = "none";
  if (s->deviceConnected) {
    IPAddress a(s->client_ip);
    snprintf(ip, sizeof(ip), "%s", a.toString().c_str());
  }
  uint32_t now = millis();
  snprintf(out, n, "auth:%s pwd:%s client:%s enc:%s fails:%lu locked:%d",
           s->authOn() ? "on" : "off", s->password()[0] ? "set" : "none", ip,
           s->session.ready() ? "on" : "off", (unsigned long)s->lockout.failsSinceBoot(),
           s->lockout.lockedCount(now));
  return true;
}

#endif // MN_TCP_AUTH
