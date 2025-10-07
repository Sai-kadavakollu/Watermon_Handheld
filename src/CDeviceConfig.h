#ifndef DEVICECONFIG_H
#define DEVICECONFIG_H

#include "stdint.h"
#include "esp_system.h"
#include "Ticker.h"
#include "FileSystem.h"

#define FNAME_SITECONFIG "/siteConfig.txt"

typedef enum
{
    NO_CNFG = 0,
    CNFG_SND
} ConfigFrametype;

class CDeviceConfig
{
public:
    /*Device Data*/
    char m_cDeviceId[25];
    char m_cSiteId[25];
    char m_cUserName[25];
    char m_cDeviceName[10];
    char m_cPname[10];
    char m_cPid[10];

    uint8_t m_u8IsReboot;
    int espResetReason;
    time_t m_tEpoch;
    bool m_bIsSafeModeOn;
    ConfigFrametype m_u8ReceivedConfigType;

    /*Construct*/
    CDeviceConfig(void);
    /*Destruct*/
    ~CDeviceConfig(void);
    void setDeciveConfig(FILESYSTEM *fileSystem);
};

#endif