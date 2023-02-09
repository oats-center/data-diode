extern "C" {
  #include "string.h"
}
#include "ModemClient.h"


uint16_t ModemClient::_srcport = 1024;

ModemClient::ModemClient(int rx, int tx) : 
        is_modem_initialised(false), 
        is_net_open(false), 
        is_tcp_open(false), 
        is_data_available(false), 
        tcp_read_data(""),
        payload_of_subscribe_cmd(""),
        ModemSerial(rx,tx)
      {

      }

	int ModemClient::initialize_modem() {	
  Serial.println("in my initialize modem");	
  uint8_t answer = 0;	
  // checks if the module is started	
  Serial.println("====================================");	
  Serial.println("        INITIALIZING MODEM          ");	
  unsigned long previous = millis();	
  while ( (answer == 0) && ((millis() - previous) < 15000) ) {     // Send AT every two seconds and wait for the answer	
    answer = sendATcmd_mdm_cli("AT", "OK", 2000);	
    delay(1000);	
  }	
  if(answer == 0){	
    	
    Serial.println("    MODEM INITIALIZATION TAKING LONG TIME");	
    Serial.println("          PLEASE RESET THE MODEM");	
    Serial.println("==================================================");	
    return answer;	
  }	
  else{	
    Serial.println("==== DONE MODEM INITIALIZATION  ====");	
  }	
  digitalWrite(PF13, HIGH);	
  delay(500);	
  digitalWrite(PF13, LOW);	
  delay(500);	
  answer = sendATcmd_mdm_cli("AT+NETOPEN", "+NETOPEN: 0", 5000);	
  if (answer)	
  {	
    Serial.println("Network Connection established with server");	
  }	
  else	
  {	
    Serial.println("Network Connection already established"); 	
  }	
  	
  is_net_open = true;	
  return true;	
  Serial.println("done my initialize modem");	
  	
  	
}

void ModemClient::begin_serial(int baud_rate) {
  ModemSerial.begin(baud_rate);
}

uint8_t ModemClient::sendATcmd_mdm_cli(const char* ATcommand, const char* expected_answer, unsigned int timeout) {
    uint8_t response_idx=0, answer=0;
    char response[100];
    unsigned long previous;
    memset(response, '\0', 100);    // Initialize the string
    delay(100);
    while( ModemSerial.available() > 0) ModemSerial.read();    // Clean the input buffer
     
    Serial.println("response from modem : "); 
    ModemSerial.println(ATcommand);    // Send the AT command 
    // this loop waits for the answer 
     previous = millis();
    do{
        if(ModemSerial.available() != 0){    
            // if there are data in the UART input buffer, reads it and checks for the asnwer
            response[response_idx] = ModemSerial.read();    
              response_idx++;
            // check if the desired answer  is in the response of the module
            if (strstr(response, expected_answer) != NULL)    
            {
                answer = 1;
            }
        }
         // Waits for the asnwer with time out
    }while((answer == 0) && ((millis() - previous) < timeout));
    Serial.println(response);
    return answer;
}

String ModemClient::IpAddress2String(const IPAddress& ipAddress) {
  return String(ipAddress[0]) + String(".") +\
  String(ipAddress[1]) + String(".") +\
  String(ipAddress[2]) + String(".") +\
  String(ipAddress[3]) ; 
}

string ModemClient::rx_tcp_data() {
    char aux_string[50];
    char response[1600];
    unsigned long previous;
    memset(response, '\0', 1600);    // Initialize the string
    snprintf(aux_string, sizeof(aux_string), "AT+CIPRXGET=2,1");    
    while( ModemSerial.available() > 0) ModemSerial.read();    // Clean the input buffer

    delay(100);
    
        uint8_t answer = 0;
        int i = 0;
        Serial.println("Rx response from modem : ");
        ModemSerial.println(aux_string); //send the AT command
        previous = millis();
        do {
          if(ModemSerial.available() != 0) {      
              // if there are data in the UART input buffer, reads it and checks for the asnwer
              response[i] = ModemSerial.read();    
              i++;

            if (strstr(response, "OK") != NULL) {
                answer = 1;
            }
          }
         // Waits for the asnwer with time out
        }while((answer == 0) && ((millis() - previous) < 1000));

    Serial.println(response);
    string str(response);
    return str;
}

