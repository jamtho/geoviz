#ifdef _WIN32
/* Must come before raylib.h to avoid Windows API name clashes */
#define NOGDI
#define NOUSER
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <direct.h>
#undef NOGDI
#undef NOUSER
#define mkdir_p(path) _mkdir(path)
#define atomic_int volatile LONG
#define atomic_store(p, v) InterlockedExchange((p), (v))
#define atomic_load(p) InterlockedCompareExchange((p), 0, 0)
#define atomic_fetch_add(p, v) InterlockedExchangeAdd((p), (v))
#define atomic_fetch_sub(p, v) InterlockedExchangeAdd((p), -(v))
#else
#include <pthread.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdatomic.h>
#define mkdir_p(path) mkdir(path, 0755)
#endif

#include "tiles.h"
#include "http.h"
#include "mercator.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* ---------- Tile source URLs ---------- */

typedef enum {
    TILE_SOURCE_OSM = 0,
    TILE_SOURCE_SATELLITE,
    TILE_SOURCE_OPENSEAMAP,
    TILE_SOURCE_COUNT
} TileSource;

static const char *tile_url_templates[TILE_SOURCE_COUNT] = {
    "https://tile.openstreetmap.org/%d/%d/%d.png",                                                      /* OSM: z/x/y */
    "https://server.arcgisonline.com/ArcGIS/rest/services/World_Imagery/MapServer/tile/%d/%d/%d",        /* Satellite: z/y/x */
    "https://tiles.openseamap.org/seamark/%d/%d/%d.png",                                                 /* OpenSeaMap: z/x/y */
};

static void build_tile_url(TileSource src, int z, int x, int y, char *buf, size_t buflen) {
    if (src == TILE_SOURCE_SATELLITE) {
        snprintf(buf, buflen, tile_url_templates[src], z, y, x);  /* z/y/x order */
    } else {
        snprintf(buf, buflen, tile_url_templates[src], z, x, y);  /* z/x/y order */
    }
}

/* ---------- Tile cache ---------- */

#define MAX_CACHED_TILES 512
#define TILE_REQUEST_QUEUE_SIZE 256

typedef enum {
    TILE_STATE_EMPTY = 0,
    TILE_STATE_REQUESTED,
    TILE_STATE_LOADED,
    TILE_STATE_TEXTURE_READY
} TileState;

typedef struct {
    int z, x, y;
    TileSource source;
    TileState state;
    Texture2D texture;
    Image image;
    atomic_int data_ready;  /* Set by background thread when image data is loaded */
} CachedTile;

typedef struct {
    int z, x, y;
    TileSource source;
} TileRequest;

static CachedTile tile_cache[MAX_CACHED_TILES];
static int tile_cache_count = 0;

static TileRequest request_queue[TILE_REQUEST_QUEUE_SIZE];
static int queue_head = 0;
static int queue_tail = 0;
static atomic_int queue_count;

#ifdef _WIN32
static HANDLE queue_mutex;
static HANDLE queue_event;
static HANDLE fetch_thread;
static volatile int shutdown_flag = 0;
#else
static pthread_mutex_t queue_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t queue_cond = PTHREAD_COND_INITIALIZER;
static pthread_t fetch_thread;
static volatile int shutdown_flag = 0;
#endif

static char cache_base_path[512];

static void ensure_cache_dirs(void) {
    const char *home;
#ifdef _WIN32
    home = getenv("USERPROFILE");
#else
    home = getenv("HOME");
#endif
    if (!home) home = ".";
    snprintf(cache_base_path, sizeof(cache_base_path), "%s/.nativeviz/tiles", home);

    /* Create directory hierarchy */
    char tmp[512];
    snprintf(tmp, sizeof(tmp), "%s/.nativeviz", home);
    mkdir_p(tmp);
    mkdir_p(cache_base_path);
}

static void build_cache_path(TileSource src, int z, int x, int y, char *buf, size_t buflen) {
    const char *src_names[] = {"osm", "satellite", "openseamap"};
    char dir1[512], dir2[512], dir3[512];

    snprintf(dir1, sizeof(dir1), "%s/%s", cache_base_path, src_names[src]);
    mkdir_p(dir1);
    snprintf(dir2, sizeof(dir2), "%s/%d", dir1, z);
    mkdir_p(dir2);
    snprintf(dir3, sizeof(dir3), "%s/%d", dir2, x);
    mkdir_p(dir3);
    snprintf(buf, buflen, "%s/%d.png", dir3, y);
}

/* ---------- Tile lookup ---------- */

static CachedTile *find_tile(int z, int x, int y, TileSource src) {
    for (int i = 0; i < tile_cache_count; i++) {
        CachedTile *t = &tile_cache[i];
        if (t->z == z && t->x == x && t->y == y && t->source == src && t->state != TILE_STATE_EMPTY) {
            return t;
        }
    }
    return NULL;
}

