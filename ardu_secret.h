// DEVICE ID -- change for every unit

typedef struct connect_data{
  char* network; // Netwokr SSID
  char* net_passwd; // password
  char* broker; // broker user ID
  char* broker_pwd; // broker PWD
  char* broker_IP; // broker IP
  int port; // port - default = 1883
  struct connect_data *next; //points to subsequent options; NULL else.
} connect_data;


connect_data* define_linked_list()
{
  connect_data *head = NULL;
  connect_data *c1 = (connect_data *)malloc(sizeof(connect_data));
  connect_data *c2 = (connect_data *)malloc(sizeof(connect_data));


  c1->network = "";
  c1->net_passwd = "";
  c1->broker = "";
  c1->broker_pwd = "";
  c1->broker_IP = "";
  c1->port = 1883;
  c1->next = c2;
 /* 
  c2->network = "";
  c2->net_passwd = "
  c2->broker = "";
  c2->broker_pwd = "
  c2->broker_IP = "";
  c2->port = 1883;
  c2->next = NULL; */
  head = c1;
  return head;
}
#define DEVICE "ESP3203"
