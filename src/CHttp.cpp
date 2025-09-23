#include "CHttp.h"
#include "mjson.h"

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

/*construct*/
cHTTP::cHTTP()
{
    m_u16HttpPort = 3000;   /*port to upload*/
    m_u16ServerPort = 5000; /*Pport to connect*/
    currprogress = 0;
}

/*destruct*/
cHTTP::~cHTTP()
{
}

/*************************************************************************
 *   Function to upload frame to server using HTTP
 *   Data-> Holds the buffer to upload
 **************************************************************************/
uint8_t cHTTP::uploadDataFrame(char *Data)
{
    uint8_t ret = 0;
    if (!m_bHttpBusy)
    {
        m_bHttpBusy = true;
        debugPrint("[HTTP] IsConnected : ");
        debugPrintln(http.connected());
        debugPrintln("[HTTP] begin : Frame");
        debugPrintln(Data);
        /*New Url HTTP link for uploading data in firebase*/
        char link[150];
        sprintf(link, "http://%s:%d/api/do/createDoReadings", m_cServerIP, m_u16HttpPort);
        http.begin(link);
        /*Specify content-type header*/
        http.addHeader("Content-Type", "application/json");
        /* Added to resolve isssue with time out */
        http.setTimeout(30000);

        int httpCode = http.POST(Data);
        debugPrint("httpCode: ");
        debugPrintln(httpCode);
        debugPrintln(http.errorToString(httpCode));
        
        if (httpCode == HTTP_CODE_OK)
        {
            debugPrintln("@@ Uploded to Nextaqua server :-)");
            ret = 1;
        }
        else
        {
            debugPrintln("@@ failed to send..:-(");
            ret = 0;
        }
        String payload = http.getString();
        debugPrintln(payload);
        http.end();
    }
    else
    {
        ret = 0;
        debugPrintln("@@ HTTP Busy");
    }
    debugPrintln("[HTTP] end : Frame");
    m_bHttpBusy = false;
    return ret;
}

/*************************************************************************
 *   Function to upload the data into ping server
 *   Data-> Holds the buffer to upload
 **************************************************************************/
time_t cHTTP::uploadPingFrame(char *Data)
{
    time_t retEpoch = 0;
    if (!m_bHttpBusy)
    {
        m_bHttpBusy = true;
        debugPrint("[HTTP] IsConnected : Ping");
        debugPrintln(http.connected());
        debugPrintln("[HTTP] begin : Ping");
        /*HTTP link for ping server and get Epoch*/
        char link[150];
        sprintf(link, "http://%s:%d/ping", m_cServerIP, m_u16HttpPort);
        http.begin(link);
        /*Specify content-type header*/
        http.addHeader("Content-Type", "application/json");
        /* Added to resolve isssue with time out */
        http.setTimeout(30000);
        // debugPrintln(Data);
        int httpCode = http.GET();
        debugPrintln(http.errorToString(httpCode));
        if (httpCode == HTTP_CODE_OK)
        {
            debugPrintln("@@ Uploded Ping Frame :-)");
            m_bIsConnected = true;
            /* Payload format {"statusCode":200,"statusMessage":"Success","serverEpoch":1625899085}*/
            String payload = http.getString();
            debugPrintln(payload);
            double num = -1;
            double offset = -1;
            mjson_get_number(payload.c_str(), strlen(payload.c_str()), "$.serverEpoch", &num);
            mjson_get_number(payload.c_str(), strlen(payload.c_str()), "$.offset", &offset);
            if (num != -1)
            {
                /*To get Local epoch*/
                retEpoch = (time_t)(num / 1000);
            }
        }
        else
        {
            debugPrintln("@@ failed to send Ping -(");
            m_bIsConnected = false;
        }
        http.end();
    }
    else
    {
        debugPrintln("@@ HTTP Busy");
    }
    debugPrintln("[HTTP] end : Ping");
    m_bHttpBusy = false;
    return retEpoch;
}

/*************************************************************************
 * Function to get the device configuration from the api link
 * Data-> Holds the buffer to upload
 * @param [in] device MacAddress
 * @param [out] None
 **************************************************************************/
uint8_t cHTTP::getConfig(char *Data)
{
    uint8_t ret = 0;
    if (!m_bHttpBusy)
    {
        m_bHttpBusy = true;
        debugPrintln("[HTTP] begin : GetD");
        char link[150];
        sprintf(link, "http://%s:%d/api/do/getconfiguration?%s", m_cServerIP, m_u16HttpPort, Data);
        /*Link*/
        debugPrintln(link);
        http.begin(link);
        http.setTimeout(30000);
        http.addHeader("Content-Type", "application/json");
        debugPrintln(Data);
        int httpCode = http.GET();
        debugPrintln(http.errorToString(httpCode));
        if (httpCode == HTTP_CODE_OK)
        {
            Serial.println("@@ Requested getDevice data :-)");
            m_sPayload = http.getString();
            debugPrintln(m_sPayload);
            ret = 1;
        }
        else
        {
            Serial.println("@@ failed to send getDevice  :-(");
            ret = 0;
        }
    }
    else
    {
        ret = 0;
        Serial.println("@@ HTTP Busy or Not connected");
    }
    http.end();
    Serial.println("[HTTP] end : GetD");
    m_bHttpBusy = false;
    return ret;
}

