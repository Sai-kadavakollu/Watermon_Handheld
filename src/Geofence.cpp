#include "Geofence.h"
#include <Arduino.h>

// #define DEBUG_ENABLE
#ifdef DEBUG_ENABLE
#define DEBUG_print(...) Serial.print(__VA_ARGS__)
#define DEBUG_println(...) Serial.println(__VA_ARGS__)
#define DEBUG_printf(...) Serial.printf(__VA_ARGS__)
#else
#define DEBUG_print(...)   // blank line
#define DEBUG_println(...) // blank line
#define DEBUG_printf(...)  // blank line
#endif

/***********************************************************
 * Function to calculate the distance between two points
 ***********************************************************/
double Geofence::distance(m_oPosition p1, m_oPosition p2)
{
    return sqrt((p1.m_lat - p2.m_lat) * (p1.m_lat - p2.m_lat) + (p1.m_lng - p2.m_lng) * (p1.m_lng - p2.m_lng));
}

/*****************************************************************
 * find the nearest edge vertices of a geofence from the point
 * returns the edge vertices of the line which is near
 ******************************************************************/
int Geofence::nearestEdgeVertices(m_oPosition curLocation, m_oPosition geofence[], int n)
{
    double minDistance = distanceTom_oPosition(curLocation, geofence[0], geofence[1], NULL);
    DEBUG_printf(" MinDistance 1 : %f \n", minDistance);
    int nearestEdgeIndex = 0;

    for (int i = 1; i < n; i++)
    {
        double dist = distanceTom_oPosition(curLocation, geofence[i], geofence[(i + 1) % n], NULL);
        DEBUG_printf(" dist %d : %f \n", i, dist);
        if (dist <= minDistance)
        {
            minDistance = dist;
            nearestEdgeIndex = i;
        }
    }
    // Print the result, indicating which vertices form the nearest edge
    DEBUG_printf("Total points in Geofence : %d\n", n);
    DEBUG_printf("Nearest edge is between post %d and %d\n", nearestEdgeIndex, ((nearestEdgeIndex + 1) % n));
    return nearestEdgeIndex;
}

/************************************************************
 * function to check bot is inside the geofence or not
 *  1 : inside, 0 : outside
 *************************************************************/
int Geofence::isInside(m_oPosition geofence[], int n, m_oPosition curLocation)
{
    int i, j, c = 0;
    for (i = 0, j = n - 1; i < n; j = i++)
    {
        if (((geofence[i].m_lng > curLocation.m_lng) != (geofence[j].m_lng > curLocation.m_lng)) &&
            (curLocation.m_lat < (geofence[j].m_lat - geofence[i].m_lat) * (curLocation.m_lng - geofence[i].m_lng) / (geofence[j].m_lng - geofence[i].m_lng) + geofence[i].m_lat))
            c = !c;
    }
    return c;
}

/*********************************************************************
 * check bot is inside or outside the geofence
 * num : distance from the geofence when outside
 * 0 : inside
 *********************************************************************/
// double Geofence::isBotInsideGeofence(m_oPosition geofence[], int n, m_oPosition curLocation)
// {
//     double distFromGeofence = 0;
//     if(!isInside(geofence, n, curLocation))
//     {
//         m_oPosition vertx1,vertx2;
//         double a = 0.0, b = 0.0 ,c = 0.0;
//         int indx = nearestEdgeVertices(curLocation, geofence, n);
//         vertx1 = geofence[indx];
//         vertx2 = geofence[((indx + 1) % n)];
//         //distacne from vertx1 to vertx2
//         a = CalGPSDistance(vertx1.m_lat,vertx1.m_lng,vertx2.m_lat,vertx2.m_lng);
//         //distacne from vertx1 to current location
//         b = CalGPSDistance(vertx1.m_lat,vertx1.m_lng,curLocation.m_lat,curLocation.m_lng);
//         //distance from vertx2 to current location
//         c = CalGPSDistance(vertx2.m_lat,vertx2.m_lng,curLocation.m_lat,curLocation.m_lng);
//         DEBUG_printf("a : %f, b : %f, c : %f\n",a,b,c);
//         distFromGeofence = CalPerpendicularHeight(a,b,c);
//         DEBUG_print("distance from geofence : ");DEBUG_println(distFromGeofence);
//         return distFromGeofence;
//     }
//     DEBUG_println("@@ bot inside");
//     return distFromGeofence;
// }

