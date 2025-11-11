/**
 * @file geofence_ops.cpp
 * @brief Geofence Operations Implementation
 * @author Watermon Team
 * @date 2025
 * 
 * @details
 * Implementation of geofence operations for polygon containment checking
 * and distance calculations using geodesic and Euclidean methods.
 */

#include "geofence_ops.h"
#include <Arduino.h>
#include <string.h>
#include <math.h>

// #define DEBUG_ENABLE
#ifdef DEBUG_ENABLE
#define DEBUG_print(...) Serial.print(__VA_ARGS__)
#define DEBUG_println(...) Serial.println(__VA_ARGS__)
#define DEBUG_printf(...) Serial.printf(__VA_ARGS__)
#else
#define DEBUG_print(...)
#define DEBUG_println(...)
#define DEBUG_printf(...)
#endif

#define SUCCESS 1
#define FAIL 0

/* ========================================================================
 * HELPER FUNCTIONS
 * ======================================================================== */

/**
 * @brief Calculate geodesic distance using Haversine formula
 * @param p1 First position
 * @param p2 Second position
 * @return Distance in meters
 */
static double haversine(position_t p1, position_t p2)
{
    double R = 6371000.0; // Earth radius in meters
    double lat1 = p1.lat * M_PI / 180.0;
    double lat2 = p2.lat * M_PI / 180.0;
    double dlat = (p2.lat - p1.lat) * M_PI / 180.0;
    double dlng = (p2.lng - p1.lng) * M_PI / 180.0;

    double a = sin(dlat / 2) * sin(dlat / 2) +
               cos(lat1) * cos(lat2) *
               sin(dlng / 2) * sin(dlng / 2);
    double c = 2 * atan2(sqrt(a), sqrt(1 - a));
    return R * c;
}

/**
 * @brief Find closest point on line segment to given point
 * @param A Line segment start
 * @param B Line segment end
 * @param P Point to project
 * @return Closest point on segment AB to point P
 */
static position_t closest_point_on_segment(position_t A, position_t B, position_t P)
{
    // Convert to cartesian plane approximation (small area assumption)
    double Ax = A.lng, Ay = A.lat;
    double Bx = B.lng, By = B.lat;
    double Px = P.lng, Py = P.lat;

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

    position_t closest;
    closest.lng = Ax + ABx * t;
    closest.lat = Ay + ABy * t;
    return closest;
}

/* ========================================================================
 * STANDARD GEOFENCE OPERATIONS IMPLEMENTATION
 * ======================================================================== */

/**
 * @brief Calculate Euclidean distance between two positions
 */
static double standard_geofence_distance(struct geofence_device *geofence, position_t p1, position_t p2)
{
    (void)geofence; // Unused parameter
    return sqrt((p1.lat - p2.lat) * (p1.lat - p2.lat) + 
                (p1.lng - p2.lng) * (p1.lng - p2.lng));
}

/**
 * @brief Check if point is inside polygon using ray casting algorithm
 */
static int standard_geofence_is_inside(struct geofence_device *geofence, position_t boundary[], 
                                      int num_points, position_t point)
{
    (void)geofence; // Unused parameter
    
    int i, j, c = 0;
    for (i = 0, j = num_points - 1; i < num_points; j = i++)
    {
        if (((boundary[i].lng > point.lng) != (boundary[j].lng > point.lng)) &&
            (point.lat < (boundary[j].lat - boundary[i].lat) * (point.lng - boundary[i].lng) / 
             (boundary[j].lng - boundary[i].lng) + boundary[i].lat))
            c = !c;
    }
    return c;
}

/**
 * @brief Calculate distance from point to line segment
 */
static double standard_geofence_distance_to_segment(struct geofence_device *geofence, position_t point, 
                                                    position_t p1, position_t p2, position_t *closest_point)
{
    double A = point.lat - p1.lat;
    double B = point.lng - p1.lng;
    double C = p2.lat - p1.lat;
    double D = p2.lng - p1.lng;

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

    double closest_lat, closest_lng;

    if (param < 0)
    {
        // Closest point is p1
        closest_lat = p1.lat;
        closest_lng = p1.lng;
    }
    else if (param > 1)
    {
        // Closest point is p2
        closest_lat = p2.lat;
        closest_lng = p2.lng;
    }
    else
    {
        // Closest point is on the line segment
        closest_lat = p1.lat + param * C;
        closest_lng = p1.lng + param * D;
    }

    if (closest_point != NULL)
    {
        closest_point->lat = closest_lat;
        closest_point->lng = closest_lng;
    }

    // Calculate and return the distance between the input point and the closest point
    position_t closest_pos = {closest_lat, closest_lng};
    return standard_geofence_distance(geofence, point, closest_pos);
}

