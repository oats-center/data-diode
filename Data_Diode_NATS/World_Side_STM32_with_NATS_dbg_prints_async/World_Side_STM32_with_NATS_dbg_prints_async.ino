#include "ArduinoNATS.h"
#include "CircularBuffer.h"
#include "MemoryBuffer.h"
#include "ModemClient.h"
#include <Arduino.h>
#include <LwIP.h>

#define BAUD_RATE 115200
#define UNIQUE_ID_SIZE 12

MemoryBuffer input;

int packet_size = 0;
int num_rx = 1;
int bytes_read;

struct Buffer diodeRx;
HardwareSerial Diode(PE7, PE8);  //(Rx, Tx), UART7 below D0 and D1
ModemClient client(PG9, PG14);   // rx,tx pins

char server_domain_name[] = "ibts-compute.ecn.purdue.edu";
char nats_username[] = "diode";
char nats_password[] = "9c7TCRO";
char publish_string[100];
char unique_id_char[2];  // buffer to hold the 12 byte Unique address of traffic
                         // signal controller (tsc)
string unique_id = "";

NATS nats(&client, server_domain_name, 4223, nats_username, nats_password);

void init_modem_and_nats_connect() {
  int is_modem_init = 0;
  do {
    is_modem_init = client.initialize_modem();
  } while (!is_modem_init);
  while (!nats.connect())
    ;
  nats.process();
}

uint8_t *bufptr = NULL;
uint16_t curpos = 0;
uint16_t data_size = 0;
uint16_t available = 0;
CircularBuffer<char *, 20> natsPending;

void setup() {
  // Init the diode memory buffer
  membuf_init(&diodeRx);

  pinMode(D7, OUTPUT);
  digitalWrite(D7, HIGH);
  delay(5000);
  Serial.begin(BAUD_RATE);
  client.begin_serial(BAUD_RATE);
  Diode.begin(38400);
  delay(1000);

  Serial.println(
    "===============================================================");
  Serial.print("Unique ID of Transmitter is : ");
  for (int i = UNIQUE_ID_SIZE - 1; i >= 0; i--) {
    byte *unique_id_ptr = (byte *)(0x1FF0F420 + i);
    sprintf(unique_id_char, "%02X", (*(unique_id_ptr)));
    unique_id += unique_id_char;
  }
  Serial.println(unique_id.c_str());

  init_modem_and_nats_connect();
  Serial.println();
  Serial.println(
    "===================  WORLD SIDE OF DIODE  ==========================");
  Serial.println(
    "=================== READY TO RECEIVE DATA ==========================");
  digitalWrite(D7, LOW);
  delay(10000);
}

// the loop function runs over and over again forever
void loop() {
  process_diode();

  while (!natsPending.isEmpty()) {
    char *msg = natsPending.shift();
    Serial.printf("New message. Length = %d data=%s\n", strlen(msg), msg->data);
    free(msg);
  }
  // nats.process();
}

void process_diode() {
  size_t available = Diode.available();

  if (!available) {
    return;
  }

  // Copy UART bytes into buffer
  Diode.readBytes(membuf_add(&diodeRx, available), available);

  // Find lines and queue
  uint8_t *line;
  while ((line = membuf_getline(&diodeRx)) != NULL) {
    natsPending.push(line);
  }
}
