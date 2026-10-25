#if defined(ESP32) && defined(WITH_NET_SERVICES) && !defined(NET_WIFI_EXTERNAL)

#include "NetOta.h"
#include "NetTls.h"
#include <Update.h>
#include <esp_idf_version.h>
#include <esp_https_server.h>
#include <lwip/sockets.h>
#include <mbedtls/base64.h>
#include <mbedtls/platform_util.h>
#include <esp_ota_ops.h>

static const size_t   OTA_CHUNK      = 4096;    // body read size (heap buffer)
static const uint32_t OTA_MAX_TIMEOUT_S = 2000000UL;  // keeps timeout_s * 1000 in 32 bits

// Upload page. The password is typed into the page: a zero-length POST checks it
// first (fetch with credentials 'omit', so a 401 never opens the browser's login
// dialog), then XMLHttpRequest sends the raw file for upload progress (fetch has
// no upload progress). No multipart, no client-side MD5.
static const char OTA_PAGE[] PROGMEM = R"HTML(<!DOCTYPE html><html><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1"><title>net ota</title>
<style>body{font-family:sans-serif;max-width:30em;margin:2em auto;padding:0 1em}
input,button{display:block;margin:.7em 0;font-size:1em}progress{width:100%}</style></head><body>
<h3>MeshNomad firmware update</h3>
<input id="p" type="password" placeholder="admin password" autocomplete="current-password">
<input id="f" type="file" accept=".bin">
<button id="b">Upload</button>
<progress id="g" max="100" value="0"></progress>
<p id="s"></p>
<script>
function E(i){return document.getElementById(i)}
function M(t){E('s').textContent=t}
function F(t){M(String(t));E('b').disabled=false}
E('b').onclick=function(){
var f=E('f').files[0],a;
if(!f){M('Choose a .bin file');return}
try{a='Basic '+btoa('admin:'+E('p').value)}catch(e){M('Unsupported characters in the password');return}
E('b').disabled=true;E('g').value=0;M('Checking the password...');
fetch('/update',{method:'POST',headers:{Authorization:a,'X-Size':String(f.size)},credentials:'omit'}).then(function(r){
if(r.status!=200)return r.text().then(function(t){throw t||'HTTP '+r.status});
var x=new XMLHttpRequest();
x.open('POST','/update');x.setRequestHeader('Authorization',a);
x.upload.onprogress=function(e){if(e.lengthComputable){E('g').value=e.loaded*100/e.total;M('Uploading '+e.loaded+' / '+e.total+' bytes')}};
x.onload=function(){if(x.status==200&&x.responseText=='OK'){E('g').value=100;M('OK - the node restarts now')}else F(x.responseText||'HTTP '+x.status)};
x.onerror=function(){F('Connection lost - check net ota status on the node')};
x.send(f);M('Uploading...')
}).catch(F)};
</script></body></html>
)HTML";

// Peer IPv4 in the same byte order as (uint32_t)WiFiClient::remoteIP(), so one
// lockout table serves the console and this server. httpd listens on a dual-stack
// socket, so IPv4 peers may show up as ::ffff:a.b.c.d.
static uint32_t peerIp(httpd_req_t* r) {
  int fd = httpd_req_to_sockfd(r);
  if (fd < 0) return 0;
  struct sockaddr_storage ss;
  socklen_t len = sizeof(ss);
  if (getpeername(fd, (struct sockaddr*)&ss, &len) != 0) return 0;
  uint32_t ip = 0;
  if (ss.ss_family == AF_INET) {
    ip = ((struct sockaddr_in*)&ss)->sin_addr.s_addr;
#if LWIP_IPV6
  } else if (ss.ss_family == AF_INET6) {
    // ::ffff:a.b.c.d is an IPv4 peer; native IPv6 peers share one lockout bucket.
    // (lwIP has no IN6_IS_ADDR_V4MAPPED; this is the same test on the raw bytes.)
    const uint8_t* a = ((struct sockaddr_in6*)&ss)->sin6_addr.s6_addr;
    static const uint8_t V4MAPPED[12] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0xff, 0xff};
    if (memcmp(a, V4MAPPED, sizeof(V4MAPPED)) == 0) memcpy(&ip, a + 12, 4);
    else ip = 0xFFFFFFFFUL;
#endif
  }
  return ip;
}

