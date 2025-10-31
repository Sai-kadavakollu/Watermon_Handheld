#include "GPS.h"

// #define SERIAL_DEBUG
#ifdef SERIAL_DEBUG
#define debugPrint(...) Serial.print(__VA_ARGS__)
#define debugPrintln(...) Serial.println(__VA_ARGS__)
#define debugPrintf(...) Serial.printf(__VA_ARGS__)
#define debugPrintlnf(...) Serial.println(F(__VA_ARGS__))
#else
#define debugPrint(...)    // blank line
#define debugPrintln(...)  // blank line
#define debugPrintf(...)   // blank line
#define debugPrintlnf(...) // blank line
#endif

TinyGPSPlus gps1;

/* Construct */
CGps::CGps() {}
/* Destruct */
CGps::~CGps() {}

void CGps::gpsInit(Stream *serialHandle)
{
  _gpsSerial = serialHandle;
}

/*****************************************
 * Conver GPS time to Ecpoch
 *****************************************/
time_t CGps::ConvertToEpoch(uint16_t year, uint8_t mon, uint8_t date, uint8_t hour, uint8_t min, uint8_t sec)
{
  uint32_t t;
  // January and February are counted as months 13 and 14 of the previous year
  if (mon <= 2)
  {
    mon += 12;
    year -= 1;
  }
  // Convert years to days
  t = (365 * year) + (year / 4) - (year / 100) + (year / 400);
  // Convert months to days
  t += (30 * mon) + (3 * (mon + 1) / 5) + date;
  // Unix time starts on January 1st, 1970
  t -= 719561;
  // Convert days to seconds
  t *= 86400;
  // Add hours, minutes and seconds
  t += (3600 * hour) + (60 * min) + sec;
  // Return Unix time
  return (time_t)(t);
}


void CGps::gpstask(void)
{
  static String nmeaLine;
  static bool serialStarted = false;
  while (_gpsSerial->available() > 0)
  {
    serialStarted = true;
    char c = _gpsSerial->read();
    gps1.encode(c);

    if (gps1.location.isUpdated() || gps1.time.isUpdated())
    {
      /*Lats and Longs*/
      mPosition.m_lat = gps1.location.lat();
      mPosition.m_lng = gps1.location.lng();
      /*Whether the GPS is valid or not*/
      if (gps1.location.isValid() && gps1.satellites.value() >= 4 && (gps1.hdop.hdop() < 3.0 && gps1.hdop.hdop() > 0.0))
      {
          m_bIsValid = true;
      }
      else
      {
          m_bIsValid = false;
      }
      /*HDop and Satellites*/
      if (gps1.hdop.isValid()) {
          mPosition.hDop = gps1.hdop.hdop();
      }
      

      mPosition.m_iSatellites = gps1.satellites.value();
    
      GpsHour = gps1.time.hour();
      GpsMins = gps1.time.minute();
      GpsDay = gps1.date.day();
      uint8_t GpsSec = gps1.time.second();

      uint8_t month = gps1.date.month();
      uint32_t year = gps1.date.year();

      Epoch = ConvertToEpoch(year, month, GpsDay, GpsHour, GpsMins, GpsSec);

      debugPrint(GpsHour);
      debugPrint(" : ");
      debugPrint(GpsMins);
      debugPrint(" : ");
      debugPrint(GpsSec);
      debugPrint("@ ");
      debugPrint(GpsDay);
      debugPrint("/");
      debugPrint(month);
      debugPrint("/");
      debugPrintln(year);
      debugPrint("Epoch  :");
      debugPrintln(Epoch);
    }
  }
}


// time_t CGps:: getEpoch()
// {
//   return Epoch;
// }
