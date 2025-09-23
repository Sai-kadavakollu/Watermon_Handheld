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
        hDop = -1;
        m_iSatellites = 0;
    }
    // ~cPosition();
};


class CGps
{
private:
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

    cPosition mPosition;
    cPosition getPondLocation(void);
    time_t getEpoch(void);
    void gpstask(void);
};
#endif