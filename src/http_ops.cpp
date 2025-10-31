/**
 * @file http_ops.cpp
 * @brief HTTP Operations Implementation - C-Style with Function Pointer Tables
 * @author Watermon Team
 * @date 2025
 * 
 * @details
 * This file implements HTTP client operations using a C-style approach with 
 * operations structures (similar to Linux kernel device drivers). This pattern
 * separates interface definition from implementation, allowing easy swapping
 * of HTTP implementations without changing application code.
 * 
 * @par Architecture Pattern:
 * - Operations Table (http_ops): Defines function pointers for all HTTP operations
 * - Device Structure (http_device): Holds device state and configuration
 * - Static Implementation Functions: Private functions that implement operations
 * - Public API: Wrapper functions that call through operations table
 * 
 * @par Key Benefits:
 * - No virtual function overhead (direct function pointers)
 * - Clear separation of interface and implementation
 * - Easy to mock for testing
 * - Industry-standard pattern (Linux kernel style)
 * 
 * ============================================================================
 *                           CALL FLOW DIAGRAM
 * ============================================================================
 * 
 * Application Layer (CApplication.cpp)
 *      |
 *      | Calls public API function
 *      v
 * ┌─────────────────────────────────────────────────────────────────┐
 * │  Public API Functions (http_upload_data_frame, etc.)            │
 * │  - Validates parameters                                         │
 * │  - Calls through ops table: http->ops->upload_data_frame()      │
 * └─────────────────────────────────────────────────────────────────┘
 *      |
 *      | Function pointer call
 *      v
 * ┌─────────────────────────────────────────────────────────────────┐
 * │  Operations Table (esp32_http_ops)                              │
 * │  const struct http_ops = {                                      │
 * │    .upload_data_frame = esp32_upload_data_frame,                │
 * │    .upload_ping_frame = esp32_upload_ping_frame,                │
 * │    ...                                                           │
 * │  }                                                               │
 * └─────────────────────────────────────────────────────────────────┘
 *      |
 *      | Resolves to static function
 *      v
 * ┌─────────────────────────────────────────────────────────────────┐
 * │  Static Implementation (esp32_upload_data_frame)                │
 * │  1. Check if busy                                               │
 * │  2. Build HTTP URL                                              │
 * │  3. Set headers (Content-Type, timeout)                         │
 * │  4. Execute HTTP POST/GET                                       │
 * │  5. Parse response                                              │
 * │  6. Store payload                                               │
 * │  7. Return status                                               │
 * └─────────────────────────────────────────────────────────────────┘
 *      |
 *      | Uses helper functions
 *      v
 * ┌─────────────────────────────────────────────────────────────────┐
 * │  Helper Functions                                               │
 * │  - get_priv(): Get private data (HTTPClient instance)           │
 * │  - set_payload(): Store response payload                        │
 * └─────────────────────────────────────────────────────────────────┘
 * 
 * ============================================================================
 *                        INITIALIZATION FLOW
 * ============================================================================
 * 
 * 1. http_device_init(&g_http_dev, "ESP32_HTTP", &esp32_http_ops)
 *    ├─> Allocates esp32_http_priv structure
 *    ├─> Creates HTTPClient and WiFiClient instances
 *    ├─> Sets default server IP and ports
 *    └─> Links ops table to device
 * 
 * 2. http_set_server(&g_http_dev, "34.93.69.40", 3000, 5000)
 *    └─> Configures server IP and ports
 * 
 * 3. Device is ready for operations
 * 
 * ============================================================================
 *                        TYPICAL USAGE EXAMPLE
 * ============================================================================
 * 
 * // Initialize
 * struct http_device g_http_dev;
 * http_device_init(&g_http_dev, "ESP32_HTTP", &esp32_http_ops);
 * http_set_server(&g_http_dev, "34.93.69.40", 3000, 5000);
 * 
 * // Upload data
 * char json_data[1400] = "{\"deviceId\":\"test\",\"do\":5.2}";
 * if (http_upload_data_frame(&g_http_dev, json_data)) {
 *     Serial.println("Upload success");
 * }
 * 
 * // Get server time
 * time_t epoch = http_upload_ping_frame(&g_http_dev);
 * 
 * // Check status
 * if (g_http_dev.is_connected) {
 *     // Server is reachable
 * }
 * 
 * ============================================================================
 */