// Constant time for equal lengths; the length itself is not secret here.
static bool sameBytes(const char* a, size_t alen, const char* b, size_t blen) {
  size_t n = alen > blen ? alen : blen;
  uint8_t d = (alen != blen);
  for (size_t i = 0; i < n; i++) {
    uint8_t ca = i < alen ? (uint8_t)a[i] : 0;
    uint8_t cb = i < blen ? (uint8_t)b[i] : 0;
    d |= ca ^ cb;
  }
  return d == 0;
}

// True if the request carries "Authorization: Basic base64(admin:<password>)".
// An empty or missing admin password never matches.
static bool basicAuthOk(httpd_req_t* r, const char* pw) {
  if (!pw || !pw[0]) return false;
  char hdr[160];
  size_t hlen = httpd_req_get_hdr_value_len(r, "Authorization");
  if (hlen == 0 || hlen >= sizeof(hdr)) return false;
  if (httpd_req_get_hdr_value_str(r, "Authorization", hdr, sizeof(hdr)) != ESP_OK) return false;
  if (strncasecmp(hdr, "Basic ", 6) != 0) return false;
  const char* b64 = hdr + 6;
  while (*b64 == ' ') b64++;

  unsigned char got[120];
  size_t got_len = 0;
  char want[120];
  int want_len = snprintf(want, sizeof(want), "admin:%s", pw);
  bool ok = want_len > 0 && want_len < (int)sizeof(want) &&
            mbedtls_base64_decode(got, sizeof(got), &got_len,
                                  (const unsigned char*)b64, strlen(b64)) == 0 &&
            sameBytes((const char*)got, got_len, want, (size_t)want_len);
  mbedtls_platform_zeroize(got, sizeof(got));
  mbedtls_platform_zeroize(want, sizeof(want));
  mbedtls_platform_zeroize(hdr, sizeof(hdr));
  return ok;
}

static esp_err_t reply(httpd_req_t* r, const char* status, const char* text, bool close) {
  httpd_resp_set_status(r, status);
  httpd_resp_set_type(r, "text/plain");
  httpd_resp_set_hdr(r, "Cache-Control", "no-store");
  if (close) httpd_resp_set_hdr(r, "Connection", "close");
  httpd_resp_sendstr(r, text);           // may fail on a dead socket; nothing to do then
  return ESP_OK;
}

struct NetOtaHttp {
  static esp_err_t page(httpd_req_t* r) {
    httpd_resp_set_type(r, "text/html");
    httpd_resp_set_hdr(r, "Cache-Control", "no-store");
    return httpd_resp_send(r, OTA_PAGE, sizeof(OTA_PAGE) - 1);
  }

  static esp_err_t post(httpd_req_t* r) {
    NetOta* o = (NetOta*)r->user_ctx;
    o->_in_handler = true;
    esp_err_t res = upload(o, r);
    o->_uploading = false;
    o->_in_handler = false;
    return res;
  }

