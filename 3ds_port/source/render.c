#include "render.h"
#include "game_loop.h"
#include "fc3ds_server.h"
#include "sprites.h"
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
    C2D_Text txt;
    C2D_TextParse(&txt, g_textBuf, str);
    C2D_TextOptimize(&txt);
    C2D_DrawText(&txt, C2D_WithColor, x, y, z, scale, scale, color);
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
static bool minimap_layout(float *dest_x, float *dest_y, float *S)
{
    if (!s_minimap_initialized || s_map_w <= 0 || s_map_h <= 0) return false;
    float sx = (float)MINIMAP_W / s_map_w, sy = (float)MINIMAP_H / s_map_h;
    *S = sx < sy ? sx : sy;
    *dest_x = MINIMAP_X + (MINIMAP_W - s_map_w * *S) / 2.0f;
    *dest_y = MINIMAP_Y + (MINIMAP_H - s_map_h * *S) / 2.0f;
    return true;
}

static void draw_minimap(int cam_x, int cam_y, float view_w_tiles, float view_h_tiles,
                         int sel_x, int sel_y)
{
    float dest_x, dest_y, S;
    C2D_DrawRectSolid(MINIMAP_X - 2, MINIMAP_Y - 2, 0.0f, MINIMAP_W + 4, MINIMAP_H + 4, C2D_Color32(0x3A, 0x55, 0x78, 255));
    C2D_DrawRectSolid(MINIMAP_X, MINIMAP_Y, 0.05f, MINIMAP_W, MINIMAP_H, C2D_Color32(0, 0, 0, 255));
    if (!minimap_layout(&dest_x, &dest_y, &S)) return;

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

// A translucent strip, for HUD text/buttons over the map.
static void hud_strip(float y, float w, float h)
{
    C2D_DrawRectSolid(0, y, 0.88f, w, h, C2D_Color32(0x08, 0x0C, 0x14, 0xC8));
}

// Text helpers that measure, for centring and right-aligning.
static float text_width(const char *str, float scale)
{
    C2D_Text t;
    C2D_TextParse(&t, g_textBuf, str);
    float w = 0, h = 0;
    C2D_TextGetDimensions(&t, scale, scale, &w, &h);
    return w;
}

static void draw_button(float x, float y, float w, float h, float z, const char *label,
                        float scale, bool enabled, bool highlight)
{
    u32 bg = highlight ? C2D_Color32(0x3C, 0x78, 0x40, 0xF0)
           : enabled   ? C2D_Color32(0x2C, 0x50, 0x80, 0xF0) : C2D_Color32(0x24, 0x2C, 0x38, 0xF0);
    u32 border = highlight ? C2D_Color32(0x8C, 0xE0, 0x90, 0xFF) : C2D_Color32(0x6C, 0xA8, 0xE0, 0xFF);
    C2D_DrawRectSolid(x, y, z, w, h, bg);
    C2D_DrawRectSolid(x, y, z + 0.001f, w, 2, border);
    float tw = text_width(label, scale);
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
    m->cols = m->n > 7 ? 2 : 1;
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
    draw_text_z(m->x + 5, m->y + 2, Z + 0.002f, 0.5f, C2D_Color32(0xFF, 0xE0, 0x80, 0xFF), m->title);
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
    if (px >= HUD_ZOUT_X && px < HUD_ZOUT_X + HUD_ZOOM_W) return HUD_ZOUT;
    if (px >= HUD_ZIN_X  && px < HUD_ZIN_X  + HUD_ZOOM_W) return HUD_ZIN;
    if (px >= HUD_END_X  && px < HUD_END_X  + HUD_END_W)  return HUD_END;
    return HUD_BAR;
}

void render_bottom_map(int cam_x, int cam_y, float scale, int sel_x, int sel_y,
                       int unit_x, int unit_y, bool unit_can_move, int goto_x, int goto_y,
                       const PopupMenu *popup, bool can_zoom_out, bool can_zoom_in, bool all_moved)
{
    render_map_view(cam_x, cam_y, scale, 320, 240, sel_x, sel_y, unit_x >= 0, goto_x, goto_y);

    // Where the commanded unit can step: a soft frame on each neighbour.
    if (unit_x >= 0 && unit_can_move && !popup) {
        int mw = 40, mh = 40;
        get_map_dimensions(&mw, &mh);
        float T = TILE_PX * scale;
        u32 c = C2D_Color32(0xFF, 0xFF, 0xFF, 0x70);
        for (int dy = -1; dy <= 1; dy++) for (int dx = -1; dx <= 1; dx++) {
            if (!dx && !dy) continue;
            int tx = unit_x + dx, ty = unit_y + dy;
            if (ty < 0 || ty >= mh) continue;
            if (!map_wraps_x() && (tx < 0 || tx >= mw)) continue;
            int wx = tx * TILE_PX;
            if (map_wraps_x()) {
                int world_w = mw * TILE_PX;
                int d = ((wx - cam_x + TILE_PX) % world_w + world_w) % world_w - TILE_PX;
                wx = cam_x + d;
            }
            float x = (wx - cam_x) * scale, y = (ty * TILE_PX - cam_y) * scale;
            float b = 1.5f, in = 3.0f;
            C2D_DrawRectSolid(x + in, y + in, 0.68f, T - 2 * in, b, c);
            C2D_DrawRectSolid(x + in, y + T - in - b, 0.68f, T - 2 * in, b, c);
            C2D_DrawRectSolid(x + in, y + in, 0.68f, b, T - 2 * in, c);
            C2D_DrawRectSolid(x + T - in - b, y + in, 0.68f, b, T - 2 * in, c);
        }
    }

    // HUD button row along the bottom edge.
    hud_strip(HUD_Y, 320, 240 - HUD_Y);
    const float Z = 0.9f;
    float by = HUD_Y + 2, bh = 240 - HUD_Y - 3;
    draw_button(HUD_MENU_X, by, HUD_MENU_W, bh, Z, "Menu", 0.5f, true, false);
    draw_button(HUD_NEXT_X, by, HUD_NEXT_W, bh, Z, "Next", 0.5f, !all_moved, false);
    draw_button(HUD_ZOUT_X, by, HUD_ZOOM_W, bh, Z, "-", 0.7f, can_zoom_out, false);
    draw_button(HUD_ZIN_X,  by, HUD_ZOOM_W, bh, Z, "+", 0.7f, can_zoom_in, false);
    draw_button(HUD_END_X,  by, HUD_END_W,  bh, Z, "End Turn", 0.5f, true, all_moved);

    if (popup) draw_popup(popup);
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
        snprintf(line, sizeof(line), "Turn %d", turn);
        draw_text(5, 1, 0.52f, white, line);
        if (ti->civ) {
            snprintf(line, sizeof(line), "Gold %d", ti->civ->gold);
            draw_text(78, 1, 0.52f, gold, line);
            snprintf(line, sizeof(line), "%s  Tax %d Lux %d Sci %d", ti->civ->government_name,
                     ti->civ->tax, ti->civ->lux, ti->civ->sci);
            draw_text(395 - text_width(line, 0.45f), 2, 0.45f, soft, line);
        }
    } else {
        draw_text(5, 1, 0.52f, white, "Generating world...");
    }

    // Left: minimap and research.
    draw_minimap(ti->cam_x, ti->cam_y, 320.0f / (TILE_PX * ti->zoom), (240.0f - (240 - HUD_Y)) / (TILE_PX * ti->zoom),
                 ti->tile_x, ti->tile_y);
    if (ti->civ && ti->civ->research_name[0]) {
        float y = MINIMAP_Y + MINIMAP_H + 6;
        draw_text(MINIMAP_X, y, 0.45f, label, "Researching");
        draw_text(MINIMAP_X, y + 13, 0.5f, white, ti->civ->research_name);
        int need = ti->civ->bulbs_needed > 0 ? ti->civ->bulbs_needed : 1;
        float frac = (float)ti->civ->bulbs / need;
        if (frac < 0) frac = 0;
        if (frac > 1) frac = 1;
        C2D_DrawRectSolid(MINIMAP_X, y + 31, 0.0f, MINIMAP_W, 6, C2D_Color32(0x24, 0x30, 0x40, 0xFF));
        C2D_DrawRectSolid(MINIMAP_X, y + 31, 0.01f, MINIMAP_W * frac, 6, C2D_Color32(0x50, 0xA0, 0xF0, 0xFF));
        if (ti->civ->research_turns >= 0)
            snprintf(line, sizeof(line), "%d/%d bulbs, %d turns", ti->civ->bulbs, ti->civ->bulbs_needed, ti->civ->research_turns);
        else
            snprintf(line, sizeof(line), "%d/%d bulbs", ti->civ->bulbs, ti->civ->bulbs_needed);
        draw_text(MINIMAP_X, y + 39, 0.42f, soft, line);
    }

    // Right: what's selected.
    const float CX = 206, CW = 190;
    C2D_DrawRectSolid(CX - 4, 24, 0.0f, CW + 6, 196, C2D_Color32(0x10, 0x18, 0x26, 0xFF));
    if (ti->unit && ti->unit->valid) {
        const UnitInfo *u = ti->unit;
        C2D_DrawRectSolid(CX - 4, 24, 0.01f, 4, 196, owner_color(u->owner));
        // Unit portrait, double size.
        C2D_DrawRectSolid(CX + 2, 30, 0.01f, 64, 64, C2D_Color32(0x1C, 0x28, 0x3A, 0xFF));
        C2D_DrawImageAt(C2D_SpriteSheetGetImage(g_spriteSheet, get_unit_sprite(u->rule_name)),
                        CX + 4, 32, 0.02f, NULL, 2.0f, 2.0f);
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
        y += 20;
        if (ti->stack_n > 1) {
            snprintf(line, sizeof(line), "Stack of %d units here", ti->stack_n);
            draw_text(CX + 2, y, 0.45f, label, line);
        }
    } else if (ti->tile && ti->tile_x >= 0) {
        const TileInfo *t = ti->tile;
        snprintf(line, sizeof(line), "Tile %d, %d", ti->tile_x, ti->tile_y);
        draw_text(CX + 2, 28, 0.45f, label, line);
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
                         is_pov_unit_owner(ti->stack[i].owner) && !ti->stack[i].can_move ? "  (done)" : "");
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

    // Hint line.
    C2D_DrawRectSolid(0, 240 - 17, 0.0f, 400, 17, C2D_Color32(0x0C, 0x12, 0x1C, 0xFF));
    if (ti->hint) draw_text(5, 240 - 16, 0.45f, ti->hint_color ? ti->hint_color : soft, ti->hint);
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

static void draw_tile_entities(const TileInfo *info, float draw_x, float draw_y)
{
    // City takes visual priority over a stacked unit.
    const float S = s_ms;
    if (info->city_owner >= 0) {
        C2D_DrawImageAt(C2D_SpriteSheetGetImage(g_spriteSheet, sprites_city_idx),
                        draw_x, draw_y, 0.6f, NULL, S, S);
        C2D_DrawRectSolid(draw_x + 11 * S, draw_y - 3 * S, 0.65f, 8 * S, 5 * S,
                          owner_color(info->city_owner));
    } else if (info->unit_owner >= 0) {
        int sprite_idx = get_unit_sprite(info->unit_name);
        C2D_DrawImageAt(C2D_SpriteSheetGetImage(g_spriteSheet, sprite_idx),
                        draw_x, draw_y, 0.6f, NULL, S, S);
        C2D_DrawRectSolid(draw_x + 11 * S, draw_y - 3 * S, 0.65f, 8 * S, 5 * S,
                          owner_color(info->unit_owner));
        if (info->num_units > 1) {
            // Stack marker: a small "+" badge, so a stack reads as more than one unit.
            C2D_DrawRectSolid(draw_x + 22 * S, draw_y + 1 * S, 0.66f, 7 * S, 7 * S, C2D_Color32(0, 0, 0, 0xC0));
            C2D_DrawRectSolid(draw_x + 23 * S, draw_y + 4 * S, 0.67f, 5 * S, 1 * S + 0.5f, 0xFFFFFFFF);
            C2D_DrawRectSolid(draw_x + 25 * S, draw_y + 2 * S, 0.67f, 1 * S + 0.5f, 5 * S, 0xFFFFFFFF);
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

            // Draw borders
            if (info.tile_owner != -1) {
                u32 bcolor = owner_color(info.tile_owner);
                C2D_ImageTint tint;
                C2D_PlainImageTint(&tint, bcolor, 1.0f);
                
                if (info.border_n) {
                    C2D_DrawImageAt(C2D_SpriteSheetGetImage(g_spriteSheet, sprites_grid_borders_n_idx), draw_x, draw_y, 0.6f, &tint, SC, SC);
                }
                if (info.border_s) {
                    C2D_DrawImageAt(C2D_SpriteSheetGetImage(g_spriteSheet, sprites_grid_borders_s_idx), draw_x, draw_y, 0.6f, &tint, SC, SC);
                }
                if (info.border_e) {
                    C2D_DrawImageAt(C2D_SpriteSheetGetImage(g_spriteSheet, sprites_grid_borders_e_idx), draw_x, draw_y, 0.6f, &tint, SC, SC);
                }
                if (info.border_w) {
                    C2D_DrawImageAt(C2D_SpriteSheetGetImage(g_spriteSheet, sprites_grid_borders_w_idx), draw_x, draw_y, 0.6f, &tint, SC, SC);
                }
            }

            // Draw entities based on visibility
            if (info.visibility == 2) {
                draw_tile_entities(&info, draw_x, draw_y);
            } else {
                // Under Fog of War (visibility == 1), we still draw city centers but no units
                if (info.city_owner != -1) {
                    C2D_DrawImageAt(C2D_SpriteSheetGetImage(g_spriteSheet, sprites_city_idx), draw_x, draw_y, 0.7f, NULL, SC, SC);
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
}

// Garrison rows visible at once on the top-screen readout before falling
// back to a "+N more" tail - the panel isn't scrollable (it's a non-touch,
// D-Pad-free readout; L/R already drive garrison_sel, see main.c), so a
// selection past this window scrolls the *window*, not the list.
#define CITY_GARRISON_VISIBLE 3

void render_city_top(const CityInfo *info, int garrison_sel)
{
    C2D_TextBufClear(g_textBuf);

    u32 white = C2D_Color32(0xFF, 0xFF, 0xFF, 0xFF);
    u32 dim   = C2D_Color32(0xC0, 0xCE, 0xDC, 0xFF);
    u32 label = C2D_Color32(0x8C, 0xA0, 0xB4, 0xFF);
    char line[160]; // wide enough for "In city (N): <garrison names>"

    C2D_DrawRectSolid(0, 0, 0.0f, 400, 30, owner_color(info->owner));
    C2D_DrawRectSolid(0, 30, 0.0f, 400, 2, C2D_Color32(0x6C, 0xA8, 0xE0, 0xFF));
    draw_text(8, 5, 0.8f, white, info->name[0] ? info->name : "?");
    snprintf(line, sizeof(line), "Size %d", info->size);
    draw_text(320, 8, 0.6f, white, line);

    // surplus[] index order is documented on CityInfo: food,shield,trade,gold,lux,sci.
    snprintf(line, sizeof(line), "Food %+d  Shield %+d  Trade %+d",
             info->surplus[0], info->surplus[1], info->surplus[2]);
    draw_text(8, 44, 0.6f, dim, line);
    snprintf(line, sizeof(line), "Gold %+d  Lux %+d  Sci %+d",
             info->surplus[3], info->surplus[4], info->surplus[5]);
    draw_text(8, 68, 0.6f, dim, line);

    snprintf(line, sizeof(line), "Food: %d/%d", info->food_stock, info->food_needed);
    draw_text(8, 96, 0.6f, dim, line);
    if (info->grow_turns >= 0) {
        snprintf(line, sizeof(line), "grow in %d", info->grow_turns);
    } else {
        snprintf(line, sizeof(line), "not growing");
    }
    draw_text(150, 96, 0.55f, label, line);

    snprintf(line, sizeof(line), "Building: %s", info->producing[0] ? info->producing : "?");
    draw_text(8, 122, 0.6f, white, line);
    snprintf(line, sizeof(line), "%d/%d shields (%d turns)",
             info->shield_stock, info->prod_cost, info->prod_turns);
    draw_text(8, 146, 0.55f, dim, line);

    // Garrison: pov units in the city, one per line, highlighting the
    // L/R-cycled ui_garrison_sel entry so "Y: take a unit out" acts on a
    // specific unit, not always the first (handoff.md §3).
    if (info->garrison_count > 0) {
        snprintf(line, sizeof(line), "In city (%d):", info->garrison_count);
        draw_text(8, 168, 0.5f, label, line);

        int window = (garrison_sel / CITY_GARRISON_VISIBLE) * CITY_GARRISON_VISIBLE;
        int shown = info->garrison_count - window;
        if (shown > CITY_GARRISON_VISIBLE) shown = CITY_GARRISON_VISIBLE;
        for (int i = 0; i < shown; i++) {
            int idx = window + i;
            bool sel = (idx == garrison_sel);
            snprintf(line, sizeof(line), "%s%s", sel ? "> " : "  ", info->garrison_names[idx]);
            draw_text(70, 168 + i * 11, 0.5f,
                      sel ? C2D_Color32(0xFF, 0xE0, 0x60, 0xFF) : dim, line);
        }
        if (window + shown < info->garrison_count) {
            snprintf(line, sizeof(line), "+%d more", info->garrison_count - (window + shown));
            draw_text(70, 168 + shown * 11, 0.45f, label, line);
        }
    } else {
        draw_text(8, 168, 0.5f, label, "In city: (no units)");
    }

    C2D_DrawText(&s_txt_tap_row_hint, C2D_WithColor, 8, 212, 0.0f, 0.5f, 0.5f, label);
    draw_text(8, 228, 0.5f, label,
              info->garrison_count > 0 ? "B: Close  L/R: pick  Y: take out" : "B: Close");
}

void draw_scroll_list(int x, const ScrollRow *rows, int count)
{
    for (int i = 0; i < count; i++) {
        float y = PANEL_LIST_Y + i * PANEL_ROW_H;
        draw_text(x, y + 2, 0.45f, rows[i].color, rows[i].text);
    }
}

void render_city_panel(const CityInfo *info, const BuildOption *opts, int n, int scroll,
                       const WorklistEntry *worklist, int worklist_n, bool queue_mode)
{
    C2D_TextBufClear(g_textBuf);

    u32 white = C2D_Color32(0xE8, 0xE8, 0xE8, 0xFF);
    u32 dim   = C2D_Color32(0xA0, 0xA0, 0xA0, 0xFF);
    u32 cur   = C2D_Color32(0xFF, 0xE0, 0x60, 0xFF);
    char line[96];

    C2D_DrawRectSolid(0, 0, 0.0f, 320, PANEL_HEADER_H, C2D_Color32(0x2C, 0x50, 0x80, 0xFF));
    draw_text(8, 5, 0.6f, white, info->name[0] ? info->name : "?");

    // NOW/QUEUE toggle (handoff.md §6.4) - switches what tapping a buildable
    // row below does. Sits in the header band, well clear of the list/footer
    // rows main.c hit-tests separately.
    C2D_DrawRectSolid(CITY_TOGGLE_X, CITY_TOGGLE_Y, 0.0f, CITY_TOGGLE_W, CITY_TOGGLE_H,
                      queue_mode ? C2D_Color32(0x80, 0x60, 0x20, 0xFF) : C2D_Color32(0x20, 0x60, 0x30, 0xFF));
    draw_text(CITY_TOGGLE_X + 8, CITY_TOGGLE_Y + 4, 0.45f, white, queue_mode ? "QUEUE" : "NOW");

    snprintf(line, sizeof(line), "Now: %s (%d/%d, %d turns)",
             info->producing[0] ? info->producing : "?",
             info->shield_stock, info->prod_cost, info->prod_turns);
    draw_text(8, PANEL_HEADER_H + 4, 0.45f, white, line);
    draw_text(8, PANEL_HEADER_H + 24, 0.45f, dim,
              queue_mode ? "Tap: add to queue ->" : "Tap: build now ->");

    // Buildable list (left column) - names truncated so they never run into
    // the worklist column starting at CITY_WORKLIST_COL_X.
    ScrollRow rows[PANEL_ROWS_VISIBLE];
    int visible = 0;
    for (int i = 0; i < PANEL_ROWS_VISIBLE; i++) {
        int idx = scroll + i;
        if (idx >= n) break;
        const BuildOption *opt = &opts[idx];
        snprintf(rows[visible].text, sizeof(rows[visible].text), "%s%.13s(%d)",
                 opt->is_current ? "> " : "  ", opt->name, opt->cost);
        rows[visible].color = opt->is_current ? cur : white;
        visible++;
    }
    draw_scroll_list(8, rows, visible);

    // Worklist column (right, "beside" the buildables list per §6.4) - not
    // independently scrollable in v1, so only the first PANEL_ROWS_VISIBLE
    // queued items show; "x" hints that tapping a row removes it.
    C2D_DrawRectSolid(CITY_WORKLIST_COL_X - 4, PANEL_LIST_Y, 0.0f, 2,
                      PANEL_ROW_H * PANEL_ROWS_VISIBLE, C2D_Color32(0x40, 0x48, 0x54, 0xFF));
    ScrollRow wrows[PANEL_ROWS_VISIBLE];
    int wvisible = 0;
    for (int i = 0; i < PANEL_ROWS_VISIBLE && i < worklist_n; i++) {
        snprintf(wrows[wvisible].text, sizeof(wrows[wvisible].text), "x %.10s", worklist[i].name);
        wrows[wvisible].color = dim;
        wvisible++;
    }
    draw_scroll_list(CITY_WORKLIST_COL_X + 4, wrows, wvisible);

    C2D_DrawRectSolid(0, PANEL_FOOTER_Y, 0.0f, 320, 240 - PANEL_FOOTER_Y, C2D_Color32(0x20, 0x20, 0x20, 0xFF));
    if (info->can_buy) {
        snprintf(line, sizeof(line), "Buy (%d g)", info->buy_cost);
        draw_text(12, PANEL_FOOTER_Y + 6, 0.55f, C2D_Color32(0x60, 0xE0, 0x60, 0xFF), line);
    } else {
        C2D_DrawText(&s_txt_buy, C2D_WithColor, 12, PANEL_FOOTER_Y + 6, 0.0f, 0.55f, 0.55f, dim);
    }
    C2D_DrawText(&s_txt_y_unit_out, C2D_WithColor, 110, PANEL_FOOTER_Y + 8, 0.0f, 0.42f, 0.42f, dim);
    C2D_DrawText(&s_txt_close, C2D_WithColor, 200, PANEL_FOOTER_Y + 6, 0.0f, 0.55f, 0.55f, white);
}

void render_civ_top(const CivInfo *civ, const char *title)
{
    C2D_TextBufClear(g_textBuf);

    u32 white = C2D_Color32(0xFF, 0xFF, 0xFF, 0xFF);
    u32 dim   = C2D_Color32(0xC0, 0xCE, 0xDC, 0xFF);
    u32 label = C2D_Color32(0x8C, 0xA0, 0xB4, 0xFF);
    char line[96];

    C2D_DrawRectSolid(0, 0, 0.0f, 400, 30, C2D_Color32(0x2C, 0x50, 0x80, 0xFF));
    C2D_DrawRectSolid(0, 30, 0.0f, 400, 2, C2D_Color32(0x6C, 0xA8, 0xE0, 0xFF));
    C2D_Text *title_txt = &s_txt_research_title;
    if (strcmp(title, "Tax & Science") == 0) title_txt = &s_txt_tax_science_title;
    else if (strcmp(title, "Government") == 0) title_txt = &s_txt_gov_title;
    else if (strcmp(title, "Diplomacy") == 0) title_txt = &s_txt_diplo_title;
    C2D_DrawText(title_txt, C2D_WithColor, 8, 5, 0.0f, 0.85f, 0.85f, white);

    snprintf(line, sizeof(line), "Gold: %d", civ->gold);
    draw_text(8, 44, 0.65f, white, line);
    snprintf(line, sizeof(line), "Tax %d%%  Lux %d%%  Sci %d%%  (max %d%%)",
             civ->tax, civ->lux, civ->sci, civ->max_rate);
    draw_text(8, 70, 0.55f, dim, line);

    snprintf(line, sizeof(line), "Researching: %s", civ->research_name[0] ? civ->research_name : "?");
    draw_text(8, 100, 0.6f, white, line);
    if (civ->research_turns >= 0) {
        snprintf(line, sizeof(line), "%d/%d bulbs (%d turns)", civ->bulbs, civ->bulbs_needed, civ->research_turns);
    } else {
        snprintf(line, sizeof(line), "%d/%d bulbs", civ->bulbs, civ->bulbs_needed);
    }
    draw_text(8, 124, 0.55f, dim, line);

    snprintf(line, sizeof(line), "Government: %s", civ->government_name[0] ? civ->government_name : "?");
    draw_text(8, 150, 0.6f, white, line);
    if (civ->target_government_id >= 0) {
        snprintf(line, sizeof(line), "Revolution: %d turn%s left",
                 civ->revolution_turns, civ->revolution_turns == 1 ? "" : "s");
        draw_text(8, 174, 0.55f, dim, line);
    }

    draw_text(8, 218, 0.5f, label, "B: Close");
}

void render_research_panel(const TechOption *opts, int n, int scroll)
{
    C2D_TextBufClear(g_textBuf);

    u32 white = C2D_Color32(0xE8, 0xE8, 0xE8, 0xFF);
    u32 dim   = C2D_Color32(0xA0, 0xA0, 0xA0, 0xFF);

    C2D_DrawRectSolid(0, 0, 0.0f, 320, PANEL_HEADER_H, C2D_Color32(0x2C, 0x50, 0x80, 0xFF));
    C2D_DrawText(&s_txt_choose_research_header, C2D_WithColor, 8, 5, 0.0f, 0.6f, 0.6f, white);

    ScrollRow rows[PANEL_ROWS_VISIBLE];
    int visible = 0;
    for (int i = 0; i < PANEL_ROWS_VISIBLE; i++) {
        int idx = scroll + i;
        if (idx >= n) break;
        snprintf(rows[visible].text, sizeof(rows[visible].text), "%s", opts[idx].name);
        rows[visible].color = white;
        visible++;
    }
    draw_scroll_list(8, rows, visible);

    C2D_DrawRectSolid(0, PANEL_FOOTER_Y, 0.0f, 320, 240 - PANEL_FOOTER_Y, C2D_Color32(0x20, 0x20, 0x20, 0xFF));
    C2D_DrawText(&s_txt_y_tree_view, C2D_WithColor, 12, PANEL_FOOTER_Y + 8, 0.0f, 0.42f, 0.42f, dim);
    C2D_DrawText(&s_txt_close, C2D_WithColor, 200, PANEL_FOOTER_Y + 6, 0.0f, 0.55f, 0.55f, white);
}

// Shared row-walk for the tiered tech-tree view (handoff.md §8.4), used
// identically by render_tech_tree_panel() (draws) and techtree_hit_test()
// (hit-tests) so they can never drift apart. nodes must be tier-sorted
// (get_tech_tree()'s contract). Starting at *idx, finds the run of entries
// sharing one tier - that run is exactly one displayable row - and reports
// its [start, end) range plus the tier value; advances *idx past it.
static void tech_tier_row_range(const TechNode *nodes, int n, int *idx, int *start, int *end, int *tier)
{
    *start = *idx;
    *tier = nodes[*idx].tier;
    int i = *idx;
    while (i < n && nodes[i].tier == *tier) {
        i++;
    }
    *end = i;
    *idx = i;
}

void render_tech_tree_panel(const TechNode *nodes, int n, int scroll)
{
    C2D_TextBufClear(g_textBuf);

    u32 white  = C2D_Color32(0xE8, 0xE8, 0xE8, 0xFF);
    u32 dim    = C2D_Color32(0xA0, 0xA0, 0xA0, 0xFF);
    u32 bright = C2D_Color32(0x60, 0xE0, 0x60, 0xFF); // PREREQS_KNOWN - tappable
    u32 known  = C2D_Color32(0x50, 0x80, 0x60, 0xFF); // KNOWN - dim green, non-tappable
    u32 locked = C2D_Color32(0x60, 0x60, 0x60, 0xFF); // UNKNOWN - grey, non-tappable
    char label[8];

    C2D_DrawRectSolid(0, 0, 0.0f, 320, PANEL_HEADER_H, C2D_Color32(0x2C, 0x50, 0x80, 0xFF));
    C2D_DrawText(&s_txt_tech_tree_header, C2D_WithColor, 8, 5, 0.0f, 0.6f, 0.6f, white);

    int idx = 0, tier_row = 0, display_row = 0;
    while (idx < n && display_row < PANEL_ROWS_VISIBLE) {
        int start, end, tier;
        tech_tier_row_range(nodes, n, &idx, &start, &end, &tier);
        if (tier_row++ < scroll) {
            continue;
        }
        float y = PANEL_LIST_Y + display_row * PANEL_ROW_H;

        snprintf(label, sizeof(label), "T%d", tier);
        draw_text(2, y + 2, 0.4f, dim, label);

        int shown = end - start;
        if (shown > TECHTREE_MAX_BOXES_PER_ROW) {
            shown = TECHTREE_MAX_BOXES_PER_ROW;
        }
        for (int i = 0; i < shown; i++) {
            const TechNode *node = &nodes[start + i];
            float bx = TECHTREE_TIER_LABEL_W + i * (TECHTREE_BOX_W + TECHTREE_BOX_GAP);
            // TechNode.state is a plain int (see its comment for the
            // TECH_UNKNOWN/PREREQS_KNOWN/KNOWN = 0/1/2 mapping) - render.c
            // doesn't include freeciv's research.h, same as BuildOption.kind
            // staying a plain int here instead of pulling in VUT_* enums.
            u32 color = (node->state == 2) ? known
                      : (node->state == 1) ? bright : locked;
            C2D_DrawRectSolid(bx, y, 0.0f, TECHTREE_BOX_W, PANEL_ROW_H - 2, C2D_Color32(0x20, 0x20, 0x20, 0xFF));
            char nm[10];
            snprintf(nm, sizeof(nm), "%.8s", node->name);
            draw_text(bx + 2, y + 2, 0.35f, color, nm);
        }
        if (end - start > shown) {
            float bx = TECHTREE_TIER_LABEL_W + shown * (TECHTREE_BOX_W + TECHTREE_BOX_GAP);
            char more[16];
            snprintf(more, sizeof(more), "+%d", end - start - shown);
            draw_text(bx + 2, y + 2, 0.35f, dim, more);
        }

        display_row++;
    }

    C2D_DrawRectSolid(0, PANEL_FOOTER_Y, 0.0f, 320, 240 - PANEL_FOOTER_Y, C2D_Color32(0x20, 0x20, 0x20, 0xFF));
    C2D_DrawText(&s_txt_y_flat_list, C2D_WithColor, 12, PANEL_FOOTER_Y + 8, 0.0f, 0.42f, 0.42f, dim);
    C2D_DrawText(&s_txt_close, C2D_WithColor, 200, PANEL_FOOTER_Y + 6, 0.0f, 0.55f, 0.55f, white);
}

int techtree_hit_test(const TechNode *nodes, int n, int scroll, int px, int py)
{
    int rel_y = py - PANEL_LIST_Y;
    if (rel_y < 0 || rel_y >= PANEL_ROW_H * PANEL_ROWS_VISIBLE) {
        return -1;
    }
    if (px < TECHTREE_TIER_LABEL_W) {
        return -1; // over the "T<n>" label, not a box - and guards the % below against a negative numerator
    }
    int target_display_row = rel_y / PANEL_ROW_H;

    int idx = 0, tier_row = 0, display_row = 0;
    while (idx < n) {
        int start, end, tier;
        tech_tier_row_range(nodes, n, &idx, &start, &end, &tier);
        if (tier_row++ < scroll) {
            continue;
        }
        if (display_row == target_display_row) {
            int shown = end - start;
            if (shown > TECHTREE_MAX_BOXES_PER_ROW) {
                shown = TECHTREE_MAX_BOXES_PER_ROW;
            }
            int box = (px - TECHTREE_TIER_LABEL_W) / (TECHTREE_BOX_W + TECHTREE_BOX_GAP);
            int box_x_rel = (px - TECHTREE_TIER_LABEL_W) % (TECHTREE_BOX_W + TECHTREE_BOX_GAP);
            if (box >= 0 && box < shown && box_x_rel < TECHTREE_BOX_W) {
                return start + box;
            }
            return -1;
        }
        display_row++;
    }
    return -1;
}

void render_rates_panel(const CivInfo *civ)
{
    C2D_TextBufClear(g_textBuf);

    u32 white = C2D_Color32(0xE8, 0xE8, 0xE8, 0xFF);
    u32 dim   = C2D_Color32(0xA0, 0xA0, 0xA0, 0xFF);
    char line[48];

    C2D_DrawRectSolid(0, 0, 0.0f, 320, PANEL_HEADER_H, C2D_Color32(0x2C, 0x50, 0x80, 0xFF));
    C2D_DrawText(&s_txt_tax_science_rates_header, C2D_WithColor, 8, 5, 0.0f, 0.6f, 0.6f, white);

    snprintf(line, sizeof(line), "Tax:      %d%%", civ->tax);
    draw_text(8, RATES_TAX_Y, 0.6f, white, line);
    draw_text(RATES_BTN_MINUS_X, RATES_TAX_Y, 0.6f, white, "-");
    draw_text(RATES_BTN_PLUS_X, RATES_TAX_Y, 0.6f, white, "+");

    snprintf(line, sizeof(line), "Luxury:   %d%%", civ->lux);
    draw_text(8, RATES_LUX_Y, 0.6f, white, line);
    draw_text(RATES_BTN_MINUS_X, RATES_LUX_Y, 0.6f, white, "-");
    draw_text(RATES_BTN_PLUS_X, RATES_LUX_Y, 0.6f, white, "+");

    snprintf(line, sizeof(line), "Science:  %d%%  (auto)", civ->sci);
    draw_text(8, RATES_SCI_Y, 0.6f, dim, line);
    snprintf(line, sizeof(line), "Max rate: %d%%", civ->max_rate);
    draw_text(8, RATES_SCI_Y + 30, 0.5f, dim, line);

    C2D_DrawRectSolid(0, PANEL_FOOTER_Y, 0.0f, 320, 240 - PANEL_FOOTER_Y, C2D_Color32(0x20, 0x20, 0x20, 0xFF));
    C2D_DrawText(&s_txt_close, C2D_WithColor, 200, PANEL_FOOTER_Y + 6, 0.0f, 0.55f, 0.55f, white);
}

void render_government_panel(const GovOption *opts, int n, int scroll, int current_gov_id)
{
    C2D_TextBufClear(g_textBuf);

    u32 white = C2D_Color32(0xE8, 0xE8, 0xE8, 0xFF);
    u32 dim   = C2D_Color32(0xA0, 0xA0, 0xA0, 0xFF);
    u32 grey  = C2D_Color32(0x60, 0x60, 0x60, 0xFF);
    u32 cur   = C2D_Color32(0xFF, 0xE0, 0x60, 0xFF);

    C2D_DrawRectSolid(0, 0, 0.0f, 320, PANEL_HEADER_H, C2D_Color32(0x2C, 0x50, 0x80, 0xFF));
    C2D_DrawText(&s_txt_choose_government_header, C2D_WithColor, 8, 5, 0.0f, 0.6f, 0.6f, white);

    ScrollRow rows[PANEL_ROWS_VISIBLE];
    int visible = 0;
    for (int i = 0; i < PANEL_ROWS_VISIBLE; i++) {
        int idx = scroll + i;
        if (idx >= n) break;
        const GovOption *opt = &opts[idx];
        bool is_current = opt->gov_id == current_gov_id;
        snprintf(rows[visible].text, sizeof(rows[visible].text), "%s%s", is_current ? "> " : "  ", opt->name);
        rows[visible].color = is_current ? cur : (opt->available ? white : grey);
        visible++;
    }
    draw_scroll_list(8, rows, visible);

    C2D_DrawRectSolid(0, PANEL_FOOTER_Y, 0.0f, 320, 240 - PANEL_FOOTER_Y, C2D_Color32(0x20, 0x20, 0x20, 0xFF));
    C2D_DrawText(&s_txt_gov_revolt_hint, C2D_WithColor, 12, PANEL_FOOTER_Y + 8, 0.0f, 0.38f, 0.38f, dim);
    C2D_DrawText(&s_txt_close, C2D_WithColor, 200, PANEL_FOOTER_Y + 6, 0.0f, 0.55f, 0.55f, white);
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
    const float TOAST_Z = 0.9f;
    int w = 140;
    C2D_DrawRectSolid(8, 8, TOAST_Z, w, 22, C2D_Color32(0x20, 0x60, 0x30, 0xE0));
    draw_text_z(16, 13, TOAST_Z, 0.5f, C2D_Color32(0xFF, 0xFF, 0xFF, 0xFF), text);
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

void render_diplomacy_top(int stage, const char *counterpart_name, const DiploInfo *diplo)
{
    C2D_TextBufClear(g_textBuf);

    u32 white = C2D_Color32(0xFF, 0xFF, 0xFF, 0xFF);
    u32 dim   = C2D_Color32(0xC0, 0xCE, 0xDC, 0xFF);
    u32 label = C2D_Color32(0x8C, 0xA0, 0xB4, 0xFF);
    char line[96];

    C2D_DrawRectSolid(0, 0, 0.0f, 400, 30, C2D_Color32(0x2C, 0x50, 0x80, 0xFF));
    C2D_DrawRectSolid(0, 30, 0.0f, 400, 2, C2D_Color32(0x6C, 0xA8, 0xE0, 0xFF));
    draw_text(8, 5, 0.85f, white, "Diplomacy");

    if (stage == 1) {
        draw_text(8, 44, 0.6f, dim, "Pick a player to negotiate with.");
    } else {
        snprintf(line, sizeof(line), "Negotiating with: %s", counterpart_name);
        draw_text(8, 44, 0.65f, white, line);
        snprintf(line, sizeof(line), "You: %s   Them: %s",
                 diplo->accept_self ? "Accepted" : "Pending",
                 diplo->accept_other ? "Accepted" : "Pending");
        draw_text(8, 70, 0.55f, dim, line);
        snprintf(line, sizeof(line), "%d clause%s proposed", diplo->clause_count,
                 diplo->clause_count == 1 ? "" : "s");
        draw_text(8, 96, 0.55f, dim, line);
    }

    draw_text(8, 218, 0.5f, label, "B: Close");
}

void render_diplomacy_panel(int stage, const char *counterpart_name,
                            const PlayerOption *players, int player_count, int scroll,
                            const DiploInfo *diplo)
{
    C2D_TextBufClear(g_textBuf);

    u32 white  = C2D_Color32(0xE8, 0xE8, 0xE8, 0xFF);
    u32 dim    = C2D_Color32(0xA0, 0xA0, 0xA0, 0xFF);
    u32 accent = C2D_Color32(0x60, 0xE0, 0x60, 0xFF);
    char line[64];

    C2D_DrawRectSolid(0, 0, 0.0f, 320, PANEL_HEADER_H, C2D_Color32(0x2C, 0x50, 0x80, 0xFF));

    if (stage == 1) {
        C2D_DrawText(&s_txt_choose_player_header, C2D_WithColor, 8, 5, 0.0f, 0.6f, 0.6f, white);

        ScrollRow rows[PANEL_ROWS_VISIBLE];
        int visible = 0;
        for (int i = 0; i < PANEL_ROWS_VISIBLE; i++) {
            int idx = scroll + i;
            if (idx >= player_count) break;
            snprintf(rows[visible].text, sizeof(rows[visible].text), "%s (%s)",
                     players[idx].name, diplstate_name(players[idx].diplstate));
            rows[visible].color = white;
            visible++;
        }
        draw_scroll_list(8, rows, visible);

        C2D_DrawRectSolid(0, PANEL_FOOTER_Y, 0.0f, 320, 240 - PANEL_FOOTER_Y, C2D_Color32(0x20, 0x20, 0x20, 0xFF));
        C2D_DrawText(&s_txt_close, C2D_WithColor, 200, PANEL_FOOTER_Y + 6, 0.0f, 0.55f, 0.55f, white);
        return;
    }

    snprintf(line, sizeof(line), "%.24s", counterpart_name);
    draw_text(8, 5, 0.6f, white, line);

    // Fixed add-clause menu (handoff.md §7.4 v1: no sub-picker beyond
    // "which type" - Gold uses a fixed offer amount main.c defines).
    static const char *labels[4] = { "Peace", "Ceasefire", "Embassy", "+50 Gold" };
    for (int i = 0; i < 4; i++) {
        float bx = i * (DIPLO_ADDCLAUSE_BTN_W + DIPLO_ADDCLAUSE_GAP);
        C2D_DrawRectSolid(bx, DIPLO_ADDCLAUSE_Y, 0.0f, DIPLO_ADDCLAUSE_BTN_W, DIPLO_ADDCLAUSE_H,
                          C2D_Color32(0x2C, 0x50, 0x80, 0xFF));
        draw_text(bx + 4, DIPLO_ADDCLAUSE_Y + 5, 0.4f, white, labels[i]);
    }

    snprintf(line, sizeof(line), "You: %s   Them: %s",
             diplo->accept_self ? "Accepted" : "Pending",
             diplo->accept_other ? "Accepted" : "Pending");
    draw_text(8, DIPLO_ADDCLAUSE_Y + DIPLO_ADDCLAUSE_H + 8, 0.5f,
              (diplo->accept_self && diplo->accept_other) ? accent : dim, line);

    // Clause list - not independently scrollable in v1 (only the first
    // PANEL_ROWS_VISIBLE proposed clauses show), same simplification as
    // the city worklist column (handoff.md §6.4).
    ScrollRow rows[PANEL_ROWS_VISIBLE];
    int visible = 0;
    for (int i = 0; i < PANEL_ROWS_VISIBLE && i < diplo->clause_count; i++) {
        snprintf(rows[visible].text, sizeof(rows[visible].text), "x %s", diplo->clauses[i].desc);
        rows[visible].color = white;
        visible++;
    }
    draw_scroll_list(8, rows, visible);
    if (diplo->clause_count == 0) {
        C2D_DrawText(&s_txt_no_clauses_hint, C2D_WithColor, 8, PANEL_LIST_Y + 2, 0.0f, 0.45f, 0.45f, dim);
    }

    C2D_DrawRectSolid(0, PANEL_FOOTER_Y, 0.0f, 320, 240 - PANEL_FOOTER_Y, C2D_Color32(0x20, 0x20, 0x20, 0xFF));
    C2D_DrawText(&s_txt_accept, C2D_WithColor, 8, PANEL_FOOTER_Y + 6, 0.0f, 0.5f, 0.5f, accent);
    C2D_DrawText(&s_txt_cancel, C2D_WithColor, DIPLO_FOOTER_SPLIT1 + 8, PANEL_FOOTER_Y + 6, 0.0f, 0.5f, 0.5f, white);
    C2D_DrawText(&s_txt_close, C2D_WithColor, DIPLO_FOOTER_SPLIT2 + 8, PANEL_FOOTER_Y + 6, 0.0f, 0.5f, 0.5f, white);
}

void render_start_menu(void)
{
    C2D_TextBufClear(g_textBuf);

    u32 white  = C2D_Color32(0xFF, 0xFF, 0xFF, 0xFF);
    u32 dim    = C2D_Color32(0xA0, 0xA8, 0xB4, 0xFF);
    u32 bg     = C2D_Color32(0x2C, 0x50, 0x80, 0xFF);
    u32 border = C2D_Color32(0x6C, 0xA8, 0xE0, 0xFF);

    C2D_DrawRectSolid(0, 0, 0.0f, 320, 240, C2D_Color32(0x10, 0x10, 0x10, 0xFF));
    draw_text(8, 20, 0.8f, white, "Freeciv 3DS");

    C2D_DrawRectSolid(STARTMENU_BTN_X, STARTMENU_NEW_Y, 0.0f, STARTMENU_BTN_W, STARTMENU_BTN_H, bg);
    C2D_DrawRectSolid(STARTMENU_BTN_X, STARTMENU_NEW_Y, 0.0f, STARTMENU_BTN_W, 2, border);
    draw_text(STARTMENU_BTN_X + 16, STARTMENU_NEW_Y + 12, 0.6f, white, "New Game");

    C2D_DrawRectSolid(STARTMENU_BTN_X, STARTMENU_LOAD_Y, 0.0f, STARTMENU_BTN_W, STARTMENU_BTN_H, bg);
    C2D_DrawRectSolid(STARTMENU_BTN_X, STARTMENU_LOAD_Y, 0.0f, STARTMENU_BTN_W, 2, border);
    draw_text(STARTMENU_BTN_X + 16, STARTMENU_LOAD_Y + 12, 0.6f, white, "Load Game");

    draw_text(8, 220, 0.45f, dim, "Tap to choose");
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
            snprintf(rows[visible].text, sizeof(rows[visible].text), "%s", files[idx].filename);
            rows[visible].color = white;
            visible++;
        }
        draw_scroll_list(8, rows, visible);
    }

    C2D_DrawRectSolid(0, PANEL_FOOTER_Y, 0.0f, 320, 240 - PANEL_FOOTER_Y, C2D_Color32(0x20, 0x20, 0x20, 0xFF));
    draw_text(200, PANEL_FOOTER_Y + 6, 0.55f, white, "Back");
}
