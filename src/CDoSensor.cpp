/*
  CDoSensor.cpp - This CPP file is used to read the register values of FLDBH-505A DO Sensor. RS485 Modbus RTU is used to establish the communication with sensor.

  Dev: Infiplus Team
  January 2024
*/

#include "CDoSensor.h"

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

m_oDOsensor getSerialNum = {0x0900, 0x07};  // Address to get the Serial Number
m_oDOsensor startMsrmnt = {0x2500, 0x01};   // Address to start measurement
m_oDOsensor stopMsrmnt = {0x2E00, 0x01};    // Address to stop measurement
m_oDOsensor getTempDoVals = {0x2600, 0x06}; // Address to get the Temp & DO values
m_oDOsensor CalData = {0x1100, 0x04};       // Address to get the calbration values
m_oDOsensor salinity = {0x1500, 0x02};      // Address to set the salinity values
m_oDOsensor pressure = {0x2400, 0x02};      // Address to set the pressure values

ModbusMaster node;

/* Construct */
CSensor::CSensor()
{
  m_fTemp = 0.0;
  m_fDoMgl = 0.0;
  noSensor = false;
}
/* Destruct */
CSensor::~CSensor() {}

/***********************************************************
 * Sensor initiation with the serial port
 * @param serial reference to serial port object (Serial, Serial1, ... Serial3)
 * @param slave  Modbus slave ID (1..255)
 **********************************************************/
void CSensor::sensorInit(Stream &serial, uint8_t slaveID)
{
  /*Serial should be defined here*/
  node.begin(slaveID, serial);
  /*Starts measuring the values.*/
  startMeasurement();
  getSalinity();
}

void float_to_hex_le(float value, uint16_t *val1, uint16_t *val2)
{
  uint32_t hex;
  unsigned char *ptr;

  // Copy bytes from float to uint32_t
  memcpy(&hex, &value, sizeof(float));

  // Swap byte order (convert from middle-endian to little-endian with DCBA)
  hex = ((hex & 0xFF) << 24) | ((hex & 0xFF00) << 8) |
        ((hex & 0xFF0000) >> 8) | ((hex & 0xFF000000) >> 24);

  // Use the same logic as before to extract val1 and val2
  ptr = (unsigned char *)&hex;
  *val1 = (ptr[3] << 8) | ptr[2];
  *val2 = (ptr[1] << 8) | ptr[0];
}

/************************************************************
 * Function to get the data from the Sensor through Modbus RTU
 * @param startAddress  Start address of the register
 * @param numRegisters  Number of registers
 * @param responseBuffer  Buffer to store the response for the sent command
 * @return 1 on success; exception number on failure
 *************************************************************/
int CSensor::getDataFrmSensor(uint16_t startAddress, uint16_t numRegisters, uint16_t *responseBuffer)
{
  // Send the read holding registers command
  uint8_t result = node.readHoldingRegisters(startAddress, numRegisters);

  if (result == node.ku8MBSuccess)
  {
    // Read the response data into the response buffer
    for (size_t i = 0; i < numRegisters; i++) // Assuming each register is 2 bytes
    {
      responseBuffer[i] = node.getResponseBuffer(i); // Assuming modbusmaster.h has a function like getResponseBuffer(index)
    }
    // Print the response data in hexadecimal format
    for (size_t i = 0; i < numRegisters; i++)
    {
      debugPrint(responseBuffer[i], HEX);
      debugPrint(" ");
    }
    debugPrintln(); // Print a newline after printing all bytes

    return SUCCESS;
  }
  else
  {
    debugPrintln("Failed to read holding registers");
    return FAIL;
  }
}

/************************************************************
 * Function to extract the data that we fot from the Sensor through Modbus RTU
 * @param responseBuffer  Buffer to store the response for the sent command
 * @param temperature  temperature variable to store the data
 * @param dissolvedOxygen  dissolvedOxygen variable to store the data
 * @param dissolvedOxygenMg  dissolvedOxygenMg variable to store the data
 *************************************************************/
