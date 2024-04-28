// These libraries are needed for beacon Scan
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>

// These are needed for WiFi connectivity

#include <WiFi.h>
#include "ardu_secret.h"

// These are needed for MQTT connection and sending MQTT messages
#include <ArduinoMqttClient.h>
#include <ArduinoJson.h>

int scanTime = 5; //In seconds 

//char* network=SECRET_SSID;
//char* passwd=SECRET_PWD;

BLEScan* pBLEScan;

WiFiClient wfc;
MqttClient mqClient(wfc);

char* topic = "BLE_beacon_details";

const long interval = 2000;
long prevMillis = 0;
connect_data *c;

bool connectWiFi(connect_data *c)
{  
  bool state=false;
  WiFi.begin(c->network,c->net_passwd);
  int tries = 0;
  while(tries < 5)
  {
    if(WiFi.status()!=WL_CONNECTED)
    {
      delay(1000);
      tries++;
    }
    else
    {
      state=true;
      tries=6;
    }
  }
  return state;
}


void setup() {
  bool state=false;
  Serial.begin(115200);
  
  WiFi.disconnect(true);
  WiFi.config(INADDR_NONE,INADDR_NONE,INADDR_NONE);
  WiFi.mode(WIFI_STA);
  WiFi.setHostname(DEVICE);
  // Connect to Wi-Fi and confirm local IP 
  c = define_linked_list();
  int counter = 0;
  
  while((state==false)) 
  {
    while(c)
    {
      state = connectWiFi(c);
      if(state == false)
      {
        continue;    
      }
      else
      {
        break;
      }
    }
    delay(5000);
    counter++;
    if(counter == 10)
    {
      delay(20000);
    }
  }

  // WiFi Connect Done

  // Set up MQTT Client and Connect to Broker //
  mqClient.setId(DEVICE);
  // use this space to set username / password for accessing the server //
  mqClient.setUsernamePassword(c->broker,c->broker_pwd);

  while(!mqClient.connect(c->broker_IP,c->port))
  {
    Serial.println("Trying to connect to broker");
    delay(2000);
  }
  
  Serial.println("Connected to broker");
  
  //Set up BLE Scan //
  Serial.println("Scanning...");

  BLEDevice::init(DEVICE);
  pBLEScan = BLEDevice::getScan(); //create new scan
  pBLEScan->setActiveScan(true); //active scan uses more power, but get results faster
  pBLEScan->setInterval(100);
  pBLEScan->setWindow(99);  // less or equal setInterval value
}



void loop() {
  // put your main code here, to run repeatedly:
  // MQTT Part

  mqClient.poll();

  if(!mqClient.connected())
  {
      mqClient.setUsernamePassword(c->broker,c->broker_pwd);
      mqClient.connect(c->broker_IP,c->port);
  }
  long curr_millis = millis();
  long diff = curr_millis - prevMillis;

  if(diff > interval)
  {
    BLEScanResults foundDevices = pBLEScan->start(scanTime, false);
//    Serial.print("Devices found: ");
//    Serial.println(foundDevices.getCount());
    int count = foundDevices.getCount();
    for(int i = 0; i < count; i++)
    {
      BLEAdvertisedDevice device=(foundDevices.getDevice(i));
      std::string dName = device.getName();
      int dTxPower = device.getTXPower();
      int rssi = device.getRSSI();
      String address = device.getAddress().toString().c_str();
      if(dName.size() > 0)
      {
//        Serial.print(dName.c_str());
//        Serial.print("\t"); Serial.print(address); 
//        Serial.print("\t"); Serial.print(dTxPower);
//        Serial.print("\t"); Serial.println(rssi);

        JsonDocument message;
        message["sender"] = DEVICE;
        message["beacon"] = dName;
        message["TXPower"] = dTxPower;
        message["RSSI"] = rssi;
        message["MAC"] = address;
        char buff[256];
//        Serial.println("Sending message:");
        serializeJson(message,buff);

        mqClient.beginMessage(topic);
        mqClient.print(buff);
        mqClient.endMessage();

//        Serial.println("\n\tMessage Sent");
      }
    }
    Serial.println("Scan done!");
    pBLEScan->clearResults();   // delete results fromBLEScan buffer to release memory
    delay(2000);
  }
}
