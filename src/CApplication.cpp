#include "CApplication.h"
#include <WiFi.h>
#include <sstream>
#include <mjson.h>
#include <WebSocketsClient_Generic.h>
#include <SocketIOclient_Generic.h>
#include <WebServer.h>
#include <DNSServer.h>
#include "RPCHandlers.h"
#include "http_ops.h"

#define SERIAL_DEBUG
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
struct http_device g_http_dev; // HTTP device with ops structure (C-style)
CSensor m_oSensor;
CGps m_oGps;
CDisplay m_oDisp;
Preferences m_oMemory;
Geofence m_oGeofence;
CPondConfig m_oPondConfig(&m_oFileSystem);
volatile ButtonState_t ButtonState;

/*Smart Config Objects and variables BEGIN*/
WebServer server(80);
DNSServer dnsServer;
SmartConfigData g_smartConfig;
const byte DNS_PORT = 53;

// Mutex for protecting shared variables accessed by multiple tasks
SemaphoreHandle_t xSharedVarMutex = NULL;

// Grouped application state - replaces scattered bool flags
AppState g_appState;
// Application timers and counters
AppTimers g_timers;
// Application configuration
AppConfig g_config;
// Sensor data struct
SensorData g_sensorData;
// Current pond information struct
CurrentPondInfo g_currentPond;

int LoadedPondsWhileCheckingCurrentPond = 0;

char sendResult[4000];
char timebuffer[6];

double SimulatedLat = 0.00000;
double SimulatedLongs = 0.00000;
bool Is_Simulated_Lat_Longs = false;

// Global variables accessed by RPC handlers
int sendFrameType = NO_FRAME;
bool pingNow = false;
int rebootAfterSetDataCmd = -1;

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
void safeStrcpy(char *destination, const char *source, int sizeofDest)
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
    
    // Protect shared variables with mutex
    if (xSharedVarMutex != NULL && xSemaphoreTake(xSharedVarMutex, portMAX_DELAY) == pdTRUE)
    {
        g_appState.getConfig = true;
        g_appState.isOnline = true;
        pingNow = true;
        xSemaphoreGive(xSharedVarMutex);
    }

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
    
    // Protect shared variables with mutex
    if (xSharedVarMutex != NULL && xSemaphoreTake(xSharedVarMutex, portMAX_DELAY) == pdTRUE)
    {
        g_appState.isOnline = false;
        xSemaphoreGive(xSharedVarMutex);
    }

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

// RPC handlers moved to RPCHandlers.cpp

/****************************************************************************************
 * Function to intialize rpc Function Handlers and mdash begin with wifi
 * @param [in] None
 * @param [out] None
 ***************************************************************************************/
void cApplication::wifiInitialization(void)
{
    init_wifi(m_cWifiSsid, m_cWifiPass);
}

void allRpcInit(void)
{
    debugPrintln("@@ [INIT] Registering RPC handlers");
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
    jsonrpc_export("FOTA", RPChandler_firmwareUpdate);
    jsonrpc_export("getLiveFrame", RPChandler_refreshFrame);
    jsonrpc_export("sysReboot", RPChandler_sysReboot);
    jsonrpc_export("setDataFrequency", RPChandler_setInterval);
    jsonrpc_export("ResetPondStatus", RPChandler_ResetPondStatus);
    jsonrpc_export("SetPondMapResetTime", RPChandler_SetPondMapResetTime);
    jsonrpc_export("ClearNonBackupFiles", RPChandler_ClearNonBackupFiles);
    jsonrpc_export("SimulatePosts", RPChandler_SimulatedPosts);
    jsonrpc_export("getFileList", RPChandler_getFileList);
    jsonrpc_export("getFileContent", RPChandler_getFileContent);
    jsonrpc_export("deleteFile", RPChandler_deleteFile);
    jsonrpc_export("updateConfig", RPChandler_updateConfig);
    jsonrpc_export("ActivateSafeMode", RPChandler_runSafeMode);
    debugPrintln("@@ [INIT] RPC handlers registered");
}

static int sender(const char *frame, int frame_len, void *privdata)
{
    debugPrintf("@@ [SENDER] Called with frame_len=%d\n", frame_len);
    
    // Calculate current length and remaining space
    size_t currentLen = strlen(sendResult);
    size_t remainingSpace = sizeof(sendResult) - currentLen - 1; // -1 for null terminator
    
    debugPrintf("@@ [SENDER] currentLen=%d, remainingSpace=%d\n", currentLen, remainingSpace);
    
    // Only append if there's enough space
    if (frame_len <= remainingSpace)
    {
        strncat(sendResult, frame, frame_len);
        debugPrintln("@@ [SENDER] Frame appended successfully");
    }
    else
    {
        // Append what we can and log overflow
        strncat(sendResult, frame, remainingSpace);
        debugPrintf("@@ [SENDER] Buffer overflow! Needed %d bytes, had %d\n", frame_len, remainingSpace);
    }
    
    debugPrintln("@@ [SENDER] Returning");
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

        debugPrint("@@ [RPC] Complete RPC Object: ");
        debugPrintln(serializedRpcObject);

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
        m_oDisp.DisplayFooterData.isWebScoketsConnected = false;
        break;

    case sIOtype_CONNECT:
        debugPrint("[IOc] Connected to url: ");
        debugPrintln((char *)payload);
        // join default namespace (no auto join in Socket.IO V3)
        socketIO.send(sIOtype_CONNECT, "/");
        m_oDisp.DisplayFooterData.isWebScoketsConnected = true;
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
    m_oMemory.putString("serverIP", g_http_dev.default_server_ip);
    delay(1000);
    ESP.restart();
}
void IRAM_ATTR handleButtonInterrupt()
{
    static bool lastPhysicalState = HIGH;
    bool currentPhysicalState = digitalRead(BSP_BTN_1);

    if (currentPhysicalState != lastPhysicalState)
    {
        unsigned long now = millis();

        if (currentPhysicalState == LOW)
        {
            ButtonState.buttonReleased = false;
            ButtonState.buttonPressedMillis = now;
        }
        else
        {
            ButtonState.buttonReleased = true;
            ButtonState.buttonChanged = true; // Signal main loop to evaluate
        }
        lastPhysicalState = currentPhysicalState;
    }
}

