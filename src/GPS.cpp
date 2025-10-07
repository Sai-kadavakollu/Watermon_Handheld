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

/*****************************************
 * Conver GPS time to Ecpoch
 *****************************************/
time_t ConvertToEpoch(uint16_t year, uint8_t mon, uint8_t date, uint8_t hour, uint8_t min, uint8_t sec)
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
  static bool serialStarted = false;
  while (Serial2.available() > 0)
  {
    serialStarted = true;
    gps1.encode(Serial2.read());
    if (gps1.location.isUpdated() || gps1.time.isUpdated())
    {
      /*Lats and Longs*/
      mPosition.m_lat = gps1.location.lat();
      mPosition.m_lng = gps1.location.lng();
      /*HDop and Satellites*/
      mPosition.hDop = gps1.hdop.hdop();
      if (gps1.satellites.isUpdated())
      {
        mPosition.m_iSatellites = gps1.satellites.value();
        // Serial.print("Satellites in view: ");
        // Serial.println(mPosition.m_iSatellites);
      }
      GpsHour = gps1.time.hour();
      GpsMins = gps1.time.minute();
      GpsDay = gps1.date.day();
      uint8_t sec = gps1.time.second();

      uint8_t month = gps1.date.month();
      uint32_t year = gps1.date.year();

      Epoch = ConvertToEpoch(year, month, GpsDay, GpsHour, GpsMins, sec);

      debugPrint(hour);
      debugPrint(" : ");
      debugPrint(min);
      debugPrint(" : ");
      debugPrint(sec);
      debugPrint("@ ");
      debugPrint(date);
      debugPrint("/");
      debugPrint(month);
      debugPrint("/");
      debugPrintln(year);
      debugPrint("Epoch  :");
      debugPrintln(Epoch);
    }
  }
}

cPosition CGps::getPondLocation()
{
  return mPosition;
}

// time_t CGps:: getEpoch()
// {
//   return Epoch;
// }
