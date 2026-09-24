#include "ui/buffer_font_render.h"
#include "ui/ui_object_raw.h"
#include "ui/ui_drawer.h"
#include "ui/ui_surface.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(PIXEL_FORMAT_RGB565)
#include "fixtures/54A99503218B6635058DAE67B9FF007B_resized.h"
#endif

#ifndef TREELIKE_UI_TEST_BITMAP_FONT_PATH
#define TREELIKE_UI_TEST_BITMAP_FONT_PATH "tests/fixtures/path_bitmap.tlf"
#endif

#ifndef TREELIKE_UI_TEST_VECTOR_FONT_PATH
#define TREELIKE_UI_TEST_VECTOR_FONT_PATH "tests/fixtures/path_vector.tlf"
#endif

#define ARRAY_COUNT(array) (sizeof(array) / sizeof((array)[0]))

#define CHECK(condition) do { \
    if (!(condition)) { \
        fprintf(stderr, "CHECK failed at %s:%d: %s\n", \
                __FILE__, __LINE__, #condition); \
        return false; \
    } \
} while (0)

typedef struct {
    bool called;
    UiRect root_bounds;
    UiRect control_bounds[4];
    size_t control_groups[4];
    UiRect group_bounds[4];
    size_t group_sizes[4];
    size_t control_count;
    size_t group_count;
    int grouping_threshold;
} DebugCapture;

static bool text_metrics_equal(UiTextMetrics a, UiTextMetrics b)
{
    return a.width == b.width && a.height == b.height &&
           a.baseline == b.baseline && a.glyph_count == b.glyph_count &&
           a.line_count == b.line_count;
}

static size_t count_pixels(const pixel_t *pixels, int width, int height,
                           int stride, pixel_t color)
{
    size_t count = 0;

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            if (pixels[(size_t)y * (size_t)stride + (size_t)x] == color) {
                ++count;
            }
        }
    }
    return count;
}

static size_t count_non_background(const pixel_t *pixels, int width,
                                   int height, int stride,
                                   pixel_t background)
{
    return (size_t)width * (size_t)height -
           count_pixels(pixels, width, height, stride, background);
}

static void capture_render_tree(const UiRenderTreeDebugSnapshot *snapshot,
                                void *context)
{
    DebugCapture *capture = context;
    size_t copy_count = snapshot->control_count;

    if (copy_count > ARRAY_COUNT(capture->control_groups)) {
        copy_count = ARRAY_COUNT(capture->control_groups);
    }
    capture->called = true;
    capture->root_bounds = snapshot->root_bounds;
    capture->control_count = snapshot->control_count;
    capture->group_count = snapshot->group_count;
    capture->grouping_threshold = snapshot->grouping_threshold;
    for (size_t i = 0; i < copy_count; ++i) {
        capture->control_bounds[i] = snapshot->control_bounds[i];
        capture->control_groups[i] = snapshot->control_groups[i];
        capture->group_bounds[i] = snapshot->group_bounds[i];
        capture->group_sizes[i] = snapshot->group_sizes[i];
    }
}

static void draw_solid(UiBuffer *buffer, void *context)
{
    const pixel_t color = *(const pixel_t *)context;

    for (int y = 0; y < buffer->height; ++y) {
        for (int x = 0; x < buffer->width; ++x) {
            buffer->pixels[(size_t)y * (size_t)buffer->stride + (size_t)x] =
                color;
        }
    }
}

static bool test_rects(void)
{
    UiRect intersection = ui_rect_intersection((UiRect){-2, 2, 6, 5},
                                                (UiRect){0, 0, 8, 4});
    UiRect united = ui_rect_union((UiRect){1, 2, 3, 4},
                                  (UiRect){5, 1, 2, 2});
    CHECK(intersection.x == 0 && intersection.y == 2);
    CHECK(intersection.w == 4 && intersection.h == 2);
    CHECK(united.x == 1 && united.y == 1);
    CHECK(united.w == 6 && united.h == 5);
    return true;
}

static bool test_clipped_blit_and_dirty_union(void)
{
    UiSurface surface;
    const pixel_t source[12] = {
        1, 2, 3, 4,
        5, 6, 7, 8,
        9, 10, 11, 12
    };

    CHECK(ui_surface_create(&surface, 5, 4));
    ui_surface_clear_dirty(&surface);
    CHECK(ui_surface_blit(&surface, source, 4, 3, 4, -2, 1));
    CHECK(surface.pixels[1 * surface.stride + 0] == 3);
    CHECK(surface.pixels[1 * surface.stride + 1] == 4);
    CHECK(surface.pixels[3 * surface.stride + 0] == 11);
    CHECK(surface.dirty.x == 0 && surface.dirty.y == 1);
    CHECK(surface.dirty.w == 2 && surface.dirty.h == 3);

    ui_surface_mark_dirty(&surface, (UiRect){4, 0, 4, 2});
    CHECK(surface.dirty.x == 0 && surface.dirty.y == 0);
    CHECK(surface.dirty.w == 5 && surface.dirty.h == 4);
    ui_surface_destroy(&surface);
    return true;
}

static bool test_ui_tree_is_platform_free(void)
{
    UiSurface surface;
    UiBuffer *root;
    UiRoundedRectStyle style = {2, COLOR_WHITE, COLOR_BLUE, true};
    UiControl controls[] = {
        {{1, 1, 5, 4}, ui_draw_rounded_rect, &style},
        {{7, 2, 4, 4}, ui_draw_debug_border, NULL}
    };

    CHECK(ui_surface_create(&surface, 16, 10));
    ui_surface_fill(&surface, COLOR_BLACK);
    ui_surface_clear_dirty(&surface);
    root = ui_buffer_root(&surface);
    CHECK(root != NULL);
    CHECK(ui_build_render_tree(root, controls, 2, 2));
    ui_buffer_render(root);
    CHECK(surface.has_dirty);
    CHECK(surface.dirty.x == 1 && surface.dirty.y == 1);
    CHECK(surface.dirty.w == 10 && surface.dirty.h == 5);
    CHECK(surface.pixels[1 * surface.stride + 3] == COLOR_WHITE);
    CHECK(surface.pixels[3 * surface.stride + 3] == COLOR_BLUE);
    CHECK(surface.pixels[2 * surface.stride + 7] == COLOR_RED);
    ui_buffer_destroy_tree(root);
    ui_surface_destroy(&surface);
    return true;
}

