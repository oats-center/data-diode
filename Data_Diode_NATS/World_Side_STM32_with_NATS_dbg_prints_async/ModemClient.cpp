#include "ModemClient.h"

ModemClient::ModemClient(HardwareSerial modem, uint8_t pwr)
  : modem(modem), pwr(pwr) {

  // Buffer for modem AT TX
  membuf_init(&tx);

  // Buffer for modem AT RX
  membuf_init(&rx);

  // Buffer connection RX data
  membuf_init(&tcp_rx);

  // Buffer connection TX data
  membuf_init(&tcp_tx);
}

void ModemClient::init() {
  modem.begin(MODEM_BAUD_RATE);
  pinMode(pwr, OUTPUT);
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
  membuf_str(&tx, "\"\r\n");

  expected_ok++;

  CHANGE_STATE(M_DNS_LOOKUP);
  return 0;
}

void ModemClient::stop() {
  if (state >= M_CONNECTED) {
    return;
  }

  // TODO: stop even if not >= M_CONNECTED?
  modem_close();
}

void ModemClient::reset() {
  if (state == M_RESET) {
    return;
  }

  // TODO: stop even if not >= M_CONNECTED?
  modem_reset();
}

int ModemClient::available() {
  return membuf_size(&tcp_rx);
}

int ModemClient::read() {
  return membuf_first(&tcp_rx);
}

char *ModemClient::getline() {
  return membuf_getline(&tcp_rx);
}

bool ModemClient::connected() {
  return state >= M_CONNECTED;
}

bool ModemClient::initialized() {
  return state >= M_READY;
}

bool ModemClient::isError() {
  return state == M_ERROR;
}

size_t ModemClient::write(const uint8_t *buf, size_t size) {
  if (state < M_CONNECTED) {
    DEBUG_PRINT("WARN : write called when not connected\n");
    return -1;
  }

  membuf_push(&tcp_tx, buf, size);

  return size;
}

const char *ModemClient::get_error() {
  return lastError;
}

void ModemClient::modem_close() {
  // Open connection
  membuf_str(&tx, "AT+CIPCLOSE=1\r\n");
  expected_ok++;

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
  membuf_clear(&tcp_tx);

  expected_errors = 0;
  expected_ok = 0;

  CHANGE_STATE(M_RESET);
}

void ModemClient::modem_send_at() {
  membuf_str(&tx, "ATE0\r\n");
  expected_ok++;

  CHANGE_STATE(M_POLL);
}

void ModemClient::modem_set_apn() {
  membuf_str(&tx, "AT+CGDCONT=1,\"IP\",\"super\"\r\n");
  expected_ok++;
  CHANGE_STATE(M_SET_APN);
}

void ModemClient::modem_open_network() {
  DEBUG_PRINT("in modem_open_network\n");
  membuf_str(&tx, "AT+NETOPEN\r\n");
  expected_ok++;

  CHANGE_STATE(M_NET_OPEN);
}

void ModemClient::modem_connect() {
  // Open connection
  snprintf(_at, sizeof(_at), "AT+CIPOPEN=1,\"TCP\",\"%s\",%d\r\n", ip, port);
  membuf_str(&tx, _at);
  expected_ok++;

  CHANGE_STATE(M_CONNECTING);
}