  // ESP_FAIL makes httpd close the socket instead of draining a large unread body.
  static esp_err_t upload(NetOta* o, httpd_req_t* r) {
    if (o->_stop_req || o->_restart_at || Update.isRunning()) {
      reply(r, "503 Service Unavailable", "Error: session ending", true);
      return ESP_FAIL;
    }
    uint32_t ip = peerIp(r);
    if (o->_hooks.is_locked && o->_hooks.is_locked(ip)) {   // before reading anything else
      reply(r, "429 Too Many Requests", "Error: locked out, try again later", true);
      return ESP_FAIL;
    }
    const char* pw = o->_hooks.admin_password ? o->_hooks.admin_password() : nullptr;
    if (!pw || !pw[0]) {                     // default/empty admin password: gate, not a failed login
      reply(r, "403 Forbidden", "Error: change the admin password first (password <new>)", true);
      return r->content_len ? ESP_FAIL : ESP_OK;
    }
    if (!basicAuthOk(r, pw)) {
      if (o->_hooks.login_failed) o->_hooks.login_failed(ip);
      httpd_resp_set_hdr(r, "WWW-Authenticate", "Basic realm=\"net ota\"");
      reply(r, "401 Unauthorized", "Error: wrong password", true);
      return r->content_len ? ESP_FAIL : ESP_OK;
    }
    if (o->_hooks.login_ok) o->_hooks.login_ok(ip);
    o->_last_activity = millis();

    if (r->content_len == 0) {               // password check from the page
      // The page announces the file size (X-Size), so a too-large image is refused
      // before the upload starts.
      char xs[16];
      const esp_partition_t* slot = esp_ota_get_next_update_partition(NULL);
      if (slot && httpd_req_get_hdr_value_str(r, "X-Size", xs, sizeof(xs)) == ESP_OK &&
          strtoul(xs, nullptr, 10) > slot->size) {
        reply(r, "413 Payload Too Large", "Error: image larger than the OTA slot", false);
        return ESP_OK;
      }
      reply(r, "200 OK", "auth ok", false);
      return ESP_OK;
    }

    // Update.begin() refuses an image larger than the OTA slot.
    if (!Update.begin(r->content_len)) {
      bool too_big = Update.getError() == UPDATE_ERROR_SIZE;
      return fail(o, r, too_big ? "413 Payload Too Large" : "500 Internal Server Error",
                  Update.errorString(), nullptr);
    }
    char* buf = (char*)malloc(OTA_CHUNK);
    if (!buf) return fail(o, r, "500 Internal Server Error", "not enough memory", nullptr);

    o->_uploaded = 0;
    o->_uploading = true;
    size_t left = r->content_len;
    while (left > 0) {
      if (o->_stop_req) {
        const char* why = o->_stop_reason;
        return fail(o, r, "503 Service Unavailable", why ? why : "stopped", buf);
      }
      // One read: under esp_https_server a timeout comes back as an esp-tls code and
      // httpd_req_recv() drops the rest of the body on any negative return, so no retry.
      int n = httpd_req_recv(r, buf, left < OTA_CHUNK ? left : OTA_CHUNK);
      if (n <= 0) {
        return fail(o, r, "408 Request Timeout",
                    n == 0 ? "client closed the connection" : "upload stalled or failed", buf);
      }
      if (Update.write((uint8_t*)buf, (size_t)n) != (size_t)n) {
        return fail(o, r, "500 Internal Server Error", Update.errorString(), buf);
      }
      left -= (size_t)n;
      o->_uploaded += (uint32_t)n;
      o->_last_activity = millis();          // every chunk restarts the idle timeout
    }
    free(buf);

    if (!Update.end()) {                     // checks the image and sets the boot slot
      return fail(o, r, "500 Internal Server Error", Update.errorString(), nullptr);
    }
    uint32_t at = millis() + 1000;           // loop() restarts after the reply is out
    o->_restart_at = at ? at : 1;
    o->_last_error = nullptr;
    reply(r, "200 OK", "OK", true);
    return ESP_OK;
  }

  // Every failed upload ends here: abort Update (the old firmware stays bootable),
  // reply with the reason, never reboot.
  static esp_err_t fail(NetOta* o, httpd_req_t* r, const char* status, const char* why, char* buf) {
    if (buf) free(buf);
    Update.abort();
    o->_last_error = why;                    // string literal (Update.errorString() too)
    Serial.printf("NetOta: upload failed: %s\n", why);
    char msg[80];
    snprintf(msg, sizeof(msg), "Error: %s", why);
    reply(r, status, msg, true);
    return ESP_FAIL;
  }
};