static bool test_raw_picture_and_blend_endpoints(void)
{
    UiSurface surface;
    UiBuffer *root;
    UiBuffer *picture_buffer;
    const pixel_t pixels[] = {
        COLOR_RED, COLOR_GREEN, COLOR_BLUE, COLOR_BLACK,
        COLOR_WHITE, COLOR_YELLOW, COLOR_CYAN, COLOR_BLACK
    };
    const UiPicture picture = {pixels, 3, 2, 4};

    CHECK(ui_surface_create(&surface, 6, 4));
    ui_surface_fill(&surface, COLOR_BLACK);
    ui_surface_clear_dirty(&surface);
    root = ui_buffer_root(&surface);
    CHECK(root != NULL);
    picture_buffer = ui_buffer_create(root, (UiRect){2, 1, 2, 2});
    CHECK(picture_buffer != NULL);
    picture_buffer->draw = ui_draw_picture;
    picture_buffer->context = (void *)&picture;
    ui_buffer_mark_dirty(picture_buffer);
    ui_buffer_render(root);

    CHECK(surface.pixels[1 * surface.stride + 2] == COLOR_RED);
    CHECK(surface.pixels[1 * surface.stride + 3] == COLOR_GREEN);
    CHECK(surface.pixels[2 * surface.stride + 2] == COLOR_WHITE);
    CHECK(surface.pixels[2 * surface.stride + 3] == COLOR_YELLOW);
    CHECK(surface.pixels[1 * surface.stride + 4] == COLOR_BLACK);
    CHECK(surface.dirty.x == 2 && surface.dirty.y == 1);
    CHECK(surface.dirty.w == 2 && surface.dirty.h == 2);
    CHECK(ui_blend_pixels(COLOR_BLUE, COLOR_RED, 0) == COLOR_BLUE);
    CHECK(ui_blend_pixels(COLOR_BLUE, COLOR_RED, 255) == COLOR_RED);

    ui_buffer_destroy_tree(root);
    ui_surface_destroy(&surface);
    return true;
}

static bool test_supplied_rgb565_picture(void)
{
#if defined(PIXEL_FORMAT_RGB565)
    UiSurface surface;
    UiBuffer *root;
    UiBuffer *picture_buffer;
    const UiPicture picture = {
        _54A99503218B6635058DAE67B9FF007B_resized,
        TREELIKE_UI_TEST_IMAGE_WIDTH,
        TREELIKE_UI_TEST_IMAGE_HEIGHT,
        TREELIKE_UI_TEST_IMAGE_WIDTH
    };

    CHECK(ui_surface_create(&surface,
                            TREELIKE_UI_TEST_IMAGE_WIDTH + 2,
                            TREELIKE_UI_TEST_IMAGE_HEIGHT + 2));
    ui_surface_fill(&surface, COLOR_BLACK);
    ui_surface_clear_dirty(&surface);
    root = ui_buffer_root(&surface);
    CHECK(root != NULL);
    picture_buffer = ui_buffer_create(
        root,
        (UiRect){1, 1,
                 TREELIKE_UI_TEST_IMAGE_WIDTH,
                 TREELIKE_UI_TEST_IMAGE_HEIGHT});
    CHECK(picture_buffer != NULL);
    picture_buffer->draw = ui_draw_picture;
    picture_buffer->context = (void *)&picture;
    ui_buffer_mark_dirty(picture_buffer);
    ui_buffer_render(root);

    for (int y = 0; y < TREELIKE_UI_TEST_IMAGE_HEIGHT; ++y) {
        CHECK(memcmp(surface.pixels +
                         (size_t)(y + 1) * (size_t)surface.stride + 1u,
                     _54A99503218B6635058DAE67B9FF007B_resized +
                         (size_t)y * TREELIKE_UI_TEST_IMAGE_WIDTH,
                     (size_t)TREELIKE_UI_TEST_IMAGE_WIDTH *
                         sizeof(pixel_t)) == 0);
    }
    CHECK(surface.dirty.x == 1 && surface.dirty.y == 1);
    CHECK(surface.dirty.w == TREELIKE_UI_TEST_IMAGE_WIDTH);
    CHECK(surface.dirty.h == TREELIKE_UI_TEST_IMAGE_HEIGHT);
    CHECK(surface.pixels[0] == COLOR_BLACK);

    ui_buffer_destroy_tree(root);
    ui_surface_destroy(&surface);
#endif
    return true;
}

static bool test_bitmap_font_control_and_golden_mask(void)
{
    static const uint8_t a_rows[7] = {
        0x0eu, 0x11u, 0x11u, 0x1fu, 0x11u, 0x11u, 0x11u
    };
    UiSurface surface;
    UiBuffer *root;
    text_renderer_t renderer;
    UiTextMetrics measured;
    UiTextRenderContext context = {
        .renderer = &renderer,
        .text = "A",
        .origin_x = 1,
        .origin_y = 1,
        .pixel_height = 7,
        .color = COLOR_WHITE,
        .background_color = COLOR_BLACK
    };
    UiControl control = {{0, 0, 7, 9}, NULL, NULL};

    CHECK(buffer_font_renderer_init(&renderer, &ui_font_bitmap_5x7));
    CHECK(renderer.engine == UI_FONT_ENGINE_BITMAP);
    CHECK(buffer_font_measure(&context, &measured) == UI_FONT_STATUS_OK);
    CHECK(measured.width == 6 && measured.height == 7);
    CHECK(measured.baseline == 6 && measured.glyph_count == 1);
    CHECK(measured.line_count == 1);

    CHECK(ui_surface_create(&surface, 7, 9));
    ui_surface_fill(&surface, COLOR_BLACK);
    ui_surface_clear_dirty(&surface);
    root = ui_buffer_root(&surface);
    CHECK(root != NULL);

    /* Exercise the renderer exactly as a raw UiControl draw callback. */
    control.draw = buffer_font_draw;
    control.context = &context;
    CHECK(ui_build_render_tree(root, &control, 1, 0));
    ui_buffer_render(root);
    CHECK(context.last_status == UI_FONT_STATUS_OK);
    CHECK(text_metrics_equal(measured, context.last_metrics));

    for (int y = 0; y < surface.height; ++y) {
        for (int x = 0; x < surface.width; ++x) {
            bool set = false;

            if (x >= 1 && x < 6 && y >= 1 && y < 8) {
                set = (a_rows[y - 1] &
                       (uint8_t)(1u << (unsigned)(5 - x))) != 0;
            }
            CHECK(surface.pixels[(size_t)y * (size_t)surface.stride +
                                 (size_t)x] ==
                  (set ? COLOR_WHITE : COLOR_BLACK));
        }
    }

    ui_buffer_destroy_tree(root);
    ui_surface_destroy(&surface);
    return true;
}

