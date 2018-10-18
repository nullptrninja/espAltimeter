#include "stringBuffer.h"

extern StringBuffer gLogEntryBuf;

// A single data log entry. Note that ToString() accesses a global buffer so don't fuck this up.
class LogEntry {  
  public:    
    static const short LogStringCharCount = 50;    
    
    LogEntry(unsigned int time, int pressure, float temperature, float altitude)
      : TimeStamp(time), Pressure(pressure), Temperature(temperature), Altitude(altitude)
    {}

    // Public fields. Yolo.
    unsigned int TimeStamp;   // Millis
    int Pressure;             // Pascals
    float Temperature;        // Centigrade
    float Altitude;           // Meters
    
    short renderStringToBuffer() {
      String strTime = String(TimeStamp);
      String strPress = String(Pressure);
      String strTemp = String(Temperature);
      String strAlt = String(Altitude);

      // Log entries are specifically hardcoded to write to the global entry buffer
      // under the very specific assumption that only one web client is connected to
      // the device and no threading is used. 
      gLogEntryBuf.clear();
      gLogEntryBuf.append(strTime.c_str());
      gLogEntryBuf.append(',');
      gLogEntryBuf.append(strPress.c_str());
      gLogEntryBuf.append(',');
      gLogEntryBuf.append(strTemp.c_str());
      gLogEntryBuf.append(',');
      gLogEntryBuf.append(strAlt.c_str());
      gLogEntryBuf.append("\r\n");

      return gLogEntryBuf.length();
    }
};