string ModemClient::get_remainlen_or_arg3(string resp, int return_arg) {
  Serial.print("in get_remainlen_or_arg3 with ARG");
  Serial.println(return_arg);

  resp.erase(0, resp.find("AT+"));
  Serial.println("resp.c_str : ");
  Serial.println(resp.c_str());

  stringstream ss(resp);

  string convert_to_str;
  string temp_data;
  int i = 0;
  //typical resp is of the form AT+CIPRXGET=2,1 \n +CIPRXGET: 2,1,491,0 \n data \n payload \n OK; getline will split by newline;
  //payload only in case of response to subscribe command
  while (getline(ss, convert_to_str)) {  
          i++;
          if ((i == ARG2) && (return_arg == ARG2)){
            break;
          }
          else if ((i == ARG3) && (return_arg == ARG3)){
            temp_data = convert_to_str;
            continue;
          }
          else if (i == (ARG3+1))
          {
            break;            
          }
  }
  
  if(return_arg == ARG3) //return the read data
  {
    Serial.print("temp_data : ");
    Serial.println(temp_data.c_str());

    Serial.print("convert_to_str : ");
    Serial.println(convert_to_str.c_str());

    if (convert_to_str.length() > 0)
    {
        Serial.println("populating payload_of_subscribe_cmd ");
        Serial.println(convert_to_str.c_str());
        payload_of_subscribe_cmd = convert_to_str;
    }
    return temp_data;
  }
  //if return arg is 2, then return the remaining data length, which will be read next i.e. the 0 in +CIPRXGET: 2,1,491,0
  regex rgx("([^,]+)");
  sregex_iterator current(convert_to_str.begin(), convert_to_str.end(), rgx);
  smatch match = *(++(++(++current)));
  string match_str = match.str();
  return match_str;
  
}

int ModemClient::connect(const char* host, uint16_t port) {
      Serial.println();
      Serial.println("in ModemClient connect");
      Serial.println();

  uint8_t response_idx=0, answer=0, resp_answer=0;
  char response[100];
  char aux_string[300];

  snprintf(aux_string, sizeof(aux_string), "AT+CDNSGIP=\"%s\"", host);
  memset(response, '\0', 100);    // Initialize the string
  while( ModemSerial.available() > 0) ModemSerial.read();    // Clean the input buffer
  delay(100);
  ModemSerial.println(aux_string);    // Send the AT command 

  do{
    if(ModemSerial.available() != 0)
      {
        response[response_idx] = ModemSerial.read();    
        response_idx++;
        if (strstr(response, "OK") != NULL)    
          {
              resp_answer = 1;
          }
      }
  }while(resp_answer == 0);
   
  string str(response);
  regex rgx("\"([^\"]*)\""); // will split string delimited by "
  sregex_iterator current(str.begin(), str.end(), rgx);
  smatch match = *++(++current);
  string ServerIP = match.str(); 
  ServerIP.erase(remove(ServerIP.begin(), ServerIP.end(), '\"'), ServerIP.end()); //remove double quotes
  string Server_Port = to_string(port); 
  
  //establishing conn; logic same as connect(IPAddress ip, uint16_t port) function
  Serial.print("IP address of domain name \"");
  Serial.print(host);
  Serial.print("\" is ");
  Serial.println(ServerIP.c_str());

      sendATcmd_mdm_cli("AT+CIPCLOSE=1", "+CIPCLOSE: 1,0", 5000);
      sendATcmd_mdm_cli("AT+CIPRXGET=1", "OK", 5000);
      Serial.print("Opening TCP Connection on port ");
      Serial.print(Server_Port.c_str());
      Serial.print(" of IP Address ");
      Serial.println(ServerIP.c_str());
      snprintf(aux_string, sizeof(aux_string), "AT+CIPOPEN=1,\"%s\",\"%s\",%s", "TCP", ServerIP.c_str(), Server_Port.c_str());
      answer = sendATcmd_mdm_cli(aux_string, "+CIPOPEN: 1,0", 3000);
      if(!answer)
      {
        Serial.println("Issue with TCP CONNECT");
        Serial.println("Please reset the modem ");
        return false;
      }
      /*
      snprintf(aux_string, sizeof(aux_string), "CONNECT {"
						"\"verbose\": false,"
						"\"pedantic\": false,"
						"\"lang\": \"arduino\","
						"\"version\": \"1.0.0\","
						"\"user\": \"diode\","
						"\"pass\": \"9c7TCRO\""
					"}"
          );
      mywrite(aux_string);
      */
      Serial.println("TCP Connection Established with server");
      Serial.println("Waiting for Server response...");
      unsigned long previous = millis();
      while((0 == available()) && ((millis() - previous) < 5000)); //Wait for 5 seconds to receive the response for TCP connect
      if(!is_data_available)
      {
        Serial.println("Not received any response from Server");
        Serial.println("Restablishing Connection...");
        return false;
      }
      Serial.println("TCP Connection Established with server");
      Serial.println("Ready to transmit and receive messages");     

  is_tcp_open = true;
  return true;

}