static CachedTile *alloc_tile(int z, int x, int y, TileSource src) {
    if (tile_cache_count < MAX_CACHED_TILES) {
        CachedTile *t = &tile_cache[tile_cache_count++];
        memset(t, 0, sizeof(CachedTile));
        t->z = z;
        t->x = x;
        t->y = y;
        t->source = src;
        return t;
    }
    /* Evict oldest tile */
    CachedTile *t = &tile_cache[0];
    if (t->state == TILE_STATE_TEXTURE_READY) {
        UnloadTexture(t->texture);
    }
    memmove(&tile_cache[0], &tile_cache[1], (MAX_CACHED_TILES - 1) * sizeof(CachedTile));
    tile_cache_count = MAX_CACHED_TILES - 1;
    t = &tile_cache[tile_cache_count++];
    memset(t, 0, sizeof(CachedTile));
    t->z = z;
    t->x = x;
    t->y = y;
    t->source = src;
    return t;
}

/* ---------- Request queue ---------- */

static void enqueue_request(int z, int x, int y, TileSource src) {
    if (atomic_load(&queue_count) >= TILE_REQUEST_QUEUE_SIZE) return;

#ifdef _WIN32
    WaitForSingleObject(queue_mutex, INFINITE);
#else
    pthread_mutex_lock(&queue_mutex);
#endif

    request_queue[queue_tail].z = z;
    request_queue[queue_tail].x = x;
    request_queue[queue_tail].y = y;
    request_queue[queue_tail].source = src;
    queue_tail = (queue_tail + 1) % TILE_REQUEST_QUEUE_SIZE;
    atomic_fetch_add(&queue_count, 1);

#ifdef _WIN32
    ReleaseMutex(queue_mutex);
    SetEvent(queue_event);
#else
    pthread_mutex_unlock(&queue_mutex);
    pthread_cond_signal(&queue_cond);
#endif
}

static int dequeue_request(TileRequest *req) {
#ifdef _WIN32
    WaitForSingleObject(queue_mutex, INFINITE);
#else
    pthread_mutex_lock(&queue_mutex);
#endif

    if (atomic_load(&queue_count) == 0) {
#ifdef _WIN32
        ReleaseMutex(queue_mutex);
#else
        pthread_mutex_unlock(&queue_mutex);
#endif
        return 0;
    }

    *req = request_queue[queue_head];
    queue_head = (queue_head + 1) % TILE_REQUEST_QUEUE_SIZE;
    atomic_fetch_sub(&queue_count, 1);

#ifdef _WIN32
    ReleaseMutex(queue_mutex);
#else
    pthread_mutex_unlock(&queue_mutex);
#endif
    return 1;
}

/* ---------- Background fetch thread ---------- */

static void fetch_tile(TileRequest *req) {
    char cache_path[512];
    build_cache_path(req->source, req->z, req->x, req->y, cache_path, sizeof(cache_path));

    /* Check disk cache first */
    FILE *f = fopen(cache_path, "rb");
    if (f) {
        fclose(f);
        /* Mark as data ready - main thread will load the image */
        CachedTile *ct = find_tile(req->z, req->x, req->y, req->source);
        if (ct) {
            ct->image = LoadImage(cache_path);
            if (ct->image.data) {
                atomic_store(&ct->data_ready, 1);
            } else {
                ct->state = TILE_STATE_EMPTY;
            }
        }
        return;
    }

    /* Fetch from network */
    char url[512];
    build_tile_url(req->source, req->z, req->x, req->y, url, sizeof(url));

    HttpResponse resp = http_get(url);
    if (resp.status == 200 && resp.data && resp.len > 0) {
        /* Write to disk cache */
        FILE *out = fopen(cache_path, "wb");
        if (out) {
            fwrite(resp.data, 1, resp.len, out);
            fclose(out);
        }

        /* Load image from memory */
        CachedTile *ct = find_tile(req->z, req->x, req->y, req->source);
        if (ct) {
            ct->image = LoadImageFromMemory(".png", resp.data, (int)resp.len);
            if (ct->image.data) {
                atomic_store(&ct->data_ready, 1);
            } else {
                ct->state = TILE_STATE_EMPTY;
            }
        }
    }
    http_response_free(&resp);
}

#ifdef _WIN32
static DWORD WINAPI fetch_thread_func(LPVOID param) {
    (void)param;
    while (!shutdown_flag) {
        TileRequest req;
        if (dequeue_request(&req)) {
            fetch_tile(&req);
        } else {
            WaitForSingleObject(queue_event, 100);
        }
    }
    return 0;
}
#else
static void *fetch_thread_func(void *param) {
    (void)param;
    while (!shutdown_flag) {
        TileRequest req;
        if (dequeue_request(&req)) {
            fetch_tile(&req);
        } else {
            struct timespec ts;
            clock_gettime(CLOCK_REALTIME, &ts);
            ts.tv_nsec += 100000000; /* 100ms */
            if (ts.tv_nsec >= 1000000000) {
                ts.tv_sec++;
                ts.tv_nsec -= 1000000000;
            }
            pthread_mutex_lock(&queue_mutex);
            pthread_cond_timedwait(&queue_cond, &queue_mutex, &ts);
            pthread_mutex_unlock(&queue_mutex);
        }
    }
    return NULL;
}
#endif

/* ---------- Public API ---------- */

