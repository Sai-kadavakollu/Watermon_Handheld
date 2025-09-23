#ifndef CSensor_H
#define CSensor_H

#include <stdint.h>
#include <Arduino.h>
#include <time.h>
#include <ModbusMaster.h>

#define SUCCESS 1
#define FAIL 0
#define ERROR -1

typedef struct
{
    uint16_t startAddress;
    uint16_t numRegisters;
    uint16_t responseBuffer[];
} m_oDOsensor;

typedef union 
{
    uint32_t uintValue;
    float floatValue;
}FloatConverter;

class CSensor
{
private:
    void extractTempAndDoValues(uint16_t responseBuffer[], float *temperature, float *dissolvedOxygen, float *dissolvedOxygenMg);
    void extractCalValues(uint16_t responseBuffer[], float *k, float *b);
    void extractValues(uint16_t responseBuffer[], float *val);

public:
    /* Construct */
    CSensor();
    /* Destruct */
    ~CSensor();

    float m_fDo;
    float m_fDoMgl;
    float m_fTemp;
    char SnNumber[15];
    float m_iK;
    float m_fSalinity;
    float m_fPressure;
    float m_iB;
    float Dovalues[10];
    bool startRead;
    bool readValues;
    bool noSensor;

    void sensorInit(Stream &serial, uint8_t slaveID = 0x01);
    int getDataFrmSensor(uint16_t startAddress, uint16_t numRegisters, uint16_t *responseBuffer);
    void getSerialNumber(void);
    void startMeasurement(void);
    void stopMeasurement(void);
    void getTempAndDoValues(void);
    void getSalinity(void);
    void getPressure(void);
    int setCalibrationValues(void);
    void getCalibrationValues(void);
    void calculateDoValue(void);
    int setSalinity(void);
    int setPressure(void);
};
#endif