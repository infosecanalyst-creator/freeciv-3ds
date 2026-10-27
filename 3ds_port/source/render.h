#ifndef RENDER_H
#define RENDER_H

#include <citro2d.h>
#include "game_loop.h"

extern C2D_SpriteSheet g_spriteSheet;

// Must be called once after C2D_Prepare().
void render_init(void);
void render_deinit(void);

// ---- Map screens (UI_MAP) ----
//
// DS-style layout: the bottom screen is the play surface (stylus map, unit
// popup menus, HUD buttons); the top screen only displays information -
// status, minimap, research and whatever the stylus last selected.
//
// Draws the map with world-pixel camera (cam_x, cam_y) at `scale` (1.0 = the
// tileset's native 30px tiles) into a view_w x view_h screen area.
void render_map_view(int cam_x, int cam_y, float scale, int view_w, int view_h,
                     int sel_x, int sel_y, bool sel_armed, int goto_x, int goto_y);

// A popup menu on the bottom screen (unit orders, stack picker, game menu).
#define POPUP_MAX     16
#define POPUP_ITEM_W  100
#define POPUP_ITEM_H  24
#define POPUP_TITLE_H 18
typedef struct {
    int  n;
    int  id[POPUP_MAX];           // caller's meaning (action kind, unit id, ...)
    char label[POPUP_MAX][24];
    bool enabled[POPUP_MAX];
    char title[40];
    int  x, y, w, h, cols;        // set by popup_layout()
} PopupMenu;
// Places the popup beside the anchor point, kept on screen above the HUD.
void popup_layout(PopupMenu *m, int anchor_x, int anchor_y);
// Index of the enabled item under (px, py); -2 inside the popup but not on an
// item; -1 outside.
int popup_hit_test(const PopupMenu *m, int px, int py);

// Bottom-screen HUD button row.
#define HUD_Y       212
#define HUD_MENU_X  2
#define HUD_MENU_W  50
#define HUD_NEXT_X  55
#define HUD_NEXT_W  50
#define HUD_MAP_X   108
#define HUD_MAP_W   44
#define HUD_ZOUT_X  155
#define HUD_ZIN_X   192
#define HUD_ZOOM_W  34
#define HUD_END_X   229
#define HUD_END_W   89
enum { HUD_NONE, HUD_BAR, HUD_MENU, HUD_NEXT, HUD_ZOUT, HUD_ZIN, HUD_END, HUD_MAP };
// World map in place of the bottom map (HUD Map button); tap maps to a tile.
void render_set_world_view(bool on);
bool world_map_hit(int px, int py, int *tx, int *ty);
int hud_hit_test(int px, int py);

// Bottom screen. unit_x/unit_y is the commanded unit's tile (-1 if none);
// popup may be NULL. all_moved highlights End Turn.
// Quick-order bar for the selected unit, just above the HUD row: up to 4
// buttons (the last is usually "More..."). n = 0 hides it.
#define UNITBAR_H 24
#define UNITBAR_Y (HUD_Y - UNITBAR_H - 2)
#define UNITBAR_MAX 4
void render_set_unit_bar(const char labels[][20], int n);
// Squares the selected unit can reach this turn (tinted on the map).
void render_set_move_range(const short *xs, const short *ys, int n);
void render_bottom_map(int cam_x, int cam_y, float scale, int sel_x, int sel_y,
                       int unit_x, int unit_y, bool unit_can_move, int goto_x, int goto_y,
                       const PopupMenu *popup, bool can_zoom_out, bool can_zoom_in, bool all_moved,
                       const RoutePreview *route, const char *end_label);

// Top screen contents.
typedef struct {
    const CivInfo   *civ;         // may be NULL
    const TileInfo  *tile;        // selected tile, may be NULL
    int              tile_x, tile_y;
    const UnitInfo  *unit;        // commanded/inspected unit, may be NULL
    const UnitBrief *stack;       // units on the selected tile
    int              stack_n;
    const char      *hint;        // controls line at the bottom
    u32              hint_color;  // 0 = default
    int              cam_x, cam_y;
    float            zoom;
    // Turn report (shown instead of the minimap/card while report_n > 0).
    const GameMessage *report;
    int              report_n, report_more, report_turn;
} TopInfo;
void render_top_info(const TopInfo *ti);

// Stereo 3D for the top screen: call before drawing each eye (eye -1 left,
// +1 right, 0 mono; amount = pixels of separation per unit of depth).
void render_set_stereo(int eye, float amount);