void CSensor::extractTempAndDoValues(uint16_t responseBuffer[], float *temperature, float *dissolvedOxygen, float *dissolvedOxygenMg)
{
  /* Temperature: Combine the two 16-bit values into a single 32-bit value*/
  uint32_t temperatureHex = ((uint32_t)responseBuffer[0] << 16) | responseBuffer[1];

  /* Dissolved Oxygen (DO): Combine the two 16-bit values into a single 32-bit value*/
  uint32_t doHex = ((uint32_t)responseBuffer[2] << 16) | responseBuffer[3];

  /* Dissolved Oxygen mg/L (DOmg): Combine the two 16-bit values into a single 32-bit value*/
  uint32_t domgHex = ((uint32_t)responseBuffer[4] << 16) | responseBuffer[5];

  /*tracting individual bytes and combining them into single uint32_t variables for each value*/
  uint8_t byte1_temp = (temperatureHex >> 24) & 0xFF;
  uint8_t byte2_temp = (temperatureHex >> 16) & 0xFF;
  uint8_t byte3_temp = (temperatureHex >> 8) & 0xFF;
  uint8_t byte4_temp = temperatureHex & 0xFF;
  uint32_t combinedValue_temp = ((uint32_t)byte4_temp << 24) | ((uint32_t)byte3_temp << 16) | ((uint32_t)byte2_temp << 8) | byte1_temp;

  uint8_t byte1_do = (doHex >> 24) & 0xFF;
  uint8_t byte2_do = (doHex >> 16) & 0xFF;
  uint8_t byte3_do = (doHex >> 8) & 0xFF;
  uint8_t byte4_do = doHex & 0xFF;
  uint32_t combinedValue_do = ((uint32_t)byte4_do << 24) | ((uint32_t)byte3_do << 16) | ((uint32_t)byte2_do << 8) | byte1_do;

  uint8_t byte1_domg = (domgHex >> 24) & 0xFF;
  uint8_t byte2_domg = (domgHex >> 16) & 0xFF;
  uint8_t byte3_domg = (domgHex >> 8) & 0xFF;
  uint8_t byte4_domg = domgHex & 0xFF;
  uint32_t combinedValue_domg = ((uint32_t)byte4_domg << 24) | ((uint32_t)byte3_domg << 16) | ((uint32_t)byte2_domg << 8) | byte1_domg;

  /* Union for reinterpreting bit pattern as float*/
  FloatConverter tempAndDoValues;

  /*Reinterpret the bit patterns as float*/
  tempAndDoValues.uintValue = combinedValue_temp;
  *temperature = tempAndDoValues.floatValue;

  tempAndDoValues.uintValue = combinedValue_do;
  *dissolvedOxygen = tempAndDoValues.floatValue * 100;

  tempAndDoValues.uintValue = combinedValue_domg;
  *dissolvedOxygenMg = tempAndDoValues.floatValue;
}

/************************************************************
 * Function to extract the data that we fot from the Sensor through Modbus RTU
 * @param responseBuffer  Buffer to store the response for the sent command
 * @param k  k variable to store the data
 * @param b  b variable to store the data
 *************************************************************/
void CSensor::extractCalValues(uint16_t responseBuffer[], float *k, float *b)
{
  /* K value: Combine the two 16-bit values into a single 32-bit value*/
  uint32_t kValue = ((uint32_t)responseBuffer[0] << 16) | responseBuffer[1];

  /*  B value: Combine the two 16-bit values into a single 32-bit value*/
  uint32_t bValue = ((uint32_t)responseBuffer[2] << 16) | responseBuffer[3];

  /*tracting individual bytes and combining them into single uint32_t variables for each value*/
  uint8_t byte1_k = (kValue >> 24) & 0xFF;
  uint8_t byte2_k = (kValue >> 16) & 0xFF;
  uint8_t byte3_k = (kValue >> 8) & 0xFF;
  uint8_t byte4_k = kValue & 0xFF;
  uint32_t combinedValue_k = ((uint32_t)byte4_k << 24) | ((uint32_t)byte3_k << 16) | ((uint32_t)byte2_k << 8) | byte1_k;

  uint8_t byte1_b = (bValue >> 24) & 0xFF;
  uint8_t byte2_b = (bValue >> 16) & 0xFF;
  uint8_t byte3_b = (bValue >> 8) & 0xFF;
  uint8_t byte4_b = bValue & 0xFF;
  uint32_t combinedValue_b = ((uint32_t)byte4_b << 24) | ((uint32_t)byte3_b << 16) | ((uint32_t)byte2_b << 8) | byte1_b;

  FloatConverter CalValues;

  /*Reinterpret the bit patterns as float*/
  CalValues.uintValue = combinedValue_k;
  *k = CalValues.floatValue;

  CalValues.uintValue = combinedValue_b;
  *b = CalValues.floatValue;
}

