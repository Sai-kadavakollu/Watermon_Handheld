#include "CApplication.h"
#include <WiFi.h>
#include <sstream>
#include <mjson.h>
#include <WebSocketsClient_Generic.h>
#include <SocketIOclient_Generic.h>
#include <WebServer.h>
#include <DNSServer.h>

// #define SERIAL_DEBUG
#ifdef SERIAL_DEBUG
#define debugPrint(...) Serial.print(__VA_ARGS__)
#define debugPrintln(...) Serial.println(__VA_ARGS__)
#define debugPrintf(...) Serial.printf(__VA_ARGS__)
#define debugPrintlnf(...) Serial.println(F(__VA_ARGS__))
#define WAITTIME 2
#else
#define debugPrint(...)    // blank line
#define debugPrintln(...)  // blank line
#define debugPrintf(...)   // blank line
#define debugPrintlnf(...) // blank line
#define WAITTIME 6
#endif
#define INTERVAL_TIME_IN_SECONDS 180
#define INTERVAL_WAKEUP_TIME_IN_SECONDS 180
#define TIMER_WAKEUP_ENABLE
#define SLEEP_TIME_IN_MICROSECONDS INTERVAL_TIME_IN_SECONDS * 1000000

#define HOOTER_ON 0


SocketIOclient socketIO;
const char *protocol = "arduino";

/*Objects initialization*/
cBsp m_oBsp;
Ticker AppTimer;
cPCF85063A m_oRtc; // define a object of PCF85063A class
FILESYSTEM m_oFileSystem;
CBackupStorage m_oBackupStore;
CDeviceConfig m_oConfig;
cHTTP m_oHttp;
CSensor m_oSensor;
CGps m_oGps;
CDisplay m_oDisp;
Preferences m_oMemory;
Geofence m_oGeofence;
CPondConfig m_oPond(&m_oFileSystem);
CPN532 m_oRfid;
volatile ButtonState_t ButtonState;

bool m_bGetConfig = true;
bool m_bGetRoutine = false;
bool m_bSendframe = false;
bool isOnline = false;
bool RTCSyncNow = false;
bool ResetEntireMap = false;
bool pingNow = false;
bool updateConfig = false;
bool m_bDoFota = false;
bool m_bGetPondLocation = false;
bool m_bGotPondName = false;
bool m_bGetPondDetails = false;
bool FoundPondName = false;
long ButtonPressedMillis;

bool morningCheckedThisBoot = false;
bool eveningCheckedThisBoot = false;
uint8_t MorningPondMapResetTime = 2;
uint8_t EveningPondMapResetTime = 14;


/*Smart Config Objects and variables BEGIN*/
WebServer server(80);
DNSServer dnsServer;

String Myname, NewSsid, NewPassword, MyPassKey = "12345678";
const byte DNS_PORT = 53;
bool IsRecievedConfig = false;
unsigned long reBoot;
unsigned long SmartConfigTimeoutMillis;
/*Smart Config Objects and variables END*/
//For short and long button press capturing
bool lastButtonState = HIGH;
unsigned long pressStartTime = 0;

bool m_bIsGPS = false;
bool showReadWriteCycle = false;
bool m_bIsCharging = false;
bool EnterConfigRestart = false;
time_t m_tPingEpoch = 0;

int m_iTimeOutFrameCounter = 0;
int TotalMinsOffSet = 330;
int buzz = 0;
int m_iOperationMode = EVENT_BASED_MODE;
int iNoActivityCounter = 0;
int sendFrameType = NO_FRAME;
int rebootAfterSetDataCmd = -1;
int rebootAfterOfflineCnt = 0;
int m_iDataFrequencyInterval = 5;
int m_iNetCheckCounter = 0;
int CountDownTimer = 0;

float voltageValue = 0.0;
float DoMglValue = 0.0;
float DoSaturationVal = 0.0;
float TempVal = 0.0;

char sendResult[1024];
char timebuffer[6];

char CurrntPondName[20];
char CurrentLocationId[100];
char CurrentPondID[100];
float CurrentPondSalinity;

char nearestPondName[10] = "";
int nearestPondDistance = 0;
bool isNearToSomePond = false;

int lastMorningDay;
int lastEveningDay;

double SimulatedLat = 0.00000;
double SimulatedLongs = 0.00000;
bool Is_Simulated_Lat_Longs = false;

/*device config*/
char m_cWifiPass[20]; /*wifi password*/
char m_cWifiSsid[20]; /*wifi name*/

/******************
 *   constructor
 *******************/
cApplication::cApplication()
{
    m_u8SendBackUpFrameConter = 0;
    m_iRtcSyncCounter = 0;
    m_iFrameInProcess = NO_FRAME;
    m_iButtonLongPressCounter = -1;
    iNoActivityCounter = 0;
}

/*****************
 *   Destruct
 ******************/
cApplication::~cApplication()
{
}

/****************************************************************
 *   Safe copy copy with Overflow protection
 *****************************************************************/
static void safeStrcpy(char *destination, const char *source, int sizeofDest)
{
    strncpy(destination, source, sizeofDest - 1);
    destination[sizeofDest - 1] = 0;
}

float cApplication::roundToDecimals(float value, int decimals)
{
    float factor = pow(10, decimals);
    return round(value * factor) / factor;
}
/****************************************************************
 *   When we're reconnected, report our current state to shadow
 *****************************************************************/
static void onConnected()
{
    debugPrintln("onConnected....");
    m_bGetConfig = true;
    isOnline = true;
    pingNow = true;

    char LocalIp[25];
    String ipStr = WiFi.localIP().toString();
    strncpy(m_oDisp.DisplayFooterData.LocalIp, ipStr.c_str(), sizeof(m_oDisp.DisplayFooterData.LocalIp));
}

/*****************************************************
 *   When we're disconnected it comes to this event
 ******************************************************/
static void onDisConnected()
{
    debugPrintln("DisConnected.. :-(");
    isOnline = false;

    char LocalIp[25];
    String ipStr = WiFi.localIP().toString();
    strncpy(m_oDisp.DisplayFooterData.LocalIp, ipStr.c_str(), sizeof(m_oDisp.DisplayFooterData.LocalIp));
}

/************************************************************************
 * Wifi setup function which is called by the mDash library
 * wifi_network_name-> string holds wifi name
 * wifi_pass-> string holds wifi password
 * @param [in] wifi network name, wifi network password
 * @param [out] None
 *************************************************************************/
static void init_wifi(const char *wifi_network_name, const char *wifi_pass)
{
    WiFi.begin(wifi_network_name, wifi_pass);
}

/***********************************************
 *  RPC Function to set the Calibration values to the DO sensor
 *  r-> pointer holds the Item data buffer
 *************************************************/
static void RPChandler_setCalValues(struct jsonrpc_request *r)
{
    char buff[30];
    snprintf(buff, r->params_len + 1, "%S", (wchar_t *)r->params);
    debugPrintln("@@ Inside setCalValues...");
    debugPrintln(buff);

    double k = -1;
    double b = -1;
    if (mjson_get_number(r->params, r->params_len, "$.k", &k) != -1)
    {
        m_oSensor.m_iK = k;
    }
    if (mjson_get_number(r->params, r->params_len, "$.b", &b) != -1)
    {
        m_oSensor.m_iB = b;
    }
    if (m_oSensor.setCalibrationValues())
    {
        jsonrpc_return_success(r, "{\"statusCode\":200,\"statusMsg\":\"Success.\"}");
        m_bSendframe = true;
    }
    else
    {
        jsonrpc_return_success(r, "{\"statusCode\":300,\"statusMsg\":\"Not Set due to communicaton error.\"}");
        m_bSendframe = true;
    }
}

/***********************************************
 *  RPC Function to set the Calibration values to the DO sensor
 *  r-> pointer holds the Item data buffer
 *************************************************/
static void RPChandler_setSalinity(struct jsonrpc_request *r)
{
    char buff[30];
    snprintf(buff, r->params_len + 1, "%S", (wchar_t *)r->params);
    debugPrintln("@@ Inside setSalinity...");
    debugPrintln(buff);

    double val = -1;
    if (mjson_get_number(r->params, r->params_len, "$.salinity", &val) != -1)
    {
        m_oSensor.m_fSalinity = val;
    }

    if (m_oSensor.setSalinity())
    {
        jsonrpc_return_success(r, "{\"statusCode\":200,\"statusMsg\":\"Success.\"}");
        m_bSendframe = true;
    }
    else
    {
        jsonrpc_return_success(r, "{\"statusCode\":300,\"statusMsg\":\"Not Set due to communicaton error.\"}");
        m_bSendframe = true;
    }
}

static void RPChandler_setOperationMode(struct jsonrpc_request *r)
{
    char buff[30];
    snprintf(buff, r->params_len + 1, "%S", (wchar_t *)r->params);
    debugPrintln("@@ Inside setOperationMode...");
    debugPrintln(buff);

    double val = -1;
    if (mjson_get_number(r->params, r->params_len, "$.operationMode", &val) != -1)
    {
        m_iOperationMode = val;
        updateConfig = true;
        jsonrpc_return_success(r, "{\"statusCode\":200,\"statusMsg\":\"Success.\"}");
        m_bSendframe = true;
    }
    else
    {
        jsonrpc_return_success(r, "{\"statusCode\":300,\"statusMsg\":\"Not Set due to communicaton error.\"}");
        m_bSendframe = true;
    }
}

static void RPChandler_setInterval(struct jsonrpc_request *r)
{
    double interval = 0;
    if (mjson_get_number(r->params, r->params_len, "$.DataFrequencyinMin", &interval))
    {
        debugPrintln("inside setInterval");
        m_iDataFrequencyInterval = interval;
        m_oMemory.putInt("interval", m_iDataFrequencyInterval);
    }
    jsonrpc_return_success(r, "{\"statusCode\":200,\"statusMsg\":\"success.\"}");
    m_bSendframe = true;
}
/**************************************************************
 *   RPC Function write data to RFID/NFC tag
 *   r-> pointer holds the config data buffer
 * 
 * method: writeTag
 * Params: {"PondLocation" : "CU8MLFk1Jfm9UCMXCAeFp"}
 ***************************************************************/
static void RPChandler_writeTag(struct jsonrpc_request *r)
{
    StaticJsonDocument<256> doc;

    // Parse JSON params
    DeserializationError error = deserializeJson(doc, r->params);
    if (error)
    {
        debugPrintln("JSON parse error");
        jsonrpc_return_success(r, "{\"statusCode\":400,\"statusMsg\":\"Invalid JSON\"}");
        return;
    }

    const char *pondLocation = doc["PondLocation"];
    if (!pondLocation || strlen(pondLocation) == 0)
    {
        debugPrintln("Missing or empty PondLocation");
        jsonrpc_return_success(r, "{\"statusCode\":401,\"statusMsg\":\"Missing PondLocation\"}");
        return;
    }

    debugPrintln("@@ Inside writeTag...");
    debugPrint("PondLocation to write: ");
    debugPrintln(pondLocation);

    int Attempts = 0;
    bool ret = false;

    while (Attempts < 5)
    {
        debugPrintln("Attempting to write data into the tag...");
        if (m_oRfid.writeDataToCard(pondLocation))
        {
            ret = true;
            debugPrintln("Successfully wrote data");
            break;
        }
        Attempts++;
    }

     bool isCardFound = m_oRfid.isTagPresent();
    if (ret)
    {
        jsonrpc_return_success(r, "{\"statusCode\":200,\"statusMsg\":\"Write successful\"}");
        showReadWriteCycle = true;
    }
    else
    {
        jsonrpc_return_success(r, "{\"statusCode\":300,\"statusMsg\":\"Write failed\"}");
    }

    m_bSendframe = true;
}


/**************************************************************
 *   RPC Function read data from RFID/NFC tag
 *   r-> pointer holds the config data buffer
 ***************************************************************/
static void RPChandler_readTag(struct jsonrpc_request *r)
{
    char rdata[512] = "";
    debugPrintln("@@ Inside readTag...");
    int Attempts = 0;
    bool ret = false;
    /*try reading the card 5 times till it read tag successfully*/
    while (Attempts < 5)
    {
        if (m_oRfid.readDataFromCard(rdata))
        {
            ret = true;
            break;
        }
        Attempts++;
    }

    debugPrintln(rdata);
    if (ret)
    {
        jsonrpc_return_success(r, "%s", (const char *)rdata);
    }
    else
    {
        jsonrpc_return_success(r, "{\"statusCode\":300,\"statusMsg\":\"fail.\"}");
    }
    m_bSendframe = true;
}

static void RPChandler_clearBoundaries(struct jsonrpc_request *r)
{
    char buff[500];
    snprintf(buff, r->params_len + 1, "%S", (wchar_t *)r->params);
    m_oFileSystem.writeFile(LOCATION_VERSIONS, buff);
    jsonrpc_return_success(r, "{\"statusCode\":200,\"statusMsg\":\"success.\"}");
    m_bSendframe = true;
}

static void RPChandler_getSalinity(struct jsonrpc_request *r)
{
    char buff[30];
    snprintf(buff, r->params_len + 1, "%S", (wchar_t *)r->params);
    debugPrintln("@@ Inside getSalinity...");
    debugPrintln(buff);

    m_oSensor.getSalinity();
    float salinity = m_oSensor.m_fSalinity; // Assuming m_oSensor.m_fSalinity is defined and holds the salinity value

    jsonrpc_return_success(r, "{\"statusCode\":200,\"statusMsg\":\"Success.\",\"salinity\":\"%s.\"}", String(salinity));
    m_bSendframe = true;
}

static void RPChandler_getPressure(struct jsonrpc_request *r)
{
    char buff[30];
    snprintf(buff, r->params_len + 1, "%S", (wchar_t *)r->params);
    debugPrintln("@@ Inside getPressure...");
    debugPrintln(buff);

    m_oSensor.getPressure();
    float pressure = m_oSensor.m_fPressure; // Assuming m_oSensor.m_fSalinity is defined and holds the salinity value

    jsonrpc_return_success(r, "{\"statusCode\":200,\"statusMsg\":\"Success.\",\"pressure\":\"%s.\"}", String(pressure));

    m_bSendframe = true;
}

/***********************************************
 *  RPC Function to set the Calibration values to the DO sensor
 *  r-> pointer holds the Item data buffer
 *************************************************/
static void RPChandler_setPressure(struct jsonrpc_request *r)
{
    char buff[30];
    snprintf(buff, r->params_len + 1, "%S", (wchar_t *)r->params);
    debugPrintln("@@ Inside setPressure...");
    debugPrintln(buff);

    double val = -1;
    if (mjson_get_number(r->params, r->params_len, "$.pressure", &val) != -1)
    {
        m_oSensor.m_fPressure = val;
    }

    if (m_oSensor.setPressure())
    {
        jsonrpc_return_success(r, "{\"statusCode\":200,\"statusMsg\":\"Success.\"}");
        m_bSendframe = true;
    }
    else
    {
        jsonrpc_return_success(r, "{\"statusCode\":300,\"statusMsg\":\"Not Set due to communicaton error.\"}");
        m_bSendframe = true;
    }
}

