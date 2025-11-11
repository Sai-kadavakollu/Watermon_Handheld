/**
 * @file geofence_ops.h
 * @brief Geofence Operations Interface - C-Style Function Pointer Pattern
 * @author Watermon Team
 * @date 2025
 * 
 * @details
 * This header defines the geofence operations interface using C-style structures
 * with function pointers (Linux kernel driver pattern). This approach provides:
 * - Clear separation between interface and implementation
 * - No virtual function overhead
 * - Easy swapping of implementations
 * - Testability through mock implementations
 * 
 * @par Usage Pattern:
 * @code
 * // 1. Declare device
 * struct geofence_device g_geofence;
 * 
 * // 2. Initialize with ops table
 * geofence_init(&g_geofence, "Geofence", &standard_geofence_ops);
 * 
 * // 3. Use operations
 * int inside = geofence_is_inside(&g_geofence, boundary, num_points, current_pos);
 * double dist = geofence_distance_to_boundary(&g_geofence, boundary, num_points, current_pos);
 * @endcode
 * 
 * @see geofence_ops.cpp for implementation details
 */

#ifndef GEOFENCE_OPS_H
#define GEOFENCE_OPS_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declarations */
struct geofence_device;

/**
 * @brief Position structure (latitude/longitude)
 */
typedef struct {
    double lat;  /**< Latitude in degrees */
    double lng;  /**< Longitude in degrees */
} position_t;

/**
 * @brief Backward compatibility typedef for old code
 * @deprecated Use position_t instead
 */
typedef struct {
    double m_lat;  /**< Latitude in degrees */
    double m_lng;  /**< Longitude in degrees */
} m_oPosition;

/**
 * @struct geofence_ops
 * @brief Geofence Operations Structure
 * 
 * @details
 * Contains function pointers for all geofence operations. This is the core
 * of the ops pattern - implementations register their functions here.
 * 
 * @note All function pointers must be non-NULL when registered
 */
struct geofence_ops {
    /** @brief Check if point is inside geofence polygon
     *  @param geofence Device structure
     *  @param boundary Array of boundary points
     *  @param num_points Number of boundary points
     *  @param point Current position to check
     *  @return 1 if inside, 0 if outside */
    int (*is_inside)(struct geofence_device *geofence, position_t boundary[], 
                     int num_points, position_t point);
    
    /** @brief Calculate distance to geofence boundary
     *  @param geofence Device structure
     *  @param boundary Array of boundary points
     *  @param num_points Number of boundary points
     *  @param point Current position
     *  @return Distance in meters (0 if inside) */
    double (*distance_to_boundary)(struct geofence_device *geofence, position_t boundary[], 
                                   int num_points, position_t point);
    
    /** @brief Find nearest edge vertices
     *  @param geofence Device structure
     *  @param boundary Array of boundary points
     *  @param num_points Number of boundary points
     *  @param point Current position
     *  @return Index of nearest edge start vertex */
    int (*nearest_edge_vertices)(struct geofence_device *geofence, position_t boundary[], 
                                 int num_points, position_t point);
    
    /** @brief Calculate distance between two positions
     *  @param geofence Device structure
     *  @param p1 First position
     *  @param p2 Second position
     *  @return Distance (Euclidean approximation) */
    double (*distance)(struct geofence_device *geofence, position_t p1, position_t p2);
    
    /** @brief Calculate distance from point to line segment
     *  @param geofence Device structure
     *  @param point Point to measure from
     *  @param p1 Line segment start
     *  @param p2 Line segment end
     *  @param closest_point Output: closest point on segment (can be NULL)
     *  @return Distance to line segment */
    double (*distance_to_segment)(struct geofence_device *geofence, position_t point, 
                                  position_t p1, position_t p2, position_t *closest_point);
};

/**
 * @struct geofence_device
 * @brief Geofence Device Structure
 * 
 * @details
 * Contains device state, configuration, and operations table.
 * This structure is passed to all geofence operations.
 * 
 * @par Lifecycle:
 * 1. Declare: `struct geofence_device g_geofence;`
 * 2. Initialize: `geofence_init(&g_geofence, ...)`
 * 3. Use: `geofence_is_inside(&g_geofence, ...)`
 * 4. Cleanup: `geofence_cleanup(&g_geofence)` (optional)
 */