#include "http_ops.h"
#include "BSP.h"
#include <HTTPClient.h>
#include <Update.h>
#include <WiFi.h>
#include <Arduino.h>
#include "mjson.h"

/* Debug macros - Enable SERIAL_DEBUG for verbose logging */
// #define SERIAL_DEBUG
#ifdef SERIAL_DEBUG
#define debugPrint(...) Serial.print(__VA_ARGS__)
#define debugPrintln(...) Serial.println(__VA_ARGS__)
#define debugPrintf(...) Serial.printf(__VA_ARGS__)
#define debugPrintlnf(...) Serial.println(F(__VA_ARGS__))
#else
#define debugPrint(...)    /* Disabled */
#define debugPrintln(...)  /* Disabled */
#define debugPrintf(...)   /* Disabled */
#define debugPrintlnf(...) /* Disabled */
#endif

/* ============================================================================
 * Private Data Structure for ESP32 HTTP Client
 * ============================================================================ */

struct esp32_http_priv {
    HTTPClient *http_client;
    WiFiClient *ota_client;
};

/* ============================================================================
 * Static Helper Functions
 * ============================================================================ */

/**
 * @brief Get private data from HTTP device
 * @param http Pointer to HTTP device structure
 * @return Pointer to ESP32 HTTP private data structure
 * @note This is a type-safe cast from void* to esp32_http_priv*
 */
static struct esp32_http_priv* get_priv(struct http_device *http)
{
    return (struct esp32_http_priv*)http->priv;
}

/**
 * @brief Store HTTP response payload in device structure
 * @param http Pointer to HTTP device structure
 * @param data Response data string to store (can be NULL)
 * @details
 * - Frees existing payload if present
 * - Allocates new memory for payload
 * - Copies data into allocated memory
 * - Handles NULL data by clearing payload
 */
static void set_payload(struct http_device *http, const char *data)
{
    if (http->payload) {
        free(http->payload);
    }
    
    if (data) {
        http->payload_size = strlen(data) + 1;
        http->payload = (char*)malloc(http->payload_size);
        if (http->payload) {
            strncpy(http->payload, data, http->payload_size);
        }
    } else {
        http->payload = NULL;
        http->payload_size = 0;
    }
}

/* ============================================================================
 * ESP32 HTTP Static Implementation Functions
 * ============================================================================ */

/**
 * @brief Upload data frame to server via HTTP POST
 * @param http Pointer to HTTP device structure
 * @param data JSON data string to upload
 * @return 1 on success, 0 on failure
 * 
 * @details
 * - Endpoint: POST /api/do/createDoReadings
 * - Content-Type: application/json
 * - Timeout: 30 seconds
 * - Checks busy flag before execution
 * - Stores server response in payload
 * 
 * @note This function is thread-safe via is_busy flag
 */
