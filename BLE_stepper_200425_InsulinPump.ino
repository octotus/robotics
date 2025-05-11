#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <BLE2902.h>
#include <string>


//#define DEVICE_UUID "0x0D40"
//#define SERVICE_UUID "0x183A"

#define SERVICE_UUID "0000183A"
#define CHARACTERISTIC_UUID1 "3a64dc90-7039-40de-b76a-448cc47e1e0d"
#define CHARACTERISTIC_UUID2 "145015be-6f8a-46ea-a040-96ec9b71d74a"
#define CHARACTERISTIC_UUID3 "d1148a0a-e83a-469f-af8e-da2891d13e01"
#define CHARACTERISTIC_UUID4 "7685fcea-b736-4bce-818d-1ff52019ddea"
#define CHARACTERISTIC_UUID5 "69613f83-3cdd-46ac-9e44-f50ff9308991"
#define CHARACTERISTIC_UUID6 "e509f87a-bdaa-46b5-9131-b3aa8454645b"
#define CHARACTERISTIC_UUID7 "3a645bfb-f437-4f51-992a-1904620242e1"

// DEFINE ESP32 PINS

const int step = 33; // PIN for STEP signal from ESP32
const int dir = 32; // PIN for DIR signal from ESP32
const int led = 2 ; // LED for testing code / response. Will be defunct in final version.
const int 
// DEFINE
int total_steps = 0;
float dose_units = 0;
float remaining_units = 0;
int total_dose = 300; // adjust this based on the total capacity of the reservoir
int periodic_dose = 8; // set to steps per 15 mins
long timer, old_timer;
long time_gap = 5000 ;// 60 * 1000 * 15 900 * 1000 => 9E5
char buffer[16]={0};
bool deviceConnected = false;

std::string warning_message;
// Stepper Microstep control pins
// Not needed in microstepper 3cm motor

// Declare all BLE characteristics here.

BLECharacteristic *insulinDrive = NULL;
BLECharacteristic *insulinUnits = NULL; // send (milli) units delivered
BLECharacteristic *insulinDeliveryMode = NULL; // Predefined values? 
BLECharacteristic *insulinDeliveryStatus = NULL; // send as notification?
BLECharacteristic *insulinControlPoint = NULL; // should be part of Insulin client?
BLECharacteristic *insulinHistory = NULL; // ??
BLECharacteristic *insulinDeliveryAlarms = NULL; // Annunciation characteristic

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

int set_periodic_dose()
{
  /* write the function to receive data through BLE - to set up */
}

void move_motor_periodic_dose(void)
{
    /* 
    5800 steps per 30 mm; approximately 193 steps per mm. => 
    62.5ul / mm with a 9.0mm ID syringe. ==> 0.16mm per 10ul
    Typical dosing is 1 IU insulin per hr. 
    Insulin is 100 IU per ml; 1 IU / 10ul.
    Pump needs to deliver 1 IU per hour. 
    Every hour, the pump needs to move 31 steps
    Set at 32 steps per hour (error rate of 2%).
    */
  int loop_i;
  for(loop_i=0; loop_i < periodic_dose; loop_i++)
  {

    digitalWrite(step,HIGH);
    delay(20);
    digitalWrite(step,LOW);
    delay(20);
  }
  total_steps = total_steps + periodic_dose;
  dose_units += 0.25;

}

void setup() {
// put your setup code here, to run once:
  Serial.begin(115200);
  Serial.println("ready to connect");

// set up all the pins here
  pinMode(step, OUTPUT);
  pinMode(dir,OUTPUT);
  pinMode(led,OUTPUT);

// set the direction to LOW - for microstepper
  digitalWrite(dir,LOW);

// Initiate BLE Device
  BLEDevice::init("InsulinPump");
  BLEServer *pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyCallBacks());
  BLEService *pService = pServer->createService(SERVICE_UUID);

// set periodic dose for patient
  periodic_dose = set_periodic_dose();

//start timer

  old_timer=millis();

// Set up all Characteristics

  insulinDrive = pService->createCharacteristic(CHARACTERISTIC_UUID1,BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_NOTIFY);
  insulinDeliveryMode = pService->createCharacteristic(CHARACTERISTIC_UUID3,BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_NOTIFY);
  insulinDeliveryStatus = pService->createCharacteristic(CHARACTERISTIC_UUID4,BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_NOTIFY);
  insulinControlPoint = pService->createCharacteristic(CHARACTERISTIC_UUID5,BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_NOTIFY);
  insulinHistory = pService->createCharacteristic(CHARACTERISTIC_UUID6,BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_NOTIFY);
  insulinUnits = pService->createCharacteristic(CHARACTERISTIC_UUID2,BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_NOTIFY);
  insulinDeliveryAlarms = pService->createCharacteristic(CHARACTERISTIC_UUID7,BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_NOTIFY);

// set up service notification

  insulinDeliveryMode->addDescriptor(new BLE2902());
  insulinDeliveryStatus->addDescriptor(new BLE2902());
  insulinControlPoint->addDescriptor(new BLE2902());
  insulinHistory->addDescriptor(new BLE2902());
  insulinDrive->addDescriptor(new BLE2902());
  insulinUnits->addDescriptor(new BLE2902());
  insulinDeliveryAlarms->addDescriptor(new BLE2902());

// Start BLE Service

  pService->start();
  BLEAdvertising *pAdvert = BLEDevice::getAdvertising();
  pAdvert->addServiceUUID(SERVICE_UUID);
  pAdvert->setScanResponse(true);
  BLEDevice::startAdvertising();
}

void loop() {
// put your main code here, to run repeatedly:
  
  if(deviceConnected)
  { 
    timer = millis();

  // Deliver a dose at predetermined times
    if(timer - old_timer > time_gap)
    {
      move_motor_periodic_dose();
      insulinUnits->setValue(total_dose);
      insulinUnits->notify();
      if(total_steps > 4000)
      {
        remaining_units = total_dose - dose_units;
        insulinDeliveryAlarms->setValue(remaining_units);
        insulinDeliveryAlarms->notify();
      }
      old_timer= timer;
    }
  }
}
