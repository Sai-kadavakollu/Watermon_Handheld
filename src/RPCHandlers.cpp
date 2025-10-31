#include "RPCHandlers.h"
#include "CApplication.h"
#include "CPondConfig.h"
#include <WiFi.h>
#include <SPIFFS.h>
#include <ArduinoJson.h>

// Debug macros
#define SERIAL_DEBUG
#ifdef SERIAL_DEBUG
#define debugPrint(...) Serial.print(__VA_ARGS__)
#define debugPrintln(...) Serial.println(__VA_ARGS__)
#define debugPrintf(...) Serial.printf(__VA_ARGS__)
#define debugPrintlnf(...) Serial.println(F(__VA_ARGS__))
#else
#define debugPrint(...)
#define debugPrintln(...)
#define debugPrintf(...)
#define debugPrintlnf(...)
#endif
void setSharedFlag(bool &flag, bool value)
{
    // debugPrintf("@@ [MUTEX] setSharedFlag attempting to take mutex, value=%d\n", value);
    if (xSharedVarMutex != NULL && xSemaphoreTake(xSharedVarMutex, portMAX_DELAY) == pdTRUE)
    {
        // debugPrintln("@@ [MUTEX] setSharedFlag mutex acquired");
        flag = value;
        xSemaphoreGive(xSharedVarMutex);
        // debugPrintln("@@ [MUTEX] setSharedFlag mutex released");
    }
    else
    {
        // debugPrintln("@@ [MUTEX] setSharedFlag FAILED to acquire mutex - using fallback");
        flag = value; // Fallback if mutex not initialized
    }
}

bool getSharedFlag(bool &flag)
{
    bool value = false;
    // debugPrintln("@@ [MUTEX] getSharedFlag attempting to take mutex");
    if (xSharedVarMutex != NULL && xSemaphoreTake(xSharedVarMutex, portMAX_DELAY) == pdTRUE)
    {
        // debugPrintln("@@ [MUTEX] getSharedFlag mutex acquired");
        value = flag;
        xSemaphoreGive(xSharedVarMutex);
        // debugPrintf("@@ [MUTEX] getSharedFlag mutex released, value=%d\n", value);
    }
    else
    {
        // debugPrintln("@@ [MUTEX] getSharedFlag FAILED to acquire mutex - using fallback");
        value = flag; // Fallback if mutex not initialized
    }
    return value;
}

void setSharedInt(int &var, int value)
{
    if (xSharedVarMutex != NULL && xSemaphoreTake(xSharedVarMutex, portMAX_DELAY) == pdTRUE)
    {
        var = value;
        xSemaphoreGive(xSharedVarMutex);
    }
    else
    {
        var = value; // Fallback if mutex not initialized
    }
}

int getSharedInt(int &var)
{
    int value = 0;
    if (xSharedVarMutex != NULL && xSemaphoreTake(xSharedVarMutex, portMAX_DELAY) == pdTRUE)
    {
        value = var;
        xSemaphoreGive(xSharedVarMutex);
    }
    else
    {
        value = var; // Fallback if mutex not initialized
    }
    return value;
}

void RPChandler_setCalValues(struct jsonrpc_request *r)
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
    }
    else
    {
        jsonrpc_return_success(r, "{\"statusCode\":300,\"statusMsg\":\"Not Set due to communicaton error.\"}");
    }
    setSharedFlag(g_appState.sendFrame, true);
}

/***********************************************
 *  RPC Function to set the Calibration values to the DO sensor
 *  r-> pointer holds the Item data buffer
 *************************************************/
void RPChandler_setSalinity(struct jsonrpc_request *r)
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
    }
    else
    {
        jsonrpc_return_success(r, "{\"statusCode\":300,\"statusMsg\":\"Not Set due to communicaton error.\"}");
    }
    setSharedFlag(g_appState.sendFrame, true);
}