static uint8_t esp32_upload_data_frame(struct http_device *http, char *data)
{
    if (!http || !data) return 0;
    
    struct esp32_http_priv *priv = get_priv(http);
    if (!priv || !priv->http_client) return 0;
    
    uint8_t ret = 0;
    
    if (!http->is_busy) {
        http->is_busy = 1;
        
        debugPrint("[HTTP] IsConnected : ");
        debugPrintln(priv->http_client->connected());
        debugPrintln("[HTTP] begin : Frame");
        debugPrintln(data);
        
        /* Build URL for uploading data */
        char link[150];
        sprintf(link, "http://%s:%d/api/do/createDoReadings", 
                http->server_ip, http->http_port);
        
        priv->http_client->begin(link);
        priv->http_client->addHeader("Content-Type", "application/json");
        priv->http_client->setTimeout(30000);
        
        int httpCode = priv->http_client->POST(data);
        
        debugPrint("httpCode: ");
        debugPrintln(httpCode);
        debugPrintln(priv->http_client->errorToString(httpCode));
        
        if (httpCode == HTTP_CODE_OK) {
            debugPrintln("@@ Uploded to Nextaqua server :-)");
            ret = 1;
        } else {
            debugPrintln("@@ failed to send..:-(");
            ret = 0;
        }
        
        String payload = priv->http_client->getString();
        debugPrintln(payload);
        set_payload(http, payload.c_str());
        
        priv->http_client->end();
    } else {
        ret = 0;
        debugPrintln("@@ HTTP Busy");
    }
    
    debugPrintln("[HTTP] end : Frame");
    http->is_busy = 0;
    return ret;
}

/**
 * @brief Send ping to server and get server epoch time
 * @param http Pointer to HTTP device structure
 * @return Server epoch time in seconds, 0 on failure
 * 
 * @details
 * - Endpoint: GET /ping
 * - Response format: {"statusCode":200,"serverEpoch":1625899085}
 * - Updates is_connected flag based on response
 * - Converts millisecond epoch to seconds
 * 
 * @note Used for RTC synchronization and connectivity check
 */
static time_t esp32_upload_ping_frame(struct http_device *http)
{
    if (!http) return 0;
    
    struct esp32_http_priv *priv = get_priv(http);
    if (!priv || !priv->http_client) return 0;
    
    time_t retEpoch = 0;
    
    if (!http->is_busy) {
        http->is_busy = 1;
        
        debugPrint("[HTTP] IsConnected : Ping");
        debugPrintln(priv->http_client->connected());
        debugPrintln("[HTTP] begin : Ping");
        
        /* Build URL for ping */
        char link[150];
        sprintf(link, "http://%s:%d/ping", http->server_ip, http->http_port);
        
        priv->http_client->begin(link);
        priv->http_client->addHeader("Content-Type", "application/json");
        priv->http_client->setTimeout(30000);
        
        int httpCode = priv->http_client->GET();
        debugPrintln(priv->http_client->errorToString(httpCode));
        
        if (httpCode == HTTP_CODE_OK) {
            debugPrintln("@@ Uploded Ping Frame :-)");
            http->is_connected = 1;
            
            /* Parse response: {"statusCode":200,"statusMessage":"Success","serverEpoch":1625899085} */
            String payload = priv->http_client->getString();
            debugPrintln(payload);
            set_payload(http, payload.c_str());
            
            double num = -1;
            double offset = -1;
            mjson_get_number(payload.c_str(), strlen(payload.c_str()), "$.serverEpoch", &num);
            mjson_get_number(payload.c_str(), strlen(payload.c_str()), "$.offset", &offset);
            
            if (num != -1) {
                /* Convert to local epoch */
                retEpoch = (time_t)(num / 1000);
            }
        } else {
            debugPrintln("@@ failed to send Ping -(");
            http->is_connected = 0;
        }
        
        priv->http_client->end();
    } else {
        debugPrintln("@@ HTTP Busy");
    }
    
    debugPrintln("[HTTP] end : Ping");
    http->is_busy = 0;
    return retEpoch;
}

/**
 * @brief Download device configuration from server
 * @param http Pointer to HTTP device structure
 * @param device_id Query string with device ID (e.g., "deviceId=AA:BB:CC")
 * @return 1 on success, 0 on failure
 * 
 * @details
 * - Endpoint: GET /api/do/getconfiguration?deviceId=...
 * - Response stored in payload for parsing by application
 * - Configuration includes operation mode, intervals, etc.
 */
