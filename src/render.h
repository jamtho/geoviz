#ifndef RENDER_H
#define RENDER_H

#include "data.h"
#include "spec.h"
#include "mercator.h"
#include "raylib.h"

/* Initialize the render overlay (allocates pixel buffer) */
void render_init(int width, int height);

/* Resize the overlay (call on window resize) */
void render_resize(int width, int height);

/* Re-rasterise all layers into the overlay buffer */
void render_rasterise(const Spec *spec, const DataSet *ds,
                      const Viewport *vp, int width, int height);

/* Draw the overlay texture */
void render_draw_overlay(void);

/* Cleanup */
void render_shutdown(void);

#endif /* RENDER_H */
