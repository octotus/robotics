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

int scanTime = 2; //In seconds 

//char* network=SECRET_SSID;
//char* passwd=SECRET_PWD;

BLEScan* pBLEScan;

WiFiClient wfc;
MqttClient mqClient(wfc);


char* topic = "BLE_beacon_details";

const long interval = 2000;
long prevMillis = 0;
struct connect_data c;

bool connectWiFi(struct connect_data)
{  
  bool state=false;
  WiFi.setHostname(DEVICE);
  WiFi.begin(c.network,c.net_passwd);
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
  WiFi.mode(WIFI_STA);

  // Connect to Wi-Fi and confirm local IP 

  while(state == false) 
  {
    c = c1;
    state = connectWiFi(c);  
    if(state == false)
    {
      c = c2;
      state=connectWiFi(c);
    }
    delay(10000);
  }

/*
  Serial.print("Connected to WiFi Network: ");
  Serial.println(network);
  IPAddress ip = WiFi.localIP();
  Serial.print("Local IP Address is: ");
  Serial.println(ip);
  Serial.println("\n");
*/
  // WiFi Connect Done

  // Set up MQTT Client and Connect to Broker //
  mqClient.setId(DEVICE);
  // use this space to set username / password for accessing the server //
  mqClient.setUsernamePassword(c.broker,c.broker_pwd);

  if(!mqClient.connect(c.broker_IP,c.port))
  {
    Serial.println("Trying to connect to broker");
    while(1);
  }
  
  Serial.println("Connected to broker");
  
  //Set up BLE Scan //
  Serial.println("Scanning...");

  BLEDevice::init(DEVICE);
  pBLEScan = BLEDevice::getScan(); //create new scan
  //pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
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
      mqClient.setUsernamePassword(c.broker,c.broker_pwd);
      mqClient.connect(c.broker_IP,c.port);
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