void RPChandler_setOperationMode(struct jsonrpc_request *r)
{
    char buff[30];
    snprintf(buff, r->params_len + 1, "%S", (wchar_t *)r->params);
    debugPrintln("@@ Inside setOperationMode...");
    debugPrintln(buff);

    double val = -1;
    if (mjson_get_number(r->params, r->params_len, "$.operationMode", &val) != -1)
    {
        if (xSharedVarMutex != NULL && xSemaphoreTake(xSharedVarMutex, portMAX_DELAY) == pdTRUE)
        {
            g_config.operationMode = val;
            xSemaphoreGive(xSharedVarMutex);
        }
        jsonrpc_return_success(r, "{\"statusCode\":200,\"statusMsg\":\"Success.\"}");
        setSharedFlag(g_appState.sendFrame, true);
    }
    else
    {
        jsonrpc_return_success(r, "{\"statusCode\":300,\"statusMsg\":\"Not Set due to communicaton error.\"}");
        setSharedFlag(g_appState.sendFrame, true);
    }
}

void RPChandler_setInterval(struct jsonrpc_request *r)
{
    double interval = 0;
    if (mjson_get_number(r->params, r->params_len, "$.DataFrequencyinMin", &interval))
    {
        debugPrintln("inside setInterval");
        if (xSharedVarMutex != NULL && xSemaphoreTake(xSharedVarMutex, portMAX_DELAY) == pdTRUE)
        {
            g_config.dataFrequencyInterval = interval;
            m_oMemory.putInt("interval", g_config.dataFrequencyInterval);
            xSemaphoreGive(xSharedVarMutex);
        }
    }
    jsonrpc_return_success(r, "{\"statusCode\":200,\"statusMsg\":\"success.\"}");
    setSharedFlag(g_appState.sendFrame, true);
}

void RPChandler_getSalinity(struct jsonrpc_request *r)
{
    char buff[30];
    snprintf(buff, r->params_len + 1, "%S", (wchar_t *)r->params);
    debugPrintln("@@ Inside getSalinity...");
    debugPrintln(buff);

    m_oSensor.getSalinity();
    float salinity = m_oSensor.m_fSalinity;

    jsonrpc_return_success(r, "{\"statusCode\":200,\"statusMsg\":\"Success.\",\"salinity\":\"%s.\"}", String(salinity));
    setSharedFlag(g_appState.sendFrame, true);
}

void RPChandler_getPressure(struct jsonrpc_request *r)
{
    char buff[30];
    snprintf(buff, r->params_len + 1, "%S", (wchar_t *)r->params);
    debugPrintln("@@ Inside getPressure...");
    debugPrintln(buff);

    m_oSensor.getPressure();
    float pressure = m_oSensor.m_fPressure;

    jsonrpc_return_success(r, "{\"statusCode\":200,\"statusMsg\":\"Success.\",\"pressure\":\"%s.\"}", String(pressure));

    setSharedFlag(g_appState.sendFrame, true);
}

/***********************************************
 *  RPC Function to set the Calibration values to the DO sensor
 *  r-> pointer holds the Item data buffer
 *************************************************/
void RPChandler_setPressure(struct jsonrpc_request *r)
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
    }
    else
    {
        jsonrpc_return_success(r, "{\"statusCode\":300,\"statusMsg\":\"Not Set due to communicaton error.\"}");
    }
    setSharedFlag(g_appState.sendFrame, true);
}

/***********************************************
 *  RPC Function to set the Calibration values to the DO sensor
 *  r-> pointer holds the Item data buffer
 *************************************************/
void RPChandler_getCalValues(struct jsonrpc_request *r)
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
    setSharedFlag(g_appState.sendFrame, true);
}
/*****************************************************************
 *  RPC Function to set device in safe mode
 * r-> pointer holds the config data buffer
 * @param [in] None
 * @param [out] Status code with status msg
 ******************************************************************/
void RPChandler_runSafeMode(struct jsonrpc_request *r)
{
    debugPrintln("@@ Inside runSafeMode.....");
    m_oConfig.m_bIsSafeModeOn = true;
    jsonrpc_return_success(r, "{\"statusCode\":200,\"statusMsg\":\"success.\"}");
    setSharedFlag(g_appState.sendFrame, true);
}

