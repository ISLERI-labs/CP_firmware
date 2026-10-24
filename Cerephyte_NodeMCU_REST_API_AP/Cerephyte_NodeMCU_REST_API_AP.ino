/*
  CerePhyte Rev. 4.5 — NodeMCU local REST API
  Keeps the Arduino serial protocol unchanged:
    DATA,sample,A0,A1,battery,current,t2,t3\n
  Serial: RX = D5 / GPIO14, TX = D2 / GPIO4, 9600 baud.
  GET /api/data  -> latest complete measurement as JSON
  GET /           -> short status/help page

  NodeMCU creates its own Wi-Fi access point, so its API address remains
  fixed even when the exhibition Wi-Fi or phone hotspot changes.
  Connect the computer to CerePhyte-Rev45 and open http://192.168.4.1/api/data
*/

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <SoftwareSerial.h>
#include <math.h>

const char* apSsid = "CerePhyte-Rev45";
const char* apPassword = "CerePhyte45";  // Change this if desired (8+ characters)

// Preserve the existing serial pins and baud rate.
SoftwareSerial ss(14, 4);  // RX=D5 (GPIO14), TX=D2 (GPIO4)
ESP8266WebServer server(80);

struct Telemetry {
  unsigned long sample = 0;
  float a0 = 0.0f;
  float a1 = 0.0f;
  float bat = 0.0f;
  float curr = 0.0f;
  int t2 = 0;
  int t3 = 0;
  unsigned long receivedAtMs = 0;
  bool valid = false;
};

Telemetry latest;
char serialLine[128];
size_t serialPos = 0;
bool apiStarted = false;

void processCompleteLine(char* line) {
  // Keep the original DATA CSV protocol; parse all seven payload values.
  if (strncmp(line, "DATA,", 5) != 0) return;

  char* fields[8];
  size_t count = 0;
  char* token = strtok(line, ",");
  while (token != nullptr && count < 8) {
    fields[count++] = token;
    token = strtok(nullptr, ",");
  }
  if (count != 8 || strcmp(fields[0], "DATA") != 0) return;

  // Convert and validate before replacing the last complete snapshot.
  char* end = nullptr;
  unsigned long sample = strtoul(fields[1], &end, 10);
  if (end == fields[1] || *end != '\0') return;

  float a0 = strtof(fields[2], &end);
  if (end == fields[2] || *end != '\0' || !isfinite(a0)) return;
  float a1 = strtof(fields[3], &end);
  if (end == fields[3] || *end != '\0' || !isfinite(a1)) return;
  float bat = strtof(fields[4], &end);
  if (end == fields[4] || *end != '\0' || !isfinite(bat)) return;
  float curr = strtof(fields[5], &end);
  if (end == fields[5] || *end != '\0' || !isfinite(curr)) return;

  int t2 = (int)strtol(fields[6], &end, 10);
  if (end == fields[6] || *end != '\0') return;
  int t3 = (int)strtol(fields[7], &end, 10);
  if (end == fields[7] || *end != '\0') return;

  latest.sample = sample;
  latest.a0 = a0;
  latest.a1 = a1;
  latest.bat = bat;
  latest.curr = curr;
  latest.t2 = t2;
  latest.t3 = t3;
  latest.receivedAtMs = millis();
  latest.valid = true;

  // ESP8266 built-in LED is active LOW. Follow Arduino's A3/t3 signal.
  digitalWrite(LED_BUILTIN, latest.t3 ? LOW : HIGH);
}

void readArduinoSerial() {
  // Non-blocking byte-by-byte receiver: no readStringUntil() timeout.
  while (ss.available() > 0) {
    char c = (char)ss.read();
    if (c == '\r') continue;

    if (c == '\n') {
      serialLine[serialPos] = '\0';
      if (serialPos > 0) processCompleteLine(serialLine);
      serialPos = 0;
    } else if (serialPos < sizeof(serialLine) - 1) {
      serialLine[serialPos++] = c;
    } else {
      // Oversized/corrupt line: discard it and resynchronize at next newline.
      serialPos = 0;
    }
  }
}

void sendData() {
  // Always return one JSON object containing every Arduino field.
  char json[320];
  unsigned long age = latest.valid ? millis() - latest.receivedAtMs : 0;

  if (!latest.valid) {
    snprintf(json, sizeof(json),
             "{\"ok\":false,\"error\":\"No valid Arduino DATA packet received yet\","
             "\"sample\":null,\"a0\":null,\"a1\":null,\"bat\":null,\"curr\":null,"
             "\"t2\":null,\"t3\":null,\"age_ms\":null}");
  } else {
    snprintf(json, sizeof(json),
             "{\"ok\":true,\"sample\":%lu,\"a0\":%.3f,\"a1\":%.3f,"
             "\"bat\":%.2f,\"curr\":%.2f,\"t2\":%d,\"t3\":%d,\"age_ms\":%lu}",
             latest.sample, latest.a0, latest.a1, latest.bat, latest.curr,
             latest.t2, latest.t3, age);
  }

  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.sendHeader("Cache-Control", "no-store");
  server.send(200, "application/json", json);
}

void sendRoot() {
  server.send(200, "text/plain",
              "CerePhyte Rev. 4.5 local API\nGET /api/data\n");
}

void startApiOnce() {
  if (!apiStarted) {
    server.on("/", HTTP_GET, sendRoot);
    server.on("/api/data", HTTP_GET, sendData);
    server.onNotFound([]() {
      server.send(404, "application/json", "{\"ok\":false,\"error\":\"Not found\"}");
    });
    server.begin();
    apiStarted = true;

    Serial.println();
    Serial.println(F("CerePhyte local REST API started"));
    Serial.println(F("Wi-Fi: CerePhyte-Rev45"));
    Serial.println(F("IP: http://192.168.4.1"));
    Serial.println(F("Endpoint: /api/data"));
  }
}

void setup() {
  Serial.begin(115200);  // USB diagnostics only; Arduino link remains 9600.
  ss.begin(9600);

  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);  // LED off (active-low)

  WiFi.persistent(false);
  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(IPAddress(192, 168, 4, 1),
                    IPAddress(192, 168, 4, 1),
                    IPAddress(255, 255, 255, 0));
  bool apStarted = WiFi.softAP(apSsid, apPassword);

  Serial.println();
  Serial.println(F("CerePhyte Rev. 4.5 — local REST API"));
  Serial.println(apStarted ? F("Wi-Fi access point started") : F("ERROR: AP failed to start"));
  Serial.print(F("Wi-Fi name: "));
  Serial.println(apSsid);
  Serial.print(F("API: http://192.168.4.1/api/data"));
  Serial.println();
}

void loop() {
  // Prioritize serial reception; serve HTTP without delay() or Blynk calls.
  readArduinoSerial();
  startApiOnce();
  if (apiStarted) server.handleClient();
  readArduinoSerial();
}
