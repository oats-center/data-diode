#include <Arduino.h>
#include <ModemClient.h>
#include "ArduinoNATS.h"
#include <LwIP.h>
#include "CircularBuffer.h" 

#define BAUD_RATE 115200
#define UNIQUE_ID_SIZE 12
#define UDP_TX_PACKET_MAX_SIZE 1000
struct natsmsg {
  uint16_t length { 0 };
  uint8_t *data { NULL };  
};

int packet_size = 0;
int num_rx = 1;
int bytes_read;
char* packetBuffer;
HardwareSerial Data_Diode_Serial(PE7, PE8); //(Rx, Tx), UART7 below D0 and D1
ModemClient client(PG9,PG14); //rx,tx pins

char server_domain_name[] = "ibts-compute.ecn.purdue.edu";
char nats_username[] = "diode";	
char nats_password[] = "9c7TCRO";
char publish_string[100];
char unique_id_char[2]; //buffer to hold the 12 byte Unique address of traffic signal controller (tsc)
string unique_id = "";

NATS nats(
	&client,
	server_domain_name , 4223, 
  nats_username,nats_password
);

void init_modem_and_nats_connect()
{
  int is_modem_init = 0;
    do
    {
      is_modem_init = client.initialize_modem();
    }while(!is_modem_init);
    while(!nats.connect());
    nats.process();
}

uint8_t *bufptr = NULL;
uint16_t curpos = 0;
uint16_t data_size = 0;
uint16_t available = 0;
CircularBuffer<natsmsg, 20> natsPending; 

void setup () {
    Serial.begin(BAUD_RATE);
    client.begin_serial(BAUD_RATE);
    Data_Diode_Serial.begin(38400);
    delay(1000);
    
	Serial.println("===============================================================");
    Serial.print("Unique ID of Transmitter is : ");
    for (int i = UNIQUE_ID_SIZE-1; i >= 0; i--)
    {
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


}

void serialEventRun(void)
{
  if (serialEvent7 && Data_Diode_Serial.available()) {
    serialEvent7();
  }
}

void serialEvent7()
{

//nats.process();
  if((NULL == bufptr) && (Data_Diode_Serial.available()>=2))
  {
    //Data_Diode_Serial.read((uint8_t*)&data_size, 2);
    data_size = Data_Diode_Serial.read() ;
    data_size = data_size + (Data_Diode_Serial.read() << 8);
    //data_size = 241;
    bufptr = (uint8_t*)malloc(sizeof(uint8_t)*data_size);
    Serial.printf("data_size = %d \n",data_size);
  }
  available = Data_Diode_Serial.available();

  if((available>0) && bufptr != NULL)
  {
    Serial.printf("point1 available = %lu curpos = %lu data_size=%lu\n",available,curpos,data_size);
    if(curpos == data_size)
    {
      return;
    }
    uint16_t len = (uint16_t)data_size - (uint16_t)curpos;
    len = (len > available)?available:len;
    Data_Diode_Serial.readBytes(bufptr+curpos, len);
    /*
    Serial.print("data read : ");
    for (int i = curpos; i<curpos+len; i++)
    {
      if(*(bufptr+i) < 16)
      Serial.print(0);
      Serial.print(*(bufptr+i),HEX);
    }
    Serial.println();
    */
    curpos += len;
    Serial.printf("point2 available = %lu curpos = %lu data_size=%lu len=%lu\n",available,curpos,data_size,len);
  }

  if(bufptr != NULL && curpos >= data_size)
  {
    struct natsmsg msg;
    msg.length = data_size;
    msg.data = bufptr;
    natsPending.push(msg);
    Serial.printf("natsPending.isFull = %lu natsPending.length = %lu\n",natsPending.isFull(), natsPending.size());
    curpos=0;
    bufptr = NULL;
  }
}





