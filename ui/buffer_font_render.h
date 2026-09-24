/**
 * @file buffer_font_render.h
 * Small bitmap and vector-font renderers for UiBuffer callbacks.
 *
 * The renderer is deliberately a UiBuffer draw service, not a UiObject.  A
 * UiTextRenderContext can be installed directly as a UiBuffer/UiControl draw
 * callback context.
 */

#ifndef TREELIKE_UI_BUFFER_FONT_RENDER_H
#define TREELIKE_UI_BUFFER_FONT_RENDER_H

#include "ui_drawer.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum {
    UI_FONT_ENGINE_BITMAP = 0,
    UI_FONT_ENGINE_VECTOR_STROKE = 1,
    UI_FONT_ENGINE_VECTOR_OUTLINE = 2,

    /* The current outline platform backend consumes TrueType/OpenType files. */
    UI_FONT_ENGINE_TRUETYPE = UI_FONT_ENGINE_VECTOR_OUTLINE
} UiFontEngine;

typedef enum {
    UI_FONT_STATUS_OK = 0,
    UI_FONT_STATUS_INVALID_ARGUMENT,
    UI_FONT_STATUS_INVALID_FONT,
    UI_FONT_STATUS_OVERFLOW,
    UI_FONT_STATUS_IO_ERROR,
    UI_FONT_STATUS_INVALID_DATA,
    UI_FONT_STATUS_UNSUPPORTED_SOURCE,
    UI_FONT_STATUS_OUT_OF_MEMORY,
    UI_FONT_STATUS_PLATFORM_ERROR
} UiFontStatus;

#define UI_FONT_STORAGE_MAX_GLYPHS 64u
#define UI_FONT_STORAGE_MAX_BITMAP_ROWS 16u
#define UI_FONT_STORAGE_MAX_SEGMENTS 32u

typedef struct {
    int16_t x1;
    int16_t y1;
    int16_t x2;
    int16_t y2;
} UiFontStrokeSegment;

typedef struct UiFontStorage UiFontStorage;

typedef struct font_type_t {
    const char *name;
    uint32_t id;

    /** Reserved embedded source address; never dereferenced by path loading. */
    uint32_t addr;

    /** Platform file source used by buffer_font_load(). */
    const char *path;

    UiFontEngine engine;
    uint16_t design_width;
    uint16_t design_height;
    uint16_t design_advance;

    /* Read-only implementation fields populated by built-ins/load(). */
    const uint32_t *_codepoints;
    const uint16_t *_bitmap_rows;
    const uint32_t *_vector_masks;
    const UiFontStrokeSegment *_vector_segments;
    size_t _glyph_count;
    size_t _segment_count;
    const UiFontStorage *_storage;
    uint32_t _format_cookie;
    uint8_t _backend;
} font_type_t;

/**
 * Storage for a path-loaded font.
 *
 * TLFNT1 uses only the fixed-capacity arrays.  An enabled platform outline
 * backend may own heap and OS resources through _platform_data.  Keep this
 * object, its font_type_t, font.name and font.path alive and unmoved while a
 * renderer uses the loaded font, then call buffer_font_unload().  The API does
 * not synchronize access to a font/storage pair; callers must serialize load,
 * rendering and unload operations for that pair.
 */
struct UiFontStorage {
    uint32_t codepoints[UI_FONT_STORAGE_MAX_GLYPHS];
    uint16_t bitmap_rows[UI_FONT_STORAGE_MAX_GLYPHS *
                         UI_FONT_STORAGE_MAX_BITMAP_ROWS];
    uint32_t vector_masks[UI_FONT_STORAGE_MAX_GLYPHS];
    UiFontStrokeSegment vector_segments[UI_FONT_STORAGE_MAX_SEGMENTS];
    bool vector_segment_defined[UI_FONT_STORAGE_MAX_SEGMENTS];

    /* Owned platform state.  Call buffer_font_unload() before reuse. */
    void *_platform_data;
    font_type_t *_owner;
    uint32_t _storage_cookie;
};

/** Initialized, immutable binding between a caller and a font definition. */
typedef struct text_renderer_t {
    const font_type_t *font;
    UiFontEngine engine;
} text_renderer_t;

typedef struct {
    int width;
    int height;
    int baseline;
    size_t glyph_count;
    size_t line_count;
} UiTextMetrics;

typedef struct {
    const text_renderer_t *renderer;
    const char *text;
    int origin_x;
    int origin_y;
    uint16_t pixel_height;
    uint16_t stroke_width;
    int16_t letter_spacing;
    int16_t line_spacing;
    pixel_t color;
    pixel_t background_color;
    bool opaque;

    /* Updated by buffer_font_draw(); ignored by measure/render. */
    UiFontStatus last_status;
    UiTextMetrics last_metrics;
} UiTextRenderContext;

/** Built-in 5 by 7, one-bit ROM font. */
extern const font_type_t ui_font_bitmap_5x7;

/** Built-in scalable font whose glyph source is geometric line segments. */
extern const font_type_t ui_font_vector_stroke;

/**
 * Load font->path into zero-initialized caller-owned storage.  Every load must eventually be
 * paired with buffer_font_unload(), even when the fixed-capacity TLFNT1
 * storage is the selected backend.
 *
 * The portable loader accepts the ASCII TLFNT1 format described below.  When
 * TREELIKE_UI_HAS_TRUETYPE is defined, a .ttf/.otf/.ttc path selects the
 * private platform outline backend.  font->name must then contain the exact
 * registered font family (for example, "Fantasque Sans Mono").
 *
 * An addr-only source returns UI_FONT_STATUS_UNSUPPORTED_SOURCE; addr is
 * metadata only in the platform implementation and is never dereferenced.
 *
 * Empty lines and lines whose first token is '#' are ignored.  Numbers are
 * decimal except codepoints, row masks and segment masks, which are hex:
 *
 *   TLFNT1 BITMAP
 *   METRICS <design-width> <design-height> <design-advance>
 *   GLYPH <codepoint> <row-0> ... <row-(design-height-1)>
 *   END
 *
 * or:
 *
 *   TLFNT1 VECTOR_STROKE
 *   METRICS <design-width> <design-height> <design-advance>
 *   SEGMENT <id> <x1> <y1> <x2> <y2>
 *   GLYPH <codepoint> <segment-mask>
 *   END
 *
 * A '?' glyph is mandatory.  Missing codepoints fall back to it.  Duplicate
 * codepoints/segment IDs, unknown records, undefined mask bits and capacity
 * overruns make the whole file invalid.
 */
UiFontStatus buffer_font_load(font_type_t *font, UiFontStorage *storage);

/**
 * Invalidate a path-loaded font, release owned platform resources and clear
 * its matching caller-owned storage.  A mismatched or repeated unload is a
 * no-op.
 */
void buffer_font_unload(font_type_t *font, UiFontStorage *storage);

bool buffer_font_renderer_init(text_renderer_t *renderer,
                               const font_type_t *font);

UiFontStatus buffer_font_measure(const UiTextRenderContext *context,
                                 UiTextMetrics *metrics);

UiFontStatus buffer_font_render(UiBuffer *buffer,
                                const UiTextRenderContext *context,
                                UiTextMetrics *metrics);

/** UiDrawCallback-compatible entry point. */
void buffer_font_draw(UiBuffer *buffer, void *context);

#endif
