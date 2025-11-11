/**
 * @file do_sensor_ops.cpp
 * @brief DO Sensor Operations Implementation
 * @author Watermon Team
 * @date 2025
 * 
 * @details
 * Implementation of DO sensor operations using Modbus RTU communication.
 * Supports FLDBH-505A DO Sensor with automatic disconnection detection.
 */

#include "do_sensor_ops.h"
#include <Arduino.h>
#include <ModbusMaster.h>
#include <string.h>
#include <math.h>

// #define SERIAL_DEBUG
#ifdef SERIAL_DEBUG
#define debugPrint(...) Serial.print(__VA_ARGS__)
#define debugPrintln(...) Serial.println(__VA_ARGS__)
#define debugPrintf(...) Serial.printf(__VA_ARGS__)
#else
#define debugPrint(...)
#define debugPrintln(...)
#define debugPrintf(...)
#endif

#define SUCCESS 1
#define FAIL 0
#define MAX_CONSECUTIVE_FAILURES 10

/* Modbus register configurations */
static do_sensor_register_t reg_serial_num = {0x0900, 0x07, {0}};
static do_sensor_register_t reg_start_msrmnt = {0x2500, 0x01, {0}};
static do_sensor_register_t reg_stop_msrmnt = {0x2E00, 0x01, {0}};
static do_sensor_register_t reg_temp_do_vals = {0x2600, 0x06, {0}};
static do_sensor_register_t reg_cal_data = {0x1100, 0x04, {0}};
static do_sensor_register_t reg_salinity = {0x1500, 0x02, {0}};
static do_sensor_register_t reg_pressure = {0x2400, 0x02, {0}};

/* Global Modbus node - stored in device->priv */
static ModbusMaster *g_modbus_node = NULL;

/* ========================================================================
 * HELPER FUNCTIONS
 * ======================================================================== */

/**
 * @brief Convert float to hex in little-endian format
 */
static void float_to_hex_le(float value, uint16_t *val1, uint16_t *val2)
{
    uint32_t hex;
    memcpy(&hex, &value, sizeof(float));
    
    // Swap byte order (convert from middle-endian to little-endian with DCBA)
    hex = ((hex & 0xFF) << 24) | ((hex & 0xFF00) << 8) |
          ((hex & 0xFF0000) >> 8) | ((hex & 0xFF000000) >> 24);
    
    unsigned char *ptr = (unsigned char *)&hex;
    *val1 = (ptr[3] << 8) | ptr[2];
    *val2 = (ptr[1] << 8) | ptr[0];
}

/**
 * @brief Get data from sensor via Modbus RTU
 */
static int get_data_from_sensor(ModbusMaster *node, uint16_t start_addr, 
                                uint16_t num_regs, uint16_t *response_buf)
{
    if (!node) return FAIL;
    
    uint8_t result = node->readHoldingRegisters(start_addr, num_regs);
    
    if (result == node->ku8MBSuccess)
    {
        for (size_t i = 0; i < num_regs; i++)
        {
            response_buf[i] = node->getResponseBuffer(i);
        }
        
        #ifdef SERIAL_DEBUG
        for (size_t i = 0; i < num_regs; i++)
        {
            debugPrint(response_buf[i], HEX);
            debugPrint(" ");
        }
        debugPrintln();
        #endif
        
        return SUCCESS;
    }
    else
    {
        debugPrintln("[DoSensor] Failed to read holding registers");
        return FAIL;
    }
}

/**
 * @brief Extract temperature and DO values from response buffer
 */