/**************************************************************
 * RPC Function to check device info
 * r-> pointer holds the config data buffer
 * @param [in] None
 * @param [out] device Info in Json format
 ***************************************************************/
void RPChandler_whoAreYou(struct jsonrpc_request *r)
{
    debugPrintln("@@ [RPC] whoAreYou START");
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
    doc["long"] = m_oGps.mPosition.m_lng;
    doc["HDop"] = m_oGps.mPosition.hDop;
    doc["Satellite"] = m_oGps.mPosition.m_iSatellites;
    doc["CurrentPondName"] = g_currentPond.CurrentPondName;
    doc["DoMg/l"] = g_sensorData.doMglValue;
    doc["Temp"] = g_sensorData.tempVal;
    doc["Saturation"] = g_sensorData.doSaturationVal;
    doc["Salinity"] = g_currentPond.CurrentPondSalinity;
    doc["localOffsetTimeMin"] = g_config.totalMinsOffSet;
    doc["operationMode"] = g_config.operationMode;
    doc["progress"] = g_http_dev.curr_progress;

    char result[700];
    serializeJson(doc, result);
    debugPrintln(result);
    jsonrpc_return_success(r, "%s", result);
    setSharedFlag(g_appState.sendFrame, true);
}

/**************************************************************
 *  Sync RTC
 ***************************************************************/
void RPChandler_syncRTC(struct jsonrpc_request *r)
{
    if (xSharedVarMutex != NULL && xSemaphoreTake(xSharedVarMutex, portMAX_DELAY) == pdTRUE)
    {
        g_appState.rtcSyncNow = true;
        xSemaphoreGive(xSharedVarMutex);
    }
    jsonrpc_return_success(r, "{\"statusCode\":200,\"statusMsg\":\"success.\"}");
    setSharedFlag(g_appState.sendFrame, true);
}


/**************************************************************
 *  RPC to get the CurrentPond
 ***************************************************************/
void RPChandler_ResetPondStatus(struct jsonrpc_request *r)
{
    Serial.println("Resetting pond status ");
    if (xSharedVarMutex != NULL && xSemaphoreTake(xSharedVarMutex, portMAX_DELAY) == pdTRUE)
    {
        g_appState.resetEntireMap = true;
        xSemaphoreGive(xSharedVarMutex);
    }
    jsonrpc_return_success(r, "{\"statusCode\":200,\"statusMsg\":\"success.\"}");
    setSharedFlag(g_appState.sendFrame, true);
}
/******************************************************************************
 * Set Pond Map reset time to clear the frame saved status on the UI
 {
    "MorningResetTime":2,
    "EveningResetTime":16
 }
*****************************************************************************/
void RPChandler_SetPondMapResetTime(struct jsonrpc_request *r)
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

    g_config.morningPondMapResetTime = ((int)a);
    g_config.eveningPondMapResetTime = ((int)b);

    Serial.println("[RPC][SetPondMapReset]: type conversion done");

    m_oMemory.putInt("MrngTime", g_config.morningPondMapResetTime);
    m_oMemory.putInt("EvngTime", g_config.eveningPondMapResetTime);

    Serial.println("[RPC][SetPondMapReset]: saving to NVS done");

    jsonrpc_return_success(r, "{\"MorningTime\":\"%d\",\"Evening Time\":\"%d\",\"statusCode\":200,\"statusMsg\":\"Success.\"}", g_config.morningPondMapResetTime, g_config.eveningPondMapResetTime);
    setSharedFlag(g_appState.sendFrame, true);
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
void RPChandler_setLocalTimeOffset(struct jsonrpc_request *r)
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
        debugPrint("LOcal time in Mins: ");
        debugPrintln(localTimeMin);
        g_config.totalMinsOffSet = ((int)localTimeMin);

        m_oMemory.putInt("LocalMins", g_config.totalMinsOffSet);

        jsonrpc_return_success(r, "{\"statusCode\":200,\"statusMsg\":\"success.\", \"localTimeMin\":%d}", g_config.totalMinsOffSet);
        setSharedFlag(g_appState.sendFrame, true);
    }
    else
    {
        debugPrintln("Invalid version...");
        jsonrpc_return_success(r, "{\"statusCode\":300,\"statusMsg\":\"Invalid Version.\"}");
        setSharedFlag(g_appState.sendFrame, true);
    }
}

