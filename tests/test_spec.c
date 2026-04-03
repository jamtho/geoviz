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
        "  \"sql\": \"SELECT lon, lat FROM read_parquet('test.parquet')\","
        "  \"layers\": ["
        "    { \"mark\": \"point\" }"
        "  ]"
        "}";

    Spec spec;
    assert(write_and_parse(json, &spec) == 0);
    assert(strcmp(spec.sql, "SELECT lon, lat FROM read_parquet('test.parquet')") == 0);
    assert(spec.basemap == BASEMAP_OSM); /* default */
    assert(spec.layer_count == 1);
    assert(spec.layers[0].mark == MARK_POINT);
    assert(spec.layers[0].scheme == COLORMAP_VIRIDIS); /* default */
    assert(spec.layers[0].point_size == 6); /* default */
    spec_free(&spec);
    printf("  PASS: minimal spec\n");
}

static void test_full_spec(void) {
    const char *json =
        "{"
        "  \"sql\": \"SELECT longitude AS lon, latitude AS lat, speed AS color FROM read_parquet('s3://bucket/data.parquet')\","
        "  \"basemap\": \"satellite\","
        "  \"layers\": ["
        "    { \"mark\": \"line\", \"scheme\": \"turbo\" },"
        "    { \"mark\": \"point\", \"scheme\": \"inferno\", \"point_size\": 3 }"
        "  ]"
        "}";

    Spec spec;
    assert(write_and_parse(json, &spec) == 0);
    assert(strstr(spec.sql, "s3://bucket/data.parquet") != NULL);
    assert(spec.basemap == BASEMAP_SATELLITE);
    assert(spec.layer_count == 2);
    assert(spec.layers[0].mark == MARK_LINE);
    assert(spec.layers[0].scheme == COLORMAP_TURBO);
    assert(spec.layers[1].mark == MARK_POINT);
    assert(spec.layers[1].scheme == COLORMAP_INFERNO);
    assert(spec.layers[1].point_size == 3);
    spec_free(&spec);
    printf("  PASS: full spec\n");
}

static void test_nautical_basemap(void) {
    const char *json =
        "{"
        "  \"sql\": \"SELECT 1 AS lon, 2 AS lat\","
        "  \"basemap\": \"nautical\","
        "  \"layers\": [{ \"mark\": \"point\" }]"
        "}";

    Spec spec;
    assert(write_and_parse(json, &spec) == 0);
    assert(spec.basemap == BASEMAP_NAUTICAL);
    spec_free(&spec);
    printf("  PASS: nautical basemap\n");
}

static void test_none_basemap(void) {
    const char *json =
        "{"
        "  \"sql\": \"SELECT 1 AS lon, 2 AS lat\","
        "  \"basemap\": \"none\","
        "  \"layers\": [{ \"mark\": \"point\" }]"
        "}";

    Spec spec;
    assert(write_and_parse(json, &spec) == 0);
    assert(spec.basemap == BASEMAP_NONE);
    spec_free(&spec);
    printf("  PASS: none basemap\n");
}

static void test_default_scheme(void) {
    const char *json =
        "{"
        "  \"sql\": \"SELECT 1 AS lon, 2 AS lat, 3 AS color\","
        "  \"layers\": [{ \"mark\": \"point\" }]"
        "}";

    Spec spec;
    assert(write_and_parse(json, &spec) == 0);
    assert(spec.layers[0].scheme == COLORMAP_VIRIDIS); /* default */
    spec_free(&spec);
    printf("  PASS: default scheme\n");
}

static void test_parse_string(void) {
    const char *json =
        "{"
        "  \"sql\": \"SELECT lon, lat FROM data\","
        "  \"layers\": [{ \"mark\": \"line\" }]"
        "}";

    Spec spec;
    assert(spec_parse_string(json, &spec) == 0);
    assert(strcmp(spec.sql, "SELECT lon, lat FROM data") == 0);
    assert(spec.layers[0].mark == MARK_LINE);
    spec_free(&spec);
    printf("  PASS: parse from string\n");
}

static void test_missing_sql(void) {
    const char *json = "{ \"layers\": [{ \"mark\": \"point\" }] }";
    Spec spec;
    assert(write_and_parse(json, &spec) != 0);
    printf("  PASS: missing sql rejected\n");
}

static void test_missing_layers(void) {
    const char *json = "{ \"sql\": \"SELECT 1 AS lon, 2 AS lat\" }";
    Spec spec;
    assert(write_and_parse(json, &spec) != 0);
    printf("  PASS: missing layers rejected\n");
}

static void test_missing_mark(void) {
    const char *json =
        "{ \"sql\": \"SELECT 1 AS lon, 2 AS lat\","
        "  \"layers\": [{}] }";
    Spec spec;
    assert(write_and_parse(json, &spec) != 0);
    printf("  PASS: missing mark rejected\n");
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
    test_default_scheme();
    test_parse_string();
    test_missing_sql();
    test_missing_layers();
    test_missing_mark();
    test_invalid_json();
    test_missing_file();
    printf("All spec tests passed.\n");
    return 0;
}