/***********************************************
 *  RPC Function to set the Calibration values to the DO sensor
 *  r-> pointer holds the Item data buffer
 *************************************************/
static void RPChandler_getCalValues(struct jsonrpc_request *r)
{
    StaticJsonDocument<100> doc;
    debugPrintln("@@ Inside getCalValues...");

    m_oSensor.getCalibrationValues();
    doc["k"] = m_oSensor.m_iK;
    doc["b"] = m_oSensor.m_iB;

    char result[100];
    serializeJson(doc, result);
    debugPrintln(result);
    jsonrpc_return_success(r, "%s", result);
    m_bSendframe = true;
}
/*****************************************************************
 * RPC Function to set device in safe mode
 * r-> pointer holds the config data buffer
 * @param [in] None
 * @param [out] Status code with status msg
 ******************************************************************/
static void RPChandler_runSafeMode(struct jsonrpc_request *r)
{
    debugPrintln("@@ Inside runSafeMode.....");
    m_oConfig.m_bIsSafeModeOn = true;
    jsonrpc_return_success(r, "{\"statusCode\":200,\"statusMsg\":\"success.\"}");
    m_bSendframe = true;
}

/**************************************************************
 * RPC Function to check device info
 * r-> pointer holds the config data buffer
 * @param [in] None
 * @param [out] device Info in Json format
 ***************************************************************/
static void RPChandler_whoAreYou(struct jsonrpc_request *r)
{
    // debugPrintln("@@ Inside whoAreYou.....");
    StaticJsonDocument<700> doc;
    doc["deviceId"] = WiFi.macAddress();
    doc["localIp"] = WiFi.localIP();
    doc["fwVersn"] = FW_VERSION;
    doc["BoardVersion"] = BOARD_VERSION;
    doc["currentEpoch"] = m_oConfig.m_tEpoch;
    doc["ResetReason"] = m_oConfig.espResetReason;
    doc["FramesInBackup"] = m_oDisp.DisplayGeneralVariables.backUpFramesCnt; 
    doc["Wifissid"] = WiFi.SSID();
    doc["rssi"] = WiFi.RSSI();
    doc["lat"] = m_oGps.mPosition.m_lat;
    doc["lng"] = m_oGps.mPosition.m_lng;
    doc["HDop"] = m_oGps.mPosition.hDop;
    doc["Satellite"] = m_oGps.mPosition.m_iSatellites;
    doc["CurrentPondName"] = CurrntPondName;
    doc["DoMg/l"] = DoMglValue;
    doc["Temp"] = TempVal;
    doc["Saturation"] = DoSaturationVal;
    doc["Salinity"] = CurrentPondSalinity;
    doc["localOffsetTimeMin"] = TotalMinsOffSet;
    doc["operationMode"] = m_iOperationMode;
    doc["progress"] = m_oHttp.currprogress;

    char result[700];
    serializeJson(doc, result);
    // debugPrintln(result);
    jsonrpc_return_success(r, "%s", result);
    m_bSendframe = true;
}

/**************************************************************
 *  Sync RTC
 ***************************************************************/
static void RPChandler_syncRTC(struct jsonrpc_request *r)
{
    RTCSyncNow = true;
    jsonrpc_return_success(r, "{\"statusCode\":200,\"statusMsg\":\"success.\"}");
    m_bSendframe = true;
}
/**************************************************************
 *  RPC to get the CurrentPond
 ***************************************************************/
static void RPChandler_getCurrentPondName(struct jsonrpc_request *r)
{
    debugPrintln(" get pond details to be activated");
    m_bGetPondLocation = true;
    m_bGotPondName = false;
    jsonrpc_return_success(r, "{\"statusCode\":200,\"statusMsg\":\"success.\"}");
    m_bSendframe = true;
}

/**************************************************************
 *  RPC to get the CurrentPond
 ***************************************************************/
static void RPChandler_ResetPondStatus(struct jsonrpc_request *r)
{
    debugPrintln("Resetting pond status ");
    ResetEntireMap = true;
    jsonrpc_return_success(r, "{\"statusCode\":200,\"statusMsg\":\"success.\"}");
    m_bSendframe = true;
}
/******************************************************************************
 * Set Pond Map reset time to clear the frame saved status on the UI
 {
    "MorningResetTime":2,
    "EveningResetTime":16
 }
*****************************************************************************/
static void RPChandler_SetPondMapResetTime(struct jsonrpc_request *r)
{
    debugPrintln("[App][RPC]:Set pond Map reset time ");
    char buff[200];
    snprintf(buff, r->params_len + 1, "%S", (wchar_t *)r->params);
    Serial.println(buff);

    double a = -1, b = -1;

    mjson_get_number(r->params, r->params_len, "$.MorningResetTime", &a);
    mjson_get_number(r->params, r->params_len, "$.EveningResetTime", &b);
    
    debugPrintf("Morning Reset Time in hours: %f", a);
    debugPrintf("Evening Reset Time in hours: %f", b);

    MorningPondMapResetTime = ((int)a);
    EveningPondMapResetTime = ((int)b);

    Serial.println("[RPC][SetPondMapReset]: type conversion done");

    m_oMemory.putInt("MrngTime", MorningPondMapResetTime);
    m_oMemory.putInt("EvngTime", EveningPondMapResetTime);

    Serial.println("[RPC][SetPondMapReset]: saving to NVS done");

    jsonrpc_return_success(r, "{\"MorningTime\":\"%d\",\"Evening Time\":\"%d\",\"statusCode\":200,\"statusMsg\":\"Success.\"}", MorningPondMapResetTime, EveningPondMapResetTime);
    m_bSendframe = true;
}
/**********************************************************
* RPC Function to operate Slot
* r-> pointer holds the config data buffer
* @param [in] Data:
    {
        "version": 1699333718,
        "LocalMin":180
    }
* @param [out] Status code with status msg
***********************************************************/
static void RPChandler_setLocalTimeOffset(struct jsonrpc_request *r)
{
    debugPrintln("@@ Inside setLocalTimeOffset.....");
    char buff[124];
    snprintf(buff, r->params_len + 1, "%S", (wchar_t *)r->params);
    debugPrintln(buff);
    static double preVersn = 0;
    static double Versn = 0;
    mjson_get_number(r->params, r->params_len, "$.version", &Versn);
    if (Versn != preVersn) /*check command version to avoid repeated commands with same versionthe file*/
    {
        debugPrintln(Versn);
        preVersn = Versn;
        double localTimeMin = -1;

        mjson_get_number(r->params, r->params_len, "$.LocalMin", &localTimeMin);
        debugPrint("LOcal time in Mins: ");debugPrintln(localTimeMin);
        TotalMinsOffSet = ((int)localTimeMin);
        

        m_oMemory.putInt("LocalMins", TotalMinsOffSet);
        
        jsonrpc_return_success(r, "{\"statusCode\":200,\"statusMsg\":\"success.\", \"localTimeMin\":%d}", TotalMinsOffSet);
        m_bSendframe = true;
    }
    else
    {
        debugPrintln("Invalid version...");
        jsonrpc_return_success(r, "{\"statusCode\":300,\"statusMsg\":\"Invalid Version.\"}");
        m_bSendframe = true;
    }
}

/**********************************************************
 * RPC Function to update routines file in feeder
 * r-> pointer holds the config data buffer
 * @param [in] None
 * @param [out] Status code with status msg
 ***********************************************************/
static void RPChandler_updateRoutine(struct jsonrpc_request *r)
{
    debugPrintln("@@ Inside updateRoutines.....");
    m_bGetRoutine = false;
    jsonrpc_return_success(r, "{\"statusCode\":200,\"statusMsg\":\"Success.\"}");
    m_bSendframe = true;
}

/**********************************************************
 * RPC Function to update configIds file in feeder
 * r-> pointer holds the config data buffer
 * @param [in] None
 * @param [out] Status code with status msg
 ***********************************************************/
static void RPChandler_updateConfig(struct jsonrpc_request *r)
{
    debugPrintln("@@ Inside updateConfigIds.....");
    m_bGetConfig = true;
    jsonrpc_return_success(r, "{\"statusCode\":200,\"statusMsg\":\"Success.\"}");
    m_bSendframe = true;
}

/**********************************************************
 * RPC Function to get configIDS file from stater
 * r-> pointer holds the config data buffer
 * @param [in] None
 * @param [out] configIDs file from filesystem on succerss
 ***********************************************************/
static void RPChandler_getConfigIDs(struct jsonrpc_request *r)
{
    int FileSize = m_oFileSystem.getFileSize(FILENAME_IDSCONFIG);
    char rdata[FileSize];
    debugPrintln("@@ Inside getConfigIds.....");
    int ret = m_oFileSystem.readFile(FILENAME_IDSCONFIG, rdata);
    if (ret)
    {
        jsonrpc_return_success(r, "%s", (const char *)rdata);
        m_bSendframe = true;
    }
    else
    {
        jsonrpc_return_success(r, "{\"statusCode\":300,\"statusMsg\":\"file Not available.\"}");
        m_bSendframe = true;
    }
}

/**********************************************************
 * RPC Function to get configIDS file from stater
 * r-> pointer holds the config data buffer
 * @param [in] None
 * @param [out] configIDs file from filesystem on succerss
 ***********************************************************/
static void RPChandler_getPondConfig(struct jsonrpc_request *r)
{
    char rdata[1500];
    debugPrintln("@@ Inside getPondConfig.....");
    int ret = m_oFileSystem.readFile(FILENAME_DEVICECONFIG, rdata);
    if (ret)
    {
        jsonrpc_return_success(r, "%s", (const char *)rdata);
        m_bSendframe = true;
    }
    else
    {
        jsonrpc_return_success(r, "{\"statusCode\":300,\"statusMsg\":\"file Not available.\"}");
        m_bSendframe = true;
    }
}

static void RPCHandler_setPondConfig(struct jsonrpc_request *r)
{
    char buff[500];
    snprintf(buff, r->params_len + 1, "%S", (wchar_t *)r->params);
    debugPrintln("@@ Inside setPondConfig...");
    debugPrintln(buff);

    if (m_iOperationMode == CONTINUOUS_BASED_MODE)
    {
        char pondid[50];
        if (mjson_get_string(r->params, r->params_len, "$.pondId", pondid, sizeof(pondid)) != -1)
        {
            safeStrcpy(m_oPond.m_u64currentPondId, pondid, sizeof(m_oPond.m_u64currentPondId));
        }
        else
        {
            jsonrpc_return_success(r, "{\"statusCode\":300,\"statusMsg\":\"need pondId.\"}");
            m_bSendframe = true;
            return;
        }

        char locationid[50];
        if (mjson_get_string(r->params, r->params_len, "$.locationId", locationid, sizeof(locationid)) != -1)
        {
            safeStrcpy(m_oPond.m_cLocationId, locationid, sizeof(m_oPond.m_cLocationId));
        }
        else
        {
            jsonrpc_return_success(r, "{\"statusCode\":300,\"statusMsg\":\"need locationId.\"}");
            m_bSendframe = true;
            return;
        }

        updateConfig = true;
    }
    else
    {
        jsonrpc_return_success(r, "{\"statusCode\":300,\"statusMsg\":\"device not in Continous mode.\"}");
        m_bSendframe = true;
        return;
    }

    jsonrpc_return_success(r, "{\"statusCode\":200,\"statusMsg\":\"Success.\"}");
    m_bSendframe = true;
}


/**************************************************************
 *   RPC Function to clear Backup files
 *   r-> pointer holds the config data buffer
 ***************************************************************/
static void RPChandler_ClearBackupFiles(struct jsonrpc_request *r)
{
    m_oBackupStore.clearAllFiles(&m_oFileSystem);
    jsonrpc_return_success(r, "{\"statusCode\":200,\"statusMsg\":\"success.\"}");
    m_bSendframe = true;
}


/**********************************************************
 * RPC Function to update firmware using elagent ota
 * r-> pointer holds the config data buffer
 * @param [in] None
 * @param [out] Status code with status msg
 ***********************************************************/
static void RPChandler_firmwareUpdate(struct jsonrpc_request *r)
{
    debugPrintln("@@ Inside firmwareUpdate.....");
    char buff[200];
    snprintf(buff, r->params_len + 1, "%S", (wchar_t *)r->params);
    debugPrintln(buff);
    static double preVersn = 0;
    static double Versn = 0;
    mjson_get_number(r->params, r->params_len, "$.version", &Versn);
    if (Versn != preVersn)
    {
        preVersn = Versn;
        char Id[250] = "";
        if (mjson_get_string(r->params, r->params_len, "$.firmwareUrl", Id, sizeof(Id)) != -1)
        {
            m_bDoFota = true;
            m_oHttp.currprogress = 0;
            safeStrcpy(m_oHttp.m_cUriFirmwareFOTA, Id, sizeof(m_oHttp.m_cUriFirmwareFOTA));
            jsonrpc_return_success(r, "{\"statusCode\":200,\"statusMsg\":\"Firmware URL update success\"}");
        }
        else
        {
            jsonrpc_return_success(r, "{\"statusCode\":250,\"statusMsg\":\"Invalid URL\"}");
        }
    }
    else
    {
        jsonrpc_return_success(r, "{\"statusCode\":300,\"statusMsg\":\"Invalid Version number\"}");
    }
    m_bSendframe = true;
}

/**************************************************************
 * RPC Function to get CALL_FRAME
 * r-> pointer holds the config data buffer
 * @param [in] None
 * @param [out] Status code with status msg
 ***************************************************************/
static void RPChandler_refreshFrame(struct jsonrpc_request *r)
{
    debugPrintln("@@ Inside refreshFrame.....");
    sendFrameType = CALL_FRAME;
    jsonrpc_return_success(r, "{\"statusCode\":200,\"statusMsg\":\"Live frame sent success\"}");
    m_bSendframe = true;
}

static void RPChandler_sysReboot(struct jsonrpc_request *r)
{
    debugPrintln("@@ Inside sysReboot.....");
    rebootAfterSetDataCmd = 100;
    jsonrpc_return_success(r, "{\"statusCode\":200,\"statusMsg\":\"Live frame sent success\"}");
    m_bSendframe = true;
}
/**********************************************************
 * RPC Function to set Wifi Credentials
 * r-> pointer holds the config data buffer
 * @param [in] Data:
    {
        "version": 1707910462,
        "wifiSsid": "Nextaqua_EAP110_1",
        "wifiPass": "Infi@2016"
    }
 * @param [out] Status code with status msg
 ***********************************************************/
