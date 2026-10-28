#include "render.h"
#include "game_loop.h"
#include "fc3ds_server.h"
#include "sprites.h"
#include "pedia_icons.h"
#include "font.h"
#include <citro2d.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <ctype.h>
#include <string.h>

C2D_SpriteSheet g_spriteSheet;
static C2D_TextBuf g_textBuf;

static C2D_TextBuf s_staticTextBuf;
static C2D_Text s_txt_title;
static C2D_Text s_txt_sel_tile_label;
static C2D_Text s_txt_empty;
static C2D_Text s_txt_tap_map_hint;
static C2D_Text s_txt_selected_dpad_move;
static C2D_Text s_txt_selected_actions_hint;
static C2D_Text s_txt_goto_button_hint;
static C2D_Text s_txt_goto_destination_hint;
static C2D_Text s_txt_goto_cancel_hint;
static C2D_Text s_txt_controls_hint_1;
static C2D_Text s_txt_controls_hint_2;
static C2D_Text s_txt_tap_row_hint;
static C2D_Text s_txt_now_queue_hint;
static C2D_Text s_txt_close;
static C2D_Text s_txt_buy;
static C2D_Text s_txt_y_unit_out;
static C2D_Text s_txt_research_title;
static C2D_Text s_txt_tax_science_title;
static C2D_Text s_txt_gov_title;
static C2D_Text s_txt_diplo_title;
static C2D_Text s_txt_accept;
static C2D_Text s_txt_cancel;
static C2D_Text s_txt_choose_player_header;
static C2D_Text s_txt_choose_research_header;
static C2D_Text s_txt_tax_science_rates_header;
static C2D_Text s_txt_choose_government_header;
static C2D_Text s_txt_tech_tree_header;
static C2D_Text s_txt_gov_revolt_hint;
static C2D_Text s_txt_no_clauses_hint;
static C2D_Text s_txt_y_tree_view;
static C2D_Text s_txt_y_flat_list;

typedef struct {
    const char *normalized_name;
    int sprite_idx;
} UnitSpriteMap;

static const UnitSpriteMap g_unit_sprites[] = {
    { "armor", sprites_unit_armor_idx },
    { "howitzer", sprites_unit_howitzer_idx },
    { "battleship", sprites_unit_battleship_idx },
    { "bomber", sprites_unit_bomber_idx },
    { "cannon", sprites_unit_cannon_idx },
    { "caravan", sprites_unit_caravan_idx },
    { "carrier", sprites_unit_carrier_idx },
    { "catapult", sprites_unit_catapult_idx },
    { "horsemen", sprites_unit_horsemen_idx },
    { "chariot", sprites_unit_chariot_idx },
    { "cruiser", sprites_unit_cruiser_idx },
    { "diplomat", sprites_unit_diplomat_idx },
    { "fighter", sprites_unit_fighter_idx },
    { "frigate", sprites_unit_frigate_idx },
    { "ironclad", sprites_unit_ironclad_idx },
    { "knights", sprites_unit_knights_idx },
    { "legion", sprites_unit_legion_idx },
    { "mech_inf", sprites_unit_mech_inf_idx },
    { "warriors", sprites_unit_warriors_idx },
    { "musketeers", sprites_unit_musketeers_idx },
    { "nuclear", sprites_unit_nuclear_idx },
    { "phalanx", sprites_unit_phalanx_idx },
    { "riflemen", sprites_unit_riflemen_idx },
    { "caravel", sprites_unit_caravel_idx },
    { "settlers", sprites_unit_settlers_idx },
    { "submarine", sprites_unit_submarine_idx },
    { "transport", sprites_unit_transport_idx },
    { "trireme", sprites_unit_trireme_idx },
    { "archers", sprites_unit_archers_idx },
    { "cavalry", sprites_unit_cavalry_idx },
    { "cruise_missile", sprites_unit_cruise_missile_idx },
    { "destroyer", sprites_unit_destroyer_idx },
    { "dragoons", sprites_unit_dragoons_idx },
    { "explorer", sprites_unit_explorer_idx },
    { "freight", sprites_unit_freight_idx },
    { "galleon", sprites_unit_galleon_idx },
    { "partisan", sprites_unit_partisan_idx },
    { "pikemen", sprites_unit_pikemen_idx },
    { "storm", sprites_unit_storm_idx },
    { "marines", sprites_unit_marines_idx },
    { "spy", sprites_unit_spy_idx },
    { "engineers", sprites_unit_engineers_idx },
    { "artillery", sprites_unit_artillery_idx },
    { "helicopter", sprites_unit_helicopter_idx },
    { "alpine_troops", sprites_unit_alpine_troops_idx },
    { "stealth_bomber", sprites_unit_stealth_bomber_idx },
    { "stealth_fighter", sprites_unit_stealth_fighter_idx },
    { "aegis_cruiser", sprites_unit_aegis_cruiser_idx },
    { "paratroopers", sprites_unit_paratroopers_idx },
    { "elephants", sprites_unit_elephants_idx },
    { "crusaders", sprites_unit_crusaders_idx },
    { "fanatics", sprites_unit_fanatics_idx },
    { "awacs", sprites_unit_awacs_idx },
    { "worker", sprites_unit_worker_idx },
    { "leader", sprites_unit_leader_idx },
    { "migrants", sprites_unit_migrants_idx },
    { "barbarian_leader", sprites_unit_barbarian_leader_idx },
};
static const int g_num_unit_sprites = 57;

static int get_unit_sprite(const char *rule_name)
{
    if (!rule_name || !rule_name[0]) return sprites_unit_idx;

    char norm[64] = {0};
    int j = 0;
    bool last_was_underscore = false;
    for (int i = 0; rule_name[i] && j < (int)sizeof(norm) - 1; i++) {
        char c = tolower((unsigned char)rule_name[i]);
        if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')) {
            norm[j++] = c;
            last_was_underscore = false;
        } else {
            if (!last_was_underscore && j > 0) {
                norm[j++] = '_';
                last_was_underscore = true;
            }
        }
    }
    if (j > 0 && norm[j-1] == '_') {
        norm[j-1] = '\0';
    }

    for (int i = 0; i < g_num_unit_sprites; i++) {
        if (strcmp(norm, g_unit_sprites[i].normalized_name) == 0) {
            return g_unit_sprites[i].sprite_idx;
        }
    }

    // Fallback: try stripping trailing 's'
    int len = strlen(norm);
    if (len > 0 && norm[len-1] == 's') {
        norm[len-1] = '\0';
        for (int i = 0; i < g_num_unit_sprites; i++) {
            if (strcmp(norm, g_unit_sprites[i].normalized_name) == 0) {
                return g_unit_sprites[i].sprite_idx;
            }
        }
        norm[len-1] = 's'; // restore
    } else if (len < (int)sizeof(norm) - 2) {
        // Fallback: try adding trailing 's'
        norm[len] = 's';
        norm[len+1] = '\0';
        for (int i = 0; i < g_num_unit_sprites; i++) {
            if (strcmp(norm, g_unit_sprites[i].normalized_name) == 0) {
                return g_unit_sprites[i].sprite_idx;
            }
        }
    }
    return sprites_unit_idx; // fallback
}

#define TILE_PX   30   // tile size in pixels (overhead square grid)

void render_init(void)
{
    g_textBuf = C2D_TextBufNew(4096);

    s_staticTextBuf = C2D_TextBufNew(4096);
    C2D_TextParse(&s_txt_title, s_staticTextBuf, "Freeciv 3DS");
    C2D_TextParse(&s_txt_sel_tile_label, s_staticTextBuf, "Selected tile");
    C2D_TextParse(&s_txt_empty, s_staticTextBuf, "Empty");
    C2D_TextParse(&s_txt_tap_map_hint, s_staticTextBuf, "Tap the map to select a tile.");
    C2D_TextParse(&s_txt_selected_dpad_move, s_staticTextBuf, "Selected - D-Pad: move");
    C2D_TextParse(&s_txt_selected_actions_hint, s_staticTextBuf, "B: Fortify/Sentry  X: Action");
    C2D_TextParse(&s_txt_goto_button_hint, s_staticTextBuf, "Y: Disband  GOTO: multi-turn");
    C2D_TextParse(&s_txt_goto_destination_hint, s_staticTextBuf, "Goto: tap the destination tile");
    C2D_TextParse(&s_txt_goto_cancel_hint, s_staticTextBuf, "B, or tap the unit again: cancel");
    C2D_TextParse(&s_txt_controls_hint_1, s_staticTextBuf, "Pan: D-Pad/Circle   SELECT: find units");
    C2D_TextParse(&s_txt_controls_hint_2, s_staticTextBuf, "A: End Turn  L: Research  R: Rates");
    C2D_TextParse(&s_txt_tap_row_hint, s_staticTextBuf, "Tap a row below: change build");
    C2D_TextParse(&s_txt_now_queue_hint, s_staticTextBuf, "NOW (tap left) builds immediately; QUEUE (right) appends.");
    C2D_TextParse(&s_txt_close, s_staticTextBuf, "Close");
    C2D_TextParse(&s_txt_buy, s_staticTextBuf, "Buy");
    C2D_TextParse(&s_txt_y_unit_out, s_staticTextBuf, "Y: unit out");
    C2D_TextParse(&s_txt_research_title, s_staticTextBuf, "Research");
    C2D_TextParse(&s_txt_tax_science_title, s_staticTextBuf, "Tax & Science");
    C2D_TextParse(&s_txt_gov_title, s_staticTextBuf, "Government");
    C2D_TextParse(&s_txt_diplo_title, s_staticTextBuf, "Diplomacy");
    C2D_TextParse(&s_txt_accept, s_staticTextBuf, "Accept");
    C2D_TextParse(&s_txt_cancel, s_staticTextBuf, "Cancel");
    C2D_TextParse(&s_txt_choose_player_header, s_staticTextBuf, "Choose player");
    C2D_TextParse(&s_txt_choose_research_header, s_staticTextBuf, "Choose research");
    C2D_TextParse(&s_txt_tax_science_rates_header, s_staticTextBuf, "Tax / Science Rates");
    C2D_TextParse(&s_txt_choose_government_header, s_staticTextBuf, "Choose government");
    C2D_TextParse(&s_txt_tech_tree_header, s_staticTextBuf, "Tech tree (by tier)");
    C2D_TextParse(&s_txt_gov_revolt_hint, s_staticTextBuf, "Tap a government to revolt");
    C2D_TextParse(&s_txt_no_clauses_hint, s_staticTextBuf, "(no clauses proposed yet)");
    C2D_TextParse(&s_txt_y_tree_view, s_staticTextBuf, "Y: Tree View");
    C2D_TextParse(&s_txt_y_flat_list, s_staticTextBuf, "Y: Flat List");

    C2D_TextOptimize(&s_txt_title);
    C2D_TextOptimize(&s_txt_sel_tile_label);
    C2D_TextOptimize(&s_txt_empty);
    C2D_TextOptimize(&s_txt_tap_map_hint);
    C2D_TextOptimize(&s_txt_selected_dpad_move);
    C2D_TextOptimize(&s_txt_selected_actions_hint);
    C2D_TextOptimize(&s_txt_goto_button_hint);
    C2D_TextOptimize(&s_txt_goto_destination_hint);
    C2D_TextOptimize(&s_txt_goto_cancel_hint);
    C2D_TextOptimize(&s_txt_controls_hint_1);
    C2D_TextOptimize(&s_txt_controls_hint_2);
    C2D_TextOptimize(&s_txt_tap_row_hint);
    C2D_TextOptimize(&s_txt_now_queue_hint);
    C2D_TextOptimize(&s_txt_close);
    C2D_TextOptimize(&s_txt_buy);
    C2D_TextOptimize(&s_txt_y_unit_out);
    C2D_TextOptimize(&s_txt_research_title);
    C2D_TextOptimize(&s_txt_tax_science_title);
    C2D_TextOptimize(&s_txt_gov_title);
    C2D_TextOptimize(&s_txt_diplo_title);
    C2D_TextOptimize(&s_txt_accept);
    C2D_TextOptimize(&s_txt_cancel);
    C2D_TextOptimize(&s_txt_choose_player_header);
    C2D_TextOptimize(&s_txt_choose_research_header);
    C2D_TextOptimize(&s_txt_tax_science_rates_header);
    C2D_TextOptimize(&s_txt_choose_government_header);
    C2D_TextOptimize(&s_txt_tech_tree_header);
    C2D_TextOptimize(&s_txt_gov_revolt_hint);
    C2D_TextOptimize(&s_txt_no_clauses_hint);
    C2D_TextOptimize(&s_txt_y_tree_view);
    C2D_TextOptimize(&s_txt_y_flat_list);
}

// ---- Stereo 3D (top screen) ----
// eye: -1 left, +1 right, 0 mono. A layer at depth d > 0 floats in front of
// the screen by shifting the two eyes' images apart.
static int s_eye;
static float s_stereo;

void render_set_stereo(int eye, float amount)
{
    s_eye = eye;
    s_stereo = amount;
    C2D_ViewReset();
}

// Depth is in "levels" (0 = screen plane, 4 = the selection card). Each eye
// moves by a WHOLE number of pixels: fractional offsets resample the
// point-sampled sprites and font differently per eye, which reads as blur and
// ghosting. At full slider the nearest layer gets 3 px per eye (6 px total
// disparity) - the 3DS stays comfortable well under ~10 px.
static void layer(float depth)
{
    C2D_ViewReset();
    if (!s_eye) return;
    float px = s_stereo * depth * 0.75f;
    if (px > 3.0f) px = 3.0f;
    int ipx = (int)(px + 0.5f);
    if (ipx) C2D_ViewTranslate((float)(-s_eye * ipx), 0);
}

// Black or white, whichever reads better on `bg` (ABGR).
static u32 contrast_text(u32 bg)
{
    int r = bg & 0xFF, g = (bg >> 8) & 0xFF, b = (bg >> 16) & 0xFF;
    return (r * 299 + g * 587 + b * 114) / 1000 > 140 ? C2D_Color32(0x10, 0x10, 0x10, 0xFF) : 0xFFFFFFFF;
}

// Distinct per-player colors (index 0..). -1 -> neutral gray.
static u32 owner_color(int idx)
{
    static const u32 pal[] = {
        0xFFF08040, // blue-ish (ABGR)
        0xFF3030E0, // red
        0xFF40C040, // green
        0xFF30D0F0, // yellow
        0xFFF0D030, // cyan
        0xFFC040C0, // magenta
        0xFF2080F0, // orange
        0xFFF0F0F0, // white
    };
    if (idx < 0) {
        return C2D_Color32(0x90, 0x90, 0x90, 0xFF);
    }
    return pal[idx % (int)(sizeof(pal) / sizeof(pal[0]))];
}

// z defaults to 0.0f via draw_text() below for every panel/HUD caller, which
// never has to compete with anything else - the one exception is
// render_toast(), which overlays the *actively-rendered* UI_MAP map (the
// only place in this file something is drawn on top of, rather than instead
// of, the map's own z~0.49-0.7 terrain/overlay/unit stack) and needs a z
// above that stack or the PICA200 depth test hides it, same failure mode as
// the panel-rect z-order bug this file already fixed once (see
// render_busy_indicator()'s comment).
static void draw_text_z(float x, float y, float z, float scale, u32 color, const char *str)
{
    font_draw(str, x, y, z, scale, color);
}

static void draw_text(float x, float y, float scale, u32 color, const char *str)
{
    draw_text_z(x, y, 0.0f, scale, color, str);
}

// --- Minimap Globals ---
//
// The minimap is drawn as per-tile C2D_DrawRectSolid rects (run-length-encoded per
// row), NOT as a GPU texture. The texture approach was abandoned after a long
// investigation (handoff §§4-11): on real hardware, C2D_DrawImage sampled our
// dynamically-built RGBA8 texture rotated 90° and anisotropically scaled, in a way
// that could not be reliably corrected (Azahar renders it differently again, so it
// was untestable on the emulator). Rects use the exact same coordinate system as the
// camera viewfinder — which always drew correctly — so orientation and alignment are
// correct by construction. s_map_w*s_map_h <= 2904 for a default map, well under
// C2D_DEFAULT_MAX_OBJECTS (4096); run-length encoding keeps the real rect count far
// lower, so 60 FPS is safe.
static u32 *s_minimap_colors = NULL;   // [s_map_w * s_map_h], row-major C2D_Color32
static int s_map_w = 0, s_map_h = 0;
static bool s_minimap_initialized = false;
static int s_current_scan_y = 0;

static inline u32 dim_color(u32 color)
{
    u32 r = (color & 0xFF) >> 1;
    u32 g = ((color >> 8) & 0xFF) >> 1;
    u32 b = ((color >> 16) & 0xFF) >> 1;
    u32 a = (color >> 24) & 0xFF;
    return r | (g << 8) | (b << 16) | (a << 24);
}

static u32 get_minimap_tile_color(const MinimapTileInfo *info)
{
    if (info->visibility == 0) {
        return C2D_Color32(0, 0, 0, 255);
    }

    u32 color = C2D_Color32(0, 0, 0, 255);
    switch (info->terrain) {
    case TERRAIN_OCEAN:
        color = C2D_Color32(0x00, 0x33, 0x66, 0xFF);
        break;
    case TERRAIN_GRASSLAND:
        color = C2D_Color32(0x3B, 0x7A, 0x3B, 0xFF);
        break;
    case TERRAIN_PLAINS:
        color = C2D_Color32(0x8B, 0x9B, 0x4F, 0xFF);
        break;
    case TERRAIN_DESERT:
        color = C2D_Color32(0xD2, 0xB4, 0x8C, 0xFF);
        break;
    case TERRAIN_TUNDRA:
        color = C2D_Color32(0x80, 0x9A, 0x90, 0xFF);
        break;
    case TERRAIN_SWAMP:
        color = C2D_Color32(0x20, 0x40, 0x30, 0xFF);
        break;
    case TERRAIN_JUNGLE:
        color = C2D_Color32(0x1B, 0x4D, 0x1B, 0xFF);
        break;
    case TERRAIN_HILLS:
        color = C2D_Color32(0x5E, 0x6D, 0x3B, 0xFF);
        break;
    case TERRAIN_FOREST:
        color = C2D_Color32(0x22, 0x55, 0x22, 0xFF);
        break;
    case TERRAIN_MOUNTAINS:
        color = C2D_Color32(0x60, 0x60, 0x60, 0xFF);
        break;
    case TERRAIN_ARCTIC:
        color = C2D_Color32(0xE0, 0xF0, 0xFF, 0xFF);
        break;
    default:
        color = C2D_Color32(0, 0, 0, 255);
        break;
    }

    if (info->visibility == 2) {
        if (info->unit_owner >= 0) {
            color = owner_color(info->unit_owner);
        }
        if (info->city_owner >= 0) {
            color = owner_color(info->city_owner);
        }
    } else if (info->visibility == 1) {
        if (info->city_owner >= 0) {
            color = owner_color(info->city_owner);
        }
        color = dim_color(color);
    }

    return color;
}

static void init_minimap_if_needed(void)
{
    if (s_minimap_initialized) return;
    int turn = get_game_turn();
    if (turn <= 0) return;

    // Defer until the Freeciv world-gen is complete – game_map_is_empty() wraps
    // the internal map_is_empty() check (see game_loop.c / game_loop.h).
    // Without this guard, the minimap gets sized at the fallback 40x40 before the
    // server finishes generating the real map, causing a squashed layout on New Game.
    if (game_map_is_empty()) return;

    get_map_dimensions(&s_map_w, &s_map_h);
    if (s_map_w <= 0 || s_map_h <= 0) return;

    s_minimap_colors = (u32*)malloc(s_map_w * s_map_h * sizeof(u32));
    if (!s_minimap_colors) return;

    // Start every tile as opaque "unknown" black so the panel reads as a solid
    // radar screen until the progressive scan fills in real terrain.
    for (int i = 0; i < s_map_w * s_map_h; i++) {
        s_minimap_colors[i] = C2D_Color32(0, 0, 0, 0xFF);
    }

    s_current_scan_y = 0;
    s_minimap_initialized = true;
}

static void update_minimap_slice(int rows_per_frame)
{
    if (get_game_turn() <= 0) {
        if (s_minimap_initialized) render_deinit();
        return;
    }

    if (!s_minimap_initialized) {
        init_minimap_if_needed();
        if (!s_minimap_initialized) return;
    }

    if (fc3ds_server_is_busy()) return;

    int start_y = s_current_scan_y;
    int end_y = start_y + rows_per_frame;
    if (end_y > s_map_h) {
        end_y = s_map_h;
    }

    for (int y = start_y; y < end_y; y++) {
        for (int x = 0; x < s_map_w; x++) {
            MinimapTileInfo info;
            get_minimap_tile_info(x, y, &info);
            s_minimap_colors[y * s_map_w + x] = get_minimap_tile_color(&info);
        }
    }

    s_current_scan_y = end_y;
    if (s_current_scan_y >= s_map_h) {
        s_current_scan_y = 0;
    }
}

void render_deinit(void)
{
    if (s_minimap_initialized) {
        if (s_minimap_colors) {
            free(s_minimap_colors);
            s_minimap_colors = NULL;
        }
        s_minimap_initialized = false;
    }
    if (s_staticTextBuf) {
        C2D_TextBufDelete(s_staticTextBuf);
        s_staticTextBuf = NULL;
    }
}

static void draw_hollow_rect(float x, float y, float w, float h, float thickness, u32 color)
{
    C2D_DrawRectSolid(x, y, 0.5f, w, thickness, color);
    C2D_DrawRectSolid(x, y + h - thickness, 0.5f, w, thickness, color);
    C2D_DrawRectSolid(x, y, 0.5f, thickness, h, color);
    C2D_DrawRectSolid(x + w - thickness, y, 0.5f, thickness, h, color);
}

// ---- Minimap (top screen, UI_MAP) ----
// Fits the whole map uniformly into the MINIMAP_* rect (render.h).
static bool minimap_layout_in(float bx, float by, float bw, float bh, float *dest_x, float *dest_y, float *S)
{
    if (!s_minimap_initialized || s_map_w <= 0 || s_map_h <= 0) return false;
    float sx = bw / s_map_w, sy = bh / s_map_h;
    *S = sx < sy ? sx : sy;
    *dest_x = bx + (bw - s_map_w * *S) / 2.0f;
    *dest_y = by + (bh - s_map_h * *S) / 2.0f;
    return true;
}

static bool minimap_layout(float *dest_x, float *dest_y, float *S)
{
    return minimap_layout_in(MINIMAP_X, MINIMAP_Y, MINIMAP_W, MINIMAP_H, dest_x, dest_y, S);
}

static void draw_minimap_in(float bx, float by, float bw, float bh, int cam_x, int cam_y,
                            float view_w_tiles, float view_h_tiles, int sel_x, int sel_y)
{
    float dest_x, dest_y, S;
    C2D_DrawRectSolid(bx - 2, by - 2, 0.0f, bw + 4, bh + 4, C2D_Color32(0x3A, 0x55, 0x78, 255));
    C2D_DrawRectSolid(bx, by, 0.05f, bw, bh, C2D_Color32(0, 0, 0, 255));
    if (!minimap_layout_in(bx, by, bw, bh, &dest_x, &dest_y, &S)) return;

    // Per-tile rects, run-length-encoded per row (a mostly uniform row - ocean,
    // or unexplored - collapses to a few rects). See the s_minimap_colors note.
    for (int ty = 0; ty < s_map_h; ty++) {
        int rx = 0;
        while (rx < s_map_w) {
            u32 c = s_minimap_colors[ty * s_map_w + rx];
            int run = 1;
            while (rx + run < s_map_w && s_minimap_colors[ty * s_map_w + rx + run] == c) run++;
            C2D_DrawRectSolid(dest_x + rx * S, dest_y + ty * S, 0.1f, run * S + 0.5f, S + 0.5f, c);
            rx += run;
        }
    }

    // The part of the world the bottom screen shows.
    float cx = (float)cam_x / TILE_PX, cy = (float)cam_y / TILE_PX;
    if (map_wraps_x()) {
        cx = fmodf(cx, (float)s_map_w);
        if (cx < 0.0f) cx += s_map_w;
    }
    float vw = view_w_tiles, vh = view_h_tiles;
    if (vw > s_map_w) vw = s_map_w;
    if (vh > s_map_h) vh = s_map_h;
    if (cy < 0.0f) cy = 0.0f;
    if (cy + vh > s_map_h) cy = s_map_h - vh;
    if (!map_wraps_x()) {
        if (cx < 0.0f) cx = 0.0f;
        if (cx + vw > s_map_w) cx = s_map_w - vw;
    }
    u32 fc = C2D_Color32(0xFF, 0xFF, 0xFF, 0xFF);
    if (map_wraps_x() && cx + vw > s_map_w) {
        float first = s_map_w - cx;
        draw_hollow_rect(dest_x + cx * S, dest_y + cy * S, first * S, vh * S, 1.5f, fc);
        draw_hollow_rect(dest_x, dest_y + cy * S, (vw - first) * S, vh * S, 1.5f, fc);
    } else {
        draw_hollow_rect(dest_x + cx * S, dest_y + cy * S, vw * S, vh * S, 1.5f, fc);
    }
    if (sel_x >= 0 && sel_y >= 0) {
        float m = S < 3 ? 3 : S;
        C2D_DrawRectSolid(dest_x + (sel_x + 0.5f) * S - m / 2, dest_y + (sel_y + 0.5f) * S - m / 2,
                          0.55f, m, m, C2D_Color32(0xFF, 0xF0, 0x30, 0xFF));
    }
}

static void draw_minimap(int cam_x, int cam_y, float view_w_tiles, float view_h_tiles,
                         int sel_x, int sel_y)
{
    draw_minimap_in(MINIMAP_X, MINIMAP_Y, MINIMAP_W, MINIMAP_H, cam_x, cam_y, view_w_tiles, view_h_tiles, sel_x, sel_y);
}

// World map on the touch screen (tap to jump there).
#define WORLD_BOX_X 4
#define WORLD_BOX_Y 4
#define WORLD_BOX_W 312
#define WORLD_BOX_H (HUD_Y - 8)
static bool s_world_view;
void render_set_world_view(bool on) { s_world_view = on; }