static uint8_t esp32_get_config(struct http_device *http, char *device_id)
{
    if (!http || !device_id) return 0;
    
    struct esp32_http_priv *priv = get_priv(http);
    if (!priv || !priv->http_client) return 0;
    
    uint8_t ret = 0;
    
    if (!http->is_busy) {
        http->is_busy = 1;
        
        debugPrintln("[HTTP] begin : GetD");
        
        char link[150];
        sprintf(link, "http://%s:%d/api/do/getconfiguration?%s", 
                http->server_ip, http->http_port, device_id);
        
        debugPrintln(link);
        
        priv->http_client->begin(link);
        priv->http_client->setTimeout(30000);
        priv->http_client->addHeader("Content-Type", "application/json");
        
        debugPrintln(device_id);
        
        int httpCode = priv->http_client->GET();
        debugPrintln(priv->http_client->errorToString(httpCode));
        
        if (httpCode == HTTP_CODE_OK) {
            Serial.println("@@ Requested getDevice data :-)");
            String payload = priv->http_client->getString();
            debugPrintln(payload);
            set_payload(http, payload.c_str());
            ret = 1;
        } else {
            Serial.println("@@ failed to send getDevice  :-(");
            ret = 0;
        }
    } else {
        ret = 0;
        Serial.println("@@ HTTP Busy or Not connected");
    }
    
    priv->http_client->end();
    Serial.println("[HTTP] end : GetD");
    http->is_busy = 0;
    return ret;
}

/**
 * @brief Download pond boundary coordinates from server
 * @param http Pointer to HTTP device structure
 * @param query Query string with pond ID (e.g., "pondId=123")
 * @return 1 on success, 0 on failure
 * 
 * @details
 * - Endpoint: GET /api/do/getPondsBoundaries?pondId=...
 * - Response contains GPS coordinates defining pond boundaries
 * - Used for geofencing and pond detection
 */
static uint8_t esp32_get_pond_boundaries(struct http_device *http, char *query)
{
    if (!http || !query) return 0;
    
    struct esp32_http_priv *priv = get_priv(http);
    if (!priv || !priv->http_client) return 0;
    
    uint8_t ret = 0;
    
    if (!http->is_busy) {
        http->is_busy = 1;
        
        debugPrintln("[HTTP] begin : GetD");
        
        char link[150];
        sprintf(link, "http://%s:%d/api/do/getPondsBoundaries?%s", 
                http->server_ip, http->http_port, query);
        
        debugPrintln(link);
        
        priv->http_client->begin(link);
        priv->http_client->setTimeout(30000);
        priv->http_client->addHeader("Content-Type", "application/json");
        
        debugPrintln(query);
        
        int httpCode = priv->http_client->GET();
        debugPrintln(priv->http_client->errorToString(httpCode));
        
        if (httpCode == HTTP_CODE_OK) {
            debugPrintln("@@ Requested get pond Boundaries :-)");
            String payload = priv->http_client->getString();
            debugPrintln(payload);
            set_payload(http, payload.c_str());
            ret = 1;
        } else {
            debugPrintln("@@ failed to get pond Boundaries  :-(");
            ret = 0;
        }
    } else {
        ret = 0;
        debugPrintln("@@ HTTP Busy or Not connected");
    }
    
    priv->http_client->end();
    debugPrintln("[HTTP] end : GetD");
    http->is_busy = 0;
    return ret;
}

/**
 * @brief Perform Over-The-Air (OTA) firmware update
 * @param http Pointer to HTTP device structure
 * @param bsp Pointer to BSP device for watchdog feeding
 * @return Error code: 0=success, 1=HTTP error, 2=no space, 3=update error, 4=not finished, 5=busy
 * 
 * @details
 * - Downloads firmware binary from URL stored in http->uri_firmware_fota
 * - Updates curr_progress field during download (0-100%)
 * - Feeds watchdog during long download to prevent reset
 * - Automatically reboots device on successful update
 * - Uses 2KB buffer for streaming download
 * 
 * @warning Device will reboot automatically on success!
 * @note Progress can be monitored via http->curr_progress
 */
