#ifndef MERCATOR_H
#define MERCATOR_H

#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

typedef struct {
    double center_lon;
    double center_lat;
    double zoom;
} Viewport;

static inline double mercator_scale(double zoom) {
    return pow(2.0, zoom) * 256.0;
}

static inline double lon_to_world_x(double lon, double scale) {
    return (lon + 180.0) / 360.0 * scale;
}

static inline double lat_to_world_y(double lat, double scale) {
    double lat_rad = lat * M_PI / 180.0;
    return (1.0 - log(tan(lat_rad) + 1.0 / cos(lat_rad)) / M_PI) / 2.0 * scale;
}

static inline double lon_to_pixel(double lon, const Viewport *vp, int screen_width) {
    double scale = mercator_scale(vp->zoom);
    double world_x = lon_to_world_x(lon, scale);
    double cam_x = lon_to_world_x(vp->center_lon, scale);
    return world_x - cam_x + screen_width / 2.0;
}

static inline double lat_to_pixel(double lat, const Viewport *vp, int screen_height) {
    double scale = mercator_scale(vp->zoom);
    double world_y = lat_to_world_y(lat, scale);
    double cam_y = lat_to_world_y(vp->center_lat, scale);
    return world_y - cam_y + screen_height / 2.0;
}

static inline double pixel_to_lon(double px, const Viewport *vp, int screen_width) {
    double scale = mercator_scale(vp->zoom);
    double cam_x = lon_to_world_x(vp->center_lon, scale);
    double world_x = px - screen_width / 2.0 + cam_x;
    return world_x / scale * 360.0 - 180.0;
}

static inline double pixel_to_lat(double py, const Viewport *vp, int screen_height) {
    double scale = mercator_scale(vp->zoom);
    double cam_y = lat_to_world_y(vp->center_lat, scale);
    double world_y = py - screen_height / 2.0 + cam_y;
    double n = M_PI - 2.0 * M_PI * world_y / scale;
    return 180.0 / M_PI * atan(0.5 * (exp(n) - exp(-n)));
}

/* Compute tile coordinates for a given world position at integer zoom level */
static inline void world_to_tile(double lon, double lat, int z, int *tx, int *ty) {
    double scale = pow(2.0, z) * 256.0;
    double wx = lon_to_world_x(lon, scale);
    double wy = lat_to_world_y(lat, scale);
    *tx = (int)(wx / 256.0);
    *ty = (int)(wy / 256.0);
    int max_tile = 1 << z;
    if (*tx < 0) *tx = 0;
    if (*tx >= max_tile) *tx = max_tile - 1;
    if (*ty < 0) *ty = 0;
    if (*ty >= max_tile) *ty = max_tile - 1;
}

/* Get the pixel offset of a tile's top-left corner on screen */
static inline void tile_screen_pos(int tx, int ty, int z, const Viewport *vp,
                                    int screen_width, int screen_height,
                                    double *sx, double *sy) {
    double scale = pow(2.0, z) * 256.0;
    double tile_world_x = tx * 256.0;
    double tile_world_y = ty * 256.0;
    double cam_x = lon_to_world_x(vp->center_lon, scale);
    double cam_y = lat_to_world_y(vp->center_lat, scale);
    *sx = tile_world_x - cam_x + screen_width / 2.0;
    *sy = tile_world_y - cam_y + screen_height / 2.0;
}

#endif /* MERCATOR_H */
