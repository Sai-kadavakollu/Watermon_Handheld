/**
 * @file do_sensor_ops.h
 * @brief DO Sensor Operations Interface - C-Style Function Pointer Pattern
 * @author Watermon Team
 * @date 2025
 * 
 * @details
 * This header defines the DO sensor operations interface using C-style structures
 * with function pointers (Linux kernel driver pattern). This approach provides:
 * - Clear separation between interface and implementation
 * - No virtual function overhead
 * - Easy swapping of implementations
 * - Testability through mock implementations
 * - Automatic disconnection detection (10 consecutive failures)
 * 
 * @par Usage Pattern:
 * @code
 * // 1. Declare device
 * struct do_sensor_device g_do_sensor;
 * 
 * // 2. Initialize with ops table
 * do_sensor_init(&g_do_sensor, "FLDBH-505A", &modbus_do_sensor_ops);
 * 
 * // 3. Configure sensor
 * do_sensor_setup(&g_do_sensor, &Serial1, 0x01);
 * 
 * // 4. Use operations
 * do_sensor_read_values(&g_do_sensor);
 * float temp = g_do_sensor.temp;
 * bool connected = do_sensor_is_connected(&g_do_sensor);
 * @endcode
 * 
 * @see do_sensor_ops.cpp for implementation details
 */

#ifndef DO_SENSOR_OPS_H
#define DO_SENSOR_OPS_H

#include <stdint.h>
#include <Stream.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declarations */
struct do_sensor_device;

/**
 * @brief DO Sensor register configuration structure
 */
typedef struct {
    uint16_t start_address;      /**< Modbus register start address */
    uint16_t num_registers;      /**< Number of registers to read/write */
    uint16_t response_buffer[10]; /**< Buffer for Modbus response */
} do_sensor_register_t;

/**
 * @brief Union for float/uint32 conversion
 */
typedef union {
    uint32_t uint_value;
    float float_value;
} float_converter_t;

/**
 * @struct do_sensor_ops
 * @brief DO Sensor Operations Structure
 * 
 * @details
 * Contains function pointers for all DO sensor operations. This is the core
 * of the ops pattern - implementations register their functions here.
 * 
 * @note All function pointers must be non-NULL when registered
 */
struct do_sensor_ops {
    /** @brief Initialize sensor communication
     *  @param sensor Device structure
     *  @param serial Serial port for Modbus communication
     *  @param slave_id Modbus slave ID (default 0x01)
     *  @return 1 on success, 0 on failure */
    uint8_t (*init)(struct do_sensor_device *sensor, Stream *serial, uint8_t slave_id);
    
    /** @brief Read temperature and DO values
     *  @param sensor Device structure
     *  @return 1 on success, 0 on failure */
    uint8_t (*read_values)(struct do_sensor_device *sensor);
    
    /** @brief Start continuous measurement
     *  @param sensor Device structure
     *  @return 1 on success, 0 on failure */
    uint8_t (*start_measurement)(struct do_sensor_device *sensor);
    
    /** @brief Stop continuous measurement
     *  @param sensor Device structure
     *  @return 1 on success, 0 on failure */
    uint8_t (*stop_measurement)(struct do_sensor_device *sensor);
    
    /** @brief Set calibration values
     *  @param sensor Device structure
     *  @return 1 on success, 0 on failure */
    uint8_t (*set_calibration)(struct do_sensor_device *sensor);
    
    /** @brief Get calibration values
     *  @param sensor Device structure
     *  @return 1 on success, 0 on failure */
    uint8_t (*get_calibration)(struct do_sensor_device *sensor);
    
    /** @brief Set salinity value
     *  @param sensor Device structure
     *  @param salinity Salinity value to set
     *  @return 1 on success, 0 on failure */
    uint8_t (*set_salinity)(struct do_sensor_device *sensor, float salinity);
    
    /** @brief Get salinity value
     *  @param sensor Device structure
     *  @return 1 on success, 0 on failure */
    uint8_t (*get_salinity)(struct do_sensor_device *sensor);
    
    /** @brief Set pressure value
     *  @param sensor Device structure
     *  @param pressure Pressure value to set
     *  @return 1 on success, 0 on failure */
    uint8_t (*set_pressure)(struct do_sensor_device *sensor, float pressure);
    
    /** @brief Get pressure value
     *  @param sensor Device structure
     *  @return 1 on success, 0 on failure */
    uint8_t (*get_pressure)(struct do_sensor_device *sensor);
    
    /** @brief Get sensor serial number
     *  @param sensor Device structure
     *  @return 1 on success, 0 on failure */
    uint8_t (*get_serial_number)(struct do_sensor_device *sensor);
};

/**
 * @struct do_sensor_device
 * @brief DO Sensor Device Structure
 * 
 * @details
 * Contains device state, configuration, and operations table.
 * This structure is passed to all DO sensor operations.
 * 
 * @par Lifecycle:
 * 1. Declare: `struct do_sensor_device g_do_sensor;`
 * 2. Initialize: `do_sensor_init(&g_do_sensor, ...)`
 * 3. Setup: `do_sensor_setup(&g_do_sensor, &Serial1, 0x01)`
 * 4. Use: `do_sensor_read_values(&g_do_sensor)`
 * 5. Cleanup: `do_sensor_cleanup(&g_do_sensor)` (optional)
 */