static void extract_temp_and_do_values(uint16_t response_buf[], float *temp, 
                                      float *do_percent, float *do_mgl)
{
    /* Temperature: Combine two 16-bit values into 32-bit */
    uint32_t temp_hex = ((uint32_t)response_buf[0] << 16) | response_buf[1];
    uint32_t do_hex = ((uint32_t)response_buf[2] << 16) | response_buf[3];
    uint32_t domg_hex = ((uint32_t)response_buf[4] << 16) | response_buf[5];
    
    /* Extract and recombine bytes */
    uint8_t byte1_temp = (temp_hex >> 24) & 0xFF;
    uint8_t byte2_temp = (temp_hex >> 16) & 0xFF;
    uint8_t byte3_temp = (temp_hex >> 8) & 0xFF;
    uint8_t byte4_temp = temp_hex & 0xFF;
    uint32_t combined_temp = ((uint32_t)byte4_temp << 24) | ((uint32_t)byte3_temp << 16) | 
                            ((uint32_t)byte2_temp << 8) | byte1_temp;
    
    uint8_t byte1_do = (do_hex >> 24) & 0xFF;
    uint8_t byte2_do = (do_hex >> 16) & 0xFF;
    uint8_t byte3_do = (do_hex >> 8) & 0xFF;
    uint8_t byte4_do = do_hex & 0xFF;
    uint32_t combined_do = ((uint32_t)byte4_do << 24) | ((uint32_t)byte3_do << 16) | 
                          ((uint32_t)byte2_do << 8) | byte1_do;
    
    uint8_t byte1_domg = (domg_hex >> 24) & 0xFF;
    uint8_t byte2_domg = (domg_hex >> 16) & 0xFF;
    uint8_t byte3_domg = (domg_hex >> 8) & 0xFF;
    uint8_t byte4_domg = domg_hex & 0xFF;
    uint32_t combined_domg = ((uint32_t)byte4_domg << 24) | ((uint32_t)byte3_domg << 16) | 
                            ((uint32_t)byte2_domg << 8) | byte1_domg;
    
    /* Reinterpret as float */
    float_converter_t converter;
    
    converter.uint_value = combined_temp;
    *temp = converter.float_value;
    
    converter.uint_value = combined_do;
    *do_percent = converter.float_value * 100;
    
    converter.uint_value = combined_domg;
    *do_mgl = converter.float_value;
}

/**
 * @brief Extract calibration values from response buffer
 */
static void extract_cal_values(uint16_t response_buf[], float *k, float *b)
{
    uint32_t k_val = ((uint32_t)response_buf[0] << 16) | response_buf[1];
    uint32_t b_val = ((uint32_t)response_buf[2] << 16) | response_buf[3];
    
    /* Extract and recombine bytes */
    uint8_t byte1_k = (k_val >> 24) & 0xFF;
    uint8_t byte2_k = (k_val >> 16) & 0xFF;
    uint8_t byte3_k = (k_val >> 8) & 0xFF;
    uint8_t byte4_k = k_val & 0xFF;
    uint32_t combined_k = ((uint32_t)byte4_k << 24) | ((uint32_t)byte3_k << 16) | 
                         ((uint32_t)byte2_k << 8) | byte1_k;
    
    uint8_t byte1_b = (b_val >> 24) & 0xFF;
    uint8_t byte2_b = (b_val >> 16) & 0xFF;
    uint8_t byte3_b = (b_val >> 8) & 0xFF;
    uint8_t byte4_b = b_val & 0xFF;
    uint32_t combined_b = ((uint32_t)byte4_b << 24) | ((uint32_t)byte3_b << 16) | 
                         ((uint32_t)byte2_b << 8) | byte1_b;
    
    float_converter_t converter;
    converter.uint_value = combined_k;
    *k = converter.float_value;
    
    converter.uint_value = combined_b;
    *b = converter.float_value;
}

/**
 * @brief Extract single float value from response buffer
 */
static void extract_value(uint16_t response_buf[], float *val)
{
    uint32_t value = ((uint32_t)response_buf[0] << 16) | response_buf[1];
    
    uint8_t byte1 = (value >> 24) & 0xFF;
    uint8_t byte2 = (value >> 16) & 0xFF;
    uint8_t byte3 = (value >> 8) & 0xFF;
    uint8_t byte4 = value & 0xFF;
    uint32_t combined = ((uint32_t)byte4 << 24) | ((uint32_t)byte3 << 16) | 
                       ((uint32_t)byte2 << 8) | byte1;
    
    float_converter_t converter;
    converter.uint_value = combined;
    *val = converter.float_value;
}

/* ========================================================================
 * MODBUS DO SENSOR OPERATIONS IMPLEMENTATION
 * ======================================================================== */

/**
 * @brief Initialize Modbus DO sensor
 */
