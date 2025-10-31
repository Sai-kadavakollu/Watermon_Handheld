#ifndef RPC_HANDLERS_H
#define RPC_HANDLERS_H

#include <Arduino.h>
#include <mjson.h>
#include "CApplication.h"  // For struct definitions
#include "http_ops.h"      // For http_device structure

// Forward declarations for external objects and variables
extern class cBsp m_oBsp;
extern class cPCF85063A m_oRtc;
extern class FILESYSTEM m_oFileSystem;
extern class CBackupStorage m_oBackupStore;
extern class CDeviceConfig m_oConfig;
extern struct http_device g_http_dev; // HTTP device with ops structure (C-style)
extern class CSensor m_oSensor;
extern class CGps m_oGps;
extern class CDisplay m_oDisp;
extern class Preferences m_oMemory;
extern class CPondConfig m_oPondConfig;

// External shared variables
extern SemaphoreHandle_t xSharedVarMutex;

// New grouped structs (Phase 3 optimization)
extern AppState g_appState;
extern AppTimers g_timers;
extern AppConfig g_config;
extern SensorData g_sensorData;
extern CurrentPondInfo g_currentPond;
extern SmartConfigData g_smartConfig;

// Legacy variables (kept for backward compatibility during migration)
extern bool m_bGetConfig;
extern bool m_bSendframe;
extern bool isOnline;
extern bool RTCSyncNow;
extern bool ResetEntireMap;
extern bool pingNow;
extern bool m_bDoFota;
extern int sendFrameType;
extern int rebootAfterSetDataCmd;
extern int m_iOperationMode;
extern int m_iDataFrequencyInterval;
extern uint8_t MorningPondMapResetTime;
extern uint8_t EveningPondMapResetTime;
extern int TotalMinsOffSet;
extern bool Is_Simulated_Lat_Longs;
extern double SimulatedLat;
extern double SimulatedLongs;
extern char m_cWifiSsid[20];
extern char m_cWifiPass[20];
extern char sendResult[4000];

// External display variables
extern char CurrntPondName[20];
extern char CurrentLocationId[100];
extern char CurrentPondID[100];
extern float DoMglValue;
extern float TempVal;
extern float DoSaturationVal;
extern float CurrentPondSalinity;

// Utility functions
void safeStrcpy(char *destination, const char *source, int sizeofDest);

// Helper functions for thread-safe access
void setSharedFlag(bool &flag, bool value);
bool getSharedFlag(bool &flag);
void setSharedInt(int &var, int value);
int getSharedInt(int &var);

// RPC Handler function declarations
void RPChandler_setCalValues(struct jsonrpc_request *r);
void RPChandler_setSalinity(struct jsonrpc_request *r);
void RPChandler_setOperationMode(struct jsonrpc_request *r);
void RPChandler_setInterval(struct jsonrpc_request *r);
void RPChandler_getSalinity(struct jsonrpc_request *r);
void RPChandler_getPressure(struct jsonrpc_request *r);
void RPChandler_setPressure(struct jsonrpc_request *r);
void RPChandler_getCalValues(struct jsonrpc_request *r);
void RPChandler_runSafeMode(struct jsonrpc_request *r);
void RPChandler_whoAreYou(struct jsonrpc_request *r);
void RPChandler_syncRTC(struct jsonrpc_request *r);
void RPChandler_ResetPondStatus(struct jsonrpc_request *r);
void RPChandler_SetPondMapResetTime(struct jsonrpc_request *r);
void RPChandler_setLocalTimeOffset(struct jsonrpc_request *r);
void RPChandler_updateConfig(struct jsonrpc_request *r);
void RPChandler_getConfigIDs(struct jsonrpc_request *r);
void RPChandler_ClearBackupFiles(struct jsonrpc_request *r);
void RPChandler_getFileList(struct jsonrpc_request *r);
void RPChandler_getFileContent(struct jsonrpc_request *r);
void RPChandler_deleteFile(struct jsonrpc_request *r);
void RPChandler_firmwareUpdate(struct jsonrpc_request *r);
void RPChandler_refreshFrame(struct jsonrpc_request *r);
void RPChandler_sysReboot(struct jsonrpc_request *r);
void RPChandler_setWifiCredentials(struct jsonrpc_request *r);
void RPChandler_setServerCredntials(struct jsonrpc_request *r);
void RPChandler_ClearNonBackupFiles(struct jsonrpc_request *r);
void RPChandler_SimulatedPosts(struct jsonrpc_request *r);

// RPC initialization function
void initializeAllRPCHandlers(void);

#endif // RPC_HANDLERS_H