/**********************************************************
 * RPC Function to update configIds file in feeder
 * r-> pointer holds the config data buffer
 * @param [in] None
 * @param [out] Status code with status msg
 ***********************************************************/
void RPChandler_updateConfig(struct jsonrpc_request *r)
{
    debugPrintln("@@ Inside updateConfigIds.....");
    if (xSharedVarMutex != NULL && xSemaphoreTake(xSharedVarMutex, portMAX_DELAY) == pdTRUE)
    {
        g_appState.getConfig = true;
        xSemaphoreGive(xSharedVarMutex);
    }
    jsonrpc_return_success(r, "{\"statusCode\":200,\"statusMsg\":\"Success.\"}");
    setSharedFlag(g_appState.sendFrame, true);
}

/**********************************************************
 * RPC Function to get configIDS file from stater
 * r-> pointer holds the config data buffer
 * @param [in] None
 * @param [out] configIDs file from filesystem on succerss
 ***********************************************************/
void RPChandler_getConfigIDs(struct jsonrpc_request *r)
{
    debugPrintln("@@ Inside getConfigIds.....");
    int FileSize = m_oFileSystem.getFileSize(FILENAME_IDSCONFIG);
    
    // Validate file size
    if (FileSize <= 0 || FileSize > 8192) // Max 8KB for config file
    {
        jsonrpc_return_success(r, "{\"statusCode\":300,\"statusMsg\":\"Invalid file size.\"}");
        setSharedFlag(g_appState.sendFrame, true);
        return;
    }
    
    // Allocate buffer dynamically with size validation
    char *rdata = (char *)malloc(FileSize + 1);
    if (!rdata)
    {
        jsonrpc_return_success(r, "{\"statusCode\":500,\"statusMsg\":\"Memory allocation failed.\"}");
        setSharedFlag(g_appState.sendFrame, true);
        return;
    }
    
    int ret = m_oFileSystem.readFile(FILENAME_IDSCONFIG, rdata);
    if (ret)
    {
        rdata[FileSize] = '\0'; // Ensure null termination
        jsonrpc_return_success(r, "%s", (const char *)rdata);
        setSharedFlag(g_appState.sendFrame, true);
    }
    else
    {
        jsonrpc_return_success(r, "{\"statusCode\":300,\"statusMsg\":\"file Not available.\"}");
        setSharedFlag(g_appState.sendFrame, true);
    }
    
    free(rdata);
}

/**************************************************************
 *   RPC Function to clear Backup files
 *   r-> pointer holds the config data buffer
 ***************************************************************/
void RPChandler_ClearBackupFiles(struct jsonrpc_request *r)
{
    m_oBackupStore.clearAllFiles(&m_oFileSystem);
    jsonrpc_return_success(r, "{\"statusCode\":200,\"statusMsg\":\"success.\"}");
    setSharedFlag(g_appState.sendFrame, true);
}
/**************************************************************
 *   RPC Function to get list of files in filesystem with sizes
 *   r-> pointer holds the config data buffer
 * @param [in] None
 * @param [out] JSON array with file names and sizes
 ***************************************************************/
void RPChandler_getFileList(struct jsonrpc_request *r)
{
    debugPrintln("@@ Inside getFileList.....");

    File root = SPIFFS.open("/");
    if (!root || !root.isDirectory())
    {
        jsonrpc_return_success(r, "{\"statusCode\":500,\"statusMsg\":\"Failed to open filesystem root.\"}");
        setSharedFlag(g_appState.sendFrame, true);
        return;
    }

    DynamicJsonDocument doc(2048);
    JsonArray result = doc.createNestedArray("result");

    File file = root.openNextFile();
    while (file)
    {
        char entry[128];
        snprintf(entry, sizeof(entry), "%s, %d", file.name(), file.size());
        result.add(entry);
        file = root.openNextFile();
    }

    char response[2048];
    serializeJson(doc, response, sizeof(response));

    jsonrpc_return_success(r, "%s", response);
    setSharedFlag(g_appState.sendFrame, true);
}

