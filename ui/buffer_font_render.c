#include "buffer_font_render.h"
#include "buffer_font_win32.h"

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BITMAP_DESIGN_WIDTH 5
#define FONT_DESIGN_HEIGHT 7
#define FONT_DESIGN_ADVANCE 6
#define VECTOR_DESIGN_WIDTH 1000
#define VECTOR_DESIGN_HEIGHT 1400
#define VECTOR_SAMPLE_GRID 4
#define FONT_FORMAT_COOKIE UINT32_C(0x544c4631)
#define FONT_STORAGE_COOKIE UINT32_C(0x544c4653)
#define FONT_LINE_CAPACITY 768u

typedef struct {
    uint16_t rows[FONT_DESIGN_HEIGHT];
} BitmapGlyph;

/* '?', '0'..'9', then 'A'..'Z'.  Only the low five bits are used. */
static const BitmapGlyph bitmap_glyphs[] = {
    {{0x0eu, 0x11u, 0x01u, 0x02u, 0x04u, 0x00u, 0x04u}},
    {{0x0eu, 0x11u, 0x13u, 0x15u, 0x19u, 0x11u, 0x0eu}},
    {{0x04u, 0x0cu, 0x04u, 0x04u, 0x04u, 0x04u, 0x0eu}},
    {{0x0eu, 0x11u, 0x01u, 0x02u, 0x04u, 0x08u, 0x1fu}},
    {{0x1eu, 0x01u, 0x01u, 0x0eu, 0x01u, 0x01u, 0x1eu}},
    {{0x02u, 0x06u, 0x0au, 0x12u, 0x1fu, 0x02u, 0x02u}},
    {{0x1fu, 0x10u, 0x10u, 0x1eu, 0x01u, 0x01u, 0x1eu}},
    {{0x0eu, 0x10u, 0x10u, 0x1eu, 0x11u, 0x11u, 0x0eu}},
    {{0x1fu, 0x01u, 0x02u, 0x04u, 0x08u, 0x08u, 0x08u}},
    {{0x0eu, 0x11u, 0x11u, 0x0eu, 0x11u, 0x11u, 0x0eu}},
    {{0x0eu, 0x11u, 0x11u, 0x0fu, 0x01u, 0x01u, 0x0eu}},
    {{0x0eu, 0x11u, 0x11u, 0x1fu, 0x11u, 0x11u, 0x11u}},
    {{0x1eu, 0x11u, 0x11u, 0x1eu, 0x11u, 0x11u, 0x1eu}},
    {{0x0eu, 0x11u, 0x10u, 0x10u, 0x10u, 0x11u, 0x0eu}},
    {{0x1eu, 0x11u, 0x11u, 0x11u, 0x11u, 0x11u, 0x1eu}},
    {{0x1fu, 0x10u, 0x10u, 0x1eu, 0x10u, 0x10u, 0x1fu}},
    {{0x1fu, 0x10u, 0x10u, 0x1eu, 0x10u, 0x10u, 0x10u}},
    {{0x0eu, 0x11u, 0x10u, 0x17u, 0x11u, 0x11u, 0x0fu}},
    {{0x11u, 0x11u, 0x11u, 0x1fu, 0x11u, 0x11u, 0x11u}},
    {{0x0eu, 0x04u, 0x04u, 0x04u, 0x04u, 0x04u, 0x0eu}},
    {{0x01u, 0x01u, 0x01u, 0x01u, 0x11u, 0x11u, 0x0eu}},
    {{0x11u, 0x12u, 0x14u, 0x18u, 0x14u, 0x12u, 0x11u}},
    {{0x10u, 0x10u, 0x10u, 0x10u, 0x10u, 0x10u, 0x1fu}},
    {{0x11u, 0x1bu, 0x15u, 0x15u, 0x11u, 0x11u, 0x11u}},
    {{0x11u, 0x19u, 0x15u, 0x13u, 0x11u, 0x11u, 0x11u}},
    {{0x0eu, 0x11u, 0x11u, 0x11u, 0x11u, 0x11u, 0x0eu}},
    {{0x1eu, 0x11u, 0x11u, 0x1eu, 0x10u, 0x10u, 0x10u}},
    {{0x0eu, 0x11u, 0x11u, 0x11u, 0x15u, 0x12u, 0x0du}},
    {{0x1eu, 0x11u, 0x11u, 0x1eu, 0x14u, 0x12u, 0x11u}},
    {{0x0fu, 0x10u, 0x10u, 0x0eu, 0x01u, 0x01u, 0x1eu}},
    {{0x1fu, 0x04u, 0x04u, 0x04u, 0x04u, 0x04u, 0x04u}},
    {{0x11u, 0x11u, 0x11u, 0x11u, 0x11u, 0x11u, 0x0eu}},
    {{0x11u, 0x11u, 0x11u, 0x11u, 0x11u, 0x0au, 0x04u}},
    {{0x11u, 0x11u, 0x11u, 0x15u, 0x15u, 0x15u, 0x0au}},
    {{0x11u, 0x11u, 0x0au, 0x04u, 0x0au, 0x11u, 0x11u}},
    {{0x11u, 0x11u, 0x0au, 0x04u, 0x04u, 0x04u, 0x04u}},
    {{0x1fu, 0x01u, 0x02u, 0x04u, 0x08u, 0x10u, 0x1fu}}
};

enum {
    SEG_TOP_LEFT = 0,
    SEG_TOP_RIGHT,
    SEG_UPPER_LEFT,
    SEG_UPPER_RIGHT,
    SEG_MIDDLE_LEFT,
    SEG_MIDDLE_RIGHT,
    SEG_LOWER_LEFT,
    SEG_LOWER_RIGHT,
    SEG_BOTTOM_LEFT,
    SEG_BOTTOM_RIGHT,
    SEG_DIAGONAL_TOP_LEFT,
    SEG_DIAGONAL_TOP_RIGHT,
    SEG_DIAGONAL_BOTTOM_LEFT,
    SEG_DIAGONAL_BOTTOM_RIGHT,
    SEG_CENTER_TOP,
    SEG_CENTER_BOTTOM,
    SEG_DOT,
    SEGMENT_COUNT
};

#define SEGMENT_BIT(segment) (UINT32_C(1) << (segment))
#define S_TL SEGMENT_BIT(SEG_TOP_LEFT)
#define S_TR SEGMENT_BIT(SEG_TOP_RIGHT)
#define S_UL SEGMENT_BIT(SEG_UPPER_LEFT)
#define S_UR SEGMENT_BIT(SEG_UPPER_RIGHT)
#define S_ML SEGMENT_BIT(SEG_MIDDLE_LEFT)
#define S_MR SEGMENT_BIT(SEG_MIDDLE_RIGHT)
#define S_LL SEGMENT_BIT(SEG_LOWER_LEFT)
#define S_LR SEGMENT_BIT(SEG_LOWER_RIGHT)
#define S_BL SEGMENT_BIT(SEG_BOTTOM_LEFT)
#define S_BR SEGMENT_BIT(SEG_BOTTOM_RIGHT)
#define S_DTL SEGMENT_BIT(SEG_DIAGONAL_TOP_LEFT)
#define S_DTR SEGMENT_BIT(SEG_DIAGONAL_TOP_RIGHT)
#define S_DBL SEGMENT_BIT(SEG_DIAGONAL_BOTTOM_LEFT)
#define S_DBR SEGMENT_BIT(SEG_DIAGONAL_BOTTOM_RIGHT)
#define S_CT SEGMENT_BIT(SEG_CENTER_TOP)
#define S_CB SEGMENT_BIT(SEG_CENTER_BOTTOM)
#define S_DOT SEGMENT_BIT(SEG_DOT)