/************************************************************
 * Function to extract the data that we fot from the Sensor through Modbus RTU
 * @param responseBuffer  Buffer to store the response for the sent command
 * @param val val variable to store the data
 *************************************************************/
void CSensor::extractValues(uint16_t responseBuffer[], float *val)
{
  /* val value: Combine the two 16-bit values into a single 32-bit value*/
  uint32_t Value = ((uint32_t)responseBuffer[0] << 16) | responseBuffer[1];

  /*tracting individual bytes and combining them into single uint32_t variables for each value*/
  uint8_t byte1 = (Value >> 24) & 0xFF;
  uint8_t byte2 = (Value >> 16) & 0xFF;
  uint8_t byte3 = (Value >> 8) & 0xFF;
  uint8_t byte4 = Value & 0xFF;
  uint32_t combinedValue = ((uint32_t)byte4 << 24) | ((uint32_t)byte3 << 16) | ((uint32_t)byte2 << 8) | byte1;

  FloatConverter CalValuess;

  /*Reinterpret the bit patterns as float*/
  CalValuess.uintValue = combinedValue;
  *val = CalValuess.floatValue;
}

/*Funtion to set the calibration vales*/
int CSensor::setCalibrationValues(void)
{
  node.clearTransmitBuffer();

  float k = 100 / m_fDo;
  debugPrintln(m_fDo);
  debugPrintln(k);
  uint16_t val1, val2;
  float_to_hex_le(k, &val1, &val2);
  // debugPrintln(val1, HEX);
  // debugPrintln(val2, HEX);

  uint16_t values[4] = {val1, val2, 0, 0};
  debugPrintln("....................");
  for (uint8_t i = 0; i < 4; i++)
  {
    debugPrint(values[i], HEX);
    debugPrint(" ");
    node.setTransmitBuffer(0 + i, values[i]);
  }
  debugPrintln("....................");
  // Write multiple registers
  uint8_t result = node.writeMultipleRegisters(0x1100, 0x04);
  // uint8_t result = node.writeMultipleRegisters(CalData.startAddress, CalData.numRegisters);

  if (result == node.ku8MBSuccess)
  {
    debugPrintln("Command successfully sent.");
  }
  else
  {
    debugPrintln("Error sending command.");
    return 0;
  }
  return 1;
}

/*Funtion to set the calibration vales*/
int CSensor::setSalinity(void)
{
  node.clearTransmitBuffer();

  uint16_t val1, val2;
  float_to_hex_le(m_fSalinity, &val1, &val2);
  debugPrintln(val1, HEX);
  debugPrintln(val2, HEX);

  uint16_t values[2] = {val1, val2};

  for (uint8_t i = 0; i < salinity.numRegisters; i++)
  {
    node.setTransmitBuffer(0 + i, values[i]);
  }

  // Write multiple registers
  uint8_t result = node.writeMultipleRegisters(salinity.startAddress, salinity.numRegisters);
  debugPrintf("result %u\n", result);
  if (result == node.ku8MBSuccess)
  {
    debugPrintln("Command successfully sent.");
  }
  else
  {
    debugPrintln("Error sending command.");
    return 0;
  }
  return 1;
}

/*Funtion to set the calibration vales*/
int CSensor::setPressure(void)
{
  node.clearTransmitBuffer();

  uint16_t val1, val2;
  float_to_hex_le(m_fPressure, &val1, &val2);
  debugPrintln(val1, HEX);
  debugPrintln(val2, HEX);

  uint16_t values[2] = {val1, val2};

  for (uint8_t i = 0; i < pressure.numRegisters; i++)
  {
    node.setTransmitBuffer(0 + i, values[i]);
  }

  // Write multiple registers
  uint8_t result = node.writeMultipleRegisters(pressure.startAddress, pressure.numRegisters);

  if (result == node.ku8MBSuccess)
  {
    debugPrintln("Command successfully sent.");
  }
  else
  {
    debugPrintln("Error sending command.");
    return 0;
  }
  return 1;
}

/*Function to Start the measurement*/
void CSensor::startMeasurement(void)
{
  /*Function to get the response from sensor*/
  getDataFrmSensor(startMsrmnt.startAddress, startMsrmnt.numRegisters, startMsrmnt.responseBuffer);
}

