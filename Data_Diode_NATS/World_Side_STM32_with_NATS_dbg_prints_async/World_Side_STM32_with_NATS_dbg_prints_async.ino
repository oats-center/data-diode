#include "ModemClient.h"
#include "ArduinoNATS.h"
#include "CircularBuffer.h"
#include "MemoryBuffer.h"
#include <Arduino.h>
#include <LwIP.h>

#define BAUD_RATE 115200
#define UNIQUE_ID_SIZE 12

int packet_size = 0;
int num_rx = 1;
int bytes_read;

HardwareSerial Diode(PE7, PE8);  //(Rx, Tx), UART7 below D0 and D1
HardwareSerial Modem(PG9, PG14);
ModemClient client(Modem, D7);

char server_domain_name[] = "ibts-compute.ecn.purdue.edu";
char nats_username[] = "diode";
char nats_password[] = "9c7TCRO";
char unique_id_char[2];  // buffer to hold the 12 byte Unique address of traffic
                         // signal controller (tsc)
String unique_id = "";
char pub_subject[33];

NATS nats(&client, server_domain_name, 4223, nats_username, nats_password);

struct MemBuffer diodeRx;
uint8_t *bufptr = NULL;
uint16_t curpos = 0;
uint16_t data_size = 0;
uint16_t available = 0;
CircularBuffer<char *, 20> natsPending;

void nats_on_connect() {
  nats.publishf("connect", "{\"id\": \"%s\", \"lastError\": \"%s\"}", unique_id, client.lastError ? client.lastError : "");
}

void setup() {
  // Init the diode memory buffer
  Serial.begin(BAUD_RATE);
  Diode.begin(38400);
  client.init();
  delay(500);
  membuf_init(&diodeRx);

  // Unlimited reconnect attempts
  nats.max_reconnect_attempts = -1;
  // Send a message on connect
  nats.on_connect = &nats_on_connect;

  DEBUG_PRINT("calling reset\n");
  client.reset();
  DEBUG_PRINT("out of reset\n");

  Serial.println("===============================================================");
  Serial.print("Unique ID of Transmitter is : ");
  for (int i = UNIQUE_ID_SIZE - 1; i >= 0; i--) {
    uint8_t *unique_id_ptr = (byte *)(0x1FF0F420 + i);
    sprintf(unique_id_char, "%02X", (*(unique_id_ptr)));
    unique_id += unique_id_char;
  }
  sprintf(pub_subject, "traffic.%s", unique_id);
  Serial.println(unique_id.c_str());

  //init_modem_and_nats_connect();
  Serial.println();
  Serial.println("===================  WORLD SIDE OF DIODE  ==========================");
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

  while (!natsPending.isEmpty()) {
    char *msg = natsPending.shift();
    Serial.printf("New message. Length = %d data=%s\n", strlen(msg), msg);
    nats.publish(pub_subject, msg);
    free(msg);
  }
  
  nats.process();
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
