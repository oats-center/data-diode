#include <Arduino.h>
#include <ModemClient.h>
#include "ArduinoNATS.h"
#include <LwIP.h>

#define BAUD_RATE 115200
#define UDP_TX_PACKET_MAX_SIZE 300
#define UNIQUE_ID_SIZE 12

int packet_size = 0;
int num_rx = 1;
int bytes_read;
char packetBuffer[UDP_TX_PACKET_MAX_SIZE] = {0};
unsigned char pktBuffer_character;
HardwareSerial Data_Diode_Serial(PE7, PE8); //(Rx, Tx), UART7 below D0 and D1
ModemClient client(PG9,PG14); //rx,tx pins

//User to modify the ServerIP and Server_Port as per the receiver's public IP address and UDP Port
//char ServerIP[] = "128.46.213.185";
//char Server_Port[] = "6969";
//IPAddress ServerIP(128,46,213,185);
char server_domain_name[] = "ecn-199-238.dhcp.ecn.purdue.edu"; 
char publish_string[100];
char unique_id_char[2]; //buffer to hold the MAC address of traffic signal controller (tsc)
string unique_id = "";

NATS nats(
	&client,
	server_domain_name , NATS_DEFAULT_PORT, 
  "diode","9c7TCRO"
);


void setup () {
    Serial.begin(BAUD_RATE);
    client.begin_serial(BAUD_RATE);
    Data_Diode_Serial.begin(BAUD_RATE);
    delay(1000);
    
    Serial.print("Unique ID of Transmitter is : ");
    for (int i = UNIQUE_ID_SIZE-1; i >= 0; i--)
    {
      byte *unique_id_ptr = (byte *)(0x1FF0F420 + i);
      sprintf(unique_id_char, "%02X", (*(unique_id_ptr)));
      Serial.print(unique_id_char);
      Serial.print("__");
      Serial.print((*(unique_id_ptr)),HEX);
      Serial.print("==");
      unique_id += unique_id_char[0];
      unique_id += unique_id_char[1];
    }
    Serial.println();
    Serial.println(unique_id.c_str());
    Serial.println("initializing modem");
    int is_modem_init = 0;
    do
    {
      is_modem_init = client.initialize_modem();
    }while(!is_modem_init);
    while(!nats.connect());
    nats.process();

  delay(3000);
  Serial.println("publishing get ready");    
  nats.publish("traffic", "Get Ready");
  delay(3000);
  Serial.println("publishing sending data");    
  nats.publish("traffic", "Sending Data");
  delay(3000);
  Serial.println("publishing once more");    
  nats.publish("traffic", "once more");
  delay(3000);
  Serial.println("publishing are you there?");    
  nats.publish("traffic", "are you there?");
 
  Serial.println();
  Serial.println("===================  WORLD SIDE OF DIODE  ==========================");
  Serial.println("=================== READY TO RECEIVE DATA ==========================");
}

// the loop function runs over and over again forever
void loop() {
//nats.process();
packet_size = Data_Diode_Serial.read();
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
        snprintf(publish_string, sizeof(publish_string), "PUB my_traffic.%s %d",unique_id.c_str(),2*(packet_size) );
        client.ModemSerial.println(publish_string);
        for (int i = 0; i < packet_size ; i++)
        {
          pktBuffer_character = (unsigned char)packetBuffer[i];
          Serial.print(pktBuffer_character < 16 ? "0" : "");
          Serial.print(pktBuffer_character, HEX);
          Serial.print("|");
          client.hexbytewrite(pktBuffer_character);
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

