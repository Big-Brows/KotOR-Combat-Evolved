#pragma once

// Borrowed, unscaled PC cursor bitmap from the same decoder as the OS cursor.
// RGBA rows are top-down; the pointer remains valid until process exit. No OS
// cursor is selected or shown. Returns false when the owned art is unavailable.
extern "C" bool k2shim_cursor_image(unsigned id, const unsigned char** rgba,
                                   int* width, int* height, int* hotX, int* hotY);
extern "C" int k2shim_cursor_visible();
