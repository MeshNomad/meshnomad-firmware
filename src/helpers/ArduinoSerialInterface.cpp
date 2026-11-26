#include "ArduinoSerialInterface.h"
#ifdef MESHNOMAD_BUILD
  #include "SerialFrameGuard.h"
#endif

#define RECV_STATE_IDLE        0
#define RECV_STATE_HDR_FOUND   1
#define RECV_STATE_LEN1_FOUND  2
#define RECV_STATE_LEN2_FOUND  3

void ArduinoSerialInterface::enable() { 
  _isEnabled = true;
  _state = RECV_STATE_IDLE;
}
void ArduinoSerialInterface::disable() {
  _isEnabled = false;
}

#ifdef MESHNOMAD_BUILD
// MeshNomad: a USB cable alone (power, open port) is not an app session. Next to Bluetooth and
// WiFi in one companion, "always connected" hid the BLE PIN on the display; count USB as
// connected only within 60 s of the last frame the app sent.
#define APP_FRAME_TIMEOUT_MILLIS  60000

bool ArduinoSerialInterface::isConnected() const {
  return _last_frame_at != 0 && millis() - _last_frame_at < APP_FRAME_TIMEOUT_MILLIS;
}
#else
bool ArduinoSerialInterface::isConnected() const {
  return true;   // no way of knowing, so assume yes
}
#endif

bool ArduinoSerialInterface::isWriteBusy() const {
  return false;
}

size_t ArduinoSerialInterface::writeFrame(const uint8_t src[], size_t len) {
  if (len > MAX_FRAME_SIZE) {
    // frame is too big!
    return 0;
  }

  uint8_t hdr[3];
  hdr[0] = '>';
  hdr[1] = (len & 0xFF);  // LSB
  hdr[2] = (len >> 8);    // MSB

  _serial->write(hdr, 3);
  return _serial->write(src, len);
}

size_t ArduinoSerialInterface::checkRecvFrame(uint8_t dest[]) {
#ifdef MESHNOMAD_BUILD
  // B90: a frame that stopped arriving (cut by a reset, a lost byte) is dropped after a gap,
  // so the next frame is read again instead of being taken as its missing bytes.
  if (_state != RECV_STATE_IDLE && _serial->available() &&
      mesh::serialFrameStale(millis(), _last_byte_at)) {
    _state = RECV_STATE_IDLE;
  }
#endif
  while (_serial->available()) {
    int c = _serial->read();
    if (c < 0) break;
#ifdef MESHNOMAD_BUILD
    _last_byte_at = millis();
#endif

    switch (_state) {
      case RECV_STATE_IDLE:
        if (c == '<') {
          _state = RECV_STATE_HDR_FOUND;
        }
        break;
      case RECV_STATE_HDR_FOUND:
        _frame_len = (uint8_t)c;   // LSB
        _state = RECV_STATE_LEN1_FOUND;
        break;
      case RECV_STATE_LEN1_FOUND:
        _frame_len |= ((uint16_t)c) << 8;   // MSB
        rx_len = 0;
#ifdef MESHNOMAD_BUILD
        // B90: a length no app frame can have (0 or over MAX_FRAME_SIZE, e.g. 3C FF FF) is not
        // waited for: the frame is dropped and the next '<' starts a new one.
        _state = mesh::serialFrameLengthOk(_frame_len, MAX_FRAME_SIZE) ? RECV_STATE_LEN2_FOUND
                                                                        : RECV_STATE_IDLE;
#else
        _state = _frame_len > 0 ? RECV_STATE_LEN2_FOUND : RECV_STATE_IDLE;
#endif
        break;
      default:
        if (rx_len < MAX_FRAME_SIZE) {
          rx_buf[rx_len] = (uint8_t)c;   // rest of frame will be discarded if > MAX
        }
        rx_len++;
        if (rx_len >= _frame_len) {  // received a complete frame?
          if (_frame_len > MAX_FRAME_SIZE) _frame_len = MAX_FRAME_SIZE;    // truncate
          memcpy(dest, rx_buf, _frame_len);
          _state = RECV_STATE_IDLE;  // reset state, for next frame
#ifdef MESHNOMAD_BUILD
          _last_frame_at = millis();
#endif
          return _frame_len;
        }
    }
  }
  return 0;
}