bool world_map_hit(int px, int py, int *tx, int *ty)
{
    float dx, dy, S;
    if (!minimap_layout_in(WORLD_BOX_X, WORLD_BOX_Y, WORLD_BOX_W, WORLD_BOX_H, &dx, &dy, &S)) return false;
    int x = (int)((px - dx) / S), y = (int)((py - dy) / S);
    if (x < 0 || y < 0 || x >= s_map_w || y >= s_map_h) return false;
    *tx = x; *ty = y;
    return true;
}

// A translucent strip, for HUD text/buttons over the map.
static void hud_strip(float y, float w, float h)
{
    C2D_DrawRectSolid(0, y, 0.88f, w, h, C2D_Color32(0x08, 0x0C, 0x14, 0xC8));
}

// Text helpers that measure, for centring and right-aligning.
static float text_width(const char *str, float scale)
{
    return font_width(str, scale);
}

static int s_idle_count = 0; // idle units, shown on the Next button
void render_set_idle_count(int n) { s_idle_count = n; }

static void draw_button(float x, float y, float w, float h, float z, const char *label,
                        float scale, bool enabled, bool highlight)
{
    u32 bg = highlight ? C2D_Color32(0x3C, 0x78, 0x40, 0xF0)
           : enabled   ? C2D_Color32(0x2C, 0x50, 0x80, 0xF0) : C2D_Color32(0x24, 0x2C, 0x38, 0xF0);
    u32 border = highlight ? C2D_Color32(0x8C, 0xE0, 0x90, 0xFF) : C2D_Color32(0x6C, 0xA8, 0xE0, 0xFF);
    C2D_DrawRectSolid(x, y, z, w, h, bg);
    C2D_DrawRectSolid(x, y, z + 0.001f, w, 2, border);
    float tw = text_width(label, scale);
    while (tw > w - 6 && scale > 0.3f) { // long labels (or Large text) shrink to fit
        scale -= 0.02f;
        tw = text_width(label, scale);
    }
    draw_text_z(x + (w - tw) / 2, y + (h - 30 * scale) / 2 + 1, z + 0.002f, scale,
                enabled ? 0xFFFFFFFF : C2D_Color32(0x80, 0x88, 0x94, 0xFF), label);
}

static void pretty_tag(const char *tag, char *out, int n)
{
    // "ts.grassland_resources" -> "Grassland resources"
    const char *p = strchr(tag, '.');
    p = p ? p + 1 : tag;
    int j = 0;
    for (int i = 0; p[i] && j < n - 1; i++) {
        char c = p[i] == '_' ? ' ' : p[i];
        out[j++] = (i == 0) ? toupper((unsigned char)c) : c;
    }
    out[j] = 0;
}

static const char *k_terrain_names[] = { "Ocean", "Grassland", "Plains", "Desert", "Tundra",
    "Swamp", "Jungle", "Hills", "Forest", "Mountains", "Arctic" };

// ---- Bottom screen: the map, move hints, HUD buttons and popup ----

void popup_layout(PopupMenu *m, int anchor_x, int anchor_y)
{
    m->cols = m->n > 12 ? 3 : m->n > 7 ? 2 : 1;
    int rows = (m->n + m->cols - 1) / m->cols;
    m->w = m->cols * POPUP_ITEM_W + (m->cols + 1) * 3;
    m->h = POPUP_TITLE_H + rows * (POPUP_ITEM_H + 3) + 3;
    int x = anchor_x + 24;
    if (x + m->w > 318) x = anchor_x - 24 - m->w;
    if (x < 2) x = 2;
    if (x + m->w > 318) x = 318 - m->w;
    int y = anchor_y - m->h / 2;
    if (y < 2) y = 2;
    if (y + m->h > HUD_Y - 2) y = HUD_Y - 2 - m->h;
    if (y < 2) y = 2;
    m->x = x; m->y = y;
}

int popup_hit_test(const PopupMenu *m, int px, int py)
{
    if (px < m->x || px >= m->x + m->w || py < m->y || py >= m->y + m->h) return -1;
    int rx = px - m->x - 3, ry = py - m->y - POPUP_TITLE_H;
    if (rx < 0 || ry < 0) return -2;
    int col = rx / (POPUP_ITEM_W + 3), row = ry / (POPUP_ITEM_H + 3);
    if (rx % (POPUP_ITEM_W + 3) >= POPUP_ITEM_W || ry % (POPUP_ITEM_H + 3) >= POPUP_ITEM_H) return -2;
    int rows = (m->n + m->cols - 1) / m->cols;
    int idx = col * rows + row;
    if (col >= m->cols || idx >= m->n || !m->enabled[idx]) return -2;
    return idx;
}

static void draw_popup(const PopupMenu *m)
{
    const float Z = 0.93f;
    C2D_DrawRectSolid(m->x - 1, m->y - 1, Z, m->w + 2, m->h + 2, C2D_Color32(0x8C, 0xB8, 0xE8, 0xFF));
    C2D_DrawRectSolid(m->x, m->y, Z + 0.001f, m->w, m->h, C2D_Color32(0x10, 0x18, 0x26, 0xF4));
    float ts = 0.5f; // long titles shrink to fit the menu
    while (ts > 0.4f && text_width(m->title, ts) > m->w - 8) ts -= 0.02f;
    draw_text_z(m->x + 5, m->y + 2, Z + 0.002f, ts, C2D_Color32(0xFF, 0xE0, 0x80, 0xFF), m->title);
    int rows = (m->n + m->cols - 1) / m->cols;
    for (int i = 0; i < m->n; i++) {
        int col = i / rows, row = i % rows;
        draw_button(m->x + 3 + col * (POPUP_ITEM_W + 3), m->y + POPUP_TITLE_H + row * (POPUP_ITEM_H + 3),
                    POPUP_ITEM_W, POPUP_ITEM_H, Z + 0.003f, m->label[i], 0.5f, m->enabled[i], false);
    }
}

int hud_hit_test(int px, int py)
{
    if (py < HUD_Y) return HUD_NONE;
    if (px >= HUD_MENU_X && px < HUD_MENU_X + HUD_MENU_W) return HUD_MENU;
    if (px >= HUD_NEXT_X && px < HUD_NEXT_X + HUD_NEXT_W) return HUD_NEXT;
    if (px >= HUD_MAP_X  && px < HUD_MAP_X  + HUD_MAP_W)  return HUD_MAP;
    if (px >= HUD_ZOUT_X && px < HUD_ZOUT_X + HUD_ZOOM_W) return HUD_ZOUT;
    if (px >= HUD_ZIN_X  && px < HUD_ZIN_X  + HUD_ZOOM_W) return HUD_ZIN;
    if (px >= HUD_END_X  && px < HUD_END_X  + HUD_END_W)  return HUD_END;
    return HUD_BAR;
}

// Screen position of a tile's centre on the bottom map (nearest wrap copy).
static void tile_center(int tx, int ty, int cam_x, int cam_y, float scale, float *sx, float *sy)
{
    int mw = 40, mh = 40;
    get_map_dimensions(&mw, &mh);
    int wx = tx * TILE_PX;
    if (map_wraps_x()) {
        int world_w = mw * TILE_PX, c = cam_x + (int)(160 / scale);
        int d = ((wx - c) % world_w + world_w + world_w / 2) % world_w - world_w / 2;
        wx = c + d;
    }
    *sx = (wx + TILE_PX / 2 - cam_x) * scale;
    *sy = (ty * TILE_PX + TILE_PX / 2 - cam_y) * scale;
}

static void draw_route(const RoutePreview *r, int cam_x, int cam_y, float scale)
{
    if (!r || r->n <= 0) return;
    u32 c = r->reachable ? C2D_Color32(0xFF, 0xB0, 0x30, 0xF0) : C2D_Color32(0xE0, 0x40, 0x40, 0xF0);
    float px = 0, py = 0;
    for (int i = 0; i < r->n; i++) {
        float x, y;
        tile_center(r->x[i], r->y[i], cam_x, cam_y, scale, &x, &y);
        if (i > 0) {
            // dotted segment from the previous point
            float dx = x - px, dy = y - py;
            float len = sqrtf(dx * dx + dy * dy);
            int dots = (int)(len / 6);
            for (int k = 1; k < dots; k++) {
                float t = (float)k / dots;
                C2D_DrawRectSolid(px + dx * t - 1, py + dy * t - 1, 0.74f, 2, 2, c);
            }
        }
        C2D_DrawRectSolid(x - 2.5f, y - 2.5f, 0.741f, 5, 5, c);
        px = x; py = y;
    }
    char lbl[24];
    if (!r->reachable) snprintf(lbl, sizeof(lbl), "can't reach");
    else if (r->turns <= 0) snprintf(lbl, sizeof(lbl), "this turn");
    else if (r->turns == 1) snprintf(lbl, sizeof(lbl), "1 turn");
    else snprintf(lbl, sizeof(lbl), "%d turns", r->turns);
    float w = font_width(lbl, 0.42f) + 8;
    float bx = px - w / 2, by = py + 10 * scale + 4;
    if (bx < 2) bx = 2;
    if (bx + w > 318) bx = 318 - w;
    if (by > HUD_Y - 18) by = py - 30;
    C2D_DrawRectSolid(bx, by, 0.85f, w, 15, C2D_Color32(0x10, 0x14, 0x1C, 0xE8));
    C2D_DrawRectSolid(bx, by, 0.851f, 2, 15, c);
    font_draw(lbl, bx + 5, by - 1, 0.852f, 0.42f, 0xFFFFFFFF);
}

static char s_bar_labels[UNITBAR_MAX][20];
static int s_bar_n;
void render_set_unit_bar(const char labels[][20], int n)
{
    if (n > UNITBAR_MAX) n = UNITBAR_MAX;
    for (int i = 0; i < n; i++) snprintf(s_bar_labels[i], sizeof(s_bar_labels[i]), "%s", labels[i]);
    s_bar_n = n;
}

static short s_range_x[256], s_range_y[256];
static int s_range_n;
void render_set_move_range(const short *xs, const short *ys, int n)
{
    if (n > 256) n = 256;
    memcpy(s_range_x, xs, n * sizeof(short));
    memcpy(s_range_y, ys, n * sizeof(short));
    s_range_n = n;
}

void render_bottom_map(int cam_x, int cam_y, float scale, int sel_x, int sel_y,
                       int unit_x, int unit_y, bool unit_can_move, int goto_x, int goto_y,
                       const PopupMenu *popup, bool can_zoom_out, bool can_zoom_in, bool all_moved,
                       const RoutePreview *route, const char *end_label)
{
    if (s_world_view) {
        C2D_DrawRectSolid(0, 0, 0.0f, 320, 240, C2D_Color32(0x0C, 0x12, 0x1C, 0xFF));
        draw_minimap_in(WORLD_BOX_X, WORLD_BOX_Y, WORLD_BOX_W, WORLD_BOX_H, cam_x, cam_y,
                        320.0f / (TILE_PX * scale), (float)HUD_Y / (TILE_PX * scale), sel_x, sel_y);
        popup = NULL;
        goto hud;
    }
    render_map_view(cam_x, cam_y, scale, 320, 240, sel_x, sel_y, unit_x >= 0, goto_x, goto_y);
    draw_route(route, cam_x, cam_y, scale);

    // Where the commanded unit can get to this turn: a soft tint on every
    // reachable square (from pathfinding, so water/mountains/ZOC are right).
    if (unit_x >= 0 && unit_can_move && !popup && s_range_n > 0) {
        int mw = 40, mh = 40;
        get_map_dimensions(&mw, &mh);
        float T = TILE_PX * scale;
        u32 fill = C2D_Color32(0xFF, 0xFF, 0xFF, 0x30), edge = C2D_Color32(0xFF, 0xFF, 0xFF, 0x80);
        for (int i = 0; i < s_range_n; i++) {
            int tx = s_range_x[i], ty = s_range_y[i];
            int wx = tx * TILE_PX;
            if (map_wraps_x()) {
                int world_w = mw * TILE_PX;
                int d = ((wx - cam_x + TILE_PX) % world_w + world_w) % world_w - TILE_PX;
                wx = cam_x + d;
            }
            float x = (wx - cam_x) * scale, y = (ty * TILE_PX - cam_y) * scale;
            if (x < -T || y < -T || x > 320 || y > 240) continue;
            C2D_DrawRectSolid(x + 1, y + 1, 0.68f, T - 2, T - 2, fill);
            C2D_DrawRectSolid(x + 1, y + 1, 0.681f, T - 2, 1, edge);
            C2D_DrawRectSolid(x + 1, y + T - 2, 0.681f, T - 2, 1, edge);
            C2D_DrawRectSolid(x + 1, y + 1, 0.681f, 1, T - 2, edge);
            C2D_DrawRectSolid(x + T - 2, y + 1, 0.681f, 1, T - 2, edge);
        }
    }

    // Quick orders for the selected unit (one tap instead of unit + menu).
    if (s_bar_n > 0 && !popup) {
        float bw = 320.0f / s_bar_n;
        for (int i = 0; i < s_bar_n; i++) {
            bool more = i == s_bar_n - 1 && strcmp(s_bar_labels[i], "More...") == 0;
            draw_button(i * bw + 1, UNITBAR_Y, bw - 2, UNITBAR_H, 0.89f, s_bar_labels[i], 0.47f, true, !more && i == 0);
        }
    }

hud:
    // HUD button row along the bottom edge.
    hud_strip(HUD_Y, 320, 240 - HUD_Y);
    const float Z = 0.9f;
    float by = HUD_Y + 2, bh = 240 - HUD_Y - 3;
    draw_button(HUD_MENU_X, by, HUD_MENU_W, bh, Z, "Menu", 0.5f, true, false);
    char next_lbl[16] = "Next";
    if (s_idle_count > 0 && !all_moved) snprintf(next_lbl, sizeof(next_lbl), "Next %d", s_idle_count);
    draw_button(HUD_NEXT_X, by, HUD_NEXT_W, bh, Z, next_lbl, 0.5f, !all_moved, false);
    draw_button(HUD_MAP_X,  by, HUD_MAP_W,  bh, Z, "Map", 0.5f, true, s_world_view);
    draw_button(HUD_ZOUT_X, by, HUD_ZOOM_W, bh, Z, "-", 0.7f, can_zoom_out, false);
    draw_button(HUD_ZIN_X,  by, HUD_ZOOM_W, bh, Z, "+", 0.7f, can_zoom_in, false);
    // The smart button: "Choose Research" / "Wien: Production" / "End Turn".
    bool decision = end_label && strcmp(end_label, "End Turn") != 0;
    if (decision) {
        // amber, so a pending decision reads differently from a plain End Turn
        C2D_DrawRectSolid(HUD_END_X, by, Z, HUD_END_W, bh, C2D_Color32(0x8A, 0x5A, 0x10, 0xF0));
        C2D_DrawRectSolid(HUD_END_X, by, Z + 0.001f, HUD_END_W, 2, C2D_Color32(0xFF, 0xC8, 0x50, 0xFF));
        float s2 = 0.5f, tw = text_width(end_label, s2);
        while (tw > HUD_END_W - 6 && s2 > 0.32f) { s2 -= 0.02f; tw = text_width(end_label, s2); }
        draw_text_z(HUD_END_X + (HUD_END_W - tw) / 2, by + (bh - 30 * s2) / 2 + 1, Z + 0.002f, s2, 0xFFFFFFFF, end_label);
    } else {
        draw_button(HUD_END_X, by, HUD_END_W, bh, Z, "End Turn", 0.5f, true, all_moved);
    }

    if (popup) draw_popup(popup);
}

// Draws text wrapped to width w; returns the number of lines used.
static int draw_wrapped(const char *text, float x, float y, float w, float scale, u32 color,
                        int max_lines, float line_h, float z)
{
    char line[200];
    int lines = 0;
    const char *p = text;
    while (*p && lines < max_lines) {
        int best = 0, i = 0;
        // take words while they fit
        while (p[i]) {
            int j = i;
            while (p[j] && p[j] != ' ') j++;
            if (j >= (int)sizeof(line)) break;
            memcpy(line, p, j); line[j] = 0;
            if (font_width(line, scale) > w && best > 0) break;
            best = j;
            if (!p[j]) break;
            i = j + 1;
        }
        if (best == 0) {      // a single over-long word: hard cut
            best = 0;
            while (p[best] && best < (int)sizeof(line) - 1) {
                line[best] = p[best]; line[best + 1] = 0;
                if (font_width(line, scale) > w) break;
                best++;
            }
            if (best == 0) best = 1;
        }
        memcpy(line, p, best); line[best] = 0;
        if (lines == max_lines - 1 && p[best]) {
            // last allowed line: ellipsis
            while (best > 0 && font_width(line, scale) + font_width("...", scale) > w) line[--best] = 0;
            strcat(line, "...");
        }
        draw_text_z(x, y + lines * line_h, z, scale, color, line);
        lines++;
        p += best;
        while (*p == ' ') p++;
    }
    return lines;
}

static u32 msg_color(int cat)
{
    switch (cat) {
    case MSG_COMBAT:    return C2D_Color32(0xF0, 0x60, 0x50, 0xFF);
    case MSG_CITY:      return C2D_Color32(0xE0, 0xA8, 0x50, 0xFF);
    case MSG_SCIENCE:   return C2D_Color32(0x70, 0xB8, 0xFF, 0xFF);
    case MSG_DIPLOMACY: return C2D_Color32(0xC0, 0x90, 0xF0, 0xFF);
    case MSG_GOOD:      return C2D_Color32(0x80, 0xE0, 0x80, 0xFF);
    default:            return C2D_Color32(0x8C, 0xA0, 0xB4, 0xFF);
    }
}

static void draw_turn_report(const TopInfo *ti)
{
    u32 white = 0xFFFFFFFF;
    u32 label = C2D_Color32(0x8C, 0xA0, 0xB4, 0xFF);
    char line[64];
    layer(3.0f);
    C2D_DrawRectSolid(6, 22, 0.0f, 388, 198, C2D_Color32(0x10, 0x18, 0x26, 0xFF));
    C2D_DrawRectSolid(6, 22, 0.01f, 388, 2, C2D_Color32(0xFF, 0xD8, 0x60, 0xFF));
    snprintf(line, sizeof(line), "Since last turn");
    draw_text(14, 25, 0.6f, white, line);
    float y = 50;
    for (int i = 0; i < ti->report_n && y < 205; i++) {
        const GameMessage *m = &ti->report[i];
        C2D_DrawRectSolid(14, y + 5, 0.01f, 6, 6, msg_color(m->category));
        int used = draw_wrapped(m->text, 26, y, 360, 0.44f, C2D_Color32(0xD8, 0xE0, 0xEA, 0xFF), 2, 15, 0.01f);
        y += used * 15 + 3;
    }
    if (ti->report_more > 0) {
        snprintf(line, sizeof(line), "+%d more in Menu > Messages", ti->report_more);
        draw_text(14, 203, 0.42f, label, line);
    }
}

// ---- Top screen: information only (never touched) ----

void render_top_info(const TopInfo *ti)
{
    C2D_TextBufClear(g_textBuf);
    update_minimap_slice(5);

    u32 white = C2D_Color32(0xFF, 0xFF, 0xFF, 0xFF);
    u32 gold  = C2D_Color32(0xFF, 0xD8, 0x60, 0xFF);
    u32 soft  = C2D_Color32(0xB8, 0xC8, 0xDC, 0xFF);
    u32 label = C2D_Color32(0x8C, 0xA0, 0xB4, 0xFF);
    char line[128];

    // Status bar.
    C2D_DrawRectSolid(0, 0, 0.0f, 400, 18, C2D_Color32(0x2C, 0x50, 0x80, 0xFF));
    C2D_DrawRectSolid(0, 18, 0.0f, 400, 1, C2D_Color32(0x6C, 0xA8, 0xE0, 0xFF));
    int turn = get_game_turn();
    if (turn > 0) {
        if (ti->civ && ti->civ->year[0]) snprintf(line, sizeof(line), "Turn %d, %s", turn, ti->civ->year);
        else snprintf(line, sizeof(line), "Turn %d", turn);
        draw_text(5, 1, 0.52f, white, line);
        float gx = 5 + text_width(line, 0.52f) + 12;
        if (ti->civ) {
            snprintf(line, sizeof(line), "Gold %d (%+d)", ti->civ->gold, ti->civ->gold_per_turn);
            draw_text(gx, 1, 0.52f, gold, line);
            gx += text_width(line, 0.52f) + 10;
            snprintf(line, sizeof(line), "%s  Tax %d Lux %d Sci %d", ti->civ->government_name,
                     ti->civ->tax, ti->civ->lux, ti->civ->sci);
            float rs = 0.45f;
            while (rs > 0.38f && 395 - text_width(line, rs) < gx) rs -= 0.01f;
            if (395 - text_width(line, rs) < gx) // still no room: drop the government name
                snprintf(line, sizeof(line), "Tax %d Lux %d Sci %d", ti->civ->tax, ti->civ->lux, ti->civ->sci);
            draw_text(395 - text_width(line, rs), 2, rs, soft, line);
        }
    } else {
        draw_text(5, 1, 0.52f, white, "Generating world...");
    }

    if (ti->report_n > 0) {
        draw_turn_report(ti);
        layer(0.0f);
        C2D_DrawRectSolid(0, 240 - 17, 0.0f, 400, 17, C2D_Color32(0x0C, 0x12, 0x1C, 0xFF));
        draw_text(5, 240 - 16, 0.45f, soft, "Tap or press anything to carry on.");
        return;
    }

    // Left: minimap and research (slightly in front).
    layer(2.5f);
    draw_minimap(ti->cam_x, ti->cam_y, 320.0f / (TILE_PX * ti->zoom), (240.0f - (240 - HUD_Y)) / (TILE_PX * ti->zoom),
                 ti->tile_x, ti->tile_y);
    if (ti->civ && ti->civ->research_name[0]) {
        float y = MINIMAP_Y + MINIMAP_H + 6;
        float lb = font_get_bias() ? 2.0f : 0.0f; // Large text needs a little more room
        draw_text(MINIMAP_X, y - lb, 0.45f, label, "Researching");
        draw_text(MINIMAP_X, y + 13 + lb, 0.5f, white, ti->civ->research_name);
        int need = ti->civ->bulbs_needed > 0 ? ti->civ->bulbs_needed : 1;
        float frac = (float)ti->civ->bulbs / need;
        if (frac < 0) frac = 0;
        if (frac > 1) frac = 1;
        C2D_DrawRectSolid(MINIMAP_X, y + 31 + lb, 0.0f, MINIMAP_W, 6, C2D_Color32(0x24, 0x30, 0x40, 0xFF));
        C2D_DrawRectSolid(MINIMAP_X, y + 31 + lb, 0.01f, MINIMAP_W * frac, 6, C2D_Color32(0x50, 0xA0, 0xF0, 0xFF));
        if (ti->civ->research_turns >= 0)
            snprintf(line, sizeof(line), "%d/%d bulbs, %d turns", ti->civ->bulbs, ti->civ->bulbs_needed, ti->civ->research_turns);
        else
            snprintf(line, sizeof(line), "%d/%d bulbs", ti->civ->bulbs, ti->civ->bulbs_needed);
        draw_text(MINIMAP_X, y + 39 + lb, 0.42f, soft, line);
        if (ti->civ->goal_name[0]) {
            snprintf(line, sizeof(line), "Goal: %s", ti->civ->goal_name);
            draw_text(MINIMAP_X, y + 53 + lb, 0.42f, gold, line);
        }
    }

    // Right: what's selected (in front; the portrait most of all).
    layer(4.0f);
    const float CX = 206, CW = 190;
    C2D_DrawRectSolid(CX - 4, 24, 0.0f, CW + 6, 196, C2D_Color32(0x10, 0x18, 0x26, 0xFF));
    if (ti->unit && ti->unit->valid) {
        const UnitInfo *u = ti->unit;
        C2D_DrawRectSolid(CX - 4, 24, 0.01f, 4, 196, owner_color(u->owner));
        // Unit portrait, double size.
        C2D_DrawRectSolid(CX + 2, 30, 0.01f, 64, 64, C2D_Color32(0x1C, 0x28, 0x3A, 0xFF));
        layer(5.0f); // the portrait sits just a little in front of its card
        C2D_DrawImageAt(C2D_SpriteSheetGetImage(g_spriteSheet, get_unit_sprite(u->rule_name)),
                        CX + 4, 32, 0.02f, NULL, 2.0f, 2.0f);
        layer(4.0f);
        draw_text(CX + 72, 30, 0.6f, white, u->name);
        if (u->veteran > 0) {
            snprintf(line, sizeof(line), "Veteran %d", u->veteran);
            draw_text(CX + 72, 50, 0.45f, gold, line);
        }
        snprintf(line, sizeof(line), "%s", u->activity);
        draw_text(CX + 72, 66, 0.45f, u->idle ? soft : C2D_Color32(0x90, 0xE0, 0x90, 0xFF), line);
        if (u->home[0]) {
            snprintf(line, sizeof(line), "Home: %s", u->home);
            draw_text(CX + 72, 80, 0.42f, label, line);
        }
        float y = 102;
        snprintf(line, sizeof(line), "Attack %d   Defense %d   FP %d", u->attack, u->defense, u->firepower);
        draw_text(CX + 2, y, 0.47f, white, line);
        y += 18;
        // HP bar
        draw_text(CX + 2, y, 0.47f, white, "HP");
        float hpf = u->hp_max > 0 ? (float)u->hp / u->hp_max : 0;
        C2D_DrawRectSolid(CX + 30, y + 5, 0.01f, 110, 7, C2D_Color32(0x40, 0x18, 0x18, 0xFF));
        C2D_DrawRectSolid(CX + 30, y + 5, 0.02f, 110 * hpf, 7,
                          hpf > 0.5f ? C2D_Color32(0x50, 0xD0, 0x50, 0xFF) : hpf > 0.25f ? C2D_Color32(0xE0, 0xC0, 0x40, 0xFF)
                                                                       : C2D_Color32(0xE0, 0x40, 0x40, 0xFF));
        snprintf(line, sizeof(line), "%d/%d", u->hp, u->hp_max);
        draw_text(CX + 146, y, 0.45f, soft, line);
        y += 18;
        snprintf(line, sizeof(line), "Moves left: %s", u->moves_left > 0 ? u->moves : "none");
        draw_text(CX + 2, y, 0.47f, u->moves_left > 0 ? white : label, line);
        y += 18;
        if (ti->tile && ti->tile_x == u->x && ti->tile_y == u->y && ti->tile->visibility > 0) {
            const TileInfo *t = ti->tile;
            snprintf(line, sizeof(line), "On: %s%s%s%s%s",
                     t->terrain <= TERRAIN_ARCTIC ? k_terrain_names[t->terrain] : "?",
                     t->has_river ? ", River" : "",
                     t->has_rail ? ", Railroad" : t->has_road ? ", Road" : "",
                     t->has_farmland ? ", Farmland" : t->has_irrigation ? ", Irrigation" : "",
                     t->has_mine ? ", Mine" : "");
            draw_text(CX + 2, y, 0.42f, soft, line);
            y += 16;
        }
        if (ti->stack_n > 1) {
            snprintf(line, sizeof(line), "Stack of %d units here", ti->stack_n);
            draw_text(CX + 2, y, 0.45f, label, line);
        }
    } else if (ti->tile && ti->tile_x >= 0) {
        const TileInfo *t = ti->tile;
        draw_text(CX + 2, 28, 0.45f, label, "This square");
        if (t->visibility == 0) {
            draw_text(CX + 2, 46, 0.6f, soft, "Unexplored");
        } else {
            const char *tn = (t->terrain <= TERRAIN_ARCTIC) ? k_terrain_names[t->terrain] : "?";
            draw_text(CX + 2, 44, 0.65f, white, tn);
            float y = 66;
            char extra[96] = "";
            snprintf(extra, sizeof(extra), "%s%s%s%s%s",
                     t->has_river ? "River  " : "",
                     t->has_maglev ? "Maglev  " : t->has_rail ? "Railroad  " : t->has_road ? "Road  " : "",
                     t->has_farmland ? "Farmland  " : t->has_irrigation ? "Irrigation  " : "",
                     t->has_mine ? "Mine  " : "",
                     t->has_pollution ? "Pollution" : t->has_fallout ? "Fallout" : "");
            if (extra[0]) { draw_text(CX + 2, y, 0.45f, soft, extra); y += 16; }
            if (t->resource_tag[0]) {
                char res[48];
                pretty_tag(t->resource_tag, res, sizeof(res));
                snprintf(line, sizeof(line), "Resource: %s", res);
                draw_text(CX + 2, y, 0.45f, gold, line);
                y += 16;
            }
            if (t->has_village) { draw_text(CX + 2, y, 0.45f, gold, "Village (hut)"); y += 16; }
            if (t->visibility == 1) { draw_text(CX + 2, y, 0.42f, label, "(last seen, fogged)"); y += 16; }
            if (t->city_owner >= 0) {
                y += 4;
                snprintf(line, sizeof(line), "%s  (size %d)", t->city_name[0] ? t->city_name : "City", t->city_size);
                draw_text(CX + 2, y, 0.55f, owner_color(t->city_owner), line);
                y += 20;
            }
            for (int i = 0; i < ti->stack_n && i < 5; i++) {
                snprintf(line, sizeof(line), "%s%s", ti->stack[i].name,
                         ti->stack[i].mine && !ti->stack[i].can_move ? "  (done)" : "");
                draw_text(CX + 2, y, 0.47f, owner_color(ti->stack[i].owner), line);
                y += 16;
            }
            if (ti->stack_n > 5) {
                snprintf(line, sizeof(line), "+%d more", ti->stack_n - 5);
                draw_text(CX + 2, y, 0.45f, label, line);
            }
        }
    } else {
        draw_text(CX + 2, 30, 0.5f, soft, "Tap the map below.");
        draw_text(CX + 2, 50, 0.45f, label, "Tap a unit for its orders.");
        draw_text(CX + 2, 66, 0.45f, label, "Drag to scroll the map.");
    }

    // Hint line (on the screen plane).
    layer(0.0f);
    C2D_DrawRectSolid(0, 240 - 17, 0.0f, 400, 17, C2D_Color32(0x0C, 0x12, 0x1C, 0xFF));
    if (ti->hint) {
        // Shrink long hints (e.g. combat odds with long unit names) to fit the strip.
        float hs = 0.45f, w = text_width(ti->hint, hs);
        if (w > 390) hs = hs * 390 / w;
        if (hs < 0.40f) hs = 0.40f; // legibility floor: hints are written to fit
        draw_text(5, 240 - 16 + (0.45f - hs) * 14, hs, ti->hint_color ? ti->hint_color : soft, ti->hint);
    }
}

