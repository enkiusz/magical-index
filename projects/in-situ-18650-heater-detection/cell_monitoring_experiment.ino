#include <Wire.h>
#include <Temperature_LM75_Derived.h>
#include "max6675.h"

/* 
 *  In-situ 18650 heater cell detection.
 *  
 *  Experimental setup v1
 *  Code version: v1
 */

// Experiment v1 wiring
int thermoDO = 4;
int thermoCS = 5;
int thermoCLK = 6;
int heaterON = 9;

// Experiment parameters
float target_temp = 40; // the temperature of the heater cell

// PID parameters
float P = 80;

MAX6675 thermocouple(thermoCLK, thermoCS, thermoDO);

// ZIF-20C brick temperature sense board rev 1
Generic_LM75 U1(0x48);
Generic_LM75 U2(0x49);
Generic_LM75 U3(0x4B);
Generic_LM75 U4(0x4C);
Generic_LM75 U5(0x4D);

void scan_for_i2c_devices() {
  int nDevices = 0;

  Serial.println("scanning for i2c devices");

  for (byte address = 1; address < 127; ++address) {
    // The i2c_scanner uses the return value of
    // the Write.endTransmisstion to see if
    // a device did acknowledge to the address.
    Wire.beginTransmission(address);
    byte error = Wire.endTransmission();

    if (error == 0) {
      Serial.print("I2C device found at address 0x");
      if (address < 16) {
        Serial.print("0");
      }
      Serial.print(address, HEX);
      Serial.println("  !");

      ++nDevices;
    } else if (error == 4) {
      Serial.print("Unknown error at address 0x");
      if (address < 16) {
        Serial.print("0");
      }
      Serial.println(address, HEX);
    }
  }
  if (nDevices == 0) {
    Serial.println("No I2C devices found\n");
  } else {
    Serial.println("done\n");
  }
}

void setup() {
  pinMode(heaterON, OUTPUT);
  Wire.begin();
  
  Serial.begin(115200);
  
  while (!Serial); // Leonardo: wait for serial monitor
  
  // Only for debugging of I2C connections
  // Reference: Examples -> Wire -> i2c_scanner
  //scan_for_i2c_devices();
  
  // wait for MAX chip to stabilize
  delay(500);
}

void loop() {
  float temp = thermocouple.readCelsius();

  float deltaT = target_temp - temp;
  
  float pwm_drive = min( max(deltaT * P, 0), 1023);

  // read temperatures for the ZIF-20C-TEMP board
  float temp_U1 = U1.readTemperatureC();
  float temp_U2 = U2.readTemperatureC();
  float temp_U3 = U3.readTemperatureC();
  float temp_U4 = U4.readTemperatureC();
  float temp_U5 = U5.readTemperatureC();
  
  Serial.print("target_temp[degC] = ");
  Serial.print(target_temp);
  Serial.print("\ttemp[degC] = ");
  Serial.print(temp);
  Serial.print("\tpwm[%] = ");
  Serial.print(map(pwm_drive, 0, 1023, 0, 100));
  Serial.print("\tU1[degC] = ");
  Serial.print(temp_U1);
  Serial.print("\tU2[degC] = ");
  Serial.print(temp_U2);
  Serial.print("\tU3[degC] = ");
  Serial.print(temp_U3);
  Serial.print("\tU4[degC] = ");
  Serial.print(temp_U4);
  Serial.print("\tU5[degC] = ");
  Serial.print(temp_U5);
  Serial.println();

  analogWrite(heaterON, pwm_drive);
  // For the MAX6675 to update, you must delay AT LEAST 250ms between reads!
  delay(300);
}