void ModemClient::_tick() {
  if (state == M_ERROR || state == M_POWER_OFF || state == M_READY) {
    // In error, do nothing.

  } else if (state == M_RESET) {
    // TODO: How long to keep the modem power signal "low" to cause a physical reset?
    if (TIME_HAS_BEEN(500)) {
      modem_on();
    }

    // Hard power on modem
  } else if (state == M_POWER_ON) {
    if (TIME_HAS_BEEN(1000)) {
      modem_send_at();
    }

    // Waiting for OK response. Hard restart after 5 second
  } else if (state == M_POLL) {
    if (TIME_HAS_BEEN(1000)) {
      SET_ERROR("POLL timeout.");
      DEBUG_PRINT("[ERROR] POLL timeout.\n");
      CHANGE_STATE(M_ERROR);
    }

    // Waiting for network open response
  } else if (state == M_SET_APN) {
    if (TIME_HAS_BEEN(10000)) {
      SET_ERROR("SET_APN timeout.");
      DEBUG_PRINT("[ERROR] SET_APN timeout.\n");
      CHANGE_STATE(M_ERROR);
    }
  }
    else if (state == M_NET_OPEN) {
    if (TIME_HAS_BEEN(10000)) {
      SET_ERROR("NET_OPEN timeout.");
      DEBUG_PRINT("[ERROR] NET_OPEN timeout.\n");
      CHANGE_STATE(M_ERROR);
    }

  } else if (state == M_DNS_LOOKUP) {

    if (TIME_HAS_BEEN(10000)) {
      SET_ERROR("DNS LOOKUP timeout");
      DEBUG_PRINT("[ERROR] DNS LOOKUP timeout.\n");
      CHANGE_STATE(M_ERROR);
    }

  } else if (state == M_CONNECTING) {
    if (TIME_HAS_BEEN(10000)) {
      SET_ERROR("CONNECTING timeout.");
      DEBUG_PRINT("[ERROR] CONNECTING timeout.\n");
      CHANGE_STATE(M_ERROR);
    }

  } else if (state == M_STOPPING) {
    if (TIME_HAS_BEEN(10000)) {
      SET_ERROR("STOPPING timeout.");
      DEBUG_PRINT("[ERROR] STOPPING timeout.\n");
      CHANGE_STATE(M_ERROR);
    }

  } else if (state == M_CONNECTED) {
    size_t size = membuf_size(&tcp_tx);
    if (size) {
      // Chunk at the MTU size
      if (size > MODEM_MTU) {
        size = MODEM_MTU;
      }
      snprintf(_at, sizeof(_at), "AT+CIPSEND=1,%u\r\n", size);
      membuf_str(&tx, _at);
      expected_ok++;
      tcp_bytes_to_send = size;

      CHANGE_STATE(M_WAITING_TO_SEND);
    }

  } else if (state == M_WAITING_TO_SEND) {
    if (TIME_HAS_BEEN(1000)) {
      SET_ERROR("WAITING_TO_SEND timeout");
      DEBUG_PRINT("[ERROR] WAITING_TO_SEND timeout.\n");
      CHANGE_STATE(M_ERROR);
    }

  } else if (state == M_SENDING) {

    if (!tcp_bytes_to_send) {
      DEBUG_PRINT("[WARN] Got to M_SENDING with no bytes to send?");
      CHANGE_STATE(M_CONNECTED);
      return;
    }

    // Send queued AT command TX
    size_t allowed = modem.availableForWrite();

    if (allowed) {
      size_t len = tcp_bytes_to_send > allowed ? allowed : tcp_bytes_to_send;
      modem.write(membuf_head(&tcp_tx), len);
      //DEBUG_PRINT("TCP TX (%d): ", tcp_bytes_to_send);
      //Serial.write(membuf_head(&tcp_tx), len);
      //DEBUG_PRINT("\n");

      // Clean up if done sending
      membuf_shift(&tcp_tx, len);
      tcp_bytes_to_send -= len;

      if (tcp_bytes_to_send == 0) {
        //modem.write("\r\n");
        //expected_ok++;

        CHANGE_STATE(M_CONNECTED);
      }
    }
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

  // Handle RX work
  if (available) {
    modem.readBytes(membuf_add(&rx, available), available);
  }

  // Take bytes for TCP if needing too
  if (tcp_bytes_to_recv && tcp_bytes_to_recv <= membuf_size(&rx)) {
    membuf_take(membuf_add(&tcp_rx, tcp_bytes_to_recv), &rx, tcp_bytes_to_recv);
    DEBUG_PRINT("TCP RX (%d): %s\n", tcp_bytes_to_recv, membuf_head(&tcp_rx));
    tcp_bytes_to_recv = 0;
  }

  // If not RX'ing TCP data, then check for pending AT to write
  // FIXME: Can we send AT commands even when TCP data is being sent to us?
  if (!tcp_bytes_to_recv) {
    // Send queued AT command TX
    size_t allowed = modem.availableForWrite();
    size_t size = membuf_size(&tx);
    // Not in the middle of a prior command, have room to write, and have something to write
    // TODO: check for == 1 here is weird, but is right. We queue up a command, add 1 to expected_ok, and THEN send. So at this point, expecting 1 ok means nothing is going on and one is ready to go.
    if (expected_ok == 1 && expected_errors == 0 && allowed && size) {
      size_t len = size > allowed ? allowed : size;
      modem.write(membuf_head(&tx), len);
      DEBUG_PRINT("cell (ok: %d, err: %d): %s\n", expected_ok, expected_errors, membuf_head(&tx));

      membuf_shift(&tx, len);
    }
  }

  // Modem sends `>` rather than `>\r\n` when waiting for TCP data, so have to match it *before* the below getline
  if (state == M_WAITING_TO_SEND && strncmp((const char *)membuf_head(&rx), ">", 1) == 0) {
    // Consume the `>`
    membuf_first(&rx);
    CHANGE_STATE(M_SENDING);
  }

  // Process complete AT rx lines (but only if we are not rx'ing TCP data)
  char *line;
  while (tcp_bytes_to_recv == 0 && (line = membuf_getline(&rx)) != NULL) {
    if (strcmp(line, "") != 0) {
      DEBUG_PRINT("line (ok: %d, err: %d)= %s\n", expected_ok, expected_errors, line);
    }

    if (strncmp(line, "OK", 2) == 0) {
      if (expected_ok <= 0) {
        DEBUG_PRINT("[ERROR] Too many `OK` returned from modem?\n");
        SET_ERROR(line);
        CHANGE_STATE(M_ERROR);
        break;
      }

      expected_ok--;

  	  if (state == M_SET_APN) {
        modem_open_network();
      }

      if (state == M_POLL) {
        modem_set_apn();
      }
	  
    } 
  else if (strncmp(line, "+NETOPEN", 8) == 0) {
      int ret;

      if (sscanf(line, "+NETOPEN: %d", &ret) != 1) {
        DEBUG_PRINT("[ERROR] Invalid NETOPEN response: %s\n", line);
        SET_ERROR(line);
        CHANGE_STATE(M_ERROR);
        break;
      }

      if (ret != 0) {
        DEBUG_PRINT("[ERROR] NETOPEN error: %s\n", line);
        SET_ERROR(line);
        CHANGE_STATE(M_ERROR);
        break;
      }

      CHANGE_STATE(M_READY);

    } else if (strncmp(line, "+CDNSGIP", 8) == 0) {
      int ret;
      if (sscanf(line, "+CDNSGIP: %d,%*s", &ret) == 2) {
        DEBUG_PRINT("[ERROR] Invalid CDNSGIP response: %s\n", line);
        SET_ERROR(line);
        CHANGE_STATE(M_ERROR);
        break;
      }

      if (ret != 1) {
        DEBUG_PRINT("[ERROR] DNS lookup error: %s\n", line);
        SET_ERROR(line);
        CHANGE_STATE(M_ERROR);
        break;
      }

      strtok(line, "\"");
      strtok(NULL, "\"");
      strtok(NULL, "\"");
      char *s = strtok(NULL, "\"");

      if (s != NULL) {
        strcpy(ip, s);
      } else {
        // The normal error code/change_state(error) code
        DEBUG_PRINT("[ERROR] DNS lookup error: %s\n", line);
        SET_ERROR(line);
        CHANGE_STATE(M_ERROR);
        break;
      }
      // start connection
      modem_connect();

    } else if (strncmp(line, "+CIPOPEN", 8) == 0) {
      int ret;

      if (sscanf(line, "+CIPOPEN: 1,%d", &ret) != 1) {
        DEBUG_PRINT("[ERROR] Invalid CIPOPEN response: %s\n", line);
        SET_ERROR(line);
        CHANGE_STATE(M_ERROR);
        break;
      }

      // Already have an open connection
      if (ret == 4) {
        DEBUG_PRINT("[WARN] Existing connection. Closing and reconnecting.\n");
        expected_errors++;
        expected_ok--;
        modem_close();
        break;
      }

      if (ret != 0) {
        DEBUG_PRINT("[ERROR] Connection error: %s\n", line);
        SET_ERROR(line);
        CHANGE_STATE(M_ERROR);
        break;
      }

      CHANGE_STATE(M_CONNECTED);

    } else if (strncmp(line, "+CIPSEND", 8) == 0) {
      int rSend, cSend;

      if (sscanf(line, "+CIPSEND: 1,%d,%d", &rSend, &cSend) != 2) {
        DEBUG_PRINT("[ERROR] Invalid CIPSEND response: %s\n", line);
        SET_ERROR(line);
        CHANGE_STATE(M_ERROR);
        break;
      }

      if (rSend != cSend) {
        DEBUG_PRINT("[ERROR] Could only send parital packet: %s\n", line);
        SET_ERROR(line);
        CHANGE_STATE(M_ERROR);
        break;
      }

      CHANGE_STATE(M_CONNECTED);

    } else if (strncmp(line, "+CIPCLOSE", 9) == 0) {
      int ret;

      if (sscanf(line, "+CIPCLOSE: 1,%d", &ret) != 1) {
        DEBUG_PRINT("[ERROR] Invalid CIPCLOSE response: %s\n", line);
        SET_ERROR(line);
        CHANGE_STATE(M_ERROR);
        break;
      }

      if (ret != 0) {
        DEBUG_PRINT("[ERROR] Connection close error: %s\n", line);
        SET_ERROR(line);
        CHANGE_STATE(M_ERROR);
        break;
      }

      CHANGE_STATE(M_READY);

    } else if (strncmp(line, "+IPD", 4) == 0) {
      size_t length;
      if (sscanf(line, "+IPD%zd", &length) != 1) {
        DEBUG_PRINT("[ERROR] Invalid CDNSGIP response: %s\n", line);
        SET_ERROR(line);
        CHANGE_STATE(M_ERROR);
        break;
      }

      tcp_bytes_to_recv = length;

      // Critical: The next bytes are NOT AT commands
      break;

    } else if (strncmp(line, "ERROR", 5) == 0) {
      if (expected_errors) {
        expected_errors--;
      } else {
        DEBUG_PRINT("[ERROR] Unknown ERROR response\n");
        SET_ERROR(line);
        CHANGE_STATE(M_ERROR);
      }

    } else if (strncmp(line, "+CIPERROR", 9) == 0) {
      int ret;

      if (sscanf(line, "+CIPERROR: %d", &ret) != 1) {
        DEBUG_PRINT("[ERROR] Invalid CIPERROR response: %s\n", line);
        SET_ERROR(line);
        CHANGE_STATE(M_ERROR);
        break;
      }

      if (ret == 4) {
        expected_errors++;
        expected_ok--;
        CHANGE_STATE(M_READY);
      } 
      /*
      else if (ret == 8) {
        expected_errors++;
        expected_ok--;
        //CHANGE_STATE(M_CONNECTED);
      }
      */
      else {
        DEBUG_PRINT("[ERROR] CIPERROR: %s\n", line);
        SET_ERROR(line);
        CHANGE_STATE(M_ERROR);
      }

    } else if (strcmp(line, "+IP ERROR: Network is already opened") == 0) {
      expected_errors++;
      expected_ok--;
      CHANGE_STATE(M_READY);

    } else if (strncmp(line, "+IPCLOSE", 8) == 0) {
      int ret;
      if (sscanf(line, "+IPCLOSE: 1,%d", &ret) != 1) {
        DEBUG_PRINT("[ERROR] Invalid IPCLOSE message: %s\n", line);
        SET_ERROR(line);
        CHANGE_STATE(M_ERROR);
        break;
      }

      // Socket closed due to something other than it being requested
      if (ret != 0) {
        // Account for the OK expected with command that prompted the closure notice.
        expected_ok--;
      }

      CHANGE_STATE(M_READY);

    } else if (strncmp(line, "+IP ERROR", 9) == 0) {
      DEBUG_PRINT("[ERROR] IP Error\n");
      SET_ERROR(line);
      CHANGE_STATE(M_ERROR);

    } 
    else if (state == M_WAITING_TO_SEND && strncmp((const char *)membuf_head(&rx), ">", 1) == 0) {
    // Consume the `>`
    membuf_first(&rx);
    CHANGE_STATE(M_SENDING);
    }
    else if (strncmp(line, "RECV FROM:", 10) == 0) {
      // Ignore RECV FROM because we only have one tcp_rx buf.

    } else if (strcmp(line, "") == 0) {
      // Ignore blank lines

    } 
    else if (
      strncmp(line, "RDY", 3) == 0 || strncmp(line, "+CPIN: READY", 12) == 0 || strncmp(line, "SMS DONE", 8) == 0 || strncmp(line, "PB DONE", 7) == 0 || strncmp(line, "ATE0", 4) == 0) {
      // Ignore init messages

    } else {
      DEBUG_PRINT("[ERROR] Unknown message: %s\n", line);
      SET_ERROR(line);
      CHANGE_STATE(M_ERROR);
    }

    free(line);
  }
}