static const UiFontStrokeSegment stroke_segments[SEGMENT_COUNT] = {
    {100, 100, 500, 100},
    {500, 100, 900, 100},
    {100, 100, 100, 700},
    {900, 100, 900, 700},
    {100, 700, 500, 700},
    {500, 700, 900, 700},
    {100, 700, 100, 1300},
    {900, 700, 900, 1300},
    {100, 1300, 500, 1300},
    {500, 1300, 900, 1300},
    {100, 100, 500, 700},
    {900, 100, 500, 700},
    {100, 1300, 500, 700},
    {900, 1300, 500, 700},
    {500, 100, 500, 700},
    {500, 700, 500, 1100},
    {500, 1260, 500, 1300}
};

const font_type_t ui_font_bitmap_5x7 = {
    .name = "builtin-bitmap-5x7",
    .id = UINT32_C(0x00050007),
    .engine = UI_FONT_ENGINE_BITMAP,
    .design_width = BITMAP_DESIGN_WIDTH,
    .design_height = FONT_DESIGN_HEIGHT,
    .design_advance = FONT_DESIGN_ADVANCE,
    ._bitmap_rows = &bitmap_glyphs[0].rows[0],
    ._glyph_count = 37u,
    ._format_cookie = FONT_FORMAT_COOKIE,
    ._backend = UI_FONT_BACKEND_BITMAP
};

const font_type_t ui_font_vector_stroke = {
    .name = "builtin-vector-stroke",
    .id = UINT32_C(0x00160001),
    .engine = UI_FONT_ENGINE_VECTOR_STROKE,
    .design_width = VECTOR_DESIGN_WIDTH,
    .design_height = VECTOR_DESIGN_HEIGHT,
    .design_advance = 1200u,
    ._vector_segments = stroke_segments,
    ._glyph_count = 37u,
    ._segment_count = SEGMENT_COUNT,
    ._format_cookie = FONT_FORMAT_COOKIE,
    ._backend = UI_FONT_BACKEND_STROKE
};

static bool font_is_valid(const font_type_t *font)
{
    if (font == NULL || font->_format_cookie != FONT_FORMAT_COOKIE ||
        font->design_width == 0u || font->design_height == 0u ||
        font->design_advance == 0u || font->_glyph_count == 0u) {
        return false;
    }
    if (font->_backend == UI_FONT_BACKEND_BITMAP) {
        if (font == &ui_font_bitmap_5x7) return true;
        if (font->_storage == NULL || font->_storage->_owner != font ||
            font->_storage->_storage_cookie != FONT_STORAGE_COOKIE ||
            font->_codepoints == NULL) {
            return false;
        }
        return font->_bitmap_rows != NULL && font->design_width <= 16u &&
               font->design_height <= UI_FONT_STORAGE_MAX_BITMAP_ROWS &&
               font->engine == UI_FONT_ENGINE_BITMAP;
    }
    if (font->_backend == UI_FONT_BACKEND_STROKE) {
        if (font == &ui_font_vector_stroke) {
            return font->_vector_segments != NULL &&
                   font->_segment_count > 0u &&
                   font->_segment_count <= UI_FONT_STORAGE_MAX_SEGMENTS &&
                   font->engine == UI_FONT_ENGINE_VECTOR_STROKE;
        }
        if (font->_storage == NULL || font->_storage->_owner != font ||
            font->_storage->_storage_cookie != FONT_STORAGE_COOKIE ||
            font->_codepoints == NULL) {
            return false;
        }
        return font->_vector_masks != NULL &&
               font->_vector_segments != NULL && font->_segment_count > 0u &&
               font->_segment_count <= UI_FONT_STORAGE_MAX_SEGMENTS &&
               font->engine == UI_FONT_ENGINE_VECTOR_STROKE;
    }
#if defined(TREELIKE_UI_HAS_TRUETYPE)
    if (font->_backend == UI_FONT_BACKEND_OUTLINE) {
        return font->engine == UI_FONT_ENGINE_VECTOR_OUTLINE &&
               font->_storage != NULL &&
               font->_storage->_platform_data != NULL &&
               font->_storage->_owner == font &&
               font->_storage->_storage_cookie == FONT_STORAGE_COOKIE;
    }
#endif
    return false;
}

static void clear_runtime_font(font_type_t *font)
{
    font->engine = UI_FONT_ENGINE_BITMAP;
    font->design_width = 0u;
    font->design_height = 0u;
    font->design_advance = 0u;
    font->_codepoints = NULL;
    font->_bitmap_rows = NULL;
    font->_vector_masks = NULL;
    font->_vector_segments = NULL;
    font->_glyph_count = 0u;
    font->_segment_count = 0u;
    font->_storage = NULL;
    font->_format_cookie = 0u;
    font->_backend = 0u;
}

static bool path_has_outline_extension(const char *path)
{
    const char *extension;
    char letters[4];

    if (path == NULL) return false;
    extension = strrchr(path, '.');
    if (extension == NULL || strlen(extension) != 4u) return false;
    for (size_t index = 0u; index < 4u; ++index) {
        char value = extension[index];
        letters[index] = value >= 'A' && value <= 'Z'
            ? (char)(value - 'A' + 'a') : value;
    }
    return memcmp(letters, ".ttf", 4u) == 0 ||
           memcmp(letters, ".otf", 4u) == 0 ||
           memcmp(letters, ".ttc", 4u) == 0;
}

static char *next_token(char **cursor)
{
    char *start = *cursor;

    while (*start == ' ' || *start == '\t' || *start == '\r' ||
           *start == '\n') {
        ++start;
    }
    if (*start == '\0' || *start == '#') {
        *cursor = start;
        return NULL;
    }

    *cursor = start;
    while (**cursor != '\0' && **cursor != ' ' && **cursor != '\t' &&
           **cursor != '\r' && **cursor != '\n' && **cursor != '#') {
        ++*cursor;
    }
    if (**cursor != '\0') {
        if (**cursor == '#') {
            **cursor = '\0';
            ++*cursor;
            while (**cursor != '\0') ++*cursor;
        } else {
            **cursor = '\0';
            ++*cursor;
        }
    }
    return start;
}

static bool parse_unsigned_token(const char *token, int base,
                                 unsigned long maximum,
                                 unsigned long *value)
{
    char *end;
    unsigned long parsed;

    if (token == NULL || *token == '\0' || *token == '-') return false;
    errno = 0;
    parsed = strtoul(token, &end, base);
    if (errno == ERANGE || *end != '\0' || parsed > maximum) return false;
    *value = parsed;
    return true;
}

static bool parse_signed_token(const char *token, long minimum, long maximum,
                               long *value)
{
    char *end;
    long parsed;

    if (token == NULL || *token == '\0') return false;
    errno = 0;
    parsed = strtol(token, &end, 10);
    if (errno == ERANGE || *end != '\0' || parsed < minimum ||
        parsed > maximum) {
        return false;
    }
    *value = parsed;
    return true;
}