void tiles_init(void) {
    ensure_cache_dirs();
    memset(tile_cache, 0, sizeof(tile_cache));
    tile_cache_count = 0;
    queue_head = 0;
    queue_tail = 0;
    atomic_store(&queue_count, 0);
    shutdown_flag = 0;

#ifdef _WIN32
    queue_mutex = CreateMutex(NULL, FALSE, NULL);
    queue_event = CreateEvent(NULL, FALSE, FALSE, NULL);
    fetch_thread = CreateThread(NULL, 0, fetch_thread_func, NULL, 0, NULL);
#else
    pthread_create(&fetch_thread, NULL, fetch_thread_func, NULL);
#endif
}

void tiles_shutdown(void) {
    shutdown_flag = 1;

#ifdef _WIN32
    SetEvent(queue_event);
    WaitForSingleObject(fetch_thread, 2000);
    CloseHandle(fetch_thread);
    CloseHandle(queue_mutex);
    CloseHandle(queue_event);
#else
    pthread_cond_signal(&queue_cond);
    pthread_join(fetch_thread, NULL);
#endif

    for (int i = 0; i < tile_cache_count; i++) {
        if (tile_cache[i].state == TILE_STATE_TEXTURE_READY) {
            UnloadTexture(tile_cache[i].texture);
        }
    }
    tile_cache_count = 0;
}

static void request_tile(int z, int x, int y, TileSource src) {
    CachedTile *ct = find_tile(z, x, y, src);
    if (ct) return; /* Already cached or requested */

    ct = alloc_tile(z, x, y, src);
    ct->state = TILE_STATE_REQUESTED;
    atomic_store(&ct->data_ready, 0);
    enqueue_request(z, x, y, src);
}

static void render_source_tiles(const Viewport *vp, TileSource src, int screen_width, int screen_height) {
    int z = (int)round(vp->zoom);
    if (z < 0) z = 0;
    if (z > 19) z = 19;

    /* Compute visible tile range */
    double tl_lon = pixel_to_lon(0, vp, screen_width);
    double tl_lat = pixel_to_lat(0, vp, screen_height);
    double br_lon = pixel_to_lon(screen_width, vp, screen_width);
    double br_lat = pixel_to_lat(screen_height, vp, screen_height);

    int tx_min, ty_min, tx_max, ty_max;
    world_to_tile(tl_lon, tl_lat, z, &tx_min, &ty_min);
    world_to_tile(br_lon, br_lat, z, &tx_max, &ty_max);

    /* Add margin */
    tx_min -= 1;
    ty_min -= 1;
    tx_max += 1;
    ty_max += 1;
    int max_tile = 1 << z;
    if (tx_min < 0) tx_min = 0;
    if (ty_min < 0) ty_min = 0;
    if (tx_max >= max_tile) tx_max = max_tile - 1;
    if (ty_max >= max_tile) ty_max = max_tile - 1;

    for (int ty = ty_min; ty <= ty_max; ty++) {
        for (int tx = tx_min; tx <= tx_max; tx++) {
            CachedTile *ct = find_tile(z, tx, ty, src);

            if (!ct) {
                request_tile(z, tx, ty, src);
                continue;
            }

            /* Check if background thread loaded image data */
            if (ct->state == TILE_STATE_REQUESTED && atomic_load(&ct->data_ready)) {
                if (ct->image.data) {
                    ct->texture = LoadTextureFromImage(ct->image);
                    UnloadImage(ct->image);
                    ct->image.data = NULL;
                    ct->state = TILE_STATE_TEXTURE_READY;
                } else {
                    ct->state = TILE_STATE_EMPTY;
                }
            }

            if (ct->state == TILE_STATE_TEXTURE_READY) {
                double sx, sy;
                tile_screen_pos(tx, ty, z, vp, screen_width, screen_height, &sx, &sy);

                /* Scale tile to account for fractional zoom.
                 * Add 1px overlap to prevent gaps from float rounding. */
                double tile_scale = mercator_scale(vp->zoom) / mercator_scale(z);
                float draw_size = (float)(256.0 * tile_scale) + 1.0f;

                Rectangle src_rect = {0, 0, (float)ct->texture.width, (float)ct->texture.height};
                Rectangle dst_rect = {(float)sx, (float)sy, draw_size, draw_size};
                DrawTexturePro(ct->texture, src_rect, dst_rect, (Vector2){0, 0}, 0, WHITE);
            }
        }
    }
}

void tiles_render(const Viewport *vp, BasemapType basemap, int screen_width, int screen_height) {
    switch (basemap) {
        case BASEMAP_OSM:
            render_source_tiles(vp, TILE_SOURCE_OSM, screen_width, screen_height);
            break;
        case BASEMAP_SATELLITE:
            render_source_tiles(vp, TILE_SOURCE_SATELLITE, screen_width, screen_height);
            break;
        case BASEMAP_NAUTICAL:
            render_source_tiles(vp, TILE_SOURCE_OSM, screen_width, screen_height);
            render_source_tiles(vp, TILE_SOURCE_OPENSEAMAP, screen_width, screen_height);
            break;
        case BASEMAP_NONE:
            break;
    }
}