// Neighbour lookups for coastline / terrain-edge matching. Coordinates are
// native; on a wrapping axis an out-of-range x/y is a real neighbour across the
// seam (get_tile_type wraps it), so only a *non-wrapping* edge is "off the map".
static bool is_land(int x, int y, int map_w, int map_h)
{
    if ((x < 0 || x >= map_w) && !map_wraps_x()) return false;
    if ((y < 0 || y >= map_h) && !map_wraps_y()) return false;
    return get_tile_type(x, y) != TERRAIN_OCEAN;
}

static bool is_same_terrain(int x, int y, int map_w, int map_h, TerrainType center_type)
{
    // Off a non-wrapping edge: treat as "same" so no coastline frame is drawn
    // against the true map boundary (matches the pre-wrap behaviour there).
    if ((x < 0 || x >= map_w) && !map_wraps_x()) return true;
    if ((y < 0 || y >= map_h) && !map_wraps_y()) return true;
    return get_tile_type(x, y) == center_type;
}

static const int plains_sprites[16] = {
    sprites_plains_n0e0s0w0_idx, sprites_plains_n1e0s0w0_idx, sprites_plains_n0e1s0w0_idx, sprites_plains_n1e1s0w0_idx,
    sprites_plains_n0e0s1w0_idx, sprites_plains_n1e0s1w0_idx, sprites_plains_n0e1s1w0_idx, sprites_plains_n1e1s1w0_idx,
    sprites_plains_n0e0s0w1_idx, sprites_plains_n1e0s0w1_idx, sprites_plains_n0e1s0w1_idx, sprites_plains_n1e1s0w1_idx,
    sprites_plains_n0e0s1w1_idx, sprites_plains_n1e0s1w1_idx, sprites_plains_n0e1s1w1_idx, sprites_plains_n1e1s1w1_idx
};
static const int desert_sprites[16] = {
    sprites_desert_n0e0s0w0_idx, sprites_desert_n1e0s0w0_idx, sprites_desert_n0e1s0w0_idx, sprites_desert_n1e1s0w0_idx,
    sprites_desert_n0e0s1w0_idx, sprites_desert_n1e0s1w0_idx, sprites_desert_n0e1s1w0_idx, sprites_desert_n1e1s1w0_idx,
    sprites_desert_n0e0s0w1_idx, sprites_desert_n1e0s0w1_idx, sprites_desert_n0e1s0w1_idx, sprites_desert_n1e1s0w1_idx,
    sprites_desert_n0e0s1w1_idx, sprites_desert_n1e0s1w1_idx, sprites_desert_n0e1s1w1_idx, sprites_desert_n1e1s1w1_idx
};
static const int tundra_sprites[16] = {
    sprites_tundra_n0e0s0w0_idx, sprites_tundra_n1e0s0w0_idx, sprites_tundra_n0e1s0w0_idx, sprites_tundra_n1e1s0w0_idx,
    sprites_tundra_n0e0s1w0_idx, sprites_tundra_n1e0s1w0_idx, sprites_tundra_n0e1s1w0_idx, sprites_tundra_n1e1s1w0_idx,
    sprites_tundra_n0e0s0w1_idx, sprites_tundra_n1e0s0w1_idx, sprites_tundra_n0e1s0w1_idx, sprites_tundra_n1e1s0w1_idx,
    sprites_tundra_n0e0s1w1_idx, sprites_tundra_n1e0s1w1_idx, sprites_tundra_n0e1s1w1_idx, sprites_tundra_n1e1s1w1_idx
};
static const int swamp_sprites[16] = {
    sprites_swamp_n0e0s0w0_idx, sprites_swamp_n1e0s0w0_idx, sprites_swamp_n0e1s0w0_idx, sprites_swamp_n1e1s0w0_idx,
    sprites_swamp_n0e0s1w0_idx, sprites_swamp_n1e0s1w0_idx, sprites_swamp_n0e1s1w0_idx, sprites_swamp_n1e1s1w0_idx,
    sprites_swamp_n0e0s0w1_idx, sprites_swamp_n1e0s0w1_idx, sprites_swamp_n0e1s0w1_idx, sprites_swamp_n1e1s0w1_idx,
    sprites_swamp_n0e0s1w1_idx, sprites_swamp_n1e0s1w1_idx, sprites_swamp_n0e1s1w1_idx, sprites_swamp_n1e1s1w1_idx
};
static const int jungle_sprites[16] = {
    sprites_jungle_n0e0s0w0_idx, sprites_jungle_n1e0s0w0_idx, sprites_jungle_n0e1s0w0_idx, sprites_jungle_n1e1s0w0_idx,
    sprites_jungle_n0e0s1w0_idx, sprites_jungle_n1e0s1w0_idx, sprites_jungle_n0e1s1w0_idx, sprites_jungle_n1e1s1w0_idx,
    sprites_jungle_n0e0s0w1_idx, sprites_jungle_n1e0s0w1_idx, sprites_jungle_n0e1s0w1_idx, sprites_jungle_n1e1s0w1_idx,
    sprites_jungle_n0e0s1w1_idx, sprites_jungle_n1e0s1w1_idx, sprites_jungle_n0e1s1w1_idx, sprites_jungle_n1e1s1w1_idx
};
static const int hills_sprites[16] = {
    sprites_hills_n0e0s0w0_idx, sprites_hills_n1e0s0w0_idx, sprites_hills_n0e1s0w0_idx, sprites_hills_n1e1s0w0_idx,
    sprites_hills_n0e0s1w0_idx, sprites_hills_n1e0s1w0_idx, sprites_hills_n0e1s1w0_idx, sprites_hills_n1e1s1w0_idx,
    sprites_hills_n0e0s0w1_idx, sprites_hills_n1e0s0w1_idx, sprites_hills_n0e1s0w1_idx, sprites_hills_n1e1s0w1_idx,
    sprites_hills_n0e0s1w1_idx, sprites_hills_n1e0s1w1_idx, sprites_hills_n0e1s1w1_idx, sprites_hills_n1e1s1w1_idx
};
static const int forest_sprites[16] = {
    sprites_forest_n0e0s0w0_idx, sprites_forest_n1e0s0w0_idx, sprites_forest_n0e1s0w0_idx, sprites_forest_n1e1s0w0_idx,
    sprites_forest_n0e0s1w0_idx, sprites_forest_n1e0s1w0_idx, sprites_forest_n0e1s1w0_idx, sprites_forest_n1e1s1w0_idx,
    sprites_forest_n0e0s0w1_idx, sprites_forest_n1e0s0w1_idx, sprites_forest_n0e1s0w1_idx, sprites_forest_n1e1s0w1_idx,
    sprites_forest_n0e0s1w1_idx, sprites_forest_n1e0s1w1_idx, sprites_forest_n0e1s1w1_idx, sprites_forest_n1e1s1w1_idx
};
static const int mountains_sprites[16] = {
    sprites_mountains_n0e0s0w0_idx, sprites_mountains_n1e0s0w0_idx, sprites_mountains_n0e1s0w0_idx, sprites_mountains_n1e1s0w0_idx,
    sprites_mountains_n0e0s1w0_idx, sprites_mountains_n1e0s1w0_idx, sprites_mountains_n0e1s1w0_idx, sprites_mountains_n1e1s1w0_idx,
    sprites_mountains_n0e0s0w1_idx, sprites_mountains_n1e0s0w1_idx, sprites_mountains_n0e1s0w1_idx, sprites_mountains_n1e1s0w1_idx,
    sprites_mountains_n0e0s1w1_idx, sprites_mountains_n1e0s1w1_idx, sprites_mountains_n0e1s1w1_idx, sprites_mountains_n1e1s1w1_idx
};
static const int arctic_sprites[16] = {
    sprites_arctic_n0e0s0w0_idx, sprites_arctic_n1e0s0w0_idx, sprites_arctic_n0e1s0w0_idx, sprites_arctic_n1e1s0w0_idx,
    sprites_arctic_n0e0s1w0_idx, sprites_arctic_n1e0s1w0_idx, sprites_arctic_n0e1s1w0_idx, sprites_arctic_n1e1s1w0_idx,
    sprites_arctic_n0e0s0w1_idx, sprites_arctic_n1e0s0w1_idx, sprites_arctic_n0e1s0w1_idx, sprites_arctic_n1e1s0w1_idx,
    sprites_arctic_n0e0s1w1_idx, sprites_arctic_n1e0s1w1_idx, sprites_arctic_n0e1s1w1_idx, sprites_arctic_n1e1s1w1_idx
};

static const int *terrain_match_sprites[] = {
    NULL, // TERRAIN_OCEAN
    NULL, // TERRAIN_GRASSLAND
    plains_sprites,
    desert_sprites,
    tundra_sprites,
    swamp_sprites,
    jungle_sprites,
    hills_sprites,
    forest_sprites,
    mountains_sprites,
    arctic_sprites
};

static const int road_sprites[16] = {
    sprites_road_isolated_idx,
    sprites_road_n1e0s0w0_idx, sprites_road_n0e1s0w0_idx, sprites_road_n1e1s0w0_idx,
    sprites_road_n0e0s1w0_idx, sprites_road_n1e0s1w0_idx, sprites_road_n0e1s1w0_idx, sprites_road_n1e1s1w0_idx,
    sprites_road_n0e0s0w1_idx, sprites_road_n1e0s0w1_idx, sprites_road_n0e1s0w1_idx, sprites_road_n1e1s0w1_idx,
    sprites_road_n0e0s1w1_idx, sprites_road_n1e0s1w1_idx, sprites_road_n0e1s1w1_idx, sprites_road_n1e1s1w1_idx
};

static const int rail_sprites[16] = {
    sprites_rail_isolated_idx,
    sprites_rail_n1e0s0w0_idx, sprites_rail_n0e1s0w0_idx, sprites_rail_n1e1s0w0_idx,
    sprites_rail_n0e0s1w0_idx, sprites_rail_n1e0s1w0_idx, sprites_rail_n0e1s1w0_idx, sprites_rail_n1e1s1w0_idx,
    sprites_rail_n0e0s0w1_idx, sprites_rail_n1e0s0w1_idx, sprites_rail_n0e1s0w1_idx, sprites_rail_n1e1s0w1_idx,
    sprites_rail_n0e0s1w1_idx, sprites_rail_n1e0s1w1_idx, sprites_rail_n0e1s1w1_idx, sprites_rail_n1e1s1w1_idx
};

static const int maglev_sprites[16] = {
    sprites_maglev_isolated_idx,
    sprites_maglev_n1e0s0w0_idx, sprites_maglev_n0e1s0w0_idx, sprites_maglev_n1e1s0w0_idx,
    sprites_maglev_n0e0s1w0_idx, sprites_maglev_n1e0s1w0_idx, sprites_maglev_n0e1s1w0_idx, sprites_maglev_n1e1s1w0_idx,
    sprites_maglev_n0e0s0w1_idx, sprites_maglev_n1e0s0w1_idx, sprites_maglev_n0e1s0w1_idx, sprites_maglev_n1e1s0w1_idx,
    sprites_maglev_n0e0s1w1_idx, sprites_maglev_n1e0s1w1_idx, sprites_maglev_n0e1s1w1_idx, sprites_maglev_n1e1s1w1_idx
};

static const int river_sprites[16] = {
    sprites_river_n0e0s0w0_idx,
    sprites_river_n1e0s0w0_idx, sprites_river_n0e1s0w0_idx, sprites_river_n1e1s0w0_idx,
    sprites_river_n0e0s1w0_idx, sprites_river_n1e0s1w0_idx, sprites_river_n0e1s1w0_idx, sprites_river_n1e1s1w0_idx,
    sprites_river_n0e0s0w1_idx, sprites_river_n1e0s0w1_idx, sprites_river_n0e1s0w1_idx, sprites_river_n1e1s0w1_idx,
    sprites_river_n0e0s1w1_idx, sprites_river_n1e0s1w1_idx, sprites_river_n0e1s1w1_idx, sprites_river_n1e1s1w1_idx
};

static const int darkness_sprites[16] = {
    0,
    sprites_tx_darkness_n1e0s0w0_idx, sprites_tx_darkness_n0e1s0w0_idx, sprites_tx_darkness_n1e1s0w0_idx,
    sprites_tx_darkness_n0e0s1w0_idx, sprites_tx_darkness_n1e0s1w0_idx, sprites_tx_darkness_n0e1s1w0_idx, sprites_tx_darkness_n1e1s1w0_idx,
    sprites_tx_darkness_n0e0s0w1_idx, sprites_tx_darkness_n1e0s0w1_idx, sprites_tx_darkness_n0e1s0w1_idx, sprites_tx_darkness_n1e1s0w1_idx,
    sprites_tx_darkness_n0e0s1w1_idx, sprites_tx_darkness_n1e0s1w1_idx, sprites_tx_darkness_n0e1s1w1_idx, sprites_tx_darkness_n1e1s1w1_idx
};

typedef struct {
    const char *graphic_str;
    int sprite_idx;
} ResourceSpriteMap;

static const ResourceSpriteMap g_resource_sprites[] = {
    { "ts.spice", sprites_ts_spice_idx },
    { "ts.furs", sprites_ts_furs_idx },
    { "ts.peat", sprites_ts_peat_idx },
    { "ts.arctic_ivory", sprites_ts_arctic_ivory_idx },
    { "ts.fruit", sprites_ts_fruit_idx },
    { "ts.iron", sprites_ts_iron_idx },
    { "ts.whales", sprites_ts_whales_idx },
    { "ts.wheat", sprites_ts_wheat_idx },
    { "ts.pheasant", sprites_ts_pheasant_idx },
    { "ts.buffalo", sprites_ts_buffalo_idx },
    { "ts.silk", sprites_ts_silk_idx },
    { "ts.wine", sprites_ts_wine_idx },
    { "ts.seals", sprites_ts_seals_idx },
    { "ts.oasis", sprites_ts_oasis_idx },
    { "ts.forest_game", sprites_ts_forest_game_idx },
    { "ts.grassland_resources", sprites_ts_grassland_resources_idx },
    { "ts.coal", sprites_ts_coal_idx },
    { "ts.gems", sprites_ts_gems_idx },
    { "ts.gold", sprites_ts_gold_idx },
    { "ts.fish", sprites_ts_fish_idx },
    { "ts.horses", sprites_ts_horses_idx },
    { "ts.river_resources", sprites_ts_river_resources_idx },
    { "ts.oil", sprites_ts_oil_idx },
    { "ts.tundra_game", sprites_ts_tundra_game_idx },
    { "ts.aluminum", sprites_ts_aluminum_idx },
    { "ts.uranium", sprites_ts_uranium_idx },
    { "ts.saltpeter", sprites_ts_saltpeter_idx },
    { "ts.elephant", sprites_ts_elephant_idx }
};
static const int g_num_resource_sprites = sizeof(g_resource_sprites) / sizeof(g_resource_sprites[0]);

static int get_resource_sprite(const char *graphic_str)
{
    if (!graphic_str || !graphic_str[0]) return -1;
    for (int i = 0; i < g_num_resource_sprites; i++) {
        if (strcasecmp(graphic_str, g_resource_sprites[i].graphic_str) == 0) {
            return g_resource_sprites[i].sprite_idx;
        }
    }
    return -1;
}

// Scale of the map currently being drawn (1.0 = the tileset's native 30px
// tiles). Set by render_map_view() for its helpers below.
static float s_ms = 1.0f;

static void draw_ocean_cell(float draw_x, float draw_y, int dx, int dy, int b0, int b1, int b2, const int* sprites)
{
    int idx = (b2 << 2) | (b1 << 1) | b0;
    C2D_Image img = C2D_SpriteSheetGetImage(g_spriteSheet, sprites[idx]);
    C2D_DrawImageAt(img, draw_x + dx * s_ms, draw_y + dy * s_ms, 0.49f, NULL, s_ms, s_ms);
}

// City banners are drawn after the tile pass so they can overlap neighbours.
typedef struct { float x, y; int owner, size; char name[48]; char prod[32]; } CityBanner;
static CityBanner s_banners[48];
static int s_nbanners;

// Owner shield: a small flag in the player's colour with a dark rim.
static void draw_shield(float x, float y, float z, u32 color)
{
    C2D_DrawRectSolid(x - 1, y - 1, z, 9, 10, C2D_Color32(0x10, 0x10, 0x10, 0xE0));
    C2D_DrawRectSolid(x, y, z + 0.001f, 7, 6, color);
    C2D_DrawRectSolid(x + 1, y + 6, z + 0.001f, 5, 1, color);
    C2D_DrawRectSolid(x + 2, y + 7, z + 0.001f, 3, 1, color);
}

static void draw_tile_entities(const TileInfo *info, float draw_x, float draw_y)
{
    // City takes visual priority over a stacked unit.
    const float S = s_ms;
    const float T = TILE_PX * S;
    if (info->city_owner >= 0) {
        C2D_DrawImageAt(C2D_SpriteSheetGetImage(g_spriteSheet, sprites_city_idx),
                        draw_x, draw_y, 0.6f, NULL, S, S);
        if (s_nbanners < (int)(sizeof(s_banners) / sizeof(s_banners[0]))) {
            CityBanner *b = &s_banners[s_nbanners++];
            b->x = draw_x + T / 2; b->y = draw_y + T - 3;
            b->owner = info->city_owner; b->size = info->city_size;
            snprintf(b->name, sizeof(b->name), "%s", info->city_name);
            if (info->city_prod[0] && info->city_prod_turns > 0 && info->city_prod_turns < 999)
                snprintf(b->prod, sizeof(b->prod), "%.20s %d", info->city_prod, info->city_prod_turns);
            else
                snprintf(b->prod, sizeof(b->prod), "%.20s", info->city_prod);
        }
    } else if (info->unit_owner >= 0) {
        int sprite_idx = get_unit_sprite(info->unit_name);
        C2D_ImageTint dim;
        if (info->unit_done) C2D_PlainImageTint(&dim, C2D_Color32(0x50, 0x50, 0x50, 0xFF), 0.35f);
        C2D_DrawImageAt(C2D_SpriteSheetGetImage(g_spriteSheet, sprite_idx),
                        draw_x, draw_y, 0.6f, info->unit_done ? &dim : NULL, S, S);
        draw_shield(draw_x + 2, draw_y + 2, 0.65f, owner_color(info->unit_owner));
        // Health bar when hurt.
        if (info->unit_hp_max > 0 && info->unit_hp < info->unit_hp_max) {
            float f = (float)info->unit_hp / info->unit_hp_max;
            float bw = T - 8;
            C2D_DrawRectSolid(draw_x + 4, draw_y + T - 4, 0.65f, bw, 3, C2D_Color32(0x20, 0x08, 0x08, 0xE0));
            C2D_DrawRectSolid(draw_x + 4, draw_y + T - 4, 0.651f, bw * f, 3,
                              f > 0.5f ? C2D_Color32(0x50, 0xD0, 0x50, 0xFF) : f > 0.25f ? C2D_Color32(0xE0, 0xC0, 0x40, 0xFF)
                                                                     : C2D_Color32(0xE0, 0x40, 0x40, 0xFF));
        }
        // What it's doing: F(ortified), S(entry), W(orking), E(xploring), G(oto).
        if (info->unit_badge && T >= 20) {
            char t[2] = { info->unit_badge, 0 };
            C2D_DrawRectSolid(draw_x + T - 11, draw_y + T - 13, 0.66f, 10, 11, C2D_Color32(0x10, 0x18, 0x26, 0xD8));
            font_draw(t, draw_x + T - 9.5f, draw_y + T - 16, 0.67f, 0.39f, C2D_Color32(0xFF, 0xF0, 0xA0, 0xFF));
        }
        if (info->num_units > 1) {
            // Stack marker: a small "+" badge, so a stack reads as more than one unit.
            C2D_DrawRectSolid(draw_x + T - 9, draw_y + 1, 0.66f, 8, 8, C2D_Color32(0, 0, 0, 0xC0));
            C2D_DrawRectSolid(draw_x + T - 8, draw_y + 4, 0.67f, 6, 2, 0xFFFFFFFF);
            C2D_DrawRectSolid(draw_x + T - 6, draw_y + 2, 0.67f, 2, 6, 0xFFFFFFFF);
        }
    }
}

// Civ Rev style name plate under each city: [size] Name, owner-coloured edge.
static void draw_city_banners(void)
{
    for (int i = 0; i < s_nbanners; i++) {
        const CityBanner *b = &s_banners[i];
        char sz[8];
        snprintf(sz, sizeof(sz), "%d", b->size);
        float nw = font_width(b->name, 0.42f), sw = font_width(sz, 0.42f);
        float w = nw + sw + 14, h = 14;
        float x = floorf(b->x - w / 2), y = floorf(b->y);
        u32 oc = owner_color(b->owner);
        C2D_DrawRectSolid(x - 1, y - 1, 0.80f, w + 2, h + 2, oc);
        C2D_DrawRectSolid(x, y, 0.801f, w, h, C2D_Color32(0x0C, 0x12, 0x1E, 0xE8));
        C2D_DrawRectSolid(x, y, 0.802f, sw + 6, h, oc);
        font_draw(sz, x + 3, y - 1.5f, 0.803f, 0.42f, contrast_text(oc));
        font_draw(b->name, x + sw + 10, y - 1.5f, 0.803f, 0.42f, 0xFFFFFFFF);
        // Your cities: what's being built and in how many turns, at the
        // closer zoom levels (Civ Rev / Civ VI style).
        if (b->prod[0] && TILE_PX * s_ms >= 22) {
            float pw = font_width(b->prod, 0.38f) + 6, ph = 12;
            float px = floorf(b->x - pw / 2), py = y + h + 1;
            C2D_DrawRectSolid(px, py, 0.80f, pw, ph, C2D_Color32(0x0C, 0x12, 0x1E, 0xD0));
            font_draw(b->prod, px + 3, py - 2.5f, 0.803f, 0.38f, C2D_Color32(0xF0, 0xC0, 0x70, 0xFF));
        }
    }
}