static uint8_t modbus_do_sensor_init(struct do_sensor_device *sensor, Stream *serial, uint8_t slave_id)
{
    if (!sensor || !serial) return FAIL;
    
    /* Allocate ModbusMaster if not already done */
    if (!sensor->priv)
    {
        sensor->priv = new ModbusMaster();
        if (!sensor->priv) return FAIL;
    }
    
    ModbusMaster *node = (ModbusMaster *)sensor->priv;
    g_modbus_node = node;
    
    /* Initialize Modbus */
    node->begin(slave_id, *serial);
    
    /* Start measurement */
    get_data_from_sensor(node, reg_start_msrmnt.start_address, 
                        reg_start_msrmnt.num_registers, reg_start_msrmnt.response_buffer);
    
    /* Get salinity */
    if (sensor->salinity == 0.0f)
    {
        if (get_data_from_sensor(node, reg_salinity.start_address, 
                                reg_salinity.num_registers, reg_salinity.response_buffer) == SUCCESS)
        {
            extract_value(reg_salinity.response_buffer, &sensor->salinity);
        }
    }
    
    sensor->is_initialized = 1;
    sensor->is_measuring = 1;
    sensor->consecutive_failures = 0;
    sensor->is_disconnected = 0;
    
    debugPrintln("[DoSensor] Initialized successfully");
    return SUCCESS;
}

/**
 * @brief Read temperature and DO values with validation and disconnection detection
 */
static uint8_t modbus_do_sensor_read_values(struct do_sensor_device *sensor)
{
    if (!sensor || !sensor->priv) return FAIL;
    
    ModbusMaster *node = (ModbusMaster *)sensor->priv;
    
    int result = get_data_from_sensor(node, reg_temp_do_vals.start_address, 
                                     reg_temp_do_vals.num_registers, reg_temp_do_vals.response_buffer);
    
    if (result == SUCCESS)
    {
        float temp_reading, do_reading, do_mgl_reading;
        
        /* Extract values */
        extract_temp_and_do_values(reg_temp_do_vals.response_buffer, &temp_reading, 
                                  &do_reading, &do_mgl_reading);
        
        /* Validate extracted values */
        bool valid_temp = (temp_reading >= 0.0 && temp_reading <= 60.0 && 
                          !isnan(temp_reading) && !isinf(temp_reading));
        bool valid_do = (do_reading >= 0.0 && do_reading <= 200.0 && 
                        !isnan(do_reading) && !isinf(do_reading));
        bool valid_do_mgl = (do_mgl_reading >= 0.0 && do_mgl_reading <= 25.0 && 
                            !isnan(do_mgl_reading) && !isinf(do_mgl_reading));
        
        /* Filter extreme invalid values */
        if (temp_reading < 0.0 || temp_reading > 100.0) valid_temp = false;
        if (do_reading < 0.0 || do_reading > 200.0) valid_do = false;
        if (do_mgl_reading <= 0.0 || do_mgl_reading > 25.0) valid_do_mgl = false;
        
        /* Update values only if valid */
        if (valid_temp && valid_do && valid_do_mgl)
        {
            sensor->temp = temp_reading;
            sensor->do_percent = do_reading;
            sensor->do_mgl = do_mgl_reading;
            
            /* Reset failure counter on successful read */
            sensor->consecutive_failures = 0;
            sensor->is_disconnected = 0;
            
            debugPrintf("[DoSensor][Success] Temp: %.2f, DO%%: %.2f, DO mg/L: %.2f\n", 
                       sensor->temp, sensor->do_percent, sensor->do_mgl);
            
            /* Get salinity if not set */
            if (sensor->salinity == 0.0f)
            {
                if (get_data_from_sensor(node, reg_salinity.start_address, 
                                        reg_salinity.num_registers, reg_salinity.response_buffer) == SUCCESS)
                {
                    extract_value(reg_salinity.response_buffer, &sensor->salinity);
                }
            }
            
            return SUCCESS;
        }
        else
        {
            debugPrintf("[DoSensor][InvalidValues] Temp: %.2f (valid:%d), DO%%: %.2f (valid:%d), DO mg/L: %.2f (valid:%d)\n",
                       temp_reading, valid_temp, do_reading, valid_do, do_mgl_reading, valid_do_mgl);
            
            /* Increment failure counter for invalid data */
            sensor->consecutive_failures++;
        }
    }
    else
    {
        /* Communication failure */
        sensor->consecutive_failures++;
        debugPrintln("[DoSensor][CommFail] Modbus communication failed");
    }
    
    /* Check for disconnection (10 consecutive failures) */
    if (sensor->consecutive_failures >= MAX_CONSECUTIVE_FAILURES)
    {
        sensor->is_disconnected = 1;
        sensor->temp = 0.0;
        sensor->do_percent = 0.0;
        sensor->do_mgl = 0.0;
        debugPrintf("[DoSensor][Disconnected] %d consecutive failures\n", sensor->consecutive_failures);
    }
    
    return FAIL;
}