static uint8_t esp32_perform_ota(struct http_device *http, cBsp *bsp)
{
    if (!http || !bsp) return 5;
    
    struct esp32_http_priv *priv = get_priv(http);
    if (!priv || !priv->http_client || !priv->ota_client) return 5;
    
    uint8_t ret = 0;
    
    if (!http->is_busy) {
        http->is_busy = 1;
        
        debugPrint("[HTTP] IsConnected : ");
        debugPrintln(priv->http_client->connected());
        debugPrintln("[HTTP] begin : Frame");
        debugPrintf("Connecting to firmware URL : %s\n", http->uri_firmware_fota);
        
        /* Begin HTTP connection for OTA */
        priv->http_client->begin(*priv->ota_client, http->uri_firmware_fota);
        
        int httpCode = priv->http_client->GET();
        
        if (httpCode == HTTP_CODE_OK) {
            int contentLength = priv->http_client->getSize();
            debugPrintf("Length: %d\n", contentLength);
            
            if (Update.begin(contentLength)) {
                debugPrintln("Begin OTA. This may take a while...");
                
                int written = 0;
                int totalWritten = 0;
                uint8_t buff[2048] = {0};
                
                WiFiClient *stream = priv->http_client->getStreamPtr();
                debugPrint("OTA start :");
                unsigned long st = millis();
                debugPrintln(st);
                
                int prevprogress = 0;
                
                /* Read stream and write to Update object */
                while (priv->http_client->connected() && 
                       (totalWritten < contentLength || contentLength == -1)) {
                    
                    /* Feed watchdog using BSP class method */
                    if (bsp) {
                        bsp->wdtfeed();
                    }
                    
                    size_t size = stream->available();
                    if (size) {
                        int c = stream->readBytes(buff, 
                                  ((size > sizeof(buff)) ? sizeof(buff) : size));
                        written = Update.write(buff, c);
                        totalWritten += written;
                        
                        /* Update progress */
                        prevprogress = http->curr_progress;
                        http->curr_progress = (totalWritten * 100) / contentLength;
                        
                        if (http->curr_progress != prevprogress) {
                            debugPrintf("Progress: %d \n", http->curr_progress);
                        }
                    }
                }
                
                debugPrint("OTA End :");
                unsigned long et = millis();
                debugPrintln(et);
                debugPrint("Duration:");
                debugPrintln(et - st);
                
                if (totalWritten == contentLength) {
                    debugPrintln("Written : " + String(totalWritten) + " successfully");
                } else {
                    debugPrintln("Written only : " + String(totalWritten) + 
                               "/" + String(contentLength) + ". Retry?");
                }
                
                if (Update.end()) {
                    if (Update.isFinished()) {
                        debugPrintln("OTA update has successfully finished. Rebooting...");
                        delay(2000);
                        ESP.restart();
                    } else {
                        debugPrintln("OTA update not finished. Something went wrong!");
                        ret = 4;
                    }
                } else {
                    debugPrintln("Error Occurred. Error #: " + String(Update.getError()));
                    ret = 3;
                }
            } else {
                debugPrintln("Not enough space to begin OTA");
                ret = 2;
            }
        } else {
            debugPrintf("Cannot download firmware. HTTP error code: %d\n", httpCode);
            ret = 1;
        }
        
        priv->http_client->end();
    } else {
        ret = 5;
        debugPrintln("@@ HTTP Busy");
    }
    
    http->is_busy = 0;
    return ret;
}

/* ============================================================================
 * ESP32 HTTP Operations Table (const static struct)
 * ============================================================================ */

/**
 * @brief ESP32 HTTP operations table
 * @details
 * This constant structure contains function pointers to all HTTP operations.
 * It follows the Linux kernel driver pattern where operations are defined
 * in a table and linked to the device at initialization.
 * 
 * @note This is the key to the ops pattern - all implementations are registered here
 */