// The cursor / selected tile: a yellow frame, kept at least 2 screen px thick
// at every zoom level.
static void draw_selection(float draw_x, float draw_y, bool armed)
{
    u32 hi = armed ? C2D_Color32(0xFF, 0xF0, 0x30, 0xFF) : C2D_Color32(0xFF, 0xFF, 0xFF, 0xFF);
    float z = 0.7f, T = TILE_PX * s_ms, b = 2.0f;
    C2D_DrawRectSolid(draw_x,         draw_y,         z, T, b, hi); // top
    C2D_DrawRectSolid(draw_x,         draw_y + T - b, z, T, b, hi); // bottom
    C2D_DrawRectSolid(draw_x,         draw_y,         z, b, T, hi); // left
    C2D_DrawRectSolid(draw_x + T - b, draw_y,         z, b, T, hi); // right
}

// Pending-goto target marker - a small cross centered on the destination tile,
// above the selection frame and unit sprites (z 0.75).
static void draw_goto_marker(float draw_x, float draw_y)
{
    u32 c = C2D_Color32(0xFF, 0xA0, 0x20, 0xFF); // orange
    float z = 0.75f;
    float cx = draw_x + TILE_PX * s_ms / 2, cy = draw_y + TILE_PX * s_ms / 2;
    C2D_DrawRectSolid(cx - 5, cy - 2, z, 10, 4, c);
    C2D_DrawRectSolid(cx - 2, cy - 5, z, 4, 10, c);
}

void render_map_view(int cam_x, int cam_y, float scale, int view_w, int view_h,
                     int sel_x, int sel_y, bool sel_armed, int goto_x, int goto_y)
{
    if (!g_spriteSheet) return;

    // While the server is still generating the map, show a status message
    // instead of the empty mock grid (which looked like a broken all-ocean map).
    if (get_game_turn() <= 0) {
        draw_text(view_w / 2 - 90, view_h / 2 - 15, 0.7f, C2D_Color32(0xFF, 0xFF, 0xFF, 0xFF),
                  "Generating world...");
        return;
    }
    s_ms = scale;
    s_nbanners = 0;
    const float SC = scale;
    const float T = TILE_PX * SC; // on-screen tile size

    int map_w = 40, map_h = 40;
    get_map_dimensions(&map_w, &map_h);
    const int coast_u_sprites[8] = {
        sprites_coast_cell_u000_idx, sprites_coast_cell_u100_idx, sprites_coast_cell_u010_idx, sprites_coast_cell_u110_idx,
        sprites_coast_cell_u001_idx, sprites_coast_cell_u101_idx, sprites_coast_cell_u011_idx, sprites_coast_cell_u111_idx
    };
    const int coast_r_sprites[8] = {
        sprites_coast_cell_r000_idx, sprites_coast_cell_r100_idx, sprites_coast_cell_r010_idx, sprites_coast_cell_r110_idx,
        sprites_coast_cell_r001_idx, sprites_coast_cell_r101_idx, sprites_coast_cell_r011_idx, sprites_coast_cell_r111_idx
    };
    const int coast_l_sprites[8] = {
        sprites_coast_cell_l000_idx, sprites_coast_cell_l100_idx, sprites_coast_cell_l010_idx, sprites_coast_cell_l110_idx,
        sprites_coast_cell_l001_idx, sprites_coast_cell_l101_idx, sprites_coast_cell_l011_idx, sprites_coast_cell_l111_idx
    };
    const int coast_d_sprites[8] = {
        sprites_coast_cell_d000_idx, sprites_coast_cell_d100_idx, sprites_coast_cell_d010_idx, sprites_coast_cell_d110_idx,
        sprites_coast_cell_d001_idx, sprites_coast_cell_d101_idx, sprites_coast_cell_d011_idx, sprites_coast_cell_d111_idx
    };

    // Whole-tile cardinal-matched coast "frame" sprite: the main coastline
    // curve, drawn on top of the corner texture fill. Indexed by bit
    // N|E<<1|S<<2|W<<3, where each bit is 1 if that cardinal neighbor is
    // water too (freeciv's MATCH_SAME convention - opposite polarity from
    // the corner-cell quadrants above). Mostly transparent when fully
    // surrounded by water. (trident also ships a layer2 "sea foam" pass on
    // top of this, but its opaque white highlights read as a solid white
    // block at this tile size, so we skip it.)
    const int coast_l1_sprites[16] = {
        sprites_coast_l1_n0e0s0w0_idx, sprites_coast_l1_n1e0s0w0_idx, sprites_coast_l1_n0e1s0w0_idx, sprites_coast_l1_n1e1s0w0_idx,
        sprites_coast_l1_n0e0s1w0_idx, sprites_coast_l1_n1e0s1w0_idx, sprites_coast_l1_n0e1s1w0_idx, sprites_coast_l1_n1e1s1w0_idx,
        sprites_coast_l1_n0e0s0w1_idx, sprites_coast_l1_n1e0s0w1_idx, sprites_coast_l1_n0e1s0w1_idx, sprites_coast_l1_n1e1s0w1_idx,
        sprites_coast_l1_n0e0s1w1_idx, sprites_coast_l1_n1e0s1w1_idx, sprites_coast_l1_n0e1s1w1_idx, sprites_coast_l1_n1e1s1w1_idx
    };

    // WRAPX: the native grid is a cylinder of circumference world_w px. A
    // column can be visible either directly or one wrap over, so map draw_x
    // into [0, world_w) and pull it onto the negative side when that copy is
    // the one on screen. world_w (1440 for 48 tiles) >> screen (320), so each
    // tile shows at most once. Without this a unit/city right at the seam
    // looked "boxed in by a false hard border" (STATUS.md O5).
    // Only the tile rows/columns that can be on screen are walked: at the
    // widest zoom the view holds ~27x16 tiles out of a map of thousands.
    const bool wrapx = map_wraps_x();
    const int world_w = map_w * TILE_PX;
    int ty0 = (int)floorf((float)cam_y / TILE_PX) - 1;
    int ty1 = (int)floorf((cam_y + view_h / SC) / TILE_PX) + 1;
    int tx0 = (int)floorf((float)cam_x / TILE_PX) - 1;
    int tx1 = (int)floorf((cam_x + view_w / SC) / TILE_PX) + 1;
    if (ty0 < 0) ty0 = 0;
    if (ty1 > map_h - 1) ty1 = map_h - 1;
    if (!wrapx) {
        if (tx0 < 0) tx0 = 0;
        if (tx1 > map_w - 1) tx1 = map_w - 1;
    } else if (tx1 - tx0 >= map_w) {
        tx1 = tx0 + map_w - 1;
    }
    for (int y = ty0; y <= ty1; y++) {
        for (int xi = tx0; xi <= tx1; xi++) {
            // On a wrapping map xi runs past the seam; x is the native column.
            int x = wrapx ? ((xi % map_w) + map_w) % map_w : xi;
            (void)world_w;
            // Overhead square grid: tile (x,y) occupies [x*30, y*30]..[+30,+30] world px.
            float draw_x = (xi * TILE_PX - cam_x) * SC;
            float draw_y = (y * TILE_PX - cam_y) * SC;

            if (draw_x <= -T || draw_x >= view_w || draw_y <= -T || draw_y >= view_h) {
                continue;
            }

            TileInfo info;
            get_tile_info(x, y, &info);

            if (info.visibility == 0) {
                // Unexplored (unknown) tile, stays black
                continue;
            }

            if (info.terrain == TERRAIN_OCEAN) {
                // Trident has no flat "whole ocean" sprite; the four coast-cell
                // quadrants below already tile seamlessly to cover the full cell.
                int N  = is_land(x, y - 1, map_w, map_h);
                int S  = is_land(x, y + 1, map_w, map_h);
                int E  = is_land(x + 1, y, map_w, map_h);
                int W  = is_land(x - 1, y, map_w, map_h);
                int NW = is_land(x - 1, y - 1, map_w, map_h);
                int NE = is_land(x + 1, y - 1, map_w, map_h);
                int SW = is_land(x - 1, y + 1, map_w, map_h);
                int SE = is_land(x + 1, y + 1, map_w, map_h);

                draw_ocean_cell(draw_x, draw_y, 0,  0,  W, NW, N, coast_u_sprites);
                draw_ocean_cell(draw_x, draw_y, 15, 0,  N, NE, E, coast_r_sprites);
                draw_ocean_cell(draw_x, draw_y, 0,  15, S, SW, W, coast_l_sprites);
                draw_ocean_cell(draw_x, draw_y, 15, 15, E, SE, S, coast_d_sprites);

                // Whole-tile coastline curve on top of the corner texture fill.
                int whole_idx = (!N) | ((!E) << 1) | ((!S) << 2) | ((!W) << 3);
                C2D_DrawImageAt(C2D_SpriteSheetGetImage(g_spriteSheet, coast_l1_sprites[whole_idx]),
                                draw_x, draw_y, 0.493f, NULL, SC, SC);
            } else if (info.terrain == TERRAIN_GRASSLAND) {
                C2D_DrawImageAt(C2D_SpriteSheetGetImage(g_spriteSheet, sprites_grassland_idx), draw_x, draw_y, 0.5f, NULL, SC, SC);
            } else {
                int N  = is_same_terrain(x, y - 1, map_w, map_h, info.terrain);
                int S  = is_same_terrain(x, y + 1, map_w, map_h, info.terrain);
                int E  = is_same_terrain(x + 1, y, map_w, map_h, info.terrain);
                int W  = is_same_terrain(x - 1, y, map_w, map_h, info.terrain);
                int match_idx = N | (E << 1) | (S << 2) | (W << 3);
                int sprite_idx = terrain_match_sprites[info.terrain][match_idx];
                C2D_DrawImageAt(C2D_SpriteSheetGetImage(g_spriteSheet, sprite_idx), draw_x, draw_y, 0.5f, NULL, SC, SC);
            }

            // Draw overlays on top of the terrain (except on raw ocean tiles)
            if (info.terrain != TERRAIN_OCEAN) {
                // 1. Irrigation / Farmland
                if (info.has_farmland) {
                    C2D_DrawImageAt(C2D_SpriteSheetGetImage(g_spriteSheet, sprites_tx_farmland_idx), draw_x, draw_y, 0.51f, NULL, SC, SC);
                } else if (info.has_irrigation) {
                    C2D_DrawImageAt(C2D_SpriteSheetGetImage(g_spriteSheet, sprites_tx_irrigation_idx), draw_x, draw_y, 0.51f, NULL, SC, SC);
                }

                // 2. River
                if (info.has_river) {
                    int rN = has_river_at(x, y - 1);
                    int rS = has_river_at(x, y + 1);
                    int rE = has_river_at(x + 1, y);
                    int rW = has_river_at(x - 1, y);
                    int r_idx = rN | (rE << 1) | (rS << 2) | (rW << 3);
                    C2D_DrawImageAt(C2D_SpriteSheetGetImage(g_spriteSheet, river_sprites[r_idx]), draw_x, draw_y, 0.52f, NULL, SC, SC);
                }

                // 3. Roads / Railroads / Maglevs
                if (info.has_maglev) {
                    int mN = has_maglev_at(x, y - 1);
                    int mS = has_maglev_at(x, y + 1);
                    int mE = has_maglev_at(x + 1, y);
                    int mW = has_maglev_at(x - 1, y);
                    int m_idx = mN | (mE << 1) | (mS << 2) | (mW << 3);
                    C2D_DrawImageAt(C2D_SpriteSheetGetImage(g_spriteSheet, maglev_sprites[m_idx]), draw_x, draw_y, 0.53f, NULL, SC, SC);
                } else if (info.has_rail) {
                    int rN = has_rail_at(x, y - 1);
                    int rS = has_rail_at(x, y + 1);
                    int rE = has_rail_at(x + 1, y);
                    int rW = has_rail_at(x - 1, y);
                    int r_idx = rN | (rE << 1) | (rS << 2) | (rW << 3);
                    C2D_DrawImageAt(C2D_SpriteSheetGetImage(g_spriteSheet, rail_sprites[r_idx]), draw_x, draw_y, 0.53f, NULL, SC, SC);
                } else if (info.has_road) {
                    int rN = has_road_at(x, y - 1);
                    int rS = has_road_at(x, y + 1);
                    int rE = has_road_at(x + 1, y);
                    int rW = has_road_at(x - 1, y);
                    int r_idx = rN | (rE << 1) | (rS << 2) | (rW << 3);
                    C2D_DrawImageAt(C2D_SpriteSheetGetImage(g_spriteSheet, road_sprites[r_idx]), draw_x, draw_y, 0.53f, NULL, SC, SC);
                }

                // 4. Mine
                if (info.has_mine) {
                    C2D_DrawImageAt(C2D_SpriteSheetGetImage(g_spriteSheet, sprites_tx_mine_idx), draw_x, draw_y, 0.54f, NULL, SC, SC);
                }

                // 5. Resource / Special
                if (info.resource_tag[0]) {
                    int res_sprite = get_resource_sprite(info.resource_tag);
                    if (res_sprite >= 0) {
                        C2D_DrawImageAt(C2D_SpriteSheetGetImage(g_spriteSheet, res_sprite), draw_x, draw_y, 0.55f, NULL, SC, SC);
                    }
                }

                // 6. Goody Hut / Village
                if (info.has_village) {
                    C2D_DrawImageAt(C2D_SpriteSheetGetImage(g_spriteSheet, sprites_tx_village_idx), draw_x, draw_y, 0.56f, NULL, SC, SC);
                }

                // 7. Pollution / Fallout
                if (info.has_pollution) {
                    C2D_DrawImageAt(C2D_SpriteSheetGetImage(g_spriteSheet, sprites_tx_pollution_idx), draw_x, draw_y, 0.57f, NULL, SC, SC);
                } else if (info.has_fallout) {
                    C2D_DrawImageAt(C2D_SpriteSheetGetImage(g_spriteSheet, sprites_tx_fallout_idx), draw_x, draw_y, 0.57f, NULL, SC, SC);
                }
            }

            // Draw Fog of War overlay (tx.fog) if known unseen (fogged)
            if (info.visibility == 1) {
                C2D_DrawImageAt(C2D_SpriteSheetGetImage(g_spriteSheet, sprites_tx_fog_idx), draw_x, draw_y, 0.58f, NULL, SC, SC);
            }

            // Draw Darkness overlay if any neighbor is unknown
            if (info.darkness_idx > 0) {
                int darkness_sprite = darkness_sprites[info.darkness_idx];
                C2D_DrawImageAt(C2D_SpriteSheetGetImage(g_spriteSheet, darkness_sprite), draw_x, draw_y, 0.59f, NULL, SC, SC);
            }

            // National borders: a thin line in the owner's colour on each edge
            // that faces another owner (or no owner). Drawn as rectangles; the
            // grid.borders sprites cut from the tileset carry stray pixels.
            if (info.tile_owner != -1) {
                u32 bc = (owner_color(info.tile_owner) & 0x00FFFFFF) | 0xD0000000;
                float tw = TILE_PX * SC, lw = SC >= 1.0f ? 2.0f : 1.0f;
                if (info.border_n) C2D_DrawRectSolid(draw_x, draw_y, 0.6f, tw, lw, bc);
                if (info.border_s) C2D_DrawRectSolid(draw_x, draw_y + tw - lw, 0.6f, tw, lw, bc);
                if (info.border_w) C2D_DrawRectSolid(draw_x, draw_y, 0.6f, lw, tw, bc);
                if (info.border_e) C2D_DrawRectSolid(draw_x + tw - lw, draw_y, 0.6f, lw, tw, bc);
            }

            // Draw entities based on visibility
            if (info.visibility == 2) {
                draw_tile_entities(&info, draw_x, draw_y);
            } else {
                // Under Fog of War (visibility == 1), we still draw city centers but no units
                if (info.city_owner != -1) {
                    C2D_DrawImageAt(C2D_SpriteSheetGetImage(g_spriteSheet, sprites_city_idx), draw_x, draw_y, 0.7f, NULL, SC, SC);
                    if (s_nbanners < (int)(sizeof(s_banners) / sizeof(s_banners[0]))) {
                        CityBanner *b = &s_banners[s_nbanners++];
                        b->x = draw_x + T / 2; b->y = draw_y + T - 3;
                        b->owner = info.city_owner; b->size = info.city_size;
                        snprintf(b->name, sizeof(b->name), "%s", info.city_name);
                        b->prod[0] = 0; // fogged: no production shown (and none left over)
                    }
                }
            }

            if (x == sel_x && y == sel_y) {
                draw_selection(draw_x, draw_y, sel_armed);
            }
            if (x == goto_x && y == goto_y) {
                draw_goto_marker(draw_x, draw_y);
            }
        }
    }

    draw_city_banners();

    // The cursor on an unexplored (black) tile still needs to be visible.
    if (sel_x >= 0 && sel_y >= 0) {
        TileInfo si;
        get_tile_info(sel_x, sel_y, &si);
        if (si.visibility == 0) {
            int wx = sel_x * TILE_PX;
            if (wrapx) {
                // the copy of the column at or just right of the view's left edge
                int d = ((wx - cam_x + TILE_PX) % world_w + world_w) % world_w - TILE_PX;
                wx = cam_x + d;
            }
            draw_selection((wx - cam_x) * SC, (sel_y * TILE_PX - cam_y) * SC, sel_armed);
        }
    }
    // Same for a route target in the dark.
    if (goto_x >= 0 && goto_y >= 0) {
        TileInfo gi;
        get_tile_info(goto_x, goto_y, &gi);
        if (gi.visibility == 0) {
            int wx = goto_x * TILE_PX;
            if (wrapx) {
                int d = ((wx - cam_x + TILE_PX) % world_w + world_w) % world_w - TILE_PX;
                wx = cam_x + d;
            }
            draw_goto_marker((wx - cam_x) * SC, (goto_y * TILE_PX - cam_y) * SC);
        }
    }
}

// Garrison rows visible at once on the top-screen readout before falling
// back to a "+N more" tail - the panel isn't scrollable (it's a non-touch,
// D-Pad-free readout; L/R already drive garrison_sel, see main.c), so a
// selection past this window scrolls the *window*, not the list.
#define CITY_GARRISON_VISIBLE 3

static void progress_bar(float x, float y, float w, float h, float frac, u32 fg)
{
    if (frac < 0) frac = 0;
    if (frac > 1) frac = 1;
    C2D_DrawRectSolid(x, y, 0.0f, w, h, C2D_Color32(0x22, 0x2C, 0x3A, 0xFF));
    C2D_DrawRectSolid(x, y, 0.01f, w * frac, h, fg);
}

void render_city_top(const CityInfo *info, int garrison_sel)
{
    C2D_TextBufClear(g_textBuf);

    u32 white = C2D_Color32(0xFF, 0xFF, 0xFF, 0xFF);
    u32 soft  = C2D_Color32(0xC0, 0xCE, 0xDC, 0xFF);
    u32 label = C2D_Color32(0x8C, 0xA0, 0xB4, 0xFF);
    u32 food  = C2D_Color32(0x70, 0xD0, 0x60, 0xFF);
    u32 shld  = C2D_Color32(0xE0, 0x98, 0x50, 0xFF);
    u32 trade = C2D_Color32(0xF0, 0xD8, 0x50, 0xFF);
    char line[160];

    // Header: owner colour band, name, size badge.
    u32 oc = owner_color(info->owner);
    C2D_DrawRectSolid(0, 0, 0.0f, 400, 32, C2D_Color32(0x14, 0x1E, 0x2E, 0xFF));
    C2D_DrawRectSolid(0, 0, 0.01f, 6, 32, oc);
    C2D_DrawRectSolid(0, 32, 0.0f, 400, 1, C2D_Color32(0x6C, 0xA8, 0xE0, 0xFF));
    draw_text(14, 3, 0.85f, white, info->name[0] ? info->name : "?");
    snprintf(line, sizeof(line), "Size %d", info->size);
    float sw = text_width(line, 0.55f);
    C2D_DrawRectSolid(392 - sw - 12, 6, 0.01f, sw + 12, 20, oc);
    draw_text_z(392 - sw - 6, 4, 0.02f, 0.55f, contrast_text(oc), line);
    // Mood, left of the size badge: disorder is the classic early surprise.
    {
        char mood[64];
        float right = 392 - sw - 20, nw = text_width(info->name[0] ? info->name : "?", 0.85f) + 22;
        if (info->disorder) {
            snprintf(mood, sizeof(mood), "DISORDER");
            float mw = text_width(mood, 0.5f) + 10;
            C2D_DrawRectSolid(right - mw, 7, 0.01f, mw, 18, C2D_Color32(0xC0, 0x30, 0x28, 0xFF));
            draw_text_z(right - mw + 5, 6, 0.02f, 0.5f, 0xFFFFFFFF, mood);
        } else {
            snprintf(mood, sizeof(mood), "%d happy  %d content  %d unhappy",
                     info->happy, info->content, info->unhappy + info->angry);
            float ms = 0.45f;
            while (ms > 0.4f && text_width(mood, ms) > right - nw) ms -= 0.01f;
            if (text_width(mood, ms) <= right - nw)
                draw_text_z(right - text_width(mood, ms), 8, 0.02f, ms,
                            info->unhappy + info->angry > info->happy ? C2D_Color32(0xFF, 0xB0, 0x70, 0xFF) : soft, mood);
        }
    }

    // Left column: yields, growth, production.
    layer(2.5f);
    float x = 12, y = 42;
    struct { const char *n; int v; u32 c; } yv[6] = {
        { "Food", info->surplus[0], food }, { "Shields", info->surplus[1], shld },
        { "Trade", info->surplus[2], trade }, { "Gold", info->surplus[3], C2D_Color32(0xFF, 0xD0, 0x40, 0xFF) },
        { "Luxury", info->surplus[4], C2D_Color32(0xE0, 0x90, 0xE0, 0xFF) }, { "Science", info->surplus[5], C2D_Color32(0x70, 0xB8, 0xFF, 0xFF) },
    };
    for (int i = 0; i < 6; i++) {
        float cx = x + (i % 2) * 116, cy = y - 4 + (i / 2) * 16;
        C2D_DrawRectSolid(cx, cy + 5, 0.0f, 8, 8, yv[i].c);
        snprintf(line, sizeof(line), "%s %+d", yv[i].n, yv[i].v);
        draw_text(cx + 12, cy, 0.45f, soft, line);
    }

    y = 94;
    draw_text(x, y, 0.45f, label, "Growth");
    snprintf(line, sizeof(line), "%d/%d", info->food_stock, info->food_needed);
    draw_text(x + 160, y, 0.45f, soft, line);
    progress_bar(x, y + 18, 224, 6, info->food_needed > 0 ? (float)info->food_stock / info->food_needed : 0, food);
    if (info->grow_turns >= 0) snprintf(line, sizeof(line), "Grows in %d turn%s", info->grow_turns, info->grow_turns == 1 ? "" : "s");
    else snprintf(line, sizeof(line), "Not growing");
    draw_text(x, y + 25, 0.42f, info->grow_turns >= 0 ? soft : C2D_Color32(0xE0, 0x80, 0x60, 0xFF), line);

    y = 142;
    draw_text(x, y, 0.45f, label, "Building");
    draw_text(x, y + 15, 0.55f, white, info->producing[0] ? info->producing : "?");
    progress_bar(x, y + 37, 224, 6, info->prod_cost > 0 ? (float)info->shield_stock / info->prod_cost : 0, shld);
    snprintf(line, sizeof(line), "%d/%d shields, %d turn%s", info->shield_stock, info->prod_cost,
             info->prod_turns, info->prod_turns == 1 ? "" : "s");
    draw_text(x, y + 44, 0.42f, soft, line);
    if (info->can_buy) {
        snprintf(line, sizeof(line), "Buy now: %d gold", info->buy_cost);
        draw_text(x, y + 60, 0.42f, C2D_Color32(0x70, 0xE0, 0x70, 0xFF), line);
    }

    // Right column: units in the city (Y leads the highlighted one out).
    layer(4.0f);
    float rx = 252;
    C2D_DrawRectSolid(rx - 6, 38, 0.0f, 152, 172, C2D_Color32(0x10, 0x18, 0x26, 0xFF));
    snprintf(line, sizeof(line), "Units here (%d)", info->garrison_count);
    draw_text(rx, 40, 0.45f, label, line);
    if (info->garrison_count == 0) {
        draw_text(rx, 62, 0.45f, soft, "None");
    } else {
        int per = 5;
        int window = (garrison_sel / per) * per;
        for (int i = 0; i < per && window + i < info->garrison_count; i++) {
            int idx = window + i;
            float uy = 60 + i * 29;
            bool sel = idx == garrison_sel;
            if (sel) C2D_DrawRectSolid(rx - 4, uy - 1, 0.01f, 148, 28, C2D_Color32(0x34, 0x50, 0x78, 0xFF));
            C2D_DrawImageAt(C2D_SpriteSheetGetImage(g_spriteSheet, get_unit_sprite(info->garrison_names[idx])),
                            rx - 2, uy - 2, 0.02f, NULL, 1.0f, 1.0f);
            draw_text_z(rx + 32, uy + 3, 0.03f, 0.47f, sel ? C2D_Color32(0xFF, 0xE8, 0x80, 0xFF) : soft,
                        info->garrison_names[idx]);
        }
        if (window + per < info->garrison_count) {
            snprintf(line, sizeof(line), "+%d more", info->garrison_count - window - per);
            draw_text(rx, 196, 0.42f, label, line);
        }
    }

    layer(0.0f);
    C2D_DrawRectSolid(0, 240 - 17, 0.0f, 400, 17, C2D_Color32(0x0C, 0x12, 0x1C, 0xFF));
    draw_text(5, 240 - 16, 0.45f, soft, info->garrison_count > 0
              ? "L/R: other cities   Y: lead the first unit out   B: close"
              : "L/R: other cities   B: close");
}

void draw_scroll_list(int x, const ScrollRow *rows, int count)
{
    for (int i = 0; i < count; i++) {
        float y = PANEL_LIST_Y + i * PANEL_ROW_H;
        draw_text(x, y + 4, 0.45f, rows[i].color, rows[i].text);
    }
}