static bool test_bitmap_font_exact_two_times_scale(void)
{
    static const uint8_t a_rows[7] = {
        0x0eu, 0x11u, 0x11u, 0x1fu, 0x11u, 0x11u, 0x11u
    };
    UiSurface surface;
    UiBuffer *root;
    text_renderer_t renderer;
    UiTextMetrics metrics;
    UiTextRenderContext context = {
        .renderer = &renderer,
        .text = "A",
        .origin_x = 1,
        .origin_y = 1,
        .pixel_height = 14,
        .color = COLOR_WHITE,
        .background_color = COLOR_BLACK
    };

    CHECK(buffer_font_renderer_init(&renderer, &ui_font_bitmap_5x7));
    CHECK(ui_surface_create(&surface, 12, 16));
    ui_surface_fill(&surface, COLOR_BLACK);
    root = ui_buffer_root(&surface);
    CHECK(root != NULL);
    CHECK(buffer_font_render(root, &context, &metrics) == UI_FONT_STATUS_OK);
    CHECK(metrics.width == 12 && metrics.height == 14);
    CHECK(metrics.baseline == 13);

    for (int source_y = 0; source_y < 7; ++source_y) {
        for (int source_x = 0; source_x < 5; ++source_x) {
            const pixel_t expected =
                (a_rows[source_y] &
                 (uint8_t)(1u << (unsigned)(4 - source_x))) != 0
                    ? COLOR_WHITE : COLOR_BLACK;

            for (int dy = 0; dy < 2; ++dy) {
                for (int dx = 0; dx < 2; ++dx) {
                    const int x = 1 + source_x * 2 + dx;
                    const int y = 1 + source_y * 2 + dy;
                    CHECK(surface.pixels[
                              (size_t)y * (size_t)surface.stride +
                              (size_t)x] == expected);
                }
            }
        }
    }
    for (int y = 0; y < surface.height; ++y) {
        CHECK(surface.pixels[(size_t)y * (size_t)surface.stride] ==
              COLOR_BLACK);
        CHECK(surface.pixels[(size_t)y * (size_t)surface.stride + 11u] ==
              COLOR_BLACK);
    }

    ui_buffer_destroy_tree(root);
    ui_surface_destroy(&surface);
    return true;
}

static bool test_bitmap_font_layout_and_utf8_fallback(void)
{
    enum { WIDTH = 24, HEIGHT = 20 };
    pixel_t unknown_pixels[WIDTH * HEIGHT];
    pixel_t fallback_pixels[WIDTH * HEIGHT];
    UiSurface unknown_surface;
    UiSurface fallback_surface;
    UiBuffer *unknown_root;
    UiBuffer *fallback_root;
    text_renderer_t renderer;
    UiTextMetrics metrics;
    UiTextRenderContext context = {
        .renderer = &renderer,
        .text = " ",
        .origin_x = 1,
        .origin_y = 1,
        .pixel_height = 7,
        .letter_spacing = 1,
        .line_spacing = 2,
        .color = COLOR_WHITE,
        .background_color = COLOR_BLACK
    };

    CHECK(buffer_font_renderer_init(&renderer, &ui_font_bitmap_5x7));
    CHECK(ui_surface_wrap(&unknown_surface, unknown_pixels,
                          WIDTH, HEIGHT, WIDTH));
    CHECK(ui_surface_wrap(&fallback_surface, fallback_pixels,
                          WIDTH, HEIGHT, WIDTH));
    unknown_root = ui_buffer_root(&unknown_surface);
    fallback_root = ui_buffer_root(&fallback_surface);
    CHECK(unknown_root != NULL && fallback_root != NULL);

    ui_surface_fill(&unknown_surface, COLOR_BLACK);
    CHECK(buffer_font_render(unknown_root, &context, &metrics) ==
          UI_FONT_STATUS_OK);
    CHECK(metrics.width == 6 && metrics.height == 7);
    CHECK(metrics.glyph_count == 1 && metrics.line_count == 1);
    CHECK(count_non_background(unknown_pixels, WIDTH, HEIGHT, WIDTH,
                               COLOR_BLACK) == 0);

    context.text = "A A\n\xe2\x98\x83";
    CHECK(buffer_font_measure(&context, &metrics) == UI_FONT_STATUS_OK);
    CHECK(metrics.width == 20 && metrics.height == 16);
    CHECK(metrics.baseline == 6 && metrics.glyph_count == 4);
    CHECK(metrics.line_count == 2);
    CHECK(buffer_font_render(unknown_root, &context, NULL) ==
          UI_FONT_STATUS_OK);
    CHECK(count_non_background(unknown_pixels, WIDTH, HEIGHT, WIDTH,
                               COLOR_BLACK) > 30);

    ui_surface_fill(&unknown_surface, COLOR_BLACK);
    ui_surface_fill(&fallback_surface, COLOR_BLACK);
    context.letter_spacing = 0;
    context.line_spacing = 0;
    context.text = "\xe2\x98\x83";
    CHECK(buffer_font_render(unknown_root, &context, NULL) ==
          UI_FONT_STATUS_OK);
    context.text = "?";
    CHECK(buffer_font_render(fallback_root, &context, NULL) ==
          UI_FONT_STATUS_OK);
    CHECK(memcmp(unknown_pixels, fallback_pixels,
                 sizeof(unknown_pixels)) == 0);

    ui_buffer_destroy_tree(fallback_root);
    ui_buffer_destroy_tree(unknown_root);
    ui_surface_destroy(&fallback_surface);
    ui_surface_destroy(&unknown_surface);
    return true;
}

static bool guards_are_intact(const pixel_t *storage, int width, int height,
                              int stride, pixel_t canary)
{
    for (int x = 0; x < stride; ++x) {
        if (storage[x] != canary ||
            storage[(size_t)(height + 1) * (size_t)stride + (size_t)x] !=
                canary) {
            return false;
        }
    }
    for (int y = 0; y < height; ++y) {
        const pixel_t *row = storage +
            (size_t)(y + 1) * (size_t)stride;
        for (int x = width; x < stride; ++x) {
            if (row[x] != canary) return false;
        }
    }
    return true;
}