/**************************************************************
 *   RPC Function to get file contents by filename
 *   r-> pointer holds the config data buffer
 * @param [in] Data: {"filename": "BAK_0.txt"}
 * @param [out] File contents or error message
 ***************************************************************/
void RPChandler_getFileContent(struct jsonrpc_request *r)
{
    debugPrintln("@@ Inside getFileContent.....");

    char filename[64];
    if (mjson_get_string(r->params, r->params_len, "$.filename", filename, sizeof(filename)) == -1)
    {
        jsonrpc_return_success(r, "{\"statusCode\":400,\"statusMsg\":\"Missing filename parameter.\"}");
        setSharedFlag(g_appState.sendFrame, true);
        return;
    }

    // Ensure filename starts with '/'
    char filepath[70];
    if (filename[0] == '/')
    {
        safeStrcpy(filepath, filename, sizeof(filepath));
    }
    else
    {
        snprintf(filepath, sizeof(filepath), "/%s", filename);
    }

    File file = SPIFFS.open(filepath, "r");
    if (!file)
    {
        jsonrpc_return_success(r, "{\"statusCode\":404,\"statusMsg\":\"File not found.\"}");
        setSharedFlag(g_appState.sendFrame, true);
        return;
    }

    size_t fileSize = file.size();
    if (fileSize == 0)
    {
        file.close();
        jsonrpc_return_success(r, "{\"statusCode\":200,\"statusMsg\":\"Success.\",\"content\":\"\"}");
        setSharedFlag(g_appState.sendFrame, true);
        return;
    }

    // Allocate buffer for file content
    char *content = (char *)malloc(fileSize + 1);
    if (!content)
    {
        file.close();
        jsonrpc_return_success(r, "{\"statusCode\":500,\"statusMsg\":\"Memory allocation failed.\"}");
        setSharedFlag(g_appState.sendFrame, true);
        return;
    }

    size_t bytesRead = file.readBytes(content, fileSize);
    content[bytesRead] = '\0';
    file.close();

    // Create response with content
    DynamicJsonDocument doc(fileSize + 256);
    doc["statusCode"] = 200;
    doc["statusMsg"] = "Success";
    doc["filename"] = filename;
    doc["size"] = bytesRead;
    doc["content"] = content;

    char *response = (char *)malloc(fileSize + 300);
    if (response)
    {
        serializeJson(doc, response, fileSize + 300);
        jsonrpc_return_success(r, "%s", response);
        free(response);
    }
    else
    {
        jsonrpc_return_success(r, "{\"statusCode\":500,\"statusMsg\":\"Response allocation failed.\"}");
    }

    free(content);
    setSharedFlag(g_appState.sendFrame, true);
}
/**************************************************************
 *   RPC Function to delete a file from filesystem
 *   r-> pointer holds the config data buffer
 * @param [in] Data: {"filename": "P1.txt"}
 * @param [out] Status code with status message
 ***************************************************************/