static void RPChandler_setWifiCredentials(struct jsonrpc_request *r)
{
    debugPrintln("@@ Inside setWifiCredentials.....");
    char buff[124];
    snprintf(buff, r->params_len + 1, "%S", (wchar_t *)r->params);
    debugPrintln(buff);
    static double preVersn = 0;
    static double Versn = 0;
    mjson_get_number(r->params, r->params_len, "$.version", &Versn);
    if (Versn != preVersn) /*check command version to avoid repeated commands with same versionthe file*/
    {
        debugPrintln(Versn);
        preVersn = Versn;
        char wifiSsid[25];
        char wifiPass[25];
        bool isUpdate = false;
        if (mjson_get_string(r->params, r->params_len, "$.wifiSsid", wifiSsid, sizeof(wifiSsid)) != -1)
        {
            safeStrcpy(m_cWifiSsid, wifiSsid, sizeof(m_cWifiSsid));
            isUpdate = true;
        }
        if (mjson_get_string(r->params, r->params_len, "$.wifiPass", wifiPass, sizeof(wifiPass)) != -1)
        {
            safeStrcpy(m_cWifiPass, wifiPass, sizeof(m_cWifiPass));
            isUpdate = true;
        }

        if (isUpdate)
        {
            debugPrintln("@@ saving wifi credentials");
            m_oMemory.putString("wifiSsid", m_cWifiSsid);
            m_oMemory.putString("wifiPass", m_cWifiPass);
            rebootAfterSetDataCmd = 100;
        }
        jsonrpc_return_success(r, "{\"wifiSsid\":\"%s\",\"wifiPass\":\"%s\",\"statusCode\":200,\"statusMsg\":\"Success.\"}", m_cWifiSsid, m_cWifiPass);
        m_bSendframe = true;
    }
    else
    {
        debugPrintln("Invalid version...");
        jsonrpc_return_success(r, "{\"wifiSsid\":\"%s\",\"wifiPass\":\"%s\",\"statusCode\":300,\"statusMsg\":\"Invalid Version.\"}", m_cWifiSsid, m_cWifiPass);
        m_bSendframe = true;
    }
}

/**********************************************************
 * RPC Function to server credentials like ip and port
 * r-> pointer holds the config data buffer
 * @param [in] Data:
    {
        "version": 1707830397,
        "IP": "34.93.69.40"
    }
 * @param [out] Status code with status msg
 ***********************************************************/
static void RPChandler_setServerCredntials(struct jsonrpc_request *r)
{
    debugPrintln("@@ Inside setServerCredntials.....");
    char buff[124];
    snprintf(buff, r->params_len + 1, "%S", (wchar_t *)r->params);
    debugPrintln(buff);
    static double preVersn = 0;
    static double Versn = 0;
    mjson_get_number(r->params, r->params_len, "$.version", &Versn);
    if (Versn != preVersn) /*check command version to avoid repeated commands with same versionthe file*/
    {
        debugPrintln(Versn);
        preVersn = Versn;
        char IP[20];
        if (mjson_get_string(r->params, r->params_len, "$.IP", IP, sizeof(IP)) != -1)
        {
            safeStrcpy(m_oHttp.m_cServerIP, IP, sizeof(m_oHttp.m_cServerIP));
            debugPrintln("@@ saving server details");
            m_oMemory.putString("serverIP", m_oHttp.m_cServerIP);
            rebootAfterSetDataCmd = 100;
        }
        jsonrpc_return_success(r, "{\"IP\":\"%s\",\"port\":\"%d\",\"statusCode\":200,\"statusMsg\":\"Success.\"}", m_oHttp.m_cServerIP, m_oHttp.m_u16ServerPort);
        m_bSendframe = true;
    }
    else
    {
        debugPrintln("Invalid version...");
        jsonrpc_return_success(r, "{\"IP\":\"%s\",\"port\":\"%d\",\"statusCode\":300,\"statusMsg\":\"Invalid Version.\"}", m_oHttp.m_cServerIP, m_oHttp.m_u16ServerPort);
        m_bSendframe = true;
    }
}
/**************************************************************
 *   RPC Function to clear files except the Backup files
 *   r-> pointer holds the config data buffer
 ***************************************************************/
static void RPChandler_ClearNonBackupFiles(struct jsonrpc_request *r)
{   
    m_oBackupStore.clearNonBackupFiles(&m_oFileSystem);
    ResetEntireMap = true;
    jsonrpc_return_success(r, "{\"statusCode\":200,\"statusMsg\":\"success.\"}");
    m_bSendframe = true;
}

/**********************************************************
 * RPC Function to server credentials like ip and port
 * r-> pointer holds the config data buffer
 * @param [in] Data:
    {
        "Epoch":179525211,
        "Lat": -2.553424,
        "Long": -80.565472
    }
 * @param [out] Status code with status msg
 ***********************************************************/
static void RPChandler_SimulatedPosts(struct jsonrpc_request *r)
{
    char buff[150];
    snprintf(buff, r->params_len + 1, "%S", (wchar_t *)r->params);
    debugPrintln("@@ Inside SimulatedPosts...");
    debugPrintln(buff);

    double e = 0;
    mjson_get_number(r->params, r->params_len, "$.Epoch", &e);
    m_oConfig.m_tEpoch = e;

    double val = -1, val2 = -1;
    if (mjson_get_number(r->params, r->params_len, "$.Lat", &val) != -1)
    {
        SimulatedLat = val;
    }
    if (mjson_get_number(r->params, r->params_len, "$.Long", &val2) != -1)
    {
        SimulatedLongs = val2;
    }
    Is_Simulated_Lat_Longs = true;
    jsonrpc_return_success(r, "{\"statusCode\":200,\"statusMsg\":\"Success.\"}");
    m_bSendframe = true;
}
/****************************************************************************************
 * Function to intialize rpc Function Handlers and mdash begin with wifi
 * @param [in] None
 * @param [out] None
 ***************************************************************************************/
void cApplication::wifiInitialization(void)
{
    init_wifi(m_cWifiSsid, m_cWifiPass);
    /*Expose RCPs for OTA and Activate safe mode*/
    jsonrpc_export("ActivateSafeMode", RPChandler_runSafeMode);
    jsonrpc_export("FOTA", RPChandler_firmwareUpdate);
}

void allRpcInit(void)
{
    jsonrpc_export("wifiConfig", RPChandler_setWifiCredentials);
    jsonrpc_export("serverConfig", RPChandler_setServerCredntials);
    jsonrpc_export("getSalinity", RPChandler_getSalinity);
    jsonrpc_export("setOperationMode", RPChandler_setOperationMode);
    jsonrpc_export("getPressure", RPChandler_getPressure);
    jsonrpc_export("setLocalTimeOffset", RPChandler_setLocalTimeOffset);
    jsonrpc_export("ClearBackupFiles", RPChandler_ClearBackupFiles);
    jsonrpc_export("syncRTC", RPChandler_syncRTC);
    jsonrpc_export("whoAreYou", RPChandler_whoAreYou);
    jsonrpc_export("setCalValues", RPChandler_setCalValues);
    jsonrpc_export("setSalinity", RPChandler_setSalinity);
    jsonrpc_export("getCalValues", RPChandler_getCalValues);
    jsonrpc_export("setPressure", RPChandler_setPressure);
    jsonrpc_export("getConfig", RPChandler_getConfigIDs);
    jsonrpc_export("getPondConfig", RPChandler_getPondConfig);
    jsonrpc_export("setPondConfig", RPCHandler_setPondConfig);
    jsonrpc_export("updateRoutine", RPChandler_updateRoutine);
    jsonrpc_export("updateConfig", RPChandler_updateConfig);
    jsonrpc_export("FOTA", RPChandler_firmwareUpdate);
    jsonrpc_export("getLiveFrame", RPChandler_refreshFrame);
    jsonrpc_export("sysReboot", RPChandler_sysReboot);
    jsonrpc_export("setDataFrequency", RPChandler_setInterval);
    jsonrpc_export("clearBoundaries", RPChandler_clearBoundaries);
    jsonrpc_export("writeTag", RPChandler_writeTag);
    jsonrpc_export("readTag", RPChandler_readTag);
    jsonrpc_export("CurrentPond", RPChandler_getCurrentPondName);
    jsonrpc_export("ResetPondStatus", RPChandler_ResetPondStatus);
    jsonrpc_export("SetPondMapResetTime", RPChandler_SetPondMapResetTime);
    jsonrpc_export("ClearNonBackupFiles", RPChandler_ClearNonBackupFiles);
    jsonrpc_export("SimulatePosts", RPChandler_SimulatedPosts);
}

static int sender(const char *frame, int frame_len, void *privdata)
{
    strncat(sendResult, frame, frame_len);
    return 1;
}
/*Process RPC and call the function*/
void processRPC(char *jsonString)
{
    // debugPrint("Received JSON: ");
    // debugPrintln(jsonString);
    // Parse the outer array using ArduinoJson
    DynamicJsonDocument outerDoc(2500); // Adjust the size according to your needs
    DeserializationError outerError = deserializeJson(outerDoc, jsonString);
    if (outerError == DeserializationError::Ok)
    {
        // Access the second element (inner JSON string)
        JsonObject innerJsonString = outerDoc[1];

        String serializedRpcObject;
        serializeJson(innerJsonString, serializedRpcObject);

        // debugPrint("Complete RPC Object: ");
        // debugPrintln(serializedRpcObject);

        jsonrpc_process(serializedRpcObject.c_str(), serializedRpcObject.length(), sender, NULL, NULL);
    }
    else
    {
        // Failed to parse the outer array
        debugPrint("Error: Unable to parse the Outer JSON (");
        debugPrint(outerError.c_str());
        debugPrintln(").");
    }
}
void socketIOEvent(const socketIOmessageType_t &type, uint8_t *payload, const size_t &length)
{
    switch (type)
    {
    case sIOtype_DISCONNECT:
        debugPrintln("[IOc] Disconnected");
        m_oDisp.DisplayFooterData.isWebScoketsConnected  = false;
        break;

    case sIOtype_CONNECT:
        debugPrint("[IOc] Connected to url: ");
        debugPrintln((char *)payload);
        // join default namespace (no auto join in Socket.IO V3)
        socketIO.send(sIOtype_CONNECT, "/");
        m_oDisp.DisplayFooterData.isWebScoketsConnected  = true;
        break;

    case sIOtype_EVENT:
        debugPrint("[IOc] Get event: ");
        debugPrintln((char *)payload);
        processRPC((char *)payload);
        break;

    case sIOtype_ACK:
        debugPrint("[IOc] Get ack: ");
        debugPrintln(length);

        // hexdump(payload, length);

        break;
    case sIOtype_ERROR:
        debugPrint("[IOc] Get error: ");
        debugPrintln(length);

        // hexdump(payload, length);

        break;

    case sIOtype_BINARY_EVENT:
        debugPrint("[IOc] Get binary: ");
        debugPrintln(length);

        // hexdump(payload, length);

        break;

    case sIOtype_BINARY_ACK:
        debugPrint("[IOc] Get binary ack: ");
        debugPrintln(length);

        // hexdump(payload, length);

        break;

    case sIOtype_PING:
        Serial.println("[IOc] Get PING");

        break;

    case sIOtype_PONG:
        Serial.println("[IOc] Get PONG");

        break;

    default:
        break;
    }
}

/*****************************************************************************
 *   Function to intialize internal watchdog
 *   taskhandler -> Indicates for which hamdler wdt should be initialized
 *****************************************************************************/
void cApplication::AppWatchdogInit(TaskHandle_t *taskhandle1, TaskHandle_t *taskhandle2)
{
    m_oBsp.wdtInit();
    m_oBsp.wdtAdd(*taskhandle1);
    m_oBsp.wdtAdd(*taskhandle2);
}

void cApplication::AppWatchdogInit(TaskHandle_t *taskhandle1, TaskHandle_t *taskhandle2, TaskHandle_t *taskhandle3)
{
    m_oBsp.wdtInit();
    m_oBsp.wdtAdd(*taskhandle1);
    m_oBsp.wdtAdd(*taskhandle2);
    m_oBsp.wdtAdd(*taskhandle3);
}

void cApplication::AppWatchdogInit(TaskHandle_t *taskhandle1, TaskHandle_t *taskhandle2, TaskHandle_t *taskhandle3, TaskHandle_t *taskhandle4)
{
    m_oBsp.wdtInit();
    m_oBsp.wdtAdd(*taskhandle1);
    m_oBsp.wdtAdd(*taskhandle2);
    m_oBsp.wdtAdd(*taskhandle3);
    m_oBsp.wdtAdd(*taskhandle4);
}

void cApplication::AppWatchdogInit(TaskHandle_t *taskhandle1, TaskHandle_t *taskhandle2, TaskHandle_t *taskhandle3, TaskHandle_t *taskhandle4, TaskHandle_t *taskhandle5)
{
    m_oBsp.wdtInit();
    m_oBsp.wdtAdd(*taskhandle1);
    m_oBsp.wdtAdd(*taskhandle2);
    m_oBsp.wdtAdd(*taskhandle3);
    m_oBsp.wdtAdd(*taskhandle4);
    m_oBsp.wdtAdd(*taskhandle5);
}

/*Function to reset the wifi credentials*/
void cApplication::ResetWifiCredentials(void)
{
    /*clear wifi settings*/
    debugPrintln("wifi settings cleared");
    m_oMemory.putString("wifiSsid", "Nextaqua_EAP110");
    m_oMemory.putString("wifiPass", "Infi@2016");
    delay(1000);
    ESP.restart();
}
/*Function to reset the server credentials*/

void cApplication::ResetServerCredentials(void)
{
    /*clear server settings*/
    debugPrintln("server settings cleared");
    m_oMemory.putString("serverIP", m_oHttp.m_cDefaultServerIP);
    delay(1000);
    ESP.restart();
}
void IRAM_ATTR handleButtonInterrupt() {
  static bool lastPhysicalState = HIGH;
  bool currentPhysicalState = digitalRead(BSP_BTN_1);

  if (currentPhysicalState != lastPhysicalState) {
    unsigned long now = millis();

    if (currentPhysicalState == LOW) {
      ButtonState.buttonReleased = false;
      ButtonState.buttonPressedMillis = now;
    } else {
      ButtonState.buttonReleased = true;
      ButtonState.buttonChanged = true;  // Signal main loop to evaluate
    }
    lastPhysicalState = currentPhysicalState;
  }
}

// void cApplication::CheckForButtonEvent() {
//     static bool isButtonPressed = false;
//     if(isButtonPressed)
//     {
//         CountDownTimer = (millis() - ButtonPressedMillis) / 1000;
//         m_oDisp.DisplayGeneralVariables.Counter = CountDownTimer;
//     }
//     debugPrintf(" CoutDown: %d \n", CountDownTimer);
//     if(CountDownTimer >= TIMER_COUNTDOWN) {
//         CountDownTimer = 0;
//         isButtonPressed = false;
//         buzz = 10;
//         sendFrameType = VDIFF_FRAME;
//         m_oDisp.PopUpDisplayData.UploadStatus = NO_FRAME_IN_PROCESS;
//         debugPrintln("Generated Frame");
//     }

//     if (!ButtonState.buttonChanged || !ButtonState.buttonReleased)
//         return;

//     ButtonState.buttonChanged = false;

//     unsigned long now = millis();
//     unsigned long pressDuration = now - ButtonState.buttonPressedMillis;

//     if (pressDuration < 2000) {
//         lastButtonEvent = JUST_PRESSED;
//     } else if (pressDuration > 2000) {
//         lastButtonEvent = SHORT_PRESS_DETECTED;
//     }
//     else {
//         lastButtonEvent = BUTTON_NONE;
//     }