/**
 * @brief Find nearest edge vertices from current position
 */
static int standard_geofence_nearest_edge_vertices(struct geofence_device *geofence, position_t boundary[], 
                                                   int num_points, position_t point)
{
    double min_distance = standard_geofence_distance_to_segment(geofence, point, boundary[0], boundary[1], NULL);
    DEBUG_printf(" MinDistance 1 : %f \n", min_distance);
    int nearest_edge_index = 0;

    for (int i = 1; i < num_points; i++)
    {
        double dist = standard_geofence_distance_to_segment(geofence, point, boundary[i], 
                                                            boundary[(i + 1) % num_points], NULL);
        DEBUG_printf(" dist %d : %f \n", i, dist);
        if (dist <= min_distance)
        {
            min_distance = dist;
            nearest_edge_index = i;
        }
    }
    
    // Print the result, indicating which vertices form the nearest edge
    DEBUG_printf("Total points in Geofence : %d\n", num_points);
    DEBUG_printf("Nearest edge is between post %d and %d\n", nearest_edge_index, ((nearest_edge_index + 1) % num_points));
    return nearest_edge_index;
}

/**
 * @brief Calculate distance to geofence boundary (0 if inside)
 */
static double standard_geofence_distance_to_boundary(struct geofence_device *geofence, position_t boundary[], 
                                                     int num_points, position_t point)
{
    if (!standard_geofence_is_inside(geofence, boundary, num_points, point))
    {
        double min_dist = 1e12; // large value

        for (int i = 0; i < num_points; i++)
        {
            position_t A = boundary[i];
            position_t B = boundary[(i + 1) % num_points]; // next vertex, wrap around

            // Find closest point on edge AB to current location
            position_t closest = closest_point_on_segment(A, B, point);

            // Measure geodesic distance
            double d = haversine(point, closest);
            if (d < min_dist)
                min_dist = d;
        }
        return min_dist;
    }
    DEBUG_println("@@ bot inside");
    return 0;
}

/* ========================================================================
 * OPERATIONS TABLE
 * ======================================================================== */

const struct geofence_ops standard_geofence_ops = {
    .is_inside = standard_geofence_is_inside,
    .distance_to_boundary = standard_geofence_distance_to_boundary,
    .nearest_edge_vertices = standard_geofence_nearest_edge_vertices,
    .distance = standard_geofence_distance,
    .distance_to_segment = standard_geofence_distance_to_segment
};

/* ========================================================================
 * PUBLIC API FUNCTIONS
 * ======================================================================== */

int geofence_init(struct geofence_device *geofence, const char *name, const struct geofence_ops *ops)
{
    if (!geofence || !ops) return FAIL;
    
    memset(geofence, 0, sizeof(struct geofence_device));
    geofence->name = name;
    geofence->ops = ops;
    geofence->is_initialized = 1;
    
    return SUCCESS;
}

int geofence_is_inside(struct geofence_device *geofence, position_t boundary[], 
                       int num_points, position_t point)
{
    if (!geofence || !geofence->ops || !geofence->ops->is_inside) return 0;
    return geofence->ops->is_inside(geofence, boundary, num_points, point);
}

double geofence_distance_to_boundary(struct geofence_device *geofence, position_t boundary[], 
                                     int num_points, position_t point)
{
    if (!geofence || !geofence->ops || !geofence->ops->distance_to_boundary) return -1.0;
    return geofence->ops->distance_to_boundary(geofence, boundary, num_points, point);
}

int geofence_nearest_edge_vertices(struct geofence_device *geofence, position_t boundary[], 
                                   int num_points, position_t point)
{
    if (!geofence || !geofence->ops || !geofence->ops->nearest_edge_vertices) return -1;
    return geofence->ops->nearest_edge_vertices(geofence, boundary, num_points, point);
}

double geofence_distance(struct geofence_device *geofence, position_t p1, position_t p2)
{
    if (!geofence || !geofence->ops || !geofence->ops->distance) return -1.0;
    return geofence->ops->distance(geofence, p1, p2);
}

double geofence_distance_to_segment(struct geofence_device *geofence, position_t point, 
                                    position_t p1, position_t p2, position_t *closest_point)
{
    if (!geofence || !geofence->ops || !geofence->ops->distance_to_segment) return -1.0;
    return geofence->ops->distance_to_segment(geofence, point, p1, p2, closest_point);
}

void geofence_cleanup(struct geofence_device *geofence)
{
    if (!geofence) return;
    
    if (geofence->priv)
    {
        // Free any private data if allocated
        geofence->priv = NULL;
    }
    
    geofence->is_initialized = 0;
}
