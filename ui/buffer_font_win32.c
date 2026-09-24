#include "buffer_font_win32.h"

#if !defined(_WIN32)
#error "buffer_font_win32.c is a Windows-only implementation"
#endif

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

typedef struct {
    wchar_t *path;
    wchar_t face[LF_FACESIZE];
    bool registered;
} UiFontWin32Data;

struct UiFontWin32Session {
    HDC dc;
    HFONT font;
    HGDIOBJ previous_font;
    int line_height;
    int ascent;
    uint8_t *bitmap;
    size_t bitmap_capacity;
};

static UiFontStatus utf8_to_wide_alloc(const char *source, wchar_t **result)
{
    int count;
    wchar_t *converted;

    if (source == NULL || *source == '\0' || result == NULL) {
        return UI_FONT_STATUS_INVALID_ARGUMENT;
    }
    *result = NULL;
    count = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
                                source, -1, NULL, 0);
    if (count <= 0) return UI_FONT_STATUS_INVALID_ARGUMENT;
    if ((size_t)count > SIZE_MAX / sizeof(*converted)) {
        return UI_FONT_STATUS_OVERFLOW;
    }
    converted = malloc((size_t)count * sizeof(*converted));
    if (converted == NULL) return UI_FONT_STATUS_OUT_OF_MEMORY;
    if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, source, -1,
                            converted, count) != count) {
        free(converted);
        return UI_FONT_STATUS_PLATFORM_ERROR;
    }
    *result = converted;
    return UI_FONT_STATUS_OK;
}

static UiFontStatus utf8_to_face(const char *source,
                                 wchar_t destination[LF_FACESIZE])
{
    int count;

    if (source == NULL || *source == '\0') {
        return UI_FONT_STATUS_INVALID_ARGUMENT;
    }
    count = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, source, -1,
                                destination, LF_FACESIZE);
    if (count <= 0) return UI_FONT_STATUS_INVALID_ARGUMENT;
    return UI_FONT_STATUS_OK;
}

static bool face_matches(HDC dc, const wchar_t *expected)
{
    wchar_t actual[LF_FACESIZE];
    int length = GetTextFaceW(dc, LF_FACESIZE, actual);

    return length > 0 && _wcsicmp(actual, expected) == 0;
}

static UiFontStatus create_session(const UiFontWin32Data *data,
                                   uint16_t pixel_height,
                                   UiFontWin32Session **result)
{
    UiFontWin32Session *session;
    TEXTMETRICW metrics;

    if (data == NULL || !data->registered || pixel_height == 0u ||
        result == NULL) {
        return UI_FONT_STATUS_INVALID_ARGUMENT;
    }
    *result = NULL;
    session = calloc(1u, sizeof(*session));
    if (session == NULL) return UI_FONT_STATUS_OUT_OF_MEMORY;

    session->dc = CreateCompatibleDC(NULL);
    if (session->dc == NULL) goto platform_error;
    session->font = CreateFontW(-(int)pixel_height, 0, 0, 0, FW_NORMAL,
                                FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                                OUT_TT_ONLY_PRECIS, CLIP_DEFAULT_PRECIS,
                                ANTIALIASED_QUALITY,
                                DEFAULT_PITCH | FF_DONTCARE, data->face);
    if (session->font == NULL) goto platform_error;
    session->previous_font = SelectObject(session->dc, session->font);
    if (session->previous_font == NULL ||
        session->previous_font == HGDI_ERROR) {
        session->previous_font = NULL;
        goto platform_error;
    }
    if (!face_matches(session->dc, data->face) ||
        !GetTextMetricsW(session->dc, &metrics) || metrics.tmHeight <= 0 ||
        metrics.tmAscent <= 0 || metrics.tmAscent > metrics.tmHeight) {
        goto platform_error;
    }
    session->line_height = metrics.tmHeight;
    session->ascent = metrics.tmAscent;
    *result = session;
    return UI_FONT_STATUS_OK;

platform_error:
    ui_font_win32_session_end(session);
    return UI_FONT_STATUS_PLATFORM_ERROR;
}

