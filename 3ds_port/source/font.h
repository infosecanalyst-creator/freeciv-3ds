#ifndef FONT_H
#define FONT_H

#include <citro2d.h>
#include <stdbool.h>

// Crisp UI text: Inter pre-rendered at exact pixel sizes (tools/make_font.py),
// drawn 1:1 from an atlas instead of scaling the system font.

// Loads romfs:/gfx/font.t3x. Call once after C2D_Prepare().
bool font_init(void);
void font_exit(void);

// `scale` is the old system-font scale (0.5 ~ 14 px) so existing layout code
// keeps working; the nearest baked pixel size is used. (x, y) is the top of
// the line box. Returns the advance width in pixels.
float font_draw(const char *utf8, float x, float y, float z, float scale, u32 color);
float font_width(const char *utf8, float scale);
// Larger text: step every size up by `steps` (0 = normal).
void font_set_bias(int steps);
int  font_get_bias(void);

#endif
