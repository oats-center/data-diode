#define CIRCULAR_BUFFER_INT_SAFE

#include <LwIP.h>
#include <STM32Ethernet.h>
#include <EthernetUdp.h>  // UDP library from: bjoern@cs.stanford.edu 
#include "CircularBuffer.h"
#include "CRC32.h"
#include "base64.hpp"

#define DEBUG
#ifdef DEBUG
  #define DEBUG_PRINT(...) Serial.printf(__VA_ARGS__)
#else 
  #define DEBUG_PRINT(...)
#endif

#define VERSION "0.3.0"
#define BAUD_RATE 115200
#define SPAT_BROADCAST_PORT 6053 

struct Frame {
  size_t length;
  size_t pos;
  uint8_t *data;  
};

CircularBuffer<struct Frame *, 20> pending; 
Frame *curFrame = NULL;
CRC32 crc;

// debug: track rx-ed and dropped frame count
uint32_t rx = 0;
uint32_t dropped = 0;

//IP address set in the TSC 
IPAddress ip(169, 254, 168, 234); 
IPAddress subnet(255, 255, 0, 0);

HardwareSerial Diode(PE7, PE8); //(Rx, Tx), UART7 below D1 and D0

// An EthernetUDP instance to let us send and receive packets over UDP
EthernetUDP tscUDP;

void setup() {
  // Debug console
  Serial.begin(BAUD_RATE);
  Serial.println("=== Traffic Signal Controller ===");
  Serial.println("Version: " VERSION);

  Serial.println("Opening serial port to world side...");
  Diode.begin(38400);

  Serial.println("Connecting to traffic signal controler SPaT broadbast...");
  Ethernet.begin(ip, subnet);
  tscUDP.onDataArrival(handleUDP);
  tscUDP.begin(SPAT_BROADCAST_PORT);
  
  Serial.println("Diode started.");
}

void loop() {
  // Get next diode frame, if done with last one
  if (curFrame == NULL && !pending.isEmpty()) {
    curFrame = pending.shift();
    //DEBUG_PRINT("Sending rx: %d, dropped:%d data=%s\n", rx, dropped, curFrame->data);
    DEBUG_PRINT("Sending rx: %d, dropped:%d\n", rx, dropped);
  }
  // Tend to diode UART
  if(curFrame != NULL) {
    sendDiode(curFrame);
    
    // We read through the frame!
    if (curFrame->pos >= curFrame->length) {
      drop(curFrame);
      curFrame = NULL;
    }
 }
}

void sendDiode(struct Frame *frame) {
  // Check if the Serial TX buffer has space
  uint16_t allowed = Diode.availableForWrite();
  //DEBUG_PRINT("allowed =%d\n",allowed);
  if (allowed > 0) {
    DEBUG_PRINT("allowed =%d\n",allowed);
    uint16_t len = frame->length - frame->pos;
    DEBUG_PRINT("point1 len=%d frame->length=%d frame->pos=%d\n",len,frame->length,frame->pos);
    // Send as much as we can, without overflowing the TX buffer
    len = len > allowed ? allowed : len;
    Diode.write(&frame->data[frame->pos], len);
    
    frame->pos += len;
    //DEBUG_PRINT("point2 len=%d frame->length=%d frame->pos=%d\n",len,frame->length,frame->pos);
  }
}

// This function will be called when new data arrives.
void handleUDP() {
  DEBUG_PRINT("===IN handleUDP===\n");
  struct Frame *frame = (struct Frame *)calloc(1, sizeof(struct Frame));

  size_t pktLen = tscUDP.parsePacket();
  size_t dataLength = pktLen + 4; // pkt + CRC32
  
  // Read pkt data in  
  uint8_t *data = (uint8_t *)malloc(dataLength*sizeof(uint8_t)); // Add NULL to make it print safe
  tscUDP.read(data, pktLen);

  // Compute CRC32
  crc.reset();
  crc.add(data, pktLen);
  uint32_t crc32 = crc.getCRC();
  
  // Add CRC32 to data
  memcpy(&data[pktLen], &crc32, 4);
  
  size_t b64Length = encode_base64_length(dataLength);   
  
  // Add base64 encoded data
  frame->data = (uint8_t *)malloc(b64Length + 2); // base64 data + \n + \0 (so it's printf safe for debugging)
  encode_base64(data, dataLength, frame->data);

// Add \n frame marker
  frame->data[b64Length] = '\n';
  frame->data[b64Length + 1] = '\0';
  frame->length = b64Length + 1;

  // Cean up UDP data 
  free(data);

  // We could do this check earlier, but we *have* to read the packet anyway, so if full just throw it away.
  if(!pending.isFull()) {
    DEBUG_PRINT("pending buf size : =%d\n", pending.size());
    pending.push(frame);
    rx++;
  } else {
    DEBUG_PRINT("WARN: Pending buffer FULL. Dropping SPaT frame.\n");
    dropped++;
  }

  DEBUG_PRINT("===OUT handleUDP===\n");
}

void drop(Frame *frame) {
  free(frame->data);
  free(frame);
}