#include <stdlib.h>
#include <vector>
#include <string>

#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>

#include <Wire.h>
#include <SPI.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BMP280.h>

#include "logEntry.h"
#include "stringBuffer.h"

using namespace std;

////////////////////////////////////////////////////
// Constants

// Endpoints
const char *apiPostLog_Start = "/api/log/start";
const char *apiPostLog_Stop = "/api/log/stop";
const char *apiGetLog_Fetch = "/api/log/fetch";
const char *apiGetState_Live = "/api/live";
const char *apiPostConfig_LogInterval = "/api/config/loggingInterval";
const char *apiPostConfig_MaxLogEntries = "/api/config/maxLogEntries";
const char *apiPostConfig_LocalSeaLevelPressure = "/api/config/localSeaLevelPressure";

// BMP PINS
#define BMP_SCK 13
#define BMP_MISO 12
#define BMP_MOSI 11
#define BMP_CS 10

const short LOG_FETCH_PAGE_SIZE = 50;

const short STATE_IDLE = 0;
const short STATE_LOGGING = 1;
////////////////////////////////////////////////////

// Web server shit. This may or may not apply depending on if your ESP8266 has been already
// configured or has a blank configuration.
const char *ssid = "altimeter";
const char *password = "derp";

////////////////////////////////////////////////////
// Global buffers
// These global buffers are used as a "register" for various parts of the sketch to read/write from/to.
// Since there's a lot of heap-string operations in here, we allocate these ahead of time to prevent
// heap fragmentation from affecting these core buffers. The only assumption we're making is that only
// one web client is connected at a time, otherwise you will get a whole buncha garbage in yo buffers.
StringBuffer gLogFetchBuf(LOG_FETCH_PAGE_SIZE * LogEntry::LogStringCharCount);
StringBuffer gLogEntryBuf(LogEntry::LogStringCharCount);


// Application state
short currentState;
short loggingInterval;
short maxLogEntries;
float localSeaLevelPressure;
unsigned int timeOffset;            // Used to log out in t-0 format
vector<LogEntry *> logEntries;

bool bmpSensorOk;
Adafruit_BMP280 bmpSensor;      // In I2C mode

ESP8266WebServer server(80);

///////////////////////////////////////////////
// Utility

void resetLogs() {
  for (int i = 0; i < logEntries.size(); i++) {
    delete logEntries[i];
    logEntries[i] = NULL;
  }
  
  logEntries.clear();
}

bool waitForBmpSensor() {
  unsigned int nextCheckTime = millis();
  int attempts = 10;
  while (attempts > 0) {    
    if (!bmpSensor.begin()) {
      nextCheckTime = millis() + 500;
      attempts--;
      
      // Wait a bit and try again
      while (millis() < nextCheckTime);
    }
    else {
      return true;
    }
  }

  return false;
}
///////////////////////////////////////////////

///////////////////////////////////////////////
// End point handlers
void handleRoot() {
  String response = String("State: ") + String(currentState) +
                    String("\r\nLogSize: ") + String(logEntries.size()) +
                    String("\r\nSea Level hPa: ") + String(localSeaLevelPressure) +
                    String("\r\nBMP Sensor: ") + String(bmpSensorOk ? "OK" : "FAIL");

  server.send(200, "text/html", response);
}

void apiHandleGetLive() {
  String response = String("Time (ms): ") + String(millis() - timeOffset) +
                    String("\r\nTemperature (C): ") + String(bmpSensor.readTemperature()) +
                    String("\r\nAltitude (m): ") + String(bmpSensor.readAltitude(localSeaLevelPressure));

  server.send(200, "text/html", response);
}

void apiHandlePostLogStart() {
  resetLogs();
  currentState = STATE_LOGGING;
  timeOffset = millis();
  server.send(200, "text/html", "Logging started");
}

void apiHandlePostLogStop() {
  currentState = STATE_IDLE;  
  server.send(200, "text/html", "Logging stopped");
}

