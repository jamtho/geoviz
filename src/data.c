#include "data.h"
#include "../third_party/duckdb/duckdb.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <float.h>
#include <math.h>

static double get_double_from_vector(duckdb_vector vec, duckdb_type col_type, uint64_t row) {
    switch (col_type) {
        case DUCKDB_TYPE_FLOAT: {
            float *data = (float *)duckdb_vector_get_data(vec);
            return (double)data[row];
        }
        case DUCKDB_TYPE_DOUBLE: {
            double *data = (double *)duckdb_vector_get_data(vec);
            return data[row];
        }
        case DUCKDB_TYPE_TINYINT: {
            int8_t *data = (int8_t *)duckdb_vector_get_data(vec);
            return (double)data[row];
        }
        case DUCKDB_TYPE_SMALLINT: {
            int16_t *data = (int16_t *)duckdb_vector_get_data(vec);
            return (double)data[row];
        }
        case DUCKDB_TYPE_INTEGER: {
            int32_t *data = (int32_t *)duckdb_vector_get_data(vec);
            return (double)data[row];
        }
        case DUCKDB_TYPE_BIGINT: {
            int64_t *data = (int64_t *)duckdb_vector_get_data(vec);
            return (double)data[row];
        }
        case DUCKDB_TYPE_UTINYINT: {
            uint8_t *data = (uint8_t *)duckdb_vector_get_data(vec);
            return (double)data[row];
        }
        case DUCKDB_TYPE_USMALLINT: {
            uint16_t *data = (uint16_t *)duckdb_vector_get_data(vec);
            return (double)data[row];
        }
        case DUCKDB_TYPE_UINTEGER: {
            uint32_t *data = (uint32_t *)duckdb_vector_get_data(vec);
            return (double)data[row];
        }
        case DUCKDB_TYPE_UBIGINT: {
            uint64_t *data = (uint64_t *)duckdb_vector_get_data(vec);
            return (double)data[row];
        }
        case DUCKDB_TYPE_HUGEINT: {
            duckdb_hugeint *data = (duckdb_hugeint *)duckdb_vector_get_data(vec);
            return (double)data[row].lower + (double)data[row].upper * 18446744073709551616.0;
        }
        default:
            return 0.0;
    }
}

static int is_numeric_type(duckdb_type t) {
    switch (t) {
        case DUCKDB_TYPE_FLOAT:
        case DUCKDB_TYPE_DOUBLE:
        case DUCKDB_TYPE_TINYINT:
        case DUCKDB_TYPE_SMALLINT:
        case DUCKDB_TYPE_INTEGER:
        case DUCKDB_TYPE_BIGINT:
        case DUCKDB_TYPE_UTINYINT:
        case DUCKDB_TYPE_USMALLINT:
        case DUCKDB_TYPE_UINTEGER:
        case DUCKDB_TYPE_UBIGINT:
        case DUCKDB_TYPE_HUGEINT:
            return 1;
        default:
            return 0;
    }
}

