#include "spec.h"
#include "data.h"
#include "tiles.h"
#include "render.h"
#include "mercator.h"
#include "raylib.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define WINDOW_WIDTH 1280
#define WINDOW_HEIGHT 800
#define ZOOM_MIN 2.0
#define ZOOM_MAX 20.0

static char *read_stdin(void) {
    size_t cap = 4096;
    size_t len = 0;
    char *buf = (char *)malloc(cap);
    if (!buf) return NULL;

    size_t n;
    while ((n = fread(buf + len, 1, cap - len - 1, stdin)) > 0) {
        len += n;
        if (len + 1 >= cap) {
            cap *= 2;
            char *tmp = (char *)realloc(buf, cap);
            if (!tmp) { free(buf); return NULL; }
            buf = tmp;
        }
    }
    buf[len] = '\0';
    return buf;
}

int main(int argc, char *argv[]) {
    const char *spec_file = NULL;
    const char *screenshot_path = NULL;

    /* Parse arguments */
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--screenshot") == 0 && i + 1 < argc) {
            screenshot_path = argv[++i];
        } else if (!spec_file) {
            spec_file = argv[i];
        } else {
            fprintf(stderr, "Usage: geoviz [spec.json] [--screenshot path]\n");
            return 1;
        }
    }

    /* Parse spec from file or stdin */
    Spec spec;
    if (spec_file) {
        if (spec_parse(spec_file, &spec) != 0) {
            return 1;
        }
    } else {
        char *json = read_stdin();
        if (!json || json[0] == '\0') {
            fprintf(stderr, "Usage: geoviz [spec.json] [--screenshot path]\n");
            fprintf(stderr, "       Or pipe spec JSON to stdin\n");
            free(json);
            return 1;
        }
        if (spec_parse_string(json, &spec) != 0) {
            free(json);
            return 1;
        }
        free(json);
    }

    fprintf(stderr, "Spec parsed: %d layers, basemap=%d\n",
            spec.layer_count, spec.basemap);

    /* Load data */
    DataSet ds;
    if (data_load(spec.sql, &ds) != 0) {
        fprintf(stderr, "Error: failed to load data\n");
        spec_free(&spec);
        return 1;
    }

    if (ds.count == 0) {
        fprintf(stderr, "Error: no data rows loaded\n");
        spec_free(&spec);
        return 1;
    }

    /* Compute initial viewport: fit to data extent */
    Viewport vp;
    vp.center_lon = (ds.x_min + ds.x_max) / 2.0;
    vp.center_lat = (ds.y_min + ds.y_max) / 2.0;

    /* Estimate zoom to fit data extent in window */
    double lon_range = ds.x_max - ds.x_min;
    double lat_range = ds.y_max - ds.y_min;
    if (lon_range < 0.001) lon_range = 0.001;
    if (lat_range < 0.001) lat_range = 0.001;

    double zoom_lon = log2(360.0 / lon_range * WINDOW_WIDTH / 256.0);
    double zoom_lat = log2(180.0 / lat_range * WINDOW_HEIGHT / 256.0);
    vp.zoom = fmin(zoom_lon, zoom_lat) - 0.5; /* Slight margin */
    if (vp.zoom < ZOOM_MIN) vp.zoom = ZOOM_MIN;
    if (vp.zoom > ZOOM_MAX) vp.zoom = ZOOM_MAX;

    fprintf(stderr, "Initial viewport: center=(%.4f, %.4f) zoom=%.2f\n",
            vp.center_lon, vp.center_lat, vp.zoom);

    /* Init window */
    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    InitWindow(WINDOW_WIDTH, WINDOW_HEIGHT, "geoviz");
    SetTargetFPS(60);

    /* Init subsystems */
    tiles_init();
    int w = GetScreenWidth();
    int h = GetScreenHeight();
    render_init(w, h);

    int frame_count = 0;

    /* Initial rasterisation */
    int viewport_dirty = 1;

    while (!WindowShouldClose()) {
        int new_w = GetScreenWidth();
        int new_h = GetScreenHeight();

        /* Handle window resize */
        if (new_w != w || new_h != h) {
            w = new_w;
            h = new_h;
            render_resize(w, h);
            viewport_dirty = 1;
        }

        /* Handle pan (left mouse drag) */
        if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
            Vector2 delta = GetMouseDelta();
            if (delta.x != 0 || delta.y != 0) {
                double scale = mercator_scale(vp.zoom);
                double dx_lon = -delta.x / scale * 360.0;
                double dy_world = -delta.y;

                /* Convert vertical pixel delta to lat delta */
                double cam_world_y = lat_to_world_y(vp.center_lat, scale);
                double new_world_y = cam_world_y + dy_world;
                double n = M_PI - 2.0 * M_PI * new_world_y / scale;
                double new_lat = 180.0 / M_PI * atan(0.5 * (exp(n) - exp(-n)));

                vp.center_lon += dx_lon;
                vp.center_lat = new_lat;

                /* Clamp */
                if (vp.center_lon < -180.0) vp.center_lon += 360.0;
                if (vp.center_lon > 180.0) vp.center_lon -= 360.0;
                if (vp.center_lat < -85.0) vp.center_lat = -85.0;
                if (vp.center_lat > 85.0) vp.center_lat = 85.0;

                viewport_dirty = 1;
            }
        }

        /* Handle zoom (scroll wheel, zoom toward cursor) */
        float scroll = GetMouseWheelMove();
        if (scroll != 0) {
            Vector2 mouse = GetMousePosition();

            /* Get lon/lat under cursor before zoom */
            double mouse_lon = pixel_to_lon(mouse.x, &vp, w);
            double mouse_lat = pixel_to_lat(mouse.y, &vp, h);

            /* Apply zoom */
            vp.zoom += scroll * 0.25;
            if (vp.zoom < ZOOM_MIN) vp.zoom = ZOOM_MIN;
            if (vp.zoom > ZOOM_MAX) vp.zoom = ZOOM_MAX;

            /* Adjust center so the point under cursor stays fixed */
            double new_px = lon_to_pixel(mouse_lon, &vp, w);
            double new_py = lat_to_pixel(mouse_lat, &vp, h);

            double dx = mouse.x - new_px;
            double dy = mouse.y - new_py;

            double scale = mercator_scale(vp.zoom);
            vp.center_lon -= dx / scale * 360.0;

            double cam_world_y = lat_to_world_y(vp.center_lat, scale);
            double adjusted_world_y = cam_world_y - dy;
            double nn = M_PI - 2.0 * M_PI * adjusted_world_y / scale;
            vp.center_lat = 180.0 / M_PI * atan(0.5 * (exp(nn) - exp(-nn)));

            if (vp.center_lat < -85.0) vp.center_lat = -85.0;
            if (vp.center_lat > 85.0) vp.center_lat = 85.0;

            viewport_dirty = 1;
        }

        /* Re-rasterise if needed */
        if (viewport_dirty) {
            render_rasterise(&spec, &ds, &vp, w, h);
            viewport_dirty = 0;
        }

        /* Draw */
        BeginDrawing();
        ClearBackground(BLACK);

        /* Basemap tiles */
        if (spec.basemap != BASEMAP_NONE) {
            tiles_render(&vp, spec.basemap, w, h);
        }

        /* Data overlay */
        render_draw_overlay();

        /* HUD */
        DrawFPS(10, 10);
        char info[128];
        snprintf(info, sizeof(info), "Zoom: %.1f  Points: %u", vp.zoom, ds.count);
        DrawText(info, 10, 30, 16, GREEN);

        EndDrawing();

        frame_count++;
        if (screenshot_path && frame_count == 300) {
            TakeScreenshot(screenshot_path);
            fprintf(stderr, "Screenshot saved to %s\n", screenshot_path);
            break;
        }
    }

    /* Cleanup */
    render_shutdown();
    tiles_shutdown();
    data_free(&ds);
    spec_free(&spec);
    CloseWindow();

    return 0;
}