/*Funtion to get the Temperatre and DO vales with retry mechanism*/
void CSensor::getTempAndDoValues(void)
{
  int val = getDataFrmSensor(getTempDoVals.startAddress, getTempDoVals.numRegisters, getTempDoVals.responseBuffer);
  if (val)
  {
    // Temporary variables to hold extracted values
    float tempReading, doReading, doMglReading;

    /*Extract the values from sensor response*/
    extractTempAndDoValues(getTempDoVals.responseBuffer, &tempReading, &doReading, &doMglReading);

    // Validate extracted values with strict filtering
    bool validTemp = (tempReading >= 0.0 && tempReading <= 60.0 && !isnan(tempReading) && !isinf(tempReading));
    bool validDo = (doReading >= 0.0 && doReading <= 200.0 && !isnan(doReading) && !isinf(doReading));
    bool validDoMgl = (doMglReading >= 0.0 && doMglReading <= 25.0 && !isnan(doMglReading) && !isinf(doMglReading));

    // Filter out extreme invalid values like -23487294
    if (tempReading < 0.0 || tempReading > 100.0)
      validTemp = false;
    if (doReading < 0.0 || doReading > 200.0)
      validDo = false;
    if (doMglReading <= 0.0 || doMglReading > 25.0)
      validDoMgl = false;

    // Only update values if they are valid
    if (validTemp && validDo && validDoMgl)
    {
      m_fTemp = tempReading;
      m_fDo = doReading;
      m_fDoMgl = doMglReading;

      noSensor = false;

      debugPrintf(" [DoSensor][Success] Temp: %.2f, DO%%: %.2f, DO mg/L: %.2f\n", m_fTemp, m_fDo, m_fDoMgl);

      if (!m_fSalinity)
        getSalinity();
    }
    else
    {
      debugPrintf(" [DoSensor][InvalidValues] Temp: %.2f (valid:%d), DO%%: %.2f (valid:%d), DO mg/L: %.2f (valid:%d)\n", tempReading, validTemp, doReading, validDo, doMglReading, validDoMgl);
    }
  }
  else
  {
    noSensor = true;
    m_fDo = 0.0;
    m_fDoMgl = 0.0;
    m_fTemp = 0.0;
    debugPrintln(" [DoSensor][CommFail] Modbus communication failed\n");
  }
  debugPrint("Final Values - Temp : ");
  debugPrintln(m_fTemp);
  debugPrint("DO(%) : ");
  debugPrintln(m_fDo);
  debugPrint("DO(mg/L) : ");
  debugPrintln(m_fDoMgl);
}

/*Function to Stop the measurement*/
void CSensor::stopMeasurement(void)
{
  /*Function to get the response from sensor*/
  getDataFrmSensor(stopMsrmnt.startAddress, stopMsrmnt.numRegisters, stopMsrmnt.responseBuffer);
}

/*Funtion to get the Serial Number of the sensor*/
void CSensor::getSerialNumber(void)
{
  /*Function to get the response from sensor*/
  getDataFrmSensor(getSerialNum.startAddress, getSerialNum.numRegisters, getSerialNum.responseBuffer);
}

/*Funtion to get the calibration vales*/
void CSensor::getCalibrationValues(void)
{
  /*Function to get the response from sensor*/
  getDataFrmSensor(0x1100, 0x04, CalData.responseBuffer);
  /*Extract the vales whatever we're getting from the sensor*/
  extractCalValues(CalData.responseBuffer, &m_iK, &m_iB);
  debugPrint("K ");
  debugPrintln(m_iK);
  debugPrint("B ");
  debugPrintln(m_iB);
}

/*Funtion to get the salinity vales*/
void CSensor::getSalinity(void)
{
  /*Function to get the response from sensor*/
  getDataFrmSensor(salinity.startAddress, salinity.numRegisters, salinity.responseBuffer);
  /*Extract the vales whatever we're getting from the sensor*/
  extractValues(salinity.responseBuffer, &m_fSalinity);
  debugPrint("salinity : ");
  debugPrintln(m_fSalinity);
}

/*Funtion to get the calibration vales*/
void CSensor::getPressure(void)
{
  /*Function to get the response from sensor*/
  getDataFrmSensor(pressure.startAddress, pressure.numRegisters, pressure.responseBuffer);
  /*Extract the vales whatever we're getting from the sensor*/
  extractValues(pressure.responseBuffer, &m_fPressure);
  debugPrint("Pressure : ");
  debugPrintln(m_fPressure);
}

/*FUnction to get the average of 10 consecutive DO values*/
void CSensor::calculateDoValue(void)
{
  float temp = 0.0;
  for (uint8_t val = 0; val < 10; val++)
  {
    temp += Dovalues[val];
  }
  temp = temp / 10;
  m_fDo = temp;
}