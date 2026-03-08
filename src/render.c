#include "render.h"
#include "colormap.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

static Color *pixel_buffer = NULL;
static Texture2D overlay_texture;
static Image overlay_image;
static int buf_width = 0;
static int buf_height = 0;
static bool texture_valid = false;

void render_init(int width, int height) {
    buf_width = width;
    buf_height = height;
    pixel_buffer = (Color *)calloc(width * height, sizeof(Color));
    overlay_image = GenImageColor(width, height, BLANK);
    overlay_texture = LoadTextureFromImage(overlay_image);
    texture_valid = true;
}

void render_resize(int width, int height) {
    if (width == buf_width && height == buf_height) return;

    free(pixel_buffer);
    buf_width = width;
    buf_height = height;
    pixel_buffer = (Color *)calloc(width * height, sizeof(Color));

    if (texture_valid) {
        UnloadTexture(overlay_texture);
        UnloadImage(overlay_image);
    }
    overlay_image = GenImageColor(width, height, BLANK);
    overlay_texture = LoadTextureFromImage(overlay_image);
    texture_valid = true;
}

static inline void plot_pixel(int px, int py, Color c) {
    if (px >= 0 && px < buf_width && py >= 0 && py < buf_height) {
        pixel_buffer[py * buf_width + px] = c;
    }
}

/* Plot a square of given size centered on (px, py) */
static inline void plot_point(int px, int py, int size, Color c) {
    int half = size / 2;
    for (int dy = -half; dy < size - half; dy++) {
        for (int dx = -half; dx < size - half; dx++) {
            plot_pixel(px + dx, py + dy, c);
        }
    }
}

static void draw_line_bresenham(int x0, int y0, int x1, int y1, Color c) {
    int dx = abs(x1 - x0);
    int dy = -abs(y1 - y0);
    int sx = x0 < x1 ? 1 : -1;
    int sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;

    for (;;) {
        plot_pixel(x0, y0, c);
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

static Color color_from_rgb(ColorRGB rgb) {
    Color c;
    c.r = rgb.r;
    c.g = rgb.g;
    c.b = rgb.b;
    c.a = 255;
    return c;
}

static Color default_point_color(void) {
    return (Color){255, 200, 50, 255};
}

void render_rasterise(const Spec *spec, const DataSet *ds,
                      const Viewport *vp, int width, int height) {
    if (!pixel_buffer || !ds || ds->count == 0) return;

    /* Clear the buffer */
    memset(pixel_buffer, 0, width * height * sizeof(Color));

    float color_range = ds->color_max - ds->color_min;
    if (color_range == 0.0f) color_range = 1.0f;

    for (int li = 0; li < spec->layer_count; li++) {
        const Layer *layer = &spec->layers[li];
        ColormapType cmap = layer->encoding.color_scheme;
        bool has_color = layer->encoding.has_color && ds->color_values != NULL;

        if (layer->mark == MARK_POINT) {
            int pt_size = layer->encoding.point_size;
            for (uint32_t i = 0; i < ds->count; i++) {
                int px = (int)lon_to_pixel(ds->x[i], vp, width);
                int py = (int)lat_to_pixel(ds->y[i], vp, height);

                Color c;
                if (has_color) {
                    float t = (ds->color_values[i] - ds->color_min) / color_range;
                    c = color_from_rgb(colormap_sample(cmap, t));
                } else {
                    c = default_point_color();
                }
                plot_point(px, py, pt_size, c);
            }
        } else if (layer->mark == MARK_LINE) {
            int prev_px = 0, prev_py = 0;
            for (uint32_t i = 0; i < ds->count; i++) {
                int px = (int)lon_to_pixel(ds->x[i], vp, width);
                int py = (int)lat_to_pixel(ds->y[i], vp, height);

                if (i > 0) {
                    Color c;
                    if (has_color) {
                        float t = (ds->color_values[i] - ds->color_min) / color_range;
                        c = color_from_rgb(colormap_sample(cmap, t));
                    } else {
                        c = default_point_color();
                    }
                    draw_line_bresenham(prev_px, prev_py, px, py, c);
                }
                prev_px = px;
                prev_py = py;
            }
        }
    }

    /* Upload to texture */
    UpdateTexture(overlay_texture, pixel_buffer);
}

void render_draw_overlay(void) {
    if (texture_valid) {
        DrawTexture(overlay_texture, 0, 0, WHITE);
    }
}

void render_shutdown(void) {
    if (texture_valid) {
        UnloadTexture(overlay_texture);
        UnloadImage(overlay_image);
        texture_valid = false;
    }
    free(pixel_buffer);
    pixel_buffer = NULL;
}
