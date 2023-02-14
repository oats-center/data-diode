#define CIRCULAR_BUFFER_INT_SAFE
#include <LwIP.h>
#include <STM32Ethernet.h>
#include <EthernetUdp.h>  // UDP library from: bjoern@cs.stanford.edu 
#include <CircularBuffer.h> // Sure, CB's aren't that hard, but if we going to live in the land of Arduino we might as well drink the wine...

#define DEBUG
#ifdef DEBUG
  #define DEBUG_PRINT(...) Serial.printf(__VA_ARGS__)
#else 
  #define DEBUG_PRINT(...)
#endif

#define VERSION "0.2.0"
#define BAUD_RATE 115200
#define SPAT_BROADCAST_PORT 6053 

struct SpatFrame {
  uint16_t length { 0 };
  byte *data { NULL };  
};

struct DiodeFrame {
  uint16_t length { 0 };
  uint16_t pos { 0 };
  byte *data { NULL };
};

CircularBuffer<SpatFrame*, 20> spatPending; 
CircularBuffer<DiodeFrame*, 20> diodePending;
DiodeFrame *curFrame;

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
  Diode.begin(BAUD_RATE);

  Serial.println("Connecting to traffic signal controler SPaT broadbast...");
  Ethernet.begin(ip, subnet);
  tscUDP.onDataArrival(handleUDP);
  tscUDP.begin(SPAT_BROADCAST_PORT);
  
  Serial.println("Diode started.");
}

// This *should* be async too, but we don't have anything else to do so this works.
void loop() {
    //DEBUG_PRINT("LOOP\n");
  // Convert a SPaT frame if avaiable
  if(!spatPending.isEmpty() && !diodePending.isFull()) {
    struct SpatFrame *spat = spatPending.pop();
    struct DiodeFrame *frame = (struct DiodeFrame *)malloc(sizeof(struct DiodeFrame));
    
    buildDiodeFrame(frame, spat);
    DEBUG_PRINT("in_main_loop");
    DEBUG_PRINT("loop pos: %lu\n", frame->pos);
    DEBUG_PRINT("=============================\n");
    diodePending.push(frame);
    drop(spat);
    //drop(frame);
  }

  // Get next diode frame, if done with last one
  if (curFrame == NULL && !diodePending.isEmpty()) {
    DEBUG_PRINT("Passing new frame. Rx: %lu Dropped: %lu Diode Pending: %u Spat Pending: %u\n", rx, dropped, diodePending.size(),spatPending.size());
    curFrame = diodePending.pop();
        DEBUG_PRINT(" curFrame->pos: %lu\n", curFrame->pos);
        DEBUG_PRINT("=============================\n");
  }

  // Tend to diode UART
  if(curFrame != NULL) {
    sendDiode(curFrame);
    
    if (curFrame->pos >= curFrame->length) {
      DEBUG_PRINT("DROP");
      drop(curFrame);
      curFrame = NULL;
    }
 }

}

void buildDiodeFrame(DiodeFrame * frame, SpatFrame *spat) {    
  frame->length = spat->length + sizeof(spat->length);
  frame->pos = 0;
  DEBUG_PRINT("in buildDiodeFrame\n");
  DEBUG_PRINT("pos : %lu\n", frame->pos);

  // Build the diode frame
  // Note: It is a bit wasteful to copy `frame.data` into a new buffer only to copy it to the Serial TX buffer,
  //       but I like the separation of concerns (diode frame is constructed in the logic loop)
  //       and we have the computational power.
  frame->data = (byte *)malloc(frame->length);
  memcpy(&frame->data[0],&spat->length, sizeof(spat->length)); // Add length to frame
  memcpy(&frame->data[sizeof(spat->length)],spat->data,spat->length);
  DEBUG_PRINT("pos : %lu\n", frame->pos);
  DEBUG_PRINT("length : %lu\n", frame->length);
  DEBUG_PRINT("=========================\n");
}

void sendDiode(DiodeFrame *frame) {
  // Nothing to do
  if (frame->pos == frame->length) {
    return;
  }
  DEBUG_PRINT("***********************************\n");
  DEBUG_PRINT("sendDiode \n",frame->pos);
  DEBUG_PRINT("frame->pos = %lu \n",frame->pos);
  // Check if the Serial TX buffer has space
  uint16_t allowed = Diode.availableForWrite();
  DEBUG_PRINT("allowed = %lu \n",allowed);
  if (allowed > 0) {
    uint16_t len = frame->length - frame->pos;
    // Send as much as we can, without overflowing the TX buffer
    DEBUG_PRINT("len1 = %lu\n",len);
    len = len > allowed ? allowed : len;
    DEBUG_PRINT("len2 = %lu\n",len);
    Diode.write(&frame->data[frame->pos], len);
    DEBUG_PRINT("frame->pos1 = %lu\n",frame->pos);
    frame->pos += len;
     DEBUG_PRINT("frame->pos2 = %lu\n",frame->pos);
  }
  DEBUG_PRINT("***********************************\n");

}

// This function will be called when new data arrives. Treat like an ISR and just copy the data into a buffer for the main loop
void handleUDP() {
  DEBUG_PRINT("######################################\n");
   DEBUG_PRINT("In handleUDP.\n");

  struct SpatFrame * frame = (struct SpatFrame *)malloc(sizeof(SpatFrame));

  frame->length = tscUDP.parsePacket();
  frame->data = (byte *)malloc(frame->length);
  tscUDP.read(frame->data, frame->length);
  
  DEBUG_PRINT("spatPending.isFull = %lu frame->length = %lu\n",spatPending.isFull(), frame->length);
  // We could do this check earlier, but we *have* to read the packet anyway, so if full just throw it away.
  if(!spatPending.isFull()) {
    spatPending.push(frame);
    rx++;
  } else {
    DEBUG_PRINT("WARN: Pending buffer FULL. Dropping SPaT frame.\n");
    dropped++;
  }
  DEBUG_PRINT("######################################\n");
}

// Free the dynamic memory allocated
void drop(SpatFrame *frame) {
  free(frame->data);
  frame->data = NULL;

  free(frame);
}

// Free the dynamic memory allocated
void drop(DiodeFrame *frame) {
  free(frame->data);
  frame->data = NULL;

  free(frame);
}