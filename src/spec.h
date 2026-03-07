#ifndef SPEC_H
#define SPEC_H

#include "colormap.h"
#include <stdbool.h>

#define MAX_LAYERS 16

typedef enum {
    BASEMAP_OSM = 0,
    BASEMAP_SATELLITE,
    BASEMAP_NAUTICAL,
    BASEMAP_NONE
} BasemapType;

typedef enum {
    MARK_POINT = 0,
    MARK_LINE
} MarkType;

typedef struct {
    char x_field[256];
    char y_field[256];
    bool has_color;
    char color_field[256];
    ColormapType color_scheme;
} Encoding;

typedef struct {
    MarkType mark;
    Encoding encoding;
} Layer;

typedef struct {
    char data_uri[1024];
    BasemapType basemap;
    Layer layers[MAX_LAYERS];
    int layer_count;
} Spec;

/* Parse a spec JSON file. Returns 0 on success, -1 on error (prints to stderr). */
int spec_parse(const char *filepath, Spec *out);

#endif /* SPEC_H */