static void scroll_thumb(int n, int scroll, float x);
static int s_city_units_n;
static bool s_buy_armed;
static int s_gov_armed = -1;
void render_set_buy_armed(bool armed) { s_buy_armed = armed; }
void render_set_gov_armed(int idx) { s_gov_armed = idx; }

void render_city_tabs(int tab)
{
    char units[16];
    snprintf(units, sizeof(units), "Units %d", s_city_units_n);
    const char *names[CITY_TAB_COUNT] = { "Build", "Tiles", "Buildings", units };
    for (int i = 0; i < CITY_TAB_COUNT; i++)
        draw_button(CITY_TAB_X(i), CITY_TAB_Y, CITY_TAB_W, CITY_TAB_H, 0.03f, names[i], 0.45f, true, i == tab);
}

int city_units_hit(int py, int scroll, int n)
{
    if (py < PANEL_LIST_Y || py >= PANEL_FOOTER_Y) return -1;
    int r = scroll + (py - PANEL_LIST_Y) / CITY_UNIT_ROW_H;
    return r < n ? r : -1;
}

void render_city_units_panel(const CityInfo *info, int scroll)
{
    C2D_TextBufClear(g_textBuf);
    u32 white = C2D_Color32(0xF0, 0xF0, 0xF0, 0xFF);
    u32 soft  = C2D_Color32(0xA8, 0xB4, 0xC4, 0xFF);
    s_city_units_n = info->garrison_count;
    C2D_DrawRectSolid(0, 0, 0.0f, 320, 240, C2D_Color32(0x10, 0x16, 0x22, 0xFF));
    C2D_DrawRectSolid(0, 0, 0.01f, 320, PANEL_HEADER_H, C2D_Color32(0x2C, 0x50, 0x80, 0xFF));
    draw_text_z(8, 3, 0.02f, 0.6f, white, info->name[0] ? info->name : "?");
    draw_text(8, PANEL_HEADER_H + 4, 0.45f, soft,
              info->garrison_count ? "Tap a unit to command it on the map." : "No units in this city.");
    render_city_tabs(CITY_TAB_UNITS);
    int rows = (PANEL_FOOTER_Y - PANEL_LIST_Y) / CITY_UNIT_ROW_H;
    for (int i = 0; i < rows; i++) {
        int idx = scroll + i;
        if (idx >= info->garrison_count) break;
        float y = PANEL_LIST_Y + i * CITY_UNIT_ROW_H;
        C2D_DrawRectSolid(4, y + 1, 0.0f, 306, CITY_UNIT_ROW_H - 2,
                          (i & 1) ? C2D_Color32(0x1A, 0x24, 0x34, 0xFF) : C2D_Color32(0x20, 0x2C, 0x3E, 0xFF));
        C2D_DrawImageAt(C2D_SpriteSheetGetImage(g_spriteSheet, get_unit_sprite(info->garrison_names[idx])),
                        8, y + 1, 0.01f, NULL, (CITY_UNIT_ROW_H - 3) / (float)TILE_PX, (CITY_UNIT_ROW_H - 3) / (float)TILE_PX);
        draw_text_z(36, y + 3, 0.01f, 0.47f, white, info->garrison_names[idx]);
        const char *st = info->garrison_status[idx];
        bool ready = !strcmp(st, "Ready");
        draw_text_z(304 - text_width(st, 0.42f), y + 4, 0.01f, 0.42f,
                    ready ? C2D_Color32(0x90, 0xE8, 0x90, 0xFF) : soft, st);
    }
    if (info->garrison_count > rows) scroll_thumb(info->garrison_count, scroll, 313);
    draw_button(4, PANEL_FOOTER_Y + 2, 312, 240 - PANEL_FOOTER_Y - 4, 0.02f, "Close", 0.5f, true, false);
}

static u32 terrain_color(int t)
{
    static const u32 c[11] = {
        0xFF663300, 0xFF3B7A3B, 0xFF4F9B8B, 0xFF8CB4D2, 0xFF909A80, 0xFF304020,
        0xFF1B4D1B, 0xFF3B6D5E, 0xFF225522, 0xFF606060, 0xFFFFF0E0 };
    return (t >= 0 && t < 11) ? c[t] : 0xFF000000;   // ABGR
}

static float grid_cell(int radius) { return (float)(int)(CITY_GRID_SIZE / (2 * radius + 1)); }

bool city_grid_hit(int radius, int px, int py, int *rx, int *ry)
{
    if (radius <= 0) return false;
    float c = grid_cell(radius);
    int gx = (int)((px - CITY_GRID_X) / c), gy = (int)((py - CITY_GRID_Y) / c);
    if (px < CITY_GRID_X || py < CITY_GRID_Y || gx > 2 * radius || gy > 2 * radius) return false;
    *rx = gx - radius; *ry = gy - radius;
    return true;
}

void render_city_tiles_panel(const CityInfo *info, const CityTileInfo *tiles, int n, int radius,
                             const SpecialistInfo *spec, int nspec)
{
    C2D_TextBufClear(g_textBuf);
    u32 white = C2D_Color32(0xF0, 0xF0, 0xF0, 0xFF);
    u32 soft  = C2D_Color32(0xA8, 0xB4, 0xC4, 0xFF);
    char line[64];
    C2D_DrawRectSolid(0, 0, 0.0f, 320, 240, C2D_Color32(0x10, 0x16, 0x22, 0xFF));
    C2D_DrawRectSolid(0, 0, 0.01f, 320, PANEL_HEADER_H, C2D_Color32(0x2C, 0x50, 0x80, 0xFF));
    draw_text_z(8, 3, 0.02f, 0.6f, white, info->name[0] ? info->name : "?");
    snprintf(line, sizeof(line), "Food %+d  Shields %+d  Trade %+d", info->surplus[0], info->surplus[1], info->surplus[2]);
    draw_text(8, PANEL_HEADER_H + 4, 0.45f, soft, line);
    s_city_units_n = info->garrison_count;
    render_city_tabs(CITY_TAB_TILES);

    float c = grid_cell(radius > 0 ? radius : 2);
    for (int i = 0; i < n; i++) {
        const CityTileInfo *t = &tiles[i];
        float x = CITY_GRID_X + (t->rx + radius) * c, y = CITY_GRID_Y + (t->ry + radius) * c;
        C2D_DrawRectSolid(x, y, 0.0f, c - 1, c - 1, terrain_color(t->terrain));
        if (t->state == 3) C2D_DrawRectSolid(x, y, 0.01f, c - 1, c - 1, C2D_Color32(0, 0, 0, 0xB0));
        if (t->state == 1 || t->state == 2) {
            u32 hc = t->state == 2 ? C2D_Color32(0xFF, 0xFF, 0xFF, 0xFF) : C2D_Color32(0xFF, 0xE0, 0x50, 0xFF);
            C2D_DrawRectSolid(x, y, 0.02f, c - 1, 2, hc);
            C2D_DrawRectSolid(x, y + c - 3, 0.02f, c - 1, 2, hc);
            C2D_DrawRectSolid(x, y, 0.02f, 2, c - 1, hc);
            C2D_DrawRectSolid(x + c - 3, y, 0.02f, 2, c - 1, hc);
        }
        if (t->state == 2) {
            C2D_DrawImageAt(C2D_SpriteSheetGetImage(g_spriteSheet, sprites_city_idx), x + 1, y + 1, 0.03f, NULL,
                            (c - 3) / TILE_PX, (c - 3) / TILE_PX);
        }
        // food / shields / trade digits
        if (t->state != 3) {
            char d[4];
            u32 col[3] = { C2D_Color32(0x60, 0xFF, 0x60, 0xFF), C2D_Color32(0xFF, 0x98, 0x38, 0xFF),
                           C2D_Color32(0xFF, 0xE8, 0x30, 0xFF) };
            int v[3] = { t->food, t->shield, t->trade };
            // One dark strip, three spaced digits; zeros are dimmed so the
            // non-zero yields stand out.
            C2D_DrawRectSolid(x + 1, y + c - 12, 0.035f, c - 3, 10, C2D_Color32(0, 0, 0, 0xA0));
            int bias = font_get_bias(); // Large text would overflow the cell
            font_set_bias(0);
            for (int k = 0; k < 3; k++) {
                snprintf(d, sizeof(d), "%d", v[k] > 9 ? 9 : v[k]);
                float dx = x + 2 + k * 7;
                font_draw(d, dx, y + c - 16, 0.04f, 0.34f, v[k] ? col[k] : C2D_Color32(0x50, 0x50, 0x50, 0xFF));
            }
            font_set_bias(bias);
        }
    }
    draw_text(CITY_SPEC_X, CITY_GRID_Y, 0.42f, soft, "Tap land to work it or free it.");
    for (int i = 0; i < nspec; i++) {
        float y = CITY_SPEC_Y + i * CITY_SPEC_H;
        C2D_DrawRectSolid(CITY_SPEC_X, y, 0.0f, CITY_SPEC_W, CITY_SPEC_H - 3, C2D_Color32(0x1A, 0x24, 0x34, 0xFF));
        draw_text_z(CITY_SPEC_X + 6, y + 1, 0.01f, 0.47f, white, spec[i].name);
        snprintf(line, sizeof(line), "%d", spec[i].count);
        draw_text_z(CITY_SPEC_X + CITY_SPEC_W - 8 - text_width(line, 0.5f), y, 0.01f, 0.5f,
                    spec[i].count ? C2D_Color32(0xFF, 0xE0, 0x60, 0xFF) : soft, line);
    }
    if (nspec > 1) draw_text(CITY_SPEC_X, CITY_SPEC_Y + nspec * CITY_SPEC_H, 0.4f, soft, "Tap a specialist to switch one.");
    draw_button(4, PANEL_FOOTER_Y + 2, 312, 240 - PANEL_FOOTER_Y - 4, 0.02f, "Close", 0.5f, true, false);
}

void render_city_buildings_panel(const CityInfo *info, const BuildingInfo *b, int n, int scroll, int armed)
{
    C2D_TextBufClear(g_textBuf);
    u32 white = C2D_Color32(0xF0, 0xF0, 0xF0, 0xFF);
    u32 soft  = C2D_Color32(0xA8, 0xB4, 0xC4, 0xFF);
    char line[48];
    C2D_DrawRectSolid(0, 0, 0.0f, 320, 240, C2D_Color32(0x10, 0x16, 0x22, 0xFF));
    C2D_DrawRectSolid(0, 0, 0.01f, 320, PANEL_HEADER_H, C2D_Color32(0x2C, 0x50, 0x80, 0xFF));
    draw_text_z(8, 3, 0.02f, 0.6f, white, info->name[0] ? info->name : "?");
    snprintf(line, sizeof(line), "%d building%s", n, n == 1 ? "" : "s");
    draw_text(8, PANEL_HEADER_H + 4, 0.45f, soft, n ? line : "Nothing built yet.");
    s_city_units_n = info->garrison_count;
    render_city_tabs(CITY_TAB_BUILDINGS);
    for (int i = 0; i < PANEL_ROWS_VISIBLE; i++) {
        int idx = scroll + i;
        if (idx >= n) break;
        float y = PANEL_LIST_Y + i * PANEL_ROW_H;
        bool arm = idx == armed;
        C2D_DrawRectSolid(4, y + 1, 0.0f, 306, PANEL_ROW_H - 2,
                          arm ? C2D_Color32(0x6A, 0x24, 0x20, 0xFF) : (i & 1) ? C2D_Color32(0x1A, 0x24, 0x34, 0xFF)
                                                                       : C2D_Color32(0x20, 0x2C, 0x3E, 0xFF));
        draw_text_z(12, y + 3, 0.01f, 0.47f, white, b[idx].name);
        if (arm) snprintf(line, sizeof(line), "tap again: sell %dg", b[idx].sell_gold);
        else if (b[idx].can_sell) snprintf(line, sizeof(line), "sell %dg", b[idx].sell_gold);
        else line[0] = 0;
        if (line[0]) draw_text_z(304 - text_width(line, 0.4f), y + 4, 0.01f, 0.4f,
                                 arm ? C2D_Color32(0xFF, 0xC0, 0xB0, 0xFF) : soft, line);
    }
    draw_button(4, PANEL_FOOTER_Y + 2, 312, 240 - PANEL_FOOTER_Y - 4, 0.02f, "Close", 0.5f, true, false);
    (void)scroll;
}

void render_city_panel(const CityInfo *info, const BuildOption *opts, int n, int scroll,
                       const WorklistEntry *worklist, int worklist_n, bool queue_mode)
{
    C2D_TextBufClear(g_textBuf);

    u32 white = C2D_Color32(0xF0, 0xF0, 0xF0, 0xFF);
    u32 soft  = C2D_Color32(0xA8, 0xB4, 0xC4, 0xFF);
    u32 cur   = C2D_Color32(0xFF, 0xE0, 0x60, 0xFF);
    char line[96];

    C2D_DrawRectSolid(0, 0, 0.0f, 320, 240, C2D_Color32(0x10, 0x16, 0x22, 0xFF));
    C2D_DrawRectSolid(0, 0, 0.01f, 320, PANEL_HEADER_H, C2D_Color32(0x2C, 0x50, 0x80, 0xFF));
    draw_text_z(8, 3, 0.02f, 0.6f, white, info->name[0] ? info->name : "?");

    // NOW/QUEUE toggle: what tapping a buildable row does.
    // Two-part switch: what tapping a row does (lit = active).
    draw_button(CITY_TOGGLE_X, CITY_TOGGLE_Y, CITY_TOGGLE_W / 2 - 1, CITY_TOGGLE_H, 0.03f,
                "Build", 0.42f, true, !queue_mode);
    draw_button(CITY_TOGGLE_X + CITY_TOGGLE_W / 2 + 1, CITY_TOGGLE_Y, CITY_TOGGLE_W / 2 - 1, CITY_TOGGLE_H, 0.03f,
                "Queue", 0.42f, true, queue_mode);

    snprintf(line, sizeof(line), "%s  %d/%d  (%d turn%s)", info->producing[0] ? info->producing : "?",
             info->shield_stock, info->prod_cost, info->prod_turns, info->prod_turns == 1 ? "" : "s");
    draw_text(8, PANEL_HEADER_H + 4, 0.47f, cur, line);
    s_city_units_n = info->garrison_count;
    render_city_tabs(CITY_TAB_BUILD);
    if (!worklist_n) draw_text(CITY_WORKLIST_COL_X + 4, PANEL_LIST_Y + 2, 0.42f, soft, "Queue empty");

    // Buildables (left), as tappable rows.
    for (int i = 0; i < PANEL_ROWS_VISIBLE; i++) {
        int idx = scroll + i;
        if (idx >= n) break;
        const BuildOption *opt = &opts[idx];
        float y = PANEL_LIST_Y + i * PANEL_ROW_H;
        C2D_DrawRectSolid(4, y + 1, 0.0f, CITY_WORKLIST_COL_X - 12, PANEL_ROW_H - 2,
                          opt->is_current ? C2D_Color32(0x3A, 0x4A, 0x20, 0xFF)
                                          : (i & 1) ? C2D_Color32(0x1A, 0x24, 0x34, 0xFF) : C2D_Color32(0x20, 0x2C, 0x3E, 0xFF));
        char nm[32];
        snprintf(nm, sizeof(nm), "%.20s", opt->name);
        draw_text_z(10, y + 3, 0.01f, 0.45f, opt->is_current ? cur : white, nm);
        // Turns first (what players plan by), as in the Civ games.
        if (opt->cost < 0) snprintf(line, sizeof(line), "gold");
        else if (opt->turns > 0 && opt->turns < 999) snprintf(line, sizeof(line), "%d turn%s", opt->turns, opt->turns == 1 ? "" : "s");
        else snprintf(line, sizeof(line), "%d", opt->cost);
        draw_text_z(CITY_WORKLIST_COL_X - 12 - text_width(line, 0.42f), y + 4, 0.01f, 0.42f, soft, line);
    }
    if (n > PANEL_ROWS_VISIBLE) {
        // scroll position
        float track = PANEL_ROW_H * PANEL_ROWS_VISIBLE;
        float th = track * PANEL_ROWS_VISIBLE / n, ty = track * scroll / n;
        C2D_DrawRectSolid(CITY_WORKLIST_COL_X - 7, PANEL_LIST_Y + ty, 0.0f, 3, th, C2D_Color32(0x6C, 0xA8, 0xE0, 0xFF));
    }

    // Worklist (right).
    for (int i = 0; i < PANEL_ROWS_VISIBLE && i < worklist_n; i++) {
        float y = PANEL_LIST_Y + i * PANEL_ROW_H;
        C2D_DrawRectSolid(CITY_WORKLIST_COL_X, y + 1, 0.0f, 316 - CITY_WORKLIST_COL_X, PANEL_ROW_H - 2,
                          C2D_Color32(0x2A, 0x24, 0x18, 0xFF));
        char nm[32];
        snprintf(nm, sizeof(nm), "%d. %.12s", i + 1, worklist[i].name);
        draw_text_z(CITY_WORKLIST_COL_X + 4, y + 3, 0.01f, 0.42f, soft, nm);
    }

    // Footer: Buy (left half) / Close (right half) - matches main.c's hit test.
    if (info->can_buy && s_buy_armed) snprintf(line, sizeof(line), "Tap again: %d gold", info->buy_cost);
    else if (info->can_buy) snprintf(line, sizeof(line), "Buy (%d gold)", info->buy_cost);
    else snprintf(line, sizeof(line), "Buy");
    draw_button(4, PANEL_FOOTER_Y + 2, 152, 240 - PANEL_FOOTER_Y - 4, 0.02f, line, 0.5f, info->can_buy, false);
    draw_button(164, PANEL_FOOTER_Y + 2, 152, 240 - PANEL_FOOTER_Y - 4, 0.02f, "Close", 0.5f, true, false);
}

// Shared look for the bottom-screen panels.
static void panel_bg(const char *title, const char *right)
{
    C2D_DrawRectSolid(0, 0, 0.0f, 320, 240, C2D_Color32(0x10, 0x16, 0x22, 0xFF));
    C2D_DrawRectSolid(0, 0, 0.01f, 320, PANEL_HEADER_H, C2D_Color32(0x2C, 0x50, 0x80, 0xFF));
    C2D_DrawRectSolid(0, PANEL_HEADER_H, 0.01f, 320, 1, C2D_Color32(0x6C, 0xA8, 0xE0, 0xFF));
    draw_text_z(8, 3, 0.02f, 0.6f, 0xFFFFFFFF, title);
    if (right) draw_text_z(312 - text_width(right, 0.45f), 6, 0.02f, 0.45f, C2D_Color32(0xC8, 0xD8, 0xE8, 0xFF), right);
}

static void list_row(int i, float x, float w, bool highlight, bool enabled)
{
    float y = PANEL_LIST_Y + i * PANEL_ROW_H;
    u32 c = highlight ? C2D_Color32(0x3A, 0x4A, 0x20, 0xFF)
          : !enabled  ? C2D_Color32(0x16, 0x1A, 0x22, 0xFF)
          : (i & 1)   ? C2D_Color32(0x1A, 0x24, 0x34, 0xFF) : C2D_Color32(0x20, 0x2C, 0x3E, 0xFF);
    C2D_DrawRectSolid(x, y + 1, 0.0f, w, PANEL_ROW_H - 2, c);
}

static void scroll_thumb(int n, int scroll, float x)
{
    if (n <= PANEL_ROWS_VISIBLE) return;
    float track = PANEL_ROW_H * PANEL_ROWS_VISIBLE;
    C2D_DrawRectSolid(x, PANEL_LIST_Y, 0.0f, 3, track, C2D_Color32(0x22, 0x2C, 0x3A, 0xFF));
    C2D_DrawRectSolid(x, PANEL_LIST_Y + track * scroll / n, 0.01f, 3, track * PANEL_ROWS_VISIBLE / n,
                      C2D_Color32(0x6C, 0xA8, 0xE0, 0xFF));
}

void render_civ_top(const CivInfo *civ, const char *title)
{
    C2D_TextBufClear(g_textBuf);

    u32 white = C2D_Color32(0xFF, 0xFF, 0xFF, 0xFF);
    u32 soft  = C2D_Color32(0xC0, 0xCE, 0xDC, 0xFF);
    u32 label = C2D_Color32(0x8C, 0xA0, 0xB4, 0xFF);
    u32 card  = C2D_Color32(0x10, 0x18, 0x26, 0xFF);
    u32 c_tax = C2D_Color32(0xFF, 0xD0, 0x40, 0xFF);
    u32 c_lux = C2D_Color32(0xE0, 0x90, 0xE0, 0xFF);
    u32 c_sci = C2D_Color32(0x70, 0xB8, 0xFF, 0xFF);
    char line[128];

    C2D_DrawRectSolid(0, 0, 0.0f, 400, 32, C2D_Color32(0x14, 0x1E, 0x2E, 0xFF));
    C2D_DrawRectSolid(0, 0, 0.01f, 6, 32, C2D_Color32(0x6C, 0xA8, 0xE0, 0xFF));
    C2D_DrawRectSolid(0, 32, 0.0f, 400, 1, C2D_Color32(0x6C, 0xA8, 0xE0, 0xFF));
    draw_text(14, 3, 0.85f, white, title);
    snprintf(line, sizeof(line), "Gold %d  (%+d a turn)", civ->gold, civ->gold_per_turn);
    draw_text(392 - text_width(line, 0.55f), 7, 0.55f, civ->gold_per_turn < 0 ? C2D_Color32(0xFF, 0x80, 0x70, 0xFF) : c_tax, line);

    // Research card.
    layer(3.0f);
    C2D_DrawRectSolid(8, 40, 0.0f, 188, 84, card);
    draw_text(16, 42, 0.45f, label, "Researching");
    draw_text(16, 58, 0.6f, white, civ->research_name[0] ? civ->research_name : "Nothing");
    progress_bar(16, 84, 172, 7, civ->bulbs_needed > 0 ? (float)civ->bulbs / civ->bulbs_needed : 0, c_sci);
    if (civ->research_turns >= 0)
        snprintf(line, sizeof(line), "%d/%d bulbs, %d turn%s", civ->bulbs, civ->bulbs_needed,
                 civ->research_turns, civ->research_turns == 1 ? "" : "s");
    else
        snprintf(line, sizeof(line), "%d/%d bulbs, no science", civ->bulbs, civ->bulbs_needed);
    draw_text(16, 96, 0.42f, soft, line);
    if (civ->goal_name[0]) {
        snprintf(line, sizeof(line), "Goal: %s", civ->goal_name);
        draw_text(16, 108, 0.42f, C2D_Color32(0xFF, 0xD8, 0x60, 0xFF), line);
    }

    // Government card.
    C2D_DrawRectSolid(204, 40, 0.0f, 188, 84, card);
    draw_text(212, 42, 0.45f, label, "Government");
    draw_text(212, 58, 0.6f, white, civ->government_name[0] ? civ->government_name : "?");
    if (civ->target_government_id >= 0) {
        snprintf(line, sizeof(line), "Revolution: %d turn%s left", civ->revolution_turns,
                 civ->revolution_turns == 1 ? "" : "s");
        draw_text(212, 88, 0.45f, C2D_Color32(0xFF, 0xA0, 0x60, 0xFF), line);
    } else {
        draw_text(212, 88, 0.42f, soft, "Stable");
    }

    // Rates: one stacked bar.
    C2D_DrawRectSolid(8, 132, 0.0f, 384, 76, card);
    draw_text(16, 134, 0.45f, label, "Where your trade goes");
    float bx = 16, bw = 368, by = 156;
    C2D_DrawRectSolid(bx, by, 0.01f, bw * civ->tax / 100.0f, 12, c_tax);
    C2D_DrawRectSolid(bx + bw * civ->tax / 100.0f, by, 0.01f, bw * civ->lux / 100.0f, 12, c_lux);
    C2D_DrawRectSolid(bx + bw * (civ->tax + civ->lux) / 100.0f, by, 0.01f, bw * civ->sci / 100.0f, 12, c_sci);
    snprintf(line, sizeof(line), "Tax %d%%", civ->tax);
    draw_text(16, 174, 0.47f, c_tax, line);
    snprintf(line, sizeof(line), "Luxury %d%%", civ->lux);
    draw_text(136, 174, 0.47f, c_lux, line);
    snprintf(line, sizeof(line), "Science %d%%", civ->sci);
    draw_text(266, 174, 0.47f, c_sci, line);
    snprintf(line, sizeof(line), "Highest rate allowed by %s: %d%%", civ->government_name, civ->max_rate);
    draw_text(16, 190, 0.4f, label, line);

    layer(0.0f);
    C2D_DrawRectSolid(0, 240 - 17, 0.0f, 400, 17, C2D_Color32(0x0C, 0x12, 0x1C, 0xFF));
    const char *hint = "B: close";
    if (!strcmp(title, "Research")) hint = "Tap a technology below to research it.   Y: tree / list   B: close";
    else if (!strcmp(title, "Tax & Science")) hint = "Use - and + below to set tax and luxury; science takes the rest.";
    else if (!strcmp(title, "Government")) hint = "Tap a government below to start a revolution.   B: close";
    draw_text(5, 240 - 16, 0.45f, soft, hint);
}