/*************************************************************************
 * Function to get the device configuration from the api link
 * Data-> Holds the buffer to upload
 * @param [in] device MacAddress
 * @param [out] None
 **************************************************************************/
uint8_t cHTTP::getPondBoundaries(char *Data)
{
    uint8_t ret = 0;
    if (!m_bHttpBusy)
    {
        m_bHttpBusy = true;
        debugPrintln("[HTTP] begin : GetD");
        char link[150];
        sprintf(link, "http://%s:%d/api/do/getPondsBoundaries?%s", m_cServerIP, m_u16HttpPort, Data);
        /*Link*/
        debugPrintln(link);
        http.begin(link);
        http.setTimeout(30000);
        http.addHeader("Content-Type", "application/json");
        debugPrintln(Data);
        int httpCode = http.GET();
        debugPrintln(http.errorToString(httpCode));
        if (httpCode == HTTP_CODE_OK)
        {
            debugPrintln("@@ Requested get pond Boundaries :-)");
            m_sPayload = http.getString();
            debugPrintln(m_sPayload);
            ret = 1;
        }
        else
        {
            debugPrintln("@@ failed to get pond Boundaries  :-(");
            ret = 0;
        }
    }
    else
    {
        ret = 0;
        debugPrintln("@@ HTTP Busy or Not connected");
    }
    http.end();
    debugPrintln("[HTTP] end : GetD");
    m_bHttpBusy = false;
    return ret;
}

uint8_t cHTTP::performOTA(cBsp *myBsp)
{
    uint8_t ret = 0;
    if (!m_bHttpBusy)
    {
        m_bHttpBusy = true;
        debugPrint("[HTTP] IsConnected : ");
        debugPrintln(http.connected());
        debugPrintln("[HTTP] begin : Frame");
        debugPrintf("Connecting to firmware URL : %s\n", m_cUriFirmwareFOTA);
        /*New Url HTTP link for downloading the bin file*/
        http.begin(OTAclient, m_cUriFirmwareFOTA);
        /* Added to resolve isssue with time out */
        int httpCode = http.GET();

        if (httpCode == HTTP_CODE_OK)
        {
            int contentLength = http.getSize(); // get firmware size
            debugPrintf("Length: %d\n", contentLength);
            if (Update.begin(contentLength))
            {
                debugPrintln("Begin OTA. This may take a while...");
                int written = 0;
                int totalWritten = 0;
                uint8_t buff[2048] = {0};

                WiFiClient *stream = http.getStreamPtr();
                debugPrint("OTA start :");
                unsigned long st = millis();
                debugPrintln(st);
                // Read stream and write it to the Update object
                int prevprogress = 0;
                while (http.connected() && (totalWritten < contentLength || contentLength == -1))
                {
                    myBsp->wdtfeed();
                    size_t size = stream->available();
                    if (size)
                    {
                        int c = stream->readBytes(buff, ((size > sizeof(buff)) ? sizeof(buff) : size));
                        // debugPrintf("read: %d\n", c);
                        written = Update.write(buff, c);
                        totalWritten += written;
                        // Print progress in percentage
                        prevprogress = currprogress;
                        currprogress = (totalWritten * 100) / contentLength;
                        if (currprogress != prevprogress)
                            debugPrintf("Progress: %d \n", currprogress);
                    }
                }
                debugPrint("OTA End :");
                unsigned long et = millis();
                debugPrintln(et);
                debugPrint("Duration:");
                debugPrintln(et - st);
                if (totalWritten == contentLength)
                {
                    debugPrintln("Written : " + String(totalWritten) + " successfully");
                }
                else
                {
                    debugPrintln("Written only : " + String(totalWritten) + "/" + String(contentLength) + ". Retry?");
                }

                if (Update.end())
                {
                    if (Update.isFinished())
                    {
                        debugPrintln("OTA update has successfully finished. Rebooting...");
                        delay(2000);
                        ESP.restart();
                    }
                    else
                    {
                        debugPrintln("OTA update not finished. Something went wrong!");
                        ret = 4;
                    }
                }
                else
                {
                    debugPrintln("Error Occurred. Error #: " + String(Update.getError()));
                    ret = 3;
                }
            }
            else
            {
                debugPrintln("Not enough space to begin OTA");
                ret = 2;
            }
        }
        else
        {
            debugPrintf("Cannot download firmware. HTTP error code: %d\n", httpCode);
            ret = 1;
        }
        http.end();
    }
    else
    {
        ret = 5;
        debugPrintln("@@ HTTP Busy");
    }
    m_bHttpBusy = false;
    return ret;
}
