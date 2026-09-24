/** @file ui_drawer.h Platform-independent buffered UI tree. */

#ifndef SDL_PLAYER_UI_DRAWER_H
#define SDL_PLAYER_UI_DRAWER_H

#include "ui_surface.h"

typedef struct UiBuffer UiBuffer;
typedef void (*UiDrawCallback)(UiBuffer *buffer, void *context);

struct UiBuffer {
    UiSurface *surface;
    UiBuffer *parent;
    UiBuffer *first_child;
    UiBuffer *next_sibling;
    int x;
    int y;
    int width;
    int height;
    pixel_t *pixels;
    int stride;
    bool is_dirty;
    bool child_dirty;
    UiDrawCallback draw;
    void *context;
};

UiBuffer *ui_buffer_root(UiSurface *surface);
UiBuffer *ui_buffer_create(UiBuffer *parent, UiRect relative_bounds);
void ui_buffer_destroy_tree(UiBuffer *buffer);
void ui_buffer_mark_dirty(UiBuffer *buffer);
void ui_buffer_render(UiBuffer *root);
UiRect ui_buffer_absolute_bounds(const UiBuffer *buffer);

#endif
