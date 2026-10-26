#pragma once

// NetOta: network firmware update over HTTPS ('net ota'), separate from upstream
// 'start ota' (which keeps its own access point and web server).
//  - esp_https_server on port 443 with the node certificate from NetTls
//  - GET /update: small upload page; POST /update: raw firmware image into Update
//  - login: HTTP Basic admin:<admin password>, checked before Update.begin();
//    lockout and password come from the owner through NetOtaHooks
//  - idle timeout (each received chunk restarts it), stop on request, restart
//    1 s after a successful upload; errors never reboot
// Implemented only where NetServices owns WiFi (not in the companion).
//
// Threading: the HTTP handlers run in the httpd task; start/stop/loop and the
// getters run in the main loop(). Shared state is single-word volatile flags;
// httpd_ssl_stop() runs in a short helper task, so loop() never blocks on it.

#include <Arduino.h>

// Called from the httpd task, NOT from loop(); must not block.
struct NetOtaHooks {
  bool (*is_locked)(uint32_t ip);       // IP locked out after failed logins
  void (*login_failed)(uint32_t ip);
  void (*login_ok)(uint32_t ip);
  const char* (*admin_password)();
};

#ifndef NET_OTA_RECV_TIMEOUT_S
  #define NET_OTA_RECV_TIMEOUT_S 30     // socket timeout per read/write/handshake step
#endif

struct NetOtaHttp;                      // HTTP handlers (NetOta.cpp)

class NetOta {
public:
  // Starts the HTTPS server. timeout_s: idle limit, 0 = no limit. While a session
  // runs, a second start() only re-arms the timeout. False + lastError() on failure.
  bool start(const NetOtaHooks& hooks, uint32_t timeout_s);
  // Requests the end of the session; safe from loop(). Aborts a running upload
  // (no reboot). The server is stopped by loop() once the handler has returned.
  void stop();
  // From NetServices::loop(): timeout, deferred stop, deferred restart after OK.
  void loop();

  bool active() const { return _state != OFF; }   // true until fully stopped
  bool running() const { return _state == ON && !_stop_req; }  // serving, no stop pending
  bool restartPending() const { return _restart_at != 0; }     // upload OK, reboot soon
  bool hasLimit() const { return _timeout_ms != 0; }
  uint32_t secondsLeft() const;                    // 0 when no limit
  bool uploading() const { return _uploading; }
  uint32_t uploadedBytes() const { return _uploaded; }
  const char* lastError() const { return _last_error ? _last_error : ""; }

private:
  friend struct NetOtaHttp;
  enum State { OFF, ON, STOPPING };

  // loop() side
  State    _state = OFF;
  void*    _server = nullptr;           // httpd_handle_t
  uint32_t _timeout_ms = 0;
  char     _err_buf[48] = {0};          // start() errors only
  NetOtaHooks _hooks = {};              // written only while the server is off

  // shared with the httpd task (single-word, volatile)
  volatile bool        _stop_req = false;
  const char* volatile _stop_reason = nullptr;
  volatile bool        _in_handler = false;
  volatile bool        _uploading = false;
  volatile uint32_t    _uploaded = 0;
  volatile uint32_t    _last_activity = 0;  // millis() of the last authed request/chunk
  volatile uint32_t    _restart_at = 0;     // millis() deadline, 0 = none
  volatile bool        _stop_done = false;  // set by the helper stop task
  const char* volatile _last_error = nullptr;  // string literal or _err_buf, never freed

  void beginShutdown();
  static void stopTask(void* arg);
};
