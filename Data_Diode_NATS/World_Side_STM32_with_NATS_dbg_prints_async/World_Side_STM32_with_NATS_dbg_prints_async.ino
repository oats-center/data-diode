#include "ModemClient.h"
#include "ArduinoNATS.h"
#include "CircularBuffer.h"
#include "MemoryBuffer.h"
#include <Arduino.h>
#include <LwIP.h>

#define BAUD_RATE 115200

#define UNIQUE_ID_ADDRESS 0x1FF0F420 // Specific to STM32F7xx series
#define UNIQUE_ID_SIZE 12
#define SUBJECT_LENGTH 8 + 2*UNIQUE_ID_SIZE + 1 // "traffic." + ID(as hex) + \0

HardwareSerial Diode(PE7, PE8);  //(Rx, Tx), UART7 below D0 and D1
HardwareSerial Modem(PG9, PG14);
ModemClient client(Modem, D7);

char unique_id[2*UNIQUE_ID_SIZE+1] = {0};
char pub_subject[SUBJECT_LENGTH] = {0};

NATS nats(&client, "ibts-compute.ecn.purdue.edu", 4223, "diode","9c7TCRO");

struct MemBuffer diodeRx;
CircularBuffer<char *, 20> natsPending;

void nats_on_connect() {
  nats.publishf("connect", "{\"event\": \"connected\", \"id\": \"%s\", \"lastError\": \"%s\"}", unique_id, client.lastError ? client.lastError : "");
}

void setup() {
  // Init the diode memory buffer
  Serial.begin(BAUD_RATE);
  // FIXME: Change back to 115200 
  Diode.begin(38400);
  client.init();

  membuf_init(&diodeRx);

  if(SERIAL_TX_BUFFER_SIZE != 256) {
    Serial.printf("[ERROR] SERIAL_TX_BUFFER_SIZE = %d. Please set to 256 for best performace!\n", SERIAL_TX_BUFFER_SIZE);
  }

  if(SERIAL_RX_BUFFER_SIZE != 256) {
    Serial.printf("[ERROR] SERIAL_RX_BUFFER_SIZE = %d. Please set to 256 for best performace!\n", SERIAL_RX_BUFFER_SIZE);
  }

  // Unlimited reconnect attempts
  nats.max_reconnect_attempts = -1;
  // Send connect message on connect
  nats.on_connect = &nats_on_connect;

  // Read chip ID (in place of a logical device ID or the TSC ID)
  uint8_t *p = (uint8_t *)UNIQUE_ID_ADDRESS;
  for (int i = 0; i < UNIQUE_ID_SIZE; i++) {
    sprintf(&unique_id[i*2], "%02X", p[UNIQUE_ID_SIZE-1-i]);
  }
  unique_id[UNIQUE_ID_SIZE*2] = '\0';
  // Build and store NATS subject for diode data
  snprintf(pub_subject, SUBJECT_LENGTH, "traffic.%s", unique_id);

  Serial.println("===================  WORLD SIDE OF DIODE  ==========================");
  Serial.printf("[INFO] Diode ID: %s\n", unique_id);
  Serial.printf("[INFO] Diode data subject: %s\n", pub_subject);
}

// the loop function runs over and over again forever
void loop() {

  if(client.isError()) {
    client.reset();
    return;
  } 

  if(client.initialized() && !client.connected()) {
    nats.connect();
  }

  process_diode();
  client.process();
  nats.process();

  while (!natsPending.isEmpty()) {
    char *msg = natsPending.shift();
    //Serial.printf("New message. Length = %d data=%s\n", strlen(msg), msg);
    Serial.printf("New message. Length = %d\n", strlen(msg), msg);
    nats.publish(pub_subject, msg);
    free(msg);
  }
}

void process_diode() {
  size_t available = Diode.available();

  if (!available) {
    return;
  }

  // Copy UART bytes into buffer
  Diode.readBytes(membuf_add(&diodeRx, available), available);

  // Find lines and queue
  char *line;
  while ((line = membuf_getline(&diodeRx)) != NULL) {
    natsPending.push(line);
  }
}
