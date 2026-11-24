#pragma once
// MeshNomad: the companion's TCP (WiFi) link with a login (firmware B89, TcpAuthGate.h).
// Same frames as MeshCore's SerialWifiInterface (which stays unchanged), plus:
//  - a new connection waits in a second slot until it logs in; only then it replaces the
//    session, so a connection without the password cannot drop the logged-in client;
//  - frames to the app (replies, pushes from the BLE/USB session too) go only to the
//    logged-in client;
//  - failed logins per IP: LoginLockout (3 -> 5 min).
// With tcp.auth 0 it behaves like SerialWifiInterface (newest connection wins at once).

#include "../BaseSerialInterface.h"
#include "../LoginLockout.h"
#include "TcpAuthGate.h"
#include <WiFi.h>

#ifndef WIFI_DEBUG_PRINTLN   // same switch as SerialWifiInterface.h (off in MeshNomad releases)
  #if WIFI_DEBUG_LOGGING && ARDUINO
    #define WIFI_DEBUG_PRINTLN(F, ...) Serial.printf("WiFi: " F "\n", ##__VA_ARGS__)
  #else
    #define WIFI_DEBUG_PRINTLN(...) {}
  #endif
#endif

typedef const char* (*TcpPasswordFn)();   // "" = not set
typedef bool (*TcpAuthOnFn)();            // tcp.auth

class SerialWifiAuthInterface : public BaseSerialInterface {
  struct FrameReader {
    uint8_t  type = 0;
    uint16_t length = 0;
    void reset() { type = 0; length = 0; }
    // One '<' frame from `c` into dest (MAX_FRAME_SIZE), 0 = none complete yet.
    size_t read(WiFiClient& c, uint8_t dest[]);
  };

  struct Frame {
    uint8_t len;
    uint8_t buf[MAX_FRAME_SIZE];
  };

  #define MN_WIFI_FRAME_QUEUE_SIZE 4

  bool _isEnabled = false;
  bool deviceConnected = false;
  WiFiServer server;
  WiFiClient client;            // the session (logged in, or any client with tcp.auth 0)
  FrameReader client_rx;
  uint32_t client_ip = 0;
  WiFiClient pending;           // waiting for its login
  FrameReader pending_rx;
  uint32_t pending_ip = 0;

  TcpAuthGate gate;
  LoginLockout lockout;
  TcpPasswordFn _password = nullptr;
  TcpAuthOnFn _auth_on = nullptr;

  int send_queue_len = 0;
  Frame send_queue[MN_WIFI_FRAME_QUEUE_SIZE];

  static SerialWifiAuthInterface* _instance;

  bool authOn() const { return _auth_on ? _auth_on() : true; }
  const char* password() const { const char* p = _password ? _password() : nullptr; return p ? p : ""; }
  static void writeRaw(WiFiClient& c, const uint8_t* payload, size_t len);
  void acceptNew();
  void servicePending();
  void dropPending();

public:
  SerialWifiAuthInterface() : server(WiFiServer()), client(WiFiClient()), pending(WiFiClient()) {
    _instance = this;
  }

  void begin(int port);
  void setAuth(TcpPasswordFn password, TcpAuthOnFn auth_on) { _password = password; _auth_on = auth_on; }

  // 'get tcp.status': "auth:on|off pwd:set|none client:<ip>|none fails:<n> locked:<n>"
  static bool status(char* out, size_t n);

  // BaseSerialInterface methods
  void enable() override;
  void disable() override;
  bool isEnabled() const override { return _isEnabled; }

  bool isConnected() const override { return deviceConnected; }
  bool isWriteBusy() const override { return false; }

  size_t writeFrame(const uint8_t src[], size_t len) override;
  size_t checkRecvFrame(uint8_t dest[]) override;
};