//     debugPrint("Button Press Duration: ");
//     debugPrint(pressDuration);
//     debugPrint(" ms → Event: ");
//     switch (lastButtonEvent) {
//         case JUST_PRESSED: debugPrintln("JUST_PRESSED"); break;
//         case SHORT_PRESS_DETECTED: debugPrintln("SHORT_PRESS_DETECTED"); break;
//         default: debugPrintln("BUTTON_NONE"); break;
//     }
//     // Assign event to display system if needed
//     m_oDisp.DisplayGeneralVariables.lastButtonEvent = lastButtonEvent;

//     if(currentScreen == 1 && lastButtonEvent == JUST_PRESSED && !isButtonPressed)
//     {
//         isButtonPressed = true;
//         buzz = 5;
//         debugPrintln(" Button Pressed CountDown Start");
//         m_oDisp.PopUpDisplayData.UploadStatus = 8;
//         ButtonPressedMillis = millis();
//     }
// }

void cApplication::CheckForButtonEvent() {
    static bool isButtonPressed = false;

    unsigned long now = millis();

    // Countdown handler
    if (isButtonPressed) {
        CountDownTimer = (now - ButtonPressedMillis) / 1000;
        m_oDisp.DisplayGeneralVariables.Counter = CountDownTimer;
    }
    // debugPrintf(" CountDown: %d \n", CountDownTimer);

    if (CountDownTimer >= TIMER_COUNTDOWN) {
        CountDownTimer = 0;
        isButtonPressed = false;
        buzz = 10;
        sendFrameType = VDIFF_FRAME;
        m_oDisp.PopUpDisplayData.UploadStatus = NO_FRAME_IN_PROCESS;
        debugPrintln("Generated Frame");
    }

    // --- Detect SHORT_PRESS while holding (no release required) ---
    if (!ButtonState.buttonReleased) {   // button still pressed
        unsigned long heldTime = now - ButtonState.buttonPressedMillis;
        if (heldTime > 2000 && lastButtonEvent != SHORT_PRESS_DETECTED) {
            lastButtonEvent = SHORT_PRESS_DETECTED;
            debugPrintln("SHORT_PRESS_DETECTED (while holding)");
            m_oDisp.DisplayGeneralVariables.lastButtonEvent = lastButtonEvent;
        }
    }

    // --- Detect JUST_PRESSED on release ---
    if (ButtonState.buttonChanged && ButtonState.buttonReleased) {
        ButtonState.buttonChanged = false;

        unsigned long pressDuration = now - ButtonState.buttonPressedMillis;

        if (pressDuration < 2000) {
            lastButtonEvent = JUST_PRESSED;
            debugPrintln("JUST_PRESSED (on release)");
        } else {
            lastButtonEvent = BUTTON_NONE; // already handled as SHORT_PRESS while holding
        }

        m_oDisp.DisplayGeneralVariables.lastButtonEvent = lastButtonEvent;

        // Start countdown only for JUST_PRESSED
        if (currentScreen == 1 && lastButtonEvent == JUST_PRESSED && !isButtonPressed) {
            isButtonPressed = true;
            buzz = 5;
            debugPrintln(" Button Pressed CountDown Start");
            m_oDisp.PopUpDisplayData.UploadStatus = 8;
            ButtonPressedMillis = now;
        }
    }
}


void cApplication::operateBuzzer(void)
{
    /*Operate Buzzer*/
    if (buzz > 0)
    {
        m_oBsp.hooterOn();
        buzz--;
    }
    else
    {
        m_oBsp.hooterOff();
        m_bButtonPressed = false;
    }
}

void cApplication::inActivityChecker(void)
{
    /*if there is no activty of button press for 30 min go to sleep*/
    if (iNoActivityCounter >= 18000)
    {
        debugPrintln("@@@ Going to sleep Mode.......ZZZZZZZZZZZZZZ");

        esp_deep_sleep_start();
    }
}

/************************************************
Method to print the reason by which ESP32
has been awaken from sleep
*************************************************/
void cApplication::print_wakeup_reason()
{
    esp_sleep_wakeup_cause_t wakeup_reason;
    /*store the wakeup reason from deep sleep*/
    wakeup_reason = esp_sleep_get_wakeup_cause();
    switch (wakeup_reason)
    {
    case ESP_SLEEP_WAKEUP_EXT0:
        Serial.println("Wakeup caused by external signal using RTC_IO");
        break;
    case ESP_SLEEP_WAKEUP_EXT1:
        Serial.println("Wakeup caused by external signal using RTC_CNTL");
        break;
    case ESP_SLEEP_WAKEUP_TIMER:
        Serial.println("Wakeup caused by timer");
        break;
    case ESP_SLEEP_WAKEUP_TOUCHPAD:
        Serial.println("Wakeup caused by touchpad");
        break;
    case ESP_SLEEP_WAKEUP_ULP:
        Serial.println("Wakeup caused by ULP program");
        break;
    default:
        Serial.printf("Wakeup was not caused by deep sleep: %d\n", wakeup_reason);
        break;
    }
}

void cApplication::print_restart_reason()
{
    /*Reading esp reset reason*/
    m_oConfig.espResetReason = esp_reset_reason();
    switch (m_oConfig.espResetReason)
    {
    case ESP_RST_UNKNOWN:
        Serial.println("Reset reason can not be determined");
        break;
    case ESP_RST_POWERON:
        Serial.println("Reset due to power-on event");
        break;
    case ESP_RST_EXT:
        Serial.println("Reset by external pin (not applicable for ESP32)");
        break;
    case ESP_RST_SW:
        Serial.println("Software reset via esp_restart");
        break;
    case ESP_RST_PANIC:
        Serial.println("Software reset due to exception/panic");
        break;
    case ESP_RST_INT_WDT:
        Serial.println("Reset (software or hardware) due to interrupt watchdog");
        break;
    case ESP_RST_TASK_WDT:
        Serial.println("Reset due to task watchdog");
        break;
    case ESP_RST_WDT:
        Serial.println("Reset due to other watchdogs");
        break;
    case ESP_RST_DEEPSLEEP:
        Serial.println("Reset after exiting deep sleep mode");
        break;
    case ESP_RST_BROWNOUT:
        Serial.println("Brownout reset (software or hardware)");
        break;
    case ESP_RST_SDIO:
        Serial.println("Reset over SDIO");
        break;
    default:
        break;
    }
}

/*******************************************************************************
 * Send ping frame to server to check if device is connected to Nextaqua server
 *******************************************************************************/
time_t cApplication::SendPing(void)
{
    char frame[256];
    sprintf(frame, "{\"ReasonForPacket\":\"Ping\",\"siteId\":\"%s\",\"epochTime\":%ld,\"deviceId\":\"%s\"}", m_oConfig.m_cSiteId, m_oConfig.m_tEpoch, m_oConfig.m_cDeviceId);
    debugPrintln(frame);
    return m_oHttp.uploadPingFrame(frame);
}

/***********************************************************
 *   Send data from backup storage periodically if available
 ************************************************************/
void cApplication::uploadframeFromBackUp(void)
{
    if (isOnline)
    {
        if (m_oHttp.m_bHttpBusy)
        {
            debugPrintln("httpBusy :-(");
            return;
        }
        if (!m_oHttp.m_bIsConnected || pingNow)
        {
            pingNow = false;
            debugPrintln("Trying ping in BAK");
            m_tPingEpoch = SendPing();
            return;
        }
        if (m_oBackupStore.available())
        {
            char fdata[1300] = {0};
            debugPrintln("files in Backup Available..");
            m_oBackupStore.readFromBS(&m_oFileSystem, fdata);

            //Function to read pondName and timebuffer from the frame and print on display till the frame is sent  
             StaticJsonDocument<1300> doc;
             DeserializationError error = deserializeJson(doc, fdata);
            if (error)
            {
                debugPrintln("JSON parse error");
            }
            char Data[1300];
            serializeJson(doc, Data);
            debugPrint(" [upload frame from backup][fdata] :\n");
            debugPrintln(fdata);
            debugPrint(" [upload frame from backup][Data] :\n");
            debugPrintln(Data);
            if (m_oHttp.uploadDataFrame(Data)) //
            {
                m_oDisp.PopUpDisplayData.UploadStatus = BACKUP_FRAME_UPLOAD_SUCCESS;
                safeStrcpy(m_oDisp.PopUpDisplayData.time, doc["timeBuffer"], sizeof(doc["timeBuffer"]));
                safeStrcpy(m_oDisp.PopUpDisplayData.pName, doc["PondName"], sizeof(doc["PondName"]));
                m_oDisp.PopUpDisplayData.doValue = doc["do"];
                debugPrintln("[App][uploadFrameFromBackup][uploadDataFrame][Success]");
                m_oBackupStore.moveToNextFile(&m_oFileSystem);
                
                if(doc["DataError"] != PONDMAP_VALUE_TAKEN_BUT_ERROR)
                {
                    m_oPond.m_pondStatusMap[doc["PondName"]].Backup = PONDMAP_VALUE_FRAME_SENT_SUCESSFULLY;
                    m_oPond.savePondStatusToFile();
                }
            }
            else 
            {   
                debugPrintln("[App][uploadFrameFromBackup][uploadDataFrame][Fail]");
                m_oDisp.PopUpDisplayData.UploadStatus = BACKUP_FRAME_UPLOAD_FAIL;
                safeStrcpy(m_oDisp.PopUpDisplayData.time, doc["timeBuffer"], sizeof(doc["timeBuffer"]));
                safeStrcpy(m_oDisp.PopUpDisplayData.pName, doc["PondName"], sizeof(doc["PondName"]));
                m_oDisp.PopUpDisplayData.doValue = doc["do"];
            }
        }
    }
}

/*****************************************************************
 *   Function to update the Json string and upload the frame
 ******************************************************************/
void cApplication::updateJsonAndSendFrame(void)
{
    debugPrint("## Generate And send frame");
    debugPrintln(m_oConfig.m_tEpoch);
    /*Check if RTC time is correct*/
    if (m_oConfig.m_tEpoch < 1609439400) 
    {
        // 1609439400 is Friday, January 1, 2021 12:00:00 AM GMT+05:30
        debugPrint("## Epoch Miss Match,Wrong Year : Sync RTC");
        RTCSyncNow = true;
        m_tPingEpoch = SendPing();
        m_iFrameInProcess = NO_FRAME;
        sendFrameType = NO_FRAME;
        m_oDisp.PopUpDisplayData.UploadStatus = (m_bIsGPS)? FRAME_GEN_FAILED : FRAME_GEN_FAILED_NO_GPS;
        return;
    }

    DynamicJsonDocument Data(1300);
    /*****************************************************/
    Data["reasonForPacket"] = "T";
    if (sendFrameType == VDIFF_FRAME)
    {
        Data["reasonForPacket"] = "V";
    }
    Data["name"] = "DO";
    Data["deviceId"] = WiFi.macAddress();
    Data["routerMacId"] = WiFi.BSSIDstr();
    Data["localIp"] = WiFi.localIP();
    Data["fwVer"] = FW_VERSION;
    Data["isReboot"] = -1;
    if (m_oConfig.m_u8IsReboot)
    {
        m_oConfig.m_u8IsReboot = 0;
        Data["isReboot"] = m_oConfig.espResetReason;
    }
    Data["FramesInBackUp"] = m_oDisp.DisplayGeneralVariables.backUpFramesCnt;
    Data["wifiSSId"] = WiFi.SSID();
    Data["rssi"] = WiFi.RSSI();
    Data["epoch"] = m_oConfig.m_tEpoch;
    Data["operationMode"] = m_iOperationMode;
    Data["lat"] = m_oGps.mPosition.m_lat;
    Data["long"] = m_oGps.mPosition.m_lng;
    Data["HDop"] = m_oGps.mPosition.hDop;
    Data["Satellites"] = m_oGps.mPosition.m_iSatellites;
    Data["rfId"] = "NO RFID";
    Data["PondName"] = CurrntPondName;
    Data["pondId"] = CurrentPondID;
    Data["locationId"] = CurrentLocationId;
    Data["localOffsetTimeInMin"] = TotalMinsOffSet;
    Data["do"] = roundToDecimals(DoMglValue, 5);        
    Data["temp"] = TempVal;                        
    Data["saturationPCT"] = roundToDecimals(DoSaturationVal, 5); 
    Data["salinity"] = CurrentPondSalinity;
    Data["voltage"] = voltageValue;
    Data["isHistory"] = LIVE_FRAME;
    // Data["isRtcSynced"] = 0;
    Data["timeBuffer"] = timebuffer;
    /*****************************************************/
    /*CHeck Here whether the DO value have any error or not*/
    if(Data["do"]<= 0 || (Data["temp"] <= 10 || Data["temp"] >= 55)){
        m_oPond.m_pondStatusMap[CurrntPondName].Backup = PONDMAP_VALUE_TAKEN_BUT_ERROR;
        m_oPond.savePondStatusToFile();
        Data["DataError"] = PONDMAP_VALUE_TAKEN_BUT_ERROR;
    }

    debugPrintln(" Document ready with data");
    /*Try to send frame if device is online of save to backup memory*/
    char frame[1300];
    if (isOnline)
    {
        serializeJson(Data, frame);
        debugPrint(frame);
        if (!m_oHttp.uploadDataFrame(frame))
        {
            Data["isHistory"] = HISTORY_FRAME;
            serializeJson(Data, frame);
            m_oBackupStore.writeInBS(&m_oFileSystem, frame);
            
            m_oDisp.PopUpDisplayData.UploadStatus = FRAME_UPLOAD_FAIL;
            safeStrcpy(m_oDisp.PopUpDisplayData.time, Data["timeBuffer"], sizeof(Data["timeBuffer"]));
            safeStrcpy(m_oDisp.PopUpDisplayData.pName, Data["PondName"], sizeof(Data["PondName"]));
            m_oDisp.PopUpDisplayData.doValue = Data["do"];
            debugPrintln("[App][updateJsonAndSendFrame][uploadDataFrame][Fail]");
            if(Data["DataError"]!= PONDMAP_VALUE_TAKEN_BUT_ERROR)
            {
                m_oPond.m_pondStatusMap[Data["PondName"]].Backup = PONDMAP_VALUE_FRAME_STORED_TO_BACKUP;
                m_oPond.savePondStatusToFile();
            }
        }
        else
        {
            m_oDisp.PopUpDisplayData.UploadStatus = FRAME_UPLOAD_SUCCESS;
            safeStrcpy(m_oDisp.PopUpDisplayData.time, Data["timeBuffer"], sizeof(Data["timeBuffer"]));
            safeStrcpy(m_oDisp.PopUpDisplayData.pName, Data["PondName"], sizeof(Data["PondName"]));
            m_oDisp.PopUpDisplayData.doValue = Data["do"];
            debugPrintln("[App][updateJsonAndSendFrame][uploadDataFrame][Success]");
            if(Data["DataError"]!= PONDMAP_VALUE_TAKEN_BUT_ERROR)
            {
                m_oPond.m_pondStatusMap[Data["PondName"]].Backup = PONDMAP_VALUE_FRAME_SENT_SUCESSFULLY;
                m_oPond.savePondStatusToFile();
            }
        }
    }
    else
    {
        m_oDisp.PopUpDisplayData.UploadStatus = FRAME_UPLOAD_FAIL_NO_INTERNET;
        safeStrcpy(m_oDisp.PopUpDisplayData.time, Data["timeBuffer"], sizeof(Data["timeBuffer"]));
        safeStrcpy(m_oDisp.PopUpDisplayData.pName, Data["PondName"], sizeof(Data["PondName"]));
        m_oDisp.PopUpDisplayData.doValue = Data["do"];
        debugPrintln("[App][updateJsonAndSendFrame][NO_INTERNET]");

        Data["isHistory"] = HISTORY_FRAME;
        serializeJson(Data, frame);
        m_oBackupStore.writeInBS(&m_oFileSystem, frame);
         debugPrintf("\n!!!!!!!!!!!!!!!!!!!!!\n%d\n!!!!!!!!!!!!!!!!!!!!!!!!\n", m_oPond.m_pondStatusMap[Data["PondName"]].Backup);
        if(Data["DataError"]!= PONDMAP_VALUE_TAKEN_BUT_ERROR)
        {
            m_oPond.m_pondStatusMap[Data["PondName"]].Backup = PONDMAP_VALUE_FRAME_STORED_TO_BACKUP;
            m_oPond.savePondStatusToFile();
        }
    }
    sendFrameType = NO_FRAME;
    /*Reset Timeout frame counter, Push Timeout Frame*/
    m_iFrameInProcess = NO_FRAME;
    FoundPondName = false;
}

