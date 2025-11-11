#ifndef APP_H
#define APP_H

#define TOSTR(x) #x
#define STRINGIFY(x) TOSTR(x)

#define FW_VERSION 3
#define BOARD_VERSION 5
#define DEVICE_TYPE "DO"

#include <Ticker.h>
#include "CAT24C32.h"
#include "BSP.h"
#include "CBackupStorage.h"
#include "GPS.h"
#include "cTftDisplay.h"
#include <Preferences.h>
#include "geofence_ops.h"
#include "CPondConfig.h"    
#include "do_sensor_ops.h"

#define MAX_NEAREST_PONDS 3
#define NEAREST_POND_MAX_VALUE 1500
#define INSIDE_POND_TOLERANCE 6

#define NO_FRAME 0
#define TOUT_FRAME 1
#define VDIFF_FRAME 2
#define ACK_FRAME 3
#define CALL_FRAME 4
#define FAULT_FRAME 5
#define TEST_FRAME 6

#define LIVE_FRAME 0
#define HISTORY_FRAME 1

#define SHORT_PRESS 1
#define LONG_PRESS 2
#define VERY_LONG_PRESS 3

#define EVENT_BASED_MODE 1

typedef struct
{
    char name[10];
    int distance;
} PondDistance;

struct ButtonState_t
{
    bool buttonReleased = true;
    unsigned long buttonPressedMillis = 0;
    bool buttonChanged = false;
};

// Grouped application timers and counters
struct AppTimers
{
    int timeOutFrameCounter = 0;
    int netCheckCounter = 0;
    int countDownTimer = 0;
    int rebootAfterOfflineCnt = 0;
    time_t lastPondNameCheckEpoch = 0;
};

struct DeviceConfig
{
    uint8_t m_u8IsReboot;
    int espResetReason;
    time_t m_tEpoch;
    bool m_bIsSafeModeOn;
    time_t pingEpoch = 0;
};

// Application configuration settings
struct AppConfig
{
    int totalMinsOffSet = 180;
    int operationMode = EVENT_BASED_MODE;
    int dataFrequencyInterval = 5;
    uint8_t morningPondMapResetTime = 2;
    uint8_t eveningPondMapResetTime = 14;
    int lastMorningDay = -1;
    int lastEveningDay = -1;
};

// Application state flags
struct AppState
{
    bool isGPS = false;
    bool isCharging = false;
    bool isOnline = false;
    bool getConfig = true;
    bool sendFrame = false;
    bool rtcSyncNow = false;
    bool resetEntireMap = false;
    bool pingNow = false;
    bool doFota = false;
    bool foundPondName = false;
    bool morningCheckedThisBoot = false;
    bool eveningCheckedThisBoot = false;
};

// Sensor readings
struct SensorData
{
    float doMglValue = 0.0;
    float doSaturationVal = 0.0;
    float tempVal = 0.0;
};

// Current pond information
struct CurrentPondInfo
{
    char CurrentPondName[20] = {0};
    char CurrentLocationId[100] = {0};
    char CurrentPondID[100] = {0};
    float CurrentPondSalinity = 0.0;
};

// Smart configuration data
struct SmartConfigData
{
    String myName;
    String newSsid;
    String newPassword;
    String myPassKey = "12345678";
    bool isReceivedConfig = false;
    bool GoToSmartConfig = false;
    unsigned long rebootTime = 0;
    unsigned long timeoutMillis = 0;
};

class cApplication
{
private:
    uint8_t m_u8SendBackUpFrameConter;
    int m_iRtcSyncCounter;
    int m_iFrameInProcess;
    char m_cUriPath[150] = "";
    bool m_bButtonPressed;
    uint8_t buzz = 0;

    uint8_t currentScreen = 1;
    /*Functions*/
    void CheckForButtonEvent(void);
    time_t SendPing(void);
    void uploadframeFromBackUp(void);
    void updateJsonAndSendFrame(void);
    void staLEDHandler(void);
    void AppTimerHandler100ms(void);
    void inActivityChecker(void);
    void checkWifiConnection(void);
    void print_wakeup_reason(void);
    void print_restart_reason(void);
    void wifiInitialization(void);
    void convertTime(int offfsetinMins);
    void operateBuzzer(void);
    uint8_t getConfigurationPondBoundaries(const char *pondID, const char *pName);
    uint8_t getConfigurationDeviceId(void);
    void readDeviceConfig(void);
    void reconnectWifi(void);
    float roundToDecimals(float value, int decimals);
    void checkBattteryVoltage(void);
    int readPondNameFromFile(const char *path, m_oPosition data);
    void RunDisplay(void);
    void ResetWifiCredentials(void);
    void ResetServerCredentials(void);
    void ResetHandler(void);
    void rfidTask(void);
    void CheckAndSyncRTC(void);
    int mapFloatToInt(float x, float in_min, float in_max, int out_min, int out_max);
    void GetPondBoundaries(void);
    String GetPondNameFromRFID(String pondLocationFromCard, const char *jsonString);
    void GetCurrentPondName(void);
    void SmartConfig(void);
    String getAPSSIDFromMAC(void);
    void startAccessPoint(void);
    void startWebServer(void);
    // to save the nearest pond details
    std::vector<PondDistance> allPondsWithDistance;
    void updateAllPondsDistance(const char *pondName, int distance);
    void finalizeNearestPonds();
    String getNearestPondString();
    void AssignDataToDisplayStructs();
    void ResetPondBackupStatusMap(int day, int hour);
    void printAllPondsSorted(void);
    void updatePopUpDisplay(uint8_t uploadStatus, const char* timeStr, const char* pondName, float doValue);

public:
    uint8_t m_u8AppConter1Sec;

    ButtonEvent lastButtonEvent = BUTTON_NONE;

    /*construct*/
    cApplication();
    /*Destruct*/
    ~cApplication();

    /*Functions*/
    int appInit(void);
    void applicationTask(void);
    void frameHandlingTask(void);
    void commandParseTask(void);
    void fotaTask(void);
    void GpsTask(void);
    void SmartConfigTask(void);
    void AppWatchdogInit(TaskHandle_t *taskhandle1, TaskHandle_t *taskhandle2);
    void AppWatchdogInit(TaskHandle_t *taskhandle1, TaskHandle_t *taskhandle2, TaskHandle_t *taskhandle3);
    void AppWatchdogInit(TaskHandle_t *taskhandle1, TaskHandle_t *taskhandle2, TaskHandle_t *taskhandle3, TaskHandle_t *taskhandle4);
    void AppWatchdogInit(TaskHandle_t *taskhandle1, TaskHandle_t *taskhandle2, TaskHandle_t *taskhandle3, TaskHandle_t *taskhandle4, TaskHandle_t *taskhandle5);
};

#endif