static bool valid_codepoint(unsigned long codepoint)
{
    return codepoint <= UINT32_C(0x10ffff) &&
           !(codepoint >= UINT32_C(0xd800) &&
             codepoint <= UINT32_C(0xdfff));
}

static bool codepoint_exists(const UiFontStorage *storage,
                             size_t glyph_count, uint32_t codepoint)
{
    for (size_t index = 0u; index < glyph_count; ++index) {
        if (storage->codepoints[index] == codepoint) return true;
    }
    return false;
}

void buffer_font_unload(font_type_t *font, UiFontStorage *storage)
{
    if (font == NULL || storage == NULL || font->_storage != storage ||
        font->_format_cookie != FONT_FORMAT_COOKIE ||
        storage->_owner != font ||
        storage->_storage_cookie != FONT_STORAGE_COOKIE) {
        return;
    }
#if defined(TREELIKE_UI_HAS_TRUETYPE)
    if (font->_backend == UI_FONT_BACKEND_OUTLINE) {
        ui_font_win32_unload(storage);
    }
#endif
    clear_runtime_font(font);
    memset(storage, 0, sizeof(*storage));
}

UiFontStatus buffer_font_load(font_type_t *font, UiFontStorage *storage)
{
    FILE *stream;
    char line[FONT_LINE_CAPACITY];
    bool have_header = false;
    bool have_metrics = false;
    bool have_end = false;
    bool have_fallback = false;
    UiFontEngine engine = UI_FONT_ENGINE_BITMAP;
    unsigned long design_width = 0u;
    unsigned long design_height = 0u;
    unsigned long design_advance = 0u;
    size_t glyph_count = 0u;
    size_t segment_count = 0u;
    const char *path;
    UiFontStatus result = UI_FONT_STATUS_INVALID_DATA;

    if (font == NULL || storage == NULL) {
        return UI_FONT_STATUS_INVALID_ARGUMENT;
    }
    path = font->path;
    if (font->_format_cookie == FONT_FORMAT_COOKIE &&
        font->_storage != NULL) {
        buffer_font_unload(font, (UiFontStorage *)font->_storage);
    } else {
        clear_runtime_font(font);
    }
    if (storage->_storage_cookie == FONT_STORAGE_COOKIE &&
        storage->_owner != NULL) {
        return UI_FONT_STATUS_INVALID_ARGUMENT;
    }
    memset(storage, 0, sizeof(*storage));
    if (path == NULL || *path == '\0') {
        return font->addr != 0u ? UI_FONT_STATUS_UNSUPPORTED_SOURCE
                                : UI_FONT_STATUS_INVALID_ARGUMENT;
    }

    if (path_has_outline_extension(path)) {
#if defined(TREELIKE_UI_HAS_TRUETYPE)
        result = ui_font_win32_load(font, storage);
        if (result != UI_FONT_STATUS_OK) return result;
        font->path = path;
        font->engine = UI_FONT_ENGINE_VECTOR_OUTLINE;
        /* Outline metrics are size-specific and queried from GDI. */
        font->design_width = 1u;
        font->design_height = 1u;
        font->design_advance = 1u;
        font->_glyph_count = 1u;
        font->_storage = storage;
        font->_format_cookie = FONT_FORMAT_COOKIE;
        font->_backend = UI_FONT_BACKEND_OUTLINE;
        storage->_owner = font;
        storage->_storage_cookie = FONT_STORAGE_COOKIE;
        return UI_FONT_STATUS_OK;
#else
        return UI_FONT_STATUS_UNSUPPORTED_SOURCE;
#endif
    }

#if defined(_MSC_VER)
    stream = NULL;
    if (fopen_s(&stream, path, "r") != 0) stream = NULL;
#else
    stream = fopen(path, "r");
#endif
    if (stream == NULL) return UI_FONT_STATUS_IO_ERROR;

    while (fgets(line, (int)sizeof(line), stream) != NULL) {
        size_t length = strlen(line);
        char *scan = line;
        char *command;

        if (length == sizeof(line) - 1u && line[length - 1u] != '\n' &&
            !feof(stream)) {
            result = UI_FONT_STATUS_INVALID_DATA;
            goto finished;
        }
        command = next_token(&scan);
        if (command == NULL) continue;
        if (have_end) {
            result = UI_FONT_STATUS_INVALID_DATA;
            goto finished;
        }

        if (!have_header) {
            char *engine_token;
            if (strcmp(command, "TLFNT1") != 0) goto finished;
            engine_token = next_token(&scan);
            if (engine_token == NULL || next_token(&scan) != NULL) {
                goto finished;
            }
            if (strcmp(engine_token, "BITMAP") == 0) {
                engine = UI_FONT_ENGINE_BITMAP;
            } else if (strcmp(engine_token, "VECTOR_STROKE") == 0) {
                engine = UI_FONT_ENGINE_VECTOR_STROKE;
            } else {
                goto finished;
            }
            have_header = true;
            continue;
        }

        if (strcmp(command, "METRICS") == 0) {
            char *width_token = next_token(&scan);
            char *height_token = next_token(&scan);
            char *advance_token = next_token(&scan);
            unsigned long metric_max = engine == UI_FONT_ENGINE_BITMAP
                ? UI_FONT_STORAGE_MAX_BITMAP_ROWS : (unsigned long)INT16_MAX;

            if (have_metrics || width_token == NULL || height_token == NULL ||
                advance_token == NULL || next_token(&scan) != NULL ||
                !parse_unsigned_token(width_token, 10, metric_max,
                                      &design_width) ||
                !parse_unsigned_token(height_token, 10, metric_max,
                                      &design_height) ||
                !parse_unsigned_token(advance_token, 10, metric_max,
                                      &design_advance) ||
                design_width == 0u || design_height == 0u ||
                design_advance == 0u ||
                (engine == UI_FONT_ENGINE_BITMAP && design_width > 16u)) {
                goto finished;
            }
            have_metrics = true;
            continue;
        }

        if (strcmp(command, "SEGMENT") == 0) {
            char *id_token = next_token(&scan);
            char *x1_token = next_token(&scan);
            char *y1_token = next_token(&scan);
            char *x2_token = next_token(&scan);
            char *y2_token = next_token(&scan);
            unsigned long id;
            long x1;
            long y1;
            long x2;
            long y2;

            if (!have_metrics || engine != UI_FONT_ENGINE_VECTOR_STROKE ||
                id_token == NULL || x1_token == NULL || y1_token == NULL ||
                x2_token == NULL || y2_token == NULL ||
                next_token(&scan) != NULL ||
                !parse_unsigned_token(id_token, 10,
                                      UI_FONT_STORAGE_MAX_SEGMENTS - 1u, &id) ||
                !parse_signed_token(x1_token, INT16_MIN, INT16_MAX, &x1) ||
                !parse_signed_token(y1_token, INT16_MIN, INT16_MAX, &y1) ||
                !parse_signed_token(x2_token, INT16_MIN, INT16_MAX, &x2) ||
                !parse_signed_token(y2_token, INT16_MIN, INT16_MAX, &y2) ||
                storage->vector_segment_defined[id]) {
                goto finished;
            }
            storage->vector_segments[id].x1 = (int16_t)x1;
            storage->vector_segments[id].y1 = (int16_t)y1;
            storage->vector_segments[id].x2 = (int16_t)x2;
            storage->vector_segments[id].y2 = (int16_t)y2;
            storage->vector_segment_defined[id] = true;
            if ((size_t)id + 1u > segment_count) {
                segment_count = (size_t)id + 1u;
            }
            continue;
        }

        if (strcmp(command, "GLYPH") == 0) {
            char *codepoint_token = next_token(&scan);
            unsigned long parsed_codepoint;

            if (!have_metrics || codepoint_token == NULL ||
                glyph_count >= UI_FONT_STORAGE_MAX_GLYPHS ||
                !parse_unsigned_token(codepoint_token, 16,
                                      UINT32_MAX, &parsed_codepoint) ||
                !valid_codepoint(parsed_codepoint) ||
                codepoint_exists(storage, glyph_count,
                                 (uint32_t)parsed_codepoint)) {
                goto finished;
            }

            storage->codepoints[glyph_count] = (uint32_t)parsed_codepoint;
            if (engine == UI_FONT_ENGINE_BITMAP) {
                unsigned long row_limit = design_width == 16u
                    ? UINT16_MAX : ((1ul << design_width) - 1ul);
                for (size_t row = 0u; row < design_height; ++row) {
                    char *row_token = next_token(&scan);
                    unsigned long row_bits;
                    if (row_token == NULL ||
                        !parse_unsigned_token(row_token, 16, row_limit,
                                              &row_bits)) {
                        goto finished;
                    }
                    storage->bitmap_rows[glyph_count * design_height + row] =
                        (uint16_t)row_bits;
                }
            } else {
                char *mask_token = next_token(&scan);
                unsigned long mask;
                if (mask_token == NULL ||
                    !parse_unsigned_token(mask_token, 16, UINT32_MAX, &mask)) {
                    goto finished;
                }
                storage->vector_masks[glyph_count] = (uint32_t)mask;
            }
            if (next_token(&scan) != NULL) goto finished;
            if (parsed_codepoint == (unsigned long)'?') have_fallback = true;
            ++glyph_count;
            continue;
        }

        if (strcmp(command, "END") == 0) {
            if (!have_metrics || next_token(&scan) != NULL) goto finished;
            have_end = true;
            continue;
        }
        goto finished;
    }

    if (ferror(stream)) {
        result = UI_FONT_STATUS_IO_ERROR;
        goto finished;
    }
    if (!have_header || !have_metrics || !have_end || !have_fallback ||
        glyph_count == 0u) {
        goto finished;
    }
    if (engine == UI_FONT_ENGINE_VECTOR_STROKE) {
        uint32_t defined_mask = 0u;
        if (segment_count == 0u) goto finished;
        for (size_t index = 0u; index < segment_count; ++index) {
            if (storage->vector_segment_defined[index]) {
                defined_mask |= UINT32_C(1) << index;
            }
        }
        for (size_t index = 0u; index < glyph_count; ++index) {
            if ((storage->vector_masks[index] & ~defined_mask) != 0u) {
                goto finished;
            }
        }
    }

    font->name = font->name != NULL ? font->name : "path-font";
    font->path = path;
    font->engine = engine;
    font->design_width = (uint16_t)design_width;
    font->design_height = (uint16_t)design_height;
    font->design_advance = (uint16_t)design_advance;
    font->_codepoints = storage->codepoints;
    font->_bitmap_rows = engine == UI_FONT_ENGINE_BITMAP
        ? storage->bitmap_rows : NULL;
    font->_vector_masks = engine == UI_FONT_ENGINE_VECTOR_STROKE
        ? storage->vector_masks : NULL;
    font->_vector_segments = engine == UI_FONT_ENGINE_VECTOR_STROKE
        ? storage->vector_segments : NULL;
    font->_glyph_count = glyph_count;
    font->_segment_count = engine == UI_FONT_ENGINE_VECTOR_STROKE
        ? segment_count : 0u;
    font->_storage = storage;
    font->_format_cookie = FONT_FORMAT_COOKIE;
    font->_backend = engine == UI_FONT_ENGINE_BITMAP
        ? UI_FONT_BACKEND_BITMAP : UI_FONT_BACKEND_STROKE;
    storage->_owner = font;
    storage->_storage_cookie = FONT_STORAGE_COOKIE;
    result = UI_FONT_STATUS_OK;

finished:
    (void)fclose(stream);
    if (result != UI_FONT_STATUS_OK) memset(storage, 0, sizeof(*storage));
    return result;
}