void render_research_panel(const TechOption *opts, int n, int scroll)
{
    C2D_TextBufClear(g_textBuf);
    u32 white = C2D_Color32(0xF0, 0xF0, 0xF0, 0xFF);
    u32 soft  = C2D_Color32(0xA8, 0xB4, 0xC4, 0xFF);
    char right[24];
    snprintf(right, sizeof(right), "%d available", n);
    panel_bg("Research", right);
    draw_text(8, PANEL_HEADER_H + 8, 0.45f, soft, "Pick what your scientists study next.");
    draw_text(8, PANEL_HEADER_H + 30, 0.42f, soft, n ? "Tap a technology:" : "Nothing left to research.");
    for (int i = 0; i < PANEL_ROWS_VISIBLE; i++) {
        int idx = scroll + i;
        if (idx >= n) break;
        list_row(i, 4, 306, opts[idx].current, true);
        float y = PANEL_LIST_Y + i * PANEL_ROW_H;
        u32 c = opts[idx].current ? C2D_Color32(0xFF, 0xE0, 0x60, 0xFF) : white;
        draw_text_z(12, y + 3, 0.01f, 0.47f, c, opts[idx].name);
        char t[24];
        if (opts[idx].turns >= 0)
            snprintf(t, sizeof(t), "%s%d turn%s", opts[idx].current ? "now, " : "", opts[idx].turns, opts[idx].turns == 1 ? "" : "s");
        else
            snprintf(t, sizeof(t), "%s", opts[idx].current ? "now" : "");
        if (t[0]) draw_text_z(304 - text_width(t, 0.42f), y + 4, 0.01f, 0.42f, opts[idx].current ? c : soft, t);
    }
    scroll_thumb(n, scroll, 313);
    draw_button(4, PANEL_FOOTER_Y + 2, 152, 240 - PANEL_FOOTER_Y - 4, 0.02f, "Tree view", 0.5f, true, false);
    draw_button(164, PANEL_FOOTER_Y + 2, 152, 240 - PANEL_FOOTER_Y - 4, 0.02f, "Close", 0.5f, true, false);
}

// Shared row-walk for the tiered tech-tree view (handoff.md §8.4), used
// identically by render_tech_tree_panel() (draws) and techtree_hit_test()
// (hit-tests) so they can never drift apart. nodes must be tier-sorted
// (get_tech_tree()'s contract). Starting at *idx, finds the run of entries
// sharing one tier - that run is exactly one displayable row - and reports
// its [start, end) range plus the tier value; advances *idx past it.
static int s_tech_goal = -1;
void render_set_tech_goal(int tech) { s_tech_goal = tech; }

// The tree is shown as display rows of up to TECHTREE_PER_ROW boxes; a tier
// with more techs wraps onto extra rows (only its first row is labelled).
// Shared by drawing, hit-testing and main.c's scroll clamp so they agree.
typedef struct { int start, count, tier; bool first; } TechRow;

static int tech_rows(const TechNode *nodes, int n, TechRow *out, int max)
{
    int rows = 0, i = 0;
    while (i < n) {
        int tier = nodes[i].tier, s0 = i;
        while (i < n && nodes[i].tier == tier) i++;
        for (int k = s0; k < i; k += TECHTREE_PER_ROW) {
            if (out && rows < max) {
                out[rows].start = k;
                out[rows].count = i - k < TECHTREE_PER_ROW ? i - k : TECHTREE_PER_ROW;
                out[rows].tier = tier;
                out[rows].first = k == s0;
            }
            rows++;
        }
    }
    return rows;
}

int techtree_display_rows(const TechNode *nodes, int n) { return tech_rows(nodes, n, NULL, 0); }

void render_tech_tree_panel(const TechNode *nodes, int n, int scroll)
{
    C2D_TextBufClear(g_textBuf);
    u32 soft   = C2D_Color32(0xA8, 0xB4, 0xC4, 0xFF);
    u32 gold   = C2D_Color32(0xFF, 0xD8, 0x60, 0xFF);
    char label[8];
    static TechRow rows[160];

    panel_bg("Tech Tree", "by era");
    C2D_DrawRectSolid(8, PANEL_HEADER_H + 12, 0.0f, 10, 10, C2D_Color32(0x2E, 0x6A, 0x34, 0xFF));
    draw_text(22, PANEL_HEADER_H + 6, 0.42f, soft, "Can research");
    C2D_DrawRectSolid(118, PANEL_HEADER_H + 12, 0.0f, 10, 10, C2D_Color32(0x24, 0x38, 0x52, 0xFF));
    draw_text(132, PANEL_HEADER_H + 6, 0.42f, soft, "Known");
    C2D_DrawRectSolid(190, PANEL_HEADER_H + 12, 0.0f, 10, 10, C2D_Color32(0x1C, 0x1E, 0x24, 0xFF));
    draw_text(204, PANEL_HEADER_H + 6, 0.42f, soft, "Locked");
    draw_text(8, PANEL_HEADER_H + 30, 0.42f, soft, "Green: research now.  Locked: set as goal.");

    int nr = tech_rows(nodes, n, rows, 160);
    if (nr > 160) nr = 160;
    for (int r = 0; r < PANEL_ROWS_VISIBLE && scroll + r < nr; r++) {
        const TechRow *row = &rows[scroll + r];
        float y = PANEL_LIST_Y + r * PANEL_ROW_H;
        if (row->first) {
            snprintf(label, sizeof(label), "%d", row->tier);
            draw_text(3, y + 4, 0.42f, soft, label);
        }
        for (int i = 0; i < row->count; i++) {
            const TechNode *node = &nodes[row->start + i];
            float bx = TECHTREE_TIER_LABEL_W + i * (TECHTREE_BOX_W + TECHTREE_BOX_GAP);
            u32 bg = node->state == 1 ? C2D_Color32(0x2E, 0x6A, 0x34, 0xFF)
                   : node->state == 2 ? C2D_Color32(0x24, 0x38, 0x52, 0xFF) : C2D_Color32(0x1C, 0x1E, 0x24, 0xFF);
            u32 fg = node->state == 1 ? 0xFFFFFFFF : node->state == 2 ? soft : C2D_Color32(0x8C, 0x94, 0xA0, 0xFF);
            bool goal = node->tech == s_tech_goal && node->state != 2;
            if (goal) {
                C2D_DrawRectSolid(bx - 1, y, 0.0f, TECHTREE_BOX_W + 2, PANEL_ROW_H, gold);
                fg = gold;
            }
            C2D_DrawRectSolid(bx, y + 1, 0.001f, TECHTREE_BOX_W, PANEL_ROW_H - 2, bg);
            char nm[32];
            snprintf(nm, sizeof(nm), "%s", node->name);
            float sc = 0.42f;
            while (sc > 0.36f && font_width(nm, sc) > TECHTREE_BOX_W - 6) sc -= 0.02f;
            while (nm[0] && font_width(nm, sc) > TECHTREE_BOX_W - 6) nm[strlen(nm) - 1] = 0;
            draw_text_z(bx + 3, y + 4, 0.01f, sc, fg, nm);
        }
    }
    if (nr > PANEL_ROWS_VISIBLE) scroll_thumb(nr, scroll, 317);
    draw_button(4, PANEL_FOOTER_Y + 2, 152, 240 - PANEL_FOOTER_Y - 4, 0.02f, "List view", 0.5f, true, false);
    draw_button(164, PANEL_FOOTER_Y + 2, 152, 240 - PANEL_FOOTER_Y - 4, 0.02f, "Close", 0.5f, true, false);
}

int techtree_hit_test(const TechNode *nodes, int n, int scroll, int px, int py)
{
    static TechRow rows[160];
    int rel_y = py - PANEL_LIST_Y;
    if (rel_y < 0 || rel_y >= PANEL_ROW_H * PANEL_ROWS_VISIBLE || px < TECHTREE_TIER_LABEL_W) return -1;
    int nr = tech_rows(nodes, n, rows, 160);
    int r = scroll + rel_y / PANEL_ROW_H;
    if (r >= nr || r >= 160) return -1;
    int box = (px - TECHTREE_TIER_LABEL_W) / (TECHTREE_BOX_W + TECHTREE_BOX_GAP);
    int box_x_rel = (px - TECHTREE_TIER_LABEL_W) % (TECHTREE_BOX_W + TECHTREE_BOX_GAP);
    if (box < rows[r].count && box_x_rel < TECHTREE_BOX_W) return rows[r].start + box;
    return -1;
}

void render_rates_panel(const CivInfo *civ)
{
    C2D_TextBufClear(g_textBuf);
    u32 soft = C2D_Color32(0xA8, 0xB4, 0xC4, 0xFF);
    char line[48];
    panel_bg("Tax Rates", NULL);
    draw_text(8, PANEL_HEADER_H + 10, 0.45f, soft, "Steps of 10%. Science gets what's left.");
    struct { const char *n; int v; int y; u32 c; bool edit; } r[3] = {
        { "Tax", civ->tax, RATES_TAX_Y, C2D_Color32(0xFF, 0xD0, 0x40, 0xFF), true },
        { "Luxury", civ->lux, RATES_LUX_Y, C2D_Color32(0xE0, 0x90, 0xE0, 0xFF), true },
        { "Science", civ->sci, RATES_SCI_Y, C2D_Color32(0x70, 0xB8, 0xFF, 0xFF), false },
    };
    for (int i = 0; i < 3; i++) {
        float y = r[i].y;
        C2D_DrawRectSolid(4, y - 4, 0.0f, 312, 34, C2D_Color32(0x1A, 0x24, 0x34, 0xFF));
        draw_text_z(12, y - 2, 0.01f, 0.5f, r[i].c, r[i].n);
        snprintf(line, sizeof(line), "%d%%", r[i].v);
        draw_text_z(150 - text_width(line, 0.55f), y - 2, 0.01f, 0.55f, 0xFFFFFFFF, line);
        // bar under the name
        C2D_DrawRectSolid(12, y + 20, 0.01f, 140, 4, C2D_Color32(0x22, 0x2C, 0x3A, 0xFF));
        C2D_DrawRectSolid(12, y + 20, 0.02f, 140 * r[i].v / 100.0f, 4, r[i].c);
        if (r[i].edit) {
            draw_button(RATES_BTN_MINUS_X, y - 2, RATES_BTN_W, RATES_ROW_TOUCH_H + 4, 0.02f, "-", 0.8f,
                        r[i].v >= 10, false);
            draw_button(RATES_BTN_PLUS_X, y - 2, RATES_BTN_W, RATES_ROW_TOUCH_H + 4, 0.02f, "+", 0.8f,
                        r[i].v + 10 <= civ->max_rate && civ->sci >= 10, false);
        } else {
            draw_text_z(RATES_BTN_MINUS_X + 4, y + 1, 0.01f, 0.42f, soft, "(the rest)");
        }
    }
    snprintf(line, sizeof(line), "Max %d%% under your government", civ->max_rate);
    draw_text(8, RATES_SCI_Y + 32, 0.42f, soft, line);
    draw_button(4, PANEL_FOOTER_Y + 2, 312, 240 - PANEL_FOOTER_Y - 4, 0.02f, "Done", 0.5f, true, false);
}

void render_government_panel(const GovOption *opts, int n, int scroll, int current_gov_id)
{
    C2D_TextBufClear(g_textBuf);
    u32 white = C2D_Color32(0xF0, 0xF0, 0xF0, 0xFF);
    u32 soft  = C2D_Color32(0xA8, 0xB4, 0xC4, 0xFF);
    u32 grey  = C2D_Color32(0x5C, 0x64, 0x70, 0xFF);
    u32 cur   = C2D_Color32(0xFF, 0xE0, 0x60, 0xFF);
    panel_bg("Government", NULL);
    draw_text(8, PANEL_HEADER_H + 8, 0.45f, soft, "Changing government means a revolution:");
    draw_text(8, PANEL_HEADER_H + 26, 0.45f, soft, "a few turns of anarchy first.");
    for (int i = 0; i < PANEL_ROWS_VISIBLE; i++) {
        int idx = scroll + i;
        if (idx >= n) break;
        const GovOption *opt = &opts[idx];
        bool is_cur = opt->gov_id == current_gov_id;
        bool arm = idx == s_gov_armed && !is_cur;
        list_row(i, 4, 306, is_cur || arm, opt->available);
        float y = PANEL_LIST_Y + i * PANEL_ROW_H;
        draw_text_z(12, y + 3, 0.01f, 0.47f, is_cur || arm ? cur : opt->available ? white : grey, opt->name);
        const char *tag = is_cur ? "current" : arm ? "tap again: revolution" : opt->available ? "" : "needs tech";
        if (arm) { draw_text_z(304 - text_width(tag, 0.42f), y + 4, 0.01f, 0.42f, cur, tag); continue; }
        if (tag[0]) draw_text_z(304 - text_width(tag, 0.4f), y + 4, 0.01f, 0.4f, is_cur ? cur : grey, tag);
    }
    scroll_thumb(n, scroll, 313);
    draw_button(4, PANEL_FOOTER_Y + 2, 312, 240 - PANEL_FOOTER_Y - 4, 0.02f, "Close", 0.5f, true, false);
}

void render_processing_overlay(int screen_w, int screen_h, ProcessingReason reason)
{
    // Deliberately touches no Freeciv game state (no get_tile_info/
    // get_game_turn/etc) - this is drawn precisely while such reads are
    // unsafe, i.e. while the server coroutine is busy running a step.
    C2D_TextBufClear(g_textBuf);

    u32 white = C2D_Color32(0xFF, 0xFF, 0xFF, 0xFF);
    u32 dim   = C2D_Color32(0xA0, 0xA8, 0xB4, 0xFF);

    C2D_DrawRectSolid(0, 0, 0.0f, screen_w, screen_h, C2D_Color32(0x14, 0x1C, 0x28, 0xFF));

    const char *headline = reason == PROC_LOADING ? "Generating world..."
                          : reason == PROC_SAVING  ? "Saving..."
                                                    : "Processing turn...";
    draw_text(8, screen_h / 2.0f - 20, 0.75f, white, headline);
    draw_text(8, screen_h / 2.0f + 8, 0.55f, dim, "Please wait");
}

void render_turn_banner(ProcessingReason reason, int frame)
{
    // A strip across the bottom of the touch screen, over the (inactive) HUD
    // row, while the map above stays visible as a captured picture. Same
    // "touches no Freeciv game state" rule as render_processing_overlay().
    C2D_TextBufClear(g_textBuf);
    const float H = 28, Y = 240 - H;
    C2D_DrawRectSolid(0, Y, 0.0f, 320, H, C2D_Color32(0x0C, 0x14, 0x22, 0xFF));
    C2D_DrawRectSolid(0, Y, 0.0f, 320, 2, C2D_Color32(0xFF, 0xD8, 0x60, 0xFF));
    const char *msg = reason == PROC_SAVING ? "Saving" : "Other civilizations are moving";
    char line[48];
    int dots = (frame / 20) % 4;
    snprintf(line, sizeof(line), "%s%.*s", msg, dots, "...");
    draw_text(10, Y + 6, 0.5f, C2D_Color32(0xFF, 0xFF, 0xFF, 0xFF), line);
}

void render_busy_indicator(int screen_w)
{
    // Same "touches no Freeciv game state" rule as render_processing_overlay
    // - drawn on top of a panel that was itself rendered from a cached
    // snapshot, never from a fresh read.
    // z=0.0f (not layered above 0, like the map-tile stack elsewhere in this
    // file) - draw_text() always draws at z=0.0f, so anything above that
    // z-fights in front of and hides the text.
    C2D_DrawRectSolid(screen_w - 30, 4, 0.0f, 26, 14, C2D_Color32(0x30, 0x30, 0x30, 0xC0));
    draw_text(screen_w - 26, 5, 0.45f, C2D_Color32(0xFF, 0xFF, 0xFF, 0xFF), "...");
}

void render_action_bar(void)
{
    u32 bg     = C2D_Color32(0x2C, 0x50, 0x80, 0xFF);
    u32 border = C2D_Color32(0x6C, 0xA8, 0xE0, 0xFF);
    u32 white  = C2D_Color32(0xFF, 0xFF, 0xFF, 0xFF);

    // z=0.9f, not 0.0f: render_action_bar() is only ever called for UI_MAP,
    // right after render_bottom_screen(), so - despite an older comment here
    // claiming otherwise - it DOES overlay the live map, same as
    // render_toast() below. z=0.0f only stayed hidden-bug-free by luck: the
    // map's terrain/coast stack (up to z=0.7, see render_bottom_screen()) is
    // usually fog/unexplored under the action bar's screen column, so
    // nothing there competed with it - until goto-driven exploration and
    // varied camera positions put real, explored terrain under it (hardware
    // finding, 2026-07-11: RES/TAX/top-of-GOV were invisible, boxed out by
    // ocean coast tiles at z=0.493, while DIP/SAV/GOTO sat over unexplored
    // tiles and stayed visible - a scroll-position-dependent bug, invisible
    // in every prior test session by chance). draw_text_z() (not
    // draw_text(), which is hardcoded to z=0.0f) for the labels for the same
    // reason - the old panel-rect/label bug is exactly what happens if the
    // rects move to a higher z than their own text.
    const float Z = 0.9f;
    C2D_DrawRectSolid(ACTIONBAR_X, ACTIONBAR_RES_Y, Z, ACTIONBAR_BTN_W, ACTIONBAR_BTN_H, bg);
    C2D_DrawRectSolid(ACTIONBAR_X, ACTIONBAR_RES_Y, Z, ACTIONBAR_BTN_W, 2, border);
    draw_text_z(ACTIONBAR_X + 4, ACTIONBAR_RES_Y + 10, Z, 0.45f, white, "RES");

    C2D_DrawRectSolid(ACTIONBAR_X, ACTIONBAR_RATES_Y, Z, ACTIONBAR_BTN_W, ACTIONBAR_BTN_H, bg);
    C2D_DrawRectSolid(ACTIONBAR_X, ACTIONBAR_RATES_Y, Z, ACTIONBAR_BTN_W, 2, border);
    draw_text_z(ACTIONBAR_X + 4, ACTIONBAR_RATES_Y + 10, Z, 0.45f, white, "TAX");

    C2D_DrawRectSolid(ACTIONBAR_X, ACTIONBAR_GOV_Y, Z, ACTIONBAR_BTN_W, ACTIONBAR_BTN_H, bg);
    C2D_DrawRectSolid(ACTIONBAR_X, ACTIONBAR_GOV_Y, Z, ACTIONBAR_BTN_W, 2, border);
    draw_text_z(ACTIONBAR_X + 4, ACTIONBAR_GOV_Y + 10, Z, 0.45f, white, "GOV");

    C2D_DrawRectSolid(ACTIONBAR_X, ACTIONBAR_DIPL_Y, Z, ACTIONBAR_BTN_W, ACTIONBAR_BTN_H, bg);
    C2D_DrawRectSolid(ACTIONBAR_X, ACTIONBAR_DIPL_Y, Z, ACTIONBAR_BTN_W, 2, border);
    draw_text_z(ACTIONBAR_X + 4, ACTIONBAR_DIPL_Y + 10, Z, 0.45f, white, "DIP");

    C2D_DrawRectSolid(ACTIONBAR_X, ACTIONBAR_SAVE_Y, Z, ACTIONBAR_BTN_W, ACTIONBAR_BTN_H, bg);
    C2D_DrawRectSolid(ACTIONBAR_X, ACTIONBAR_SAVE_Y, Z, ACTIONBAR_BTN_W, 2, border);
    draw_text_z(ACTIONBAR_X + 4, ACTIONBAR_SAVE_Y + 10, Z, 0.45f, white, "SAV");

    C2D_DrawRectSolid(ACTIONBAR_X, ACTIONBAR_GOTO_Y, Z, ACTIONBAR_BTN_W, ACTIONBAR_BTN_H, bg);
    C2D_DrawRectSolid(ACTIONBAR_X, ACTIONBAR_GOTO_Y, Z, ACTIONBAR_BTN_W, 2, border);
    draw_text_z(ACTIONBAR_X + 1, ACTIONBAR_GOTO_Y + 10, Z, 0.45f, white, "GOTO");
}

void render_toast(const char *text)
{
    // Unlike every other rect+text pair in this file, this one overlays the
    // live UI_MAP map render, whose terrain/overlay/unit stack goes up to
    // z=0.7 (see render_bottom_screen()) - z=0.0f (the safe value
    // everywhere else, since panels never draw anything else over
    // themselves) loses the PICA200 depth test and renders invisibly behind
    // the map. 0.9f clears that whole stack with margin.
    const float TOAST_Z = 0.95f;
    float sc = 0.5f, tw = text_width(text, sc);
    while (tw > 290 && sc > 0.4f) { sc -= 0.02f; tw = text_width(text, sc); }
    float w = tw + 16, x = (320 - w) / 2;
    C2D_DrawRectSolid(x, 8, TOAST_Z, w, 22, C2D_Color32(0x20, 0x40, 0x60, 0xF0));
    C2D_DrawRectSolid(x, 8, TOAST_Z + 0.001f, w, 2, C2D_Color32(0x6C, 0xA8, 0xE0, 0xFF));
    draw_text_z(x + 8, 13, TOAST_Z + 0.002f, sc, C2D_Color32(0xFF, 0xFF, 0xFF, 0xFF), text);
}

// enum diplstate_type's 0..5 values (see PlayerOption's comment in
// game_loop.h) as short display labels - render.c doesn't include
// freeciv's player.h, same reasoning as TechNode.state staying a plain int.
static const char *diplstate_name(int ds)
{
    switch (ds) {
    case 0: return "Armistice";
    case 1: return "War";
    case 2: return "Ceasefire";
    case 3: return "Peace";
    case 4: return "Alliance";
    case 5: return "No Contact";
    default: return "?";
    }
}

static u32 diplstate_color(int ds)
{
    switch (ds) {
    case 1: return C2D_Color32(0xF0, 0x60, 0x50, 0xFF);   // war
    case 2: case 0: return C2D_Color32(0xF0, 0xC0, 0x50, 0xFF);
    case 3: return C2D_Color32(0x80, 0xD8, 0x80, 0xFF);   // peace
    case 4: return C2D_Color32(0x70, 0xC0, 0xFF, 0xFF);   // alliance
    default: return C2D_Color32(0x8C, 0xA0, 0xB4, 0xFF);
    }
}

void render_diplomacy_top(int stage, const char *counterpart_name, const DiploInfo *diplo)
{
    C2D_TextBufClear(g_textBuf);
    u32 white = C2D_Color32(0xFF, 0xFF, 0xFF, 0xFF);
    u32 soft  = C2D_Color32(0xC0, 0xCE, 0xDC, 0xFF);
    u32 label = C2D_Color32(0x8C, 0xA0, 0xB4, 0xFF);
    u32 ok    = C2D_Color32(0x70, 0xE0, 0x70, 0xFF);
    char line[96];

    C2D_DrawRectSolid(0, 0, 0.0f, 400, 32, C2D_Color32(0x14, 0x1E, 0x2E, 0xFF));
    C2D_DrawRectSolid(0, 0, 0.01f, 6, 32, C2D_Color32(0x6C, 0xA8, 0xE0, 0xFF));
    C2D_DrawRectSolid(0, 32, 0.0f, 400, 1, C2D_Color32(0x6C, 0xA8, 0xE0, 0xFF));
    draw_text(14, 3, 0.85f, white, "Diplomacy");

    layer(3.0f);
    if (stage == 1) {
        C2D_DrawRectSolid(8, 44, 0.0f, 384, 120, C2D_Color32(0x10, 0x18, 0x26, 0xFF));
        draw_text(18, 52, 0.6f, white, "Meet another leader");
        draw_text(18, 80, 0.47f, soft, "Pick a civilization below to open talks.");
        draw_text(18, 100, 0.47f, soft, "You can offer peace, a ceasefire, an embassy");
        draw_text(18, 118, 0.47f, soft, "or gold. A treaty is made when both accept.");
    } else {
        C2D_DrawRectSolid(8, 44, 0.0f, 384, 166, C2D_Color32(0x10, 0x18, 0x26, 0xFF));
        draw_text(18, 48, 0.45f, label, "Talks with");
        float ns = 0.7f; // long leader names must stop short of the badges
        while (ns > 0.42f && text_width(counterpart_name, ns) > 228) ns -= 0.02f;
        draw_text(18, 64 + (0.7f - ns) * 12, ns, white, counterpart_name);
        // acceptance badges
        struct { const char *who; bool yes; } b[2] = { { "You", diplo->accept_self }, { "Them", diplo->accept_other } };
        for (int i = 0; i < 2; i++) {
            float x = 250 + i * 70;
            C2D_DrawRectSolid(x, 52, 0.01f, 62, 36, b[i].yes ? C2D_Color32(0x2E, 0x6A, 0x34, 0xFF) : C2D_Color32(0x2A, 0x30, 0x3C, 0xFF));
            draw_text_z(x + 31 - text_width(b[i].who, 0.42f) / 2, 52, 0.02f, 0.42f, soft, b[i].who);
            const char *st = b[i].yes ? "Accepts" : "Thinking";
            draw_text_z(x + 31 - text_width(st, 0.42f) / 2, 68, 0.02f, 0.42f, b[i].yes ? ok : label, st);
        }
        draw_text(18, 98, 0.45f, label, "On the table");
        if (diplo->clause_count == 0) draw_text(18, 116, 0.47f, soft, "Nothing yet - add something below.");
        for (int i = 0; i < diplo->clause_count && i < 5; i++) {
            draw_text(18, 116 + i * 17, 0.47f, white, diplo->clauses[i].desc);
        }
        if (diplo->clause_count > 5) {
            snprintf(line, sizeof(line), "+%d more", diplo->clause_count - 5);
            draw_text(18, 116 + 5 * 17, 0.42f, label, line);
        }
    }
    layer(0.0f);
    C2D_DrawRectSolid(0, 240 - 17, 0.0f, 400, 17, C2D_Color32(0x0C, 0x12, 0x1C, 0xFF));
    draw_text(5, 240 - 16, 0.45f, soft, stage == 1 ? "Tap a civilization below.   B: close"
                                                   : "Add terms, then Accept. Tap a term below to remove it.");
}