// ---- lifecycle (loop() side) ----
bool NetOta::start(const NetOtaHooks& hooks, uint32_t timeout_s) {
  if (timeout_s > OTA_MAX_TIMEOUT_S) timeout_s = OTA_MAX_TIMEOUT_S;
  if (_restart_at) {
    _last_error = "update done, restarting";
    return false;
  }
  // A stop already requested (handler still running) must not be "re-armed" into a
  // session that loop() is about to shut down.
  if (_state == STOPPING || (_state == ON && _stop_req)) {
    _last_error = "previous session still stopping";
    return false;
  }
  if (_state == ON) {                        // already running: re-arm the timeout only
    _timeout_ms = timeout_s * 1000UL;
    _last_activity = millis();
    return true;
  }
  if (!NetTls::ready()) {
    _last_error = "TLS certificate not ready";
    return false;
  }

  _hooks = hooks;
  _stop_req = false;
  _stop_reason = nullptr;
  _in_handler = false;
  _uploading = false;
  _uploaded = 0;
  _restart_at = 0;
  _stop_done = false;
  _last_error = nullptr;

  httpd_ssl_config_t conf = HTTPD_SSL_CONFIG_DEFAULT();
  // httpd copies the certificate and key, they need not stay valid afterwards.
#if ESP_IDF_VERSION_MAJOR >= 5
  conf.servercert     = NetTls::certPem();
  conf.servercert_len = NetTls::certPemLen();
#else
  conf.cacert_pem     = NetTls::certPem();   // IDF 4.4: the server's own certificate
  conf.cacert_len     = NetTls::certPemLen();
#endif
  conf.prvtkey_pem    = NetTls::keyPem();
  conf.prvtkey_len    = NetTls::keyPemLen();
  conf.transport_mode = HTTPD_SSL_TRANSPORT_SECURE;
  conf.port_secure    = 443;
  conf.httpd.stack_size        = 10240;
  conf.httpd.max_open_sockets  = 1;     // one TLS session (~40 KB heap)
  conf.httpd.max_uri_handlers  = 3;
  conf.httpd.backlog_conn      = 2;
  conf.httpd.lru_purge_enable  = true;  // a new connection replaces an idle one
  conf.httpd.recv_wait_timeout = NET_OTA_RECV_TIMEOUT_S;
  conf.httpd.send_wait_timeout = 10;
  // Never above loopTask; off the mesh loop's core where there are two cores.
  conf.httpd.task_priority     = tskIDLE_PRIORITY + 1;
#if !CONFIG_FREERTOS_UNICORE
  conf.httpd.core_id           = 0;
#endif

  httpd_handle_t h = nullptr;
  esp_err_t err = httpd_ssl_start(&h, &conf);
  if (err != ESP_OK) {
    snprintf(_err_buf, sizeof(_err_buf), "HTTPS server failed (%s)", esp_err_to_name(err));
    _last_error = _err_buf;
    return false;
  }

  httpd_uri_t get_page = {};
  get_page.uri      = "/update";
  get_page.method   = HTTP_GET;
  get_page.handler  = NetOtaHttp::page;
  get_page.user_ctx = this;
  httpd_uri_t post_fw = {};
  post_fw.uri      = "/update";
  post_fw.method   = HTTP_POST;
  post_fw.handler  = NetOtaHttp::post;
  post_fw.user_ctx = this;
  if (httpd_register_uri_handler(h, &get_page) != ESP_OK ||
      httpd_register_uri_handler(h, &post_fw) != ESP_OK) {
    httpd_ssl_stop(h);                       // no client yet: returns at once
    _last_error = "HTTPS handler setup failed";
    return false;
  }

  _server = h;
  _timeout_ms = timeout_s * 1000UL;
  _last_activity = millis();
  _state = ON;
  return true;
}

void NetOta::stop() {
  if (_state != ON || _stop_req) return;
  _stop_reason = "stopped";
  _stop_req = true;                          // the handler checks it between chunks
}

uint32_t NetOta::secondsLeft() const {
  if (_state != ON || _timeout_ms == 0) return 0;
  uint32_t idle = millis() - _last_activity;
  return idle >= _timeout_ms ? 0 : (_timeout_ms - idle + 999) / 1000;
}

void NetOta::loop() {
  // Checked before anything else: a stop() after a successful upload must not
  // cancel the restart (the new image is already the boot slot).
  uint32_t restart_at = _restart_at;
  if (restart_at && (int32_t)(millis() - restart_at) >= 0) ESP.restart();

  if (_state == ON) {
    if (!_stop_req && !restart_at && _timeout_ms &&
        millis() - _last_activity >= _timeout_ms) {
      _stop_reason = "session timeout";
      _stop_req = true;
      if (!_uploading) _last_error = "session timeout";
    }
    if (_stop_req && !_in_handler) beginShutdown();
  } else if (_state == STOPPING && _stop_done) {
    _server = nullptr;
    _state = OFF;
  }
}

// httpd_ssl_stop() waits for the server task, which may still be inside a TLS
// handshake (up to NET_OTA_RECV_TIMEOUT_S per step); a helper task keeps loop() free.
void NetOta::beginShutdown() {
  _state = STOPPING;
  _stop_done = false;
  if (xTaskCreate(stopTask, "net_ota_stop", 4096, this, tskIDLE_PRIORITY + 1, nullptr) != pdPASS) {
    httpd_ssl_stop((httpd_handle_t)_server);   // no memory for the task: stop inline
    _stop_done = true;
  }
}

void NetOta::stopTask(void* arg) {
  NetOta* o = (NetOta*)arg;
  httpd_ssl_stop((httpd_handle_t)o->_server);
  o->_stop_done = true;
  vTaskDelete(nullptr);
}

#endif // ESP32 && WITH_NET_SERVICES && !NET_WIFI_EXTERNAL
