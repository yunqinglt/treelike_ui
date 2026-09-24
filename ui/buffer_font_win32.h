#ifndef TREELIKE_UI_BUFFER_FONT_WIN32_H
#define TREELIKE_UI_BUFFER_FONT_WIN32_H

#include "buffer_font_render.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Private backend tags stored in font_type_t::_backend. */
#define UI_FONT_BACKEND_BITMAP UINT8_C(1)
#define UI_FONT_BACKEND_STROKE UINT8_C(2)
#define UI_FONT_BACKEND_OUTLINE UINT8_C(3)

#if defined(TREELIKE_UI_HAS_TRUETYPE)

typedef struct UiFontWin32Session UiFontWin32Session;

typedef struct {
    int advance;
    int bitmap_left;
    int bitmap_top;
    int width;
    int height;
    int stride;
    const uint8_t *coverage;
} UiFontWin32Glyph;

UiFontStatus ui_font_win32_load(font_type_t *font, UiFontStorage *storage);
void ui_font_win32_unload(UiFontStorage *storage);

UiFontStatus ui_font_win32_session_begin(const font_type_t *font,
                                         uint16_t pixel_height,
                                         UiFontWin32Session **session);
void ui_font_win32_session_end(UiFontWin32Session *session);

int ui_font_win32_line_height(const UiFontWin32Session *session);
int ui_font_win32_ascent(const UiFontWin32Session *session);

UiFontStatus ui_font_win32_glyph(UiFontWin32Session *session,
                                 uint32_t codepoint,
                                 bool rasterize,
                                 UiFontWin32Glyph *glyph);

#endif

#endif
