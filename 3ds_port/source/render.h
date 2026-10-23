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
// The world map is drawn on the 400x240 top screen at one of several zoom
// levels; the bottom screen holds a touch minimap, the selected tile's
// readout, a button row and the action bar.
//
// Draws the map with world-pixel camera (cam_x, cam_y) at `scale` (1.0 = the
// tileset's native 30px tiles) into a view_w x view_h screen area.
void render_map_view(int cam_x, int cam_y, float scale, int view_w, int view_h,
                     int sel_x, int sel_y, bool sel_armed, int goto_x, int goto_y);

// Top screen: the map plus a status bar (turn/gold/research) and a controls
// line. civ may be NULL.
void render_top_map(const TileInfo *sel, int sel_x, int sel_y, int armed_unit_id,
                    bool goto_armed, int cam_x, int cam_y, float scale,
                    int goto_x, int goto_y, const CivInfo *civ);

// Bottom screen: minimap (tap/drag to look anywhere), tile readout, buttons.
void render_bottom_map(const TileInfo *sel, int sel_x, int sel_y, int armed_unit_id,
                       int cam_x, int cam_y, float scale, int zoom_idx, int zoom_count);

// Maps a bottom-screen touch on the minimap to a tile; false if (px, py) is
// outside the minimap or it isn't ready yet.
bool minimap_hit_test(int px, int py, int *tx, int *ty);

// Bottom-screen layout, shared by drawing and touch hit-testing. The action
// bar (ACTIONBAR_*, below) keeps the right-hand 32px column.
#define MINIMAP_X      2
#define MINIMAP_Y      2
#define MINIMAP_W      284
#define MINIMAP_H      150
#define BOTBTN_Y       206
#define BOTBTN_H       30
#define BOTBTN_END_X   2
#define BOTBTN_END_W   98
#define BOTBTN_ZOUT_X  104
#define BOTBTN_ZIN_X   156
#define BOTBTN_ZOOM_W  48
#define BOTBTN_UNIT_X  208
#define BOTBTN_UNIT_W  78

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
#define PANEL_ROW_H        20
#define PANEL_ROWS_VISIBLE 6
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
#define TECHTREE_TIER_LABEL_W      28
#define TECHTREE_BOX_W             58
#define TECHTREE_BOX_GAP           2
// (320 - 8 left margin - TECHTREE_TIER_LABEL_W) / (BOX_W + GAP), floored.
#define TECHTREE_MAX_BOXES_PER_ROW 4
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

void render_start_menu(void);

// Load-file picker: reuses PANEL_LIST_Y/PANEL_ROW_H/PANEL_ROWS_VISIBLE/
// PANEL_FOOTER_Y above, so main.c's existing hit_test_scroll_list() applies
// unchanged - same "draw and hit-test share one set of constants" rule as
// every other scrollable list panel.
#define MAX_SAVE_FILES 32
void render_load_picker(const SaveFile *files, int n, int scroll);

#endif // RENDER_H