bool buffer_font_renderer_init(text_renderer_t *renderer,
                               const font_type_t *font)
{
    if (renderer == NULL || !font_is_valid(font)) return false;
    renderer->font = font;
    renderer->engine = font->engine;
    return true;
}

static bool renderer_is_valid(const text_renderer_t *renderer)
{
    return renderer != NULL && font_is_valid(renderer->font) &&
           renderer->engine == renderer->font->engine;
}

static uint32_t normalize_codepoint(uint32_t codepoint)
{
    if (codepoint >= (uint32_t)'a' && codepoint <= (uint32_t)'z') {
        return codepoint - (uint32_t)'a' + (uint32_t)'A';
    }
    if (codepoint == (uint32_t)' ' || codepoint == (uint32_t)'?' ||
        (codepoint >= (uint32_t)'0' && codepoint <= (uint32_t)'9') ||
        (codepoint >= (uint32_t)'A' && codepoint <= (uint32_t)'Z')) {
        return codepoint;
    }
    return (uint32_t)'?';
}

static uint32_t decode_utf8(const unsigned char **cursor)
{
    const unsigned char *source = *cursor;
    uint32_t codepoint;

    if (source[0] < 0x80u) {
        *cursor = source + 1;
        return source[0];
    }
    if (source[0] >= 0xc2u && source[0] <= 0xdfu &&
        source[1] >= 0x80u && source[1] <= 0xbfu) {
        codepoint = ((uint32_t)(source[0] & 0x1fu) << 6) |
                    (uint32_t)(source[1] & 0x3fu);
        *cursor = source + 2;
        return codepoint;
    }
    if (source[0] >= 0xe0u && source[0] <= 0xefu && source[1] != 0u &&
        source[2] != 0u && source[1] >= 0x80u && source[1] <= 0xbfu &&
        source[2] >= 0x80u && source[2] <= 0xbfu &&
        !(source[0] == 0xe0u && source[1] < 0xa0u) &&
        !(source[0] == 0xedu && source[1] >= 0xa0u)) {
        codepoint = ((uint32_t)(source[0] & 0x0fu) << 12) |
                    ((uint32_t)(source[1] & 0x3fu) << 6) |
                    (uint32_t)(source[2] & 0x3fu);
        *cursor = source + 3;
        return codepoint;
    }
    if (source[0] >= 0xf0u && source[0] <= 0xf4u && source[1] != 0u &&
        source[2] != 0u && source[3] != 0u &&
        source[1] >= 0x80u && source[1] <= 0xbfu &&
        source[2] >= 0x80u && source[2] <= 0xbfu &&
        source[3] >= 0x80u && source[3] <= 0xbfu &&
        !(source[0] == 0xf0u && source[1] < 0x90u) &&
        !(source[0] == 0xf4u && source[1] >= 0x90u)) {
        codepoint = ((uint32_t)(source[0] & 0x07u) << 18) |
                    ((uint32_t)(source[1] & 0x3fu) << 12) |
                    ((uint32_t)(source[2] & 0x3fu) << 6) |
                    (uint32_t)(source[3] & 0x3fu);
        *cursor = source + 4;
        return codepoint;
    }

    /* Malformed input consumes one byte and follows the normal '?' fallback. */
    *cursor = source + 1;
    return UINT32_C(0xfffd);
}

