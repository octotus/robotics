#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <BLE2901.h>
#include <BLE2902.h>


// Define some text shortcuts - to ease typing in the code

// Define all UUIDs here
  #define SERVICE_UUID "00000D40"
  #define CHARACTERISTIC_UUID1 "3a64dc90-7039-40de-b76a-448cc47e1e0d"
  #define CHARACTERISTIC_UUID2 "145015be-6f8a-46ea-a040-96ec9b71d74a"
  #define CHARACTERISTIC_UUID3 "d1148a0a-e83a-469f-af8e-da2891d13e01"
  //#define CHARACTERISTIC_UUID4 "7685fcea-b736-4bce-818d-1ff52019ddea"
  #define CHARACTERISTIC_UUID4 "69613f83-3cdd-46ac-9e44-f50ff9308991"
  #define CHARACTERISTIC_UUID6 "e509f87a-bdaa-46b5-9131-b3aa8454645b"
  #define CHARACTERISTIC_UUID7 "3a645bfb-f437-4f51-992a-1904620242e1"


// Declare all global variables here.
  int l = 0;
  std::string message="";
  float total_dose = 60; // 300 is the default value
  float decrement = 1; // 0.25 is the default value 
  const int step = 33; // PIN for STEP signal from ESP32
  const int dir = 32; // PIN for DIR signal from ESP32
  const int led = 2 ; // LED for testing code / response. Will be defunct in final version.
  int total_steps = 0;
  float dose_units = 0;
  float remaining_units = 60; // 300 is the default value
  //int total_dose = 300; // adjust this based on the total capacity of the reservoir
  int periodic_dose = 8; // set to steps per 15 mins
  long timer, old_timer;
  long time_gap = 5000 ;// 60 * 1000 * 15 900 * 1000 => 9E5
  char buffer[16]={0};
  bool deviceConnected = false;
  std::string warning_message;


// Declare BLECharacteristic variables
  BLECharacteristic *insulinRemaining = NULL;
  BLECharacteristic *insulinUnits = NULL; // send (milli) units delivered
  BLECharacteristic *insulinDeliveryMode = NULL; // Predefined values? 
  //BLECharacteristic *insulinDeliveryStatus = NULL; // send as notification?
  //BLECharacteristic *insulinControlPoint = NULL; // should be part of Insulin client?
  //BLECharacteristic *insulinHistory = NULL; // ??
  BLECharacteristic *insulinDeliveryAlarms = NULL; // Annunciation characteristic



// Server Callback Class
class MyCallBacks: public BLEServerCallbacks
{
  void onConnect(BLEServer* pServer)
  {
    deviceConnected=true;
  }
  void onDisconnect(BLEServer* pServer)
  {
    deviceConnected=false;
  }
};

void setup() {
// put your setup code here, to run once:
  Serial.begin(115200);
  Serial.println("BLE Starting");

// Start device, server and service for BLE.

  BLEDevice::init("ESP32_BLE");
  BLEServer *pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyCallBacks());
  BLEService *pService = pServer->createService(SERVICE_UUID);

// Assign all characteristic pointers to service
  insulinRemaining = pService->createCharacteristic(CHARACTERISTIC_UUID1, BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_NOTIFY);
  insulinUnits = pService->createCharacteristic(CHARACTERISTIC_UUID2, BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_NOTIFY);
  insulinDeliveryMode = pService->createCharacteristic(CHARACTERISTIC_UUID3, BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_NOTIFY);
  insulinDeliveryAlarms = pService->createCharacteristic(CHARACTERISTIC_UUID4, BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_NOTIFY);

  //insulinDeliveryStatus = pService->createCharacteristic(CHARACTERISTIC_UUID5, BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_NOTIFY);
  //insulinControlPoint = pService->createCharacteristic(CHARACTERISTIC_UUID6,  BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_NOTIFY);
  //insulinHistory = pService->createCharacteristic(CHARACTERISTIC_UUID7, BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_NOTIFY);
  
    /* insulinRemaining, insulinUnits, insulinDeliveryMode, insulinDeliveryStatus, 
insulinControlPoint, insulinHistory, insulinDeliveryAlarms*/

// Add Descriptor to all characteristics

  BLE2901 *IUDesc = new BLE2901();
  IUDesc->setDescription("Units Delivered");
  insulinUnits->addDescriptor(new BLE2902());
  insulinUnits->addDescriptor(IUDesc);

  BLE2901 *IRDesc = new BLE2901();
  IRDesc->setDescription("Units Remaining");
  insulinRemaining->addDescriptor(IRDesc);
  insulinRemaining->addDescriptor(new BLE2902());

  BLE2901 *IDMDesc = new BLE2901();
  IDMDesc->setDescription("Delivery Mode");
  insulinDeliveryMode->addDescriptor(IDMDesc);
  insulinDeliveryMode->addDescriptor(new BLE2902());

  BLE2901 *AlarmDesc = new BLE2901();
  AlarmDesc->setDescription("Ding Dong");
  insulinDeliveryAlarms->addDescriptor(new BLE2902());
  insulinDeliveryAlarms->addDescriptor(AlarmDesc);


/*
  BLE2901 *IDSDesc = new BLE2901();
  IDSDesc->setDescription("Delivery Status");
  insulinDeliveryStatus->addDescriptor(IDSDesc);
  insulinDeliveryStatus->addDescriptor(new BLE2902());

  insulinControlPoint->addDescriptor(new BLE2902());
  
  insulinHistory->addDescriptor(new BLE2902());
*/


// start the service and start advertising

  pService->start();
  BLEAdvertising *pAdvert = BLEDevice::getAdvertising();
  pAdvert->addServiceUUID(SERVICE_UUID);
  pAdvert->setScanResponse(true);
  BLEDevice::startAdvertising();
}

void loop() {
  delay(2000);
  remaining_units = remaining_units - decrement;
  dose_units = dose_units + decrement;
  if(deviceConnected)
  {
    insulinRemaining->setValue(remaining_units);
    insulinRemaining->notify();

    insulinUnits->setValue(dose_units);
    insulinUnits->notify();

    if(remaining_units >=10)
    {
      insulinDeliveryMode->setValue("normal");
      insulinDeliveryMode->notify();

      insulinDeliveryAlarms->setValue("None");
      insulinDeliveryAlarms->notify();
    
    }
    if(remaining_units < 10)
    {      
      insulinDeliveryAlarms->setValue("Reservoir Low");
      insulinDeliveryAlarms->notify();

      insulinDeliveryMode->setValue("Normal. No PPD.");
      insulinDeliveryMode->notify();
    }
    if(remaining_units < 2)
    {
      insulinDeliveryAlarms->setValue("Reservoir Empty");
      insulinDeliveryAlarms->notify();

      insulinDeliveryMode->setValue("Stopped");
      insulinDeliveryMode->notify();

      delay(10000);
      remaining_units=60;
      dose_units=0;
    }
  }
}