int ModemClient::connect(IPAddress ip, uint16_t port) {
  //to do
  return 1;
}

void ModemClient::initiate_tcp_write() {
  char aux_string[20];
  snprintf(aux_string, sizeof(aux_string), "AT+CIPSEND=1,");
  ModemSerial.println(aux_string);
}

void ModemClient::hexbytewrite(uint8_t hexbyte){
    ModemSerial.print(hexbyte < 16 ? "0" : "");
    //Serial.print(hexbyte, HEX);
    ModemSerial.print(hexbyte, HEX);
}

void ModemClient::stoptcpwrite(){
    ModemSerial.println("");
}

int ModemClient::mywrite(const char* to_write) {
  delay(10);
  initiate_tcp_write();
  delay(10);
  Serial.println(to_write);
  ModemSerial.println(to_write);
  stoptcpwrite();

  return 1;
        
}
size_t ModemClient::write(uint8_t b) {
	  return write(&b, 1);
}

size_t ModemClient::write(const uint8_t *buf, size_t size) {
    mywrite((char*)buf);
    return 1;
        
}



int ModemClient::available() {
      Serial.println();
      Serial.println("in ModemClient avialable");
  if(is_data_available == false)
  {
     string tcp_resp_local;
    //sendATcmd_mdm_cli("AT+CIPRXGET=1", "OK", 5000);
    do
    {
      tcp_resp_local = rx_tcp_data();
      if (strstr(tcp_resp_local.c_str(), "ERROR") != NULL)  
        break;
      tcp_read_data += get_remainlen_or_arg3(tcp_resp_local,ARG3);
    } while(stoi(get_remainlen_or_arg3(tcp_resp_local,ARG2)));
    Serial.print("tcp_read_data before payload append: ");
    Serial.println(tcp_read_data.c_str());
    if(tcp_read_data.length() > 0)
    {
      if(payload_of_subscribe_cmd.length()>0)
      {
        tcp_read_data += '\n';
        tcp_read_data += payload_of_subscribe_cmd;
        payload_of_subscribe_cmd = "";
        Serial.print("tcp_read_data after payload append: ");
        Serial.println(tcp_read_data.c_str());
      } 
      is_data_available = true;
      for (int i = 0; i < tcp_read_data.length()-1; i++)
      {
          my_char_queue.push(tcp_read_data[i]);
      }
      my_char_queue.push('\0');
      tcp_read_data = "";
    }
    else
    {
      is_data_available = false;
      tcp_read_data = "";
      my_char_queue = queue<char>();
    }
  }
  Serial.print("returning : ");
  Serial.println(is_data_available);
  Serial.println("finished ModemClient avialable");
  Serial.println();
  return is_data_available;
}

int ModemClient::read() {
  uint8_t char_to_ret = 0;
    if (my_char_queue.front() != '\0')
    {
        char_to_ret = my_char_queue.front();
        my_char_queue.pop();
        return char_to_ret;
    }
    else
    {
      my_char_queue = queue<char>();
      is_data_available = false;
      return -1;
    }
}

int ModemClient::read(uint8_t* buf, size_t size) {
  //to implement
  return 0;
}

int ModemClient::peek() {
  //to implement
  return 0;
}

void ModemClient::flush() {
  // TODO: a real check to ensure transmission has been completed
}

void ModemClient::stop() {
  if(is_tcp_open==true)
  {
    Serial.println("tcp conn closing");
    sendATcmd_mdm_cli("AT+CIPCLOSE=1", "+CIPCLOSE: 1,0", 5000);
    is_tcp_open = false;   
  }
  
}

uint8_t ModemClient::connected() {
  uint8_t is_tcp_connected = 0;
  if (is_tcp_open)
  {
    is_tcp_connected = 1;
  }
  
  return is_tcp_connected;
  
}

uint8_t ModemClient::status() {
}

ModemClient::operator bool() {

}

// Private Methods