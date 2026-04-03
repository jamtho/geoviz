#include "spec.h"
#include "../third_party/cjson/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *str_dup(const char *s) {
    size_t len = strlen(s) + 1;
    char *copy = (char *)malloc(len);
    if (copy) memcpy(copy, s, len);
    return copy;
}

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

int spec_parse_string(const char *json, Spec *out) {
    cJSON *root = cJSON_Parse(json);
    if (!root) {
        fprintf(stderr, "Error: invalid JSON in spec\n");
        return -1;
    }

    memset(out, 0, sizeof(Spec));

    /* sql (required) */
    cJSON *sql = cJSON_GetObjectItemCaseSensitive(root, "sql");
    if (!sql || !cJSON_IsString(sql)) {
        fprintf(stderr, "Error: missing 'sql' string in spec\n");
        cJSON_Delete(root);
        return -1;
    }
    out->sql = str_dup(sql->valuestring);

    /* basemap */
    cJSON *basemap = cJSON_GetObjectItemCaseSensitive(root, "basemap");
    out->basemap = parse_basemap(basemap && cJSON_IsString(basemap) ? basemap->valuestring : NULL);

    /* layers (required) */
    cJSON *layers = cJSON_GetObjectItemCaseSensitive(root, "layers");
    if (!layers || !cJSON_IsArray(layers)) {
        fprintf(stderr, "Error: missing 'layers' array in spec\n");
        spec_free(out);
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
            spec_free(out);
            cJSON_Delete(root);
            return -1;
        }
        l->mark = parse_mark(mark->valuestring);

        /* scheme (optional, default viridis) */
        cJSON *scheme = cJSON_GetObjectItemCaseSensitive(layer, "scheme");
        l->scheme = colormap_from_name(
            scheme && cJSON_IsString(scheme) ? scheme->valuestring : NULL);

        /* point_size (optional, default 6) */
        cJSON *ps = cJSON_GetObjectItemCaseSensitive(layer, "point_size");
        l->point_size = (ps && cJSON_IsNumber(ps) && ps->valueint > 0) ? ps->valueint : 6;
    }

    cJSON_Delete(root);
    return 0;
}

int spec_parse(const char *filepath, Spec *out) {
    char *buf = NULL;
    if (read_file(filepath, &buf) != 0) return -1;
    int result = spec_parse_string(buf, out);
    free(buf);
    return result;
}

void spec_free(Spec *spec) {
    free(spec->sql);
    spec->sql = NULL;
}