const struct http_ops esp32_http_ops = {
    .upload_data_frame = esp32_upload_data_frame,
    .upload_ping_frame = esp32_upload_ping_frame,
    .get_config = esp32_get_config,
    .get_pond_boundaries = esp32_get_pond_boundaries,
    .perform_ota = esp32_perform_ota,
};

/* ============================================================================
 * Public API Implementation
 * ============================================================================ */

/**
 * @brief Initialize HTTP device with operations table
 * @param http Pointer to HTTP device structure to initialize
 * @param name Device name string (for logging)
 * @param ops Pointer to operations table (e.g., &esp32_http_ops)
 * @return 0 on success, -1 on failure
 * 
 * @details
 * Initialization steps:
 * 1. Links operations table to device
 * 2. Allocates private data structure
 * 3. Creates HTTPClient and WiFiClient instances
 * 4. Sets default server IP (34.93.69.40) and ports (3000, 5000)
 * 5. Initializes state flags (busy, connected, progress)
 * 
 * @note Must be called before any HTTP operations
 * @see http_set_server() to configure custom server
 */
int http_device_init(struct http_device *http, const char *name, const struct http_ops *ops)
{
    if (!http || !ops) return -1;
    
    http->name = name;
    http->ops = ops;
    http->is_busy = 0;
    http->is_connected = 0;
    http->curr_progress = 0;
    http->payload = NULL;
    http->payload_size = 0;
    
    /* Set default values */
    http->http_port = 3000;
    http->server_port = 5000;
    strncpy(http->default_server_ip, "34.93.69.40", sizeof(http->default_server_ip));
    strncpy(http->server_ip, http->default_server_ip, sizeof(http->server_ip));
    http->uri_firmware_fota[0] = '\0';
    
    /* Allocate and initialize private data */
    struct esp32_http_priv *priv = (struct esp32_http_priv*)malloc(sizeof(struct esp32_http_priv));
    if (!priv) return -1;
    
    priv->http_client = new HTTPClient();
    priv->ota_client = new WiFiClient();
    
    if (!priv->http_client || !priv->ota_client) {
        if (priv->http_client) delete priv->http_client;
        if (priv->ota_client) delete priv->ota_client;
        free(priv);
        return -1;
    }
    
    http->priv = priv;
    
    Serial.printf("[HTTP] Device initialized: %s\n", name);
    return 0;
}

/**
 * @brief Cleanup HTTP device and free resources
 * @param http Pointer to HTTP device structure
 * 
 * @details
 * - Deletes HTTPClient and WiFiClient instances
 * - Frees private data structure
 * - Frees payload buffer
 * - Resets all pointers to NULL
 * 
 * @note Call this before destroying the device structure
 */
void http_device_cleanup(struct http_device *http)
{
    if (!http) return;
    
    struct esp32_http_priv *priv = get_priv(http);
    if (priv) {
        if (priv->http_client) {
            delete priv->http_client;
        }
        if (priv->ota_client) {
            delete priv->ota_client;
        }
        free(priv);
        http->priv = NULL;
    }
    
    if (http->payload) {
        free(http->payload);
        http->payload = NULL;
    }
}

/**
 * @brief Configure server IP address and ports
 * @param http Pointer to HTTP device structure
 * @param ip Server IP address string (e.g., "34.93.69.40")
 * @param http_port HTTP port for REST API (typically 3000)
 * @param server_port WebSocket/Socket.IO port (typically 5000)
 * 
 * @details
 * - Copies IP address to device structure
 * - Updates both HTTP and server ports
 * - Prints configuration to serial for debugging
 * 
 * @note Call after http_device_init() to set custom server
 */