/**
 * @brief Start continuous measurement
 */
static uint8_t modbus_do_sensor_start_measurement(struct do_sensor_device *sensor)
{
    if (!sensor || !sensor->priv) return FAIL;
    
    ModbusMaster *node = (ModbusMaster *)sensor->priv;
    int result = get_data_from_sensor(node, reg_start_msrmnt.start_address, 
                                     reg_start_msrmnt.num_registers, reg_start_msrmnt.response_buffer);
    
    if (result == SUCCESS)
    {
        sensor->is_measuring = 1;
        return SUCCESS;
    }
    return FAIL;
}

/**
 * @brief Stop continuous measurement
 */
static uint8_t modbus_do_sensor_stop_measurement(struct do_sensor_device *sensor)
{
    if (!sensor || !sensor->priv) return FAIL;
    
    ModbusMaster *node = (ModbusMaster *)sensor->priv;
    int result = get_data_from_sensor(node, reg_stop_msrmnt.start_address, 
                                     reg_stop_msrmnt.num_registers, reg_stop_msrmnt.response_buffer);
    
    if (result == SUCCESS)
    {
        sensor->is_measuring = 0;
        return SUCCESS;
    }
    return FAIL;
}

/**
 * @brief Set calibration values
 */
static uint8_t modbus_do_sensor_set_calibration(struct do_sensor_device *sensor)
{
    if (!sensor || !sensor->priv) return FAIL;
    
    ModbusMaster *node = (ModbusMaster *)sensor->priv;
    node->clearTransmitBuffer();
    
    float k = 100.0 / sensor->do_percent;
    debugPrintln(sensor->do_percent);
    debugPrintln(k);
    
    uint16_t val1, val2;
    float_to_hex_le(k, &val1, &val2);
    
    uint16_t values[4] = {val1, val2, 0, 0};
    
    for (uint8_t i = 0; i < 4; i++)
    {
        debugPrint(values[i], HEX);
        debugPrint(" ");
        node->setTransmitBuffer(i, values[i]);
    }
    
    uint8_t result = node->writeMultipleRegisters(0x1100, 0x04);
    
    if (result == node->ku8MBSuccess)
    {
        debugPrintln("[DoSensor] Calibration set successfully");
        return SUCCESS;
    }
    else
    {
        debugPrintln("[DoSensor] Error setting calibration");
        return FAIL;
    }
}

/**
 * @brief Get calibration values
 */
static uint8_t modbus_do_sensor_get_calibration(struct do_sensor_device *sensor)
{
    if (!sensor || !sensor->priv) return FAIL;
    
    ModbusMaster *node = (ModbusMaster *)sensor->priv;
    int result = get_data_from_sensor(node, 0x1100, 0x04, reg_cal_data.response_buffer);
    
    if (result == SUCCESS)
    {
        extract_cal_values(reg_cal_data.response_buffer, &sensor->cal_k, &sensor->cal_b);
        debugPrintf("[DoSensor] K: %.2f, B: %.2f\n", sensor->cal_k, sensor->cal_b);
        return SUCCESS;
    }
    return FAIL;
}

/**
 * @brief Set salinity value
 */