static bool test_font_stride_guards_and_clipping(void)
{
    enum { WIDTH = 8, HEIGHT = 8, STRIDE = 11 };
    static const int origins[][2] = {
        {-2, 1}, {1, -2}, {5, 1}, {1, 5}, {20, 20}
    };
    pixel_t storage[(HEIGHT + 2) * STRIDE];
    const pixel_t canary = PIXEL_RGB(17, 34, 51);
    text_renderer_t renderer;

    CHECK(buffer_font_renderer_init(&renderer, &ui_font_bitmap_5x7));
    for (size_t test = 0; test < ARRAY_COUNT(origins); ++test) {
        UiSurface surface;
        UiBuffer *root;
        UiTextMetrics measured;
        UiTextMetrics rendered;
        UiTextRenderContext context = {
            .renderer = &renderer,
            .text = "A",
            .origin_x = origins[test][0],
            .origin_y = origins[test][1],
            .pixel_height = 7,
            .color = COLOR_WHITE,
            .background_color = COLOR_BLACK
        };

        for (size_t i = 0; i < ARRAY_COUNT(storage); ++i) {
            storage[i] = canary;
        }
        CHECK(ui_surface_wrap(&surface, storage + STRIDE,
                              WIDTH, HEIGHT, STRIDE));
        ui_surface_fill(&surface, COLOR_BLACK);
        root = ui_buffer_root(&surface);
        CHECK(root != NULL);
        CHECK(buffer_font_measure(&context, &measured) == UI_FONT_STATUS_OK);
        CHECK(buffer_font_render(root, &context, &rendered) ==
              UI_FONT_STATUS_OK);
        CHECK(text_metrics_equal(measured, rendered));
        CHECK(guards_are_intact(storage, WIDTH, HEIGHT, STRIDE, canary));
        if (test + 1u == ARRAY_COUNT(origins)) {
            CHECK(count_non_background(surface.pixels, WIDTH, HEIGHT,
                                       STRIDE, COLOR_BLACK) == 0);
        } else {
            CHECK(count_non_background(surface.pixels, WIDTH, HEIGHT,
                                       STRIDE, COLOR_BLACK) > 0);
        }
        ui_buffer_destroy_tree(root);
        ui_surface_destroy(&surface);
    }
    return true;
}

static bool region_has_ink(const UiSurface *surface, UiRect region,
                           pixel_t background)
{
    region = ui_rect_intersection(region,
                                  (UiRect){0, 0, surface->width,
                                           surface->height});
    for (int y = region.y; y < region.y + region.h; ++y) {
        for (int x = region.x; x < region.x + region.w; ++x) {
            if (surface->pixels[(size_t)y * (size_t)surface->stride +
                                (size_t)x] != background) {
                return true;
            }
        }
    }
    return false;
}

static bool test_vector_font_rescales_geometry(void)
{
    static const uint16_t heights[] = {9, 13, 17, 31};
    text_renderer_t renderer;
    size_t previous_coverage = 0;
    int previous_width = 0;

    CHECK(buffer_font_renderer_init(&renderer, &ui_font_vector_stroke));
    CHECK(renderer.engine == UI_FONT_ENGINE_VECTOR_STROKE);
    for (size_t index = 0; index < ARRAY_COUNT(heights); ++index) {
        UiSurface surface;
        UiBuffer *root;
        UiTextMetrics measured;
        const int height = (int)heights[index];
        const int surface_width = height * 3 + 8;
        const int surface_height = height + 6;
        size_t coverage;
        size_t partial_coverage = 0;
        UiTextRenderContext context = {
            .renderer = &renderer,
            .text = "AX",
            .origin_x = 2,
            .origin_y = 2,
            .pixel_height = heights[index],
            .stroke_width = 0,
            .letter_spacing = 1,
            .color = COLOR_WHITE,
            .background_color = COLOR_BLACK
        };
        UiControl control = {
            {0, 0, surface_width, surface_height},
            buffer_font_draw,
            &context
        };

        CHECK(ui_surface_create(&surface, surface_width, surface_height));
        ui_surface_fill(&surface, COLOR_BLACK);
        root = ui_buffer_root(&surface);
        CHECK(root != NULL);
        CHECK(buffer_font_measure(&context, &measured) == UI_FONT_STATUS_OK);
        CHECK(ui_build_render_tree(root, &control, 1, 0));
        ui_buffer_render(root);
        CHECK(context.last_status == UI_FONT_STATUS_OK);
        CHECK(text_metrics_equal(measured, context.last_metrics));
        CHECK(measured.height == height && measured.baseline == height - 1);
        CHECK(measured.glyph_count == 2 && measured.line_count == 1);
        CHECK(measured.width > previous_width);

        coverage = count_non_background(surface.pixels, surface.width,
                                        surface.height, surface.stride,
                                        COLOR_BLACK);
        for (int y = 0; y < surface.height; ++y) {
            for (int x = 0; x < surface.width; ++x) {
                const pixel_t pixel = surface.pixels[
                    (size_t)y * (size_t)surface.stride + (size_t)x];
                if (pixel != COLOR_BLACK && pixel != COLOR_WHITE) {
                    ++partial_coverage;
                }
            }
        }
        CHECK(coverage > previous_coverage);
        CHECK(partial_coverage > 0);

        /* A must retain its apex, crossbar and separated lower legs. */
        CHECK(region_has_ink(&surface,
                             (UiRect){2 + height / 4, 2,
                                      height / 2 + 1, height / 3 + 1},
                             COLOR_BLACK));
        CHECK(region_has_ink(&surface,
                             (UiRect){2, 2 + height / 3,
                                      height, height / 3 + 1},
                             COLOR_BLACK));
        CHECK(region_has_ink(&surface,
                             (UiRect){2, 2 + (height * 2) / 3,
                                      height / 2 + 1, height / 3 + 1},
                             COLOR_BLACK));
        CHECK(region_has_ink(&surface,
                             (UiRect){2 + height / 2,
                                      2 + (height * 2) / 3,
                                      height / 2 + 1, height / 3 + 1},
                             COLOR_BLACK));

        previous_width = measured.width;
        previous_coverage = coverage;
        ui_buffer_destroy_tree(root);
        ui_surface_destroy(&surface);
    }
    return true;
}

