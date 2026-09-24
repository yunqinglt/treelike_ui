#include "ui_drawer.h"

#include <stdlib.h>

static void append_child(UiBuffer *parent, UiBuffer *child)
{
    UiBuffer **slot = &parent->first_child;
    while (*slot != NULL) slot = &(*slot)->next_sibling;
    *slot = child;
}

UiRect ui_buffer_absolute_bounds(const UiBuffer *buffer)
{
    UiRect result = {0, 0, 0, 0};
    const UiBuffer *parent;

    if (buffer == NULL) return result;
    result = (UiRect){buffer->x, buffer->y, buffer->width, buffer->height};
    parent = buffer->parent;

    while (parent != NULL) {
        result.x += parent->x;
        result.y += parent->y;
        parent = parent->parent;
    }
    return result;
}

UiBuffer *ui_buffer_root(UiSurface *surface)
{
    UiBuffer *root;

    if (surface == NULL || surface->pixels == NULL) return NULL;
    root = calloc(1, sizeof(*root));
    if (root == NULL) return NULL;

    root->surface = surface;
    root->width = surface->width;
    root->height = surface->height;
    root->pixels = surface->pixels;
    root->stride = surface->stride;
    return root;
}

UiBuffer *ui_buffer_create(UiBuffer *parent, UiRect bounds)
{
    UiBuffer *buffer;

    if (parent == NULL || bounds.x < 0 || bounds.y < 0 ||
        bounds.w <= 0 || bounds.h <= 0 ||
        bounds.x > parent->width - bounds.w ||
        bounds.y > parent->height - bounds.h) {
        return NULL;
    }

    buffer = calloc(1, sizeof(*buffer));
    if (buffer == NULL) return NULL;

    buffer->surface = parent->surface;
    buffer->parent = parent;
    buffer->x = bounds.x;
    buffer->y = bounds.y;
    buffer->width = bounds.w;
    buffer->height = bounds.h;
    buffer->stride = parent->stride;
    buffer->pixels = parent->pixels +
        (size_t)bounds.y * (size_t)parent->stride + (size_t)bounds.x;
    append_child(parent, buffer);
    return buffer;
}

void ui_buffer_destroy_tree(UiBuffer *buffer)
{
    UiBuffer *child;

    if (buffer == NULL) return;
    child = buffer->first_child;
    while (child != NULL) {
        UiBuffer *next = child->next_sibling;
        ui_buffer_destroy_tree(child);
        child = next;
    }
    free(buffer);
}

void ui_buffer_mark_dirty(UiBuffer *buffer)
{
    UiBuffer *ancestor;

    if (buffer == NULL) return;
    buffer->is_dirty = true;
    ancestor = buffer->parent;
    while (ancestor != NULL) {
        ancestor->child_dirty = true;
        ancestor = ancestor->parent;
    }
}

static void render_node(UiBuffer *buffer)
{
    if (buffer == NULL || (!buffer->is_dirty && !buffer->child_dirty)) return;

    if (buffer->is_dirty) {
        if (buffer->draw != NULL) buffer->draw(buffer, buffer->context);
        ui_surface_mark_dirty(buffer->surface,
                              ui_buffer_absolute_bounds(buffer));
        buffer->is_dirty = false;
    }

    for (UiBuffer *child = buffer->first_child;
         child != NULL;
         child = child->next_sibling) {
        render_node(child);
    }
    buffer->child_dirty = false;
}

void ui_buffer_render(UiBuffer *root)
{
    render_node(root);
}
