#ifndef GPS_H
#define GPS_H

#include "TinyGPS++.h"
class cPosition
{
public:
    double m_lat;
    double m_lng;
    double hDop;
    int m_iSatellites;

    cPosition()
    {
        m_lat = 0.0;
        m_lng = 0.0;
        hDop = 0.0;
        m_iSatellites = 0;  
    }
    // ~cPosition();
};

class CGps
{
private:
    Stream* _gpsSerial; 
    time_t ConvertToEpoch(uint16_t year, uint8_t mon, uint8_t date, uint8_t hour, uint8_t min, uint8_t sec);

public:
    /* Construct */
    CGps();
    /* Destruct */
    ~CGps();

    int m_iCurrTime;
    time_t Epoch;
    int GpsMins;
    int GpsHour;
    int GpsDay;
    bool m_bIsValid = false;

    cPosition mPosition;
    void gpsInit(Stream *serialHandle);
    time_t getEpoch(void);
    void gpstask(void);
};
#endif