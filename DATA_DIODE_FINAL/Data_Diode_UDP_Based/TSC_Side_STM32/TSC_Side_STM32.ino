#include <LwIP.h>
#include <STM32Ethernet.h>
#include <EthernetUdp.h>  // UDP library from: bjoern@cs.stanford.edu 
#define BAUD_RATE 115200
#define UDP_TX_PACKET_MAX_SIZE 300
#define CIRCULAR_BUF_SIZE 3
#define UNIQUE_ID_SIZE 12 //96bit or 12 byte unique STM ID

// local port to listen on; fixed to 6053 on most TSCs. But on some TSCs it will be 6054
unsigned int localPort = 6053;  
//IP address set in the TSC 
IPAddress ip(169, 254, 168, 234); 

HardwareSerial Serial1(PE7, PE8); //(Rx, Tx), UART7 below D1 and D0
byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };
byte unique_id[UNIQUE_ID_SIZE]; //buffer to hold the MAC address of traffic signal controller (tsc)
IPAddress gateway(169, 254, 0, 1);
IPAddress myDns(169, 254, 0, 1);
IPAddress subnet(255, 255, 0, 0);

// An EthernetUDP instance to let us send and receive packets over UDP
EthernetUDP Udp;

int num_pkt = 1; //num_pkt will count the number of pkts
// buffers for receiving and sending data
int packetSize[CIRCULAR_BUF_SIZE] = {0};
char packetBuffer[CIRCULAR_BUF_SIZE][UDP_TX_PACKET_MAX_SIZE]; 
unsigned char circular_buf_idx = 0;
unsigned char current_buf_idx = 0;
char myChar;

void udp_parse_and_read()
{
  packetSize[circular_buf_idx] = Udp.parsePacket();
   if ((packetSize[circular_buf_idx] > 0) && (packetSize[circular_buf_idx] < UDP_TX_PACKET_MAX_SIZE))
   {
      Udp.read(packetBuffer[circular_buf_idx], packetSize[circular_buf_idx]);
      circular_buf_idx = (circular_buf_idx+1) % CIRCULAR_BUF_SIZE;
   }
}
void setup() {
  Serial1.begin(BAUD_RATE);
  Serial.begin(BAUD_RATE);
  Ethernet.begin(mac, ip, myDns, gateway, subnet);
  Udp.begin(localPort);

  Serial.print("Unique ID of Transmitter is : ");
  for (int i =0; i < UNIQUE_ID_SIZE; i++)
  {
    byte *unique_id_ptr = (byte *)(0x1FF0F420 + i);
    unique_id[i] = *(unique_id_ptr);
    Serial.print(unique_id[i],HEX);
  }
  Serial.println();
  delay(1000);

  Serial.println("Traffic Signal Controller");
  Serial.println("   Ready to Transmit  ");
  delay(500);
}

void loop() {
  // if there's data available, read a packet
    udp_parse_and_read();
    if ((packetSize[current_buf_idx] > 0) && (packetSize[current_buf_idx] < UDP_TX_PACKET_MAX_SIZE))
    {
    Serial.print("pkt num ");
    Serial.print(num_pkt++);
    Serial.print("  :");
    Serial1.write(packetSize[current_buf_idx]);
    // ************* read the packet into packetBufffer *******************
    for (int i = 0; i < packetSize[current_buf_idx] + UNIQUE_ID_SIZE ; i++) 
    {
      //reading the data in loop so that incoming data is not missed while in loop
      //Any data read here will be written in adjacent circular buffer so that existing data will not be overwritten
      //Circular buffer designed precisely for this reason 
      udp_parse_and_read(); 

      if(i == UNIQUE_ID_SIZE)
      {
        Serial.print("__");
      }

      if(i < UNIQUE_ID_SIZE)
      {
        Serial1.print((char)(unique_id[i]));
        Serial.print(unique_id[i],HEX);
      }
      else
      {
        Serial1.print(packetBuffer[current_buf_idx][i-UNIQUE_ID_SIZE]);
        myChar = packetBuffer[current_buf_idx][i-UNIQUE_ID_SIZE];
        Serial.print(myChar, HEX);
      }
      delayMicroseconds(100);
    }    
    packetSize[current_buf_idx] = 0;
    current_buf_idx = (current_buf_idx + 1) % CIRCULAR_BUF_SIZE;
    Serial.println();
    }
  }