#include "spec.h"
#include "../third_party/cjson/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int read_file(const char *path, char **out_buf) {
    FILE *f = fopen(path, "rb");
    if (!f) {
        fprintf(stderr, "Error: cannot open spec file '%s'\n", path);
        return -1;
    }
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);
    *out_buf = (char *)malloc(len + 1);
    if (!*out_buf) {
        fclose(f);
        return -1;
    }
    fread(*out_buf, 1, len, f);
    (*out_buf)[len] = '\0';
    fclose(f);
    return 0;
}

static BasemapType parse_basemap(const char *s) {
    if (!s) return BASEMAP_OSM;
    if (strcmp(s, "osm") == 0) return BASEMAP_OSM;
    if (strcmp(s, "satellite") == 0) return BASEMAP_SATELLITE;
    if (strcmp(s, "nautical") == 0) return BASEMAP_NAUTICAL;
    if (strcmp(s, "none") == 0) return BASEMAP_NONE;
    fprintf(stderr, "Warning: unknown basemap '%s', defaulting to 'osm'\n", s);
    return BASEMAP_OSM;
}

static MarkType parse_mark(const char *s) {
    if (strcmp(s, "line") == 0) return MARK_LINE;
    return MARK_POINT;
}

int spec_parse(const char *filepath, Spec *out) {
    char *buf = NULL;
    if (read_file(filepath, &buf) != 0) return -1;

    cJSON *root = cJSON_Parse(buf);
    free(buf);
    if (!root) {
        fprintf(stderr, "Error: invalid JSON in spec file\n");
        return -1;
    }

    memset(out, 0, sizeof(Spec));

    /* data.uri */
    cJSON *data = cJSON_GetObjectItemCaseSensitive(root, "data");
    if (!data || !cJSON_IsObject(data)) {
        fprintf(stderr, "Error: missing 'data' object in spec\n");
        cJSON_Delete(root);
        return -1;
    }
    cJSON *uri = cJSON_GetObjectItemCaseSensitive(data, "uri");
    if (!uri || !cJSON_IsString(uri)) {
        fprintf(stderr, "Error: missing 'data.uri' string in spec\n");
        cJSON_Delete(root);
        return -1;
    }
    strncpy(out->data_uri, uri->valuestring, sizeof(out->data_uri) - 1);

    /* basemap */
    cJSON *basemap = cJSON_GetObjectItemCaseSensitive(root, "basemap");
    out->basemap = parse_basemap(basemap && cJSON_IsString(basemap) ? basemap->valuestring : NULL);

    /* layers */
    cJSON *layers = cJSON_GetObjectItemCaseSensitive(root, "layers");
    if (!layers || !cJSON_IsArray(layers)) {
        fprintf(stderr, "Error: missing 'layers' array in spec\n");
        cJSON_Delete(root);
        return -1;
    }

    int count = cJSON_GetArraySize(layers);
    if (count > MAX_LAYERS) {
        fprintf(stderr, "Warning: too many layers (%d), clamping to %d\n", count, MAX_LAYERS);
        count = MAX_LAYERS;
    }
    out->layer_count = count;

    for (int i = 0; i < count; i++) {
        cJSON *layer = cJSON_GetArrayItem(layers, i);
        Layer *l = &out->layers[i];

        cJSON *mark = cJSON_GetObjectItemCaseSensitive(layer, "mark");
        if (!mark || !cJSON_IsString(mark)) {
            fprintf(stderr, "Error: layer %d missing 'mark'\n", i);
            cJSON_Delete(root);
            return -1;
        }
        l->mark = parse_mark(mark->valuestring);

        cJSON *enc = cJSON_GetObjectItemCaseSensitive(layer, "encoding");
        if (!enc || !cJSON_IsObject(enc)) {
            fprintf(stderr, "Error: layer %d missing 'encoding'\n", i);
            cJSON_Delete(root);
            return -1;
        }

        cJSON *x = cJSON_GetObjectItemCaseSensitive(enc, "x");
        cJSON *y = cJSON_GetObjectItemCaseSensitive(enc, "y");
        if (!x || !y) {
            fprintf(stderr, "Error: layer %d missing x or y encoding\n", i);
            cJSON_Delete(root);
            return -1;
        }

        cJSON *x_field = cJSON_GetObjectItemCaseSensitive(x, "field");
        cJSON *y_field = cJSON_GetObjectItemCaseSensitive(y, "field");
        if (!x_field || !cJSON_IsString(x_field) || !y_field || !cJSON_IsString(y_field)) {
            fprintf(stderr, "Error: layer %d x/y encoding missing 'field'\n", i);
            cJSON_Delete(root);
            return -1;
        }

        strncpy(l->encoding.x_field, x_field->valuestring, sizeof(l->encoding.x_field) - 1);
        strncpy(l->encoding.y_field, y_field->valuestring, sizeof(l->encoding.y_field) - 1);

        cJSON *color = cJSON_GetObjectItemCaseSensitive(enc, "color");
        if (color && cJSON_IsObject(color)) {
            cJSON *cf = cJSON_GetObjectItemCaseSensitive(color, "field");
            if (cf && cJSON_IsString(cf)) {
                l->encoding.has_color = true;
                strncpy(l->encoding.color_field, cf->valuestring, sizeof(l->encoding.color_field) - 1);
                cJSON *scheme = cJSON_GetObjectItemCaseSensitive(color, "scheme");
                l->encoding.color_scheme = colormap_from_name(
                    scheme && cJSON_IsString(scheme) ? scheme->valuestring : NULL);
            }
        }
    }

    cJSON_Delete(root);
    return 0;
}