struct do_sensor_device {
    const char *name;                   /**< Device name for logging */
    const struct do_sensor_ops *ops;    /**< Operations table (function pointers) */
    void *priv;                         /**< Private data (ModbusMaster instance) */
    
    /* Sensor readings */
    float temp;                         /**< Temperature (°C) */
    float do_percent;                   /**< Dissolved Oxygen (%) */
    float do_mgl;                       /**< Dissolved Oxygen (mg/L) */
    
    /* Configuration */
    float salinity;                     /**< Salinity value */
    float pressure;                     /**< Pressure value */
    float cal_k;                        /**< Calibration K value */
    float cal_b;                        /**< Calibration B value */
    char serial_number[15];             /**< Sensor serial number */
    
    /* State flags */
    uint8_t is_initialized;             /**< 1 if sensor is initialized */
    uint8_t is_measuring;               /**< 1 if continuous measurement active */
    uint8_t stop_reading;               /**< 1 to stop reading during calibration */
    
    /* Disconnection detection */
    uint8_t consecutive_failures;       /**< Count of consecutive read failures */
    uint8_t is_disconnected;            /**< 1 if sensor disconnected (10+ failures) */
    
    /* Historical data */
    float do_values[10];                /**< Array for averaging DO values */
    uint8_t do_values_index;            /**< Current index in do_values array */
};

/* Modbus DO Sensor Operations - exported for registration */
extern const struct do_sensor_ops modbus_do_sensor_ops;

/* Public API Functions */

/**
 * @brief Initialize DO sensor device structure
 * @param sensor Pointer to sensor device structure
 * @param name Device name for logging
 * @param ops Operations table
 * @return 1 on success, 0 on failure
 */
int do_sensor_init(struct do_sensor_device *sensor, const char *name, const struct do_sensor_ops *ops);

/**
 * @brief Setup sensor communication
 * @param sensor Pointer to sensor device structure
 * @param serial Serial port for Modbus
 * @param slave_id Modbus slave ID (default 0x01)
 * @return 1 on success, 0 on failure
 */
uint8_t do_sensor_setup(struct do_sensor_device *sensor, Stream *serial, uint8_t slave_id);

/**
 * @brief Read temperature and DO values from sensor
 * @param sensor Pointer to sensor device structure
 * @return 1 on success, 0 on failure
 */
uint8_t do_sensor_read_values(struct do_sensor_device *sensor);

/**
 * @brief Start continuous measurement
 * @param sensor Pointer to sensor device structure
 * @return 1 on success, 0 on failure
 */
uint8_t do_sensor_start_measurement(struct do_sensor_device *sensor);

/**
 * @brief Stop continuous measurement
 * @param sensor Pointer to sensor device structure
 * @return 1 on success, 0 on failure
 */
uint8_t do_sensor_stop_measurement(struct do_sensor_device *sensor);

/**
 * @brief Set calibration values
 * @param sensor Pointer to sensor device structure
 * @return 1 on success, 0 on failure
 */
uint8_t do_sensor_set_calibration(struct do_sensor_device *sensor);

/**
 * @brief Get calibration values
 * @param sensor Pointer to sensor device structure
 * @return 1 on success, 0 on failure
 */
uint8_t do_sensor_get_calibration(struct do_sensor_device *sensor);

/**
 * @brief Set salinity value
 * @param sensor Pointer to sensor device structure
 * @param salinity Salinity value to set
 * @return 1 on success, 0 on failure
 */
uint8_t do_sensor_set_salinity(struct do_sensor_device *sensor, float salinity);

/**
 * @brief Get salinity value
 * @param sensor Pointer to sensor device structure
 * @return 1 on success, 0 on failure
 */
uint8_t do_sensor_get_salinity(struct do_sensor_device *sensor);

/**
 * @brief Set pressure value
 * @param sensor Pointer to sensor device structure
 * @param pressure Pressure value to set
 * @return 1 on success, 0 on failure
 */
uint8_t do_sensor_set_pressure(struct do_sensor_device *sensor, float pressure);

/**
 * @brief Get pressure value
 * @param sensor Pointer to sensor device structure
 * @return 1 on success, 0 on failure
 */
uint8_t do_sensor_get_pressure(struct do_sensor_device *sensor);

/**
 * @brief Get sensor serial number
 * @param sensor Pointer to sensor device structure
 * @return 1 on success, 0 on failure
 */
uint8_t do_sensor_get_serial_number(struct do_sensor_device *sensor);

/**
 * @brief Check if sensor is connected
 * @param sensor Pointer to sensor device structure
 * @return 1 if connected, 0 if disconnected
 */
uint8_t do_sensor_is_connected(struct do_sensor_device *sensor);

/**
 * @brief Calculate average DO value from historical data
 * @param sensor Pointer to sensor device structure
 * @return Average DO value
 */
float do_sensor_calculate_average_do(struct do_sensor_device *sensor);

/**
 * @brief Cleanup sensor device (optional)
 * @param sensor Pointer to sensor device structure
 */
void do_sensor_cleanup(struct do_sensor_device *sensor);

#ifdef __cplusplus
}
#endif

#endif /* DO_SENSOR_OPS_H */
