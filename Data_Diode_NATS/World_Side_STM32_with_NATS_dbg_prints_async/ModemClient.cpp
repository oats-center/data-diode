#include "ModemClient.h"
#include "MemoryBuffer.h"
#include <cstring>

ModemClient::ModemClient(HardwareSerial modem, uint8_t pwr)
    : modem(modem), pwr(pwr) {

  // Buffer for modem AT TX
  membuf_init(&tx);

  // Buffer for modem AT RX
  membuf_init(&rx);

  // Buffer connection RX data
  membuf_init(&tcp_rx);

  modem_off();
}

void ModemClient::process() {
  _processSerial();
  _tick();
}

int ModemClient::connect(const char *host, const uint16_t prt) {
  // We only allow one connection, so may only connect when in READY
  if (state != M_READY) {
    return -1;
  }

  port = prt;

  // Determine the host IP address
  membuf_str(&tx, "AT+CDNSGIP=\"");
  membuf_str(&tx, host);
  membuf_str(&tx, "\"\n");

  return 0;
}

void ModemClient::stop() {
  if (state >= M_CONNECTED) {
    return;
  }

  // TODO: stop even if not >= M_CONNECTED?
  modem_stop();
}

size_t ModemClient::write(const uint8_t *buf, size_t size) {
  if (state != M_CONNECTED) {
    DEBUG_PRINT("[WARN] write() called when not connected.\n");
    return -1;
  }

  struct MemBuffer *b = (struct MemBuffer *)malloc(sizeof(struct MemBuffer));
  membuf_init(b);
  membuf_cpy(b, buf, size);

  tcp_tx.push(b);

  return size;
}

int ModemClient::available() { return membuf_size(&tcp_rx); }

int ModemClient::read() { return membuf_first(&tcp_rx); }

bool ModemClient::connected() { return state >= M_CONNECTED; }

bool ModemClient::initialized() { return state >= M_READY; }

bool ModemClient::isError() { return state == M_ERROR; }

const char *ModemClient::get_error() { return lastError; }

void ModemClient::modem_stop() {
  // Open connection
  membuf_str(&tx, "AT+CIPCLOSE=1");

  CHANGE_STATE(M_STOPPING);
}

void ModemClient::modem_off() {
  digitalWrite(pwr, LOW);
  CHANGE_STATE(M_POWER_OFF);
}

void ModemClient::modem_on() {
  digitalWrite(pwr, HIGH);
  CHANGE_STATE(M_POWER_ON);
}

void ModemClient::modem_reset() {
  modem_off();

  // Clear UART RX
  while (modem.available())
    modem.read();

  membuf_clear(&rx);
  membuf_clear(&tx);
  membuf_clear(&tcp_rx);
  tcp_tx.clear();

  pinMode(pwr, OUTPUT);

  modem.begin(MODEM_BAUD_RATE);

  CHANGE_STATE(M_RESET);
}

void ModemClient::modem_send_at() {
  membuf_str(&tx, "AT\n");

  CHANGE_STATE(M_POLL);
}

void ModemClient::modem_open_network() {
  membuf_str(&tx, "AT+NETOPEN\n");

  CHANGE_STATE(M_NET_OPEN);
}

void ModemClient::modem_connect() {
  // Open connection
  snprintf((char *)&_at, sizeof(_at), "AT+CIPOPEN=1,\"TCP\",%s,%d\n", ip, port);
  membuf_str(&tx, _at);

  CHANGE_STATE(M_CONNECTING);
}

void ModemClient::modem_send() {
  if (!tcp_tx.isEmpty()) {

    snprintf((char *)&_at, sizeof(_at), "AT+CIPSEND=1,%zu",
             membuf_size(tcp_tx.first()));
    membuf_str(&tx, _at);

    CHANGE_STATE(M_WAITING_TO_SEND);
  }
}