/*******************************************
 *   Timer handler for led blink
 *******************************************/
void cApplication::staLEDHandler(void)
{
    static int ckCnt = 0;
    static int rssi = 0;
    static int ledCnt = 0;

    ckCnt++;
    ledCnt++;
    if (ckCnt >= 50) // 5 sec
    {
        ckCnt = 0;
        rssi = WiFi.RSSI();
    }
    if (isOnline && m_oHttp.m_bIsConnected)
    {
        if (ledCnt >= ((rssi / 10) * (-1)))
        {
            ledCnt = 0;
            m_oBsp.indLedToggle();
        }
    }
    else
    {
        m_oBsp.indLedOff();
    }
}

/*******************************************
 *   Timer handler for 100ms interrupt
 *******************************************/
void cApplication::AppTimerHandler100ms(void)
{
    m_u8AppConter1Sec++;
    staLEDHandler();
    operateBuzzer();
}

// void cApplication::convertTime(int TimeInMinsOffset)
// {
//     int offsetHour = TimeInMinsOffset/60;
//     int offSetMin = TimeInMinsOffset % 60;
//     // debugPrint("OffsetHour: ");debugPrint(offsetHour);debugPrintln("OffsetHour: ");debugPrintln(offSetMin);
//     // int hour = m_oRtc.m_iHour;
//     // int minutes = m_oRtc.m_iMinute + offSetMin;
//     int hour = m_oGps.GpsHour;
//     int minutes = m_oGps.GpsMins + offSetMin;
//     if (minutes >= 60)
//     {
//         minutes -= 60;
//         hour += 1;
//     }
//     hour += offsetHour;
//     if (hour >= 24)
//     {
//         hour -= 24;
//     }
//     // Format the time as "HH:MM"
//     if(m_bIsGPS)
//     {
//         snprintf(timebuffer, 6, "%02d:%02d", hour, minutes);
//     }
//     else
//     {
//         snprintf(timebuffer, 6, "00:00");
//     }
//     safeStrcpy(m_oDisp.DisplayHeaderData.time, timebuffer, sizeof(m_oDisp.DisplayHeaderData.time));
// }

void cApplication::convertTime(int TimeInMinsOffset)
{
    int gpsHour = m_oGps.GpsHour;
    int gpsMin  = m_oGps.GpsMins;

    // Convert GPS to total minutes
    int totalMinutes = gpsHour * 60 + gpsMin;

    // Apply offset
    totalMinutes += TimeInMinsOffset;

    // Normalize to 0–1439
    totalMinutes = (totalMinutes % (24 * 60) + (24 * 60)) % (24 * 60);

    int hour    = totalMinutes / 60;
    int minutes = totalMinutes % 60;

    // Convert to 12-hour format (no AM/PM)
    if (hour == 0) {
        hour = 12;          // midnight → 12
    } else if (hour > 12) {
        hour -= 12;         // 13–23 → 1–11
    }

    if (m_bIsGPS) {
        snprintf(timebuffer, sizeof(timebuffer), "%02d:%02d", hour, minutes);
    } else {
        snprintf(timebuffer, sizeof(timebuffer), "12:00");
    }

    safeStrcpy(m_oDisp.DisplayHeaderData.time, timebuffer, sizeof(m_oDisp.DisplayHeaderData.time));
}


void cApplication::checkBattteryVoltage(void)
{
    const float referenceVoltage = 3.3;
    const float R1 = 10000.0;
    const float R2 = 33000.0;
    const float maxVoltage = 3.23;

    int adcValue = analogRead(VBAT_ADC);
    float voltage = (adcValue / 4095.0) * referenceVoltage * (R1 + R2) / R2;
    voltage *= maxVoltage / referenceVoltage;
    voltageValue = voltage;
    m_oDisp.DisplayHeaderData.batteryPercentage = mapFloatToInt(voltageValue, 2.9, 4.2, 0, 100);
    /*check whether the power supply is connected or not*/
    m_bIsCharging = (m_oBsp.ioPinRead(BSP_PWR_DET))? true : false;
}

int cApplication::mapFloatToInt(float x, float in_min, float in_max, int out_min, int out_max) {
    return (int)((x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min);
}

/*Reset the pond backup status map mornign adn evening*/
void cApplication::ResetPondBackupStatusMap(int day, int hour) {
    int TimeInHoursWithOffset = hour + (TotalMinsOffSet / 60);
    // Morning task (2:00 AM)
    if (!morningCheckedThisBoot && TimeInHoursWithOffset >= MorningPondMapResetTime && (day != lastMorningDay)) {
        m_oPond.m_bMapNeedToBeUpdated = true;
        m_oPond.loadPondConfig();
        m_oMemory.putInt("morningDay", day);
        lastMorningDay = day;
        morningCheckedThisBoot = true;
        debugPrintln(" Resetting the ponds status as time is 2AM");
    }
        // Evening task (4:00 PM)
    if (!eveningCheckedThisBoot && TimeInHoursWithOffset >= EveningPondMapResetTime && (day != lastEveningDay)) {
        m_oPond.m_bMapNeedToBeUpdated = true;
        m_oPond.loadPondConfig();
        m_oMemory.putInt("eveDay", day);
        lastEveningDay = day;
        eveningCheckedThisBoot = true;
        debugPrintln(" Resetting the ponds status as time is 4PM");
    }  
}

/**************************************************
 *   Function to complete cApplication related tasks
 **************************************************/
void cApplication::applicationTask(void)
{
    if(m_oDisp.m_bSmartConfigMode) return;
    m_oBsp.wdtfeed();
    socketIO.loop();

    if(m_bDoFota) return;
    /* Update the display every 100millisecond*/
    RunDisplay();
    CheckForButtonEvent();

    if (m_bSendframe)
    {
        String output = String("[\"rpcr\"," + String(sendResult) + "]");

        // Send event
        socketIO.sendEVENT(output);
        // Print the serialized JSON for debugging
        debugPrintln(output);
        memset(sendResult, 0, sizeof(sendResult));
        m_bSendframe = false;
    }
    
    /*Reset Device on receipt of devicedata ,like Clamps type Siteid,name*/
    if (rebootAfterSetDataCmd > 0)
    {
        rebootAfterSetDataCmd--;
        debugPrint("espRestart In : ");
        debugPrint(rebootAfterSetDataCmd);
        debugPrintln(" secs");
        if (rebootAfterSetDataCmd <= 0)
        {
            ESP.restart();
        }
    }

    /*one Sec task*/
    if (m_u8AppConter1Sec >= 10)
    {
        convertTime(TotalMinsOffSet);
        m_oDisp.DisplayGeneralVariables.backUpFramesCnt = m_oBackupStore.countStoredFiles(&m_oFileSystem);
        // function to check the Http isconnected status
        
        static int sec5timer = 0;
        sec5timer++;
        if (sec5timer >= 5 && (m_iOperationMode == EVENT_BASED_MODE) && !FoundPondName && (m_bIsGPS || Is_Simulated_Lat_Longs))
        {
            // for (const auto &pair : m_oPond.m_pondStatusMap) {
            //     Serial.printf("Pond: %s, ActiveStatus: %d, BackupState: %d\n", pair.first.c_str(), pair.second.ActiveStatus,pair.second.Backup);
            // }
            m_bGetPondLocation = true;
            long st = millis();
            /*check for the pond location*/
            GetCurrentPondName();
            debugPrint(" TIme taken to get all the ponds data: ");debugPrintln(millis() - st);
            m_bGotPondName = false;
            sec5timer = 0;
        }
        /* Run the GPS function very second to encode lats, longs */
        m_oGps.gpstask();

        m_u8AppConter1Sec = 0;
        m_iTimeOutFrameCounter++;
        if (updateConfig) { updateDeviceConfigFile(); }
        if (m_bGetConfig && isOnline && m_oHttp.m_bIsConnected)
        {
            Serial.println(" Getting configuration initially");
            getConfigurationDeviceId();
        }


        checkBattteryVoltage();
        GetPondBoundaries();

        if (m_iOperationMode == EVENT_BASED_MODE) rfidTask();//rfid read and write the card data
        /*Read Time from GPS*/
        if(!Is_Simulated_Lat_Longs) m_oConfig.m_tEpoch = m_oGps.Epoch;
        // m_iRtcSyncCounter++;
        if ((isOnline == false) || (m_oHttp.m_bIsConnected == false))
        {
            rebootAfterOfflineCnt++;
            m_iNetCheckCounter++;
        }
        else
        {
            rebootAfterOfflineCnt = 0;
            m_iNetCheckCounter = 0;
        }

        if(m_bIsGPS) ResetPondBackupStatusMap(m_oGps.GpsDay, m_oGps.GpsHour);
        /*Reset the entire map*/
        if(ResetEntireMap){
            ResetEntireMap = false;
            m_oPond.m_bMapNeedToBeUpdated = true;
            m_oPond.loadPondConfig();
        }
    }
    /*Not ResetHandler change it to ResetHandler or something*/
    ResetHandler();
}


/*****************************************************************************************************
 *Function to check my current coordinates with the coords present in the pondBoundaries and
  get the current pondName if the distance between me and the coords is less than 1 metre
  And set the current pond salinity to the sensor
 ******************************************************************************************************/
void cApplication::GetCurrentPondName(void){
    if (m_bGetPondLocation)
    {
        nearestPonds.clear(); 
        /*set the current location coordinates*/
        if(Is_Simulated_Lat_Longs)
        {
            m_oGps.mPosition.m_lat = SimulatedLat;
            m_oGps.mPosition.m_lng = SimulatedLongs;
        }
        
        m_oPosition location = {m_oGps.mPosition.m_lat, m_oGps.mPosition.m_lng};
        debugPrintf("lat: %f, lng : %f\n", location.m_lat, location.m_lng);
        int len = m_oPond.m_locationVersions.size();
        debugPrintf("location versions size : %d \n", len);
        /*read the data from the locationID file*/
        for (int i = 0; i < len; i++)
        {
            char fileName[20] = "";
            snprintf(fileName, sizeof(fileName), "/%s.txt", m_oPond.m_oPondSettingList[i].m_cPondname);
            debugPrintf("Current FileName to check distance: %s\n", fileName);//NOTE: [GEOFENCE DISTANCE]uncomment to set distance
            /*If distance btw our coords and pond boundaries < 1, this below func return 1 so we read the pondName from the pondConfig*/
            int inPond = readPondNameFromFile(fileName, location);
            if (inPond == 1)
            {
                safeStrcpy(m_oPond.m_cCurrentPondname, m_oPond.m_oPondSettingList[i].m_cPondname, sizeof(m_oPond.m_cCurrentPondname));
                safeStrcpy(m_oPond.m_u64currentPondId, m_oPond.m_oPondSettingList[i].m_cPondId, sizeof(m_oPond.m_u64currentPondId));
                safeStrcpy(m_oPond.m_cLocationId, m_oPond.m_oPondSettingList[i].m_cLocationID, sizeof(m_oPond.m_cLocationId));
                m_oSensor.m_fSalinity = m_oPond.m_oPondSettingList[i].m_iSalinity;
                debugPrintf("got salinity value %f\n", m_oSensor.m_fSalinity);
                CurrentPondSalinity = m_oSensor.m_fSalinity;
                m_bGotPondName = true;
                debugPrint(" Current PondName: ");debugPrintln(m_oPond.m_cCurrentPondname);
                if (m_oSensor.m_fSalinity)
                {
                    int val = m_oSensor.setSalinity();
                    if (!val)
                        m_oSensor.setSalinity();
                }
                break;
            }
            else if(inPond >= 2)
            {
                updateNearestPonds(m_oPond.m_oPondSettingList[i].m_cPondname, inPond);
                strcpy(nearestPondName,  m_oPond.m_oPondSettingList[i].m_cPondname);
                nearestPondDistance = inPond;
                isNearToSomePond = true;
                debugPrintf(" NearestPondDistance: %d , nearestPondName: %s \n", nearestPondDistance, nearestPondName);
            }
            else
            {
                isNearToSomePond = false;
                debugPrintln(" No nearest ponds avaialble for this coordinates");
            }
        }
        if (!m_bGotPondName)
        {
            strcpy(m_oPond.m_cCurrentPondname, "");
            strcpy(m_oPond.m_u64currentPondId, "");
            strcpy(m_oPond.m_cLocationId, "");
            debugPrintln("There is No pond for the current coordinates");
        }
        strcpy(CurrntPondName, m_oPond.m_cCurrentPondname);
        strcpy(CurrentLocationId, m_oPond.m_cLocationId);
        strcpy(CurrentPondID, m_oPond.m_u64currentPondId);
        
        /*get the pond location using the posts*/
        m_bGetPondLocation = false; /*after getting the pond location */
    }
}
/*********************************************************************************************
 * Get the pond name from RFID tag data
 *********************************************************************************************/
String cApplication::GetPondNameFromRFID(String pondLocationFromCard, const char* jsonString) {
    StaticJsonDocument<4096> doc; // Adjust size based on actual JSON size

    DeserializationError error = deserializeJson(doc, jsonString);
    if (error) {    
        debugPrint("JSON deserialization failed: ");
        debugPrintln(error.c_str());
        return "";
    }

    JsonArray configArray = doc["config"].as<JsonArray>();
    for (JsonVariant entry : configArray) {
        String line = entry.as<String>();
        int firstPipe = line.indexOf('|');
        int secondPipe = line.indexOf('|', firstPipe + 1);
        int thirdPipe = line.indexOf('|', secondPipe + 1);
        int FourthPipe = line.indexOf('|', thirdPipe + 1);

        String pondName = line.substring(firstPipe + 1, secondPipe);
        String location = line.substring(secondPipe + 1, thirdPipe);
        String Sal = line.substring(thirdPipe + 1, FourthPipe);

        CurrentPondSalinity = atoi(Sal.c_str());

        if (location == pondLocationFromCard) {
            FoundPondName = true;
            debugPrint(" pond name found using the pond location from RFID tag: ");debugPrintln(pondName);
            return pondName;
        }
    }

    return ""; // Not found
}

/*******************************************************************************************************************************
 * Function to get the pond boundaries whenever there is version change i.e change of some pond details or a new pond is added
 ********************************************************************************************************************************/
void cApplication::GetPondBoundaries(void){
    if (m_oPond.m_bGetPondBoundaries && m_oHttp.m_bIsConnected)
        {
            int length = m_oPond.updatedPondIds.size();
            if (!length)
                m_oPond.m_bGetPondBoundaries = false;
            for (auto it = m_oPond.updatedPondIds.begin(); it != m_oPond.updatedPondIds.end(); /* no increment here */)
            {
                debugPrintf("PondId: %s, PondName: %s\n", it->first.c_str(), it->second.c_str());
                int response = getConfigurationPondBoundaries(it->first.c_str(), it->second.c_str());
                if (response)
                {
                    // Erase the pair and move the iterator to the next element
                    it = m_oPond.updatedPondIds.erase(it);
                }
                else
                {
                    // Only increment the iterator if no deletion was made
                    ++it;
                }
            }
        }
}

/****************************************************************************************************
 * Function to handle the reset actions that are made in display or buttons menu in config mode
 ****************************************************************************************************/
void cApplication::ResetHandler(void)
{
    if (m_oDisp.m_bResetWifiFlag)
    {
        ResetWifiCredentials();
    }
    else if (m_oDisp.m_bResetServerFlag)
    {
        ResetServerCredentials();
    }
}

void cApplication::AssignDataToDisplayStructs(){
    safeStrcpy(m_oDisp.DisplayLeftPanelData.pName, CurrntPondName, sizeof(m_oDisp.DisplayLeftPanelData.pName));
    safeStrcpy(m_oDisp.DisplayLeftPanelData.nearestPonds, String(getNearestPondString()).c_str(), sizeof(m_oDisp.DisplayLeftPanelData.nearestPonds));
    m_oDisp.DisplayHeaderData.Satellites = m_oGps.mPosition.m_iSatellites;
    m_oDisp.DisplayHeaderData.rssi = WiFi.RSSI();
    m_oDisp.DisplayHeaderData.LocationStatus = m_bIsGPS;
    m_oDisp.DisplayFooterData.isHttpConnected = m_oHttp.m_bIsConnected;
    /*Footer Data*/
    m_oDisp.DisplayFooterData.FooterType = m_oDisp.PopUpDisplayData.UploadStatus;//TODO : to handle the display updation by comparing the structure

    strncpy(m_oDisp.DisplayFooterData.RouterMac, String(WiFi.BSSIDstr()).c_str(), sizeof(m_oDisp.DisplayFooterData.RouterMac));
}
/****************************************************************************************************
 * Function to run the display and handle send frame
 ****************************************************************************************************/
void cApplication::RunDisplay(void)
{
    AssignDataToDisplayStructs();
    /* Check whether the GPS Coordinates are found or not*/
    m_bIsGPS = ((m_oGps.mPosition.m_lat != 0.0) && (m_oGps.mPosition.m_lng != 0.0)) ? true : false;
    if(Is_Simulated_Lat_Longs) m_bIsGPS = ((SimulatedLat != 0.0) && (SimulatedLongs != 0.0)) ? true : false;
    unsigned long st = millis();
    m_oDisp.renderDisplay(currentScreen, &m_oPond);
    // Serial.print(" Unsigned : ");
    // Serial.println(millis() - st);
}


//TODO: seperate function for GPS and read the values every single time and update the gloabal variables in app.cpp instead from gps.cpp 
void convertEpoch(time_t epoch) {
  struct tm *timeinfo = localtime(&epoch); // or gmtime(&epoch) for UTC

  int hour   = timeinfo->tm_hour;
  int minute = timeinfo->tm_min;
  int second = timeinfo->tm_sec;

  int day    = timeinfo->tm_mday;
  int month  = timeinfo->tm_mon + 1;  // tm_mon is 0-based
  int year   = timeinfo->tm_year + 1900; // tm_year is years since 1900

  debugPrintf("Time: %02d:%02d:%02d  Date: %02d-%02d-%04d\n",
                hour, minute, second, day, month, year);
}

void printSystemInfo() {
  // 🟢 Heap info
  size_t freeHeap = ESP.getFreeHeap();
  size_t minFreeHeap = ESP.getMinFreeHeap();   // lowest recorded free heap
  size_t largestBlock = heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);

  Serial.println("-----------------------------------");
  Serial.printf("Heap: Free=%u, MinFree=%u, LargestBlock=%u bytes\n",
                freeHeap, minFreeHeap, largestBlock);

  // 🟢 Stack info (for current task)
  UBaseType_t watermark = uxTaskGetStackHighWaterMark(NULL);
  Serial.printf("Stack: HighWaterMark=%u words (~%u bytes free)\n",
                watermark, watermark * sizeof(StackType_t));

  // 🟢
    size_t total = SPIFFS.totalBytes();
    size_t used  = SPIFFS.usedBytes();
    Serial.printf("SPIFFS: Total=%u, Used=%u, Free=%u bytes\n", total, used, total - used);

  Serial.println("-----------------------------------");
}

