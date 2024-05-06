#include<TFMPlus.h>

/*
Script for working with TF-Luna ToF Lidar from Benewake.
Uses TFMPlus library (https://github.com/budryerson/TFMini-Plus)
Very closely follows the example for ESP32 given in https://www.waveshare.com/wiki/TF-Luna_LiDAR_Range_Sensor and the example provided in TFMPlus repo. 

Uses the Rx2 / Tx2 in ESP32 (built-in GPIO) as second serial connection.

Fairly straightforward to use. With good signal quality (Flux), the measuresment is quite accurate.
*/


TFMPlus lidar;

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  delay(200);
//  printf_begin();

  Serial2.begin(115200);
  delay(200);
  lidar.begin(&Serial2);
  delay(500);
//  printf("Soft reset: ");
}

int16_t dist = 0;
int16_t flux = 0;
int16_t temp = 0;

void loop() {
  // put your main code here, to run repeatedly:
  //delay(500);
  lidar.getData(dist,flux,temp);
  Serial.print(dist);
  Serial.print("\t");
  Serial.print(flux);
  Serial.print("\t");
  Serial.println(temp);
}