static bool test_path_fonts_and_unsupported_addr(void)
{
    static const uint8_t loaded_a_rows[7] = {
        0x11u, 0x0au, 0x04u, 0x04u, 0x04u, 0x0au, 0x11u
    };
    enum { VECTOR_WIDTH = 24, VECTOR_HEIGHT = 18 };
    pixel_t loaded_vector_pixels[VECTOR_WIDTH * VECTOR_HEIGHT];
    pixel_t builtin_vector_pixels[VECTOR_WIDTH * VECTOR_HEIGHT];
    UiFontStorage bitmap_storage = {0};
    UiFontStorage vector_storage = {0};
    UiFontStorage addr_storage = {0};
    font_type_t bitmap_font = {
        .path = TREELIKE_UI_TEST_BITMAP_FONT_PATH
    };
    font_type_t vector_font = {
        .path = TREELIKE_UI_TEST_VECTOR_FONT_PATH
    };
    font_type_t addr_font = {
        .addr = UINT32_C(1)
    };
    text_renderer_t renderer;
    UiSurface surface;
    UiBuffer *root;
    UiTextRenderContext context = {
        .renderer = &renderer,
        .text = "A",
        .pixel_height = 7,
        .color = COLOR_WHITE,
        .background_color = COLOR_BLACK
    };

    /* Address metadata is not a host pointer and must never be dereferenced. */
    CHECK(buffer_font_load(&addr_font, &addr_storage) ==
          UI_FONT_STATUS_UNSUPPORTED_SOURCE);

    CHECK(buffer_font_load(&bitmap_font, &bitmap_storage) ==
          UI_FONT_STATUS_OK);
    CHECK(bitmap_font.engine == UI_FONT_ENGINE_BITMAP);
    CHECK(bitmap_font.design_width == 5 && bitmap_font.design_height == 7);
    CHECK(bitmap_font.design_advance == 6);
    CHECK(buffer_font_renderer_init(&renderer, &bitmap_font));
    CHECK(ui_surface_create(&surface, 5, 7));
    ui_surface_fill(&surface, COLOR_BLACK);
    root = ui_buffer_root(&surface);
    CHECK(root != NULL);
    CHECK(buffer_font_render(root, &context, NULL) == UI_FONT_STATUS_OK);
    for (int y = 0; y < 7; ++y) {
        for (int x = 0; x < 5; ++x) {
            const bool set = (loaded_a_rows[y] &
                (uint8_t)(1u << (unsigned)(4 - x))) != 0;
            CHECK(surface.pixels[(size_t)y * (size_t)surface.stride +
                                 (size_t)x] ==
                  (set ? COLOR_WHITE : COLOR_BLACK));
        }
    }
    ui_buffer_destroy_tree(root);
    ui_surface_destroy(&surface);
    buffer_font_unload(&bitmap_font, &bitmap_storage);
    CHECK(!buffer_font_renderer_init(&renderer, &bitmap_font));

    CHECK(buffer_font_load(&vector_font, &vector_storage) ==
          UI_FONT_STATUS_OK);
    CHECK(vector_font.engine == UI_FONT_ENGINE_VECTOR_STROKE);
    CHECK(buffer_font_renderer_init(&renderer, &vector_font));
    CHECK(ui_surface_wrap(&surface, loaded_vector_pixels,
                          VECTOR_WIDTH, VECTOR_HEIGHT, VECTOR_WIDTH));
    ui_surface_fill(&surface, COLOR_BLACK);
    root = ui_buffer_root(&surface);
    CHECK(root != NULL);
    context.pixel_height = 14;
    CHECK(buffer_font_render(root, &context, NULL) == UI_FONT_STATUS_OK);
    CHECK(count_non_background(loaded_vector_pixels,
                               VECTOR_WIDTH, VECTOR_HEIGHT, VECTOR_WIDTH,
                               COLOR_BLACK) > 0);
    ui_buffer_destroy_tree(root);
    ui_surface_destroy(&surface);

    CHECK(buffer_font_renderer_init(&renderer, &ui_font_vector_stroke));
    CHECK(ui_surface_wrap(&surface, builtin_vector_pixels,
                          VECTOR_WIDTH, VECTOR_HEIGHT, VECTOR_WIDTH));
    ui_surface_fill(&surface, COLOR_BLACK);
    root = ui_buffer_root(&surface);
    CHECK(root != NULL);
    CHECK(buffer_font_render(root, &context, NULL) == UI_FONT_STATUS_OK);
    CHECK(memcmp(loaded_vector_pixels, builtin_vector_pixels,
                 sizeof(loaded_vector_pixels)) != 0);
    ui_buffer_destroy_tree(root);
    ui_surface_destroy(&surface);
    buffer_font_unload(&vector_font, &vector_storage);
    return true;
}

