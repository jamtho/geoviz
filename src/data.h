#ifndef DATA_H
#define DATA_H

#include "spec.h"
#include <stdint.h>
#include <stdbool.h>

typedef struct {
    float *x;
    float *y;
    float *color_values;   /* NULL if no color encoding */
    uint32_t count;
    float x_min, x_max;
    float y_min, y_max;
    float color_min, color_max;
} DataSet;

/* Load data from the URI specified in the spec. Returns 0 on success. */
int data_load(const Spec *spec, DataSet *out);

/* Free the dataset arrays */
void data_free(DataSet *ds);

#endif /* DATA_H */
