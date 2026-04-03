#include "data.h"
#include "../third_party/duckdb/duckdb.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <float.h>
#include <math.h>

/* For DECIMAL columns, wrap the user query to cast to DOUBLE */
static char *wrap_decimal_casts(const char *sql, duckdb_result *result) {
    /* Check if any column is DECIMAL */
    idx_t col_count = duckdb_column_count(result);
    int has_decimal = 0;
    for (idx_t i = 0; i < col_count; i++) {
        if (duckdb_column_type(result, i) == DUCKDB_TYPE_DECIMAL) {
            has_decimal = 1;
            break;
        }
    }
    if (!has_decimal) return NULL;

    /* Build: SELECT CAST(x AS DOUBLE) AS x, ... FROM (<original sql>) AS _t */
    size_t buf_size = strlen(sql) + col_count * 64 + 128;
    char *buf = (char *)malloc(buf_size);
    if (!buf) return NULL;

    size_t pos = 0;
    pos += snprintf(buf + pos, buf_size - pos, "SELECT ");
    for (idx_t i = 0; i < col_count; i++) {
        const char *name = duckdb_column_name(result, i);
        if (i > 0) pos += snprintf(buf + pos, buf_size - pos, ", ");
        if (duckdb_column_type(result, i) == DUCKDB_TYPE_DECIMAL) {
            pos += snprintf(buf + pos, buf_size - pos, "CAST(\"%s\" AS DOUBLE) AS \"%s\"", name, name);
        } else {
            pos += snprintf(buf + pos, buf_size - pos, "\"%s\"", name);
        }
    }
    pos += snprintf(buf + pos, buf_size - pos, " FROM (%s) AS _t", sql);
    return buf;
}

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

/* Find a column index by name. Returns -1 if not found. */
static int find_column(duckdb_result *result, const char *name) {
    idx_t col_count = duckdb_column_count(result);
    for (idx_t i = 0; i < col_count; i++) {
        const char *col_name = duckdb_column_name(result, i);
        if (col_name && strcmp(col_name, name) == 0) {
            return (int)i;
        }
    }
    return -1;
}

int data_load(const char *sql, DataSet *out) {
    memset(out, 0, sizeof(DataSet));

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

    /* Enable auto-loading of extensions (parquet, httpfs, etc.) */
    {
        duckdb_result res;
        duckdb_query(con, "SET autoinstall_known_extensions=1; SET autoload_known_extensions=1;", &res);
        duckdb_destroy_result(&res);
    }

    /* Install httpfs if SQL references S3 */
    if (strstr(sql, "s3://") || strstr(sql, "S3://")) {
        duckdb_result res;
        if (duckdb_query(con, "INSTALL httpfs; LOAD httpfs;", &res) != DuckDBSuccess) {
            fprintf(stderr, "Warning: failed to load httpfs extension\n");
        }
        duckdb_destroy_result(&res);
    }

    fprintf(stderr, "Executing query: %s\n", sql);

    duckdb_result result;
    if (duckdb_query(con, sql, &result) != DuckDBSuccess) {
        const char *err = duckdb_result_error(&result);
        fprintf(stderr, "Error: DuckDB query failed: %s\n", err ? err : "unknown error");
        duckdb_destroy_result(&result);
        duckdb_disconnect(&con);
        duckdb_close(&db);
        return -1;
    }

    /* If any columns are DECIMAL, re-run with casts to DOUBLE */
    char *wrapped = wrap_decimal_casts(sql, &result);
    if (wrapped) {
        duckdb_destroy_result(&result);
        fprintf(stderr, "Re-executing with DECIMAL casts: %s\n", wrapped);
        if (duckdb_query(con, wrapped, &result) != DuckDBSuccess) {
            const char *err = duckdb_result_error(&result);
            fprintf(stderr, "Error: DuckDB query failed: %s\n", err ? err : "unknown error");
            free(wrapped);
            duckdb_destroy_result(&result);
            duckdb_disconnect(&con);
            duckdb_close(&db);
            return -1;
        }
        free(wrapped);
    }

    /* Find columns by name */
    int x_col = find_column(&result, "x");
    int y_col = find_column(&result, "y");
    int color_col = find_column(&result, "color");

    if (x_col < 0 || y_col < 0) {
        fprintf(stderr, "Error: query result must have columns named 'x' and 'y'\n");
        duckdb_destroy_result(&result);
        duckdb_disconnect(&con);
        duckdb_close(&db);
        return -1;
    }

    /* Validate column types */
    if (!is_numeric_type(duckdb_column_type(&result, x_col))) {
        fprintf(stderr, "Error: column 'x' is not numeric\n");
        duckdb_destroy_result(&result);
        duckdb_disconnect(&con);
        duckdb_close(&db);
        return -1;
    }
    if (!is_numeric_type(duckdb_column_type(&result, y_col))) {
        fprintf(stderr, "Error: column 'y' is not numeric\n");
        duckdb_destroy_result(&result);
        duckdb_disconnect(&con);
        duckdb_close(&db);
        return -1;
    }
    if (color_col >= 0 && !is_numeric_type(duckdb_column_type(&result, color_col))) {
        fprintf(stderr, "Warning: column 'color' is not numeric, ignoring\n");
        color_col = -1;
    }

    out->has_color = (color_col >= 0);

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
    if (out->has_color) {
        out->color_values = (float *)malloc(total_rows * sizeof(float));
    }

    if (!out->x || !out->y || (out->has_color && !out->color_values)) {
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

    duckdb_type x_type = duckdb_column_type(&result, x_col);
    duckdb_type y_type = duckdb_column_type(&result, y_col);
    duckdb_type c_type = out->has_color ? duckdb_column_type(&result, color_col) : DUCKDB_TYPE_FLOAT;

    for (idx_t ci = 0; ci < chunk_count; ci++) {
        duckdb_data_chunk chunk = duckdb_result_get_chunk(result, ci);
        idx_t chunk_size = duckdb_data_chunk_get_size(chunk);

        duckdb_vector x_vec = duckdb_data_chunk_get_vector(chunk, x_col);
        duckdb_vector y_vec = duckdb_data_chunk_get_vector(chunk, y_col);
        duckdb_vector c_vec = out->has_color ? duckdb_data_chunk_get_vector(chunk, color_col) : NULL;

        uint64_t *x_validity = duckdb_vector_get_validity(x_vec);
        uint64_t *y_validity = duckdb_vector_get_validity(y_vec);
        uint64_t *c_validity = c_vec ? duckdb_vector_get_validity(c_vec) : NULL;

        for (idx_t r = 0; r < chunk_size; r++) {
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

            if (out->has_color && c_vec) {
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