static bool test_truetype_path_font(void)
{
#if defined(TREELIKE_UI_HAS_TRUETYPE) && \
    defined(TREELIKE_UI_TEST_TRUETYPE_FONT_PATH)
    enum { WIDTH = 200, HEIGHT = 64, STRIDE = 207 };
    pixel_t guarded[(HEIGHT + 2) * STRIDE];
    pixel_t comparison_a[WIDTH * HEIGHT];
    pixel_t comparison_b[WIDTH * HEIGHT];
    const pixel_t canary = PIXEL_RGB(29, 47, 61);
    UiFontStorage storage = {0};
    UiFontStorage wrong_storage = {0};
    font_type_t font = {
        .name = "Fantasque Sans Mono",
        .path = TREELIKE_UI_TEST_TRUETYPE_FONT_PATH
    };
    text_renderer_t renderer;
    UiSurface surface;
    UiBuffer *root;
    UiTextMetrics measured;
    size_t coverage;
    size_t partial_coverage = 0u;
    UiTextRenderContext context = {
        .renderer = &renderer,
        .text = "VECTOR",
        .origin_x = 3,
        .origin_y = 3,
        .pixel_height = 31,
        .letter_spacing = 1,
        .color = COLOR_WHITE,
        .background_color = COLOR_BLACK
    };
    UiControl control = {
        {0, 0, WIDTH, HEIGHT},
        buffer_font_draw,
        &context
    };

    CHECK(buffer_font_load(&font, &storage) == UI_FONT_STATUS_OK);
    CHECK(font.engine == UI_FONT_ENGINE_VECTOR_OUTLINE);
    CHECK(buffer_font_renderer_init(&renderer, &font));
    CHECK(renderer.engine == UI_FONT_ENGINE_VECTOR_OUTLINE);
    CHECK(buffer_font_measure(&context, &measured) == UI_FONT_STATUS_OK);
    CHECK(measured.width > 0 && measured.width < WIDTH - context.origin_x);
    CHECK(measured.height > 0 && measured.height < HEIGHT - context.origin_y);
    CHECK(measured.baseline > 0 && measured.baseline <= measured.height);
    CHECK(measured.glyph_count == 6u && measured.line_count == 1u);

    for (size_t index = 0u; index < ARRAY_COUNT(guarded); ++index) {
        guarded[index] = canary;
    }
    CHECK(ui_surface_wrap(&surface, guarded + STRIDE,
                          WIDTH, HEIGHT, STRIDE));
    ui_surface_fill(&surface, COLOR_BLACK);
    root = ui_buffer_root(&surface);
    CHECK(root != NULL);
    CHECK(ui_build_render_tree(root, &control, 1u, 0));
    ui_buffer_render(root);
    CHECK(context.last_status == UI_FONT_STATUS_OK);
    CHECK(text_metrics_equal(measured, context.last_metrics));
    CHECK(guards_are_intact(guarded, WIDTH, HEIGHT, STRIDE, canary));

    coverage = count_non_background(surface.pixels, WIDTH, HEIGHT, STRIDE,
                                    COLOR_BLACK);
    CHECK(coverage > 100u);
    for (int y = 0; y < HEIGHT; ++y) {
        for (int x = 0; x < WIDTH; ++x) {
            pixel_t pixel = surface.pixels[(size_t)y * STRIDE + (size_t)x];
            if (pixel != COLOR_BLACK && pixel != COLOR_WHITE) {
                ++partial_coverage;
            }
        }
    }
    CHECK(partial_coverage > 0u);

    /* Outline metrics, rasterization and baseline all come from one GDI face. */
    {
        static const uint16_t heights[] = {9u, 13u, 17u, 31u};
        int previous_height = 0;

        for (size_t size_index = 0u;
             size_index < ARRAY_COUNT(heights); ++size_index) {
            UiTextMetrics rendered;
            bool above_baseline = false;
            bool below_baseline = false;
            int baseline_y;

            ui_surface_fill(&surface, COLOR_BLACK);
            context.text = "Ag";
            context.origin_x = 3;
            context.origin_y = 2;
            context.pixel_height = heights[size_index];
            context.letter_spacing = 0;
            CHECK(buffer_font_measure(&context, &measured) ==
                  UI_FONT_STATUS_OK);
            CHECK(buffer_font_render(root, &context, &rendered) ==
                  UI_FONT_STATUS_OK);
            CHECK(text_metrics_equal(measured, rendered));
            CHECK(measured.height > previous_height);
            CHECK(measured.baseline > 0 &&
                  measured.baseline < measured.height);
            baseline_y = context.origin_y + measured.baseline;
            for (int y = 0; y < HEIGHT; ++y) {
                for (int x = 0; x < WIDTH; ++x) {
                    if (surface.pixels[(size_t)y * STRIDE + (size_t)x] !=
                        COLOR_BLACK) {
                        if (y < baseline_y) above_baseline = true;
                        else below_baseline = true;
                    }
                }
            }
            CHECK(above_baseline && below_baseline);
            previous_height = measured.height;
        }
    }

    /* TrueType keeps case and maps unsupported astral input to '?'. */
    context.origin_x = 3;
    context.origin_y = 3;
    context.pixel_height = 31;
    context.text = "g";
    ui_surface_fill(&surface, COLOR_BLACK);
    CHECK(buffer_font_render(root, &context, NULL) == UI_FONT_STATUS_OK);
    for (int y = 0; y < HEIGHT; ++y) {
        memcpy(comparison_a + (size_t)y * WIDTH,
               surface.pixels + (size_t)y * STRIDE,
               (size_t)WIDTH * sizeof(pixel_t));
    }
    context.text = "G";
    ui_surface_fill(&surface, COLOR_BLACK);
    CHECK(buffer_font_render(root, &context, NULL) == UI_FONT_STATUS_OK);
    for (int y = 0; y < HEIGHT; ++y) {
        memcpy(comparison_b + (size_t)y * WIDTH,
               surface.pixels + (size_t)y * STRIDE,
               (size_t)WIDTH * sizeof(pixel_t));
    }
    CHECK(memcmp(comparison_a, comparison_b, sizeof(comparison_a)) != 0);

    context.text = "\xf0\x9f\x98\x80";
    ui_surface_fill(&surface, COLOR_BLACK);
    CHECK(buffer_font_render(root, &context, NULL) == UI_FONT_STATUS_OK);
    for (int y = 0; y < HEIGHT; ++y) {
        memcpy(comparison_a + (size_t)y * WIDTH,
               surface.pixels + (size_t)y * STRIDE,
               (size_t)WIDTH * sizeof(pixel_t));
    }
    context.text = "?";
    ui_surface_fill(&surface, COLOR_BLACK);
    CHECK(buffer_font_render(root, &context, NULL) == UI_FONT_STATUS_OK);
    for (int y = 0; y < HEIGHT; ++y) {
        memcpy(comparison_b + (size_t)y * WIDTH,
               surface.pixels + (size_t)y * STRIDE,
               (size_t)WIDTH * sizeof(pixel_t));
    }
    CHECK(memcmp(comparison_a, comparison_b, sizeof(comparison_a)) == 0);

    context.text = "A\ng";
    context.pixel_height = 17;
    context.line_spacing = 2;
    ui_surface_fill(&surface, COLOR_BLACK);
    CHECK(buffer_font_measure(&context, &measured) == UI_FONT_STATUS_OK);
    CHECK(measured.glyph_count == 2u && measured.line_count == 2u);
    CHECK(buffer_font_render(root, &context, NULL) == UI_FONT_STATUS_OK);
    CHECK(region_has_ink(&surface,
                         (UiRect){0, context.origin_y,
                                  WIDTH, measured.baseline + 1},
                         COLOR_BLACK));
    CHECK(region_has_ink(&surface,
                         (UiRect){0,
                                  context.origin_y + measured.height / 2,
                                  WIDTH, measured.height / 2 + 1},
                         COLOR_BLACK));

    /* Exercise all four clipping edges without touching row padding. */
    ui_surface_fill(&surface, COLOR_BLACK);
    context.text = "VECTOR";
    context.pixel_height = 31;
    context.line_spacing = 0;
    context.origin_x = -8;
    context.origin_y = -8;
    CHECK(buffer_font_render(root, &context, NULL) == UI_FONT_STATUS_OK);
    CHECK(count_non_background(surface.pixels, WIDTH, HEIGHT, STRIDE,
                               COLOR_BLACK) > 0u);
    CHECK(guards_are_intact(guarded, WIDTH, HEIGHT, STRIDE, canary));
    ui_surface_fill(&surface, COLOR_BLACK);
    context.origin_x = WIDTH - 8;
    context.origin_y = HEIGHT - 8;
    CHECK(buffer_font_render(root, &context, NULL) == UI_FONT_STATUS_OK);
    CHECK(guards_are_intact(guarded, WIDTH, HEIGHT, STRIDE, canary));
    ui_surface_fill(&surface, COLOR_BLACK);
    context.origin_x = WIDTH + 20;
    context.origin_y = HEIGHT + 20;
    CHECK(buffer_font_render(root, &context, NULL) == UI_FONT_STATUS_OK);
    CHECK(count_non_background(surface.pixels, WIDTH, HEIGHT, STRIDE,
                               COLOR_BLACK) == 0u);
    CHECK(guards_are_intact(guarded, WIDTH, HEIGHT, STRIDE, canary));

    /* Ownership is a strict (font, storage) pair. */
    buffer_font_unload(&font, &wrong_storage);
    CHECK(buffer_font_renderer_init(&renderer, &font));
    CHECK(buffer_font_load(&font, &storage) == UI_FONT_STATUS_OK);
    CHECK(buffer_font_renderer_init(&renderer, &font));
    context.origin_x = 0;
    context.origin_y = 0;
    CHECK(buffer_font_measure(&context, &measured) == UI_FONT_STATUS_OK);

    buffer_font_unload(&font, &storage);
    CHECK(!buffer_font_renderer_init(&renderer, &font));
    CHECK(buffer_font_render(root, &context, NULL) ==
          UI_FONT_STATUS_INVALID_FONT);
    buffer_font_unload(&font, &storage);

    ui_buffer_destroy_tree(root);
    ui_surface_destroy(&surface);
#endif
    return true;
}