// Top-screen minimap rect.
#define MINIMAP_X      6
#define MINIMAP_Y      26
#define MINIMAP_W      190
#define MINIMAP_H      126

// Which headline render_processing_overlay() shows - the server-thread step
// that's making the app "busy" this frame. PROC_SAVING exists because an
// on-demand save (§A.2) is a normal busy step like any other and gets the
// same full overlay while it writes to the SD card - without this it would
// silently reuse PROC_TURN's "Processing turn..." wording, which is
// misleading for what's actually a save.
typedef enum { PROC_LOADING, PROC_SAVING, PROC_TURN } ProcessingReason;

// Drawn on both screens instead of the normal map/HUD while the server
// coroutine is busy (fc3ds_server_is_busy()) - touches no Freeciv game
// state, since that's unsafe to read mid-step.
void render_processing_overlay(int screen_w, int screen_h, ProcessingReason reason);

// Small corner "..." badge drawn over an already-rendered modal panel while
// the server is busy (handoff.md §4) - unlike render_processing_overlay(),
// this does not cover the panel, since the panel itself is only ever drawn
// from the last cached !busy snapshot and needs no fresh reads.
void render_busy_indicator(int screen_w);
// Bottom-strip "Other civilizations are moving..." banner shown over the
// last captured map frame while a long step runs.
void render_turn_banner(ProcessingReason reason, int frame);

// On-screen action bar (bottom-screen right edge, UI_MAP only) - reaches
// panels that have no dedicated shoulder-button shortcut left (handoff.md
// §2). L/R keep opening Research/Rates directly; this is the discoverable,
// touch-only path to the same panels plus future ones.
// GAP shrunk from the original 8px (RES..SAVE) to fit a 6th button (GOTO)
// in the same 320x240 bottom screen - 6 * (32 + 8) would overflow the
// screen height starting from Y=40. 12px start + 5px gaps keeps the last
// button's bottom edge (229) comfortably inside 240.
#define ACTIONBAR_BTN_W    32
#define ACTIONBAR_BTN_H    32
#define ACTIONBAR_X        (320 - ACTIONBAR_BTN_W)
#define ACTIONBAR_GAP      5
#define ACTIONBAR_RES_Y    12
#define ACTIONBAR_RATES_Y  (ACTIONBAR_RES_Y + ACTIONBAR_BTN_H + ACTIONBAR_GAP)
#define ACTIONBAR_GOV_Y    (ACTIONBAR_RATES_Y + ACTIONBAR_BTN_H + ACTIONBAR_GAP)
#define ACTIONBAR_DIPL_Y   (ACTIONBAR_GOV_Y + ACTIONBAR_BTN_H + ACTIONBAR_GAP)
#define ACTIONBAR_SAVE_Y   (ACTIONBAR_DIPL_Y + ACTIONBAR_BTN_H + ACTIONBAR_GAP)
// Only tappable while a unit is armed (main.c checks selected_unit_id >= 0
// before entering goto-target sub-mode) - handoff.md §2c.
#define ACTIONBAR_GOTO_Y   (ACTIONBAR_SAVE_Y + ACTIONBAR_BTN_H + ACTIONBAR_GAP)

// Draws the action-bar buttons described above. Bottom screen, UI_MAP only.
void render_action_bar(void);

// Transient status banner (e.g. "Saved") drawn over the bottom-screen
// map/action-bar (handoff-nohw.md §A.2). Purely cosmetic - main.c owns the
// on/off countdown and only calls this while it wants the banner visible.
void render_toast(const char *text);

// ---- City / research / rates modal panels (bottom-screen 320x240 logical
// space; touch space == render space, see main.c's UiMode routing) ----
//
// Shared bottom-panel layout constants - main.c hit-tests touches against
// these same rects, so draw and hit-test never drift apart.
#define PANEL_HEADER_H     30
#define PANEL_LIST_Y       90
#define PANEL_ROW_H        24   // a comfortable stylus target
#define PANEL_ROWS_VISIBLE 5
#define PANEL_FOOTER_Y     (PANEL_LIST_Y + PANEL_ROW_H * PANEL_ROWS_VISIBLE) // 210

// One already-formatted, already-scrolled-into-view row for draw_scroll_list()
// below. Callers slice their own [scroll, scroll+PANEL_ROWS_VISIBLE) window and
// pick per-row color (current/available/disabled) themselves - this only
// owns the layout math every bottom-panel list (build, research, government)
// was repeating identically (handoff.md §1.3/§9: factor on the third caller).
typedef struct {
    char text[64];
    u32  color;
} ScrollRow;

