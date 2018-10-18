#include <stdlib.h>
#include <vector>
#include <string>

#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>

#include "logEntry.h"
#include "stringBuffer.h"

using namespace std;

////////////////////////////////////////////////////
// Constants

// Endpoints
const char* apiPostLog_Start = "/api/log/start";
const char* apiPostLog_Stop = "/api/log/stop";
const char* apiGetLog_Fetch = "/api/log/fetch";
const char* apiPostConfig_LogInterval = "/api/config/loggingInterval";
const char* apiPostConfig_MaxLogEntries = "/api/config/maxLogEntries";
const char* apiPostConfig_LocalSeaLevelPressure = "/api/config/localSeaLevelPressure";

const short DATAPIN = 13;
const short LOG_FETCH_PAGE_SIZE = 50;

const short STATE_IDLE = 0;
const short STATE_LOGGING = 1;
////////////////////////////////////////////////////

// Web server shit
// 192.168.4.1 seems to be the default IP
const char* ssid = "altimeter";
const char* password = "derp";

////////////////////////////////////////////////////
// Global buffers
// These global buffers are used as a "register" for various parts of the sketch to read/write from/to.
// Since there's a lot of heap-string operations in here, we allocate these ahead of time to prevent
// heap fragmentation from affecting these core buffers. The only assumption we're making is that only
// one web client is connected at a time, otherwise you will get a whole buncha garbage in yo buffers.
StringBuffer gLogFetchBuf(LOG_FETCH_PAGE_SIZE);
StringBuffer gLogEntryBuf(LogEntry::LogStringCharCount);


// Application state
short currentState;
short loggingInterval;
short maxLogEntries;
float localSeaLevelPressure;
vector<LogEntry*> logEntries;

ESP8266WebServer server(80);

///////////////////////////////////////////////
// Utility

// This is only when we don't have a neo pixel attached
void setLedPin(bool on) {
  digitalWrite(DATAPIN, on ? HIGH : LOW);
}

void resetLogs() {
  for (int i = 0; i < logEntries.size(); i++) {
    delete logEntries[i];
    logEntries[i] = NULL;
  }
  
  logEntries.clear();
}

///////////////////////////////////////////////

///////////////////////////////////////////////
// End point handlers
void handleRoot() {
  String response = String("State: ") + String(currentState) +
                    String("\r\nLogSize: ") + String(logEntries.size()) +
                    String("\r\nSea Level hPa: ") + String(localSeaLevelPressure);

  server.send(200, "text/html", response);
}

void apiHandlePostLogStart() {
  resetLogs();
  currentState = STATE_LOGGING;
  setLedPin(true);
  server.send(200, "text/html", "Log cleared\r\nLog started");
}

void apiHandlePostLogStop() {
  currentState = STATE_IDLE;
  setLedPin(false);
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
    LogEntry* entry = new LogEntry(millis(), 0, 150, 0);
    logEntries.push_back(entry);
    currentState = STATE_IDLE;
    return;    
  }
  
  if (millis() >= nextLogTime) {
    // Capture and log data from sensor
    int pressure = 1;
    float temperature = 2;
    float altitude = 1;    

    LogEntry* entry = new LogEntry(millis(), pressure, temperature, altitude);
    logEntries.push_back(entry);

    nextLogTime = millis() + loggingInterval;
  }
}

////////////////////////////////////////////
void setup() {
  // Configure pins
  pinMode(DATAPIN, OUTPUT);

  delay(1000);
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

  ////////////////////////////////////////////
  
  server.begin();

  currentState = STATE_IDLE;
  loggingInterval = 200;
  maxLogEntries = 3000;         // This will need adjusting as the final mem footprint is known
  localSeaLevelPressure = 1000; // This is just some default value, you NEED to calibrate this on flight-day to hPa pressure at sea level
}

void loop() {
  server.handleClient();
  
  switch (currentState) {
    case STATE_IDLE:
      break;
      
    case STATE_LOGGING:
      doLogging();
      break;
  }
}