static int scaled_units(int units, int pixel_height, int design_height)
{
    int64_t numerator = (int64_t)units * pixel_height + design_height / 2;
    return (int)(numerator / design_height);
}

typedef struct {
    int glyph_width;
    int glyph_advance;
    int line_advance;
    int stroke_width;
} RenderGeometry;

static UiFontStatus context_geometry(const UiTextRenderContext *context,
                                     RenderGeometry *geometry)
{
    int advance_with_spacing;

    if (context == NULL || geometry == NULL || context->text == NULL ||
        context->renderer == NULL || context->pixel_height == 0u) {
        return UI_FONT_STATUS_INVALID_ARGUMENT;
    }
    if (!renderer_is_valid(context->renderer)) {
        return UI_FONT_STATUS_INVALID_FONT;
    }

    geometry->glyph_width = scaled_units(context->renderer->font->design_width,
                                         context->pixel_height,
                                         context->renderer->font->design_height);
    geometry->glyph_advance = scaled_units(
        context->renderer->font->design_advance, context->pixel_height,
        context->renderer->font->design_height);
    geometry->line_advance = (int)context->pixel_height +
                             (int)context->line_spacing;
    geometry->stroke_width = context->stroke_width != 0u
        ? (int)context->stroke_width
        : ((int)context->pixel_height + 4) / 8;
    if (geometry->stroke_width < 1) geometry->stroke_width = 1;

    advance_with_spacing = geometry->glyph_advance +
                           (int)context->letter_spacing;
    if (geometry->glyph_width <= 0 || geometry->glyph_advance <= 0 ||
        advance_with_spacing <= 0 || geometry->line_advance <= 0) {
        return UI_FONT_STATUS_INVALID_ARGUMENT;
    }
    return UI_FONT_STATUS_OK;
}

static UiFontStatus measure_with_geometry(const UiTextRenderContext *context,
                                           const RenderGeometry *geometry,
                                           UiTextMetrics *metrics)
{
    const unsigned char *cursor = (const unsigned char *)context->text;
    int64_t pen = 0;
    int64_t maximum_width = 0;
    bool line_has_glyph = false;
    size_t glyph_count = 0u;
    size_t line_count = 1u;

    while (*cursor != 0u) {
        uint32_t codepoint = decode_utf8(&cursor);
        if (codepoint == (uint32_t)'\n') {
            int64_t line_width = line_has_glyph
                ? pen - (int64_t)context->letter_spacing
                : 0;
            if (line_width > maximum_width) maximum_width = line_width;
            if (line_count == SIZE_MAX) return UI_FONT_STATUS_OVERFLOW;
            ++line_count;
            pen = 0;
            line_has_glyph = false;
            continue;
        }

        if (glyph_count == SIZE_MAX) return UI_FONT_STATUS_OVERFLOW;
        ++glyph_count;
        pen += (int64_t)geometry->glyph_advance +
               (int64_t)context->letter_spacing;
        line_has_glyph = true;
        if (pen > (int64_t)INT_MAX + INT16_MAX || pen < 0) {
            return UI_FONT_STATUS_OVERFLOW;
        }
    }

    if (line_has_glyph) pen -= (int64_t)context->letter_spacing;
    else pen = 0;
    if (pen > maximum_width) maximum_width = pen;
    if (maximum_width > INT_MAX) return UI_FONT_STATUS_OVERFLOW;
    if (line_count - 1u >
        (size_t)(INT_MAX - (int)context->pixel_height) /
            (size_t)geometry->line_advance) {
        return UI_FONT_STATUS_OVERFLOW;
    }

    metrics->width = (int)maximum_width;
    metrics->height = (int)context->pixel_height +
        (int)(line_count - 1u) * geometry->line_advance;
    metrics->baseline = (int)context->pixel_height - 1;
    metrics->glyph_count = glyph_count;
    metrics->line_count = line_count;
    return UI_FONT_STATUS_OK;
}

#if defined(TREELIKE_UI_HAS_TRUETYPE)
static UiFontStatus measure_outline_session(
    const UiTextRenderContext *context,
    UiFontWin32Session *session,
    UiTextMetrics *metrics)
{
    const unsigned char *cursor = (const unsigned char *)context->text;
    const int line_height = ui_font_win32_line_height(session);
    const int ascent = ui_font_win32_ascent(session);
    const int64_t line_advance = (int64_t)line_height +
                                 (int64_t)context->line_spacing;
    int64_t pen = 0;
    int64_t maximum_width = 0;
    bool line_has_glyph = false;
    size_t glyph_count = 0u;
    size_t line_count = 1u;

    if (metrics == NULL || line_height <= 0 || ascent <= 0 ||
        ascent > line_height || line_advance <= 0 ||
        line_advance > INT_MAX) {
        return UI_FONT_STATUS_INVALID_ARGUMENT;
    }
    while (*cursor != 0u) {
        UiFontWin32Glyph glyph;
        UiFontStatus status;
        int64_t step;
        uint32_t codepoint = decode_utf8(&cursor);

        if (codepoint == (uint32_t)'\n') {
            int64_t line_width = line_has_glyph
                ? pen - (int64_t)context->letter_spacing : 0;
            if (line_width > maximum_width) maximum_width = line_width;
            if (line_count == SIZE_MAX) return UI_FONT_STATUS_OVERFLOW;
            ++line_count;
            pen = 0;
            line_has_glyph = false;
            continue;
        }
        status = ui_font_win32_glyph(session, codepoint, false, &glyph);
        if (status != UI_FONT_STATUS_OK) return status;
        step = (int64_t)glyph.advance + (int64_t)context->letter_spacing;
        if (step <= 0) return UI_FONT_STATUS_INVALID_ARGUMENT;
        if (glyph_count == SIZE_MAX) return UI_FONT_STATUS_OVERFLOW;
        ++glyph_count;
        if (pen > INT_MAX - step) return UI_FONT_STATUS_OVERFLOW;
        pen += step;
        line_has_glyph = true;
    }
    if (line_has_glyph) pen -= (int64_t)context->letter_spacing;
    else pen = 0;
    if (pen > maximum_width) maximum_width = pen;
    if (maximum_width < 0 || maximum_width > INT_MAX) {
        return UI_FONT_STATUS_OVERFLOW;
    }
    if (line_count - 1u >
        (size_t)(INT_MAX - line_height) / (size_t)line_advance) {
        return UI_FONT_STATUS_OVERFLOW;
    }
    metrics->width = (int)maximum_width;
    metrics->height = line_height +
        (int)(line_count - 1u) * (int)line_advance;
    metrics->baseline = ascent;
    metrics->glyph_count = glyph_count;
    metrics->line_count = line_count;
    return UI_FONT_STATUS_OK;
}