UiFontStatus ui_font_win32_load(font_type_t *font, UiFontStorage *storage)
{
    UiFontWin32Data *data;
    UiFontWin32Session *verification = NULL;
    UiFontWin32Glyph fallback;
    UiFontStatus status;
    DWORD attributes;

    if (font == NULL || storage == NULL || font->path == NULL ||
        *font->path == '\0' || font->name == NULL || *font->name == '\0') {
        return UI_FONT_STATUS_INVALID_ARGUMENT;
    }
    data = calloc(1u, sizeof(*data));
    if (data == NULL) return UI_FONT_STATUS_OUT_OF_MEMORY;
    status = utf8_to_wide_alloc(font->path, &data->path);
    if (status != UI_FONT_STATUS_OK) goto failed;
    status = utf8_to_face(font->name, data->face);
    if (status != UI_FONT_STATUS_OK) goto failed;

    attributes = GetFileAttributesW(data->path);
    if (attributes == INVALID_FILE_ATTRIBUTES ||
        (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0u) {
        status = UI_FONT_STATUS_IO_ERROR;
        goto failed;
    }
    if (AddFontResourceExW(data->path, FR_PRIVATE, NULL) == 0) {
        status = UI_FONT_STATUS_INVALID_DATA;
        goto failed;
    }
    data->registered = true;

    status = create_session(data, 32u, &verification);
    if (status != UI_FONT_STATUS_OK) goto failed;
    status = ui_font_win32_glyph(verification, (uint32_t)'?', false,
                                 &fallback);
    if (status != UI_FONT_STATUS_OK || fallback.advance <= 0) {
        status = UI_FONT_STATUS_INVALID_DATA;
        goto failed;
    }
    ui_font_win32_session_end(verification);

    storage->_platform_data = data;
    return UI_FONT_STATUS_OK;

failed:
    ui_font_win32_session_end(verification);
    if (data->registered) {
        (void)RemoveFontResourceExW(data->path, FR_PRIVATE, NULL);
    }
    free(data->path);
    free(data);
    return status;
}

void ui_font_win32_unload(UiFontStorage *storage)
{
    UiFontWin32Data *data;

    if (storage == NULL || storage->_platform_data == NULL) return;
    data = storage->_platform_data;
    if (data->registered) {
        (void)RemoveFontResourceExW(data->path, FR_PRIVATE, NULL);
    }
    free(data->path);
    free(data);
    storage->_platform_data = NULL;
}

UiFontStatus ui_font_win32_session_begin(const font_type_t *font,
                                         uint16_t pixel_height,
                                         UiFontWin32Session **session)
{
    const UiFontWin32Data *data;

    if (font == NULL || font->_storage == NULL ||
        font->_storage->_platform_data == NULL ||
        font->_backend != UI_FONT_BACKEND_OUTLINE) {
        return UI_FONT_STATUS_INVALID_FONT;
    }
    data = font->_storage->_platform_data;
    return create_session(data, pixel_height, session);
}

void ui_font_win32_session_end(UiFontWin32Session *session)
{
    if (session == NULL) return;
    if (session->dc != NULL && session->previous_font != NULL) {
        (void)SelectObject(session->dc, session->previous_font);
    }
    if (session->font != NULL) (void)DeleteObject(session->font);
    if (session->dc != NULL) (void)DeleteDC(session->dc);
    free(session->bitmap);
    free(session);
}

int ui_font_win32_line_height(const UiFontWin32Session *session)
{
    return session != NULL ? session->line_height : 0;
}

int ui_font_win32_ascent(const UiFontWin32Session *session)
{
    return session != NULL ? session->ascent : 0;
}

static bool glyph_index_for_codepoint(HDC dc, uint32_t codepoint,
                                      WORD *glyph_index)
{
    wchar_t character;
    DWORD result;

    if (codepoint > UINT32_C(0xffff) ||
        (codepoint >= UINT32_C(0xd800) &&
         codepoint <= UINT32_C(0xdfff))) {
        return false;
    }
    character = (wchar_t)codepoint;
    result = GetGlyphIndicesW(dc, &character, 1, glyph_index,
                              GGI_MARK_NONEXISTING_GLYPHS);
    return result != GDI_ERROR && *glyph_index != UINT16_MAX;
}

UiFontStatus ui_font_win32_glyph(UiFontWin32Session *session,
                                 uint32_t codepoint,
                                 bool rasterize,
                                 UiFontWin32Glyph *glyph)
{
    static const MAT2 identity = {
        {0, 1}, {0, 0}, {0, 0}, {0, 1}
    };
    GLYPHMETRICS metrics;
    WORD glyph_index;
    UINT format;
    DWORD required;
    size_t stride;
    size_t expected;

    if (session == NULL || glyph == NULL) {
        return UI_FONT_STATUS_INVALID_ARGUMENT;
    }
    memset(glyph, 0, sizeof(*glyph));
    if (!glyph_index_for_codepoint(session->dc, codepoint, &glyph_index) &&
        !glyph_index_for_codepoint(session->dc, (uint32_t)'?',
                                   &glyph_index)) {
        return UI_FONT_STATUS_INVALID_DATA;
    }

    format = GGO_GLYPH_INDEX | (rasterize ? GGO_GRAY8_BITMAP : GGO_METRICS);
    required = GetGlyphOutlineW(session->dc, glyph_index, format, &metrics,
                                0u, NULL, &identity);
    if (required == GDI_ERROR) return UI_FONT_STATUS_PLATFORM_ERROR;
    if (metrics.gmBlackBoxX > (UINT)INT_MAX ||
        metrics.gmBlackBoxY > (UINT)INT_MAX) {
        return UI_FONT_STATUS_OVERFLOW;
    }
    glyph->advance = (int)metrics.gmCellIncX;
    glyph->bitmap_left = metrics.gmptGlyphOrigin.x;
    glyph->bitmap_top = session->ascent - metrics.gmptGlyphOrigin.y;
    glyph->width = (int)metrics.gmBlackBoxX;
    glyph->height = (int)metrics.gmBlackBoxY;

    if (!rasterize || required == 0u || glyph->width == 0 ||
        glyph->height == 0) {
        return UI_FONT_STATUS_OK;
    }
    stride = ((size_t)glyph->width + 3u) & ~(size_t)3u;
    if ((size_t)glyph->height > SIZE_MAX / stride) {
        return UI_FONT_STATUS_OVERFLOW;
    }
    expected = stride * (size_t)glyph->height;
    if (expected > (size_t)required || stride > (size_t)INT_MAX) {
        return UI_FONT_STATUS_INVALID_DATA;
    }
    if (session->bitmap_capacity < (size_t)required) {
        uint8_t *replacement = realloc(session->bitmap, (size_t)required);
        if (replacement == NULL) return UI_FONT_STATUS_OUT_OF_MEMORY;
        session->bitmap = replacement;
        session->bitmap_capacity = (size_t)required;
    }
    if (GetGlyphOutlineW(session->dc, glyph_index, format, &metrics, required,
                         session->bitmap, &identity) == GDI_ERROR) {
        return UI_FONT_STATUS_PLATFORM_ERROR;
    }
    glyph->stride = (int)stride;
    glyph->coverage = session->bitmap;
    return UI_FONT_STATUS_OK;
}