void cApplication::CheckForButtonEvent()
{
    static bool isButtonPressed = false;
    static bool backupViewerJustEntered = false;
    static unsigned long countdownStartTime = 0; // Store countdown start time

    unsigned long now = millis();

    // Countdown handler - update timer continuously
    if (isButtonPressed)
    {
        g_timers.countDownTimer = (now - countdownStartTime) / 1000;
        m_oDisp.DisplayGeneralVariables.Counter = g_timers.countDownTimer;
    }
    // debugPrintf(" CountDown: %d \n", g_timers.countDownTimer);

    if (g_timers.countDownTimer >= TIMER_COUNTDOWN)
    {
        g_timers.countDownTimer = 0;
        isButtonPressed = false;
        m_oDisp.DisplayGeneralVariables.Counter = 0; // Reset counter display
        buzz = 10;
        sendFrameType = VDIFF_FRAME;
        m_oDisp.PopUpDisplayData.UploadStatus = NO_FRAME_IN_PROCESS;
        debugPrintln("Generated Frame");
    }

    // --- Detect button press events while holding (no release required) ---
    // Ignore button presses during countdown
    if (!ButtonState.buttonReleased && !isButtonPressed)
    { // button still pressed and countdown not active
        unsigned long heldTime = now - ButtonState.buttonPressedMillis;
        
        // 10-second press for backup viewer (only from main screen)
        if (heldTime > 10000 && currentScreen == 1 && lastButtonEvent != VERY_LONG_PRESS_DETECTED)
        {
            lastButtonEvent = VERY_LONG_PRESS_DETECTED;
            debugPrintln("VERY_LONG_PRESS_DETECTED - Entering Backup Viewer");
            m_oDisp.DisplayGeneralVariables.lastButtonEvent = lastButtonEvent;
            
            // Switch to backup viewer screen
            currentScreen = 3;
            backupViewerJustEntered = true; // Mark that we just entered
            
            // Load backup data only once
            if (!m_oDisp.backupDataLoaded)
            {
                m_oDisp.totalBackupEntries = m_oBackupStore.loadAllBackupEntries(
                    &m_oFileSystem, 
                    m_oDisp.backupEntries, 
                    50
                );
                m_oDisp.backupDataLoaded = true;
                m_oDisp.backupScrollIndex = 0;
                debugPrintf("Loaded %d backup entries\n", m_oDisp.totalBackupEntries);
            }
            
            buzz = 10; // Feedback beep
            m_oDisp.ClearDisplay();
        }
        // 2-second press for config mode or exit backup viewer
        else if (heldTime > 2000 && lastButtonEvent != SHORT_PRESS_DETECTED && !backupViewerJustEntered)
        {
            lastButtonEvent = SHORT_PRESS_DETECTED;
            debugPrintln("SHORT_PRESS_DETECTED (while holding)");
            m_oDisp.DisplayGeneralVariables.lastButtonEvent = lastButtonEvent;
            
            // Exit backup viewer if currently in it
            if (currentScreen == 3)
            {
                currentScreen = 1;
                m_oDisp.resetBackupViewerScreen(); // Reset screen state
                m_oDisp.forceMainScreenRefresh(); // Force complete refresh of main screen
                m_oDisp.ClearDisplay();
                debugPrintln("Exiting Backup Viewer");
                buzz = 5;
            }
        }
    }
    
    // Reset the flag when button is released
    if (ButtonState.buttonReleased)
    {
        backupViewerJustEntered = false;
    }

    // --- Detect JUST_PRESSED on release ---
    // Only process button release if countdown is not active
    if (ButtonState.buttonChanged && ButtonState.buttonReleased && !isButtonPressed)
    {
        ButtonState.buttonChanged = false;

        unsigned long pressDuration = now - ButtonState.buttonPressedMillis;

        if (pressDuration < 2000)
        {
            lastButtonEvent = JUST_PRESSED;
            debugPrintln("JUST_PRESSED (on release)");
            
            // Start countdown only for JUST_PRESSED on main screen
            if (currentScreen == 1)
            {
                isButtonPressed = true;
                countdownStartTime = now; // Store countdown start time
                buzz = 5;
                debugPrintln(" Button Pressed CountDown Start");
                m_oDisp.PopUpDisplayData.UploadStatus = FRAME_CAPTURE_COUNTDOWN;
            }
        }
        else
        {
            lastButtonEvent = BUTTON_NONE; // already handled as SHORT_PRESS while holding
        }

        m_oDisp.DisplayGeneralVariables.lastButtonEvent = lastButtonEvent;
    }
    else if (ButtonState.buttonChanged && ButtonState.buttonReleased && isButtonPressed)
    {
        // Button released during countdown - ignore it completely
        ButtonState.buttonChanged = false;
        lastButtonEvent = BUTTON_NONE; // Don't set JUST_PRESSED
        m_oDisp.DisplayGeneralVariables.lastButtonEvent = BUTTON_NONE;
        debugPrintln("Button press ignored - countdown active");
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
    return http_upload_ping_frame(&g_http_dev);
}

/*******************************************************************************
 * Update pop up display
 *******************************************************************************/
void cApplication::updatePopUpDisplay(uint8_t uploadStatus, const char *timeStr, const char *pondName, float doValue)
{
    if (m_oDisp.PopUpDisplayData.UploadStatus != FRAME_CAPTURE_COUNTDOWN) m_oDisp.PopUpDisplayData.UploadStatus = uploadStatus;
    safeStrcpy(m_oDisp.PopUpDisplayData.time, timeStr, sizeof(m_oDisp.PopUpDisplayData.time));
    safeStrcpy(m_oDisp.PopUpDisplayData.pName, pondName, sizeof(m_oDisp.PopUpDisplayData.pName));
    m_oDisp.PopUpDisplayData.doValue = doValue;
}

/***********************************************************
 *   Send data from backup storage periodically if available
 ************************************************************/
void cApplication::uploadframeFromBackUp(void)
{
    if (g_appState.isOnline)
    {
        if (g_http_dev.is_busy)
        {
            debugPrintln("httpBusy :-(");
            return;
        }
        if (!g_http_dev.is_connected || pingNow)
        {
            pingNow = false;
            debugPrintln("Trying ping in BAK");
            g_timers.pingEpoch = SendPing();
            return;
        }
        if (m_oBackupStore.available())
        {
            char fdata[1300] = {0};
            debugPrintln("files in Backup Available..");
            m_oBackupStore.readFromBS(&m_oFileSystem, fdata);

            // Function to read pondName and timebuffer from the frame and print on display till the frame is sent
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
            if (http_upload_data_frame(&g_http_dev, Data)) //
            {
                updatePopUpDisplay(BACKUP_FRAME_UPLOAD_SUCCESS, doc["timeBuffer"], doc["PondName"], doc["do"]);
                m_oBackupStore.moveToNextFile(&m_oFileSystem);

                if (doc["DataError"] != PONDMAP_VALUE_TAKEN_BUT_ERROR && (doc["PondName"][0] != '\0'))
                {
                    m_oPondConfig.updatePondStatus(doc["PondName"], PONDMAP_VALUE_FRAME_SENT_SUCESSFULLY);
                }
            }
            else
            {
                updatePopUpDisplay(BACKUP_FRAME_UPLOAD_FAIL, doc["timeBuffer"], doc["PondName"], doc["do"]);
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
        g_appState.rtcSyncNow = true;
        g_timers.pingEpoch = SendPing();
        m_iFrameInProcess = NO_FRAME;
        sendFrameType = NO_FRAME;
        m_oDisp.PopUpDisplayData.UploadStatus = (g_appState.isGPS) ? FRAME_GEN_FAILED : FRAME_GEN_FAILED_NO_GPS;
        return;
    }

    DynamicJsonDocument Data(1400);
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
    Data["operationMode"] = g_config.operationMode;  // Using struct
    Data["lat"] = m_oGps.mPosition.m_lat;
    Data["lng"] = m_oGps.mPosition.m_lng;
    Data["HDop"] = m_oGps.mPosition.hDop;
    Data["Satellites"] = m_oGps.mPosition.m_iSatellites;
    Data["IsGpsValid"] = m_oGps.m_bIsValid;
    Data["rfId"] = "NO RFID";
    Data["PondName"] = g_currentPond.CurrentPondName;  // Using struct
    Data["pondId"] = g_currentPond.CurrentPondID;  // Using struct
    Data["locationId"] = g_currentPond.CurrentLocationId;  // Using struct
    Data["localOffsetTimeInMin"] = g_config.totalMinsOffSet;  // Using struct
    Data["do"] = roundToDecimals(g_sensorData.doMglValue, 5);  // Using struct
    Data["temp"] = g_sensorData.tempVal;  // Using struct
    Data["saturationPCT"] = roundToDecimals(g_sensorData.doSaturationVal, 5);  // Using struct
    Data["salinity"] = g_currentPond.CurrentPondSalinity;  // Using struct
    Data["BatPercent"] = m_oDisp.DisplayHeaderData.batteryPercentage;
    Data["isHistory"] = LIVE_FRAME;
    Data["Nearest"] = getNearestPondString();
    Data["timeBuffer"] = timebuffer;
    Data["UpTime"] = millis()/1000;
    Data["LstPNameChkTime"] = g_timers.lastPondNameCheckEpoch;  // Using struct
    Data["PNameCheckingCntr"] = LoadedPondsWhileCheckingCurrentPond;
    /*****************************************************/

    /*CHeck Here whether the DO value have any error or not*/
    if ((Data["do"] <= 0 || (Data["temp"] <= 10 || Data["temp"] >= 55)) && (Data["PondName"][0] != '\0'))
    {
        m_oPondConfig.updatePondStatus(Data["PondName"], PONDMAP_VALUE_TAKEN_BUT_ERROR);
        Data["DataError"] = PONDMAP_VALUE_TAKEN_BUT_ERROR;
    }

    debugPrintln(" Document ready with data");
    /*Try to send frame if device is online or save to backup memory*/
    char frame[1400];
    if (g_appState.isOnline)
    {
        serializeJson(Data, frame);
        debugPrint(frame);
        if (!http_upload_data_frame(&g_http_dev, frame))
        {
            Data["isHistory"] = HISTORY_FRAME;
            serializeJson(Data, frame);
            m_oBackupStore.writeInBS(&m_oFileSystem, frame);

            updatePopUpDisplay(FRAME_UPLOAD_FAIL, Data["timeBuffer"], Data["PondName"], Data["do"]);
            if ((Data["DataError"] != PONDMAP_VALUE_TAKEN_BUT_ERROR) && (Data["PondName"][0] != '\0'))
            {
                m_oPondConfig.updatePondStatus(Data["PondName"], PONDMAP_VALUE_FRAME_STORED_TO_BACKUP);
            }
        }
        else
        {
            updatePopUpDisplay(FRAME_UPLOAD_SUCCESS, Data["timeBuffer"], Data["PondName"], Data["do"]);
            debugPrintln("[App][updateJsonAndSendFrame][uploadDataFrame][Success]");
            if (Data["DataError"] != PONDMAP_VALUE_TAKEN_BUT_ERROR && (Data["PondName"][0] != '\0'))
            {
                m_oPondConfig.updatePondStatus(Data["PondName"], PONDMAP_VALUE_FRAME_SENT_SUCESSFULLY);
            }
        }
    }
    else
    {
        Data["isHistory"] = HISTORY_FRAME;
        serializeJson(Data, frame);
        m_oBackupStore.writeInBS(&m_oFileSystem, frame);

        updatePopUpDisplay(FRAME_UPLOAD_FAIL_NO_INTERNET, Data["timeBuffer"], Data["PondName"], Data["do"]);
        if ((Data["DataError"] != PONDMAP_VALUE_TAKEN_BUT_ERROR) && (Data["PondName"][0] != '\0'))
        {
            m_oPondConfig.updatePondStatus(Data["PondName"], PONDMAP_VALUE_FRAME_STORED_TO_BACKUP);
        }
    }

    sendFrameType = NO_FRAME;
    /*Reset Timeout frame counter, Push Timeout Frame*/
    m_iFrameInProcess = NO_FRAME;
    g_appState.foundPondName = false;
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
    if (g_appState.isOnline && g_http_dev.is_connected)
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
//     if(g_appState.isGPS)
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
    int gpsMin = m_oGps.GpsMins;

    // Convert GPS to total minutes
    int totalMinutes = gpsHour * 60 + gpsMin;

    // Apply offset
    totalMinutes += TimeInMinsOffset;

    // Normalize to 0–1439
    totalMinutes = (totalMinutes % (24 * 60) + (24 * 60)) % (24 * 60);

    int hour = totalMinutes / 60;
    int minutes = totalMinutes % 60;

    // Convert to 12-hour format (no AM/PM)
    if (hour == 0)
    {
        hour = 12; // midnight → 12
    }
    else if (hour > 12)
    {
        hour -= 12; // 13–23 → 1–11
    }

    if (g_appState.isGPS)
    {
        snprintf(timebuffer, sizeof(timebuffer), "%02d:%02d", hour, minutes);
    }
    else
    {
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
    m_oDisp.DisplayHeaderData.batteryPercentage = constrain(mapFloatToInt(voltage, 2.8, 4.2, 0, 100), 0, 100);
    /*check whether the power supply is connected or not*/
    g_appState.isCharging = (m_oBsp.ioPinRead(BSP_PWR_DET)) ? true : false;
}

int cApplication::mapFloatToInt(float x, float in_min, float in_max, int out_min, int out_max)
{
    return (int)((x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min);
}

/*Reset the pond backup status map mornign adn evening*/
void cApplication::ResetPondBackupStatusMap(int day, int hour)
{
    int TimeInHoursWithOffset = hour + (g_config.totalMinsOffSet / 60);
    // Morning task (2:00 AM)
    if (!g_appState.morningCheckedThisBoot && TimeInHoursWithOffset >= g_config.morningPondMapResetTime && (day != g_config.lastMorningDay))
    {
        m_oMemory.putInt("morningDay", day);
        g_config.lastMorningDay = day;
        g_appState.morningCheckedThisBoot = true;
        debugPrintln(" Resetting the ponds status as time is 2AM");
        g_appState.resetEntireMap = true;
    }
    // Evening task (2:00 PM)
    if (!g_appState.eveningCheckedThisBoot && TimeInHoursWithOffset >= g_appState.eveningCheckedThisBoot && (day != g_config.lastEveningDay))
    {
        m_oMemory.putInt("eveDay", day);
        g_config.lastEveningDay = day;
        g_appState.eveningCheckedThisBoot = true;
        debugPrintln(" Resetting the ponds status as time is 4PM");
        g_appState.resetEntireMap = true;
    }
}

/**************************************************
 *   Function to complete cApplication related tasks
 **************************************************/
void cApplication::applicationTask(void)
{
    if (m_oDisp.m_bSmartConfigMode)
        return;
    m_oBsp.wdtfeed();
    socketIO.loop();

    if (g_appState.doFota)
        return;
    /* Update the display every 100millisecond*/
    RunDisplay();
    CheckForButtonEvent();

    if (getSharedFlag(g_appState.sendFrame))
    {
        debugPrintln("@@ [APP] sendFrame flag detected - preparing RPC response");
        debugPrint("@@ [APP] sendResult content: ");
        debugPrintln(sendResult);
        
        String output = String("[\"rpcr\"," + String(sendResult) + "]");
        
        debugPrintln("@@ [APP] Sending RPC response via socketIO");
        // Send event
        socketIO.sendEVENT(output);
        
        debugPrintln("@@ [APP] RPC response sent");
        // Print the serialized JSON for debugging
        debugPrintln(output);
        
        debugPrintln("@@ [APP] Clearing sendResult buffer");
        memset(sendResult, 0, sizeof(sendResult));
        
        debugPrintln("@@ [APP] Clearing sendFrame flag");
        setSharedFlag(g_appState.sendFrame, false);
        
        debugPrintln("@@ [APP] RPC response handling complete");
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
        convertTime(g_config.totalMinsOffSet);
        m_oDisp.DisplayGeneralVariables.backUpFramesCnt = m_oBackupStore.countStoredFiles(&m_oFileSystem);

        static int sec5timer = 0;
        sec5timer++;

        if (sec5timer >= 5 && (g_config.operationMode == EVENT_BASED_MODE) && !g_appState.foundPondName && (g_appState.isGPS || Is_Simulated_Lat_Longs))
        {
            // for (const auto &pair : m_oPondConfig.m_pondStatusMap) {
            //     Serial.printf("Pond: %s, isBoundariesAvailable: %d, BackupState: %d\n", pair.first.c_str(), pair.second.isBoundariesAvailable,pair.second.PondDataStatus);
            // }

            long st = millis();
            GetCurrentPondName();
            g_timers.lastPondNameCheckEpoch = m_oConfig.m_tEpoch;
            debugPrint(" TIme taken to get all the ponds data: ");debugPrintln(millis() - st);
            sec5timer = 0;
            Serial.print(" Nearest Ponds: ");Serial.println(getNearestPondString());
        }

        m_u8AppConter1Sec = 0;
        g_timers.timeOutFrameCounter++;

        checkBattteryVoltage();
        GetPondBoundaries();

        /*Read Time from GPS*/
        if (!Is_Simulated_Lat_Longs)
            m_oConfig.m_tEpoch = m_oGps.Epoch;
        if ((g_appState.isOnline == false) || (g_http_dev.is_connected == false))
        {
            g_timers.rebootAfterOfflineCnt++;
            g_timers.netCheckCounter++;
        }
        else
        {
            g_timers.rebootAfterOfflineCnt = 0;
            g_timers.netCheckCounter = 0;
        }

        if (g_appState.isGPS)
            ResetPondBackupStatusMap(m_oGps.GpsDay, m_oGps.GpsHour);
        /*Reset the entire map*/
        if (g_appState.resetEntireMap)
        {
            Serial.println("[DEBUG] g_appState.resetEntireMap flag detected → resetting...");
            g_appState.resetEntireMap = false;
            m_oPondConfig.resetAllPondDataStatus();
        }
    }
    ResetHandler();
}

void cApplication::GpsTask(void)
{
    m_oBsp.wdtfeed();
    /* Run the GPS function very second to encode lats, longs */
    m_oGps.gpstask();
}

/*****************************************************************************************************
 *Function to check my current coordinates with the coords present in the pondBoundaries and
  get the current pondName if the distance between me and the coords is less than 1 metre
  And set the current pond salinity to the sensor
 ******************************************************************************************************/
void cApplication::GetCurrentPondName(void)
{
    int cntr = 0;
    allPondsWithDistance.clear();
    /*set the current location coordinates*/
    if (Is_Simulated_Lat_Longs)
    {
        m_oGps.mPosition.m_lat = SimulatedLat;
        m_oGps.mPosition.m_lng = SimulatedLongs;
    }

    m_oPosition location = {m_oGps.mPosition.m_lat, m_oGps.mPosition.m_lng};
    debugPrintf("lat: %f, lng : %f\n", location.m_lat, location.m_lng);
    int len = m_oPondConfig.m_u8TotalNoOfPonds;
    debugPrintf("location versions size : %d \n", len);
    /*read the data from the locationID file*/
    for (int i = 0; i < len; i++)
    {
        char fileName[20] = "";
        snprintf(fileName, sizeof(fileName), "/%s.txt", m_oPondConfig.m_oPondList[i].m_cPondname);
        debugPrintf("Current FileName to check distance: %s\n", fileName);
        int distanceFromNearestPond = readPondNameFromFile(fileName, location);
        if(distanceFromNearestPond >= 0)
        {
            cntr++;
            updateAllPondsDistance(m_oPondConfig.m_oPondList[i].m_cPondname, distanceFromNearestPond);
        }
    }
    finalizeNearestPonds();
    LoadedPondsWhileCheckingCurrentPond = cntr;
}

/*******************************************************************************************************************************
 * Function to get the pond boundaries whenever there is version change i.e change of some pond details or a new pond is added
 ********************************************************************************************************************************/
void cApplication::GetPondBoundaries(void)
{
    if (m_oPondConfig.m_bGetPondBoundaries && g_http_dev.is_connected)
    {
        int length = m_oPondConfig.updatedPondIds.size();
        if (!length)
            m_oPondConfig.m_bGetPondBoundaries = false;

        for (auto it = m_oPondConfig.updatedPondIds.begin(); it != m_oPondConfig.updatedPondIds.end(); /* no increment here */)
        {
            debugPrintf("PondId: %s, PondName: %s\n", it->first.c_str(), it->second.c_str());
            int response = getConfigurationPondBoundaries(it->first.c_str(), it->second.c_str());
            m_oDisp.printSavingCoordinates();
            if (response)
            {
                m_oPondConfig.m_pondStatusMap[it->second.c_str()].isBoundariesAvailable = AVAILABLE;
                m_oPondConfig.m_pondStatusMap[it->second.c_str()].PondDataStatus = m_oPondConfig.m_pondStatusMap[it->second.c_str()].isActive;
                // Erase the pair and move the iterator to the next element
                it = m_oPondConfig.updatedPondIds.erase(it);
            }
            else
            {
                // Only increment the iterator if no deletion was made
                ++it;
            }
        }
        m_oDisp.m_bRefreshRightPanel = true;
        m_oPondConfig.savePondStatusToFile();
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

void cApplication::AssignDataToDisplayStructs()
{
    safeStrcpy(m_oDisp.DisplayLeftPanelData.pName, g_currentPond.CurrentPondName, sizeof(m_oDisp.DisplayLeftPanelData.pName));  // Using struct
    safeStrcpy(m_oDisp.DisplayLeftPanelData.nearestPonds, String(getNearestPondString()).c_str(), sizeof(m_oDisp.DisplayLeftPanelData.nearestPonds));
    m_oDisp.DisplayHeaderData.Satellites = m_oGps.mPosition.m_iSatellites;
    m_oDisp.DisplayHeaderData.rssi = WiFi.RSSI();
    /* Check whether the GPS Coordinates are found or not*/
    // g_appState.isGPS = ((m_oGps.mPosition.m_lat != 0.0) && (m_oGps.mPosition.m_lng != 0.0)) ? true : false;
    // if (Is_Simulated_Lat_Longs)
    //     g_appState.isGPS = ((SimulatedLat != 0.0) && (SimulatedLongs != 0.0)) ? true : false;
    g_appState.isGPS = m_oGps.m_bIsValid;
    m_oDisp.DisplayHeaderData.LocationStatus = g_appState.isGPS;
    m_oDisp.DisplayFooterData.isHttpConnected = g_http_dev.is_connected;
    /*Footer Data*/
    m_oDisp.DisplayFooterData.FooterType = m_oDisp.PopUpDisplayData.UploadStatus; // TODO : to handle the display updation by comparing the structure

    strncpy(m_oDisp.DisplayFooterData.RouterMac, String(WiFi.BSSIDstr()).c_str(), sizeof(m_oDisp.DisplayFooterData.RouterMac));
}

/****************************************************************************************************
 * Function to run the display and handle send frame
 ****************************************************************************************************/
void cApplication::RunDisplay(void)
{
    AssignDataToDisplayStructs();
    unsigned long st = millis();
    m_oDisp.renderDisplay(currentScreen, &m_oPondConfig);
}

// TODO: seperate function for GPS and read the values every single time and update the gloabal variables in app.cpp instead from gps.cpp
void convertEpoch(time_t epoch)
{
    struct tm *timeinfo = localtime(&epoch); // or gmtime(&epoch) for UTC

    int hour = timeinfo->tm_hour;
    int minute = timeinfo->tm_min;
    int second = timeinfo->tm_sec;

    int day = timeinfo->tm_mday;
    int month = timeinfo->tm_mon + 1;    // tm_mon is 0-based
    int year = timeinfo->tm_year + 1900; // tm_year is years since 1900

    debugPrintf("Time: %02d:%02d:%02d  Date: %02d-%02d-%04d\n",
                hour, minute, second, day, month, year);
}

void printSystemInfo()
{
    // 🟢 Heap info
    size_t freeHeap = ESP.getFreeHeap();
    size_t minFreeHeap = ESP.getMinFreeHeap(); // lowest recorded free heap
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
    size_t used = SPIFFS.usedBytes();
    Serial.printf("SPIFFS: Total=%u, Used=%u, Free=%u bytes\n", total, used, total - used);

    Serial.println("-----------------------------------");
}

/****************************************************************************************************
 * Function to parse the commands from sensor comm:modbus RTU, when it is not in calibration mode
 ****************************************************************************************************/
void cApplication::commandParseTask(void)
{
    if (g_appState.doFota)
    {
        m_oDisp.printFOTA(g_http_dev.curr_progress);
        return;
    }
    // printSystemInfo();
    m_oBsp.wdtfeed();
    /*function to read the DO and Temp values only when not setting the calibration values to the sensor*/
    if (!m_oDisp.StopReadingSensor)
    {
        m_oSensor.getTempAndDoValues();
    }
    else if (m_oDisp.StopReadingSensor)
    {
        m_oDisp.m_bCalibrationResponse = (m_oSensor.setCalibrationValues() ? 1 : (m_oSensor.setCalibrationValues() ? 1 : 0));
        debugPrintf(" Calibration response values: %d \n", m_oDisp.m_bCalibrationResponse);
    }

    // Track sensor communication status
    static int sensorFailCntr = 0;

    // Count consecutive sensor communication failures
    if (m_oSensor.noSensor)
    {
        sensorFailCntr++;
    }
    else
    {
        sensorFailCntr = 0;
    }

    // Only update working values if we have valid sensor readings
    // Ignore zero values from single communication failures
    bool validSensorData = (m_oSensor.m_fDo > 0 && m_oSensor.m_fTemp > 0 && !m_oSensor.noSensor && (m_oSensor.m_fDoMgl >= -1.0 && m_oSensor.m_fDoMgl <= 25.0));

    if (validSensorData)
    {
        // Update sensor data struct
        g_sensorData.doMglValue = m_oSensor.m_fDoMgl;
        g_sensorData.doSaturationVal = m_oSensor.m_fDo;
        g_sensorData.tempVal = m_oSensor.m_fTemp;

        // Update display
        m_oDisp.DisplayLeftPanelData.DoSaturationValue = roundToDecimals(m_oSensor.m_fDo, 2);
        m_oDisp.DisplayLeftPanelData.DoValueMgL = roundToDecimals(m_oSensor.m_fDoMgl, 2);
        m_oDisp.DisplayLeftPanelData.TempValue = roundToDecimals(m_oSensor.m_fTemp, 1);
        m_oDisp.DisplayLeftPanelData.Salinity = m_oSensor.m_fSalinity;
    }

    // Reset working values when sensor has been disconnected for too long, this prevents loss of valid data due to temporary communication issues
    if (sensorFailCntr >= 5)
    {
        // debugPrintln(" [App][commandParseTask] Sensor disconnected - resetting working values");
        m_oDisp.DisplayGeneralVariables.IsSensorConnected = true;
        
        // Reset sensor data struct
        g_sensorData.tempVal = 0.0;
        g_sensorData.doSaturationVal = 0.0;
        g_sensorData.doMglValue = 0.0;
        
        // Reset display
        m_oDisp.DisplayLeftPanelData.TempValue = 0.0;
        m_oDisp.DisplayLeftPanelData.DoSaturationValue = 0;
        m_oDisp.DisplayLeftPanelData.DoValueMgL = 0;
        m_oDisp.DisplayLeftPanelData.Salinity = 0.0;
        sensorFailCntr = 0;
    }
    else
    {
        m_oDisp.DisplayGeneralVariables.IsSensorConnected = false;
    }
}

// -----------------------------------------------------
// Function: updateAllPondsDistance
// Purpose : Add each pond’s distance to the list
// -----------------------------------------------------
void cApplication::updateAllPondsDistance(const char *pondName, int distance)
{
    if (distance < 0.0f || distance > NEAREST_POND_MAX_VALUE)
        return;

    PondDistance p;
    strncpy(p.name, pondName, sizeof(p.name));
    p.name[sizeof(p.name) - 1] = '\0';
    p.distance = distance;

    allPondsWithDistance.push_back(p);
}

// -----------------------------------------------------
// Purpose : Sort ponds and detect current pond (if distance is less than the Inside the pond tolerance value)
// -----------------------------------------------------
void cApplication::finalizeNearestPonds()
{
    if (allPondsWithDistance.empty())
        return;

    // Sort by ascending distance
    std::sort(allPondsWithDistance.begin(), allPondsWithDistance.end(),
              [](const PondDistance &a, const PondDistance &b)
              { return a.distance < b.distance; });

    // Check if the device is inside a pond
    if (allPondsWithDistance[0].distance <= INSIDE_POND_TOLERANCE)
    {
        // Update pond info struct
        strncpy(g_currentPond.CurrentPondName, allPondsWithDistance[0].name, sizeof(g_currentPond.CurrentPondName));
        Serial.print("Current Pond Name: "); Serial.println(g_currentPond.CurrentPondName);
        g_currentPond.CurrentPondName[sizeof(g_currentPond.CurrentPondName) - 1] = '\0';

        for (int j = 0; j < m_oPondConfig.m_u8TotalNoOfPonds; j++)
        {
            if (strcmp(m_oPondConfig.m_oPondList[j].m_cPondname, g_currentPond.CurrentPondName) == 0)
            {
                // Found the pond — copy details to struct
                strcpy(g_currentPond.CurrentPondID, m_oPondConfig.m_oPondList[j].m_cPondId);
                strcpy(g_currentPond.CurrentLocationId, m_oPondConfig.m_oPondList[j].m_cLocationID);
                g_currentPond.CurrentPondSalinity = m_oPondConfig.m_oPondList[j].m_iSalinity;
                m_oSensor.m_fSalinity = g_currentPond.CurrentPondSalinity;
            }
        }
        if (m_oSensor.m_fSalinity)
        {
            int val = m_oSensor.setSalinity();
            if (!val)
                m_oSensor.setSalinity();
        }
    }
    else
    {
        // Clear pond info struct
        g_currentPond.CurrentPondName[0] = '\0';
        strcpy(g_currentPond.CurrentPondName, "");
        strcpy(g_currentPond.CurrentLocationId, "");
        strcpy(g_currentPond.CurrentPondID, "");
        g_currentPond.CurrentPondSalinity = 0;
        m_oSensor.m_fSalinity = 0;
        debugPrintln("There is No pond for the current coordinates");
    }

    // Keep only the N nearest ponds (optional trimming)
    if (allPondsWithDistance.size() > MAX_NEAREST_PONDS)
        allPondsWithDistance.resize(MAX_NEAREST_PONDS);

    // Debug print all ponds after sorting
    // printAllPondsSorted();
}

// -----------------------------------------------------
// Function: printAllPondsSorted
// Purpose : Debug helper - prints all ponds & distances
// -----------------------------------------------------
void cApplication::printAllPondsSorted()
{
    Serial.println("\n--- Sorted Pond Distances ---");
    for (size_t i = 0; i < allPondsWithDistance.size(); ++i)
    {
        Serial.print(i);
        Serial.print(": ");
        Serial.print(allPondsWithDistance[i].name);
        Serial.print(" -> ");
        Serial.print(allPondsWithDistance[i].distance, 3);
        Serial.println(" m");
    }
    Serial.println("-----------------------------\n");
}

// -----------------------------------------------------
// Function: getNearestPondString
// Purpose : Create a user-readable message
// -----------------------------------------------------
String cApplication::getNearestPondString()
{
    if (allPondsWithDistance.empty())
        return "No Ponds in range";

    // Case 1: Inside a pond
    if (strlen(g_currentPond.CurrentPondName) > 0)  // Using struct
    {
        String msg;

        // Find next two nearest ponds (if available)
        if (allPondsWithDistance.size() > 1)
        {
            msg += "Near to ";

            size_t limit = min((size_t)MAX_NEAREST_PONDS, allPondsWithDistance.size());
            for (size_t i = 1; i < limit; ++i)
            {
                msg += allPondsWithDistance[i].name;
                msg += "(";
                msg += String(allPondsWithDistance[i].distance);
                msg += "m)";
                if (i < limit - 1)
                    msg += ", ";
            }
        }
        return msg;
    }

    // Case 2: Not inside any pond
    String msg = "Near to ";
    size_t limit = min((size_t)MAX_NEAREST_PONDS - 1, allPondsWithDistance.size());
    for (size_t i = 0; i < limit; ++i)
    {
        msg += allPondsWithDistance[i].name;
        msg += "(";
        msg += String(allPondsWithDistance[i].distance);
        msg += "m)";
        if (i < limit - 1)
            msg += ", ";
    }

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
                        return value;
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
    return -1;
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
        if (!g_appState.isOnline)
            onConnected();
    }
    else
    {
        if (g_appState.isOnline)
            onDisConnected();
    }
}

/*******************************************************
 *   Function for frame handling related tasks
 *********************************************************/
void cApplication::frameHandlingTask(void)
{
    if (m_oDisp.m_bSmartConfigMode)
        return;

    m_oBsp.wdtfeed();
    /*return from here when fota is running*/
    if (g_appState.doFota)
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

    /*Check for multiple Wifi Networks when device is offline every 15sec*/
    if (g_timers.netCheckCounter > 15)
    {
        g_timers.netCheckCounter = 0;
        reconnectWifi();
    }

    static int cntr = 0;
    if (cntr >= 50 && g_http_dev.is_connected)
    {
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

    if (g_timers.timeOutFrameCounter > 5 * 60 && sendFrameType == NO_FRAME && strcmp(g_currentPond.CurrentPondID, "") == 0)
    {
        debugPrintln("@ generating T T T frame");
        g_timers.timeOutFrameCounter = 0;
        sendFrameType = TOUT_FRAME;
    }

    if (g_appState.getConfig && g_appState.isOnline && g_http_dev.is_connected)
    {
        Serial.println(" Getting configuration initially");
        getConfigurationDeviceId();
    }
}

/***********************************************************
 * Function to get firmware and update device
 * @param [in] None
 * @param [out] None
 ***********************************************************/
void cApplication::fotaTask(void)
{
    if (m_oDisp.m_bSmartConfigMode)
        return;

    m_oBsp.wdtfeed();
    if (g_appState.doFota)
    {
        m_oBsp.wdtfeed();
        sendFrameType = NO_FRAME;
        debugPrintln("Calling performOTA()");
        m_oDisp.ClearDisplay();
        uint8_t u8OtaResponse = http_perform_ota(&g_http_dev, &m_oBsp);
        switch (u8OtaResponse)
        {
        case 0:
            debugPrintln("OTA success");
            break;
        case 5:
            debugPrintln("OTA fail due to http busy, retrying...");
            break;
        default:
            g_appState.doFota = false;
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
    if (m_iRtcSyncCounter >= 600 || g_appState.rtcSyncNow)
    {
        if (g_appState.isOnline)
        {
            debugPrintln("@@ Check and Sync RTC");
            /*Get server epoch in Local time*/
            time_t currentEpoch;
            if (m_oGps.Epoch == 943920000 || m_oGps.Epoch == 0)
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
            g_appState.rtcSyncNow = false;
        }
        else
        {
            m_iRtcSyncCounter = 540;
        }
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
    // String macAdress = "10:06:1C:07:D9:F4";//WARNING
    sprintf(deviceid, "deviceId=%s", String(macAdress).c_str());
    if (http_get_config(&g_http_dev, deviceid))
    {
        const char *payload = http_get_payload(&g_http_dev);
        String responseData = String(payload ? payload : "");
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
                g_config.operationMode = configInfo["operationMode"];
                debugPrintf("@@ operationMode found %d\n", g_config.operationMode);
            }
            else
            {
                g_config.operationMode = EVENT_BASED_MODE;
                debugPrintln("@@ operationMode not found");
            }
            /* Read Pond Setting Version */
            int64_t iVersionInFrame = 0;
            if (configInfo.containsKey("version"))
            {
                iVersionInFrame = configInfo["version"];
                debugPrintln("@@ version found in file");
            }
            else
            {
                iVersionInFrame = 0;
                debugPrintln("@@ version not found in file");
            }
            /***********************************************************************************************
                check for the operation mode in the payload
                if it is EventBasedMode, load the FILE of EventMode config and check for the version
            ************************************************************************************************/
            if (configInfo.containsKey("version"))
            {
                iVersionInFrame = configInfo["version"];
                debugPrintln("EVENT: Obtained Version from payload");
            }
            Serial.print("   Version from payload:   ");
            Serial.print(iVersionInFrame);
            Serial.print("   Version from Saved File:   ");
            Serial.println(m_oPondConfig.m_i64ConfigIdsVersion);
            if (iVersionInFrame != m_oPondConfig.m_i64ConfigIdsVersion)
            {
                /*write to file when version changes*/
                Serial.printf(" Response data length : %d \n", responseData.length());
                if (responseData.length() > 2500)
                {
                    return 0;
                }
                m_oBackupStore.clearNonBackupFiles(&m_oFileSystem);
                m_oFileSystem.writeFile(FILENAME_IDSCONFIG, String(responseData).c_str());
                debugPrintln("@@ file saved in file");
                /*Clear the existing pond status map file in the Filesystem*/
                SPIFFS.remove(PONDS_STATUS_CONFIG);
                m_oPondConfig.m_pondStatusMap.clear();
                /*load location ids from file*/
                m_oPondConfig.loadPondConfig();
                debugPrintln("@@ location ids loaded from file here");
            }
            else
            {
                Serial.println(" Version not changed...");
            }
            g_appState.getConfig = false;
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
    if (http_get_pond_boundaries(&g_http_dev, pondid))
    {
        const char *payload = http_get_payload(&g_http_dev);
        String responseData = String(payload ? payload : "");
        debugPrintln(payload);
        int Size = responseData.length();
        if (Size > 3000)
            return 0;

        /*deserialize the json document*/
        DynamicJsonDocument configInfo(Size * 2);
        DeserializationError err = deserializeJson(configInfo, responseData); /*Deserialize the json document*/
        if (err.code() == DeserializationError::Ok)                           /*check for deserialization error*/
        {
            char fileName[20] = "";
            snprintf(fileName, sizeof(fileName), "/%s.txt", pName);
            debugPrintf("PondName: %s\n", fileName);
            /*write to file when version changes*/
            m_oFileSystem.writeFile(fileName, String(responseData).c_str());
            debugPrintln("@@ file saved in file");
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

/***************************************************************
 * Function to read the wifi configuration from config file
 * @param [in] None
 * @param [out] None
 ***************************************************************/
void cApplication::readDeviceConfig(void)
{
    String sWifiSSID = m_oMemory.getString("wifiSsid", "Nextaqua_EAP110");         // get wifi SSID
    String sWifiPASS = m_oMemory.getString("wifiPass", "Infi@2016");               // get wifi password
    String sSeverIP = m_oMemory.getString("serverIP", g_http_dev.default_server_ip); // get server IP
    g_config.totalMinsOffSet = m_oMemory.getInt("LocalMins", 180);                 // get offset time
    g_config.dataFrequencyInterval = m_oMemory.getInt("interval", 5);              // To post the data that frequently

    safeStrcpy(m_cWifiPass, sWifiPASS.c_str(), sizeof(m_cWifiPass));
    safeStrcpy(m_cWifiSsid, sWifiSSID.c_str(), sizeof(m_cWifiSsid));
    safeStrcpy(g_http_dev.server_ip, sSeverIP.c_str(), sizeof(g_http_dev.server_ip));

    safeStrcpy(m_oDisp.DisplayFooterData.ServerIp, g_http_dev.server_ip, sizeof(m_oDisp.DisplayFooterData.ServerIp));
    safeStrcpy(m_oDisp.DisplayGeneralVariables.WiFiSsid, m_cWifiSsid, sizeof(m_oDisp.DisplayGeneralVariables.WiFiSsid));
    safeStrcpy(m_oDisp.DisplayGeneralVariables.WiFiPass, m_cWifiPass, sizeof(m_oDisp.DisplayGeneralVariables.WiFiPass));

    debugPrint("wifissid : ");
    debugPrintln(m_cWifiSsid);
    debugPrint("wifiPass : ");
    debugPrintln(m_cWifiPass);
    debugPrint("serverIP : ");
    debugPrintln(g_http_dev.server_ip);
    debugPrint("offsetMin : ");
    debugPrintln(g_config.totalMinsOffSet);
    debugPrint("interval : ");
    debugPrintln(g_config.dataFrequencyInterval);

    g_config.lastMorningDay = m_oMemory.getInt("morningDay", -1);
    g_config.lastEveningDay = m_oMemory.getInt("eveDay", -1);

    g_config.morningPondMapResetTime = m_oMemory.getInt("MrngTime", 2);
    g_appState.eveningCheckedThisBoot = m_oMemory.getInt("EvngTime", 14);
    debugPrintf("\n Last Morning Date : %d \n", g_config.lastMorningDay);
    debugPrintf("\n Last Evening Date : %d \n", g_config.lastEveningDay);

    /*load DO Configuration*/
    m_oPondConfig.loadPondConfig();
}

void listSPIFFSFiles()
{
    File root = SPIFFS.open("/");
    if (!root || !root.isDirectory())
    {
        Serial.println("Failed to open SPIFFS root directory!");
        return;
    }

    File file = root.openNextFile();
    while (file)
    {
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
    debugPrint("millis after serial begin: ");
    debugPrintln(millis());
    
    // Create mutex for shared variable protection
    xSharedVarMutex = xSemaphoreCreateMutex();
    if (xSharedVarMutex == NULL)
    {
        debugPrintln("ERROR: Failed to create shared variable mutex!");
    }
    else
    {
        debugPrintln("Shared variable mutex created successfully");
    }
    
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
    m_oGps.gpsInit(&Serial2);
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
    
    /*HTTP device initialization with ops structure - MUST be before readDeviceConfig*/
    if (http_device_init(&g_http_dev, "WATERMON_HTTP", &esp32_http_ops) != 0) {
        debugPrintln("HTTP device init failed!");
    } else {
        debugPrintln("HTTP device initialized with default server");
    }
    
    /*read device memory and load Do config - this will update g_http_dev.server_ip*/
    readDeviceConfig();
    
    /*Wifi initailization */
    wifiInitialization();
    /*Generate the URI path with esp32 MacAddress*/
    sprintf(m_cUriPath, "/socket.io/?deviceId=%s&deviceType=%s&fwVersion=%d&EIO=4", String(WiFi.macAddress()).c_str(), DEVICE_TYPE, FW_VERSION);
    /*Init Socket Connection*/
    socketIO.setReconnectInterval(10000);
    socketIO.setExtraHeaders("Authorization: 1234567890");
    socketIO.begin(g_http_dev.server_ip, g_http_dev.server_port, m_cUriPath, protocol);
    socketIO.onEvent(socketIOEvent);
    /*Bsp GPIO Initalization*/
    m_oBsp.gpioInitialization();
    /*to know device is rebooted*/
    m_oConfig.m_u8IsReboot = 1;
    m_oBsp.hooterInit();
    allRpcInit();
    /*I2C initialization*/
    m_oBsp.i2cInitialization();
    debugPrintln("initialization completed :-)");
    /*Iniatlization for application timer*/
    AppTimer.attach(0.1, +[](cApplication *App)
                         { App->AppTimerHandler100ms(); },
                    this);

    char mac_id[20] = {0};
    String macID = WiFi.macAddress();
    safeStrcpy(mac_id, String(macID).c_str(), sizeof(mac_id));
    char FV[10];
    String firmVersion = String(FW_VERSION) + "." + String(BOARD_VERSION) + "." + "0";
    String FirmwaverVersionForDisplay = "v" + String(FW_VERSION);

    safeStrcpy(m_oDisp.DisplayGeneralVariables.FirmwareVersion, String(firmVersion).c_str(), sizeof(m_oDisp.DisplayGeneralVariables.FirmwareVersion));
    safeStrcpy(m_oDisp.DisplayHeaderData.FWVerison, String(FirmwaverVersionForDisplay).c_str(), sizeof(m_oDisp.DisplayHeaderData.FWVerison));
    safeStrcpy(m_oDisp.DisplayFooterData.DeviceMac, mac_id, sizeof(m_oDisp.DisplayFooterData.DeviceMac));
    delay(200);
    m_oDisp.ClearDisplay();
    if (digitalRead(BSP_BTN_1) == LOW)
    {
        m_oDisp.DisplayGeneralVariables.IsSensorConnected = true;
        currentScreen = 2; // Enter Config Mode
        debugPrintln("Button held at boot → Config Mode");
    }
    else
    {
        debugPrintln("Entering normal mode");
        m_oDisp.defaultDisplay();
        delay(5000);
        currentScreen = 1;
        m_oDisp.ClearDisplay();
    }
    listSPIFFSFiles();
    attachInterrupt(digitalPinToInterrupt(BSP_BTN_1), handleButtonInterrupt, CHANGE);
    debugPrint("millis before the button detection ");
    debugPrintln(millis());
    return 1;
}

/*****************************************************************
 * Below lines of code belongs to Smart Config Page redirect
 ******************************************************************/
// Generates SSID from MAC (correct order)
String cApplication::getAPSSIDFromMAC(void)
{
    uint8_t mac[6];
    WiFi.macAddress(mac);
    char macStr[13];
    sprintf(macStr, "%02X%02X%02X%02X%02X%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    return "NextAqua-" + String(macStr).substring(8);
}

// HTML form
String htmlForm(void)
{
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
      <p style="margin-top:10px;">Current SSID: <strong>)rawliteral" +
           ssid + R"rawliteral(</strong></p>
      <p>Password: <strong>)rawliteral" +
           pass + R"rawliteral(</strong></p>
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
void cApplication::startAccessPoint()
{
    g_smartConfig.myName = getAPSSIDFromMAC();
    IPAddress apIP(192, 168, 4, 1);
    IPAddress subnet(255, 255, 255, 0);

    WiFi.mode(WIFI_AP);
    WiFi.softAPConfig(apIP, apIP, subnet);
    WiFi.softAP(g_smartConfig.myName.c_str(), g_smartConfig.myPassKey.c_str());

    dnsServer.start(DNS_PORT, "*", apIP);

    debugPrintln("AP Mode started.");
    Serial.print("SSID: ");
    Serial.println(g_smartConfig.myName);
    debugPrintln("Go to http://192.168.4.1 or connect to WiFi for redirect");
}

/***************************************************************************************************************
 * Function to handle we server events
 ***************************************************************************************************************/
void cApplication::startWebServer()
{
    server.on("/", HTTP_GET, []()
              { server.send(200, "text/html", htmlForm()); });

    server.on("/save", HTTP_POST, []()
              {
    g_smartConfig.newSsid = server.arg("ssid");
    g_smartConfig.newPassword = server.arg("password");

    g_smartConfig.isReceivedConfig = true;
    g_smartConfig.rebootTime = millis();
    
    m_oMemory.putString("wifiSsid", g_smartConfig.newSsid);
    m_oMemory.putString("wifiPass", g_smartConfig.newPassword);

    server.sendHeader("Location", "/done", true);
    server.send(302, "text/plain", ""); });
    server.on("/done", HTTP_GET, []()
              {
    String html = "<html><head><meta name='viewport' content='width=device-width, initial-scale=1'>"
                  "<style>body { background:white; text-align:center; font-size:20px; padding-top:50px; }"
                  ".tick { font-size:48px; color:green; }</style></head><body>"
                  "<div class='tick'>&#10004;</div>"
                  "<h2>Saved! Rebooting...</h2>"
                  "<p>SSID: <strong>" + g_smartConfig.newSsid + "</strong></p>"
                  "<p>Password: <strong>" + g_smartConfig.newPassword + "</strong></p>"
                  "</body></html>";
    server.send(200, "text/html", html); });
    // Captive portal auto-redirect routes
    server.on("/generate_204", HTTP_GET, []()
              {
    server.sendHeader("Location", "/", true);
    server.send(302, "text/plain", ""); });
    server.on("/hotspot-detect.html", HTTP_GET, []()
              {
    server.sendHeader("Location", "/", true);
    server.send(302, "text/plain", ""); });
    server.onNotFound([]()
                      { server.send(200, "text/html", htmlForm()); });
    server.begin();
    debugPrintln("Web server started.");
}

/************************************************************************************
 * When to enter smart Config, if button is pressed for 10 seconds long,
 * if m_bSmartConfig is true then ESP will enter the smart config mode
 ************************************************************************************/
void cApplication::SmartConfig(void)
{
    g_smartConfig.timeoutMillis = millis(); // Timeout to restart the device after entering smart config
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
        if (!g_smartConfig.GoToSmartConfig)
        {
            SmartConfig();
            g_smartConfig.GoToSmartConfig = true;
        }
        server.handleClient();
        dnsServer.processNextRequest();
        /*Display smart config screens before and after configuration*/
        if (!g_smartConfig.isReceivedConfig)
        {
            static int screen1 = 0;
            if (screen1 < 1)
            {
                delay(500);
                m_oDisp.DisplaySmartConfig(g_smartConfig.myName, g_smartConfig.myPassKey);
                screen1 = 1;
            }
        }
        else
        {
            static int y = 0;
            y++;
            if (y >= 10)
            {
                m_oDisp.DisplaySaveSmartConfig(g_smartConfig.newSsid, g_smartConfig.newPassword);
            }
        }
    }
    // Restart the device, if credentials are set
    if (g_smartConfig.isReceivedConfig && (millis() - g_smartConfig.rebootTime) / 1000 >= 5)
    {
        debugPrintln(" Reboot time completed, i am restarting the device...........");
        ESP.restart();
    }
    // Restart the device if the smart config timeout is greater than 10 mins
    if (m_oDisp.m_bSmartConfigMode && (millis() - g_smartConfig.timeoutMillis) / 1000 >= 600)
    {
        debugPrintln("[ERROR]:Smart config timeout exceeded, device will be restarted");
        ESP.restart();
    }
}