static bool test_render_tree_debug_snapshot(void)
{
    UiSurface surface;
    UiBuffer *root;
    DebugCapture capture = {0};
    const UiRenderTreeDebugOptions debug = {capture_render_tree, &capture};
    const UiControl controls[] = {
        {{-2, 1, 4, 3}, NULL, NULL},
        {{3, 1, 3, 3}, NULL, NULL},
        {{12, 1, 2, 2}, NULL, NULL}
    };

    CHECK(ui_surface_create(&surface, 16, 8));
    root = ui_buffer_root(&surface);
    CHECK(root != NULL);
    CHECK(ui_build_render_tree_debug(root, controls, ARRAY_COUNT(controls),
                                     2, &debug));
    CHECK(capture.called);
    CHECK(capture.root_bounds.x == 0 && capture.root_bounds.y == 0);
    CHECK(capture.root_bounds.w == 16 && capture.root_bounds.h == 8);
    CHECK(capture.control_count == 3 && capture.group_count == 2);
    CHECK(capture.grouping_threshold == 2);
    CHECK(capture.control_groups[0] == 0);
    CHECK(capture.control_groups[1] == 0);
    CHECK(capture.control_groups[2] == 2);
    CHECK(capture.group_sizes[0] == 2);
    CHECK(capture.group_sizes[1] == 0);
    CHECK(capture.group_sizes[2] == 1);
    CHECK(capture.control_bounds[0].x == 0);
    CHECK(capture.control_bounds[0].w == 2);
    CHECK(capture.group_bounds[0].x == 0 && capture.group_bounds[0].y == 1);
    CHECK(capture.group_bounds[0].w == 6 && capture.group_bounds[0].h == 3);

    ui_buffer_destroy_tree(root);
    ui_surface_destroy(&surface);
    return true;
}

static bool test_fully_clipped_control_has_no_group(void)
{
    UiSurface surface;
    UiBuffer *root;
    DebugCapture capture = {0};
    const UiRenderTreeDebugOptions debug = {capture_render_tree, &capture};
    const UiControl controls[] = {
        {{-4, 1, 2, 2}, NULL, NULL},
        {{0, 1, 2, 2}, NULL, NULL}
    };

    CHECK(ui_surface_create(&surface, 10, 6));
    root = ui_buffer_root(&surface);
    CHECK(root != NULL);
    CHECK(ui_build_render_tree_debug(root, controls, ARRAY_COUNT(controls),
                                     3, &debug));
    CHECK(capture.called);
    CHECK(capture.group_count == 1);
    CHECK(ui_rect_is_empty(capture.control_bounds[0]));
    CHECK(capture.control_groups[0] == UI_RENDER_TREE_NO_GROUP);
    CHECK(capture.control_groups[1] == 1);
    CHECK(capture.group_sizes[0] == 0);
    CHECK(capture.group_sizes[1] == 1);
    CHECK(capture.group_bounds[1].x == 0);
    CHECK(capture.group_bounds[1].y == 1);
    CHECK(capture.group_bounds[1].w == 2);
    CHECK(capture.group_bounds[1].h == 2);
    CHECK(root->first_child != NULL);
    CHECK(root->first_child->next_sibling == NULL);

    ui_buffer_destroy_tree(root);
    ui_surface_destroy(&surface);
    return true;
}

static bool test_grouping_threshold_is_strict(void)
{
    UiSurface surface;
    UiBuffer *root;
    DebugCapture capture = {0};
    const UiRenderTreeDebugOptions debug = {capture_render_tree, &capture};
    const UiControl controls[] = {
        {{1, 1, 3, 3}, NULL, NULL},
        {{7, 1, 3, 3}, NULL, NULL}
    };

    CHECK(ui_surface_create(&surface, 16, 8));
    root = ui_buffer_root(&surface);
    CHECK(root != NULL);
    CHECK(ui_build_render_tree_debug(root, controls, ARRAY_COUNT(controls),
                                     3, &debug));
    CHECK(capture.called);
    CHECK(capture.group_count == 2);
    CHECK(capture.control_groups[0] == 0);
    CHECK(capture.control_groups[1] == 1);
    CHECK(capture.group_sizes[0] == 1);
    CHECK(capture.group_sizes[1] == 1);

    ui_buffer_destroy_tree(root);
    ui_surface_destroy(&surface);
    return true;
}

