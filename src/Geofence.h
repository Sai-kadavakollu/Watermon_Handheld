#ifndef GEOFENCE_H
#define GEOFENCE_H

#include "Utils.h"

typedef struct
{
    double m_lat;
    double m_lng;
} m_oPosition;

#define MAXDISTFROMGEOFENCE 5
#define MINDISTFROMGEOFENCE 1

class Geofence
{
public:
    int nearestEdgeVertices(m_oPosition curLocation, m_oPosition geofence[], int n);
    int isInside(m_oPosition geofence[], int n, m_oPosition curLocation);
    double isBotInsideGeofence(m_oPosition geofence[], int n, m_oPosition curLocation);
    double distanceTom_oPosition(m_oPosition point, m_oPosition p1, m_oPosition p2, m_oPosition *closestPoint);
    double distance(m_oPosition p1, m_oPosition p2);
};

#endif
