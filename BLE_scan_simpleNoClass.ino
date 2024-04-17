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
#include <ArduinoJSON.h>
int scanTime = 5; //In seconds

char* network=SECRET_SSID;
char* passwd=SECRET_PWD;

BLEScan* pBLEScan;

WiFiClient wfc;
MqttClient mqClient(wfc);

char* broker = "192.168.4.42";
int port = 1883;
char* topic = "BLE_beacon_details";

const long interval = 5000;
long prevMillis = 0;

void setup() {
  Serial.begin(115200);

  // Connect to Wi-Fi and confirm local IP 

  WiFi.mode(WIFI_STA);
  WiFi.begin(network, passwd);
  while(WiFi.status != WL_CONNECTED)
  {
    delay(500);
  }
  Serial.print("Connected to WiFi Network: ");
  Serial.println(network);
  IPADDRESS ip = WiFi.localIP();
  Serial.print("Local IP Address is: ");
  Serial.println(ip);
  Serial.println("\n");
  // WiFi Connect Done

  // Set up MQTT Client and Connect to Broker //
  mqClient.setId("ESP3201");
  // use this space to set username / password for accessing the server //
  //mqClient.setUsernamePassword(str user, str psswd);

  if(!mqClient.connect(broker,port))
  {
    Serial.println("Trying to connect to broker");
    while(1);
  }
  
  Serial.println("Connected to broker");
  
  //Set up BLE Scan //
  Serial.println("Scanning...");

  BLEDevice::init("ESP3201");
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

  long curr_millis = millis();
  long diff = curr_millis - prevMillis;

  if(diff > interval)
  {
    BLEScanResults foundDevices = pBLEScan->start(scanTime, false);
    Serial.print("Devices found: ");
    Serial.println(foundDevices.getCount());
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
        Serial.print(dName.c_str());
        Serial.print("\t"); Serial.print(address); 
        Serial.print("\t"); Serial.print(dTxPower);
        Serial.print("\t"); Serial.println(rssi);

        JsonDocument message;
        message["sender"] = DEVICE;
        message["beacon"] = dName;
        message["TXPower"] = dTxPower;
        message["RSSI"] = rssi;
        message["MAC"] = address;
        char buff[256];
        Serial.println("Sending message:");
        serializeJson(message,buff);

        mqClient.beginMessage(topic);
        mqClient.print(buff);
        mqClient.endMessage(); 
        
      }
    }
     Serial.println("Scan done!");
    pBLEScan->clearResults();   // delete results fromBLEScan buffer to release memory
  delay(500);
}