// Draws rows[0..count) at (x, PANEL_LIST_Y), one per PANEL_ROW_H, top to
// bottom. count must already be <= PANEL_ROWS_VISIBLE (the caller's slice).
// x lets a panel place more than one column of rows side by side (see the
// city panel's worklist column, handoff.md §6.4).
void draw_scroll_list(int x, const ScrollRow *rows, int count);

// Rates panel row/button rects (bottom screen).
#define RATES_TAX_Y        84
#define RATES_LUX_Y        124
#define RATES_SCI_Y         164
#define RATES_ROW_TOUCH_H  24
#define RATES_BTN_MINUS_X  200
#define RATES_BTN_PLUS_X   260
#define RATES_BTN_W        40

// City panel worklist column (handoff.md §6.4) - "beside" the buildables
// list rather than shrinking it, so the shared PANEL_ROWS_VISIBLE/
// PANEL_LIST_Y grid stays untouched for every other panel. Buildable row
// text is truncated to fit left of CITY_WORKLIST_COL_X (see
// render_city_panel()). The queue-mode toggle sits in the header band
// (y 0..PANEL_HEADER_H), well above the list/footer rows so its hit-test
// can never collide with them.
#define CITY_WORKLIST_COL_X   200
#define CITY_TOGGLE_X         228
#define CITY_TOGGLE_Y         4
#define CITY_TOGGLE_W         84
#define CITY_TOGGLE_H         22

// Top-screen readout for the open city (mirrors render_top_screen's role).
// garrison_sel is the L/R-cycled highlighted index into
// info->garrison_ids/garrison_names (handoff.md §3); ignored if
// garrison_count == 0.
void render_city_top(const CityInfo *info, int garrison_sel);

// Bottom-screen city panel: stats header + NOW/QUEUE toggle + scrollable
// build list (rows [scroll, scroll+PANEL_ROWS_VISIBLE) of opts[0..n)) beside
// a worklist column (worklist[0..worklist_n), unscrolled, capped to
// PANEL_ROWS_VISIBLE - handoff.md §6.4) + Buy/Close footer. queue_mode
// mirrors main.c's tap semantics (build now vs append to worklist) so the
// hint text stays in sync with what tapping a build row actually does.
void render_city_panel(const CityInfo *info, const BuildOption *opts, int n, int scroll,
                       const WorklistEntry *worklist, int worklist_n, bool queue_mode);

// City screen tabs (Build / Tiles / Buildings), under the header.
#define CITY_TAB_Y      56
#define CITY_TAB_H      26
#define CITY_TAB_W      76
#define CITY_TAB_X(i)   (4 + (i) * 78)
enum { CITY_TAB_BUILD, CITY_TAB_TILES, CITY_TAB_BUILDINGS, CITY_TAB_UNITS, CITY_TAB_COUNT };
void render_city_tabs(int tab);
// Units tab: one row per unit in the city (sprite + name).
#define CITY_UNIT_ROW_H 24
void render_city_units_panel(const CityInfo *info, int scroll);
int city_units_hit(int py, int scroll, int n);
// Two-tap confirmations drawn by the city Build tab / Government panel.
void render_set_buy_armed(bool armed);
void render_set_gov_armed(int idx);
// Tiles tab: the city's land as a grid (left) and its specialists (right).
#define CITY_GRID_X     6
#define CITY_GRID_Y     88
#define CITY_GRID_SIZE  120
#define CITY_SPEC_X     134
#define CITY_SPEC_Y     110
#define CITY_SPEC_W     182
#define CITY_SPEC_H     26
void render_city_tiles_panel(const CityInfo *info, const CityTileInfo *tiles, int n, int radius,
                             const SpecialistInfo *spec, int nspec);
// Maps a tap to a grid offset; false if outside the grid.
bool city_grid_hit(int radius, int px, int py, int *rx, int *ry);
// Buildings tab: rows on the shared PANEL_* list layout; armed = row waiting
// for its confirming second tap (-1 none).
void render_city_buildings_panel(const CityInfo *info, const BuildingInfo *b, int n, int scroll, int armed);

// Top-screen readout for the civ-level (research/rates) panels. title is
// the panel name shown in the header bar ("Research" / "Tax & Science").
void render_civ_top(const CivInfo *civ, const char *title);

