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

void render_top_screen(const TileInfo *sel, int sel_x, int sel_y, int armed_unit_id,
                       int touch_px, int touch_py, bool goto_armed, int cam_x, int cam_y)
{
    C2D_TextBufClear(g_textBuf);

    u32 white = C2D_Color32(0xFF, 0xFF, 0xFF, 0xFF);
    u32 dim   = C2D_Color32(0xC0, 0xCE, 0xDC, 0xFF);
    u32 label = C2D_Color32(0x8C, 0xA0, 0xB4, 0xFF);

    char line[96];
    int turn = get_game_turn();

    // Scan slice of minimap
    update_minimap_slice(5);

    // --- Title bar ---
    C2D_DrawRectSolid(0, 0, 0.0f, 400, 30, C2D_Color32(0x2C, 0x50, 0x80, 0xFF));
    C2D_DrawRectSolid(0, 30, 0.0f, 400, 2,  C2D_Color32(0x6C, 0xA8, 0xE0, 0xFF));
    C2D_DrawText(&s_txt_title, C2D_WithColor, 8, 5, 0.0f, 0.85f, 0.85f, white);
    if (turn > 0) {
        snprintf(line, sizeof(line), "Turn %d", turn);
        draw_text(310, 8, 0.6f, white, line);
    }

    // --- Turn / status line (in the body, where it is reliably visible) ---
    if (turn > 0) {
        snprintf(line, sizeof(line), "Turn %d", turn);
    } else {
        snprintf(line, sizeof(line), "Generating world...");
    }
    draw_text(8, 40, 0.7f, white, line);

    // --- Selected tile panel ---
    C2D_DrawText(&s_txt_sel_tile_label, C2D_WithColor, 8, 68, 0.0f, 0.6f, 0.6f, label);
    if (sel && sel_x >= 0) {
        snprintf(line, sizeof(line), "(%d, %d)", sel_x, sel_y);
        draw_text(150, 68, 0.6f, dim, line);

        float y = 96;
        if (sel->city_owner >= 0) {
            snprintf(line, sizeof(line), "City: %s  (size %d, P%d)",
                     sel->city_name[0] ? sel->city_name : "?",
                     sel->city_size, sel->city_owner);
            draw_text(8, y, 0.6f, owner_color(sel->city_owner), line);
            y += 26;
        }
        if (sel->unit_owner >= 0) {
            snprintf(line, sizeof(line), "Unit: %s  x%d (P%d)",
                     sel->unit_name[0] ? sel->unit_name : "?",
                     sel->num_units, sel->unit_owner);
            draw_text(8, y, 0.6f, owner_color(sel->unit_owner), line);
            y += 26;

            if (armed_unit_id >= 0) {
                // Keep each line clear of the minimap panel's left edge (x=228):
                // the actions are split across the lines rather than one long row
                // (the old single "B: Fortify/Sentry  X: Action  Y: Disband" ran
                // under the map).
                u32 selected = C2D_Color32(0xFF, 0xE0, 0x60, 0xFF);
                C2D_DrawText(&s_txt_selected_dpad_move, C2D_WithColor, 8, y, 0.0f, 0.55f, 0.55f, selected);
                y += 20;
                C2D_DrawText(&s_txt_selected_actions_hint, C2D_WithColor, 8, y, 0.0f, 0.5f, 0.5f, selected);
                y += 20;
                C2D_DrawText(&s_txt_goto_button_hint, C2D_WithColor, 8, y, 0.0f, 0.5f, 0.5f, selected);
                y += 22;
            }
        }
        if (sel->city_owner < 0 && sel->unit_owner < 0) {
            C2D_DrawText(&s_txt_empty, C2D_WithColor, 8, y, 0.0f, 0.6f, 0.6f, dim);
        }
    } else {
        C2D_DrawText(&s_txt_tap_map_hint, C2D_WithColor, 8, 96, 0.0f, 0.55f, 0.55f, label);
    }

    // --- Controls hint ---
    if (goto_armed) {
        // Overrides the normal hint entirely while armed (handoff.md §2c) -
        // this is the only input that matters until the next map tap lands.
        u32 warn = C2D_Color32(0xFF, 0xD0, 0x40, 0xFF);
        C2D_DrawText(&s_txt_goto_destination_hint, C2D_WithColor, 8, 200, 0.0f, 0.55f, 0.55f, warn);
        C2D_DrawText(&s_txt_goto_cancel_hint, C2D_WithColor, 8, 218, 0.0f, 0.5f, 0.5f, warn);
    } else {
        C2D_DrawText(&s_txt_controls_hint_1, C2D_WithColor, 8, 200, 0.0f, 0.5f, 0.5f, label);
        C2D_DrawText(&s_txt_controls_hint_2, C2D_WithColor, 8, 218, 0.0f, 0.5f, 0.5f, label);
    }

    // --- Minimap drawing ---
    if (s_minimap_initialized) {
        // Minimap panel box. Kept to the right of x=228 so it clears the persistent
        // left-column text ("Selected tile (NN, NN)" value ends ~x220, the "Unit: ..."
        // line ends ~x210) — earlier it started at x=180 and the map grey overlapped
        // that text. Map is scaled uniformly to fit inside PANEL_W x PANEL_H and
        // centered in the box.
        const float PANEL_X = 228.0f, PANEL_Y = 40.0f, PANEL_W = 168.0f, PANEL_H = 180.0f;
        float scale_x = PANEL_W / s_map_w;
        float scale_y = PANEL_H / s_map_h;
        float S = scale_x < scale_y ? scale_x : scale_y;
        if (S < 1.0f) S = 1.0f;

        float dest_x = PANEL_X + (PANEL_W - s_map_w * S) / 2.0f;
        float dest_y = PANEL_Y + (PANEL_H - s_map_h * S) / 2.0f;

        // Draw solid background to clearly show map edges
        C2D_DrawRectSolid(dest_x, dest_y, 0.0f, s_map_w * S, s_map_h * S, C2D_Color32(20, 30, 40, 255));

        // Draw the map as per-tile rects, run-length-encoded per row: merge a run of
        // consecutive same-colour tiles into one wide rect (a mostly-uniform row -
        // ocean, or unexplored - collapses to a handful of rects). Same coordinate
        // system as the viewfinder below, so they align by construction. A tile rect
        // is drawn 1px oversized (ceilf) to avoid seams from fractional S.
        float tile_px = ceilf(S);
        for (int ty = 0; ty < s_map_h; ty++) {
            int rx = 0;
            while (rx < s_map_w) {
                u32 c = s_minimap_colors[ty * s_map_w + rx];
                int run = 1;
                while (rx + run < s_map_w &&
                       s_minimap_colors[ty * s_map_w + rx + run] == c) {
                    run++;
                }
                C2D_DrawRectSolid(dest_x + rx * S, dest_y + ty * S, 0.1f,
                                  run * S + (tile_px - S), tile_px, c);
                rx += run;
            }
        }

        // Draw camera frustum
        float viewport_w_tiles = 320.0f / 30.0f;
        float viewport_h_tiles = 240.0f / 30.0f;

        float tile_cam_x = (float)cam_x / 30.0f;
        float tile_cam_y = (float)cam_y / 30.0f;

        if (map_wraps_x()) {
            tile_cam_x = fmodf(tile_cam_x, (float)s_map_w);
            if (tile_cam_x < 0.0f) tile_cam_x += s_map_w;
        } else {
            if (tile_cam_x < 0.0f) tile_cam_x = 0.0f;
            if (tile_cam_x + viewport_w_tiles > s_map_w) tile_cam_x = s_map_w - viewport_w_tiles;
        }

        if (map_wraps_y()) {
            tile_cam_y = fmodf(tile_cam_y, (float)s_map_h);
            if (tile_cam_y < 0.0f) tile_cam_y += s_map_h;
        } else {
            if (tile_cam_y < 0.0f) tile_cam_y = 0.0f;
            if (tile_cam_y + viewport_h_tiles > s_map_h) tile_cam_y = s_map_h - viewport_h_tiles;
        }

        u32 frustum_color = C2D_Color32(0xFF, 0xFF, 0xFF, 0xFF);

        if (map_wraps_x() && (tile_cam_x + viewport_w_tiles > s_map_w)) {
            float first_w_tiles = (float)s_map_w - tile_cam_x;
            float second_w_tiles = viewport_w_tiles - first_w_tiles;

            draw_hollow_rect(dest_x + tile_cam_x * S, dest_y + tile_cam_y * S,
                             first_w_tiles * S, viewport_h_tiles * S, 1.5f, frustum_color);
            draw_hollow_rect(dest_x, dest_y + tile_cam_y * S,
                             second_w_tiles * S, viewport_h_tiles * S, 1.5f, frustum_color);
        } else {
            draw_hollow_rect(dest_x + tile_cam_x * S, dest_y + tile_cam_y * S,
                             viewport_w_tiles * S, viewport_h_tiles * S, 1.5f, frustum_color);
        }
    }

    (void)touch_px; (void)touch_py;
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

static void draw_ocean_cell(int draw_x, int draw_y, int dx, int dy, int b0, int b1, int b2, const int* sprites)
{
    int idx = (b2 << 2) | (b1 << 1) | b0;
    C2D_Image img = C2D_SpriteSheetGetImage(g_spriteSheet, sprites[idx]);
    C2D_DrawImageAt(img, draw_x + dx, draw_y + dy, 0.49f, NULL, 1.0f, 1.0f);
}

static void draw_tile_entities(const TileInfo *info, int draw_x, int draw_y)
{
    // City takes visual priority over a stacked unit.
    if (info->city_owner >= 0) {
        C2D_DrawImageAt(C2D_SpriteSheetGetImage(g_spriteSheet, sprites_city_idx),
                        draw_x, draw_y, 0.6f, NULL, 1.0f, 1.0f);
        C2D_DrawRectSolid(draw_x + 11, draw_y - 3, 0.65f, 8, 5,
                          owner_color(info->city_owner));
    } else if (info->unit_owner >= 0) {
        int sprite_idx = get_unit_sprite(info->unit_name);
        C2D_DrawImageAt(C2D_SpriteSheetGetImage(g_spriteSheet, sprite_idx),
                        draw_x, draw_y, 0.6f, NULL, 1.0f, 1.0f);
        C2D_DrawRectSolid(draw_x + 11, draw_y - 3, 0.65f, 8, 5,
                          owner_color(info->unit_owner));
    }
}

static void draw_selection(int draw_x, int draw_y)
{
    u32 hi = C2D_Color32(0xFF, 0xF0, 0x30, 0xFF); // yellow border
    float z = 0.7f;
    C2D_DrawRectSolid(draw_x,              draw_y,              z, TILE_PX, 2, hi); // top
    C2D_DrawRectSolid(draw_x,              draw_y + TILE_PX-2,  z, TILE_PX, 2, hi); // bottom
    C2D_DrawRectSolid(draw_x,              draw_y,              z, 2, TILE_PX, hi); // left
    C2D_DrawRectSolid(draw_x + TILE_PX-2,  draw_y,              z, 2, TILE_PX, hi); // right
}

// Pending-goto target marker (handoff.md §2c/2b) - a small diamond centered
// on the destination tile. z=0.75 is above draw_selection()/unit sprites
// (both 0.6-0.7f, the highest this per-tile loop otherwise reaches) so it's
// never hidden by them when the goto target happens to also be the
// selected tile or share it with a unit - this loop's z values are already
// proven on hardware to layer correctly in ascending order (see the
// terrain/overlay stack above), unlike the toast overlay's separate
// draw-after-the-fact case (STATUS.md Resolved) which needed 0.9 to beat
// this whole loop from outside it.
static void draw_goto_marker(int draw_x, int draw_y)
{
    u32 c = C2D_Color32(0xFF, 0xA0, 0x20, 0xFF); // orange
    float z = 0.75f;
    int cx = draw_x + TILE_PX / 2, cy = draw_y + TILE_PX / 2;
    C2D_DrawRectSolid(cx - 5, cy - 2, z, 10, 4, c);
    C2D_DrawRectSolid(cx - 2, cy - 5, z, 4, 10, c);
}

void render_bottom_screen(int cam_x, int cam_y, int sel_x, int sel_y,
                          int touch_px, int touch_py, int goto_x, int goto_y)
{
    if (!g_spriteSheet) return;

    // While the server is still generating the map, show a status message
    // instead of the empty mock grid (which looked like a broken all-ocean map).
    if (get_game_turn() <= 0) {
        draw_text(70, 105, 0.7f, C2D_Color32(0xFF, 0xFF, 0xFF, 0xFF),
                  "Generating world...");
        return;
    }

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
    const bool wrapx = map_wraps_x();
    const int world_w = map_w * TILE_PX;
    for (int y = 0; y < map_h; y++) {
        for (int x = 0; x < map_w; x++) {
            // Overhead square grid: tile (x,y) occupies [x*30, y*30]..[+30,+30].
            int draw_x = x * TILE_PX - cam_x;
            int draw_y = y * TILE_PX - cam_y;

            if (wrapx) {
                draw_x = ((draw_x % world_w) + world_w) % world_w; // [0, world_w)
                if (draw_x >= 320) draw_x -= world_w;              // visible on the left instead
            }

            if (draw_x <= -TILE_PX || draw_x >= 320 || draw_y <= -TILE_PX || draw_y >= 240) {
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
                                draw_x, draw_y, 0.493f, NULL, 1.0f, 1.0f);
            } else if (info.terrain == TERRAIN_GRASSLAND) {
                C2D_DrawImageAt(C2D_SpriteSheetGetImage(g_spriteSheet, sprites_grassland_idx), draw_x, draw_y, 0.5f, NULL, 1.0f, 1.0f);
            } else {
                int N  = is_same_terrain(x, y - 1, map_w, map_h, info.terrain);
                int S  = is_same_terrain(x, y + 1, map_w, map_h, info.terrain);
                int E  = is_same_terrain(x + 1, y, map_w, map_h, info.terrain);
                int W  = is_same_terrain(x - 1, y, map_w, map_h, info.terrain);
                int match_idx = N | (E << 1) | (S << 2) | (W << 3);
                int sprite_idx = terrain_match_sprites[info.terrain][match_idx];
                C2D_DrawImageAt(C2D_SpriteSheetGetImage(g_spriteSheet, sprite_idx), draw_x, draw_y, 0.5f, NULL, 1.0f, 1.0f);
            }

            // Draw overlays on top of the terrain (except on raw ocean tiles)
            if (info.terrain != TERRAIN_OCEAN) {
                // 1. Irrigation / Farmland
                if (info.has_farmland) {
                    C2D_DrawImageAt(C2D_SpriteSheetGetImage(g_spriteSheet, sprites_tx_farmland_idx), draw_x, draw_y, 0.51f, NULL, 1.0f, 1.0f);
                } else if (info.has_irrigation) {
                    C2D_DrawImageAt(C2D_SpriteSheetGetImage(g_spriteSheet, sprites_tx_irrigation_idx), draw_x, draw_y, 0.51f, NULL, 1.0f, 1.0f);
                }

                // 2. River
                if (info.has_river) {
                    int rN = has_river_at(x, y - 1);
                    int rS = has_river_at(x, y + 1);
                    int rE = has_river_at(x + 1, y);
                    int rW = has_river_at(x - 1, y);
                    int r_idx = rN | (rE << 1) | (rS << 2) | (rW << 3);
                    C2D_DrawImageAt(C2D_SpriteSheetGetImage(g_spriteSheet, river_sprites[r_idx]), draw_x, draw_y, 0.52f, NULL, 1.0f, 1.0f);
                }

                // 3. Roads / Railroads / Maglevs
                if (info.has_maglev) {
                    int mN = has_maglev_at(x, y - 1);
                    int mS = has_maglev_at(x, y + 1);
                    int mE = has_maglev_at(x + 1, y);
                    int mW = has_maglev_at(x - 1, y);
                    int m_idx = mN | (mE << 1) | (mS << 2) | (mW << 3);
                    C2D_DrawImageAt(C2D_SpriteSheetGetImage(g_spriteSheet, maglev_sprites[m_idx]), draw_x, draw_y, 0.53f, NULL, 1.0f, 1.0f);
                } else if (info.has_rail) {
                    int rN = has_rail_at(x, y - 1);
                    int rS = has_rail_at(x, y + 1);
                    int rE = has_rail_at(x + 1, y);
                    int rW = has_rail_at(x - 1, y);
                    int r_idx = rN | (rE << 1) | (rS << 2) | (rW << 3);
                    C2D_DrawImageAt(C2D_SpriteSheetGetImage(g_spriteSheet, rail_sprites[r_idx]), draw_x, draw_y, 0.53f, NULL, 1.0f, 1.0f);
                } else if (info.has_road) {
                    int rN = has_road_at(x, y - 1);
                    int rS = has_road_at(x, y + 1);
                    int rE = has_road_at(x + 1, y);
                    int rW = has_road_at(x - 1, y);
                    int r_idx = rN | (rE << 1) | (rS << 2) | (rW << 3);
                    C2D_DrawImageAt(C2D_SpriteSheetGetImage(g_spriteSheet, road_sprites[r_idx]), draw_x, draw_y, 0.53f, NULL, 1.0f, 1.0f);
                }

                // 4. Mine
                if (info.has_mine) {
                    C2D_DrawImageAt(C2D_SpriteSheetGetImage(g_spriteSheet, sprites_tx_mine_idx), draw_x, draw_y, 0.54f, NULL, 1.0f, 1.0f);
                }

                // 5. Resource / Special
                if (info.resource_tag[0]) {
                    int res_sprite = get_resource_sprite(info.resource_tag);
                    if (res_sprite >= 0) {
                        C2D_DrawImageAt(C2D_SpriteSheetGetImage(g_spriteSheet, res_sprite), draw_x, draw_y, 0.55f, NULL, 1.0f, 1.0f);
                    }
                }

                // 6. Goody Hut / Village
                if (info.has_village) {
                    C2D_DrawImageAt(C2D_SpriteSheetGetImage(g_spriteSheet, sprites_tx_village_idx), draw_x, draw_y, 0.56f, NULL, 1.0f, 1.0f);
                }

                // 7. Pollution / Fallout
                if (info.has_pollution) {
                    C2D_DrawImageAt(C2D_SpriteSheetGetImage(g_spriteSheet, sprites_tx_pollution_idx), draw_x, draw_y, 0.57f, NULL, 1.0f, 1.0f);
                } else if (info.has_fallout) {
                    C2D_DrawImageAt(C2D_SpriteSheetGetImage(g_spriteSheet, sprites_tx_fallout_idx), draw_x, draw_y, 0.57f, NULL, 1.0f, 1.0f);
                }
            }

            // Draw Fog of War overlay (tx.fog) if known unseen (fogged)
            if (info.visibility == 1) {
                C2D_DrawImageAt(C2D_SpriteSheetGetImage(g_spriteSheet, sprites_tx_fog_idx), draw_x, draw_y, 0.58f, NULL, 1.0f, 1.0f);
            }

            // Draw Darkness overlay if any neighbor is unknown
            if (info.darkness_idx > 0) {
                int darkness_sprite = darkness_sprites[info.darkness_idx];
                C2D_DrawImageAt(C2D_SpriteSheetGetImage(g_spriteSheet, darkness_sprite), draw_x, draw_y, 0.59f, NULL, 1.0f, 1.0f);
            }

            // Draw borders
            if (info.tile_owner != -1) {
                u32 bcolor = owner_color(info.tile_owner);
                C2D_ImageTint tint;
                C2D_PlainImageTint(&tint, bcolor, 1.0f);
                
                if (info.border_n) {
                    C2D_DrawImageAt(C2D_SpriteSheetGetImage(g_spriteSheet, sprites_grid_borders_n_idx), draw_x, draw_y, 0.6f, &tint, 1.0f, 1.0f);
                }
                if (info.border_s) {
                    C2D_DrawImageAt(C2D_SpriteSheetGetImage(g_spriteSheet, sprites_grid_borders_s_idx), draw_x, draw_y, 0.6f, &tint, 1.0f, 1.0f);
                }
                if (info.border_e) {
                    C2D_DrawImageAt(C2D_SpriteSheetGetImage(g_spriteSheet, sprites_grid_borders_e_idx), draw_x, draw_y, 0.6f, &tint, 1.0f, 1.0f);
                }
                if (info.border_w) {
                    C2D_DrawImageAt(C2D_SpriteSheetGetImage(g_spriteSheet, sprites_grid_borders_w_idx), draw_x, draw_y, 0.6f, &tint, 1.0f, 1.0f);
                }
            }

            // Draw entities based on visibility
            if (info.visibility == 2) {
                draw_tile_entities(&info, draw_x, draw_y);
            } else {
                // Under Fog of War (visibility == 1), we still draw city centers but no units
                if (info.city_owner != -1) {
                    C2D_DrawImageAt(C2D_SpriteSheetGetImage(g_spriteSheet, sprites_city_idx), draw_x, draw_y, 0.7f, NULL, 1.0f, 1.0f);
                }
            }

            if (x == sel_x && y == sel_y) {
                draw_selection(draw_x, draw_y);
            }
            if (x == goto_x && y == goto_y) {
                draw_goto_marker(draw_x, draw_y);
            }
        }
    }

    (void)touch_px; (void)touch_py;
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
