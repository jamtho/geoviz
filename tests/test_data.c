#include "data.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#define ASSERT_NEAR(a, b, eps) assert(fabs((double)(a) - (double)(b)) < (eps))

/* Helper: write a CSV file to disk */
static void write_csv(const char *path, const char *content) {
    FILE *f = fopen(path, "w");
    assert(f);
    fputs(content, f);
    fclose(f);
}

static int file_exists(const char *path) {
    FILE *f = fopen(path, "rb");
    if (f) { fclose(f); return 1; }
    return 0;
}

static void test_load_csv_basic(void) {
    const char *csv = "test_load_basic.csv";
    write_csv(csv,
        "lon,lat,color\n"
        "1.0,50.0,5.0\n"
        "2.0,51.0,10.0\n"
        "-1.5,49.5,15.0\n"
    );

    DataSet ds;
    int rc = data_load("SELECT lon, lat, color FROM read_csv('test_load_basic.csv')", &ds);
    assert(rc == 0);
    assert(ds.count == 3);
    assert(ds.has_color == true);

    ASSERT_NEAR(ds.x_min, -1.5f, 0.01);
    ASSERT_NEAR(ds.x_max, 2.0f, 0.01);
    ASSERT_NEAR(ds.y_min, 49.5f, 0.01);
    ASSERT_NEAR(ds.y_max, 51.0f, 0.01);
    ASSERT_NEAR(ds.color_min, 5.0f, 0.01);
    ASSERT_NEAR(ds.color_max, 15.0f, 0.01);

    assert(ds.x != NULL);
    assert(ds.y != NULL);
    assert(ds.color_values != NULL);

    data_free(&ds);
    remove(csv);
    printf("  PASS: load CSV basic\n");
}

static void test_load_csv_no_color(void) {
    const char *csv = "test_load_no_color.csv";
    write_csv(csv,
        "lon,lat\n"
        "10.0,20.0\n"
        "30.0,40.0\n"
    );

    DataSet ds;
    int rc = data_load("SELECT lon, lat FROM read_csv('test_load_no_color.csv')", &ds);
    assert(rc == 0);
    assert(ds.count == 2);
    assert(ds.has_color == false);
    assert(ds.color_values == NULL);
    ASSERT_NEAR(ds.x_min, 10.0f, 0.01);
    ASSERT_NEAR(ds.x_max, 30.0f, 0.01);

    data_free(&ds);
    remove(csv);
    printf("  PASS: load CSV no color\n");
}

static void test_load_csv_with_alias(void) {
    const char *csv = "test_load_alias.csv";
    write_csv(csv,
        "longitude,latitude,speed\n"
        "1.0,50.0,5.0\n"
        "2.0,51.0,10.0\n"
    );

    DataSet ds;
    int rc = data_load("SELECT longitude AS lon, latitude AS lat, speed AS color FROM read_csv('test_load_alias.csv')", &ds);
    assert(rc == 0);
    assert(ds.count == 2);
    assert(ds.has_color == true);
    ASSERT_NEAR(ds.x[0], 1.0f, 0.01);
    ASSERT_NEAR(ds.y[0], 50.0f, 0.01);
    ASSERT_NEAR(ds.color_values[0], 5.0f, 0.01);

    data_free(&ds);
    remove(csv);
    printf("  PASS: load CSV with column aliases\n");
}

static void test_load_csv_single_row(void) {
    const char *csv = "test_load_single.csv";
    write_csv(csv,
        "lon,lat\n"
        "5.5,55.5\n"
    );

    DataSet ds;
    int rc = data_load("SELECT lon, lat FROM read_csv('test_load_single.csv')", &ds);
    assert(rc == 0);
    assert(ds.count == 1);
    ASSERT_NEAR(ds.x[0], 5.5f, 0.01);
    ASSERT_NEAR(ds.y[0], 55.5f, 0.01);

    data_free(&ds);
    remove(csv);
    printf("  PASS: load CSV single row\n");
}

static void test_load_csv_integer_columns(void) {
    const char *csv = "test_load_int.csv";
    write_csv(csv,
        "lon,lat,color\n"
        "1,2,100\n"
        "3,4,200\n"
    );

    DataSet ds;
    int rc = data_load("SELECT lon, lat, color FROM read_csv('test_load_int.csv')", &ds);
    assert(rc == 0);
    assert(ds.count == 2);
    ASSERT_NEAR(ds.x[0], 1.0f, 0.01);
    ASSERT_NEAR(ds.color_values[1], 200.0f, 0.01);

    data_free(&ds);
    remove(csv);
    printf("  PASS: load CSV integer columns\n");
}

