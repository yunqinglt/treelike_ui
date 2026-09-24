# Experimental UI model

This directory preserves the direction of the former `ui_typedef.h` and
`new_extract/` drafts without mixing them into the working renderer.

- `ui_object.h` is a platform-neutral scene/event/animation data model.
- `block_pool.*` is a fixed-size framebuffer-block pool intended for MCUs.
- `buffer_font_render.h` is a compatibility include for the promoted public
  `ui/buffer_font_render.h` API.

The experimental object and block-pool drafts are intentionally not part of
the CMake target yet. Their ownership, allocation-failure policy, event
routing and animation scheduler still need a single design before they should
become public runtime APIs. The font implementation itself now lives in the
working target; the forwarding header remains outside it. None of these
headers includes SDL.