void RPChandler_deleteFile(struct jsonrpc_request *r)
{
    debugPrintln("@@ Inside deleteFile.....");

    char filename[64];
    if (mjson_get_string(r->params, r->params_len, "$.filename", filename, sizeof(filename)) == -1)
    {
        jsonrpc_return_success(r, "{\"statusCode\":400,\"statusMsg\":\"Missing filename parameter.\"}");
        setSharedFlag(g_appState.sendFrame, true);
        return;
    }

    // Ensure filename starts with '/'
    char filepath[70];
    if (filename[0] == '/')
    {
        safeStrcpy(filepath, filename, sizeof(filepath));
    }
    else
    {
        snprintf(filepath, sizeof(filepath), "/%s", filename);
    }

    // Check if file exists
    if (!SPIFFS.exists(filepath))
    {
        jsonrpc_return_success(r, "{\"statusCode\":404,\"statusMsg\":\"File not found.\"}");
        setSharedFlag(g_appState.sendFrame, true);
        return;
    }

    // Delete the file
    if (SPIFFS.remove(filepath))
    {
        debugPrintf("File deleted: %s\n", filepath);
        jsonrpc_return_success(r, "{\"statusCode\":200,\"statusMsg\":\"File deleted successfully.\"}");
    }
    else
    {
        debugPrintf("Failed to delete file: %s\n", filepath);
        jsonrpc_return_success(r, "{\"statusCode\":500,\"statusMsg\":\"Failed to delete file.\"}");
    }

    setSharedFlag(g_appState.sendFrame, true);
}
/**********************************************************
 * RPC Function to update firmware using elagent ota
 * r-> pointer holds the config data buffer
 * @param [in] None
 * @param [out] Status code with status msg
 ***********************************************************/
void RPChandler_firmwareUpdate(struct jsonrpc_request *r)
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
            g_appState.doFota = true;
            g_http_dev.curr_progress = 0;
            safeStrcpy(g_http_dev.uri_firmware_fota, Id, sizeof(g_http_dev.uri_firmware_fota));
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
    setSharedFlag(g_appState.sendFrame, true);
}

/**************************************************************
 * RPC Function to get CALL_FRAME
 * r-> pointer holds the config data buffer
 * @param [in] None
 * @param [out] Status code with status msg
 ***************************************************************/
void RPChandler_refreshFrame(struct jsonrpc_request *r)
{
    debugPrintln("@@ Inside refreshFrame.....");
    if (xSharedVarMutex != NULL && xSemaphoreTake(xSharedVarMutex, portMAX_DELAY) == pdTRUE)
    {
        sendFrameType = CALL_FRAME;
        xSemaphoreGive(xSharedVarMutex);
    }
    jsonrpc_return_success(r, "{\"statusCode\":200,\"statusMsg\":\"Live frame sent success\"}");
    setSharedFlag(g_appState.sendFrame, true);
}

void RPChandler_sysReboot(struct jsonrpc_request *r)
{
    debugPrintln("@@ Inside sysReboot.....");
    if (xSharedVarMutex != NULL && xSemaphoreTake(xSharedVarMutex, portMAX_DELAY) == pdTRUE)
    {
        rebootAfterSetDataCmd = 100;
        xSemaphoreGive(xSharedVarMutex);
    }
    jsonrpc_return_success(r, "{\"statusCode\":200,\"statusMsg\":\"Live frame sent success\"}");
    setSharedFlag(g_appState.sendFrame, true);
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
void RPChandler_setWifiCredentials(struct jsonrpc_request *r)
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
        setSharedFlag(g_appState.sendFrame, true);
    }
    else
    {
        debugPrintln("Invalid version...");
        jsonrpc_return_success(r, "{\"wifiSsid\":\"%s\",\"wifiPass\":\"%s\",\"statusCode\":300,\"statusMsg\":\"Invalid Version.\"}", m_cWifiSsid, m_cWifiPass);
        setSharedFlag(g_appState.sendFrame, true);
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
void RPChandler_setServerCredntials(struct jsonrpc_request *r)
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
            safeStrcpy(g_http_dev.server_ip, IP, sizeof(g_http_dev.server_ip));
            debugPrintln("@@ saving server details");
            m_oMemory.putString("serverIP", g_http_dev.server_ip);
            rebootAfterSetDataCmd = 100;
        }
        jsonrpc_return_success(r, "{\"IP\":\"%s\",\"port\":\"%d\",\"statusCode\":200,\"statusMsg\":\"Success.\"}", g_http_dev.server_ip, g_http_dev.http_port);
        setSharedFlag(g_appState.sendFrame, true);
    }
    else
    {
        debugPrintln("Invalid version...");
        jsonrpc_return_success(r, "{\"IP\":\"%s\",\"port\":\"%d\",\"statusCode\":300,\"statusMsg\":\"Invalid Version.\"}", g_http_dev.server_ip, g_http_dev.http_port);
        setSharedFlag(g_appState.sendFrame, true);
    }
}
/**************************************************************
 *   RPC Function to clear files except the Backup files
 *   r-> pointer holds the config data buffer
 ***************************************************************/
