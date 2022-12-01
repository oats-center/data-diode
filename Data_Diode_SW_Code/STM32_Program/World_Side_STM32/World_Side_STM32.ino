#define BAUD_RATE 115200
#define UDP_TX_PACKET_MAX_SIZE 300
#define UNIQUE_ID_SIZE 12

//User to modify the ServerIP and Server_Port as per the receiver's public IP address and UDP Port
char ServerIP[] = "128.46.213.185";
char Server_Port[] = "6969";

int packet_size = 0;
int num_rx = 1;
int bytes_read;
char packetBuffer[UDP_TX_PACKET_MAX_SIZE + UNIQUE_ID_SIZE] = {0};
unsigned char pktBuffer_character;
HardwareSerial Data_Diode_Serial(PE7, PE8); //(Rx, Tx), UART7 below D0 and D1

char APN[] = "super";
char aux_string[50];
HardwareSerial Sim7600_Serial(PG9, PG14); //(Rx, Tx) UART6 i.e. D0 and D1

uint8_t sendATcommand(const char* ATcommand, const char* expected_answer, unsigned int timeout) {
  
    uint8_t response_idx=0, answer=0;
    char response[100];
    unsigned long previous;
    memset(response, '\0', 100);    // Initialize the string
    delay(100);
    while( Sim7600_Serial.available() > 0) Sim7600_Serial.read();    // Clean the input buffer
     
     Serial.println("-------------------------------------------------------------------------------");
     Serial.print("Sending CMD : ");
     Serial.println(ATcommand);
     Sim7600_Serial.println(ATcommand);    // Send the AT command 

    // this loop waits for the answer 
    Serial.print("response from modem : ");
    previous = millis();
    do{
        if(Sim7600_Serial.available() != 0){    
            // if there are data in the UART input buffer, reads it and checks for the asnwer
            response[response_idx] = Sim7600_Serial.read();      
            Serial.print(response[response_idx]);
            response_idx++;
            // check if the desired answer  is in the response of the module
            if (strstr(response, expected_answer) != NULL)    
            {
                answer = 1;
            }
        }
    // Waits for the asnwer with time out
    }while((answer == 0) && ((millis() - previous) < timeout));

    Serial.println();
    Serial.println("-------------------------------------------------------------------------------");
    return answer;

}

void PowerOn(){
   uint8_t answer = 0;
  // checks if the module is started
  while (answer == 0) {     // Send AT every two seconds and wait for the answer
    answer = sendATcommand("AT", "OK", 2000);
	  delay(1000);
  }
}

void initialize_modem()
{
  PowerOn();

  //restart the module with a pulse to the PWR pin
  digitalWrite(PF13, HIGH);
  delay(500);
  digitalWrite(PF13, LOW);
  delay(500);

  sendATcommand("AT+NETOPEN", "+NETOPEN: 0", 5000);
  delay(100);
  snprintf(aux_string, sizeof(aux_string), "AT+CIPOPEN=1,\"%s\",,,%s", "UDP", Server_Port);
  sendATcommand(aux_string, "+CIPOPEN: 1,0", 5000);
  delay(100);
}

void setup() {
  pinMode(PF15,OUTPUT);
  digitalWrite(PF15,HIGH);
  
  Serial.begin(BAUD_RATE); // this is the traffic print on Serial monitor for user reference
  Data_Diode_Serial.begin(BAUD_RATE);
  Sim7600_Serial.begin(BAUD_RATE);
  delay(1000);

  Serial.println("=====================================================================");
  Serial.println("=====================  INITIALIZING MODEM  ==========================");
  initialize_modem();
  Serial.println("===================  DONE MODEM INITIALIZING  =======================");
  Serial.println("=====================================================================");
  Serial.println();
  Serial.println("===================  WORLD SIDE OF DIODE  ==========================");
  Serial.println("=================== READY TO RECEIVE DATA ==========================");
  
}
 
  void loop() {
    packet_size = Data_Diode_Serial.read();
    delay(2);
    if((packet_size > 0) && (packet_size < UDP_TX_PACKET_MAX_SIZE))
    //intentionally left commented to allow only a fixed size data read if needed
    //if( (packet_size == 241)|| (packet_size == 245))  
    {
        //Opens a UDP connection
        snprintf(aux_string, sizeof(aux_string), "AT+CIPSEND=1,,\"%s\",%s",ServerIP, Server_Port);
        Sim7600_Serial.println(aux_string); 

        if(num_rx < 10) 
          Serial.print(" ");
        if(num_rx < 100) 
          Serial.print(" ");
        Serial.print("pkt");
        Serial.print(num_rx++);
        Serial.print(" : ");
         Data_Diode_Serial.readBytes(packetBuffer,packet_size + UNIQUE_ID_SIZE);

        for (int i = 0; i < packet_size + UNIQUE_ID_SIZE; i++)
        {
          if(i == UNIQUE_ID_SIZE)
          {
            Serial.print("__|"); //Just to allow readability between the Unique ID and SPaT Data 
          }
          pktBuffer_character = (unsigned char)packetBuffer[i];
          Serial.print(pktBuffer_character < 16 ? "0" : "");
          Serial.print(pktBuffer_character, HEX);
          Serial.print("|");
          Sim7600_Serial.print(pktBuffer_character < 16 ? "0" : "");
          Sim7600_Serial.print(pktBuffer_character, HEX);
        }        
        //Character 1A to indicate "finish data send" to the modem; "" is ASCII equivalent of 1A
        Sim7600_Serial.println("");   
        Serial.println();
    }
  }