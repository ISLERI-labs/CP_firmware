#include <Wire.h>
#include <SoftwareSerial.h>
#include <Adafruit_ADS1X15.h>
#include <Adafruit_INA219.h>

Adafruit_ADS1115 ads;     
Adafruit_INA219 ina219;   

// SoftwareSerial across TXS0108E level shifter: RX = Pin 12, TX = Pin 11
SoftwareSerial ss(12, 11);

// MUX 1 (U4) Control Pins
const int MUX1_S0 = 2;
const int MUX1_S1 = 3;
const int MUX1_S2 = 4;
const int MUX1_INH = 8;

// MUX 2 (U5) Control Pins
const int MUX2_S0 = 5;
const int MUX2_S1 = 6;
const int MUX2_S2 = 7;
const int MUX2_INH = 9;

const int TOUCH_A2 = A2;
const int TOUCH_A3 = A3;

unsigned long sampleCounter = 0;
unsigned long lastSampleTime = 0;
const unsigned long sampleInterval = 100; // ms per loop

uint8_t mux1Channel = 0; // MUX 1 active channel (0-7)
uint8_t mux2Channel = 0; // MUX 2 active channel (0-7)

void setup() {
  Wire.begin(); 
  ss.begin(9600); 

  // Configure MUX 1 control & enable pins
  pinMode(MUX1_S0, OUTPUT);
  pinMode(MUX1_S1, OUTPUT);
  pinMode(MUX1_S2, OUTPUT);
  pinMode(MUX1_INH, OUTPUT);

  // Configure MUX 2 control & enable pins
  pinMode(MUX2_S0, OUTPUT);
  pinMode(MUX2_S1, OUTPUT);
  pinMode(MUX2_S2, OUTPUT);
  pinMode(MUX2_INH, OUTPUT);

  // Enable both multiplexers (Active Low EN/INH)
  digitalWrite(MUX1_INH, LOW);
  digitalWrite(MUX2_INH, LOW);

  pinMode(TOUCH_A2, INPUT);
  pinMode(TOUCH_A3, INPUT);
  pinMode(LED_BUILTIN, OUTPUT);

  ads.begin();
  ina219.begin();
}

void selectMux1(uint8_t channel) {
  digitalWrite(MUX1_S0, (channel & 0x01) ? HIGH : LOW);
  digitalWrite(MUX1_S1, (channel & 0x02) ? HIGH : LOW);
  digitalWrite(MUX1_S2, (channel & 0x04) ? HIGH : LOW);
  digitalWrite(MUX1_INH, LOW);
  delayMicroseconds(100);
}

void selectMux2(uint8_t channel) {
  digitalWrite(MUX2_S0, (channel & 0x01) ? HIGH : LOW);
  digitalWrite(MUX2_S1, (channel & 0x02) ? HIGH : LOW);
  digitalWrite(MUX2_S2, (channel & 0x04) ? HIGH : LOW);
  digitalWrite(MUX2_INH, LOW);
  delayMicroseconds(100);
}

void checkSerialCommands() {
  static char cmdBuffer[16];
  static size_t cmdIndex = 0;

  while (ss.available() > 0) {
    char c = (char)ss.read();
    if (c == '\r') continue;
    if (c == '\n') {
      cmdBuffer[cmdIndex] = '\0';
      if (strncmp(cmdBuffer, "MUX1,", 5) == 0) {
        int ch = atoi(cmdBuffer + 5);
        if (ch >= 0 && ch <= 7) {
          mux1Channel = ch;
          selectMux1(mux1Channel);
        }
      } else if (strncmp(cmdBuffer, "MUX2,", 5) == 0) {
        int ch = atoi(cmdBuffer + 5);
        if (ch >= 0 && ch <= 7) {
          mux2Channel = ch;
          selectMux2(mux2Channel);
        }
      }
      cmdIndex = 0;
    } else if (cmdIndex < sizeof(cmdBuffer) - 1) {
      cmdBuffer[cmdIndex++] = c;
    } else {
      cmdIndex = 0;
    }
  }
}

void loop() {
  checkSerialCommands();

  if (millis() - lastSampleTime >= sampleInterval) {
    lastSampleTime = millis();
    sampleCounter++;

    // Ensure current channel states are latched
    selectMux1(mux1Channel);
    selectMux2(mux2Channel);

    int touchValA2 = digitalRead(TOUCH_A2);
    int touchValA3 = digitalRead(TOUCH_A3);
    digitalWrite(LED_BUILTIN, touchValA2); 

    // Read analog voltage exclusively from ADS1115 pin A0
    float voltage = ads.computeVolts(ads.readADC_SingleEnded(1));
    float busVoltage = ina219.getBusVoltage_V();
    float current_mA = ina219.getCurrent_mA();

    // Stream CSV packet: DATA,sample,mux1_ch,mux2_ch,voltage,bus_voltage,current_mA,t2,t3
    ss.print(F("DATA,"));
    ss.print(sampleCounter);
    ss.print(F(","));
    ss.print(mux1Channel);
    ss.print(F(","));
    ss.print(mux2Channel);
    ss.print(F(","));
    ss.print(voltage, 3);
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