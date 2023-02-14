#include <Arduino.h>
#include <ModemClient.h>
#include "ArduinoNATS.h"
#include <LwIP.h>

#define BAUD_RATE 115200
#define UDP_TX_PACKET_MAX_SIZE 300
#define UNIQUE_ID_SIZE 12

int packet_size_lsb = 0;
int packet_size_usb = 0;
int packet_size = 0;
int num_rx = 1;
int bytes_read;
char packetBuffer[UDP_TX_PACKET_MAX_SIZE] = {0};
unsigned char pktBuffer_character;
HardwareSerial Data_Diode_Serial(PE7, PE8); //(Rx, Tx), UART7 below D0 and D1
ModemClient client(PG9,PG14); //rx,tx pins

char server_domain_name[] = "ibts-compute.ecn.purdue.edu";
char nats_username[] = "diode";
char nats_password[] = "9c7TCRO";
char publish_string[100];
char unique_id_char[2]; 
string unique_id = ""; //buffer to hold the 12 byte Unique address of traffic signal controller (tsc)

NATS nats(
	&client,
	server_domain_name , 4223, 
  nats_username, nats_password
);


void setup () {	
    Serial.begin(BAUD_RATE);
    client.begin_serial(BAUD_RATE);
    Data_Diode_Serial.begin(BAUD_RATE);
    delay(4000);
    
    Serial.println("===============================================================");
    Serial.print("Unique ID of Transmitter is : ");
    for (int i = UNIQUE_ID_SIZE-1; i >= 0; i--)
    {
      byte *unique_id_ptr = (byte *)(0x1FF0F420 + i);
      sprintf(unique_id_char, "%02X", (*(unique_id_ptr)));
      unique_id += unique_id_char[0];
      unique_id += unique_id_char[1];
    }
    Serial.println(unique_id.c_str());
    Serial.println("===============================================================");

    int is_modem_init = 0;
    do
    {
      is_modem_init = client.initialize_modem();
    }while(!is_modem_init);
    while(!nats.connect());
    nats.process();

  delay(3000);
  Serial.println("publishing sample message");    
  nats.publish("my_traffic", "Sending Data");
  nats.publish("my_traffic", "Get Ready");
  Serial.println();

  Serial.println("===================  WORLD SIDE OF DIODE  ==========================");
  Serial.println("=================== READY TO RECEIVE DATA ==========================");
}

// the loop function runs over and over again forever
void loop() {
//nats.process();
packet_size_lsb = Data_Diode_Serial.read();
packet_size_usb = Data_Diode_Serial.read();
packet_size = (packet_size_usb << 16) + packet_size_lsb;
    delay(2);
    //if((packet_size > 0) && (packet_size < UDP_TX_PACKET_MAX_SIZE))
    //intentionally left commented to allow only a fixed size data read if needed
  if((packet_size > 0) && (packet_size < UDP_TX_PACKET_MAX_SIZE))
  {
    if( (packet_size == 241)|| (packet_size == 245))  
    {
        //Opens a UDP connection
        client.initiate_tcp_write();
        
        if(num_rx < 10) 
          Serial.print(" ");
        if(num_rx < 100) 
          Serial.print(" ");
        Serial.print("pkt");
        Serial.print(num_rx++);
        Serial.print(" : ");
        Data_Diode_Serial.readBytes(packetBuffer,packet_size);
        snprintf(publish_string, sizeof(publish_string), "PUB traffic.%s %d",unique_id.c_str(),packet_size );
        client.ModemSerial.println(publish_string);
        for (int i = 0; i < packet_size ; i++)
        {
          pktBuffer_character = (unsigned char)packetBuffer[i];
          Serial.print(pktBuffer_character < 16 ? "0" : "");
          Serial.print(pktBuffer_character, HEX);
          Serial.print("|");
          client.ModemSerial.print(pktBuffer_character);
        }        
        //client.hexbytewrite((unsigned char)packetBuffer[0]);
        client.ModemSerial.println();
        //Character 1A to indicate "finish data send" to the modem; "" is ASCII equivalent of 1A  
        client.stoptcpwrite();
        Serial.println();
    }
    else
    {
      while( Data_Diode_Serial.available() > 0) Data_Diode_Serial.read(); //clean the input buffer
    }
  }
  }

