#include "WifiGuardProbe.h"
#ifdef MN_WIFI_GUARD

#include <esp_attr.h>
#include <esp_netif.h>
#include <esp_netif_net_stack.h>   // esp_netif_get_netif_impl
#include <esp_wifi.h>
#include <lwip/etharp.h>
#include <lwip/tcpip.h>

namespace WifiGuardProbe {

static const uint32_t ANSWER_WAIT_MS = 2000;

enum : int { IDLE, SENT, CHECKING, DONE_OK, DONE_FAIL };
static volatile int _state = IDLE;
static ip4_addr_t _gw;
static uint32_t _sent_at = 0;

struct RtcCounter { uint32_t magic, count; };
RTC_NOINIT_ATTR static RtcCounter _rtc;
static const uint32_t RTC_MAGIC = 0x4D4E5747;   // "MNWG"

static struct netif* staNetif() {
  esp_netif_t* n = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
  return n ? (struct netif*)esp_netif_get_netif_impl(n) : nullptr;
}

// Both run in the tcpip thread (lwIP is not thread-safe outside it).
static void sendCb(void*) {
  struct netif* nf = staNetif();
  if (!nf) { _state = DONE_FAIL; return; }
  etharp_cleanup_netif(nf);            // forget cached answers, so only a fresh one counts
  etharp_request(nf, &_gw);
}
static void checkCb(void*) {
  struct netif* nf = staNetif();
  struct eth_addr* eth = nullptr;
  const ip4_addr_t* ip = nullptr;
  _state = (nf && etharp_find_addr(nf, &_gw, &eth, &ip) >= 0) ? DONE_OK : DONE_FAIL;
}

void start(uint32_t gw, uint32_t now) {
  _gw.addr = gw;                       // same byte order as IPAddress -> uint32_t
  _sent_at = now;
  _state = SENT;
  if (gw == 0 || tcpip_callback(sendCb, nullptr) != ERR_OK) _state = DONE_FAIL;
}

int poll(uint32_t now) {
  int s = _state;
  if (s == SENT && now - _sent_at >= ANSWER_WAIT_MS) {
    _state = CHECKING;
    if (tcpip_callback(checkCb, nullptr) != ERR_OK) _state = DONE_FAIL;
    return -1;
  }
  if (s == DONE_OK || s == DONE_FAIL) {
    _state = IDLE;
    return s == DONE_OK ? 1 : 0;
  }
  return -1;
}

uint32_t restarts() { return _rtc.magic == RTC_MAGIC ? _rtc.count : 0; }

void countRestart() {
  if (_rtc.magic != RTC_MAGIC) { _rtc.magic = RTC_MAGIC; _rtc.count = 0; }
  _rtc.count++;
}

void reconnect() {
  esp_wifi_disconnect();
  esp_wifi_stop();
  esp_wifi_start();
  esp_wifi_connect();
}

}  // namespace WifiGuardProbe

#endif  // MN_WIFI_GUARD