int data_load(const Spec *spec, DataSet *out) {
    memset(out, 0, sizeof(DataSet));

    /* Find the first layer's encoding to determine columns to query.
       All layers reference the same data source, so we collect the union of fields. */
    const char *x_field = NULL;
    const char *y_field = NULL;
    const char *color_field = NULL;

    for (int i = 0; i < spec->layer_count; i++) {
        const Layer *l = &spec->layers[i];
        if (!x_field) x_field = l->encoding.x_field;
        if (!y_field) y_field = l->encoding.y_field;
        if (!color_field && l->encoding.has_color) {
            color_field = l->encoding.color_field;
        }
    }

    if (!x_field || !y_field) {
        fprintf(stderr, "Error: no x/y fields found in spec layers\n");
        return -1;
    }

    duckdb_database db;
    duckdb_connection con;

    if (duckdb_open(NULL, &db) != DuckDBSuccess) {
        fprintf(stderr, "Error: failed to open DuckDB in-memory database\n");
        return -1;
    }
    if (duckdb_connect(db, &con) != DuckDBSuccess) {
        fprintf(stderr, "Error: failed to connect to DuckDB\n");
        duckdb_close(&db);
        return -1;
    }

    /* Install httpfs if S3 URI */
    if (strncmp(spec->data_uri, "s3://", 5) == 0) {
        duckdb_result res;
        if (duckdb_query(con, "INSTALL httpfs; LOAD httpfs;", &res) != DuckDBSuccess) {
            fprintf(stderr, "Warning: failed to load httpfs extension\n");
        }
        duckdb_destroy_result(&res);
    }

    /* Build query */
    char query[2048];
    if (color_field) {
        snprintf(query, sizeof(query),
                 "SELECT \"%s\", \"%s\", \"%s\" FROM '%s'",
                 x_field, y_field, color_field, spec->data_uri);
    } else {
        snprintf(query, sizeof(query),
                 "SELECT \"%s\", \"%s\" FROM '%s'",
                 x_field, y_field, spec->data_uri);
    }

    fprintf(stderr, "Executing query: %s\n", query);

    duckdb_result result;
    if (duckdb_query(con, query, &result) != DuckDBSuccess) {
        const char *err = duckdb_result_error(&result);
        fprintf(stderr, "Error: DuckDB query failed: %s\n", err ? err : "unknown error");
        duckdb_destroy_result(&result);
        duckdb_disconnect(&con);
        duckdb_close(&db);
        return -1;
    }

    /* Validate column types */
    idx_t col_count = duckdb_column_count(&result);
    int expected_cols = color_field ? 3 : 2;
    if ((int)col_count < expected_cols) {
        fprintf(stderr, "Error: query returned %llu columns, expected %d\n",
                (unsigned long long)col_count, expected_cols);
        duckdb_destroy_result(&result);
        duckdb_disconnect(&con);
        duckdb_close(&db);
        return -1;
    }

    for (int c = 0; c < expected_cols; c++) {
        duckdb_type t = duckdb_column_type(&result, c);
        if (!is_numeric_type(t)) {
            fprintf(stderr, "Error: column %d is not numeric (type=%d)\n", c, (int)t);
            duckdb_destroy_result(&result);
            duckdb_disconnect(&con);
            duckdb_close(&db);
            return -1;
        }
    }

    /* Count total rows via chunks */
    uint64_t total_rows = 0;
    idx_t chunk_count = duckdb_result_chunk_count(result);
    for (idx_t ci = 0; ci < chunk_count; ci++) {
        duckdb_data_chunk chunk = duckdb_result_get_chunk(result, ci);
        total_rows += duckdb_data_chunk_get_size(chunk);
        duckdb_destroy_data_chunk(&chunk);
    }

    if (total_rows == 0) {
        fprintf(stderr, "Warning: query returned 0 rows\n");
        duckdb_destroy_result(&result);
        duckdb_disconnect(&con);
        duckdb_close(&db);
        return 0;
    }

    fprintf(stderr, "Loading %llu rows...\n", (unsigned long long)total_rows);

    /* Allocate arrays */
    out->x = (float *)malloc(total_rows * sizeof(float));
    out->y = (float *)malloc(total_rows * sizeof(float));
    if (color_field) {
        out->color_values = (float *)malloc(total_rows * sizeof(float));
    }

    if (!out->x || !out->y || (color_field && !out->color_values)) {
        fprintf(stderr, "Error: failed to allocate memory for %llu rows\n",
                (unsigned long long)total_rows);
        data_free(out);
        duckdb_destroy_result(&result);
        duckdb_disconnect(&con);
        duckdb_close(&db);
        return -1;
    }

    /* Extract data chunk by chunk */
    float x_min = FLT_MAX, x_max = -FLT_MAX;
    float y_min = FLT_MAX, y_max = -FLT_MAX;
    float c_min = FLT_MAX, c_max = -FLT_MAX;
    uint64_t row_offset = 0;

    duckdb_type x_type = duckdb_column_type(&result, 0);
    duckdb_type y_type = duckdb_column_type(&result, 1);
    duckdb_type c_type = color_field ? duckdb_column_type(&result, 2) : DUCKDB_TYPE_FLOAT;

    for (idx_t ci = 0; ci < chunk_count; ci++) {
        duckdb_data_chunk chunk = duckdb_result_get_chunk(result, ci);
        idx_t chunk_size = duckdb_data_chunk_get_size(chunk);

        duckdb_vector x_vec = duckdb_data_chunk_get_vector(chunk, 0);
        duckdb_vector y_vec = duckdb_data_chunk_get_vector(chunk, 1);
        duckdb_vector c_vec = color_field ? duckdb_data_chunk_get_vector(chunk, 2) : NULL;

        uint64_t *x_validity = duckdb_vector_get_validity(x_vec);
        uint64_t *y_validity = duckdb_vector_get_validity(y_vec);
        uint64_t *c_validity = c_vec ? duckdb_vector_get_validity(c_vec) : NULL;

        for (idx_t r = 0; r < chunk_size; r++) {
            /* Check validity (NULL handling) */
            int x_valid = !x_validity || duckdb_validity_row_is_valid(x_validity, r);
            int y_valid = !y_validity || duckdb_validity_row_is_valid(y_validity, r);

            float xv = x_valid ? (float)get_double_from_vector(x_vec, x_type, r) : 0.0f;
            float yv = y_valid ? (float)get_double_from_vector(y_vec, y_type, r) : 0.0f;

            out->x[row_offset + r] = xv;
            out->y[row_offset + r] = yv;

            if (xv < x_min) x_min = xv;
            if (xv > x_max) x_max = xv;
            if (yv < y_min) y_min = yv;
            if (yv > y_max) y_max = yv;

            if (color_field && c_vec) {
                int c_valid = !c_validity || duckdb_validity_row_is_valid(c_validity, r);
                float cv = c_valid ? (float)get_double_from_vector(c_vec, c_type, r) : 0.0f;
                out->color_values[row_offset + r] = cv;
                if (cv < c_min) c_min = cv;
                if (cv > c_max) c_max = cv;
            }
        }

        row_offset += chunk_size;
        duckdb_destroy_data_chunk(&chunk);
    }

    out->count = (uint32_t)total_rows;
    out->x_min = x_min;
    out->x_max = x_max;
    out->y_min = y_min;
    out->y_max = y_max;
    out->color_min = c_min;
    out->color_max = c_max;

    fprintf(stderr, "Loaded %u rows. X: [%.4f, %.4f] Y: [%.4f, %.4f]\n",
            out->count, x_min, x_max, y_min, y_max);

    duckdb_destroy_result(&result);
    duckdb_disconnect(&con);
    duckdb_close(&db);
    return 0;
}

void data_free(DataSet *ds) {
    free(ds->x);
    free(ds->y);
    free(ds->color_values);
    memset(ds, 0, sizeof(DataSet));
}