static bool test_grouping_is_transitive(void)
{
    UiSurface surface;
    UiBuffer *root;
    DebugCapture capture = {0};
    const UiRenderTreeDebugOptions debug = {capture_render_tree, &capture};
    const UiControl controls[] = {
        {{1, 1, 2, 2}, NULL, NULL},
        {{5, 1, 2, 2}, NULL, NULL},
        {{9, 1, 2, 2}, NULL, NULL}
    };

    CHECK(ui_surface_create(&surface, 16, 8));
    root = ui_buffer_root(&surface);
    CHECK(root != NULL);
    CHECK(ui_build_render_tree_debug(root, controls, ARRAY_COUNT(controls),
                                     3, &debug));
    CHECK(capture.called);
    CHECK(capture.group_count == 1);
    CHECK(capture.control_groups[0] == 0);
    CHECK(capture.control_groups[1] == 0);
    CHECK(capture.control_groups[2] == 0);
    CHECK(capture.group_sizes[0] == 3);
    CHECK(capture.group_bounds[0].x == 1 && capture.group_bounds[0].y == 1);
    CHECK(capture.group_bounds[0].w == 10 && capture.group_bounds[0].h == 2);

    ui_buffer_destroy_tree(root);
    ui_surface_destroy(&surface);
    return true;
}

static bool test_group_debug_bounds_overlay(void)
{
    UiSurface surface;
    UiBuffer *root;
    const pixel_t blue = COLOR_BLUE;
    const pixel_t green = COLOR_GREEN;
    const pixel_t white = COLOR_WHITE;
    const UiControl controls[] = {
        {{2, 2, 4, 4}, draw_solid, (void *)&blue},
        {{7, 2, 4, 4}, draw_solid, (void *)&green},
        {{15, 2, 3, 3}, draw_solid, (void *)&white}
    };

    CHECK(ui_surface_create(&surface, 20, 10));
    ui_surface_fill(&surface, COLOR_BLACK);
    root = ui_buffer_root(&surface);
    CHECK(root != NULL);
    CHECK(ui_build_render_tree(root, controls, ARRAY_COUNT(controls), 2));
    CHECK(root->first_child != NULL);
    CHECK(root->first_child->next_sibling != NULL);
    ui_buffer_render(root);
    CHECK(surface.pixels[2 * surface.stride + 3] == COLOR_BLUE);

    ui_surface_clear_dirty(&surface);
    ui_surface_mark_dirty(&surface, (UiRect){15, 2, 3, 3});
    ui_buffer_draw_group_debug_bounds(root, COLOR_RED);

    CHECK(surface.pixels[2 * surface.stride + 2] == COLOR_RED);
    CHECK(surface.pixels[2 * surface.stride + 6] == COLOR_RED);
    CHECK(surface.pixels[5 * surface.stride + 10] == COLOR_RED);
    CHECK(surface.pixels[3 * surface.stride + 3] == COLOR_BLUE);
    CHECK(surface.pixels[3 * surface.stride + 8] == COLOR_GREEN);
    CHECK(surface.pixels[2 * surface.stride + 15] == COLOR_WHITE);
    CHECK(surface.pixels[0] == COLOR_BLACK);
    CHECK(surface.dirty.x == 2 && surface.dirty.y == 2);
    CHECK(surface.dirty.w == 16 && surface.dirty.h == 4);

    ui_buffer_destroy_tree(root);
    ui_surface_destroy(&surface);
    return true;
}

static bool test_ordinary_composite_is_not_group_overlay(void)
{
    UiSurface surface;
    UiBuffer *root;
    UiBuffer *composite;
    UiBuffer *left;
    UiBuffer *right;
    pixel_t blue = COLOR_BLUE;
    pixel_t green = COLOR_GREEN;

    CHECK(ui_surface_create(&surface, 12, 8));
    ui_surface_fill(&surface, COLOR_BLACK);
    root = ui_buffer_root(&surface);
    CHECK(root != NULL);
    composite = ui_buffer_create(root, (UiRect){1, 1, 8, 4});
    CHECK(composite != NULL);
    left = ui_buffer_create(composite, (UiRect){0, 0, 3, 3});
    right = ui_buffer_create(composite, (UiRect){4, 0, 3, 3});
    CHECK(left != NULL && right != NULL);
    left->draw = draw_solid;
    left->context = &blue;
    right->draw = draw_solid;
    right->context = &green;
    ui_buffer_mark_dirty(left);
    ui_buffer_mark_dirty(right);
    ui_buffer_render(root);
    CHECK(surface.pixels[1 * surface.stride + 4] == COLOR_BLACK);

    ui_surface_clear_dirty(&surface);
    ui_buffer_draw_group_debug_bounds(root, COLOR_RED);
    CHECK(!surface.has_dirty);
    CHECK(surface.pixels[1 * surface.stride + 1] == COLOR_BLUE);
    CHECK(surface.pixels[1 * surface.stride + 4] == COLOR_BLACK);
    CHECK(surface.pixels[1 * surface.stride + 5] == COLOR_GREEN);

    ui_buffer_destroy_tree(root);
    ui_surface_destroy(&surface);
    return true;
}

int main(void)
{
    if (!test_rects() || !test_clipped_blit_and_dirty_union() ||
        !test_ui_tree_is_platform_free() ||
        !test_raw_picture_and_blend_endpoints() ||
        !test_supplied_rgb565_picture() ||
        !test_bitmap_font_control_and_golden_mask() ||
        !test_bitmap_font_exact_two_times_scale() ||
        !test_bitmap_font_layout_and_utf8_fallback() ||
        !test_font_stride_guards_and_clipping() ||
        !test_vector_font_rescales_geometry() ||
        !test_path_fonts_and_unsupported_addr() ||
        !test_truetype_path_font() ||
        !test_render_tree_debug_snapshot() ||
        !test_fully_clipped_control_has_no_group() ||
        !test_grouping_threshold_is_strict() ||
        !test_grouping_is_transitive() ||
        !test_group_debug_bounds_overlay() ||
        !test_ordinary_composite_is_not_group_overlay()) {
        return EXIT_FAILURE;
    }
    puts("ui-core-tests: all checks passed");
    return EXIT_SUCCESS;
}
