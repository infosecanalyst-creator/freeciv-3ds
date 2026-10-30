// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 the freeciv-3ds authors
#include "font.h"
#include "font_data.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

static C2D_SpriteSheet s_sheet;
static C3D_Tex *s_tex;
static Tex3DS_SubTexture s_sub[FONT_NSIZES][FONT_NGLYPHS];

bool font_init(void)
{
    s_sheet = C2D_SpriteSheetLoad("romfs:/gfx/font.t3x");
    if (!s_sheet) return false;
    C2D_Image im = C2D_SpriteSheetGetImage(s_sheet, 0);
    s_tex = im.tex;
    C3D_TexSetFilter(s_tex, GPU_NEAREST, GPU_NEAREST);   // 1:1 pixels, no blur
    // Map atlas pixels through the sheet's own sub-texture so the UV
    // orientation is whatever tex3ds produced.
    const Tex3DS_SubTexture *all = im.subtex;
    float du = (all->right - all->left) / all->width;
    float dv = (all->bottom - all->top) / all->height;
    for (int s = 0; s < FONT_NSIZES; s++) {
        for (int g = 0; g < FONT_NGLYPHS; g++) {
            const FontGlyph *fg = &k_font_glyphs[s][g];
            Tex3DS_SubTexture *t = &s_sub[s][g];
            t->width = fg->w;
            t->height = fg->h;
            t->left = all->left + fg->x * du;
            t->right = all->left + (fg->x + fg->w) * du;
            t->top = all->top + fg->y * dv;
            t->bottom = all->top + (fg->y + fg->h) * dv;
        }
    }
    return true;
}

void font_exit(void)
{
    if (s_sheet) C2D_SpriteSheetFree(s_sheet);
    s_sheet = NULL;
}

static int s_bias;
void font_set_bias(int steps) { s_bias = steps < 0 ? 0 : steps > 2 ? 2 : steps; }
int  font_get_bias(void) { return s_bias; }

static int size_index(float scale)
{
    int px = (int)(scale * 28.0f + 0.5f);
    int best = 0;
    for (int i = 1; i < FONT_NSIZES; i++) {
        if (abs(k_font_px[i] - px) < abs(k_font_px[best] - px)) best = i;
    }
    // Larger text only for the small sizes (body text), so headings stay put.
    if (s_bias && k_font_px[best] <= 16) {
        best += s_bias;
        while (best > 0 && k_font_px[best] > 16) best--;
    }
    return best;
}

static int glyph_index(unsigned cp)
{
    if (cp >= 32 && cp <= 126) return cp - 32;
    if (cp >= 160 && cp <= 255) return 95 + (cp - 160);
    for (int i = 191; i < FONT_NGLYPHS; i++) if (k_font_cps[i] == cp) return i;
    return '?' - 32;
}

// Next code point from a UTF-8 string (invalid bytes read as Latin-1).
static unsigned next_cp(const unsigned char **p)
{
    const unsigned char *s = *p;
    unsigned c = *s++;
    if (c >= 0xC0 && c < 0xE0 && (s[0] & 0xC0) == 0x80) {
        c = ((c & 0x1F) << 6) | (s[0] & 0x3F); s += 1;
    } else if (c >= 0xE0 && c < 0xF0 && (s[0] & 0xC0) == 0x80 && (s[1] & 0xC0) == 0x80) {
        c = ((c & 0x0F) << 12) | ((s[0] & 0x3F) << 6) | (s[1] & 0x3F); s += 2;
    } else if (c >= 0xF0 && (s[0] & 0xC0) == 0x80 && (s[1] & 0xC0) == 0x80 && (s[2] & 0xC0) == 0x80) {
        c = '?'; s += 3;
    }
    *p = s;
    return c;
}

float font_width(const char *utf8, float scale)
{
    if (!utf8) return 0;
    int si = size_index(scale);
    const unsigned char *p = (const unsigned char *)utf8;
    float w = 0, best = 0;
    while (*p) {
        unsigned cp = next_cp(&p);
        if (cp == '\n') { if (w > best) best = w; w = 0; continue; }
        w += k_font_glyphs[si][glyph_index(cp)].adv;
    }
    return w > best ? w : best;
}

float font_draw(const char *utf8, float x, float y, float z, float scale, u32 color)
{
    if (!utf8 || !s_sheet) return 0;
    int si = size_index(scale);
    // Centre the line in the box the old system font used (~30 px * scale),
    // so existing layout keeps its vertical placement.
    float top = floorf(y + (30.0f * scale - k_font_line[si]) / 2.0f + 0.5f);
    float x0 = floorf(x + 0.5f), pen = x0;
    C2D_ImageTint tint;
    C2D_PlainImageTint(&tint, color, 1.0f);
    const unsigned char *p = (const unsigned char *)utf8;
    float widest = 0;
    while (*p) {
        unsigned cp = next_cp(&p);
        if (cp == '\n') {
            if (pen - x0 > widest) widest = pen - x0;
            pen = x0;
            top += k_font_line[si];
            continue;
        }
        int gi = glyph_index(cp);
        const FontGlyph *g = &k_font_glyphs[si][gi];
        if (g->w) {
            C2D_Image img = { s_tex, &s_sub[si][gi] };
            C2D_DrawImageAt(img, pen + g->xoff, top + g->yoff, z, &tint, 1.0f, 1.0f);
        }
        pen += g->adv;
    }
    return (pen - x0 > widest) ? pen - x0 : widest;
}