void render_diplomacy_panel(int stage, const char *counterpart_name,
                            const PlayerOption *players, int player_count, int scroll,
                            const DiploInfo *diplo)
{
    C2D_TextBufClear(g_textBuf);
    u32 white = C2D_Color32(0xF0, 0xF0, 0xF0, 0xFF);
    u32 soft  = C2D_Color32(0xA8, 0xB4, 0xC4, 0xFF);
    char line[64];

    if (stage == 1) {
        panel_bg("Diplomacy", NULL);
        draw_text(8, PANEL_HEADER_H + 8, 0.45f, soft, player_count ? "Civilizations you know of:" : "You haven't met anyone yet.");
        for (int i = 0; i < PANEL_ROWS_VISIBLE; i++) {
            int idx = scroll + i;
            if (idx >= player_count) break;
            list_row(i, 4, 306, false, true);
            float y = PANEL_LIST_Y + i * PANEL_ROW_H;
            draw_text_z(12, y + 3, 0.01f, 0.47f, players[idx].can_meet ? white : soft, players[idx].name);
            const char *st = players[idx].can_meet ? diplstate_name(players[idx].diplstate) : "No contact";
            draw_text_z(304 - text_width(st, 0.42f), y + 4, 0.01f, 0.42f, diplstate_color(players[idx].diplstate), st);
        }
        scroll_thumb(player_count, scroll, 313);
        draw_button(4, PANEL_FOOTER_Y + 2, 312, 240 - PANEL_FOOTER_Y - 4, 0.02f, "Close", 0.5f, true, false);
        return;
    }

    snprintf(line, sizeof(line), "%.28s", counterpart_name);
    panel_bg(line, NULL);
    static const char *labels[4] = { "Peace", "Ceasefire", "Embassy", "+50 Gold" };
    for (int i = 0; i < 4; i++) {
        float bx = i * (DIPLO_ADDCLAUSE_BTN_W + DIPLO_ADDCLAUSE_GAP);
        draw_button(bx + 2, DIPLO_ADDCLAUSE_Y, DIPLO_ADDCLAUSE_BTN_W - 2, DIPLO_ADDCLAUSE_H, 0.02f, labels[i], 0.42f, true, false);
    }
    draw_text(8, DIPLO_ADDCLAUSE_Y + DIPLO_ADDCLAUSE_H + 6, 0.42f, soft,
              diplo->clause_count ? "Terms (tap to remove):" : "No terms yet - add one above.");
    for (int i = 0; i < PANEL_ROWS_VISIBLE && i < diplo->clause_count; i++) {
        list_row(i, 4, 312, false, true);
        draw_text_z(12, PANEL_LIST_Y + i * PANEL_ROW_H + 3, 0.01f, 0.45f, white, diplo->clauses[i].desc);
    }
    float fh = 240 - PANEL_FOOTER_Y - 4;
    draw_button(2, PANEL_FOOTER_Y + 2, DIPLO_FOOTER_SPLIT1 - 4, fh, 0.02f,
                diplo->accept_self ? "Accepted" : "Accept", 0.5f, diplo->clause_count > 0, true);
    draw_button(DIPLO_FOOTER_SPLIT1 + 2, PANEL_FOOTER_Y + 2, DIPLO_FOOTER_SPLIT2 - DIPLO_FOOTER_SPLIT1 - 4, fh, 0.02f,
                "Back", 0.5f, true, false);
    draw_button(DIPLO_FOOTER_SPLIT2 + 2, PANEL_FOOTER_Y + 2, 318 - DIPLO_FOOTER_SPLIT2 - 2, fh, 0.02f,
                "Close", 0.5f, true, false);
}

void render_start_menu(const char *continue_label)
{
    C2D_TextBufClear(g_textBuf);
    u32 white = C2D_Color32(0xFF, 0xFF, 0xFF, 0xFF);
    u32 dim   = C2D_Color32(0xA0, 0xA8, 0xB4, 0xFF);
    bool cont = continue_label != NULL;
    C2D_DrawRectSolid(0, 0, 0.0f, 320, 240, C2D_Color32(0x10, 0x10, 0x10, 0xFF));
    draw_text(8, cont ? 8 : 20, 0.8f, white, "Freeciv 3DS");
    if (cont) {
        int y = startmenu_slot_y(0, true);
        draw_button(STARTMENU_BTN_X, y, STARTMENU_BTN_W, STARTMENU_BTN_H, 0.0f, "", 0.6f, true, true);
        draw_text(STARTMENU_BTN_X + 16, y + 3, 0.6f, white, "Continue");
        draw_text(STARTMENU_BTN_X + 16, y + 22, 0.42f, C2D_Color32(0xD0, 0xF0, 0xD0, 0xFF), continue_label);
    }
    int yn = startmenu_slot_y(1, cont), yl = startmenu_slot_y(2, cont);
    draw_button(STARTMENU_BTN_X, yn, STARTMENU_BTN_W, STARTMENU_BTN_H, 0.0f, "New Game", 0.6f, true, false);
    draw_button(STARTMENU_BTN_X, yl, STARTMENU_BTN_W, STARTMENU_BTN_H, 0.0f, "Load Game", 0.6f, true, false);
    draw_text(8, 220, 0.45f, dim, "Tap to choose");
}

// ---------------------------------------------------------------- title
static C2D_SpriteSheet s_title_sheet;
static unsigned char s_bd[24][64];   // backdrop terrain kinds
static bool s_bd_ready;

static void backdrop_init(void)
{
    if (s_bd_ready) return;
    // A few overlapping "continents": sum of random blobs, thresholded.
    float hgt[24][64] = {{0}}, wet[24][64] = {{0}};
    srand(1234);
    for (int k = 0; k < 30; k++) {
        float cx = rand() % 64, cy = rand() % 24, r = 3 + rand() % 7, a = 0.5f + (rand() % 50) / 100.0f;
        float wx = rand() % 64, wy = rand() % 24;
        for (int y = 0; y < 24; y++) for (int x = 0; x < 64; x++) {
            float dx = fabsf(x - cx); if (dx > 32) dx = 64 - dx;   // wraps
            float d = sqrtf(dx * dx + (y - cy) * (y - cy));
            if (d < r) hgt[y][x] += a * (1 - d / r);
            float ex = fabsf(x - wx); if (ex > 32) ex = 64 - ex;
            float e = sqrtf(ex * ex + (y - wy) * (y - wy));
            if (e < 6) wet[y][x] += 1 - e / 6;
        }
    }
    for (int y = 0; y < 24; y++) for (int x = 0; x < 64; x++) {
        float h = hgt[y][x];
        int k;
        if (h < 0.22f) k = 0;                       // ocean
        else if (h < 0.55f) k = wet[y][x] > 0.6f ? 2 : (wet[y][x] < 0.15f && y > 14 ? 6 : 1); // plains/grass/desert
        else if (h < 0.80f) k = 3;                  // forest
        else if (h < 1.05f) k = 4;                  // hills
        else k = 5;                                 // mountains
        s_bd[y][x] = (unsigned char)k;
    }
    s_bd_ready = true;
}

// The slow map flight behind the title. dim: 0..255 darkening for legibility.
static void draw_backdrop(int frame, float w, float h, int dim)
{
    if (!g_spriteSheet) { // no tiles: a plain dark backdrop rather than a crash
        C2D_DrawRectSolid(0, 0, 0.0f, w, h, C2D_Color32(0x14, 0x1C, 0x28, 0xFF));
        return;
    }
    backdrop_init();
    int idx[7] = { sprites_ocean_idx, sprites_plains_idx, sprites_grassland_idx, sprites_forest_idx,
                   sprites_hills_idx, sprites_mountains_idx, sprites_desert_idx };
    const float T = 30.0f;
    float off = frame * 0.25f;            // px; ~15 px/s at 60 fps
    float wrap = 64 * T;
    off = fmodf(off, wrap);
    int x0 = (int)(off / T);
    float fx = -(off - x0 * T);
    for (int ty = 0; ty * T < h; ty++) {
        for (int i = 0; fx + i * T < w; i++) {
            int tx = (x0 + i) % 64;
            int k = s_bd[ty % 24][tx];
            C2D_DrawImageAt(C2D_SpriteSheetGetImage(g_spriteSheet, idx[k]), fx + i * T, ty * T, 0.0f, NULL, 1.0f, 1.0f);
        }
    }
    if (dim > 0) C2D_DrawRectSolid(0, 0, 0.01f, w, h, C2D_Color32(0x06, 0x0A, 0x12, dim));
}

int title_rows(bool has_continue, int *items)
{
    int n = 0;
    for (int i = has_continue ? TI_CONTINUE : TI_NEW; i < TI_COUNT; i++) items[n++] = i;
    return n;
}

int title_hit(int px, int py, bool has_continue)
{
    int items[TI_COUNT], n = title_rows(has_continue, items);
    if (px < TITLE_ROW_X || px >= TITLE_ROW_X + TITLE_ROW_W || py < TITLE_ROW_Y0) return -1;
    int r = (py - TITLE_ROW_Y0) / TITLE_ROW_H;
    if (r >= n || (py - TITLE_ROW_Y0) % TITLE_ROW_H >= TITLE_ROW_H - 5) return -1;
    return items[r];
}

void render_title_top(int frame, const char *line, const char *headline)
{
    C2D_TextBufClear(g_textBuf);
    layer(0.0f);
    draw_backdrop(frame, 400, 240, 0x70);
    // Logo floats in front (stereo), with a gentle bob.
    if (!s_title_sheet) s_title_sheet = C2D_SpriteSheetLoad("romfs:/gfx/title.t3x");
    layer(4.0f);
    float bob = sinf(frame * 0.03f) * 1.5f;
    if (s_title_sheet) {
        C2D_Image logo = C2D_SpriteSheetGetImage(s_title_sheet, 0);
        float lw = logo.subtex->width;
        C2D_DrawImageAt(logo, (400 - lw) / 2, 44 + bob, 0.5f, NULL, 1.0f, 1.0f);
    } else {
        draw_text_z(120, 50, 0.2f, 0.9f, C2D_Color32(0xFF, 0xD8, 0x60, 0xFF), "FREECIV");
    }
    layer(2.0f);
    const char *tag = headline ? headline : "Build a civilization, on two screens";
    float tw = text_width(tag, 0.55f);
    draw_text_z((400 - tw) / 2, 116 + bob * 0.5f, 0.2f, 0.55f, C2D_Color32(0xF4, 0xEC, 0xD8, 0xFF), tag);
    layer(0.0f);
    if (line && line[0]) {
        C2D_DrawRectSolid(0, 240 - 30, 0.02f, 400, 30, C2D_Color32(0x06, 0x0A, 0x12, 0xC8));
        C2D_DrawRectSolid(0, 240 - 30, 0.03f, 400, 1, C2D_Color32(0xFF, 0xD8, 0x60, 0x90));
        draw_wrapped(line, 10, 240 - 26, 380, 0.47f, C2D_Color32(0xE0, 0xE6, 0xEE, 0xFF), 2, 15, 0.04f);
    }
}

static void title_row(float y, const char *label, const char *sub, bool sel)
{
    u32 bg = sel ? C2D_Color32(0x5A, 0x44, 0x12, 0xF0) : C2D_Color32(0x16, 0x22, 0x34, 0xE8);
    u32 edge = sel ? C2D_Color32(0xFF, 0xD8, 0x60, 0xFF) : C2D_Color32(0x4C, 0x6C, 0x90, 0xFF);
    float h = TITLE_ROW_H - 5;
    C2D_DrawRectSolid(TITLE_ROW_X, y, 0.10f, TITLE_ROW_W, h, bg);
    C2D_DrawRectSolid(TITLE_ROW_X, y, 0.11f, TITLE_ROW_W, 2, edge);
    C2D_DrawRectSolid(TITLE_ROW_X, y + h - 1, 0.11f, TITLE_ROW_W, 1, edge);
    if (sel) C2D_DrawRectSolid(TITLE_ROW_X, y, 0.11f, 4, h, edge);
    u32 tc = sel ? C2D_Color32(0xFF, 0xF2, 0xC8, 0xFF) : 0xFFFFFFFF;
    if (sub && sub[0]) {
        draw_text_z(TITLE_ROW_X + 14, y + 1, 0.12f, 0.55f, tc, label);
        float sw = text_width(sub, 0.42f);
        draw_text_z(TITLE_ROW_X + TITLE_ROW_W - 10 - sw, y + 7, 0.12f, 0.42f,
                    sel ? C2D_Color32(0xFF, 0xE0, 0x90, 0xFF) : C2D_Color32(0xA8, 0xC0, 0xD8, 0xFF), sub);
    } else {
        draw_text_z(TITLE_ROW_X + 14, y + 2, 0.12f, 0.55f, tc, label);
    }
}

void render_title_bottom(int sel, bool has_continue, const char *continue_label, int frame)
{
    C2D_TextBufClear(g_textBuf);
    draw_backdrop(frame + 900, 320, 240, 0xB0);  // the same map, further along
    static const char *labels[TI_COUNT] = { "Continue", "New Game", "Load Game", "Settings", "Controls", "Manual", "Quit" };
    int items[TI_COUNT], n = title_rows(has_continue, items);
    for (int r = 0; r < n; r++) {
        int it = items[r];
        title_row(TITLE_ROW_Y0 + r * TITLE_ROW_H, labels[it], it == TI_CONTINUE ? continue_label : NULL, it == sel);
    }
}

void render_title_settings(int music, int sfx, int text_large)
{
    C2D_TextBufClear(g_textBuf);
    draw_backdrop(900, 320, 240, 0xC0);
    C2D_DrawRectSolid(0, 0, 0.1f, 320, 26, C2D_Color32(0x2C, 0x50, 0x80, 0xFF));
    draw_text_z(8, 3, 0.12f, 0.6f, 0xFFFFFFFF, "Settings");
    const char *names[3] = { "Music", "Effects", "Text size" };
    char vals[3][16];
    snprintf(vals[0], sizeof(vals[0]), "%d", music);
    snprintf(vals[1], sizeof(vals[1]), "%d", sfx);
    snprintf(vals[2], sizeof(vals[2]), "%s", text_large ? "Large" : "Normal");
    int v[3] = { music, sfx, text_large }, hi[3] = { 10, 10, 1 };
    for (int i = 0; i < 3; i++) {
        float y = SETUP_ROW_Y0 + i * SETUP_ROW_H;
        draw_text_z(12, y + 2, 0.12f, 0.5f, 0xFFFFFFFF, names[i]);
        draw_button(SETUP_MINUS_X, y, SETUP_BTN_W, SETUP_BTN_H, 0.12f, "-", 0.6f, v[i] > 0, false);
        float vw = text_width(vals[i], 0.5f);
        draw_text_z(SETUP_MINUS_X + SETUP_BTN_W + (SETUP_PLUS_X - SETUP_MINUS_X - SETUP_BTN_W - vw) / 2, y + 2, 0.12f, 0.5f,
                    C2D_Color32(0xFF, 0xE8, 0xA0, 0xFF), vals[i]);
        draw_button(SETUP_PLUS_X, y, SETUP_BTN_W, SETUP_BTN_H, 0.12f, "+", 0.6f, v[i] < hi[i], false);
    }
    draw_button(SETUP_BACK_X, SETUP_FOOT_Y, SETUP_FOOT_W, SETUP_FOOT_H, 0.12f, "Back", 0.55f, true, false);
}

void render_title_controls_top(int frame)
{
    C2D_TextBufClear(g_textBuf);
    layer(0.0f);
    draw_backdrop(frame, 400, 240, 0xD0);
    draw_text_z(12, 6, 0.2f, 0.6f, C2D_Color32(0xFF, 0xD8, 0x60, 0xFF), "Controls");
    static const char *rows[][2] = {
        { "Tap a unit",        "select it; orders show above the bar" },
        { "Tap next to it",    "move there (far away: tap twice)" },
        { "Hold the stylus",   "peek at a square without moving" },
        { "Drag",              "scroll the map" },
        { "Map button",        "whole world; tap to jump there" },
        { "End Turn",          "shows what needs you first" },
        { "A / X",             "orders / next unit" },
        { "B",                 "back or cancel" },
        { "D-Pad, Circle Pad", "move the unit / scroll" },
        { "L / R  ZL / ZR",    "research, taxes / zoom" },
        { "START / SEL+START", "end turn / quit" },
    };
    u32 key = C2D_Color32(0xFF, 0xE8, 0xA0, 0xFF), val = C2D_Color32(0xE0, 0xE6, 0xEE, 0xFF);
    for (int i = 0; i < (int)(sizeof(rows) / sizeof(rows[0])); i++) {
        float y = 30 + i * 19;
        draw_text_z(12, y, 0.2f, 0.45f, key, rows[i][0]);
        float sc = 0.45f;
        while (text_width(rows[i][1], sc) > 400 - 140 - 8 && sc > 0.34f) sc -= 0.01f;
        draw_text_z(140, y, 0.2f, sc, val, rows[i][1]);
    }
}

void render_title_loading_bottom(const char *msg, int frame)
{
    C2D_TextBufClear(g_textBuf);
    if (frame < 0) { // an error, not progress
        draw_backdrop(0, 320, 240, 0xD0);
        float tw = text_width(msg, 0.55f);
        draw_text_z((320 - tw) / 2, 92, 0.2f, 0.55f, C2D_Color32(0xFF, 0xB0, 0xA0, 0xFF), msg);
        const char *sub = "Press START to quit, then try another save.";
        draw_text_z((320 - text_width(sub, 0.45f)) / 2, 124, 0.2f, 0.45f, 0xFFFFFFFF, sub);
        return;
    }
    draw_backdrop(frame + 900, 320, 240, 0xB0);
    char line[64];
    int dots = (frame / 20) % 4;
    snprintf(line, sizeof(line), "%s%.*s", msg, dots, "...");
    float tw = text_width(msg, 0.6f);
    draw_text_z((320 - tw) / 2, 104, 0.2f, 0.6f, 0xFFFFFFFF, line);
    // a little progress shimmer
    float x = fmodf(frame * 2.0f, 360.0f) - 40;
    C2D_DrawRectSolid(60, 134, 0.1f, 200, 4, C2D_Color32(0x20, 0x2C, 0x40, 0xFF));
    float a = x < 60 ? 60 : x, b = x + 40 > 260 ? 260 : x + 40;
    if (b > a) C2D_DrawRectSolid(a, 134, 0.11f, b - a, 4, C2D_Color32(0xFF, 0xD8, 0x60, 0xFF));
}

void render_new_game_setup(const char *labels[SETUP_ROWS], const char *values[SETUP_ROWS],
                           const bool can_dec[SETUP_ROWS], const bool can_inc[SETUP_ROWS])
{
    u32 white = C2D_Color32(0xFF, 0xFF, 0xFF, 0xFF);
    u32 soft  = C2D_Color32(0xB8, 0xC8, 0xDC, 0xFF);
    C2D_DrawRectSolid(0, 0, 0.0f, 320, 240, C2D_Color32(0x10, 0x16, 0x22, 0xFF));
    C2D_DrawRectSolid(0, 0, 0.0f, 320, 28, C2D_Color32(0x2C, 0x50, 0x80, 0xFF));
    draw_text(8, 4, 0.6f, white, "New Game");
    for (int i = 0; i < SETUP_ROWS; i++) {
        float y = SETUP_ROW_Y0 + i * SETUP_ROW_H;
        draw_text(10, y + 1, 0.5f, soft, labels[i]);
        draw_button(SETUP_MINUS_X, y, SETUP_BTN_W, SETUP_BTN_H, 0.1f, "-", 0.7f, can_dec[i], false);
        float tw = text_width(values[i], 0.5f);
        float mid = (SETUP_MINUS_X + SETUP_BTN_W + SETUP_PLUS_X) / 2.0f;
        draw_text(mid - tw / 2, y + 1, 0.5f, white, values[i]);
        draw_button(SETUP_PLUS_X, y, SETUP_BTN_W, SETUP_BTN_H, 0.1f, "+", 0.7f, can_inc[i], false);
    }
    draw_button(SETUP_BACK_X, SETUP_FOOT_Y, SETUP_FOOT_W, SETUP_FOOT_H, 0.1f, "Back", 0.6f, true, false);
    draw_button(SETUP_START_X, SETUP_FOOT_Y, SETUP_FOOT_W, SETUP_FOOT_H, 0.1f, "Start", 0.6f, true, true);
}

void render_new_game_top(bool is_new_3ds, const char *summary)
{
    C2D_TextBufClear(g_textBuf);
    u32 white = C2D_Color32(0xFF, 0xFF, 0xFF, 0xFF);
    u32 soft  = C2D_Color32(0xB8, 0xC8, 0xDC, 0xFF);
    u32 label = C2D_Color32(0x8C, 0xA0, 0xB4, 0xFF);
    int f = (int)(osGetTime() / 17); // same for both eyes
    layer(0.0f);
    draw_backdrop(f, 400, 240, 0xC8);
    draw_text_z(16, 20, 0.2f, 0.9f, white, "Freeciv 3DS");
    draw_wrapped(summary, 16, 56, 368, 0.5f, soft, 4, 17, 0.2f);
    if (!strncmp(summary, "Pick a saved", 12)) return; // load picker: the hardware notes are for new games
    draw_text_z(16, 150, 0.2f, 0.45f, label, is_new_3ds
        ? "New 3DS detected: 804 MHz, room for big maps."
        : "Old 3DS detected: smaller maps keep turns quick.");
    draw_text_z(16, 170, 0.2f, 0.45f, label, "Larger maps and more rivals mean longer AI turns.");
}

void render_messages_panel(const GameMessage *msgs, int n, int scroll, int highlight)
{
    C2D_TextBufClear(g_textBuf);
    char right[24];
    snprintf(right, sizeof(right), "%d", n);
    panel_bg("Messages", right);
    draw_text(8, PANEL_HEADER_H + 8, 0.45f, C2D_Color32(0xA8, 0xB4, 0xC4, 0xFF),
              n ? "Newest first. Tap to read, tap again to go there." : "No news yet.");
    for (int i = 0; i < PANEL_ROWS_VISIBLE; i++) {
        int idx = scroll + i;
        if (idx >= n) break;
        const GameMessage *m = &msgs[idx];
        list_row(i, 4, 306, idx == highlight, true);
        float y = PANEL_LIST_Y + i * PANEL_ROW_H;
        C2D_DrawRectSolid(9, y + 7, 0.01f, 5, 5, msg_color(m->category));
        char t[160];
        snprintf(t, sizeof(t), "%s", m->text);
        while (t[0] && font_width(t, 0.42f) > 284) t[strlen(t) - 1] = 0;
        draw_text_z(18, y + 4, 0.01f, 0.42f, 0xFFFFFFFF, t);
    }
    scroll_thumb(n, scroll, 313);
    draw_button(4, PANEL_FOOTER_Y + 2, 312, 240 - PANEL_FOOTER_Y - 4, 0.02f, "Close", 0.5f, true, false);
}

void render_messages_top(const GameMessage *m)
{
    C2D_TextBufClear(g_textBuf);
    u32 soft = C2D_Color32(0xC0, 0xCE, 0xDC, 0xFF);
    C2D_DrawRectSolid(0, 0, 0.0f, 400, 32, C2D_Color32(0x14, 0x1E, 0x2E, 0xFF));
    C2D_DrawRectSolid(0, 0, 0.01f, 6, 32, C2D_Color32(0xFF, 0xD8, 0x60, 0xFF));
    C2D_DrawRectSolid(0, 32, 0.0f, 400, 1, C2D_Color32(0x6C, 0xA8, 0xE0, 0xFF));
    draw_text(14, 3, 0.85f, 0xFFFFFFFF, "Messages");
    layer(3.0f);
    C2D_DrawRectSolid(8, 44, 0.0f, 384, 166, C2D_Color32(0x10, 0x18, 0x26, 0xFF));
    if (m) {
        static const char *cats[] = { "News", "Combat", "Cities", "Science", "Diplomacy", "Good news" };
        char line[48];
        snprintf(line, sizeof(line), "%s - turn %d", cats[m->category % 6], m->turn);
        C2D_DrawRectSolid(16, 54, 0.01f, 8, 8, msg_color(m->category));
        draw_text(30, 48, 0.47f, msg_color(m->category), line);
        draw_wrapped(m->text, 18, 72, 364, 0.55f, 0xFFFFFFFF, 7, 19, 0.01f);
        if (m->x >= 0) draw_text(18, 190, 0.42f, soft, "Tap it again below to go there.");
    } else {
        draw_text(18, 52, 0.55f, soft, "Nothing has happened yet.");
    }
    layer(0.0f);
    C2D_DrawRectSolid(0, 240 - 17, 0.0f, 400, 17, C2D_Color32(0x0C, 0x12, 0x1C, 0xFF));
    draw_text(5, 240 - 16, 0.45f, soft, "D-Pad: scroll   B: close");
}

void render_pedia_panel(int kind, const PediaEntry *list, int n, int scroll, int selected)
{
    C2D_TextBufClear(g_textBuf);
    panel_bg("Civilopedia", NULL);
    static const char *tabs[PEDIA_KINDS] = { "Units", "Builds", "Wonders", "Techs", "Govts" };
    for (int i = 0; i < PEDIA_KINDS; i++)
        draw_button(PEDIA_TAB_X(i), PEDIA_TAB_Y, PEDIA_TAB_W, PEDIA_TAB_H, 0.03f, tabs[i], 0.42f, true, i == kind);
    draw_text(8, PEDIA_TAB_Y + PEDIA_TAB_H + 6, 0.42f, C2D_Color32(0xA8, 0xB4, 0xC4, 0xFF),
              "Tap an entry to read about it above.");
    for (int i = 0; i < PANEL_ROWS_VISIBLE; i++) {
        int idx = scroll + i;
        if (idx >= n) break;
        list_row(i, 4, 306, idx == selected, true);
        draw_text_z(12, PANEL_LIST_Y + i * PANEL_ROW_H + 3, 0.01f, 0.47f, 0xFFFFFFFF, list[idx].name);
    }
    scroll_thumb(n, scroll, 313);
    draw_button(4, PANEL_FOOTER_Y + 2, 312, 240 - PANEL_FOOTER_Y - 4, 0.02f, "Close", 0.5f, true, false);
}

static int pedia_icon_index(const char *tag)
{
    if (!tag || !tag[0]) return -1;
    int lo = 0, hi = PEDIA_ICON_COUNT - 1;
    while (lo <= hi) {
        int mid = (lo + hi) / 2, c = strcmp(k_pedia_icons[mid].tag, tag);
        if (!c) return k_pedia_icons[mid].idx;
        if (c < 0) lo = mid + 1; else hi = mid - 1;
    }
    return -1;
}

