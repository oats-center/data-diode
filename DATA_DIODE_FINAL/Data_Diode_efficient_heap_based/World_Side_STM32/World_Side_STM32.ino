#include "ModemClient.h"
#include "ArduinoNATS.h"
#include "CircularBuffer.h"
#include "MemoryBuffer.h"
#include <Arduino.h>
#include <LwIP.h>

#define SERIAL_BAUD_RATE 115200
#define DIODE_BAUD_RATE 38400
#define BLUE_STATUS_LED PB7
#define RED_STATUS_LED PB14

#define UNIQUE_ID_ADDRESS 0x1FF0F420 // Specific to STM32F7xx series
#define UNIQUE_ID_SIZE 12
#define SUBJECT_LENGTH 8 + 2*UNIQUE_ID_SIZE + 1 // "traffic." + ID(as hex) + \0

HardwareSerial Diode(PE7, PE8);  //(Rx, Tx), UART7 below D0 and D1
HardwareSerial Modem(PG9, PG14);
ModemClient client(Modem, D7); //Modem serialobject and pwr pin

char unique_id[2*UNIQUE_ID_SIZE+1] = {0};
char pub_subject[SUBJECT_LENGTH] = {0};
char pub_subject_err_msg[SUBJECT_LENGTH] = {0};
bool comm_status = false;
bool prev_comm_status = false;
NATS nats(&client, "ibts-compute.ecn.purdue.edu", 4223, "diode","9c7TCRO");

struct MemBuffer diodeRx;
CircularBuffer<char *, 20> natsPending;

void nats_on_connect() {
  nats.publishf(pub_subject_err_msg, "{\"event\": \"connected\", \"id\": \"%s\", \"lastError\": \"%s\"}", unique_id, client.lastError ? client.lastError : "");
}

void setup() {
  // Init the diode memory buffer
  Serial.begin(SERIAL_BAUD_RATE);
  pinMode(BLUE_STATUS_LED, OUTPUT);
  pinMode(RED_STATUS_LED, OUTPUT);
  digitalWrite(RED_STATUS_LED, !comm_status);
  digitalWrite(BLUE_STATUS_LED, comm_status);
  // FIXME: Change back to 115200 
  Diode.begin(DIODE_BAUD_RATE);
  client.init();

  membuf_init(&diodeRx);

  if(SERIAL_TX_BUFFER_SIZE != 256) {
    DEBUG_PRINT("[ERROR] SERIAL_TX_BUFFER_SIZE = %d. Please set to 256 for best performace!\n", SERIAL_TX_BUFFER_SIZE);
  }

  if(SERIAL_RX_BUFFER_SIZE != 256) {
    DEBUG_PRINT("[ERROR] SERIAL_RX_BUFFER_SIZE = %d. Please set to 256 for best performace!\n", SERIAL_RX_BUFFER_SIZE);
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
  snprintf(pub_subject_err_msg, SUBJECT_LENGTH, "connect.%s", unique_id);

  Serial.println("===================  WORLD SIDE OF DIODE  ==========================");
  DEBUG_PRINT("[INFO] Diode ID: %s\n", unique_id);
  DEBUG_PRINT("[INFO] Diode data subject: %s\n", pub_subject);
}

// the loop function runs over and over again forever
void loop() {
  check_led_status();

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
    //DEBUG_PRINT("New message. Length = %d data=%s\n", strlen(msg), msg);
    DEBUG_PRINT("New message. Length = %d\n", strlen(msg));
    nats.publish(pub_subject, msg);
    free(msg);
  }
}

void check_led_status()
{
  comm_status = client.connected();
  if(comm_status != prev_comm_status)
  {
    digitalWrite(RED_STATUS_LED, !comm_status);
    digitalWrite(BLUE_STATUS_LED, comm_status);
  }
  prev_comm_status = comm_status;
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