void http_set_server(struct http_device *http, const char *ip, uint16_t http_port, uint16_t server_port)
{
    if (!http || !ip) return;
    
    strncpy(http->server_ip, ip, sizeof(http->server_ip) - 1);
    http->server_ip[sizeof(http->server_ip) - 1] = '\0';
    http->http_port = http_port;
    http->server_port = server_port;
    
    Serial.printf("[HTTP] Server set: %s:%d (server_port:%d)\n", ip, http_port, server_port);
}

/**
 * @brief Upload data frame to server (Public API)
 * @param http Pointer to HTTP device structure
 * @param data JSON data string to upload
 * @return 1 on success, 0 on failure
 * 
 * @details
 * This is a wrapper function that calls through the operations table.
 * It validates parameters and delegates to the implementation function.
 * 
 * @see esp32_upload_data_frame() for implementation details
 */
uint8_t http_upload_data_frame(struct http_device *http, char *data)
{
    if (!http || !http->ops || !http->ops->upload_data_frame) return 0;
    return http->ops->upload_data_frame(http, data);
}

/**
 * @brief Send ping and get server time (Public API)
 * @param http Pointer to HTTP device structure
 * @return Server epoch time in seconds, 0 on failure
 * 
 * @details
 * Wrapper function that calls through operations table.
 * Used for connectivity check and RTC synchronization.
 * 
 * @see esp32_upload_ping_frame() for implementation details
 */
time_t http_upload_ping_frame(struct http_device *http)
{
    if (!http || !http->ops || !http->ops->upload_ping_frame) return 0;
    return http->ops->upload_ping_frame(http);
}

/**
 * @brief Download device configuration (Public API)
 * @param http Pointer to HTTP device structure
 * @param device_id Query string with device ID
 * @return 1 on success, 0 on failure
 * 
 * @details
 * Wrapper function that calls through operations table.
 * Response is stored in payload - use http_get_payload() to retrieve.
 * 
 * @see esp32_get_config() for implementation details
 * @see http_get_payload() to access response
 */
uint8_t http_get_config(struct http_device *http, char *device_id)
{
    if (!http || !http->ops || !http->ops->get_config) return 0;
    return http->ops->get_config(http, device_id);
}

/**
 * @brief Download pond boundaries (Public API)
 * @param http Pointer to HTTP device structure
 * @param query Query string with pond ID
 * @return 1 on success, 0 on failure
 * 
 * @details
 * Wrapper function that calls through operations table.
 * Response is stored in payload - use http_get_payload() to retrieve.
 * 
 * @see esp32_get_pond_boundaries() for implementation details
 * @see http_get_payload() to access response
 */
uint8_t http_get_pond_boundaries(struct http_device *http, char *query)
{
    if (!http || !http->ops || !http->ops->get_pond_boundaries) return 0;
    return http->ops->get_pond_boundaries(http, query);
}

/**
 * @brief Perform OTA firmware update (Public API)
 * @param http Pointer to HTTP device structure
 * @param bsp Pointer to BSP device for watchdog
 * @return Error code: 0=success, 1-5=various errors
 * 
 * @details
 * Wrapper function that calls through operations table.
 * Device will reboot automatically on successful update.
 * Monitor progress via http->curr_progress (0-100%).
 * 
 * @warning Device reboots on success!
 * @see esp32_perform_ota() for implementation details
 */
uint8_t http_perform_ota(struct http_device *http, cBsp *bsp)
{
    if (!http || !http->ops || !http->ops->perform_ota) return 5;
    return http->ops->perform_ota(http, bsp);
}

/**
 * @brief Get last HTTP response payload
 * @param http Pointer to HTTP device structure
 * @return Pointer to payload string, NULL if no payload
 * 
 * @details
 * Returns the response from the last HTTP operation.
 * Payload is stored internally and remains valid until:
 * - Next HTTP operation overwrites it
 * - Device is cleaned up
 * 
 * @note Do not free the returned pointer - it's managed internally
 * @note Copy the data if you need it long-term
 */
const char* http_get_payload(struct http_device *http)
{
    if (!http) return NULL;
    return http->payload;
}