static UiFontStatus measure_outline(const UiTextRenderContext *context,
                                    UiTextMetrics *metrics)
{
    UiFontWin32Session *session = NULL;
    UiFontStatus status;

    if (context == NULL || metrics == NULL || context->text == NULL ||
        context->renderer == NULL || context->pixel_height == 0u) {
        return UI_FONT_STATUS_INVALID_ARGUMENT;
    }
    if (!renderer_is_valid(context->renderer)) {
        return UI_FONT_STATUS_INVALID_FONT;
    }
    status = ui_font_win32_session_begin(context->renderer->font,
                                         context->pixel_height, &session);
    if (status == UI_FONT_STATUS_OK) {
        status = measure_outline_session(context, session, metrics);
    }
    ui_font_win32_session_end(session);
    return status;
}
#endif

UiFontStatus buffer_font_measure(const UiTextRenderContext *context,
                                 UiTextMetrics *metrics)
{
    RenderGeometry geometry;
    UiFontStatus status;

    if (metrics == NULL) return UI_FONT_STATUS_INVALID_ARGUMENT;
#if defined(TREELIKE_UI_HAS_TRUETYPE)
    if (context != NULL && context->renderer != NULL &&
        context->renderer->font != NULL &&
        context->renderer->font->_backend == UI_FONT_BACKEND_OUTLINE) {
        return measure_outline(context, metrics);
    }
#endif
    status = context_geometry(context, &geometry);
    if (status != UI_FONT_STATUS_OK) return status;
    return measure_with_geometry(context, &geometry, metrics);
}

static size_t builtin_glyph_index(uint32_t codepoint)
{
    codepoint = normalize_codepoint(codepoint);
    if (codepoint == (uint32_t)' ') return SIZE_MAX;
    if (codepoint == (uint32_t)'?') return 0u;
    if (codepoint >= (uint32_t)'0' && codepoint <= (uint32_t)'9') {
        return 1u + (size_t)(codepoint - (uint32_t)'0');
    }
    return 11u + (size_t)(codepoint - (uint32_t)'A');
}

static size_t loaded_glyph_index(const font_type_t *font, uint32_t codepoint)
{
    uint32_t alternate = codepoint;
    size_t fallback = SIZE_MAX;

    for (size_t index = 0u; index < font->_glyph_count; ++index) {
        if (font->_codepoints[index] == codepoint) return index;
        if (font->_codepoints[index] == (uint32_t)'?') fallback = index;
    }
    if (codepoint >= (uint32_t)'a' && codepoint <= (uint32_t)'z') {
        alternate = codepoint - (uint32_t)'a' + (uint32_t)'A';
        for (size_t index = 0u; index < font->_glyph_count; ++index) {
            if (font->_codepoints[index] == alternate) return index;
        }
    }
    return fallback;
}

static const uint16_t *bitmap_glyph(const font_type_t *font,
                                    uint32_t codepoint)
{
    size_t index;

    if (codepoint == (uint32_t)' ') return NULL;
    if (font == &ui_font_bitmap_5x7) {
        index = builtin_glyph_index(codepoint);
        return index == SIZE_MAX ? NULL : bitmap_glyphs[index].rows;
    }
    index = loaded_glyph_index(font, codepoint);
    return index == SIZE_MAX ? NULL
        : font->_bitmap_rows + index * font->design_height;
}

static uint32_t builtin_vector_glyph_mask(uint32_t codepoint)
{
    codepoint = normalize_codepoint(codepoint);
    switch (codepoint) {
    case ' ': return 0u;
    case '?': return S_TL | S_TR | S_UR | S_MR | S_CB | S_DOT;
    case '0': return S_TL | S_TR | S_UL | S_UR | S_LL | S_LR | S_BL | S_BR;
    case '1': return S_UR | S_LR;
    case '2': return S_TL | S_TR | S_UR | S_ML | S_MR | S_LL | S_BL | S_BR;
    case '3': return S_TL | S_TR | S_UR | S_ML | S_MR | S_LR | S_BL | S_BR;
    case '4': return S_UL | S_UR | S_ML | S_MR | S_LR;
    case '5': return S_TL | S_TR | S_UL | S_ML | S_MR | S_LR | S_BL | S_BR;
    case '6': return S_TL | S_TR | S_UL | S_ML | S_MR | S_LL | S_LR | S_BL | S_BR;
    case '7': return S_TL | S_TR | S_UR | S_LR;
    case '8': return S_TL | S_TR | S_UL | S_UR | S_ML | S_MR |
                     S_LL | S_LR | S_BL | S_BR;
    case '9': return S_TL | S_TR | S_UL | S_UR | S_ML | S_MR | S_LR | S_BL | S_BR;
    case 'A': return S_TL | S_TR | S_UL | S_UR | S_ML | S_MR | S_LL | S_LR;
    case 'B': return S_TL | S_TR | S_UL | S_ML | S_MR | S_LL | S_LR | S_BL | S_BR;
    case 'C': return S_TL | S_TR | S_UL | S_LL | S_BL | S_BR;
    case 'D': return S_TL | S_TR | S_UL | S_UR | S_LL | S_LR | S_BL | S_BR;
    case 'E': return S_TL | S_TR | S_UL | S_ML | S_MR | S_LL | S_BL | S_BR;
    case 'F': return S_TL | S_TR | S_UL | S_ML | S_MR | S_LL;
    case 'G': return S_TL | S_TR | S_UL | S_LL | S_MR | S_LR | S_BL | S_BR;
    case 'H': return S_UL | S_UR | S_ML | S_MR | S_LL | S_LR;
    case 'I': return S_TL | S_TR | S_CT | S_CB | S_BL | S_BR;
    case 'J': return S_UR | S_LR | S_LL | S_BL | S_BR;
    case 'K': return S_UL | S_LL | S_DTR | S_DBR;
    case 'L': return S_UL | S_LL | S_BL | S_BR;
    case 'M': return S_UL | S_UR | S_LL | S_LR | S_DTL | S_DTR;
    case 'N': return S_UL | S_UR | S_LL | S_LR | S_DTL | S_DBR;
    case 'O': return S_TL | S_TR | S_UL | S_UR | S_LL | S_LR | S_BL | S_BR;
    case 'P': return S_TL | S_TR | S_UL | S_UR | S_ML | S_MR | S_LL;
    case 'Q': return S_TL | S_TR | S_UL | S_UR | S_LL | S_LR | S_BL | S_BR | S_DBR;
    case 'R': return S_TL | S_TR | S_UL | S_UR | S_ML | S_MR | S_LL | S_DBR;
    case 'S': return S_TL | S_TR | S_UL | S_ML | S_MR | S_LR | S_BL | S_BR;
    case 'T': return S_TL | S_TR | S_CT | S_CB;
    case 'U': return S_UL | S_UR | S_LL | S_LR | S_BL | S_BR;
    case 'V': return S_UL | S_UR | S_DBL | S_DBR;
    case 'W': return S_UL | S_UR | S_LL | S_LR | S_DBL | S_DBR;
    case 'X': return S_DTL | S_DTR | S_DBL | S_DBR;
    case 'Y': return S_DTL | S_DTR | S_CB;
    case 'Z': return S_TL | S_TR | S_DTR | S_DBL | S_BL | S_BR;
    default: return 0u;
    }
}