/****************************************************************************************************
 * Function to parse the commands from sensor comm:modbus RTU, when it is not in calibration mode
 ****************************************************************************************************/
void cApplication::commandParseTask(void)
{
    if(m_bDoFota)
    {
        m_oDisp.printFOTA(m_oHttp.currprogress);
        return;
    }
    // printSystemInfo();
    m_oBsp.wdtfeed();
    /*function to read the DO and Temp values only when not setting the calibration values to the sensor*/
    if (!m_oDisp.StopReadingSensor)
    {
        m_oSensor.getTempAndDoValues();
    }
    else if(m_oDisp.StopReadingSensor)
    {
        m_oDisp.m_bCalibrationResponse = (m_oSensor.setCalibrationValues() ? 1 : (m_oSensor.setCalibrationValues() ? 1 : 0));
        debugPrintf(" Calibration response values: %d \n", m_oDisp.m_bCalibrationResponse);
    }
    
    // Track sensor communication status
    static int sensorFailCntr = 0;
    
    // Count consecutive sensor communication failures
    if(m_oSensor.noSensor) {
        sensorFailCntr++;
    } else {
        sensorFailCntr = 0;
    }
    
    // Only update working values if we have valid sensor readings
    // Ignore zero values from single communication failures
    bool validSensorData = (m_oSensor.m_fDo > 0 && m_oSensor.m_fTemp > 0 && !m_oSensor.noSensor && (m_oSensor.m_fDoMgl >= -1.0 && m_oSensor.m_fDoMgl <= 25.0));
    
    if(validSensorData)
    {
        DoMglValue = m_oSensor.m_fDoMgl;
        DoSaturationVal = m_oSensor.m_fDo;
        TempVal = m_oSensor.m_fTemp;

        m_oDisp.DisplayLeftPanelData.DoSaturationValue = roundToDecimals(m_oSensor.m_fDo, 2);
        m_oDisp.DisplayLeftPanelData.DoValueMgL = roundToDecimals(m_oSensor.m_fDoMgl, 2);
        m_oDisp.DisplayLeftPanelData.TempValue = roundToDecimals(m_oSensor.m_fTemp, 1);
        m_oDisp.DisplayLeftPanelData.Salinity = m_oSensor.m_fSalinity;
    }
    
    
    // Reset working values when sensor has been disconnected for too long, this prevents loss of valid data due to temporary communication issues
    if(sensorFailCntr >= 5)
    {
        Serial.println(" [App][commandParseTask] Sensor disconnected - resetting working values");
        m_oDisp.DisplayGeneralVariables.IsSensorConnected = true;
        TempVal = m_oDisp.DisplayLeftPanelData.TempValue = 0.0;
        DoSaturationVal = m_oDisp.DisplayLeftPanelData.DoSaturationValue = 0;
        DoMglValue = m_oDisp.DisplayLeftPanelData.DoValueMgL = 0;
        m_oDisp.DisplayLeftPanelData.Salinity = 0.0;
        sensorFailCntr = 0;
    }
    else
    {
        m_oDisp.DisplayGeneralVariables.IsSensorConnected = false;
    }
}

void cApplication::updateNearestPonds(const char* pondName, int distance)
{
    if (distance < 2 || distance > NEAREST_POND_MAX_VALUE) return;

    // Check if the pond already exists, update if found
    for (auto& pond : nearestPonds)
    {
        if (strcmp(pond.name, pondName) == 0)
        {
            pond.distance = distance;
            goto sort;
        }
    }

    // Add if less than 2 ponds
    if (nearestPonds.size() < MAX_NEAREST_PONDS)
    {
        PondDistance newPond;
        strncpy(newPond.name, pondName, sizeof(newPond.name));
        newPond.distance = distance;
        nearestPonds.push_back(newPond);
    }
    else
    {
        // Replace farthest if new is closer
        auto maxIt = std::max_element(nearestPonds.begin(), nearestPonds.end(),
                                      [](const PondDistance& a, const PondDistance& b) { return a.distance < b.distance; });

        if (maxIt != nearestPonds.end() && distance < maxIt->distance)
        {
            strncpy(maxIt->name, pondName, sizeof(maxIt->name));
            maxIt->distance = distance;
        }
    }

    sort:std::sort(nearestPonds.begin(), nearestPonds.end(),
              [](const PondDistance& a, const PondDistance& b) { return a.distance < b.distance; });
}


String cApplication::getNearestPondString()
{
    if(strcmp(CurrntPondName, "") && nearestPonds.empty()) return "Inside the Pond";
    else if(nearestPonds.empty()) return "No Ponds in range";

    String msg = "Near to ";
    for (size_t i = 0; i < nearestPonds.size(); ++i)
    {
        char temp[20];
        snprintf(temp, sizeof(temp), "%s-%dm", nearestPonds[i].name, nearestPonds[i].distance);
        msg += temp;
        if (i < nearestPonds.size() - 1)
            msg += ", ";
    }
    
    // debugPrintln(msg);
    return msg;
}
/****************************************************************************************************
 * Function to read pond name from file 
 ****************************************************************************************************/
int cApplication::readPondNameFromFile(const char *path, m_oPosition data)
{
    char output[3500];
    /*read the file and copy to output buffer*/
    int ret = m_oFileSystem.readFile(path, output);
    // Serial.println(output);
    if (ret > 0)
    {
        debugPrintln("File read succesfully");
        StaticJsonDocument<7000> deviceInfo;
        DeserializationError err = deserializeJson(deviceInfo, output); /*Deserialize the json document*/
        if (err.code() == DeserializationError::Ok)                     /*check for deserialization error*/
        {   
            if (deviceInfo.containsKey("config"))
            {   
                JsonObject config = deviceInfo["config"];
                if (config.containsKey("posts"))
                {   
                    int length = deviceInfo["config"]["posts"].size();
                    debugPrintf("@@ length : %d\n", length);
                    if (length)
                    {
                        m_oPosition m_aPosts[length] = {};
                        for (int i = 0; i < length; i++)
                        {
                            m_aPosts[i].m_lat = config["posts"][i]["lat"].as<double>();
                            m_aPosts[i].m_lng = config["posts"][i]["lng"].as<double>();
                        }
                        /*Geo fencing*/
                        double value = m_oGeofence.isBotInsideGeofence(m_aPosts, length, data);
                        debugPrintf("value = %f\n", value);
                        /*If the distance form our coords and pond boundary is less than 1 tehn return 1*/
                        if (value < 2)
                        {
                            return 1;
                        }
                        else if(value >= 2 && value <= NEAREST_POND_MAX_VALUE)
                        {
                            int dist = (int)value;
                            return dist;
                        }
                    }
                }
                else
                {
                    debugPrintln("[App][ReadPNameFromFile]: There is no key named posts in the JSON");
                }
            }
            else
            {
                debugPrintln("@@ not found");
            }
        }
        else
        {
            debugPrintln("[App][ReadPNameFromFile]: Deserialization error...");
        }
    }
    else
    {
        debugPrintln("[App][ReadPNameFromFile]: File reading failed..");
    }
    return 0;
}

/*********************************************************
 * Function for Switch wifi Networks
 * @param [in] None
 * @param [out] None
 *********************************************************/
void cApplication::reconnectWifi(void)
{
    debugPrintln("@@ reconnectWifi  network....");
    WiFi.disconnect();
    WiFi.begin(m_cWifiSsid, m_cWifiPass);
}

/**************************************************************
 * funciton to check esp32 is connected to wifi
 * @param [in] None
 * @param [out] None
 **************************************************************/
void cApplication::checkWifiConnection(void)
{
    if (WiFi.status() == WL_CONNECTED)
    {
        if (!isOnline)
            onConnected();
    }
    else
    {
        if (isOnline)
            onDisConnected();
    }
}

/*******************************************************
 *   Function for frame handling related tasks
 *********************************************************/
void cApplication::frameHandlingTask(void)
{
    if(m_oDisp.m_bSmartConfigMode) return;

    m_oBsp.wdtfeed();
    /*return from here when fota is running*/
    if (m_bDoFota)
    {
        return;
    }
    checkWifiConnection();

    /*Update and send frame*/   
    if ((m_iFrameInProcess == NO_FRAME) && (sendFrameType != NO_FRAME))
    {
        m_iFrameInProcess = sendFrameType;
        updateJsonAndSendFrame();
    }
    m_oBsp.wdtfeed();

    /*Check for multiple Wifi Networks when device is offline every 30sec*/
    if (m_iNetCheckCounter > 15)
    {
        m_iNetCheckCounter = 0;
        reconnectWifi();
    }

    static int cntr = 0;
        if(cntr >= 50  && m_oHttp.m_bIsConnected) {
            SendPing(); 
            cntr = 0;
        }
        cntr++;

    /*Sends frame from backup if any, and if not connceted ping*/
    m_u8SendBackUpFrameConter++;
    if (m_u8SendBackUpFrameConter >= 10)
    {
        m_u8SendBackUpFrameConter = 0;
        uploadframeFromBackUp();
    }
    /*If the device is in continuous mode send the frames for every 5 minutes*/
    if (m_iOperationMode == CONTINUOUS_BASED_MODE)
    {
        if (m_iTimeOutFrameCounter > m_iDataFrequencyInterval * 60)
        {
            debugPrintln("@T T T frame");
            if (sendFrameType == NO_FRAME && strcmp(CurrentPondID,"") == 1)
            {
                m_iTimeOutFrameCounter = 0;
                sendFrameType = TOUT_FRAME;
            }
        }
    }
    else
    {
        if (m_iTimeOutFrameCounter > 5 * 60 && sendFrameType == NO_FRAME && strcmp(CurrentPondID,"") == 1)
        {
            debugPrintln("@ generating T T T frame");
            m_iTimeOutFrameCounter = 0;
            sendFrameType = TOUT_FRAME;
        }
    }
}

/***********************************************************
 * Function to get firmware and update device
 * @param [in] None
 * @param [out] None
 ***********************************************************/