static uint8_t modbus_do_sensor_set_salinity(struct do_sensor_device *sensor, float salinity)
{
    if (!sensor || !sensor->priv) return FAIL;
    
    ModbusMaster *node = (ModbusMaster *)sensor->priv;
    node->clearTransmitBuffer();
    
    sensor->salinity = salinity;
    
    uint16_t val1, val2;
    float_to_hex_le(salinity, &val1, &val2);
    
    uint16_t values[2] = {val1, val2};
    
    for (uint8_t i = 0; i < 2; i++)
    {
        node->setTransmitBuffer(i, values[i]);
    }
    
    uint8_t result = node->writeMultipleRegisters(reg_salinity.start_address, reg_salinity.num_registers);
    
    if (result == node->ku8MBSuccess)
    {
        debugPrintln("[DoSensor] Salinity set successfully");
        return SUCCESS;
    }
    else
    {
        debugPrintln("[DoSensor] Error setting salinity");
        return FAIL;
    }
}

/**
 * @brief Get salinity value
 */
static uint8_t modbus_do_sensor_get_salinity(struct do_sensor_device *sensor)
{
    if (!sensor || !sensor->priv) return FAIL;
    
    ModbusMaster *node = (ModbusMaster *)sensor->priv;
    int result = get_data_from_sensor(node, reg_salinity.start_address, 
                                     reg_salinity.num_registers, reg_salinity.response_buffer);
    
    if (result == SUCCESS)
    {
        extract_value(reg_salinity.response_buffer, &sensor->salinity);
        debugPrintf("[DoSensor] Salinity: %.2f\n", sensor->salinity);
        return SUCCESS;
    }
    return FAIL;
}

/**
 * @brief Set pressure value
 */
static uint8_t modbus_do_sensor_set_pressure(struct do_sensor_device *sensor, float pressure)
{
    if (!sensor || !sensor->priv) return FAIL;
    
    ModbusMaster *node = (ModbusMaster *)sensor->priv;
    node->clearTransmitBuffer();
    
    sensor->pressure = pressure;
    
    uint16_t val1, val2;
    float_to_hex_le(pressure, &val1, &val2);
    
    uint16_t values[2] = {val1, val2};
    
    for (uint8_t i = 0; i < 2; i++)
    {
        node->setTransmitBuffer(i, values[i]);
    }
    
    uint8_t result = node->writeMultipleRegisters(reg_pressure.start_address, reg_pressure.num_registers);
    
    if (result == node->ku8MBSuccess)
    {
        debugPrintln("[DoSensor] Pressure set successfully");
        return SUCCESS;
    }
    else
    {
        debugPrintln("[DoSensor] Error setting pressure");
        return FAIL;
    }
}

/**
 * @brief Get pressure value
 */
static uint8_t modbus_do_sensor_get_pressure(struct do_sensor_device *sensor)
{
    if (!sensor || !sensor->priv) return FAIL;
    
    ModbusMaster *node = (ModbusMaster *)sensor->priv;
    int result = get_data_from_sensor(node, reg_pressure.start_address, 
                                     reg_pressure.num_registers, reg_pressure.response_buffer);
    
    if (result == SUCCESS)
    {
        extract_value(reg_pressure.response_buffer, &sensor->pressure);
        debugPrintf("[DoSensor] Pressure: %.2f\n", sensor->pressure);
        return SUCCESS;
    }
    return FAIL;
}

/**
 * @brief Get sensor serial number
 */
static uint8_t modbus_do_sensor_get_serial_number(struct do_sensor_device *sensor)
{
    if (!sensor || !sensor->priv) return FAIL;
    
    ModbusMaster *node = (ModbusMaster *)sensor->priv;
    int result = get_data_from_sensor(node, reg_serial_num.start_address, 
                                     reg_serial_num.num_registers, reg_serial_num.response_buffer);
    
    if (result == SUCCESS)
    {
        /* Store serial number (implementation depends on format) */
        debugPrintln("[DoSensor] Serial number retrieved");
        return SUCCESS;
    }
    return FAIL;
}

/* ========================================================================
 * OPERATIONS TABLE
 * ======================================================================== */