// This retrieves the data log by page parameter. Your fetching iterator should stop fetching
// when the number of CSV rows coming back is less than LOG_FETCH_PAGE_SIZE or zero.
void apiHandleGetLogFetch() {
  if (currentState != STATE_IDLE) {
    server.send(200, "text/html", "Stop logging first");
  }

  String pageValue = server.arg("page");
  int page = pageValue.toInt();
  
  int startIndex = page * LOG_FETCH_PAGE_SIZE;
  gLogFetchBuf.clear();
  
  for (int i = startIndex; i < (startIndex + LOG_FETCH_PAGE_SIZE) && i < logEntries.size(); i++) {
    // This writes the log entry to gLogEntryBuf
    (*logEntries[i]).renderStringToBuffer();

    gLogFetchBuf.append(gLogEntryBuf.c_str());
  }
  
  server.send(200, "text/html", gLogFetchBuf.c_str());
}

void apiHandlePostConfigLogInterval() {
  String value = server.arg("value");
  loggingInterval = value.toInt();
  
  server.send(200, "text/html", String(loggingInterval));
}

void apiHandlePostConfigMaxLogEntries() {
  String value = server.arg("value");
  maxLogEntries = value.toInt();
  
  server.send(200, "text/html", String(maxLogEntries));
}

void apiHandlePostConfigLocalSeaLevelPressure() {
  String value = server.arg("value");
  localSeaLevelPressure = value.toFloat();
  
  server.send(200, "text/html", String(localSeaLevelPressure));
}

////////////////////////////////////////////
// State machine

void doLogging() {
  static unsigned long nextLogTime = 0;

  // To prevent crashes when we're at the memory ceiling (which we initially tuned before compile time)
  // we will insert a known-stop value as the last log entry with implausible temperature data
  if (logEntries.size() > maxLogEntries - 1) {
    LogEntry *entry = new LogEntry(millis(), 150, 0);
    logEntries.push_back(entry);
    currentState = STATE_IDLE;
    return;    
  }
  
  if (millis() >= nextLogTime) {
    // Capture and log data from sensor    
    float temperature = bmpSensor.readTemperature();
    float altitude = bmpSensor.readAltitude(localSeaLevelPressure);

    LogEntry *entry = new LogEntry(millis() - timeOffset, temperature, altitude);
    logEntries.push_back(entry);

    nextLogTime = millis() + loggingInterval;
  }
}

////////////////////////////////////////////
void setup() {  
  Serial.begin(115200);

  WiFi.softAP(ssid, password);

  IPAddress ip = WiFi.softAPIP();
  Serial.println("Server IP: ");
  Serial.print(ip);

  ////////////////////////////////////////////
  // Configure routes
  server.on("/", handleRoot);       // This fetches the status of the device

  // Log routes
  server.on(apiPostLog_Start, HTTP_POST, apiHandlePostLogStart);
  server.on(apiPostLog_Stop, HTTP_POST, apiHandlePostLogStop);
  server.on(apiGetLog_Fetch, HTTP_GET, apiHandleGetLogFetch);

  // Config routes
  server.on(apiPostConfig_LogInterval, HTTP_POST, apiHandlePostConfigLogInterval);
  server.on(apiPostConfig_MaxLogEntries, HTTP_POST, apiHandlePostConfigMaxLogEntries);
  server.on(apiPostConfig_LocalSeaLevelPressure, HTTP_POST, apiHandlePostConfigLocalSeaLevelPressure);

  // Misc
  server.on(apiGetState_Live, HTTP_GET, apiHandleGetLive);

  ////////////////////////////////////////////
  
  server.begin();

  currentState = STATE_IDLE;
  loggingInterval = 200;
  maxLogEntries = 1900;         // On an Adafruit Huzzah ESP8266 board this is our safe-limit
  localSeaLevelPressure = 1000; // This is just some default value, you NEED to calibrate this on flight-day to hPa pressure at sea level
  
  // Start sensor  
  bmpSensorOk = waitForBmpSensor();
}

void loop() {
  server.handleClient();
  
  switch (currentState) {    
    case STATE_IDLE:
      // NOOP
      break;
      
    case STATE_LOGGING:
      doLogging();
      break;

    // Other states here if needed
  }
}
