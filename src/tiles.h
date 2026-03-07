#ifndef TILES_H
#define TILES_H

#include "mercator.h"
#include "spec.h"
#include "raylib.h"

/* Initialize the tile system (creates background thread, cache dir, etc.) */
void tiles_init(void);

/* Shutdown the tile system */
void tiles_shutdown(void);

/* Render basemap tiles for the current viewport */
void tiles_render(const Viewport *vp, BasemapType basemap, int screen_width, int screen_height);

#endif /* TILES_H */
