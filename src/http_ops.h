/**
 * @file http_ops.h
 * @brief HTTP Operations Interface - C-Style Function Pointer Pattern
 * @author Watermon Team
 * @date 2025
 * 
 * @details
 * This header defines the HTTP operations interface using C-style structures
 * with function pointers (Linux kernel driver pattern). This approach provides:
 * - Clear separation between interface and implementation
 * - No virtual function overhead
 * - Easy swapping of implementations
 * - Testability through mock implementations
 * 
 * @par Usage Pattern:
 * @code
 * // 1. Declare device
 * struct http_device g_http_dev;
 * 
 * // 2. Initialize with ops table
 * http_device_init(&g_http_dev, "ESP32_HTTP", &esp32_http_ops);
 * 
 * // 3. Configure server
 * http_set_server(&g_http_dev, "34.93.69.40", 3000, 5000);
 * 
 * // 4. Use operations
 * http_upload_data_frame(&g_http_dev, json_data);
 * time_t epoch = http_upload_ping_frame(&g_http_dev);
 * @endcode
 * 
 * @see http_ops.cpp for implementation details
 */

#ifndef HTTP_OPS_H
#define HTTP_OPS_H

#include <stdint.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declarations */
struct http_device;
class cBsp;  /**< BSP device for watchdog operations */

/**
 * @struct http_ops
 * @brief HTTP Operations Structure
 * 
 * @details
 * Contains function pointers for all HTTP operations. This is the core
 * of the ops pattern - implementations register their functions here.
 * 
 * @note All function pointers must be non-NULL when registered
 */
struct http_ops {
    /** @brief Upload data frame to server via POST
     *  @param http Device structure
     *  @param data JSON data to upload
     *  @return 1 on success, 0 on failure */
    uint8_t (*upload_data_frame)(struct http_device *http, char *data);
    
    /** @brief Send ping and get server time
     *  @param http Device structure
     *  @return Server epoch time, 0 on failure */
    time_t (*upload_ping_frame)(struct http_device *http);
    
    /** @brief Download device configuration
     *  @param http Device structure
     *  @param device_id Query string with device ID
     *  @return 1 on success, 0 on failure */
    uint8_t (*get_config)(struct http_device *http, char *device_id);
    
    /** @brief Download pond boundary coordinates
     *  @param http Device structure
     *  @param query Query string with pond ID
     *  @return 1 on success, 0 on failure */
    uint8_t (*get_pond_boundaries)(struct http_device *http, char *query);
    
    /** @brief Perform OTA firmware update
     *  @param http Device structure
     *  @param bsp BSP device for watchdog
     *  @return Error code: 0=success, 1-5=errors
     *  @warning Device reboots on success! */
    uint8_t (*perform_ota)(struct http_device *http, cBsp *bsp);
};

/**
 * @struct http_device
 * @brief HTTP Device Structure
 * 
 * @details
 * Contains device state, configuration, and operations table.
 * This structure is passed to all HTTP operations.
 * 
 * @par Lifecycle:
 * 1. Declare: `struct http_device g_http_dev;`
 * 2. Initialize: `http_device_init(&g_http_dev, ...)`
 * 3. Use: `http_upload_data_frame(&g_http_dev, ...)`
 * 4. Cleanup: `http_device_cleanup(&g_http_dev)` (optional)
 */
struct http_device {
    const char *name;                   /**< Device name for logging */
    const struct http_ops *ops;         /**< Operations table (function pointers) */
    void *priv;                         /**< Private data (HTTPClient instance) */
    
    /* Configuration */
    char server_ip[25];                 /**< Server IP address */
    char default_server_ip[25];         /**< Default server IP (34.93.69.40) */
    uint16_t server_port;               /**< WebSocket/Socket.IO port (5000) */
    uint16_t http_port;                 /**< HTTP REST API port (3000) */
    char uri_firmware_fota[150];        /**< OTA firmware download URL */
    
    /* State */
    uint8_t is_busy;                    /**< 1 if HTTP operation in progress */
    uint8_t is_connected;               /**< 1 if server is reachable */
    int curr_progress;                  /**< OTA progress percentage (0-100) */
    
    /* Response payload */
    char *payload;                      /**< Last HTTP response (dynamically allocated) */
    size_t payload_size;                /**< Size of payload buffer */
};

/* ESP32 HTTP Client Operations - exported for registration */
extern const struct http_ops esp32_http_ops;

/* Public API Functions */
int http_device_init(struct http_device *http, const char *name, const struct http_ops *ops);
void http_device_cleanup(struct http_device *http);
void http_set_server(struct http_device *http, const char *ip, uint16_t http_port, uint16_t server_port);
uint8_t http_upload_data_frame(struct http_device *http, char *data);
time_t http_upload_ping_frame(struct http_device *http);
uint8_t http_get_config(struct http_device *http, char *device_id);
uint8_t http_get_pond_boundaries(struct http_device *http, char *query);
uint8_t http_perform_ota(struct http_device *http, cBsp *bsp);
const char* http_get_payload(struct http_device *http);

#ifdef __cplusplus
}
#endif

#endif /* HTTP_OPS_H */
