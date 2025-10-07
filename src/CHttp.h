#ifndef HTTP_H
#define HTTP_H

#include <stdint.h>
#include <HTTPClient.h>
#include <Update.h>
#include <BSP.h>

class cHTTP
{
private:
    HTTPClient http; // HTTP Class
    WiFiClient OTAclient;

public:
    bool m_bHttpBusy;
    bool m_bIsConnected;
    String m_sPayload;
    char m_cServerIP[25];
    char m_cDefaultServerIP[25] = "34.93.69.40";
    uint16_t m_u16ServerPort;
    uint16_t m_u16HttpPort;
    char m_cUriFirmwareFOTA[150] = "";
    int currprogress;
    /*construct*/
    cHTTP(void);
    /*destruct*/
    ~cHTTP(void);
    /*functions*/
    uint8_t uploadDataFrame(char *);
    time_t uploadPingFrame(char *);
    uint8_t getConfig(char *);
    uint8_t getPondBoundaries(char *);
    uint8_t performOTA(cBsp *myBsp);
};

#endif