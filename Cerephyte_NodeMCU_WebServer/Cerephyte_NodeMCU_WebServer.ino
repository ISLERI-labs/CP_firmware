#define BLYNK_TEMPLATE_ID   "TMPL3qRb1CWOi"
#define BLYNK_TEMPLATE_NAME "cerephyte"
#define BLYNK_AUTH_TOKEN    "ZDw0Ot4FY6JbJdGMBY6dQf05u5DJkpON"

#include <ESP8266WiFi.h>
#include <BlynkSimpleEsp8266.h>
#include <SoftwareSerial.h>

char auth[] = BLYNK_AUTH_TOKEN;
char ssid[] = "Airtel_Roshni";
char pass[] = "AIR14879";

// SoftwareSerial on NodeMCU: RX = D5 (GPIO14), TX = D2 (GPIO4)
SoftwareSerial ss(14, 4);

BlynkTimer timer;
WidgetTerminal terminal(V7);

unsigned long latestSample = 0;
float latestA0 = 0;
float latestA1 = 0;
float latestBat = 0;
float latestCurr = 0;
int latestT2 = 0;
int latestT3 = 0;
bool newDataAvailable = false;

void processSerialInput() {
  while (ss.available()) {
    String line = ss.readStringUntil('\n');
    line.trim();
    
    if (line.startsWith("DATA,")) {
      unsigned int c;
      float v0, v1, bat, curr;
      int t2, t3;
      
      // Parse the CSV line safely
      int parsed = sscanf(line.c_str(), "DATA,%u,%f,%f,%f,%f,%d,%d", &c, &v0, &v1, &bat, &curr, &t2, &t3);
      if (parsed == 7) {
        latestSample = c;
        latestA0 = v0;
        latestA1 = v1;
        latestBat = bat;
        latestCurr = curr;
        latestT2 = t2;
        latestT3 = t3;
        newDataAvailable = true;
      }
    }
  }
}

void sendToBlynkTask() {
  if (newDataAvailable) {
    // Control onboard NodeMCU LED based on Touch A3
    if (latestT3 == HIGH) {
      digitalWrite(LED_BUILTIN, LOW);  // ON
    } else {
      digitalWrite(LED_BUILTIN, HIGH); // OFF
    }

    if (Blynk.connected()) {
      Blynk.virtualWrite(V0, latestSample);
      Blynk.virtualWrite(V1, latestA0);   
      Blynk.virtualWrite(V2, latestA1);   
      Blynk.virtualWrite(V3, latestBat); 
      Blynk.virtualWrite(V4, latestCurr); 
      Blynk.virtualWrite(V5, latestT2);  
      Blynk.virtualWrite(V6, latestT3);  

      terminal.print(F("Sample: ")); terminal.print(latestSample);
      terminal.print(F(" | A0: ")); terminal.print(latestA0, 3);
      terminal.print(F("V | Bat: ")); terminal.print(latestBat, 2);
      terminal.println(F("V"));
      terminal.flush();
    }
    newDataAvailable = false;
  }
}

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);

  ss.begin(9600);

  WiFi.begin(ssid, pass);
  unsigned long startAttempt = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startAttempt < 10000) {
    delay(500);
  }

  Blynk.begin(auth, ssid, pass);
  timer.setInterval(200L, sendToBlynkTask);
}

void loop() {
  Blynk.run();
  timer.run();
  processSerialInput();
}