void cApplication::fotaTask(void)
{
    if(m_oDisp.m_bSmartConfigMode) return;
    
    m_oBsp.wdtfeed();
    if (m_bDoFota)
    {
        m_oBsp.wdtfeed();
        sendFrameType = NO_FRAME;
        debugPrintln("Calling performOTA()");
        m_oDisp.ClearDisplay();
        uint8_t u8OtaResponse = m_oHttp.performOTA(&m_oBsp);
        switch (u8OtaResponse)
        {
        case 0:
            debugPrintln("OTA success");
            break;
        case 5:
            debugPrintln("OTA fail due to http busy, retrying...");
            break;
        default:
            m_bDoFota = false;
            break;
        }
    }
}
/*
* Check and sync rTc
*/
void cApplication::CheckAndSyncRTC(void)
{
    m_oBsp.wdtfeed();
    /*Check and sync RTC every 10 MIN */
    if (m_iRtcSyncCounter >= 600 || RTCSyncNow)
    {
        if (isOnline)
        {
            debugPrintln("@@ Check and Sync RTC");
            /*Get server epoch in Local time*/
            time_t currentEpoch;
            if(m_oGps.Epoch == 943920000 || m_oGps.Epoch == 0)
            {
                currentEpoch = SendPing();
                debugPrint("Server EPoch : ");
            }
            else
            {
                debugPrint("GPS EPoch : ");
                currentEpoch = m_oGps.Epoch;
            }
            debugPrintln(currentEpoch);
            /*Convert local epoch to  GMT*/
            if (m_oBsp.syncRTCTime(&m_oRtc, currentEpoch, m_oConfig.m_tEpoch))
            {
                m_iRtcSyncCounter = 0;
            }
            else
            {
                /*If Rtc sync is failed it will try to sync in one Min again*/
                m_iRtcSyncCounter = 540;
            }
            RTCSyncNow = false;
        }
        else
        {
            m_iRtcSyncCounter = 540;
        }
    }
}


void cApplication::rfidTask(void)
{
    m_oBsp.wdtfeed();
    /*checks weather the RFID is available or not*/
    bool isCardFound = m_oRfid.isTagPresent();
    // debugPrintf("isCardFound %d\n", isCardFound);
    if (isCardFound) //
    {
        buzz = 5;
        // showReadWriteCycle = false;
        char cardPayload[716] = {'\0'}; // max data stored in a card is 716 bytes
        /*reads the data from the card and saves the data in the variable sent as parameter*/
        m_oRfid.readDataFromCard(cardPayload);
        if (strcmp(cardPayload, "\0"))
        {
            debugPrintln("data read succesfully: ");
            debugPrintln(cardPayload);
            
            int FileSize = m_oFileSystem.getFileSize(FILENAME_IDSCONFIG);
            char FileData[FileSize];
            int ret = m_oFileSystem.readFile(FILENAME_IDSCONFIG, FileData);
            if(ret)
            {
                debugPrintln("[App][RFIDTASK]: There is some data in the file going to find the pond name in the file");
                String PondCurrname = GetPondNameFromRFID(cardPayload, FileData);
                strcpy(CurrntPondName, PondCurrname.c_str());
            } 
            else
            {
                debugPrintln(" NO data in the File or error occured while reading the file");
            }
        }
        else
        {
            debugPrintln("card read failed");
        }
    }
    else
    {
        // debugPrintln("old card detected");
    }
}


/****************************************************************
 * Function to get the devic configuration from the server
 * @param [in] None
 * @param [out] None
 *****************************************************************/
uint8_t cApplication::getConfigurationDeviceId(void)
{
    uint8_t ret = 0;
    char deviceid[256];
    String macAdress = WiFi.macAddress();
    // String macAdress = "90:15:06:E4:47:44";
    sprintf(deviceid, "deviceId=%s", String(macAdress).c_str());
    if (m_oHttp.getConfig(deviceid))
    {
        String responseData = m_oHttp.m_sPayload;
        debugPrintln(responseData);
        int PayLoadSize = responseData.length();
        Serial.printf(" PayLoad Size : %d \n", PayLoadSize);
        /*deserialize the json document*/
        DynamicJsonDocument configInfo(PayLoadSize * 2);
        DeserializationError err = deserializeJson(configInfo, responseData); /*Deserialize the json document*/
        if (err.code() == DeserializationError::Ok)                           /*check for deserialization error*/
        {
            /*read operationMode*/
            if (configInfo.containsKey("operationMode"))
            {
                m_iOperationMode = configInfo["operationMode"];
                debugPrintf("@@ operationMode found %d\n", m_iOperationMode);
            }
            else
            {
                m_iOperationMode = EVENT_BASED_MODE;
                debugPrintln("@@ operationMode not found");
            }
            /***********************************************************************************************
                check for the operation mode in the payload
                if it is ContinuousMode, load the FILE of coninuosmode config and check for the verison
                if it is EventBasedMode, load the FILE of EventMode config and check for the version
            ************************************************************************************************/
            if (m_iOperationMode == EVENT_BASED_MODE) {
                m_oPond.loadPondConfig();
                int64_t iVersionInFrame = 0;
                if (configInfo.containsKey("version"))
                {
                    iVersionInFrame = configInfo["version"];
                    debugPrintln("EVENT: Obtained Version from payload");
                }
                if (iVersionInFrame != m_oPond.m_i64ConfigIdsVersion)
                {
                    /*write to file when version changes*/
                    Serial.printf(" Response data length : %d \n", responseData.length());
                    if (responseData.length() >2500) {
                        return 0;
                    }
                    m_oBackupStore.clearNonBackupFiles(&m_oFileSystem);
                    m_oFileSystem.writeFile(FILENAME_IDSCONFIG, String(responseData).c_str());
                    debugPrintln("@@ file saved in file");
                    updateConfig = true;
                    m_oPond.m_bMapNeedToBeUpdated = true;
                    /*load location ids from file*/
                    m_oPond.loadPondConfig();
                    ResetEntireMap = true;
                    debugPrintln("@@ location ids loaded from file here");
                }
            }
            else if(m_iOperationMode == CONTINUOUS_BASED_MODE)
            {
                loadDeviceConfig();
                int64_t iVersionInFrame = 0;
                if (configInfo.containsKey("version"))
                {
                    iVersionInFrame = configInfo["version"];
                    debugPrintln("CONTINUOUS: Obtained Version from payload");
                }
                debugPrintf(" versionFrom Payload : %d,  version from deviceConfig : %d \n", iVersionInFrame, m_oPond.m_i64ConfigIdsVersion);
                if (iVersionInFrame != m_oPond.m_i64ConfigIdsVersion)
                {
                    /*Only write Key:Value pair to file when version changes, inorder to avoid data mismatch when updated through RPC*/
                    if (configInfo.containsKey("config"))
                    {
                        bool updateFile = false;
                        JsonArray slots = configInfo["config"];
                        int length = slots.size();
                        debugPrintf("length: %d\n", length);
                        if (length == 1)
                        {
                            char data[150];
                            strncpy(data, slots[0], sizeof(data));
                            debugPrintln(data);

                            long locationVersion;
                            char pondName[50];
                            char pondId[50];
                            int salinity;
                            char locationId[50];

                            sscanf(data, "%ld|%[^|]|%[^|]|%d|%[^|]", &locationVersion, pondName, pondId, &salinity, locationId);
                        
                            char output[800];
                            /*create the json document*/
                            StaticJsonDocument<800> doc;
                            doc["version"] = locationVersion;
                            doc["operationMode"] = m_iOperationMode;
                            doc["pondId"] = pondId;
                            doc["locationId"] = locationId;
                            doc["pondName"] = pondName;
                            doc["salinity"] = salinity;
                            /*Serialize the json document*/
                            serializeJson(doc, output);
                            debugPrintln(output);
                            m_oFileSystem.writeFile(FILENAME_DEVICECONFIG, output);
                        }
                    }
                    debugPrintln("@@ file saved in file");
                    updateConfig = true;
                    /*load location ids from file*/
                    debugPrintln("[App][getconfig] Going to load the device config second time ");
                    loadDeviceConfig();
                    debugPrintln("@@ location ids loaded from file here");
                }
                else {
                    debugPrintln(" version is same in continuous mode file");
                }
            }
            m_bGetConfig = false;
            ret = 1;
        }
        else
        {
            debugPrintln("deserialization error");
            ret = 0;
        }
    }
    else
    {
        ret = 0;
        debugPrintln("failed to get DO config..:-(");
        m_oPond.loadPondConfig();
        debugPrintln("@@ location ids loaded from file");
    }
    return ret;
}

/****************************************************************
 * Function to get the pond boundaries configuration from the server with pondID
 * create if the pond is 
 * @param [in] None
 * @param [out] None
 *****************************************************************/
uint8_t cApplication::getConfigurationPondBoundaries(const char *pondID, const char *pName)
{
    debugPrintln(" In the getconfiguration pondBoundaries");
    uint8_t ret = 0;
    char pondid[256];
    sprintf(pondid, "pondId=%s", pondID);
    if (m_oHttp.getPondBoundaries(pondid))
    {
        String responseData = m_oHttp.m_sPayload;
        debugPrintln(m_oHttp.m_sPayload);
        int Size = responseData.length();
        if(Size > 3000) return 0;

        /*deserialize the json document*/
        DynamicJsonDocument configInfo(Size*2);
        DeserializationError err = deserializeJson(configInfo, responseData); /*Deserialize the json document*/
        if (err.code() == DeserializationError::Ok)                           /*check for deserialization error*/
        {
            int64_t iVersionInFrame = 0;
            if (configInfo.containsKey("version"))
            {
                iVersionInFrame = configInfo["version"];
            }
            if (iVersionInFrame != m_oPond.m_locationVersions[pondID])
            {
                char fileName[20] = "";
                snprintf(fileName, sizeof(fileName), "/%s.txt", pName);
                debugPrintf("PondName: %s\n", fileName);
                /*write to file when version changes*/
                m_oFileSystem.writeFile(fileName, String(responseData).c_str());
                debugPrintln("@@ file saved in file");
            }

            ret = 1;
        }
        else
        {
            debugPrintln("deserialization error");
            ret = 0;
        }
    }
    else
    {
        ret = 0;
        debugPrintln("failed to get pond boundaries config..:-(");
    }
    return ret;
}

void cApplication::loadDeviceConfig(void)
{
    debugPrintln("@@@@@@@@@@@@--------------------------------------------################## In the load device configfile");
    char output[800];
    /*read the file and copy to output buffer*/
    int ret = m_oFileSystem.readFile(FILENAME_DEVICECONFIG, output);
    if (ret > 0)
    {
        StaticJsonDocument<800> deviceInfo;
        DeserializationError err = deserializeJson(deviceInfo, output); /*Deserialize the json document*/
        if (err.code() == DeserializationError::Ok)                     /*check for deserialization error*/
        {
            /*version*/
            if (deviceInfo.containsKey("version"))
            {
                m_oPond.m_i64ConfigIdsVersion = deviceInfo["version"];
                debugPrint("@@ version found in file : ");
                debugPrintln(m_oPond.m_i64ConfigIdsVersion);
            }
            else
            {
                m_oPond.m_i64ConfigIdsVersion = 0;
                debugPrintln("@@ version not found in file");
            }
            /*read operationMode*/
            if (deviceInfo.containsKey("operationMode"))
            {
                m_iOperationMode = deviceInfo["operationMode"];
                debugPrintf("@@ operationMode found %d\n", m_iOperationMode);
            }
            else
            {
                m_iOperationMode = EVENT_BASED_MODE;
                debugPrintln("@@ operationMode not found");
            }
            //TODO: If config is not ready statically set the operationmode to 0
            /*read pond detials*/
            if (m_iOperationMode == CONTINUOUS_BASED_MODE)
            {
                /*Read PondID*/
                if (deviceInfo.containsKey("pondId"))
                {
                    strcpy(CurrentPondID, deviceInfo["pondId"]);
                    debugPrintf("@@ pond Id found %s\n", CurrentPondID);
                }
                else
                {
                    strcpy(CurrentPondID, "");
                    debugPrintln("@@ pond Id not found");
                }
                /*read LocationID*/
                if (deviceInfo.containsKey("locationId"))
                {
                    strcpy(CurrentLocationId, deviceInfo["locationId"]);
                    debugPrintf("@@ location Id found %s\n", CurrentLocationId);
                }
                else
                {
                    strcpy(CurrentLocationId, "");
                    debugPrintln("@@ location Id not found");
                }
                /*read PondName*/
                if (deviceInfo.containsKey("pondName"))
                {
                    strcpy(CurrntPondName, deviceInfo["pondName"]);
                    debugPrintf("@@ pond Name found %s\n", CurrntPondName);
                }
                else
                {
                    strcpy(CurrntPondName, "");
                    debugPrintln("@@ pond Name not found");
                }
                /*read salinity*/
                if (deviceInfo.containsKey("salinity"))
                {
                    CurrentPondSalinity = deviceInfo["salinity"];
                    debugPrintf("@@ salinity found %s\n", CurrentPondSalinity);
                }
                else
                {
                    CurrentPondSalinity = 0;
                    debugPrintln("@@ salinity not found");
                }
            }
        }
        else {
            debugPrintln(" Deserialization Error");
        }
    }
    else {
        debugPrintln(" Nothing found in the file");
    }
}

/**********************************************************************************************************
 * Function to store the pond details in fixed mode, changed from RPC in fixed mode
 * *******************************************************************************************************/
void cApplication::updateDeviceConfigFile()
{
    debugPrintln(" @ @ @ inside updateDeviceConfigFile");
    char output[800];
    /*create the json document*/
    StaticJsonDocument<800> doc;
    doc["operationMode"] = m_iOperationMode;
    doc["LocalTimeMinute"] = TotalMinsOffSet;
    if (m_iOperationMode == CONTINUOUS_BASED_MODE)
    {
        doc["pondId"] = CurrentPondID;
        doc["locationId"] = CurrentLocationId;
        doc["pondName"] = CurrntPondName;
        doc["salinity"] = CurrentPondSalinity;
    }
    /*Serialize the json document*/
    serializeJson(doc, output);
    debugPrintln(output);
    /*save file in filesystem*/
    m_oFileSystem.writeFile(FILENAME_DEVICECONFIG, output);
    updateConfig = false;
}

/***************************************************************
 * Function to read the wifi configuration from config file
 * @param [in] None
 * @param [out] None
 ***************************************************************/