const struct do_sensor_ops modbus_do_sensor_ops = {
    .init = modbus_do_sensor_init,
    .read_values = modbus_do_sensor_read_values,
    .start_measurement = modbus_do_sensor_start_measurement,
    .stop_measurement = modbus_do_sensor_stop_measurement,
    .set_calibration = modbus_do_sensor_set_calibration,
    .get_calibration = modbus_do_sensor_get_calibration,
    .set_salinity = modbus_do_sensor_set_salinity,
    .get_salinity = modbus_do_sensor_get_salinity,
    .set_pressure = modbus_do_sensor_set_pressure,
    .get_pressure = modbus_do_sensor_get_pressure,
    .get_serial_number = modbus_do_sensor_get_serial_number
};

/* ========================================================================
 * PUBLIC API FUNCTIONS
 * ======================================================================== */

int do_sensor_init(struct do_sensor_device *sensor, const char *name, const struct do_sensor_ops *ops)
{
    if (!sensor || !ops) return FAIL;
    
    memset(sensor, 0, sizeof(struct do_sensor_device));
    sensor->name = name;
    sensor->ops = ops;
    
    return SUCCESS;
}

uint8_t do_sensor_setup(struct do_sensor_device *sensor, Stream *serial, uint8_t slave_id)
{
    if (!sensor || !sensor->ops || !sensor->ops->init) return FAIL;
    return sensor->ops->init(sensor, serial, slave_id);
}

uint8_t do_sensor_read_values(struct do_sensor_device *sensor)
{
    if (!sensor || !sensor->ops || !sensor->ops->read_values) return FAIL;
    return sensor->ops->read_values(sensor);
}

uint8_t do_sensor_start_measurement(struct do_sensor_device *sensor)
{
    if (!sensor || !sensor->ops || !sensor->ops->start_measurement) return FAIL;
    return sensor->ops->start_measurement(sensor);
}

uint8_t do_sensor_stop_measurement(struct do_sensor_device *sensor)
{
    if (!sensor || !sensor->ops || !sensor->ops->stop_measurement) return FAIL;
    return sensor->ops->stop_measurement(sensor);
}

uint8_t do_sensor_set_calibration(struct do_sensor_device *sensor)
{
    if (!sensor || !sensor->ops || !sensor->ops->set_calibration) return FAIL;
    return sensor->ops->set_calibration(sensor);
}

uint8_t do_sensor_get_calibration(struct do_sensor_device *sensor)
{
    if (!sensor || !sensor->ops || !sensor->ops->get_calibration) return FAIL;
    return sensor->ops->get_calibration(sensor);
}

uint8_t do_sensor_set_salinity(struct do_sensor_device *sensor, float salinity)
{
    if (!sensor || !sensor->ops || !sensor->ops->set_salinity) return FAIL;
    return sensor->ops->set_salinity(sensor, salinity);
}

uint8_t do_sensor_get_salinity(struct do_sensor_device *sensor)
{
    if (!sensor || !sensor->ops || !sensor->ops->get_salinity) return FAIL;
    return sensor->ops->get_salinity(sensor);
}

uint8_t do_sensor_set_pressure(struct do_sensor_device *sensor, float pressure)
{
    if (!sensor || !sensor->ops || !sensor->ops->set_pressure) return FAIL;
    return sensor->ops->set_pressure(sensor, pressure);
}

uint8_t do_sensor_get_pressure(struct do_sensor_device *sensor)
{
    if (!sensor || !sensor->ops || !sensor->ops->get_pressure) return FAIL;
    return sensor->ops->get_pressure(sensor);
}

uint8_t do_sensor_get_serial_number(struct do_sensor_device *sensor)
{
    if (!sensor || !sensor->ops || !sensor->ops->get_serial_number) return FAIL;
    return sensor->ops->get_serial_number(sensor);
}

uint8_t do_sensor_is_connected(struct do_sensor_device *sensor)
{
    if (!sensor) return 0;
    return !sensor->is_disconnected;
}

float do_sensor_calculate_average_do(struct do_sensor_device *sensor)
{
    if (!sensor) return 0.0;
    
    float sum = 0.0;
    for (uint8_t i = 0; i < 10; i++)
    {
        sum += sensor->do_values[i];
    }
    return sum / 10.0;
}

void do_sensor_cleanup(struct do_sensor_device *sensor)
{
    if (!sensor) return;
    
    if (sensor->priv)
    {
        delete (ModbusMaster *)sensor->priv;
        sensor->priv = NULL;
    }
}
