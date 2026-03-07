#include "mercator.h"
#include <assert.h>
#include <stdio.h>
#include <math.h>

#define EPSILON 1e-6
#define ASSERT_NEAR(a, b) assert(fabs((a) - (b)) < EPSILON)

static void test_mercator_scale(void) {
    ASSERT_NEAR(mercator_scale(0), 256.0);
    ASSERT_NEAR(mercator_scale(1), 512.0);
    ASSERT_NEAR(mercator_scale(2), 1024.0);
    ASSERT_NEAR(mercator_scale(10), 262144.0);
    printf("  PASS: mercator_scale\n");
}

static void test_lon_to_world_x(void) {
    double scale = mercator_scale(0); /* 256 */
    /* lon = -180 -> world_x = 0 */
    ASSERT_NEAR(lon_to_world_x(-180.0, scale), 0.0);
    /* lon = 0 -> world_x = 128 */
    ASSERT_NEAR(lon_to_world_x(0.0, scale), 128.0);
    /* lon = 180 -> world_x = 256 */
    ASSERT_NEAR(lon_to_world_x(180.0, scale), 256.0);
    printf("  PASS: lon_to_world_x\n");
}

static void test_lat_to_world_y_equator(void) {
    double scale = mercator_scale(0);
    /* lat = 0 -> world_y = 128 (center of the world) */
    ASSERT_NEAR(lat_to_world_y(0.0, scale), 128.0);
    printf("  PASS: lat_to_world_y equator\n");
}

static void test_lat_to_world_y_northern(void) {
    double scale = mercator_scale(0);
    /* Northern latitudes should give world_y < 128 */
    double wy = lat_to_world_y(45.0, scale);
    assert(wy > 0 && wy < 128.0);
    printf("  PASS: lat_to_world_y northern\n");
}

static void test_lon_pixel_roundtrip(void) {
    Viewport vp = {.center_lon = -1.5, .center_lat = 50.0, .zoom = 10.0};
    int w = 1280;

    /* Pick a test lon, convert to pixel, convert back */
    double test_lon = -0.5;
    double px = lon_to_pixel(test_lon, &vp, w);
    double recovered = pixel_to_lon(px, &vp, w);
    ASSERT_NEAR(recovered, test_lon);
    printf("  PASS: lon pixel roundtrip\n");
}

static void test_lat_pixel_roundtrip(void) {
    Viewport vp = {.center_lon = -1.5, .center_lat = 50.0, .zoom = 10.0};
    int h = 800;

    double test_lat = 50.5;
    double py = lat_to_pixel(test_lat, &vp, h);
    double recovered = pixel_to_lat(py, &vp, h);
    assert(fabs(recovered - test_lat) < 1e-4); /* Slightly looser for lat */
    printf("  PASS: lat pixel roundtrip\n");
}

static void test_center_maps_to_screen_center(void) {
    Viewport vp = {.center_lon = 10.0, .center_lat = 45.0, .zoom = 8.0};
    int w = 1280, h = 800;

    double px = lon_to_pixel(vp.center_lon, &vp, w);
    double py = lat_to_pixel(vp.center_lat, &vp, h);

    ASSERT_NEAR(px, w / 2.0);
    ASSERT_NEAR(py, h / 2.0);
    printf("  PASS: center maps to screen center\n");
}

static void test_world_to_tile(void) {
    int tx, ty;
    /* At zoom 0, the whole world is one tile (0,0) */
    world_to_tile(0.0, 0.0, 0, &tx, &ty);
    assert(tx == 0 && ty == 0);

    /* At zoom 1, (lon=0, lat=0) is tile (1,1) */
    world_to_tile(0.0, 0.0, 1, &tx, &ty);
    assert(tx == 1 && ty == 1);

    /* Negative lon at zoom 1 should be tile 0 */
    world_to_tile(-90.0, 0.0, 1, &tx, &ty);
    assert(tx == 0);

    printf("  PASS: world_to_tile\n");
}

static void test_tile_screen_pos(void) {
    Viewport vp = {.center_lon = 0.0, .center_lat = 0.0, .zoom = 1.0};
    int w = 512, h = 512;
    double sx, sy;

    /* Tile (1,1) at zoom 1 should be near center for center=(0,0) */
    tile_screen_pos(1, 1, 1, &vp, w, h, &sx, &sy);
    /* world position of tile(1,1) = (256, 256), cam at center = (256, 256)
       screen = 256 - 256 + 256 = 256 */
    ASSERT_NEAR(sx, 256.0);
    ASSERT_NEAR(sy, 256.0);
    printf("  PASS: tile_screen_pos\n");
}

static void test_multiple_zoom_levels(void) {
    /* Roundtrip at various zoom levels */
    Viewport vp = {.center_lon = -3.0, .center_lat = 50.5, .zoom = 0.0};
    int w = 1280, h = 800;

    for (int z = 2; z <= 18; z++) {
        vp.zoom = (double)z;
        double test_lon = -2.0;
        double test_lat = 51.0;

        double px = lon_to_pixel(test_lon, &vp, w);
        double py = lat_to_pixel(test_lat, &vp, h);
        double rl = pixel_to_lon(px, &vp, w);
        double ra = pixel_to_lat(py, &vp, h);

        assert(fabs(rl - test_lon) < 1e-3);
        assert(fabs(ra - test_lat) < 1e-3);
    }
    printf("  PASS: roundtrip at multiple zoom levels\n");
}

int main(void) {
    printf("Running mercator tests...\n");
    test_mercator_scale();
    test_lon_to_world_x();
    test_lat_to_world_y_equator();
    test_lat_to_world_y_northern();
    test_lon_pixel_roundtrip();
    test_lat_pixel_roundtrip();
    test_center_maps_to_screen_center();
    test_world_to_tile();
    test_tile_screen_pos();
    test_multiple_zoom_levels();
    printf("All mercator tests passed.\n");
    return 0;
}
