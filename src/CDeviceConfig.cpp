#include "CDeviceConfig.h"
#include <ArduinoJson.h>

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

/*Construct*/
CDeviceConfig::CDeviceConfig()
{
    m_u8IsReboot = 1;
    espResetReason = 0;
    m_tEpoch = 0;
    m_bIsSafeModeOn = false;
    m_u8ReceivedConfigType = NO_CNFG;
}
/*Destruct*/
CDeviceConfig::~CDeviceConfig()
{
}
