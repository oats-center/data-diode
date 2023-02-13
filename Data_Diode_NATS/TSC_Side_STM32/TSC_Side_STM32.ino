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

CircularBuffer<SpatFrame, 20> spatPending; 
CircularBuffer<DiodeFrame, 20> diodePending;

// debug: track rx-ed and dropped frame count
uint64_t rx = 0;
uint64_t dropped = 0;

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
  DiodeFrame curFrame;

  // Convert a SPaT frame if avaiable
  if(!spatPending.isEmpty() && !diodePending.isFull()) {
    struct SpatFrame spat = spatPending.pop();
    diodePending.push(buildDiodeFrame(spat));
    drop(&spat);
  }

  // Get next diode frame, if done with last one
  if (curFrame.data == NULL) {
    DEBUG_PRINT("Passing new frame. Rx: %llu Dropped: %llu Pending: %u\n", rx, dropped, spatPending.size());
    curFrame = diodePending.pop();
  }

  // Tend to diode UART 
  tendDiode(&curFrame);
}

DiodeFrame buildDiodeFrame(SpatFrame spat) {    
  struct DiodeFrame frame = {
    length: spat.length,
    pos: 0,
    data: NULL
  };

  // Build the diode frame
  // Note: It is a bit wasteful to copy `frame.data` into a new buffer only to copy it to the Serial TX buffer,
  //       but I like the separation of concerns (diode frame is constructed in the logic loop)
  //       and we have the computational power.
  frame.data = (byte *)malloc(spat.length + sizeof(spat.length));
  memcpy(&spat.length, &frame.data[0], sizeof(spat.length)); // Add length to frame
  memcpy(spat.data, &frame.data[2], spat.length);

  return frame;
}

void tendDiode(DiodeFrame *frame) {
  // Nothing to do
  if (frame->pos == frame->length) {
    return;
  }

  // Check if the Serial TX buffer has space
  uint16_t allowed = Serial.availableForWrite();

  if (allowed > 0) {
    uint16_t len = frame->length - frame->pos;
    // Send as much as we can, without overflowing the TX buffer
    len = len > allowed ? allowed : len;
    Serial.write(&frame->data[frame->pos], len);
    frame->pos += len;
  }
  
  if (frame->length >= frame->pos) {
    drop(frame);
  }
}

// This function will be called when new data arrives. Treat like an ISR and just copy the data into a buffer for the main loop
void handleUDP() {
  struct SpatFrame frame;
  frame.length = tscUDP.parsePacket();
  frame.data = (byte *)malloc(frame.length);
  tscUDP.read(frame.data, frame.length);

  // We could do this check earlier, but we *have* to read the packet anyway, so if full just throw it away.
  if(!spatPending.isFull()) {
    spatPending.push(frame);
    rx++;
  } else {
    DEBUG_PRINT("WARN: Pending buffer FULL. Dropping SPaT frame.");
    dropped++;
  }
}

// Free the dynamic memory allocated
void drop(SpatFrame *frame) {
  free(frame->data);
  frame->data = NULL;
}

// Free the dynamic memory allocated
void drop(DiodeFrame *frame) {
  free(frame->data);
  frame->data = NULL;
}