void render_pedia_top(const char *title, const char *text, int scroll_lines,
                      const char *icon_tag, const char *icon_alt, bool icon_is_unit)
{
    C2D_TextBufClear(g_textBuf);
    u32 soft = C2D_Color32(0xC0, 0xCE, 0xDC, 0xFF);
    C2D_DrawRectSolid(0, 0, 0.0f, 400, 32, C2D_Color32(0x14, 0x1E, 0x2E, 0xFF));
    C2D_DrawRectSolid(0, 0, 0.01f, 6, 32, C2D_Color32(0x70, 0xB8, 0xFF, 0xFF));
    C2D_DrawRectSolid(0, 32, 0.0f, 400, 1, C2D_Color32(0x6C, 0xA8, 0xE0, 0xFF));
    draw_text(14, 3, 0.85f, 0xFFFFFFFF, title && title[0] ? title : "Civilopedia");
    layer(3.0f);
    C2D_DrawRectSolid(8, 40, 0.0f, 384, 176, C2D_Color32(0x10, 0x18, 0x26, 0xFF));

    // The entry's picture, framed at the top right; text beside it wraps
    // narrower until it clears the frame. The icon sheet loads on first use.
    static C2D_SpriteSheet s_pedia_sheet;
    C2D_Image pic = { 0 };
    float pic_scale = 1.0f;
    if (icon_tag && icon_tag[0]) {
        if (icon_is_unit) {
            pic = C2D_SpriteSheetGetImage(g_spriteSheet, get_unit_sprite(icon_tag));
            pic_scale = 2.0f;
        } else {
            if (!s_pedia_sheet) s_pedia_sheet = C2D_SpriteSheetLoad("romfs:/gfx/pedia.t3x");
            int idx = pedia_icon_index(icon_tag);
            if (idx < 0) idx = pedia_icon_index(icon_alt);
            if (s_pedia_sheet && idx >= 0) {
                pic = C2D_SpriteSheetGetImage(s_pedia_sheet, idx);
                // whole-number scale that fits an 80x64 frame
                int w = pic.subtex->width, h = pic.subtex->height;
                pic_scale = 1.0f;
                while (w * (pic_scale + 1) <= 80 && h * (pic_scale + 1) <= 64) pic_scale += 1.0f;
            }
        }
    }
    float text_w_near = 368, pic_bottom = 0;
    if (pic.tex) {
        float pw = pic.subtex->width * pic_scale, ph = pic.subtex->height * pic_scale;
        float fx = 384 - 4 - 84, fy = 44;
        C2D_DrawRectSolid(fx, fy, 0.01f, 84, 68, C2D_Color32(0x1C, 0x28, 0x3A, 0xFF));
        C2D_DrawImageAt(pic, fx + (84 - pw) / 2, fy + (68 - ph) / 2, 0.02f, NULL, pic_scale, pic_scale);
        text_w_near = fx - 16 - 8;
        pic_bottom = fy + 68;
    }
    // Paragraph-by-paragraph wrap, skipping scroll_lines lines.
    float y = 44;
    int line_no = 0;
    const char *p = text ? text : "";
    char para[1200];
    while (*p && y < 204) {
        const char *e = strchr(p, '\n');
        int l = e ? (int)(e - p) : (int)strlen(p);
        if (l >= (int)sizeof(para)) l = sizeof(para) - 1;
        memcpy(para, p, l); para[l] = 0;
        p += l + (e ? 1 : 0);
        if (!para[0]) { if (line_no++ >= scroll_lines) y += 8; continue; }
        // wrap this paragraph into lines
        const char *q = para;
        while (*q) {
            char line[200];
            int best = 0, i = 0;
            while (q[i]) {
                int j = i;
                while (q[j] && q[j] != ' ') j++;
                if (j >= (int)sizeof(line)) break;
                memcpy(line, q, j); line[j] = 0;
                if (font_width(line, 0.47f) > (y < pic_bottom ? text_w_near : 368) && best) break;
                best = j;
                if (!q[j]) break;
                i = j + 1;
            }
            if (!best) best = strlen(q) < sizeof(line) - 1 ? (int)strlen(q) : (int)sizeof(line) - 1;
            memcpy(line, q, best); line[best] = 0;
            if (line_no++ >= scroll_lines && y < 204) {
                draw_text_z(16, y, 0.01f, 0.47f, 0xFFFFFFFF, line);
                y += 16;
            }
            q += best;
            while (*q == ' ') q++;
        }
    }
    layer(0.0f);
    C2D_DrawRectSolid(0, 240 - 17, 0.0f, 400, 17, C2D_Color32(0x0C, 0x12, 0x1C, 0xFF));
    draw_text(5, 240 - 16, 0.45f, soft, "L/R: scroll the text   D-Pad: list   B: close");
}

void render_spaceship_top(const SpaceshipInfo *s)
{
    C2D_TextBufClear(g_textBuf);
    u32 soft = C2D_Color32(0xC0, 0xCE, 0xDC, 0xFF), label = C2D_Color32(0x8C, 0xA0, 0xB4, 0xFF);
    char line[96];
    C2D_DrawRectSolid(0, 0, 0.0f, 400, 240, C2D_Color32(0x06, 0x08, 0x14, 0xFF));
    // a few stars
    for (int i = 0; i < 60; i++) {
        int x = (i * 97 + 13) % 400, y = (i * 53 + 29) % 240;
        C2D_DrawRectSolid(x, y, 0.0f, 1 + (i % 3 == 0), 1 + (i % 3 == 0), C2D_Color32(0xC0, 0xC8, 0xE0, 0xC0));
    }
    draw_text(14, 6, 0.85f, 0xFFFFFFFF, "Spaceship");
    layer(4.0f);
    C2D_DrawRectSolid(8, 44, 0.0f, 384, 170, C2D_Color32(0x10, 0x18, 0x26, 0xE8));
    static const char *states[] = { "Not started", "Under construction", "On its way", "Arrived" };
    draw_text(18, 50, 0.6f, 0xFFFFFFFF, states[s->state & 3]);
    if (s->state == 0) {
        draw_text(18, 80, 0.47f, soft, "Research Space Flight, then build the Apollo Program.");
        draw_text(18, 98, 0.47f, soft, "Cities can then build Structurals, Components and");
        draw_text(18, 116, 0.47f, soft, "Modules. Parts are fitted automatically each turn.");
    } else {
        snprintf(line, sizeof(line), "Structurals %d   Components %d   Modules %d", s->structurals, s->components, s->modules);
        draw_text(18, 80, 0.5f, soft, line);
        snprintf(line, sizeof(line), "Colonists %d", s->population);
        draw_text(18, 102, 0.5f, soft, line);
        snprintf(line, sizeof(line), "Chance of success: %d%%", s->success_pct);
        draw_text(18, 124, 0.55f, s->success_pct >= 80 ? C2D_Color32(0x90, 0xE8, 0x90, 0xFF) : C2D_Color32(0xFF, 0xD0, 0x60, 0xFF), line);
        snprintf(line, sizeof(line), "Travel time: %d.%d years", s->travel_tenths / 10, s->travel_tenths % 10);
        draw_text(18, 148, 0.5f, soft, line);
        if (s->state >= 2) {
            snprintf(line, sizeof(line), "Launched in %d", s->launch_year);
            draw_text(18, 170, 0.5f, label, line);
        }
    }
    layer(0.0f);
}

void render_spaceship_panel(const SpaceshipInfo *s)
{
    C2D_TextBufClear(g_textBuf);
    panel_bg("Spaceship", NULL);
    draw_text(8, PANEL_HEADER_H + 10, 0.47f, C2D_Color32(0xA8, 0xB4, 0xC4, 0xFF),
              s->can_launch ? "Launch when the chance of success is high enough."
              : s->state >= 2 ? "Your ship is on its way." : "Nothing to launch yet.");
    draw_button(4, PANEL_FOOTER_Y + 2, 152, 240 - PANEL_FOOTER_Y - 4, 0.02f, "Launch", 0.5f, s->can_launch, s->can_launch);
    draw_button(164, PANEL_FOOTER_Y + 2, 152, 240 - PANEL_FOOTER_Y - 4, 0.02f, "Close", 0.5f, true, false);
}

void render_game_over_top(bool won, int score, int turn)
{
    C2D_TextBufClear(g_textBuf);
    char line[64];
    C2D_DrawRectSolid(0, 0, 0.0f, 400, 240, won ? C2D_Color32(0x14, 0x22, 0x18, 0xFF) : C2D_Color32(0x22, 0x12, 0x14, 0xFF));
    layer(4.0f);
    const char *title = won ? "Victory!" : "Your civilization has fallen";
    draw_text(200 - text_width(title, 0.9f) / 2, 60, 0.9f, won ? C2D_Color32(0xFF, 0xE0, 0x60, 0xFF) : 0xFFFFFFFF, title);
    snprintf(line, sizeof(line), "Turn %d", turn);
    draw_text(200 - text_width(line, 0.55f) / 2, 110, 0.55f, C2D_Color32(0xC0, 0xCE, 0xDC, 0xFF), line);
    snprintf(line, sizeof(line), "Score %d", score);
    draw_text(200 - text_width(line, 0.7f) / 2, 136, 0.7f, 0xFFFFFFFF, line);
    layer(0.0f);
}

void render_game_over_bottom(void)
{
    C2D_DrawRectSolid(0, 0, 0.0f, 320, 240, C2D_Color32(0x10, 0x16, 0x22, 0xFF));
    draw_text(160 - text_width("The game is over.", 0.55f) / 2, 80, 0.55f, 0xFFFFFFFF, "The game is over.");
    draw_button(10, GAMEOVER_BTN_Y, 145, 44, 0.02f, "Look at the map", 0.5f, true, false);
    draw_button(165, GAMEOVER_BTN_Y, 145, 44, 0.02f, "Quit", 0.5f, true, true);
}

// "freeciv-T0012-Y-3450-auto.sav.gz" -> "Turn 12, 3450 BC  (autosave)";
// "3ds-T0012-Y-3450.sav.gz" -> "Turn 12, 3450 BC  (saved)".
void save_file_label(const char *fn, char *out, int n)
{
    int turn = 0, year = 0;
    bool manual = !strncmp(fn, "3ds-", 4);
    if (sscanf(fn, manual ? "3ds-T%d-Y%d" : "freeciv-T%d-Y%d", &turn, &year) == 2)
        snprintf(out, n, "Turn %d, %d %s%s", turn, year < 0 ? -year : year, year < 0 ? "BC" : "AD",
                 manual ? "  (saved)" : strstr(fn, "auto") ? "  (autosave)" : "");
    else if (manual && sscanf(fn, "3ds-T%d", &turn) == 1)
        snprintf(out, n, "Turn %d  (saved)", turn);
    else
        snprintf(out, n, "%.40s", fn);
}

void render_load_picker(const SaveFile *files, int n, int scroll)
{
    C2D_TextBufClear(g_textBuf);

    u32 white = C2D_Color32(0xE8, 0xE8, 0xE8, 0xFF);
    u32 dim   = C2D_Color32(0xA0, 0xA0, 0xA0, 0xFF);

    C2D_DrawRectSolid(0, 0, 0.0f, 320, PANEL_HEADER_H, C2D_Color32(0x2C, 0x50, 0x80, 0xFF));
    draw_text(8, 5, 0.6f, white, "Load Game");

    if (n == 0) {
        draw_text(8, PANEL_LIST_Y + 2, 0.5f, dim, "No saves found.");
    } else {
        ScrollRow rows[PANEL_ROWS_VISIBLE];
        int visible = 0;
        for (int i = 0; i < PANEL_ROWS_VISIBLE; i++) {
            int idx = scroll + i;
            if (idx >= n) break;
            save_file_label(files[idx].filename, rows[visible].text, sizeof(rows[visible].text));
            rows[visible].color = white;
            visible++;
        }
        draw_scroll_list(8, rows, visible);
    }

    if (n > 0) draw_text(8, PANEL_HEADER_H + 8, 0.45f, dim, "Newest first. Tap one to load it.");
    draw_button(4, PANEL_FOOTER_Y + 2, 312, 240 - PANEL_FOOTER_Y - 4, 0.02f, "Back", 0.5f, true, false);
}

// ---- Instruction manual ---------------------------------------------------
// Readable from the title screen (Manual) and the in-game Menu (Manual). The
// top screen is for reading: the section's text, scrolled with the Circle Pad
// or D-pad. The bottom screen is for choosing: a button per section, and
// Close. L / R step through sections, B or START closes. It reopens at the
// section and place it was left at. The text is romfs:/manual.txt; a
// manual.txt in sdmc:/3ds/freeciv/ replaces it. Format: "# " section,
// "## " heading, "- " list item, "1. " step, "| a | b" two-column row.

#define MAN_MAX_SECTIONS 24
#define MAN_MAX_LINES    600
#define MAN_TEXT_X       12
#define MAN_TEXT_W       376
#define MAN_TOP          28
#define MAN_BOTTOM       222
#define MAN_SCALE        0.47f
#define MAN_LH           16
enum { ML_TEXT, ML_HEAD, ML_ROW, ML_GAP };
typedef struct { unsigned char kind; unsigned char x; short h; char a[96]; char b[96]; } ManLine;

static char   *s_man_text;
static char    s_man_title[MAN_MAX_SECTIONS][40];
static const char *s_man_body[MAN_MAX_SECTIONS], *s_man_end[MAN_MAX_SECTIONS];
static int     s_man_n;
static int     s_man_sec, s_man_scroll;      // section, scroll in px
static ManLine *s_ml;
static int     s_ml_n, s_ml_sec = -1, s_ml_bias = -1, s_ml_h;

static bool man_parse(FILE *f)
{
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    s_man_text = malloc(n + 1);
    if (!s_man_text) { fclose(f); return false; }
    n = (long)fread(s_man_text, 1, n, f);
    fclose(f);
    s_man_text[n] = 0;
    char *start = s_man_text;
    if ((unsigned char)start[0] == 0xEF && (unsigned char)start[1] == 0xBB && (unsigned char)start[2] == 0xBF)
        start += 3; // UTF-8 byte order mark (Windows Notepad)
    s_man_n = 0;
    // Sections start at lines beginning with "# ".
    for (const char *p = start; *p; ) {
        const char *eol = strchr(p, '\n');
        const char *next = eol ? eol + 1 : p + strlen(p);
        if (p[0] == '#' && p[1] == ' ' && s_man_n < MAN_MAX_SECTIONS) {
            if (s_man_n > 0) s_man_end[s_man_n - 1] = p;
            int len = (int)((eol ? eol : next) - (p + 2));
            if (len > 0 && p[2 + len - 1] == '\r') len--;
            snprintf(s_man_title[s_man_n], sizeof(s_man_title[0]), "%.*s", len, p + 2);
            s_man_body[s_man_n] = next;
            s_man_n++;
        }
        p = next;
    }
    if (s_man_n > 0) s_man_end[s_man_n - 1] = s_man_text + n;
    if (s_man_n == 0) { free(s_man_text); s_man_text = NULL; return false; }
    return true;
}

static void man_load(void)
{
    if (s_man_text) return;
    // An SD-card copy replaces the built-in one, unless it has no "# " sections.
    FILE *f = fopen("sdmc:/3ds/freeciv/manual.txt", "rb");
    if (!f || !man_parse(f)) {
        f = fopen("romfs:/manual.txt", "rb");
        if (!f || !man_parse(f)) { s_man_text = calloc(1, 1); s_man_n = 0; }
    }
    if (!s_ml) s_ml = malloc(sizeof(ManLine) * MAN_MAX_LINES);
}

static void man_add(int kind, int x, int h, const char *a, const char *b)
{
    if (!s_ml || s_ml_n >= MAN_MAX_LINES) return;
    ManLine *l = &s_ml[s_ml_n++];
    l->kind = (unsigned char)kind; l->x = (unsigned char)x; l->h = (short)h;
    snprintf(l->a, sizeof(l->a), "%s", a ? a : "");
    snprintf(l->b, sizeof(l->b), "%s", b ? b : "");
    s_ml_h += h;
}

// Word-wraps text into lines of width w; calls back with each line.
static void man_wrap(const char *text, float w, float scale, int kind, int x, const char *first_a, bool row)
{
    char line[96];
    int n = 0;
    bool first = true;
    const char *p = text;
    while (*p) {
        while (*p == ' ') p++;
        const char *we = p;
        while (*we && *we != ' ') we++;
        int wl = (int)(we - p);
        char trial[96];
        snprintf(trial, sizeof(trial), "%.*s%s%.*s", n, line, n ? " " : "", wl, p);
        if (n && font_width(trial, scale) > w) {
            line[n] = 0;
            if (row) man_add(ML_ROW, x, MAN_LH, first ? first_a : "", line);
            else man_add(kind, x, MAN_LH, first ? first_a : "", line);
            first = false;
            n = 0;
            continue; // retry this word on a new line
        }
        snprintf(line, sizeof(line), "%s", trial);
        n = (int)strlen(line);
        p = we;
    }
    if (n || first) {
        line[n] = 0;
        man_add(row ? ML_ROW : kind, x, MAN_LH, first ? first_a : "", line);
    }
}

static void man_layout(void)
{
    if (s_ml_sec == s_man_sec && s_ml_bias == font_get_bias()) return;
    s_ml_sec = s_man_sec; s_ml_bias = font_get_bias();
    s_ml_n = 0; s_ml_h = 0;
    if (s_man_sec < 0 || s_man_sec >= s_man_n) return;
    const char *p = s_man_body[s_man_sec];
    const char *end = s_man_end[s_man_sec];
    char ln[400];
    while (p < end && *p) {
        const char *eol = p;
        while (eol < end && *eol && *eol != '\n') eol++;
        int len = (int)(eol - p);
        if (len > (int)sizeof(ln) - 1) len = sizeof(ln) - 1;
        memcpy(ln, p, len); ln[len] = 0;
        char *cr = strchr(ln, '\r'); if (cr) *cr = 0;
        p = *eol ? eol + 1 : eol;
        if (!ln[0]) { man_add(ML_GAP, 0, 6, NULL, NULL); continue; }
        if (!strncmp(ln, "## ", 3)) {
            if (s_ml_n > 0) man_add(ML_GAP, 0, 4, NULL, NULL);
            man_add(ML_HEAD, 0, 20, ln + 3, NULL);
        } else if (!strncmp(ln, "- ", 2)) {
            man_wrap(ln + 2, MAN_TEXT_W - 14, MAN_SCALE, ML_TEXT, 14, "\x01", false); // \x01 = bullet
        } else if (ln[0] >= '1' && ln[0] <= '9' && ln[1] == '.' && ln[2] == ' ') {
            char num[4] = { ln[0], '.', 0, 0 };
            man_wrap(ln + 3, MAN_TEXT_W - 18, MAN_SCALE, ML_TEXT, 18, num, false);
        } else if (ln[0] == '|') {
            char *a = ln + 1, *bar = strchr(a, '|');
            char *b = bar ? bar + 1 : (char *)"";
            if (bar) *bar = 0;
            while (*a == ' ') a++;
            for (char *e = a + strlen(a); e > a && e[-1] == ' '; ) *--e = 0;
            while (*b == ' ') b++;
            man_wrap(b, MAN_TEXT_W - 132, 0.45f, ML_ROW, 132, a, true);
        } else {
            man_wrap(ln, MAN_TEXT_W, MAN_SCALE, ML_TEXT, 0, "", false);
        }
    }
}

bool manual_available(void) { man_load(); return s_man_n > 0; }

static int man_max_scroll(void)
{
    int m = s_ml_h - (MAN_BOTTOM - MAN_TOP);
    return m > 0 ? m : 0;
}

void render_manual_top(void)
{
    C2D_TextBufClear(g_textBuf);
    man_load();
    man_layout();
    u32 white = 0xFFFFFFFF, soft = C2D_Color32(0xC0, 0xCE, 0xDC, 0xFF), gold = C2D_Color32(0xFF, 0xD8, 0x60, 0xFF);
    C2D_DrawRectSolid(0, 0, 0.0f, 400, 240, C2D_Color32(0x10, 0x16, 0x22, 0xFF));
    // Header and footer bands sit in front of the text, hiding what scrolls under them.
    C2D_DrawRectSolid(0, 0, 0.5f, 400, 24, C2D_Color32(0x2C, 0x50, 0x80, 0xFF));
    C2D_DrawRectSolid(0, 24, 0.5f, 400, 1, C2D_Color32(0x6C, 0xA8, 0xE0, 0xFF));
    char hdr[64];
    if (s_man_n == 0) {
        draw_text_z(10, 3, 0.51f, 0.6f, white, "Manual");
        draw_text(12, 40, 0.5f, soft, "The manual is missing or has no \"# \" sections.");
        return;
    }
    draw_text_z(10, 2, 0.51f, 0.6f, white, s_man_title[s_man_sec]);
    snprintf(hdr, sizeof(hdr), "%d of %d", s_man_sec + 1, s_man_n);
    draw_text_z(392 - text_width(hdr, 0.45f), 5, 0.51f, 0.45f, C2D_Color32(0xC8, 0xD8, 0xE8, 0xFF), hdr);
    C2D_DrawRectSolid(0, MAN_BOTTOM + 1, 0.5f, 400, 240 - MAN_BOTTOM - 1, C2D_Color32(0x0C, 0x12, 0x1C, 0xFF));
    draw_text_z(5, 240 - 16, 0.51f, 0.45f, soft, "D-Pad / Circle Pad: scroll   L / R: section   B: close");

    int y = MAN_TOP - s_man_scroll;
    for (int i = 0; i < s_ml_n; i++) {
        const ManLine *l = &s_ml[i];
        if (y + l->h > MAN_TOP - 2 && y < MAN_BOTTOM - 10) {
            float x = MAN_TEXT_X;
            switch (l->kind) {
            case ML_HEAD:
                draw_text_z(x, y + 2, 0.1f, 0.55f, gold, l->a);
                break;
            case ML_ROW:
                if (l->a[0]) draw_text_z(x, y, 0.1f, 0.45f, C2D_Color32(0xFF, 0xE8, 0xA0, 0xFF), l->a);
                draw_text_z(x + l->x, y, 0.1f, 0.45f, white, l->b);
                break;
            case ML_TEXT:
                if (l->a[0] == 1) C2D_DrawRectSolid(x + 3, y + 7, 0.1f, 4, 4, gold);
                else if (l->a[0]) draw_text_z(x, y, 0.1f, MAN_SCALE, gold, l->a);
                draw_text_z(x + l->x, y, 0.1f, MAN_SCALE, white, l->b);
                break;
            default: break;
            }
        }
        y += l->h;
    }
    // Scroll bar.
    int ms = man_max_scroll();
    if (ms > 0) {
        float track = MAN_BOTTOM - MAN_TOP, th = track * track / (track + ms);
        C2D_DrawRectSolid(395, MAN_TOP, 0.2f, 3, track, C2D_Color32(0x22, 0x2C, 0x3A, 0xFF));
        C2D_DrawRectSolid(395, MAN_TOP + (track - th) * s_man_scroll / ms, 0.21f, 3, th, C2D_Color32(0x6C, 0xA8, 0xE0, 0xFF));
    }
}

// Bottom screen: 3 columns x 5 rows of section buttons; Close fills the rest.
#define MAN_COLS 3
#define MAN_ROWS 5
static void man_cell(int i, float *x, float *y, float *w, float *h)
{
    float cw = (320 - 4.0f * (MAN_COLS + 1)) / MAN_COLS, ch = (240 - 4.0f * (MAN_ROWS + 1)) / MAN_ROWS;
    *x = 4 + (i % MAN_COLS) * (cw + 4); *y = 4 + (i / MAN_COLS) * (ch + 4); *w = cw; *h = ch;
}

static int man_hit(int px, int py)
{
    int ncell = s_man_n < MAN_COLS * MAN_ROWS ? s_man_n : MAN_COLS * MAN_ROWS - 1;
    for (int i = 0; i < MAN_COLS * MAN_ROWS; i++) {
        float x, y, w, h;
        man_cell(i, &x, &y, &w, &h);
        if (i >= ncell) { // Close: this cell to the end of the grid
            float x2, y2, w2, h2;
            man_cell(MAN_COLS * MAN_ROWS - 1, &x2, &y2, &w2, &h2);
            if (py >= y && py < y + h && px >= x && px < x2 + w2) return -2;
            return -1;
        }
        if (px >= x && px < x + w && py >= y && py < y + h) return i;
    }
    return -1;
}

void render_manual_bottom(int frame)
{
    C2D_TextBufClear(g_textBuf);
    man_load();
    draw_backdrop(frame + 900, 320, 240, 0xC8);
    int ncell = s_man_n < MAN_COLS * MAN_ROWS ? s_man_n : MAN_COLS * MAN_ROWS - 1;
    for (int i = 0; i < ncell; i++) {
        float x, y, w, h;
        man_cell(i, &x, &y, &w, &h);
        const char *t = s_man_title[i];
        float sc = 0.47f;
        while (sc > 0.36f && text_width(t, sc) > w - 8) sc -= 0.01f;
        draw_button(x, y, w, h, 0.12f, t, sc, true, i == s_man_sec);
    }
    float x, y, w, h, x2, y2, w2, h2;
    man_cell(ncell, &x, &y, &w, &h);
    man_cell(MAN_COLS * MAN_ROWS - 1, &x2, &y2, &w2, &h2);
    draw_button(x, y, x2 + w2 - x, h, 0.12f, "Close", 0.5f, true, false);
    C2D_DrawRectSolid(x, y, 0.13f, x2 + w2 - x, 2, C2D_Color32(0xE0, 0x60, 0x50, 0xFF));
}

bool manual_input(u32 kDown, u32 kHeld, bool touched, int px, int py, int circle_dy)
{
    man_load();
    if (kDown & (KEY_B | KEY_START)) return false;
    if (s_man_n == 0) return !(kDown & KEY_TOUCH);
    int prev = s_man_sec;
    if (kDown & KEY_L) s_man_sec = (s_man_sec + s_man_n - 1) % s_man_n;
    if (kDown & KEY_R) s_man_sec = (s_man_sec + 1) % s_man_n;
    if (touched) {
        int hit = man_hit(px, py);
        if (hit == -2) return false;
        if (hit >= 0) s_man_sec = hit;
    }
    if (s_man_sec != prev) s_man_scroll = 0;
    man_layout();
    if (kHeld & KEY_DUP)   s_man_scroll -= 4;
    if (kHeld & KEY_DDOWN) s_man_scroll += 4;
    if (circle_dy > 20 || circle_dy < -20) s_man_scroll -= circle_dy / 12; // up = scroll up
    int ms = man_max_scroll();
    if (s_man_scroll > ms) s_man_scroll = ms;
    if (s_man_scroll < 0) s_man_scroll = 0;
    return true;
}
