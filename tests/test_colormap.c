#include "colormap.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

static void test_colormap_from_name(void) {
    assert(colormap_from_name("viridis") == COLORMAP_VIRIDIS);
    assert(colormap_from_name("inferno") == COLORMAP_INFERNO);
    assert(colormap_from_name("plasma") == COLORMAP_PLASMA);
    assert(colormap_from_name("turbo") == COLORMAP_TURBO);
    assert(colormap_from_name("unknown") == COLORMAP_VIRIDIS);
    assert(colormap_from_name(NULL) == COLORMAP_VIRIDIS);
    assert(colormap_from_name("") == COLORMAP_VIRIDIS);
    printf("  PASS: colormap_from_name\n");
}

static void test_colormap_get_lut(void) {
    for (int i = 0; i < COLORMAP_COUNT; i++) {
        const ColorRGB *lut = colormap_get_lut((ColormapType)i);
        assert(lut != NULL);
        /* Verify first and last entries are not all zeros (sanity check) */
        int has_nonzero = lut[0].r + lut[0].g + lut[0].b +
                          lut[255].r + lut[255].g + lut[255].b;
        assert(has_nonzero > 0);
    }
    /* Out-of-range should fall back to viridis */
    const ColorRGB *lut = colormap_get_lut((ColormapType)99);
    const ColorRGB *viridis = colormap_get_lut(COLORMAP_VIRIDIS);
    assert(lut == viridis);
    printf("  PASS: colormap_get_lut\n");
}

static void test_colormap_sample_clamping(void) {
    /* t < 0 should clamp to index 0 */
    ColorRGB c0 = colormap_sample(COLORMAP_VIRIDIS, -1.0f);
    ColorRGB c_first = colormap_get_lut(COLORMAP_VIRIDIS)[0];
    assert(c0.r == c_first.r && c0.g == c_first.g && c0.b == c_first.b);

    /* t > 1 should clamp to index 255 */
    ColorRGB c1 = colormap_sample(COLORMAP_VIRIDIS, 2.0f);
    ColorRGB c_last = colormap_get_lut(COLORMAP_VIRIDIS)[255];
    assert(c1.r == c_last.r && c1.g == c_last.g && c1.b == c_last.b);

    printf("  PASS: colormap_sample clamping\n");
}

static void test_colormap_sample_midpoint(void) {
    /* t = 0.5 should give index 127 */
    ColorRGB c = colormap_sample(COLORMAP_TURBO, 0.5f);
    ColorRGB expected = colormap_get_lut(COLORMAP_TURBO)[127];
    assert(c.r == expected.r && c.g == expected.g && c.b == expected.b);
    printf("  PASS: colormap_sample midpoint\n");
}

static void test_colormap_sample_endpoints(void) {
    /* t = 0 should give index 0 */
    ColorRGB c0 = colormap_sample(COLORMAP_INFERNO, 0.0f);
    ColorRGB expected0 = colormap_get_lut(COLORMAP_INFERNO)[0];
    assert(c0.r == expected0.r && c0.g == expected0.g && c0.b == expected0.b);

    /* t = 1.0 should give index 255 */
    ColorRGB c1 = colormap_sample(COLORMAP_INFERNO, 1.0f);
    ColorRGB expected1 = colormap_get_lut(COLORMAP_INFERNO)[255];
    assert(c1.r == expected1.r && c1.g == expected1.g && c1.b == expected1.b);

    printf("  PASS: colormap_sample endpoints\n");
}

static void test_all_colormaps_have_distinct_entries(void) {
    for (int i = 0; i < COLORMAP_COUNT; i++) {
        const ColorRGB *lut = colormap_get_lut((ColormapType)i);
        /* First and last entries should differ for a useful colormap */
        int differs = (lut[0].r != lut[255].r) ||
                      (lut[0].g != lut[255].g) ||
                      (lut[0].b != lut[255].b);
        assert(differs);
    }
    printf("  PASS: all colormaps have distinct first/last entries\n");
}

int main(void) {
    printf("Running colormap tests...\n");
    test_colormap_from_name();
    test_colormap_get_lut();
    test_colormap_sample_clamping();
    test_colormap_sample_midpoint();
    test_colormap_sample_endpoints();
    test_all_colormaps_have_distinct_entries();
    printf("All colormap tests passed.\n");
    return 0;
}