struct geofence_device {
    const char *name;                   /**< Device name for logging */
    const struct geofence_ops *ops;     /**< Operations table (function pointers) */
    void *priv;                         /**< Private data (reserved for future use) */
    
    /* State (reserved for future use) */
    uint8_t is_initialized;             /**< 1 if device is initialized */
};

/* Standard Geofence Operations - exported for registration */
extern const struct geofence_ops standard_geofence_ops;

/* Public API Functions */

/**
 * @brief Initialize geofence device structure
 * @param geofence Pointer to geofence device structure
 * @param name Device name for logging
 * @param ops Operations table
 * @return 1 on success, 0 on failure
 */
int geofence_init(struct geofence_device *geofence, const char *name, const struct geofence_ops *ops);

/**
 * @brief Check if point is inside geofence polygon
 * @param geofence Pointer to geofence device structure
 * @param boundary Array of boundary points defining the polygon
 * @param num_points Number of points in boundary array
 * @param point Current position to check
 * @return 1 if inside, 0 if outside
 */
int geofence_is_inside(struct geofence_device *geofence, position_t boundary[], 
                       int num_points, position_t point);

/**
 * @brief Calculate distance to geofence boundary
 * @param geofence Pointer to geofence device structure
 * @param boundary Array of boundary points defining the polygon
 * @param num_points Number of points in boundary array
 * @param point Current position
 * @return Distance in meters (0 if inside geofence)
 */
double geofence_distance_to_boundary(struct geofence_device *geofence, position_t boundary[], 
                                     int num_points, position_t point);

/**
 * @brief Find nearest edge vertices of geofence from current position
 * @param geofence Pointer to geofence device structure
 * @param boundary Array of boundary points defining the polygon
 * @param num_points Number of points in boundary array
 * @param point Current position
 * @return Index of the start vertex of the nearest edge
 */
int geofence_nearest_edge_vertices(struct geofence_device *geofence, position_t boundary[], 
                                   int num_points, position_t point);

/**
 * @brief Calculate distance between two positions (Euclidean approximation)
 * @param geofence Pointer to geofence device structure
 * @param p1 First position
 * @param p2 Second position
 * @return Distance (approximation for small areas)
 */
double geofence_distance(struct geofence_device *geofence, position_t p1, position_t p2);

/**
 * @brief Calculate distance from point to line segment
 * @param geofence Pointer to geofence device structure
 * @param point Point to measure distance from
 * @param p1 Line segment start point
 * @param p2 Line segment end point
 * @param closest_point Output parameter for closest point on segment (can be NULL)
 * @return Distance from point to line segment
 */
double geofence_distance_to_segment(struct geofence_device *geofence, position_t point, 
                                    position_t p1, position_t p2, position_t *closest_point);

/**
 * @brief Cleanup geofence device (optional)
 * @param geofence Pointer to geofence device structure
 */
void geofence_cleanup(struct geofence_device *geofence);

/* ========================================================================
 * HELPER FUNCTIONS FOR BACKWARD COMPATIBILITY
 * ======================================================================== */

/**
 * @brief Wrapper for isBotInsideGeofence using old m_oPosition type
 * @param geofence Pointer to geofence device structure
 * @param boundary Array of boundary points (old format)
 * @param num_points Number of boundary points
 * @param point Current position (old format)
 * @return Distance in meters (0 if inside)
 */
static inline double geofence_is_bot_inside_geofence(struct geofence_device *geofence, 
                                                     m_oPosition boundary[], 
                                                     int num_points, 
                                                     m_oPosition point)
{
    // Convert old format to new format
    position_t new_boundary[num_points];
    for (int i = 0; i < num_points; i++)
    {
        new_boundary[i].lat = boundary[i].m_lat;
        new_boundary[i].lng = boundary[i].m_lng;
    }
    position_t new_point;
    new_point.lat = point.m_lat;
    new_point.lng = point.m_lng;
    
    return geofence_distance_to_boundary(geofence, new_boundary, num_points, new_point);
}

#ifdef __cplusplus
}
#endif

#endif /* GEOFENCE_OPS_H */
