// DEVICE ID -- change for every unit

typedef struct connect_data{
  char* network;
  char* net_passwd;
  char* broker;
  char* broker_pwd;
  char* broker_IP;
  int port;
} connect_data;

connect_data c1 = {
  .network = <SSID1>,
  .net_passwd = <PASSWD1>,
  .broker = <BROKER_UNAME1>,
  .broker_pwd = <BROKER_PWD1>,
  .broker_IP = <BROKER_IP1>,
  .port = <BROKER_PORT1>
};

connect_data c2 = {
  .network = <SSID2>
  .net_passwd = <PASSWD2>,
  .broker = <BROKER_UNAME2>,
  .broker_pwd = <BROKER_PWD2>,
  .broker_IP = <BROKER_IP2>,
  .port = <BROKER_PORT2>
};
#define DEVICE "ESP3203"

