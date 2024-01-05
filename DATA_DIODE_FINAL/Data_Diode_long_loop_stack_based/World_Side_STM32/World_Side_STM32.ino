#include <Arduino.h>
#include <ModemClient.h>
#include "ArduinoNATS.h"
#include <LwIP.h>

uint32_t my_timeMark;
bool time_mark_done = false;
#define TIME_HAS_BEEN(x) millis() - my_timeMark > x
#define TIME_MARK() my_timeMark = millis();

#define BAUD_RATE 115200
#define UDP_TX_PACKET_MAX_SIZE 300
#define UNIQUE_ID_SIZE 12
#define BLUE_STATUS_LED PB7
#define RED_STATUS_LED PB14
bool comm_status = false;

int packet_size = 0;
int num_rx = 1;
int mdm_send_error = 0;
int reconn_atmpts = 0;
int bytes_read;
int mdm_ser_available_bytes;
char mdm_ser_response[50];
char packetBuffer[UDP_TX_PACKET_MAX_SIZE] = {0};
unsigned char pktBuffer_character;
HardwareSerial Data_Diode_Serial(PE7, PE8); //(Rx, Tx), UART7 below D0 and D1
ModemClient client(PG9,PG14); //rx,tx pins

//User to modify the ServerIP and Server_Port as per the receiver's public IP address and UDP Port
//char ServerIP[] = "128.46.213.185";
//char Server_Port[] = "6969";
//IPAddress ServerIP(128,46,213,185);

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
  digitalWrite(RED_STATUS_LED, HIGH);
  digitalWrite(BLUE_STATUS_LED, LOW);
  int is_modem_init = 0;
  mdm_send_error = 0;
    do
    {
      is_modem_init = client.initialize_modem();
      if(!is_modem_init)
      {
        Serial.println("          RESETTING THE MODEM                ");	
        Serial.println("==================================================");	
        digitalWrite(D7, LOW);	
        delay(500);	
        digitalWrite(D7, HIGH);	
        delay(15000);
      }
    }while(!is_modem_init);
    while(!nats.connect());
    nats.process();
    client.sendATcmd_mdm_cli("ATE0", "OK", 1000);
  snprintf(publish_string, sizeof(publish_string), "Sending Data = %d", reconn_atmpts );
  delay(500);
  reconn_atmpts++;
  Serial.print("publishing sample message # ");   
  Serial.println(reconn_atmpts); 	
  nats.publish("my_traffic", publish_string);	
  Serial.println();
  Serial.println("===================  WORLD SIDE OF DIODE  ==========================");
  Serial.println(reconn_atmpts);
  Serial.println("=================== READY TO RECEIVE DATA ==========================");
  digitalWrite(RED_STATUS_LED, LOW);
  digitalWrite(BLUE_STATUS_LED, HIGH);
  
  delay(500);
}

void setup () {
    Serial.begin(BAUD_RATE);
    client.begin_serial(BAUD_RATE);
    Data_Diode_Serial.begin(BAUD_RATE);
    pinMode(D7,OUTPUT);
    pinMode(BLUE_STATUS_LED, OUTPUT);
    pinMode(RED_STATUS_LED, OUTPUT);
    digitalWrite(D7,HIGH);
    
	Serial.println("===============================================================");
    Serial.print("Unique ID of Transmitter is : ");
    for (int i = UNIQUE_ID_SIZE-1; i >= 0; i--)
    {
      byte *unique_id_ptr = (byte *)(0x1FF0F420 + i);
      sprintf(unique_id_char, "%02X", (*(unique_id_ptr)));
      unique_id += unique_id_char;
    }
    Serial.println(unique_id.c_str());
    Serial.println("initializing modem");
  
  init_modem_and_nats_connect();
  memset(mdm_ser_response, '\0', 50);

}

// the loop function runs over and over again forever
void loop() {
//nats.process();
packet_size = Data_Diode_Serial.read();
    //if((packet_size > 0) && (packet_size < UDP_TX_PACKET_MAX_SIZE))
    //intentionally left commented to allow only a fixed size data read if needed
  if((packet_size > 0) && (packet_size < UDP_TX_PACKET_MAX_SIZE))
  {
    if( (packet_size == 241)|| (packet_size == 245))  
    {
        while( client.ModemSerial.available() > 0) client.ModemSerial.read();
        client.initiate_tcp_write();
          if(num_rx < 10) 
            //Serial.print(" ");
          if(num_rx < 100) 
            //Serial.print(" ");
          //Serial.print("pkt");
          //Serial.print(num_rx++);
          //Serial.print(" : ");
          Data_Diode_Serial.readBytes(packetBuffer,packet_size);
          mdm_ser_available_bytes = client.ModemSerial.available();
          if(client.ModemSerial.available() != 0){
            client.ModemSerial.readBytes(mdm_ser_response,mdm_ser_available_bytes);
          }
          mdm_ser_response[mdm_ser_available_bytes] = '\0';
          snprintf(publish_string, sizeof(publish_string), "PUB traffic.%s %d",unique_id.c_str(),2*(packet_size) );
          client.ModemSerial.println(publish_string);
          for (int i = 0; i < packet_size ; i++)
          {
            pktBuffer_character = (unsigned char)packetBuffer[i];
            //Serial.print(pktBuffer_character < 16 ? "0" : "");
            //Serial.print(pktBuffer_character, HEX);
            //Serial.print("|");
            client.hexbytewrite(pktBuffer_character);
          }        
          //client.hexbytewrite((unsigned char)packetBuffer[0]);
          client.ModemSerial.println();
          //Character 1A to indicate "finish data send" to the modem; "" is ASCII equivalent of 1A  
          client.stoptcpwrite();
          //Serial.println();
          //Serial.println(mdm_ser_response[2]);
          if(strchr(mdm_ser_response, '>') == NULL)
          {
            //Serial.println();
            if(mdm_ser_response[13] != '8')
            {
              for (int i = 0; i<mdm_ser_available_bytes;i++)
               {
                  Serial.print(mdm_ser_response[i]);
               }
               mdm_send_error++;
               if(mdm_send_error == 1 && time_mark_done==false ) {
                 TIME_MARK();
                 time_mark_done = true;
               }

               if(TIME_HAS_BEEN(2000))
               {
                   time_mark_done = false;                   
                   if(mdm_send_error > 15)  //let me make sure its actually an error
                      Serial.print(reconn_atmpts);
                      init_modem_and_nats_connect();
                      while( Data_Diode_Serial.available() > 0) Data_Diode_Serial.read();
                    }
                  else
                  {
                    mdm_send_error = 0;
                  }
                }
             }
    }
    else
    { 
      while( Data_Diode_Serial.available() > 0) Data_Diode_Serial.read(); //clean the input buffer
      
    }
  }
  else
  {
    //nats.process();
  }
}