static void test_load_csv_large(void) {
    const char *csv = "test_load_large.csv";
    FILE *f = fopen(csv, "w");
    assert(f);
    fprintf(f, "lon,lat,color\n");
    for (int i = 0; i < 10000; i++) {
        fprintf(f, "%.4f,%.4f,%.2f\n",
                -5.0 + (i % 100) * 0.07,
                49.0 + (i / 100) * 0.025,
                (float)(i % 25));
    }
    fclose(f);

    DataSet ds;
    int rc = data_load("SELECT lon, lat, color FROM read_csv('test_load_large.csv')", &ds);
    assert(rc == 0);
    assert(ds.count == 10000);
    assert(ds.x_min < ds.x_max);
    assert(ds.y_min < ds.y_max);

    data_free(&ds);
    remove(csv);
    printf("  PASS: load CSV large (10k rows)\n");
}

static void test_load_parquet(void) {
    const char *pq = "tests/fixture.parquet";
    if (!file_exists(pq)) {
        printf("  SKIP: load parquet (fixture not found, run: uv run python tests/gen_test_fixture.py)\n");
        return;
    }

    DataSet ds;
    int rc = data_load("SELECT lon, lat, speed AS color FROM read_parquet('tests/fixture.parquet')", &ds);
    assert(rc == 0);
    assert(ds.count == 4);
    ASSERT_NEAR(ds.x_min, -3.0f, 0.01);
    ASSERT_NEAR(ds.x_max, 0.0f, 0.01);
    ASSERT_NEAR(ds.y_min, 50.0f, 0.01);
    ASSERT_NEAR(ds.y_max, 51.0f, 0.01);
    ASSERT_NEAR(ds.color_min, 6.0f, 0.01);
    ASSERT_NEAR(ds.color_max, 18.0f, 0.01);

    data_free(&ds);
    printf("  PASS: load parquet\n");
}

static void test_load_inline_sql(void) {
    DataSet ds;
    int rc = data_load("SELECT 1.5 AS lon, 51.0 AS lat, 42.0 AS color", &ds);
    assert(rc == 0);
    assert(ds.count == 1);
    assert(ds.has_color == true);
    ASSERT_NEAR(ds.x[0], 1.5f, 0.01);
    ASSERT_NEAR(ds.y[0], 51.0f, 0.01);
    ASSERT_NEAR(ds.color_values[0], 42.0f, 0.01);

    data_free(&ds);
    printf("  PASS: load inline SQL\n");
}

static void test_load_bad_sql(void) {
    DataSet ds;
    int rc = data_load("SELECT nonsense FROM nowhere", &ds);
    assert(rc != 0);
    printf("  PASS: bad SQL fails\n");
}

static void test_load_missing_xy(void) {
    DataSet ds;
    int rc = data_load("SELECT 1 AS a, 2 AS b", &ds);
    assert(rc != 0);
    printf("  PASS: missing lon/lat columns fails\n");
}

static void test_load_negative_values(void) {
    const char *csv = "test_load_neg.csv";
    write_csv(csv,
        "lon,lat,color\n"
        "-180.0,-90.0,-40.5\n"
        "180.0,90.0,50.0\n"
    );

    DataSet ds;
    int rc = data_load("SELECT lon, lat, color FROM read_csv('test_load_neg.csv')", &ds);
    assert(rc == 0);
    assert(ds.count == 2);
    ASSERT_NEAR(ds.x_min, -180.0f, 0.1);
    ASSERT_NEAR(ds.x_max, 180.0f, 0.1);
    ASSERT_NEAR(ds.color_min, -40.5f, 0.1);
    ASSERT_NEAR(ds.color_max, 50.0f, 0.1);

    data_free(&ds);
    remove(csv);
    printf("  PASS: load negative values\n");
}

static void test_load_uniform_color(void) {
    DataSet ds;
    int rc = data_load(
        "SELECT lon, lat, 7.0 AS color FROM (VALUES (1.0, 2.0), (3.0, 4.0), (5.0, 6.0)) AS t(lon, lat)",
        &ds);
    assert(rc == 0);
    assert(ds.count == 3);
    ASSERT_NEAR(ds.color_min, 7.0f, 0.01);
    ASSERT_NEAR(ds.color_max, 7.0f, 0.01);

    data_free(&ds);
    printf("  PASS: load uniform color values\n");
}

static void test_data_free_null_safe(void) {
    DataSet ds;
    memset(&ds, 0, sizeof(ds));
    data_free(&ds); /* should not crash */
    printf("  PASS: data_free on zeroed struct\n");
}

int main(void) {
    printf("Running data loading tests...\n");
    test_load_csv_basic();
    test_load_csv_no_color();
    test_load_csv_with_alias();
    test_load_csv_single_row();
    test_load_csv_integer_columns();
    test_load_csv_large();
    test_load_parquet();
    test_load_inline_sql();
    test_load_bad_sql();
    test_load_missing_xy();
    test_load_negative_values();
    test_load_uniform_color();
    test_data_free_null_safe();
    printf("All data loading tests passed.\n");
    return 0;
}
