#include "spec.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

/* Helper: write a temp spec file and parse it */
static int write_and_parse(const char *json, Spec *out) {
    const char *path = "test_spec_tmp.json";
    FILE *f = fopen(path, "w");
    if (!f) return -1;
    fputs(json, f);
    fclose(f);
    int result = spec_parse(path, out);
    remove(path);
    return result;
}

static void test_minimal_spec(void) {
    const char *json =
        "{"
        "  \"data\": { \"uri\": \"test.parquet\" },"
        "  \"layers\": ["
        "    {"
        "      \"mark\": \"point\","
        "      \"encoding\": {"
        "        \"x\": { \"field\": \"lon\" },"
        "        \"y\": { \"field\": \"lat\" }"
        "      }"
        "    }"
        "  ]"
        "}";

    Spec spec;
    assert(write_and_parse(json, &spec) == 0);
    assert(strcmp(spec.data_uri, "test.parquet") == 0);
    assert(spec.basemap == BASEMAP_OSM); /* default */
    assert(spec.layer_count == 1);
    assert(spec.layers[0].mark == MARK_POINT);
    assert(strcmp(spec.layers[0].encoding.x_field, "lon") == 0);
    assert(strcmp(spec.layers[0].encoding.y_field, "lat") == 0);
    assert(spec.layers[0].encoding.has_color == false);
    printf("  PASS: minimal spec\n");
}

static void test_full_spec(void) {
    const char *json =
        "{"
        "  \"data\": { \"uri\": \"s3://bucket/data.parquet\" },"
        "  \"basemap\": \"satellite\","
        "  \"layers\": ["
        "    {"
        "      \"mark\": \"line\","
        "      \"encoding\": {"
        "        \"x\": { \"field\": \"longitude\" },"
        "        \"y\": { \"field\": \"latitude\" },"
        "        \"color\": { \"field\": \"speed\", \"scheme\": \"turbo\" }"
        "      }"
        "    },"
        "    {"
        "      \"mark\": \"point\","
        "      \"encoding\": {"
        "        \"x\": { \"field\": \"longitude\" },"
        "        \"y\": { \"field\": \"latitude\" },"
        "        \"color\": { \"field\": \"speed\", \"scheme\": \"inferno\" }"
        "      }"
        "    }"
        "  ]"
        "}";

    Spec spec;
    assert(write_and_parse(json, &spec) == 0);
    assert(strcmp(spec.data_uri, "s3://bucket/data.parquet") == 0);
    assert(spec.basemap == BASEMAP_SATELLITE);
    assert(spec.layer_count == 2);
    assert(spec.layers[0].mark == MARK_LINE);
    assert(strcmp(spec.layers[0].encoding.x_field, "longitude") == 0);
    assert(spec.layers[0].encoding.has_color == true);
    assert(strcmp(spec.layers[0].encoding.color_field, "speed") == 0);
    assert(spec.layers[0].encoding.color_scheme == COLORMAP_TURBO);
    assert(spec.layers[1].mark == MARK_POINT);
    assert(spec.layers[1].encoding.color_scheme == COLORMAP_INFERNO);
    printf("  PASS: full spec\n");
}

static void test_nautical_basemap(void) {
    const char *json =
        "{"
        "  \"data\": { \"uri\": \"x.csv\" },"
        "  \"basemap\": \"nautical\","
        "  \"layers\": ["
        "    { \"mark\": \"point\", \"encoding\": {"
        "      \"x\": { \"field\": \"x\" }, \"y\": { \"field\": \"y\" }"
        "    }}"
        "  ]"
        "}";

    Spec spec;
    assert(write_and_parse(json, &spec) == 0);
    assert(spec.basemap == BASEMAP_NAUTICAL);
    printf("  PASS: nautical basemap\n");
}

static void test_none_basemap(void) {
    const char *json =
        "{"
        "  \"data\": { \"uri\": \"x.csv\" },"
        "  \"basemap\": \"none\","
        "  \"layers\": ["
        "    { \"mark\": \"point\", \"encoding\": {"
        "      \"x\": { \"field\": \"x\" }, \"y\": { \"field\": \"y\" }"
        "    }}"
        "  ]"
        "}";

    Spec spec;
    assert(write_and_parse(json, &spec) == 0);
    assert(spec.basemap == BASEMAP_NONE);
    printf("  PASS: none basemap\n");
}

static void test_default_color_scheme(void) {
    const char *json =
        "{"
        "  \"data\": { \"uri\": \"x.csv\" },"
        "  \"layers\": ["
        "    { \"mark\": \"point\", \"encoding\": {"
        "      \"x\": { \"field\": \"x\" }, \"y\": { \"field\": \"y\" },"
        "      \"color\": { \"field\": \"val\" }"
        "    }}"
        "  ]"
        "}";

    Spec spec;
    assert(write_and_parse(json, &spec) == 0);
    assert(spec.layers[0].encoding.has_color == true);
    assert(spec.layers[0].encoding.color_scheme == COLORMAP_VIRIDIS); /* default */
    printf("  PASS: default color scheme\n");
}

static void test_missing_data(void) {
    const char *json = "{ \"layers\": [{ \"mark\": \"point\", \"encoding\": {"
                       "\"x\":{\"field\":\"x\"}, \"y\":{\"field\":\"y\"}}}] }";
    Spec spec;
    assert(write_and_parse(json, &spec) != 0);
    printf("  PASS: missing data rejected\n");
}

static void test_missing_layers(void) {
    const char *json = "{ \"data\": { \"uri\": \"x.csv\" } }";
    Spec spec;
    assert(write_and_parse(json, &spec) != 0);
    printf("  PASS: missing layers rejected\n");
}

static void test_missing_encoding(void) {
    const char *json =
        "{ \"data\": { \"uri\": \"x.csv\" },"
        "  \"layers\": [{ \"mark\": \"point\" }] }";
    Spec spec;
    assert(write_and_parse(json, &spec) != 0);
    printf("  PASS: missing encoding rejected\n");
}

static void test_invalid_json(void) {
    Spec spec;
    assert(write_and_parse("not json at all {{{", &spec) != 0);
    printf("  PASS: invalid JSON rejected\n");
}

static void test_missing_file(void) {
    Spec spec;
    assert(spec_parse("nonexistent_file_12345.json", &spec) != 0);
    printf("  PASS: missing file rejected\n");
}

int main(void) {
    printf("Running spec tests...\n");
    test_minimal_spec();
    test_full_spec();
    test_nautical_basemap();
    test_none_basemap();
    test_default_color_scheme();
    test_missing_data();
    test_missing_layers();
    test_missing_encoding();
    test_invalid_json();
    test_missing_file();
    printf("All spec tests passed.\n");
    return 0;
}
