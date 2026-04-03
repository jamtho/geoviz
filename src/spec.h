#ifndef SPEC_H
#define SPEC_H

#include "colormap.h"

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
    MarkType mark;
    ColormapType scheme;
    int point_size;  /* Diameter in pixels (default 6) */
} Layer;

typedef struct {
    char *sql;       /* Dynamically allocated SQL query */
    BasemapType basemap;
    Layer layers[MAX_LAYERS];
    int layer_count;
} Spec;

/* Parse a spec from a JSON string. Returns 0 on success, -1 on error. */
int spec_parse_string(const char *json, Spec *out);

/* Parse a spec JSON file. Returns 0 on success, -1 on error. */
int spec_parse(const char *filepath, Spec *out);

/* Free dynamically allocated spec fields */
void spec_free(Spec *spec);

#endif /* SPEC_H */