void ModemClient::_tick() {
  switch (state) {
  case M_ERROR:
    break;

  case M_POWER_OFF:
    // Do nothing. Forever.
    break;

  case M_RESET:
    // TODO: How long to keep off durring reset?
    if (TIME_HAS_BEEN(1000)) {
      modem_on();
    }
    break;

  // Hard power on modem
  case M_POWER_ON:
    // TODO: How long to wait before trying "AT" poll?
    if (TIME_HAS_BEEN(1000)) {
      modem_send_at();
    }
    break;

  // Waiting for OK response. Hard restart after 5 second
  case M_POLL:
    // TODO: How long to wait for AT?
    if (TIME_HAS_BEEN(1000)) {
      DEBUG_PRINT("[ERROR] Cell modem did not respond.\n");
      // TODO: Update lastError
      CHANGE_STATE(M_ERROR);
    }
    break;

  // Waiting for network open response
  case M_NET_OPEN:
    // TODO: How long to wait ?
    if (TIME_HAS_BEEN(5000)) {
      DEBUG_PRINT("[ERROR] Cell modem did not respond.\n");
      // TODO: Update lastError
      CHANGE_STATE(M_ERROR);
    }
    break;

  case M_READY:
    // Nothing to do
    break;

  case M_CONNECTING:
    // TODO: How long to wait ?
    if (TIME_HAS_BEEN(5000)) {
      DEBUG_PRINT("[ERROR] Cell modem did not respond.\n");
      // TODO: Update lastError
      CHANGE_STATE(M_ERROR);
    }
    break;

  case M_STOPPING:
    // TODO: How long to wait?
    if (TIME_HAS_BEEN(5000)) {
      DEBUG_PRINT("[ERROR] Cell modem disconnect timeout.\n");
      // TODO: Update lastError
      CHANGE_STATE(M_ERROR);
    }
    break;

  case M_CONNECTED:
    modem_send();
    break;

  case M_WAITING_TO_SEND:
    if (TIME_HAS_BEEN(100)) {
      DEBUG_PRINT("[ERROR] Cell modem did not accept TCP RX data.\n");
      // TODO: Update lastError
      CHANGE_STATE(M_ERROR);
    }

  case M_SENDING:
    struct MemBuffer *buf = tcp_tx.first();

    // Send queued AT command TX
    size_t allowed = modem.availableForWrite();
    size_t size = membuf_size(buf);

    if (allowed && size) {
      size_t len = size > allowed ? allowed : size;
      modem.write(membuf_head(buf), len);

      // Clean up if done sending
      if (membuf_shift(buf, len) == 0) {
        membuf_drop(tcp_tx.shift());
      }
    }

    break;
  }
}

