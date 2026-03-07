#ifndef COLORMAP_H
#define COLORMAP_H

#include <stdint.h>

typedef struct {
    uint8_t r, g, b;
} ColorRGB;

typedef enum {
    COLORMAP_VIRIDIS = 0,
    COLORMAP_INFERNO,
    COLORMAP_PLASMA,
    COLORMAP_TURBO,
    COLORMAP_COUNT
} ColormapType;

/* Get the 256-entry LUT for a given colormap */
const ColorRGB *colormap_get_lut(ColormapType type);

/* Parse a colormap name string; returns COLORMAP_VIRIDIS on unknown */
ColormapType colormap_from_name(const char *name);

/* Sample a colormap: t in [0,1] */
ColorRGB colormap_sample(ColormapType type, float t);

#endif /* COLORMAP_H */
