/*
  cerephyte version:5
  lisense: apache 2.0

*/

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <SoftwareSerial.h>
#include <math.h>

const char* apSsid = "CerePhyte-5";
const char* apPassword = "npmsplash";

// SoftwareSerial: RX = D5 (GPIO14), TX = D2 (GPIO4)
SoftwareSerial ss(14, 4);
ESP8266WebServer server(80);

struct Telemetry {
  unsigned long sample = 0;
  int mux1_channel = 0;
  int mux2_channel = 0;
  float voltage = 0.0f;
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
  if (strncmp(line, "DATA,", 5) != 0) return;

  char* fields[10];
  size_t count = 0;
  char* token = strtok(line, ",");
  while (token != nullptr && count < 10) {
    fields[count++] = token;
    token = strtok(nullptr, ",");
  }
  if (count != 9 || strcmp(fields[0], "DATA") != 0) return;

  char* end = nullptr;
  unsigned long sample = strtoul(fields[1], &end, 10);
  if (end == fields[1] || *end != '\0') return;

  int mux1_ch = (int)strtol(fields[2], &end, 10);
  if (end == fields[2] || *end != '\0') return;

  int mux2_ch = (int)strtol(fields[3], &end, 10);
  if (end == fields[3] || *end != '\0') return;

  float voltage = strtof(fields[4], &end);
  if (end == fields[4] || *end != '\0' || !isfinite(voltage)) return;
  
  float bat = strtof(fields[5], &end);
  if (end == fields[5] || *end != '\0' || !isfinite(bat)) return;
  
  float curr = strtof(fields[6], &end);
  if (end == fields[6] || *end != '\0' || !isfinite(curr)) return;

  int t2 = (int)strtol(fields[7], &end, 10);
  if (end == fields[7] || *end != '\0') return;
  
  int t3 = (int)strtol(fields[8], &end, 10);
  if (end == fields[8] || *end != '\0') return;

  latest.sample = sample;
  latest.mux1_channel = mux1_ch;
  latest.mux2_channel = mux2_ch;
  latest.voltage = voltage;
  latest.bat = bat;
  latest.curr = curr;
  latest.t2 = t2;
  latest.t3 = t3;
  latest.receivedAtMs = millis();
  latest.valid = true;

  digitalWrite(LED_BUILTIN, latest.t3 ? LOW : HIGH);
}

void readArduinoSerial() {
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
      serialPos = 0;
    }
  }
}

void sendData() {
  char json[380];
  unsigned long age = latest.valid ? millis() - latest.receivedAtMs : 0;

  if (!latest.valid) {
    snprintf(json, sizeof(json),
             "{\"ok\":false,\"error\":\"No valid Arduino DATA packet received yet\","
             "\"sample\":null,\"mux1_channel\":null,\"mux2_channel\":null,\"voltage\":null,"
             "\"bat\":null,\"curr\":null,\"t2\":null,\"t3\":null,\"age_ms\":null}");
  } else {
    snprintf(json, sizeof(json),
             "{\"ok\":true,\"sample\":%lu,\"mux1_channel\":%d,\"mux2_channel\":%d,\"voltage\":%.3f,"
             "\"bat\":%.2f,\"curr\":%.2f,\"t2\":%d,\"t3\":%d,\"age_ms\":%lu}",
             latest.sample, latest.mux1_channel, latest.mux2_channel, latest.voltage,
             latest.bat, latest.curr, latest.t2, latest.t3, age);
  }

  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.sendHeader("Cache-Control", "no-store");
  server.send(200, "application/json", json);
}

void handleSetMux1() {
  if (server.hasArg("channel")) {
    int ch = server.arg("channel").toInt();
    if (ch >= 0 && ch <= 7) {
      ss.print("MUX1,");
      ss.println(ch);
      server.send(200, "application/json", "{\"ok\":true,\"mux1_target\":" + String(ch) + "}");
    } else {
      server.send(400, "application/json", "{\"ok\":false,\"error\":\"Channel must be 0-7\"}");
    }
  } else {
    server.send(400, "application/json", "{\"ok\":false,\"error\":\"Missing channel parameter\"}");
  }
}

void handleSetMux2() {
  if (server.hasArg("channel")) {
    int ch = server.arg("channel").toInt();
    if (ch >= 0 && ch <= 7) {
      ss.print("MUX2,");
      ss.println(ch);
      server.send(200, "application/json", "{\"ok\":true,\"mux2_target\":" + String(ch) + "}");
    } else {
      server.send(400, "application/json", "{\"ok\":false,\"error\":\"Channel must be 0-7\"}");
    }
  } else {
    server.send(400, "application/json", "{\"ok\":false,\"error\":\"Missing channel parameter\"}");
  }
}

void sendRoot() {
  server.send(200, "text/plain", "CerePhyte Rev. 4.5 local API\nGET /api/data\nGET /api/mux1?channel=x\nGET /api/mux2?channel=x\n");
}

void startApiOnce() {
  if (!apiStarted) {
    server.on("/", HTTP_GET, sendRoot);
    server.on("/api/data", HTTP_GET, sendData);
    server.on("/api/mux1", HTTP_GET, handleSetMux1);
    server.on("/api/mux2", HTTP_GET, handleSetMux2);
    server.onNotFound([]() {
      server.send(404, "application/json", "{\"ok\":false,\"error\":\"Not found\"}");
    });
    server.begin();
    apiStarted = true;
  }
}

void setup() {
  Serial.begin(115200);
  ss.begin(9600);

  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);

  WiFi.persistent(false);
  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(IPAddress(192, 168, 4, 1), IPAddress(192, 168, 4, 1), IPAddress(255, 255, 255, 0));
  WiFi.softAP(apSsid, apPassword);
}

void loop() {
  readArduinoSerial();
  startApiOnce();
  if (apiStarted) server.handleClient();
  readArduinoSerial();
}