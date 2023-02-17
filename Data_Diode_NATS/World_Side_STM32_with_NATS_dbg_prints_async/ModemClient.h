#ifndef INCLUDE_MODEM_CLIENT_H
#define INCLUDE_MODEM_CLIENT_H

#include "CircularBuffer.h"
#include "MemoryBuffer.h"
#include <stdio.h>
#include <stdlib.h>

// #define ANDREW
#ifdef ANDREW
class HardwareSerial {
public:
  void readBytes(void *buf, size_t len);
  size_t available();
  size_t availableForWrite();
  void write(void *b, size_t len);
  uint8_t read();
  void begin(int);
};
uint32_t millis();
typedef uint8_t PinName;
#define OUTPUT 1
#define HIGH 1
#define LOW 0
void pinMode(PinName, uint8_t);
void digitalWrite(PinName, uint8_t);
#endif

#ifndef ANDREW
#include <Arduino.h>
#endif

#define MODEM_BAUD_RATE 115200

#ifndef DEBUG_PRINT
#define DEBUG_PRINT(...)
#endif

// Note: Measuring time is hard when the clock can overflow. Instead, compute
// and compare durations
// Checks if it has been at least x milliseconds since the last TIME_MARK()
#define TIME_HAS_BEEN(x) millis() - timeMark > x
#define TIME_MARK() timeMark = millis()

#define CHANGE_STATE(x)                                                        \
  {                                                                            \
    state = x;                                                                 \
    TIME_MARK();                                                               \
  }

enum State {
  M_ERROR,
  M_POWER_OFF,
  M_RESET,
  M_POWER_ON,
  M_POLL,
  M_READY,
  M_NET_OPEN,
  M_CONNECTING,
  M_STOPPING,
  M_CONNECTED,
  M_WAITING_TO_SEND,
  M_SENDING,
};

class ModemClient {

public:
  // constructor
  ModemClient(HardwareSerial modem, uint8_t pwr);

  int connect(const char *host, uint16_t port);
  void stop();
  size_t write(const uint8_t *buf, size_t size);
  int available();
  int read(); // TODO: remove
  int getline();
  bool connected();
  const char *get_error();

  bool initialized();
  void on();
  void off();
  void reset();

  void process(); // Non-blocking, call regularly in main loop.

protected:
  HardwareSerial modem;
  struct MemBuffer tx;
  struct MemBuffer rx;
  struct MemBuffer tcp_rx;
  CircularBuffer<struct MemBuffer *, 10> tcp_tx;
  size_t tcp_bytes_to_recv = 0;

  uint8_t pwr;
  enum State state = M_ERROR;
  uint32_t timeMark;
  char *lastError;

  char ip[16]; // XXX.XXX.XXX.XXX
  uint16_t port;

  char _at[100];

  void _processSerial();
  void _tick();
  void _change_state(State nState);

  void modem_reset();
  void modem_send_at();
  void modem_open_network();
  void modem_connect();
  void modem_send();
  void modem_off();
  void modem_on();
  void modem_stop();
};

#endif
