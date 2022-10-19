#include <LwIP.h>
#include <STM32Ethernet.h>
#include <EthernetUdp.h>  // UDP library from: bjoern@cs.stanford.edu 12/30/2008
#define UDP_TX_PACKET_MAX_SIZE 800
#define CIRCULAR_BUF_SIZE 3
#define UDP_PACKET_DATA_START_BYTE 42

HardwareSerial Serial1(PE7, PE8); //(Rx, Tx), UART7 below D1 and D0

byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };
IPAddress ip0(169, 254, 0, 1);
IPAddress ip(169, 254, 173, 252);
IPAddress gateway(169, 254, 0, 1);
IPAddress myDns(169, 254, 0, 1);
IPAddress subnet(255, 255, 0, 0);
unsigned int localPort = 6054;  // local port to listen on

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
  Serial1.begin(115200);
  Serial.begin(115200);
  Ethernet.begin(mac, ip, myDns, gateway, subnet);
  Udp.begin(localPort);
  Serial.println("Traffic Signal Controller");
  delay(500);
  Serial.println("   Ready to Transmit  ");
}

void loop() {
  // if there's data available, read a packet
    udp_parse_and_read();
    if ((packetSize[current_buf_idx] > 0) && (packetSize[current_buf_idx] < UDP_TX_PACKET_MAX_SIZE))
    {
    Serial.print("pkt num ");
    Serial.print(num_pkt++);
    Serial.print("  :");
    Serial1.write(packetSize[current_buf_idx]-UDP_PACKET_DATA_START_BYTE);
    // ************* read the packet into packetBufffer *******************
    for (int i = 0; i < packetSize[current_buf_idx] ; i++) 
    {
      udp_parse_and_read();
      if(i>=UDP_PACKET_DATA_START_BYTE)
      {
         Serial1.print(packetBuffer[current_buf_idx][i]);
         myChar = packetBuffer[current_buf_idx][i];
         Serial.print(myChar, HEX);
         delayMicroseconds(100);
      }
      else
      {
        myChar = packetBuffer[current_buf_idx][i];
        Serial.print(myChar, HEX);
      }
    }    
    packetSize[current_buf_idx] = 0;
    current_buf_idx = (current_buf_idx + 1) % CIRCULAR_BUF_SIZE;
    Serial.println();
    }
  }