void cApplication::readDeviceConfig(void)
{
    String sWifiSSID = m_oMemory.getString("wifiSsid", "Nextaqua_EAP110"); // get wifi SSID
    String sWifiPASS = m_oMemory.getString("wifiPass", "Infi@2016");       // get wifi password
    String sSeverIP = m_oMemory.getString("serverIP", m_oHttp.m_cDefaultServerIP);      // get server IP
    TotalMinsOffSet = m_oMemory.getInt("LocalMins", 330);                // get offset time
    m_iDataFrequencyInterval = m_oMemory.getInt("interval", 5);// To post the data that frequently

    safeStrcpy(m_cWifiPass, sWifiPASS.c_str(), sizeof(m_cWifiPass));
    safeStrcpy(m_cWifiSsid, sWifiSSID.c_str(), sizeof(m_cWifiSsid));
    safeStrcpy(m_oHttp.m_cServerIP, sSeverIP.c_str(), sizeof(m_oHttp.m_cServerIP));

    safeStrcpy(m_oDisp.DisplayFooterData.ServerIp, m_oHttp.m_cServerIP, sizeof(m_oDisp.DisplayFooterData.ServerIp));
    safeStrcpy(m_oDisp.DisplayGeneralVariables.WiFiSsid, m_cWifiSsid, sizeof(m_oDisp.DisplayGeneralVariables.WiFiSsid));
    safeStrcpy(m_oDisp.DisplayGeneralVariables.WiFiPass, m_cWifiPass, sizeof(m_oDisp.DisplayGeneralVariables.WiFiPass));

    debugPrint("wifissid : ");
    debugPrintln(m_cWifiSsid);
    debugPrint("wifiPass : ");
    debugPrintln(m_cWifiPass);
    debugPrint("serverIP : ");
    debugPrintln(m_oHttp.m_cServerIP);
    debugPrint("offsetMin : ");
    debugPrintln(TotalMinsOffSet);
    debugPrint("interval : ");
    debugPrintln(m_iDataFrequencyInterval);

    lastMorningDay = m_oMemory.getInt("morningDay", -1);
    lastEveningDay = m_oMemory.getInt("eveDay", -1);

    MorningPondMapResetTime = m_oMemory.getInt("MrngTime", 2);
    EveningPondMapResetTime = m_oMemory.getInt("EvngTime", 14);
    debugPrintf("\n Last Morning Date : %d \n", lastMorningDay);
    debugPrintf("\n Last Evening Date : %d \n", lastEveningDay);

    /*load DO Configuration*/
    m_oPond.loadPondConfig();
    // loadDeviceConfig();
}

void listSPIFFSFiles() {
  File root = SPIFFS.open("/");
  if (!root || !root.isDirectory()) {
    Serial.println("Failed to open SPIFFS root directory!");
    return;
  }

  File file = root.openNextFile();
  while (file) {
    Serial.print("FILE: ");
    Serial.print(file.name());
    Serial.print("\tSIZE: ");
    Serial.println(file.size());
    file = root.openNextFile();
  }
}

/**************************************************************
 * Function to check and complete cApplication related tasks
 ***************************************************************/
int cApplication::appInit(void)
{
    /*Serial debug bin*/
    Serial.begin(115200);
    debugPrint("millis after serial begin: ");debugPrintln(millis());
    m_oDisp.begin();
    /*print the wakeup reason when code restarts*/
    print_wakeup_reason();
    /*Init serial for DO data*/
    Serial1.begin(9600, SERIAL_8N1, 14, 13);
    /*******************************************************
     * Serial for GPS module
     * 9mm - Serial2(38400);
     * 6mm - Serial2(9600);
     * Change the baudrates based on the GPS module Connected.
     * **********************************************************/
    Serial2.begin(38400, SERIAL_8N1, 26, 27);
    /*Modbus and sensor initialization*/
    m_oSensor.sensorInit(Serial1);
    char hostName[50] = {0};
    sprintf(hostName, "NA_IOT_DO_%s", WiFi.macAddress().c_str());
    WiFi.setHostname(hostName);
    /*Print the restart reason and if it is Brownout goes to sleepMode*/
    print_restart_reason();
    if (m_oConfig.espResetReason == ESP_RST_BROWNOUT)
    {
        debugPrintln("BrownOut Triggered");
        esp_deep_sleep_start();
    }
    /*File System initialization*/
    m_oFileSystem.begin();
    /*Backup storage initialization*/
    m_oBackupStore.InitilizeBS(&m_oFileSystem);
    /*NVS memory initialization*/
    m_oMemory.begin("deviceMemory", false);
    /*read device memory and load Do config*/
    readDeviceConfig();
    /*Wifi initailization */
    wifiInitialization();
    /*Generate the URI path with esp32 MacAddress*/
    sprintf(m_cUriPath, "/socket.io/?deviceId=%s&deviceType=%s&fwVersion=%d&EIO=4", String(WiFi.macAddress()).c_str(), DEVICE_TYPE, FW_VERSION);
    /*Init Socket Connection*/
    socketIO.setReconnectInterval(10000);
    socketIO.setExtraHeaders("Authorization: 1234567890");
    socketIO.begin(m_oHttp.m_cServerIP, m_oHttp.m_u16ServerPort, m_cUriPath, protocol);
    socketIO.onEvent(socketIOEvent);
    /*Bsp GPIO Initalization*/
    m_oBsp.gpioInitialization();
    /*to know device is rebooted*/
    m_oConfig.m_u8IsReboot = 1;
    m_oBsp.hooterInit();
    allRpcInit();
    /*I2C initialization*/
    m_oBsp.i2cInitialization();
    /*Rfid initialization I2c mode*/
    m_oRfid.init();
    debugPrintln("initialization completed :-)");
    /*Iniatlization for application timer*/
    AppTimer.attach(0.1, +[](cApplication *App){ App->AppTimerHandler100ms(); },this);

    char mac_id[20] = {0};
    String macID = WiFi.macAddress();
    // String macID = "90:15:06:E4:47:44";
    safeStrcpy(mac_id, String(macID).c_str(), sizeof(mac_id));
    char FV[10];
    String firmVersion = String(FW_VERSION)+"."+String(BOARD_VERSION) + "." + "0";

    safeStrcpy(m_oDisp.DisplayGeneralVariables.FirmwareVersion, String(firmVersion).c_str(), sizeof(m_oDisp.DisplayGeneralVariables.FirmwareVersion));
    safeStrcpy(m_oDisp.DisplayFooterData.DeviceMac, mac_id, sizeof(m_oDisp.DisplayFooterData.DeviceMac));
    delay(200);
    m_oDisp.ClearDisplay();
    if (digitalRead(BSP_BTN_1) == LOW) {
        m_oDisp.DisplayGeneralVariables.IsSensorConnected = true;
        currentScreen = 2; // Enter Config Mode
        debugPrintln("Button held at boot → Config Mode");
    } else {
        debugPrintln("Entering normal mode");
        m_oDisp.defaultDisplay();
        delay(5000);
        currentScreen = 1;
        m_oDisp.ClearDisplay();
    }
    listSPIFFSFiles();
    attachInterrupt(digitalPinToInterrupt(BSP_BTN_1), handleButtonInterrupt, CHANGE);
    debugPrint("millis before the button detection ");debugPrintln(millis());
    return 1;
}

/*****************************************************************
* Below lines of code belongs to Smart Config Page redirect
******************************************************************/
// Generates SSID from MAC (correct order)
String cApplication::getAPSSIDFromMAC(void) {
    uint8_t mac[6];
    WiFi.macAddress(mac);
    char macStr[13];
    sprintf(macStr, "%02X%02X%02X%02X%02X%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    return "NextAqua-" + String(macStr).substring(8);
  }
  
  // HTML form
  String htmlForm(void) {
    String ssid = m_oMemory.getString("wifiSsid", "Nextaqua_EAP110");
    String pass = m_oMemory.getString("wifiPass", "Infi@2016"); 
    debugPrintln(" Read the ssid and password to show in the html form");
    return R"rawliteral(
  <!DOCTYPE html>
  <html>
  <head>
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <title>NextAqua WiFi Config</title>
    <style>
      body {
        font-family: Arial, sans-serif;
        background-color: white;
        color: black;
        text-align: center;
        padding: 20px;
      }
      .form-container {
        background-color: #f5f5dc; /* beige */
        padding: 20px;
        border-radius: 12px;
        max-width: 320px;
        margin: auto;
        box-shadow: 0 0 10px rgba(0,0,0,0.1);
      }
      h1 {
        margin-top: 20px;
        font-size: 24px;
        color: black;
      }
      input[type="text"], input[type="password"] {
        padding: 10px;
        margin: 10px auto;
        width: 90%;
        border: 1px solid #ccc;
        border-radius: 8px;
        font-size: 16px;
        display: block;
      }
      .password-container {
        position: relative;
      }
      .eye-icon {
        position: absolute;
        right: 20px;
        top: 12px;
        cursor: pointer;
        font-size: 18px;
      }
      .error-msg {
        color: red;
        font-size: 14px;
        margin-top: 5px;
      }
      input[type="submit"] {
        background-color: #007BFF;
        color: white;
        padding: 10px 20px;
        border: none;
        border-radius: 20px;
        font-weight: bold;
        font-size: 16px;
        margin-top: 10px;
        cursor: pointer;
      }
      .nextaqua-logo {
        display: flex;
        justify-content: center;
        align-items: center;
        font-size: 32px;
        font-weight: bold;
      }
      .logo-text {
        color: black;
      }
      .logo-aqua {
        color: #007BFF;
      }
      .symbol {
        display: flex;
        flex-direction: row;
        margin-left: 10px;
      }
       .column {
          display: flex;
          flex-direction: column;
          margin: 0 3px;
        }
        .column.middle {
          margin-top: 10px;
        }
        .column.single {
          margin-top: 20px;
        }
        .dot {
          width: 6px;
          height: 6px;
          background-color: #007BFF;
          border-radius: 50%;
          margin: 2px 0;
        }
  
    </style>
  </head>
  <body>
    <div class="nextaqua-logo">
      <span class="logo-text">NEXT</span><span class="logo-aqua">AQUA</span>
      <div class="symbol">
          <div class="column">
            <div class="dot"></div>
            <div class="dot"></div>
            <div class="dot"></div>
            <div class="dot"></div>
            <div class="dot"></div>
          </div>
          <div class="column middle">
            <div class="dot"></div>
            <div class="dot"></div>
            <div class="dot"></div>
          </div>
          <div class="column single">
            <div class="dot"></div>
          </div>
        </div>
      </div>
    <h1>Set WiFi Credentials</h1>
    <div class="form-container">
      <form id="wifiForm" action="/save" method="POST" onsubmit="return validateForm()">
        <input type="text" name="ssid" id="ssid" placeholder="WiFi SSID"><br>
        <div class="password-container">
          <input type="password" name="password" id="password" placeholder="Password">
          <span class="eye-icon" onclick="togglePassword()">Show</span>
        </div>
        <div id="error" class="error-msg"></div>
        <input type="submit" value="Save & Reboot">
      </form>
      <p style="margin-top:10px;">Current SSID: <strong>)rawliteral" + ssid + R"rawliteral(</strong></p>
      <p>Password: <strong>)rawliteral" + pass + R"rawliteral(</strong></p>
    </div>
  
    <script>
      function togglePassword() {
        var x = document.getElementById("password");
        x.type = x.type === "password" ? "text" : "password";
      }
      function validateForm() {
        let ssid = document.getElementById("ssid").value;
        let pass = document.getElementById("password").value;
        let error = document.getElementById("error");
        error.innerHTML = "";
        if (ssid === "" || pass === "") {
          error.innerHTML = "Please fill in both SSID and Password";
          return false;
        }
        if (pass.length < 8) {
          error.innerHTML = "Password must be at least 8 characters long";
          return false;
        }
        return true;
      }
    </script>
  </body>
  </html>
  )rawliteral";
 }

/*****************************************************************************************************
 * Function to start access Point with SSID 
 * ************************************************************************************************* */
void cApplication::startAccessPoint() {
  Myname = getAPSSIDFromMAC();
  IPAddress apIP(192, 168, 4, 1);
  IPAddress subnet(255, 255, 255, 0);

  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(apIP, apIP, subnet);
  WiFi.softAP(Myname.c_str(), MyPassKey.c_str());

  dnsServer.start(DNS_PORT, "*", apIP);

  debugPrintln("AP Mode started.");
  Serial.print("SSID: ");
  Serial.println(Myname);
  debugPrintln("Go to http://192.168.4.1 or connect to WiFi for redirect");
}

/***************************************************************************************************************
 * Function to handle we server events 
***************************************************************************************************************/
void cApplication::startWebServer() {
  server.on("/", HTTP_GET, []() {
    server.send(200, "text/html", htmlForm());
  });

  server.on("/save", HTTP_POST, []() {
    NewSsid = server.arg("ssid");
    NewPassword = server.arg("password");

    IsRecievedConfig = true;
    reBoot = millis();
    
    m_oMemory.putString("wifiSsid", NewSsid);
    m_oMemory.putString("wifiPass", NewPassword);

    server.sendHeader("Location", "/done", true);
    server.send(302, "text/plain", "");
  });
  server.on("/done", HTTP_GET, []() {
    String html = "<html><head><meta name='viewport' content='width=device-width, initial-scale=1'>"
                  "<style>body { background:white; text-align:center; font-size:20px; padding-top:50px; }"
                  ".tick { font-size:48px; color:green; }</style></head><body>"
                  "<div class='tick'>&#10004;</div>"
                  "<h2>Saved! Rebooting...</h2>"
                  "<p>SSID: <strong>" + NewSsid + "</strong></p>"
                  "<p>Password: <strong>" + NewPassword + "</strong></p>"
                  "</body></html>";
    server.send(200, "text/html", html);
  });
    // Captive portal auto-redirect routes
  server.on("/generate_204", HTTP_GET, []() {
    server.sendHeader("Location", "/", true);
    server.send(302, "text/plain", "");
  });
  server.on("/hotspot-detect.html", HTTP_GET, []() {
    server.sendHeader("Location", "/", true);
    server.send(302, "text/plain", ""); 
  });
  server.onNotFound([]() {
    server.send(200, "text/html", htmlForm());
  });
  server.begin();
  debugPrintln("Web server started.");
}
  
/************************************************************************************
 * When to enter smart Config, if button is pressed for 10 seconds long, 
 * if m_bSmartConfig is true then ESP will enter the smart config mode
 ************************************************************************************/
void cApplication::SmartConfig(void){
    SmartConfigTimeoutMillis = millis();//Timeout to restart the device after entering smart config
    WiFi.disconnect();
    startAccessPoint();
    startWebServer();
}
  
/****************************************************************************************
 * Function runs for every 10ms to handle the smart Config and restart the device after 
 ***************************************************************************************/
void cApplication::SmartConfigTask(void)
{
    if (m_oDisp.m_bSmartConfigMode)
    {
        if(!GoToSmartConfig)
        {
            SmartConfig();
            GoToSmartConfig = true;
        }
        server.handleClient();
        dnsServer.processNextRequest();
        /*Display smart config screens before and after configuration*/
        if (!IsRecievedConfig) {
            static int screen1 = 0;
            if(screen1 < 1){
                delay(500);
                m_oDisp.DisplaySmartConfig(Myname, MyPassKey);
                screen1 = 1;
            }
        } else {
            static int y = 0;
            y++;
            if(y >= 100)
            {
                m_oDisp.DisplaySaveSmartConfig(NewSsid, NewPassword);
            }   
        }
    }
    //Restart the device, if credentials are set
    if (IsRecievedConfig && (millis() - reBoot)/1000 >= 5) {
        debugPrintln(" Reboot time completed, i am restarting the device...........");
        ESP.restart();
    }
    //Restart the device if the smart config timeout is greater than 10 mins
    if(m_oDisp.m_bSmartConfigMode && (millis() - SmartConfigTimeoutMillis)/1000 >= 600)
    {
        debugPrintln("[ERROR]:Smart config timeout exceeded, device will be restarted");
        ESP.restart();
    }
}