// Bottom-screen research panel: scrollable researchable-tech list + Close.
// tree_view_available is shown as a footer hint ("Y: Tree View") - the flat
// list itself is unchanged from before §8 (handoff.md §8.5 regression rule).
void render_research_panel(const TechOption *opts, int n, int scroll);

// Tiered tech-tree view (handoff.md §8.4) - one row per tier (a consecutive
// run in nodes[], see TechNode's ordering contract), each tech in that row a
// small tappable box: bright = PREREQS_KNOWN (tappable), dim = KNOWN, grey =
// UNKNOWN (both non-tappable). No connecting lines in v1. scroll counts
// tier-rows, not techs - a different unit from the flat list's scroll, so
// main.c resets it to 0 when toggling views.
#define TECHTREE_TIER_LABEL_W      16
#define TECHTREE_BOX_W             97
#define TECHTREE_BOX_GAP           3
#define TECHTREE_PER_ROW           3   // a tier with more techs wraps onto more rows
// Display rows the tree needs (the scroll unit), and the goal box to ring.
int techtree_display_rows(const TechNode *nodes, int n);
void render_set_tech_goal(int tech);
void render_tech_tree_panel(const TechNode *nodes, int n, int scroll);

// Hit-tests a bottom-screen tap against the tiered tech-tree view above.
// Returns the index into nodes[0..n) under (px, py), or -1 if the tap isn't
// over a box. Shares render_tech_tree_panel()'s exact row/box-layout walk
// (same source file, same constants) so draw and hit-test can't drift apart
// - the caller still must check nodes[idx].state == TECH_PREREQS_KNOWN
// before acting, same as the grey/non-tappable boxes being non-tappable.
int techtree_hit_test(const TechNode *nodes, int n, int scroll, int px, int py);

// Bottom-screen tax/luxury/science rates panel: +/- on tax and luxury,
// science shown as the auto-balanced remainder, + Close.
void render_rates_panel(const CivInfo *civ);

// Bottom-screen government panel: scrollable list of every government in
// the ruleset (handoff.md §5), current one marked, unavailable ones
// greyed/non-tappable, + Close.
void render_government_panel(const GovOption *opts, int n, int scroll, int current_gov_id);

// Diplomacy (handoff.md §7.4) - one UI_DIPLOMACY mode, two internal stages
// main.c tracks as a plain int (not a new UiMode): 1 = player picker
// (scrollable PlayerOption list), 2 = clause editor (fixed add-clause
// button row + clause list + Accept/Cancel/Close footer). Both stages
// render from one function since they share a mode; only stage 2 uses
// counterpart_name/diplo.
#define DIPLO_ADDCLAUSE_Y     34
#define DIPLO_ADDCLAUSE_H     24
#define DIPLO_ADDCLAUSE_BTN_W 78
#define DIPLO_ADDCLAUSE_GAP   4
// Footer is a 3-way Accept/Cancel/Close split instead of the usual 1-2
// buttons - x < DIPLO_FOOTER_SPLIT1 is Accept, between the splits is
// Cancel, >= DIPLO_FOOTER_SPLIT2 is Close.
#define DIPLO_FOOTER_SPLIT1   106
#define DIPLO_FOOTER_SPLIT2   213

// Top-screen readout: stage 1 shows a picker prompt, stage 2 shows
// counterpart_name plus DiploInfo's accept_self/accept_other status.
void render_diplomacy_top(int stage, const char *counterpart_name, const DiploInfo *diplo);

// Bottom-screen panel for both diplomacy stages - see the block comment
// above. players/player_count/scroll are only used in stage 1;
// counterpart_name/diplo only in stage 2.
void render_diplomacy_panel(int stage, const char *counterpart_name,
                            const PlayerOption *players, int player_count, int scroll,
                            const DiploInfo *diplo);

// ---- Start menu / load picker (handoff-nohw.md §A.3) ----
//
// Shown before the server coroutine is released to run setup (see
// fc3ds_server_begin_new_game()/_begin_load_game(), fc3ds_server.h) - drawn
// on the bottom screen only, same as every other touch-driven panel here.
#define STARTMENU_BTN_W   200
#define STARTMENU_BTN_H   40
#define STARTMENU_BTN_X   ((320 - STARTMENU_BTN_W) / 2)
#define STARTMENU_NEW_Y   80
#define STARTMENU_LOAD_Y  (STARTMENU_NEW_Y + STARTMENU_BTN_H + 20)

