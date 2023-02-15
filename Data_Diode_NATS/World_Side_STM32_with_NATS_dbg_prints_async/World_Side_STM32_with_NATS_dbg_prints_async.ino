#include <Arduino.h>
#include <ModemClient.h>
#include "ArduinoNATS.h"
#include <LwIP.h>
#include "CircularBuffer.h"

#define BAUD_RATE 115200
#define UNIQUE_ID_SIZE 12
#define UDP_TX_PACKET_MAX_SIZE 1000
struct natsmsg {
  uint16_t length{ 0 };
  uint8_t *data{ NULL };
};

int packet_size = 0;
int num_rx = 1;
int bytes_read;

uint8_t input[1000];
size_t inputPos = 0;

HardwareSerial Diode(PE7, PE8);  //(Rx, Tx), UART7 below D0 and D1
ModemClient client(PG9, PG14);   //rx,tx pins

char server_domain_name[] = "ibts-compute.ecn.purdue.edu";
char nats_username[] = "diode";
char nats_password[] = "9c7TCRO";
char publish_string[100];
char unique_id_char[2];  //buffer to hold the 12 byte Unique address of traffic signal controller (tsc)
string unique_id = "";

NATS nats(
  &client,
  server_domain_name, 4223,
  nats_username, nats_password);

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
CircularBuffer<struct natsmsg *, 20> natsPending;

void setup() {
  Serial.begin(BAUD_RATE);
  client.begin_serial(BAUD_RATE);
  Diode.begin(38400);
  delay(1000);

  Serial.println("===============================================================");
  Serial.print("Unique ID of Transmitter is : ");
  for (int i = UNIQUE_ID_SIZE - 1; i >= 0; i--) {
    byte *unique_id_ptr = (byte *)(0x1FF0F420 + i);
    sprintf(unique_id_char, "%02X", (*(unique_id_ptr)));
    unique_id += unique_id_char;
  }
  Serial.println(unique_id.c_str());

  Serial.println();
  Serial.println("===================  WORLD SIDE OF DIODE  ==========================");
  Serial.println("=================== READY TO RECEIVE DATA ==========================");
}

// the loop function runs over and over again forever
void loop() {
  processDiode();

  while(!natsPending.isEmpty()) {
    struct natsmsg *msg = natsPending.shift();
    Serial.printf("New message. Length = %d\n", msg->length);
    drop(msg);
  }
  //nats.process();
}

void processDiode() {
  size_t available = Diode.available();

  size_t oldPos = inputPos;
  Diode.readBytes(input, available);
  inputPos += available;

  size_t start = 0;
  for (size_t i = oldPos; i < inputPos; i++) {
    if (input[i] == '\n') {
      struct natsmsg *msg = (struct natsmsg *)malloc(sizeof(struct natsmsg));

      msg->length = i - start;

      msg->data = (uint8_t *)malloc(msg->length * sizeof(uint8_t));
      memcpy(msg->data, &input[start], msg->length);
      natsPending.push(msg);

      start = i + 1;
    }
  }

  // Shift buffer foward
  if(start != 0) {
    memmove(input, &input[start], inputPos - start);
    inputPos = 0;
  }
}

void drop(struct natsmsg *msg) {
  free(msg->data);
  free(msg);
}