void RPChandler_ClearNonBackupFiles(struct jsonrpc_request *r)
{
    m_oBackupStore.clearNonBackupFiles(&m_oFileSystem);
    if (xSharedVarMutex != NULL && xSemaphoreTake(xSharedVarMutex, portMAX_DELAY) == pdTRUE)
    {
        g_appState.resetEntireMap = true;
        xSemaphoreGive(xSharedVarMutex);
    }
    jsonrpc_return_success(r, "{\"statusCode\":200,\"statusMsg\":\"success.\"}");
    setSharedFlag(g_appState.sendFrame, true);
}

/**********************************************************
 * RPC Function to server credentials like ip and port
 * r-> pointer holds the config data buffer
 * @param [in] Data:
    {
        "Epoch":1795525211,
        "Lat": -2.553424,
        "Long": -80.565472
    }
 * @param [out] Status code with status msg
 ***********************************************************/
void RPChandler_SimulatedPosts(struct jsonrpc_request *r)
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
    setSharedFlag(g_appState.sendFrame, true);
}

/****************************************************************************************
 * RPC Handler Lookup Table (Phase 4 Optimization)
 * Cleaner, more maintainable approach using function pointers
 ***************************************************************************************/
struct RPCHandlerEntry {
    const char* method;
    void (*handler)(struct jsonrpc_request *r);
};

// Centralized RPC handler table - easy to maintain and review
static const RPCHandlerEntry rpcHandlerTable[] = {
    // System & Configuration
    {"ActivateSafeMode", RPChandler_runSafeMode},
    {"wifiConfig", RPChandler_setWifiCredentials},
    {"serverConfig", RPChandler_setServerCredntials},
    {"setLocalTimeOffset", RPChandler_setLocalTimeOffset},
    {"syncRTC", RPChandler_syncRTC},
    {"whoAreYou", RPChandler_whoAreYou},
    {"getConfig", RPChandler_getConfigIDs},
    {"setOperationMode", RPChandler_setOperationMode},
    {"setDataFrequency", RPChandler_setInterval},
    
    // Sensor Operations
    {"setCalValues", RPChandler_setCalValues},
    {"getCalValues", RPChandler_getCalValues},
    {"setSalinity", RPChandler_setSalinity},
    {"getSalinity", RPChandler_getSalinity},
    {"setPressure", RPChandler_setPressure},
    {"getPressure", RPChandler_getPressure},
    
    // Data & Frame Operations
    {"getLiveFrame", RPChandler_refreshFrame},
    {"ClearBackupFiles", RPChandler_ClearBackupFiles},
    {"ClearNonBackupFiles", RPChandler_ClearNonBackupFiles},
    
    // Pond Management
    {"ResetPondStatus", RPChandler_ResetPondStatus},
    {"SetPondMapResetTime", RPChandler_SetPondMapResetTime},
    {"SimulatePosts", RPChandler_SimulatedPosts},
    
    // File Operations
    {"getFileList", RPChandler_getFileList},
    {"getFileContent", RPChandler_getFileContent},
    {"deleteFile", RPChandler_deleteFile},
    
    // System Control
    {"FOTA", RPChandler_firmwareUpdate},
    {"sysReboot", RPChandler_sysReboot},
    
    // Sentinel (marks end of table)
    {nullptr, nullptr}
};

/****************************************************************************************
 * Function to initialize all RPC handlers using the lookup table
 * @param [in] None
 * @param [out] None
 ***************************************************************************************/
void initializeAllRPCHandlers(void)
{
    // Register all handlers from the table
    for (int i = 0; rpcHandlerTable[i].method != nullptr; i++) {
        jsonrpc_export(rpcHandlerTable[i].method, rpcHandlerTable[i].handler);
    }
    
    debugPrintln("RPC Handlers initialized successfully");
}