// Private Methods
// Note: This function must be non-blocking
void ModemClient::_processSerial() {

  if (state == M_ERROR) {
    return;
  }

  // Receive any modem RX
  size_t available = modem.available();

  if (!available) {
    return;
  }

  // Copy UART bytes into buffer
  modem.readBytes(membuf_add(&rx, available), available);

  // We are receiving a packet, so don't process as typical AT commands
  // NOTE: The docs suggest there is NOT <CR><LF> at the end of TCP RX data
  if (tcp_bytes_to_recv) {
    size_t size = membuf_size(&rx);
    size_t len = tcp_bytes_to_recv > size ? size : tcp_bytes_to_recv;

    // Copy from typical AT RX buffer to TCP RX buffer
    membuf_take(membuf_add(&tcp_rx, len), &rx, len);
    tcp_bytes_to_recv -= len;
  }

  // If we are not actively receiving a packet, process UART as AT commands
  if (!tcp_bytes_to_recv) {
    char *line;

    // Send queued AT command TX
    size_t allowed = modem.availableForWrite();
    size_t size = membuf_size(&tx);

    if (allowed && size) {
      size_t len = size > allowed ? allowed : size;
      modem.write(membuf_head(&tx), len);
      membuf_shift(&tx, len);
    }

    // Loop over all available lines and drive state machine
    while ((line = membuf_getline(&rx)) != NULL) {

      if (strcmp(line, "OK") == 0) {
        if (state == M_POLL) {
          CHANGE_STATE(M_NET_OPEN);
          continue;
        }

      } else if (strcmp(line, "+NETOPEN") == 0) {
        int ret;

        if (sscanf(line, "+NETOPEN: 1,%d", &ret) != 1) {
          DEBUG_PRINT("[ERROR] Invalid NETOPEN response: %s\n", line);
          CHANGE_STATE(M_ERROR);
          break;
        }

        if (ret != 0) {
          DEBUG_PRINT("[ERROR] NETOPEN error: %s\n", line);
          CHANGE_STATE(M_ERROR);
          break;
        }

        CHANGE_STATE(M_READY);

      } else if (strcmp(line, "+CDNSGIP") == 0) {
        int ret;

        if (sscanf(line, "+CDNSGIP: %d,%*s,%15s", &ret, (char *)&ip) == 2) {
          DEBUG_PRINT("[ERROR] Invalid CDNSGIP response: %s\n", line);
          CHANGE_STATE(M_ERROR);
          break;
        }

        if (ret != 1) {
          DEBUG_PRINT("[ERROR] DNS lookup error: %s\n", line);
          CHANGE_STATE(M_ERROR);
          break;
        }

        // start connection
        modem_connect();

      } else if (strcmp(line, "+CIPOPEN") == 0) {
        int ret;

        if (sscanf(line, "+CIPOPEN: 1,%d", &ret) != 1) {
          DEBUG_PRINT("[ERROR] Invalid CIPOPEN response: %s\n", line);
          CHANGE_STATE(M_ERROR);
          break;
        }

        if (ret != 0) {
          DEBUG_PRINT("[ERROR] Connection error: %s\n", line);
          CHANGE_STATE(M_ERROR);
          break;
        }

        CHANGE_STATE(M_CONNECTED);

      } else if (strcmp(">", line) == 0) {
        if (state == M_WAITING_TO_SEND) {
          CHANGE_STATE(M_SENDING);
        } else {
          DEBUG_PRINT("[ERROR] Modem requested data without being in waiting "
                      "to send state %s\n",
                      line);
          CHANGE_STATE(M_ERROR);
          break;
        }

      } else if (strcmp(line, "+CIPSEND") == 0) {
        size_t rSend, cSend;

        if (sscanf(line, "+CIPSEND: 1,%zd,%zd", &rSend, &cSend) != 2) {
          DEBUG_PRINT("[ERROR] Invalid CIPSEND response: %s\n", line);
          CHANGE_STATE(M_ERROR);
          break;
        }

        if (rSend != cSend) {
          DEBUG_PRINT("[ERROR] Could only send parital packet: %s\n", line);
          CHANGE_STATE(M_ERROR);
          break;
        }

        CHANGE_STATE(M_CONNECTED);

      } else if (strcmp(line, "+CIPCLOSE") == 0) {
        int ret;

        if (sscanf(line, "+CIPCLOSE: 1,%d", &ret) != 1) {
          DEBUG_PRINT("[ERROR] Invalid CIPCLOSE response: %s\n", line);
          CHANGE_STATE(M_ERROR);
          break;
        }

        if (ret != 0) {
          DEBUG_PRINT("[ERROR] Connection close error: %s\n", line);
          CHANGE_STATE(M_ERROR);
          break;
        }

        CHANGE_STATE(M_NET_OPEN);

      } else if (strcmp(line, "+IPD") == 0) {
        size_t length;
        if (sscanf(line, "+IPD(%zd)", &length) != 1) {
          DEBUG_PRINT("[ERROR] Invalid CDNSGIP response: %s\n", line);
          CHANGE_STATE(M_ERROR);
          break;
        }

        tcp_bytes_to_recv = length;
        break;

      } else if (strcmp(line, "ERROR") == 0) {
        DEBUG_PRINT("[ERROR] Unknown ERROR response");
        CHANGE_STATE(M_ERROR);
        break;

      } else if (strcmp(line, "+CIPERROR") == 0) {
        DEBUG_PRINT("[ERROR] CIPERROR: %s", line);
        CHANGE_STATE(M_ERROR);
        break;

      } else if (strcmp(line, "+IP ERROR") == 0) {
        DEBUG_PRINT("[ERROR] IP Error: %s", line);
        CHANGE_STATE(M_ERROR);
        break;

      } else if (strcmp(line, "RECV FROM:") == 0) {
        // Ignore RECV FROM because we only have one tcp_rx buf.

      } else if (strcmp(line, "") == 0) {
        // Ignore blank lines

      } else {
        DEBUG_PRINT("[ERROR] Unknown message: %s", line);
        CHANGE_STATE(M_ERROR);
        break;
      }

      if (state == M_ERROR) {
        if (lastError != NULL) {
          free(lastError);
          lastError = NULL;
        }

        lastError = line;
        break;
      } else {
        free(line);
      }
    }
  }
}
