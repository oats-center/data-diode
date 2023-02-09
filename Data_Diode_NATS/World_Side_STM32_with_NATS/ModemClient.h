#ifndef modemclient_h
#define modemclient_h
#include <string>
#include <regex>
#include <sstream>
#include <queue>
#include "Arduino.h"	
#include "Print.h"
#include "Client.h"
#include "IPAddress.h"
using namespace std;

#if defined(RAMEND) && defined(RAMSTART) && ((RAMEND - RAMSTART) <= 2048)
#define MAX_SOCK_NUM_MODEM 4
#else
#define MAX_SOCK_NUM_MODEM 8
#endif

#define ARG2 2
#define ARG3 3


class ModemClient : public Client {

public:
  //constructor
  ModemClient(int rx, int);

  //non virtual constructor
  HardwareSerial ModemSerial;
  bool is_modem_initialised;
  bool is_net_open;
  bool is_tcp_open;
  bool is_data_available;
  string tcp_read_data;
  string payload_of_subscribe_cmd;
  queue<char> my_char_queue;
  int initialize_modem();
  void begin_serial(int baud_rate);
  String IpAddress2String(const IPAddress& ipAddress);
  uint8_t sendATcmd_mdm_cli(const char* ATcommand, const char* expected_answer, unsigned int timeout);
  string rx_tcp_data();
  string get_remainlen_or_arg3(string resp, int return_arg);

  uint8_t status();
  virtual int connect(IPAddress ip, uint16_t port);
  virtual int connect(const char *host, uint16_t port);
  int mywrite(const char* to_write);
  void initiate_tcp_write();
  void hexbytewrite(uint8_t hexbyte);
  void stoptcpwrite();
  virtual size_t write(uint8_t);
  virtual size_t write(const uint8_t *buf, size_t size);
  virtual int available();
  virtual int read();
  virtual int read(uint8_t *buf, size_t size);
  virtual int peek();
  virtual void flush();
  virtual void stop();
  virtual uint8_t connected();
  virtual operator bool();
  friend class WiFiServer;
  

  using Print::write;

private:
  static uint16_t _srcport;
  uint16_t  _socket;
  
};

#endif