static uint32_t vector_glyph_mask(const font_type_t *font,
                                  uint32_t codepoint)
{
    size_t index;

    if (codepoint == (uint32_t)' ') return 0u;
    if (font == &ui_font_vector_stroke) {
        return builtin_vector_glyph_mask(codepoint);
    }
    index = loaded_glyph_index(font, codepoint);
    return index == SIZE_MAX ? 0u : font->_vector_masks[index];
}

static pixel_t blend_pixel(pixel_t background, pixel_t foreground,
                           uint8_t alpha)
{
    if (alpha == 0u) return background;
    if (alpha == 255u) return foreground;

#if defined(PIXEL_FORMAT_RGB565)
    {
        uint32_t bg_r = ((uint32_t)background >> 11) & 0x1fu;
        uint32_t bg_g = ((uint32_t)background >> 5) & 0x3fu;
        uint32_t bg_b = (uint32_t)background & 0x1fu;
        uint32_t fg_r = ((uint32_t)foreground >> 11) & 0x1fu;
        uint32_t fg_g = ((uint32_t)foreground >> 5) & 0x3fu;
        uint32_t fg_b = (uint32_t)foreground & 0x1fu;
        uint32_t out_r = (fg_r * alpha + bg_r * (255u - alpha) + 127u) / 255u;
        uint32_t out_g = (fg_g * alpha + bg_g * (255u - alpha) + 127u) / 255u;
        uint32_t out_b = (fg_b * alpha + bg_b * (255u - alpha) + 127u) / 255u;
        return (pixel_t)((out_r << 11) | (out_g << 5) | out_b);
    }
#else
    {
        uint32_t bg_r = ((uint32_t)background >> 16) & 0xffu;
        uint32_t bg_g = ((uint32_t)background >> 8) & 0xffu;
        uint32_t bg_b = (uint32_t)background & 0xffu;
        uint32_t fg_r = ((uint32_t)foreground >> 16) & 0xffu;
        uint32_t fg_g = ((uint32_t)foreground >> 8) & 0xffu;
        uint32_t fg_b = (uint32_t)foreground & 0xffu;
        uint32_t out_r = (fg_r * alpha + bg_r * (255u - alpha) + 127u) / 255u;
        uint32_t out_g = (fg_g * alpha + bg_g * (255u - alpha) + 127u) / 255u;
        uint32_t out_b = (fg_b * alpha + bg_b * (255u - alpha) + 127u) / 255u;
        return (pixel_t)((out_r << 16) | (out_g << 8) | out_b);
    }
#endif
}

static void fill_clipped(UiBuffer *buffer, int64_t x, int64_t y,
                         int width, int height, pixel_t color)
{
    int left = x < 0 ? 0 : (x >= buffer->width ? buffer->width : (int)x);
    int top = y < 0 ? 0 : (y >= buffer->height ? buffer->height : (int)y);
    int64_t right64 = x + (int64_t)width;
    int64_t bottom64 = y + (int64_t)height;
    int right = right64 <= 0 ? 0
        : (right64 >= buffer->width ? buffer->width : (int)right64);
    int bottom = bottom64 <= 0 ? 0
        : (bottom64 >= buffer->height ? buffer->height : (int)bottom64);

    for (int row = top; row < bottom; ++row) {
        pixel_t *destination = buffer->pixels +
            (size_t)row * (size_t)buffer->stride + (size_t)left;
        for (int column = left; column < right; ++column) {
            *destination++ = color;
        }
    }
}

static void draw_bitmap_glyph(UiBuffer *buffer, const uint16_t *glyph_rows,
                              int64_t destination_x, int64_t destination_y,
                              const RenderGeometry *geometry,
                              const font_type_t *font, int pixel_height,
                              pixel_t color)
{
    if (glyph_rows == NULL) return;
    for (int y = 0; y < pixel_height; ++y) {
        int64_t target_y = destination_y + y;
        int source_y;
        if (target_y < 0 || target_y >= buffer->height) continue;
        source_y = y * font->design_height / pixel_height;
        for (int x = 0; x < geometry->glyph_width; ++x) {
            int64_t target_x = destination_x + x;
            int source_x;
            if (target_x < 0 || target_x >= buffer->width) continue;
            source_x = x * font->design_width / geometry->glyph_width;
            if ((glyph_rows[source_y] & (uint16_t)(1u <<
                (font->design_width - 1u - (unsigned)source_x))) != 0u) {
                buffer->pixels[(size_t)target_y * (size_t)buffer->stride +
                               (size_t)target_x] = color;
            }
        }
    }
}

static bool sample_hits_segment(double px, double py,
                                const UiFontStrokeSegment *segment,
                                const font_type_t *font, int glyph_width,
                                int pixel_height,
                                double radius_squared)
{
    double ax = (double)segment->x1 * (double)glyph_width /
                (double)font->design_width;
    double ay = (double)segment->y1 * (double)pixel_height /
                (double)font->design_height;
    double bx = (double)segment->x2 * (double)glyph_width /
                (double)font->design_width;
    double by = (double)segment->y2 * (double)pixel_height /
                (double)font->design_height;
    double vx = bx - ax;
    double vy = by - ay;
    double wx = px - ax;
    double wy = py - ay;
    double length_squared = vx * vx + vy * vy;
    double projection = wx * vx + wy * vy;
    double dx;
    double dy;

    if (projection <= 0.0 || length_squared == 0.0) {
        dx = px - ax;
        dy = py - ay;
    } else if (projection >= length_squared) {
        dx = px - bx;
        dy = py - by;
    } else {
        double ratio = projection / length_squared;
        dx = px - (ax + ratio * vx);
        dy = py - (ay + ratio * vy);
    }
    return dx * dx + dy * dy <= radius_squared;
}

static void draw_vector_glyph(UiBuffer *buffer, uint32_t mask,
                              int64_t destination_x, int64_t destination_y,
                              const RenderGeometry *geometry,
                              const font_type_t *font, int pixel_height,
                              pixel_t color)
{
    double radius = (double)geometry->stroke_width / 2.0;
    double radius_squared = radius * radius;
    const int sample_count = VECTOR_SAMPLE_GRID * VECTOR_SAMPLE_GRID;

    if (mask == 0u) return;
    for (int y = 0; y < pixel_height; ++y) {
        int64_t target_y = destination_y + y;
        if (target_y < 0 || target_y >= buffer->height) continue;
        for (int x = 0; x < geometry->glyph_width; ++x) {
            int64_t target_x = destination_x + x;
            int covered = 0;
            if (target_x < 0 || target_x >= buffer->width) continue;

            for (int sy = 0; sy < VECTOR_SAMPLE_GRID; ++sy) {
                for (int sx = 0; sx < VECTOR_SAMPLE_GRID; ++sx) {
                    double sample_x = (double)x +
                        ((double)sx + 0.5) / VECTOR_SAMPLE_GRID;
                    double sample_y = (double)y +
                        ((double)sy + 0.5) / VECTOR_SAMPLE_GRID;
                    bool hit = false;

                    for (int segment_index = 0;
                         segment_index < (int)font->_segment_count;
                         ++segment_index) {
                        if ((mask & SEGMENT_BIT(segment_index)) != 0u &&
                            sample_hits_segment(sample_x, sample_y,
                                                &font->_vector_segments[segment_index],
                                                font,
                                                geometry->glyph_width,
                                                pixel_height,
                                                radius_squared)) {
                            hit = true;
                            break;
                        }
                    }
                    if (hit) ++covered;
                }
            }

            if (covered != 0) {
                uint8_t alpha = (uint8_t)((covered * 255 +
                    sample_count / 2) / sample_count);
                pixel_t *pixel = buffer->pixels +
                    (size_t)target_y * (size_t)buffer->stride +
                    (size_t)target_x;
                *pixel = blend_pixel(*pixel, color, alpha);
            }
        }
    }
}