static double haversine(m_oPosition p1, m_oPosition p2)
{
    double R = 6371000.0; // Earth radius in meters
    double lat1 = p1.m_lat * M_PI / 180.0;
    double lat2 = p2.m_lat * M_PI / 180.0;
    double dlat = (p2.m_lat - p1.m_lat) * M_PI / 180.0;
    double dlng = (p2.m_lng - p1.m_lng) * M_PI / 180.0;

    double a = sin(dlat / 2) * sin(dlat / 2) +
               cos(lat1) * cos(lat2) *
                   sin(dlng / 2) * sin(dlng / 2);
    double c = 2 * atan2(sqrt(a), sqrt(1 - a));
    return R * c;
}

// Project point onto segment and get closest point on edge
static m_oPosition closestPointOnSegment(m_oPosition A, m_oPosition B, m_oPosition P)
{
    // Convert to cartesian plane approximation (small area assumption)
    double Ax = A.m_lng, Ay = A.m_lat;
    double Bx = B.m_lng, By = B.m_lat;
    double Px = P.m_lng, Py = P.m_lat;

    double ABx = Bx - Ax;
    double ABy = By - Ay;
    double APx = Px - Ax;
    double APy = Py - Ay;

    double ab2 = ABx * ABx + ABy * ABy;
    double ap_ab = APx * ABx + APy * ABy;
    double t = (ab2 == 0) ? 0 : ap_ab / ab2;

    if (t < 0.0)
        t = 0.0;
    else if (t > 1.0)
        t = 1.0;

    m_oPosition closest;
    closest.m_lng = Ax + ABx * t;
    closest.m_lat = Ay + ABy * t;
    return closest;
}

double Geofence::isBotInsideGeofence(m_oPosition geofence[], int n, m_oPosition curLocation)
{
    if (!isInside(geofence, n, curLocation))
    {
        double minDist = 1e12; // large value

        for (int i = 0; i < n; i++)
        {
            m_oPosition A = geofence[i];
            m_oPosition B = geofence[(i + 1) % n]; // next vertex, wrap around

            // Find closest point on edge AB to current location
            m_oPosition closest = closestPointOnSegment(A, B, curLocation);

            // Measure geodesic distance
            double d = haversine(curLocation, closest);
            if (d < minDist)
                minDist = d;
        }
        return minDist;
    }
    DEBUG_println("@@ bot inside");
    return 0;
}

/***********************************************************************
 * Function to calculate the distance from a point to a line segment
 ***********************************************************************/
double Geofence::distanceTom_oPosition(m_oPosition point, m_oPosition p1, m_oPosition p2, m_oPosition *closestPoint)
{
    double A = point.m_lat - p1.m_lat;
    double B = point.m_lng - p1.m_lng;
    double C = p2.m_lat - p1.m_lat;
    double D = p2.m_lng - p1.m_lng;

    // Calculate the dot product of vectors (point - p1) and (p2 - p1)
    double dot = A * C + B * D;
    // Calculate the squared length of vector (p2 - p1)
    double len_sq = C * C + D * D;
    double param = -1;

    // Ensure that the denominator is not zero (len_sq)
    if (len_sq != 0)
    {
        // Calculate the parameter (param) representing the position of the closest point
        param = dot / len_sq;
    }

    double closestX, closestm_lng;

    if (param < 0)
    {
        // Closest point is p1
        closestX = p1.m_lat;
        closestm_lng = p1.m_lng;
    }
    else if (param > 1)
    {
        // Closest point is p2
        closestX = p2.m_lat;
        closestm_lng = p2.m_lng;
    }
    else
    {
        // Closest point is on the line segment
        closestX = p1.m_lat + param * C;
        closestm_lng = p1.m_lng + param * D;
    }

    if (closestPoint != NULL)
    {
        closestPoint->m_lat = closestX;
        closestPoint->m_lng = closestm_lng;
    }

    // Calculate and return the distance between the input point and the closest point
    return distance(point, (m_oPosition){closestX, closestm_lng});
}