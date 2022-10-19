  int packet_size = 0;
  int num_rx = 1;
  int bytes_read;
  #define UDP_TX_PACKET_MAX_SIZE 800
  unsigned char packetBuffer[UDP_TX_PACKET_MAX_SIZE] = {0};
  HardwareSerial Serial1(PE7, PE8); //(Rx, Tx), UART7 below D1 and D0

  void setup() {
    Serial1.begin(115200);
    Serial.begin(115200);
    delay(1000);
    Serial.println(" Processor Side");
    delay(500);
    Serial.println("Ready to Receive");
  }
 
  void loop() {
    packet_size = Serial1.read();
    delay(2);
    if((packet_size > 0) && (packet_size < UDP_TX_PACKET_MAX_SIZE))
    {
        Serial.print("pkt");
        Serial.print(num_rx++);
        Serial.print(" : ");
        
        Serial1.readBytes(packetBuffer,packet_size);
        for (int i = 0; i < packet_size; i++)
        {
          Serial.print(packetBuffer[i], HEX);             
        }
        Serial.println();
    }
  }