#if defined(TREELIKE_UI_HAS_TRUETYPE)
static void draw_outline_glyph(UiBuffer *buffer,
                               const UiFontWin32Glyph *glyph,
                               int64_t pen_x,
                               int64_t line_top,
                               pixel_t color)
{
    if (glyph->coverage == NULL || glyph->width <= 0 ||
        glyph->height <= 0 || glyph->stride < glyph->width) {
        return;
    }
    for (int source_y = 0; source_y < glyph->height; ++source_y) {
        int64_t target_y = line_top + glyph->bitmap_top + source_y;
        if (target_y < 0 || target_y >= buffer->height) continue;
        for (int source_x = 0; source_x < glyph->width; ++source_x) {
            int64_t target_x = pen_x + glyph->bitmap_left + source_x;
            uint8_t coverage;
            uint8_t alpha;
            pixel_t *pixel;

            if (target_x < 0 || target_x >= buffer->width) continue;
            coverage = glyph->coverage[
                (size_t)source_y * (size_t)glyph->stride +
                (size_t)source_x];
            if (coverage == 0u) continue;
            if (coverage > 64u) coverage = 64u;
            alpha = (uint8_t)(((unsigned)coverage * 255u + 32u) / 64u);
            pixel = buffer->pixels +
                (size_t)target_y * (size_t)buffer->stride +
                (size_t)target_x;
            *pixel = blend_pixel(*pixel, color, alpha);
        }
    }
}

static UiFontStatus render_outline(UiBuffer *buffer,
                                   const UiTextRenderContext *context,
                                   UiTextMetrics *metrics)
{
    UiFontWin32Session *session = NULL;
    UiTextMetrics measured;
    UiFontStatus status;
    const unsigned char *cursor;
    int64_t pen_x;
    int64_t line_top;
    int line_advance;

    if (context == NULL || context->text == NULL ||
        context->renderer == NULL || context->pixel_height == 0u) {
        return UI_FONT_STATUS_INVALID_ARGUMENT;
    }
    if (!renderer_is_valid(context->renderer)) {
        return UI_FONT_STATUS_INVALID_FONT;
    }
    if (buffer == NULL || buffer->pixels == NULL || buffer->width <= 0 ||
        buffer->height <= 0 || buffer->stride < buffer->width) {
        return UI_FONT_STATUS_INVALID_ARGUMENT;
    }
    status = ui_font_win32_session_begin(context->renderer->font,
                                         context->pixel_height, &session);
    if (status != UI_FONT_STATUS_OK) return status;
    status = measure_outline_session(context, session, &measured);
    if (status != UI_FONT_STATUS_OK) goto finished;
    if (metrics != NULL) *metrics = measured;

    if (context->opaque && measured.width > 0 && measured.height > 0) {
        fill_clipped(buffer, context->origin_x, context->origin_y,
                     measured.width, measured.height,
                     context->background_color);
    }
    line_advance = ui_font_win32_line_height(session) +
                   (int)context->line_spacing;
    cursor = (const unsigned char *)context->text;
    pen_x = context->origin_x;
    line_top = context->origin_y;
    while (*cursor != 0u) {
        UiFontWin32Glyph glyph;
        uint32_t codepoint = decode_utf8(&cursor);

        if (codepoint == (uint32_t)'\n') {
            pen_x = context->origin_x;
            line_top += line_advance;
            continue;
        }
        status = ui_font_win32_glyph(session, codepoint, true, &glyph);
        if (status != UI_FONT_STATUS_OK) goto finished;
        draw_outline_glyph(buffer, &glyph, pen_x, line_top, context->color);
        pen_x += (int64_t)glyph.advance +
                 (int64_t)context->letter_spacing;
    }

finished:
    ui_font_win32_session_end(session);
    return status;
}
#endif

UiFontStatus buffer_font_render(UiBuffer *buffer,
                                const UiTextRenderContext *context,
                                UiTextMetrics *metrics)
{
    RenderGeometry geometry;
    UiTextMetrics measured;
    UiFontStatus status;
    const unsigned char *cursor;
    int64_t pen_x;
    int64_t pen_y;

#if defined(TREELIKE_UI_HAS_TRUETYPE)
    if (context != NULL && context->renderer != NULL &&
        context->renderer->font != NULL &&
        context->renderer->font->_backend == UI_FONT_BACKEND_OUTLINE) {
        return render_outline(buffer, context, metrics);
    }
#endif
    status = context_geometry(context, &geometry);
    if (status != UI_FONT_STATUS_OK) return status;
    if (buffer == NULL || buffer->pixels == NULL || buffer->width <= 0 ||
        buffer->height <= 0 || buffer->stride < buffer->width) {
        return UI_FONT_STATUS_INVALID_ARGUMENT;
    }
    status = measure_with_geometry(context, &geometry, &measured);
    if (status != UI_FONT_STATUS_OK) return status;
    if (metrics != NULL) *metrics = measured;

    if (context->opaque && measured.width > 0 && measured.height > 0) {
        fill_clipped(buffer, context->origin_x, context->origin_y,
                     measured.width, measured.height,
                     context->background_color);
    }

    cursor = (const unsigned char *)context->text;
    pen_x = context->origin_x;
    pen_y = context->origin_y;
    while (*cursor != 0u) {
        uint32_t codepoint = decode_utf8(&cursor);
        if (codepoint == (uint32_t)'\n') {
            pen_x = context->origin_x;
            pen_y += geometry.line_advance;
            continue;
        }

        if (context->renderer->engine == UI_FONT_ENGINE_BITMAP) {
            draw_bitmap_glyph(buffer,
                              bitmap_glyph(context->renderer->font, codepoint),
                              pen_x, pen_y, &geometry,
                              context->renderer->font, context->pixel_height,
                              context->color);
        } else {
            draw_vector_glyph(buffer,
                              vector_glyph_mask(context->renderer->font,
                                                codepoint),
                              pen_x, pen_y, &geometry,
                              context->renderer->font,
                              context->pixel_height, context->color);
        }
        pen_x += geometry.glyph_advance + context->letter_spacing;
    }
    return UI_FONT_STATUS_OK;
}

void buffer_font_draw(UiBuffer *buffer, void *context)
{
    UiTextRenderContext *render_context = context;
    UiTextMetrics metrics = {0, 0, 0, 0u, 0u};
    UiFontStatus status;

    if (render_context == NULL) return;
    status = buffer_font_render(buffer, render_context, &metrics);
    render_context->last_status = status;
    render_context->last_metrics = metrics;
}