// Title-screen buttons, top to bottom: [Continue,] New Game, Load Game.
// slot 0 = Continue (only when a save exists), 1 = New Game, 2 = Load Game.
static inline int startmenu_slot_y(int slot, bool has_continue)
{
    return has_continue ? 52 + slot * 52 : STARTMENU_NEW_Y + (slot - 1) * (STARTMENU_BTN_H + 20);
}
// continue_label: e.g. "Turn 12, 3450 BC" for the newest save, or NULL.
void render_start_menu(const char *continue_label);

// ---- Title screen (handheld style, like the SimGolf port) ----
// Top: the logo over a slow flight across a map, and a line about the chosen
// row. Bottom: big rows for the stylus, D-pad + A as well.
enum { TI_CONTINUE, TI_NEW, TI_LOAD, TI_SETTINGS, TI_CONTROLS, TI_QUIT, TI_COUNT };
#define TITLE_ROW_X  28
#define TITLE_ROW_W  264
#define TITLE_ROW_Y0 10
#define TITLE_ROW_H  37   // 32 px row + 5 px gap
// Rows shown (Continue only with a save); returns how many, items in order.
int  title_rows(bool has_continue, int *items);
int  title_hit(int px, int py, bool has_continue);   // TI_* or -1
// line: the description strip text; headline: replaces the tagline (loading).
void render_title_top(int frame, const char *line, const char *headline);
void render_title_bottom(int sel, bool has_continue, const char *continue_label, int frame);
// Settings page (rows like New Game: label, -, value, +) and a Back button.
void render_title_settings(int music, int sfx, int text_large);
// Controls page on the top screen.
void render_title_controls_top(int frame);
// Loading splash on the bottom while a game starts.
void render_title_loading_bottom(const char *msg, int frame);

// New Game setup (bottom: rows of  label  [-] value [+], Start/Back;
// top: summary). Values arrive pre-formatted.
#define SETUP_ROWS      6
#define SETUP_ROW_Y0    32
#define SETUP_ROW_H     26
#define SETUP_MINUS_X   150
#define SETUP_PLUS_X    274
#define SETUP_BTN_W     42
#define SETUP_BTN_H     23
#define SETUP_START_X   170
#define SETUP_BACK_X    10
#define SETUP_FOOT_Y    196
#define SETUP_FOOT_W    140
#define SETUP_FOOT_H    38
void render_new_game_setup(const char *labels[SETUP_ROWS], const char *values[SETUP_ROWS],
                           const bool can_dec[SETUP_ROWS], const bool can_inc[SETUP_ROWS]);
void render_new_game_top(bool is_new_3ds, const char *summary);

// Load-file picker: reuses PANEL_LIST_Y/PANEL_ROW_H/PANEL_ROWS_VISIBLE/
// PANEL_FOOTER_Y above, so main.c's existing hit_test_scroll_list() applies
// unchanged - same "draw and hit-test share one set of constants" rule as
// every other scrollable list panel.
#define MAX_SAVE_FILES 32
// Friendly name for a save file ("Turn 12, 3450 BC  (autosave)").
void save_file_label(const char *fn, char *out, int n);
void render_load_picker(const SaveFile *files, int n, int scroll);

// Message log (Menu > Messages): bottom list newest first, top shows the
// highlighted message in full. Rows use the shared PANEL_* list layout.
void render_messages_panel(const GameMessage *msgs, int n, int scroll, int highlight);
void render_messages_top(const GameMessage *m);

// Civilopedia: category tabs + entry list (bottom), entry text (top).
#define PEDIA_TAB_Y   34
#define PEDIA_TAB_H   24
#define PEDIA_TAB_W   61
#define PEDIA_TAB_X(i) (3 + (i) * 63)
void render_pedia_panel(int kind, const PediaEntry *list, int n, int scroll, int selected);
void render_pedia_top(const char *title, const char *text, int scroll_lines,
                      const char *icon_tag, const char *icon_alt, bool icon_is_unit);

// Spaceship (Menu > Spaceship).
void render_spaceship_top(const SpaceshipInfo *s);
void render_spaceship_panel(const SpaceshipInfo *s);   // [Launch] left half, [Close] right half of the footer

// End of the game.
#define GAMEOVER_BTN_Y 170
void render_game_over_top(bool won, int score, int turn);
void render_game_over_bottom(void);   // [Look at the map] [Quit]

#endif // RENDER_H
