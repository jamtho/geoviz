#ifndef DATA_H
#define DATA_H

#include <stdint.h>
#include <stdbool.h>

typedef struct {
    float *x;
    float *y;
    float *color_values;   /* NULL if query has no 'color' column */
    uint32_t count;
    float x_min, x_max;
    float y_min, y_max;
    float color_min, color_max;
    bool has_color;
} DataSet;

/* Execute SQL via DuckDB and load results. Expects columns named lon, lat,
   and optionally color. Returns 0 on success. */
int data_load(const char *sql, DataSet *out);

/* Free the dataset arrays */
void data_free(DataSet *ds);

#endif /* DATA_H */
