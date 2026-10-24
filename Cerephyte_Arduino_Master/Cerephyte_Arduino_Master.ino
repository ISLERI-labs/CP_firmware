#include <Wire.h>
#include <SoftwareSerial.h>
#include <Adafruit_ADS1X15.h>
#include <Adafruit_INA219.h>

Adafruit_ADS1115 ads;     
Adafruit_INA219 ina219;   

// SoftwareSerial: RX = Pin 12, TX = Pin 11 (Connected via level shifter)
SoftwareSerial ss(12, 11);

const int TOUCH_A2 = A2;
const int TOUCH_A3 = A3;

unsigned long sampleCounter = 0;
unsigned long lastSampleTime = 0;
const unsigned long sampleInterval = 200;

void setup() {
  Wire.begin(); 
  ss.begin(9600); // Rock-solid baud rate across the level shifter
  pinMode(2, OUTPUT);
  pinMode(5, OUTPUT);
  digitalWrite(2, HIGH);
  digitalWrite(5, HIGH);
  pinMode(TOUCH_A2, INPUT);
  pinMode(TOUCH_A3, INPUT);
  pinMode(LED_BUILTIN, OUTPUT);

  ads.begin();
  ina219.begin();
}

void loop() {
  if (millis() - lastSampleTime >= sampleInterval) {
    lastSampleTime = millis();
    sampleCounter++;

    int touchValA2 = digitalRead(TOUCH_A2);
    int touchValA3 = digitalRead(TOUCH_A3);
    digitalWrite(LED_BUILTIN, touchValA2); // Visual touch confirmation

    float volts0 = ads.computeVolts(ads.readADC_SingleEnded(1));
    float volts1 = ads.computeVolts(ads.readADC_SingleEnded(0));
    float busVoltage = ina219.getBusVoltage_V();
    float current_mA = ina219.getCurrent_mA();

    // Stream a clean, human-readable CSV packet
    // Format: DATA,counter,v0,v1,bat,curr,t2,t3
    ss.print(F("DATA,"));
    ss.print(sampleCounter);
    ss.print(F(","));
    ss.print(volts0, 3);
    ss.print(F(","));
    ss.print(volts1, 3);
    ss.print(F(","));
    ss.print(busVoltage, 2);
    ss.print(F(","));
    ss.print(current_mA, 2);
    ss.print(F(","));
    ss.print(touchValA2);
    ss.print(F(","));
    ss.println(touchValA3);
  }
}