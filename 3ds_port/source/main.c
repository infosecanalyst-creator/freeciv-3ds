#include <3ds.h>
#include <citro2d.h>

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include "game_loop.h"
#include "fc3ds_server.h"
#include "render.h"

// Screen dimensions
#define SCREEN_WIDTH_TOP  400
#define SCREEN_HEIGHT_TOP 240
#define SCREEN_WIDTH_BOT  320
#define SCREEN_HEIGHT_BOT 240

void __real_exit(int status);

// Override exit to prevent libctru exit handlers from crashing the main thread
// and to ensure logs are fully flushed to the SD card.
void __wrap_exit(int status) {
    // During app shutdown, the Freeciv server thread runs server_quit() which
    // ends in exit(). Turn that into a clean end of *just that thread* so the
    // main thread can then tear libctru down without a racing thread.
    if (fc3ds_server_is_quitting()) {
        threadExit(0); // never returns
    }

    if (status == 0) {
        __real_exit(status);
    }

    void *caller = __builtin_return_address(0);
    printf("FATAL EXIT: %d from %p\n", status, caller);
    fflush(stdout);
    fflush(stderr);
    
    // Close files to ensure FAT tables are updated
    fclose(stdout);
    fclose(stderr);
    
    // Halt the thread infinitely so the main thread keeps rendering
    // and libctru doesn't unmap shared memory
    while (1) {
        svcSleepThread(1000000000);
    }
}

void __wrap_abort(void) {
    void *caller = __builtin_return_address(0);
    printf("FATAL ABORT from %p\n", caller);
    fflush(stdout);
    fflush(stderr);
    fclose(stdout);
    fclose(stderr);
    while (1) {
        svcSleepThread(1000000000);
    }
}

// UI mode: drives both input routing and which render path runs each frame.
// UI_MAP is the existing map/unit-control screen (untouched behavior);
// UI_CITY/UI_RESEARCH/UI_RATES are the new modal panels from handoff.md.
typedef enum { UI_MAP, UI_CITY, UI_RESEARCH, UI_RATES, UI_GOVERNMENT, UI_DIPLOMACY } UiMode;

// App-level state, one level above UiMode (handoff-nohw.md §A.3). The
// server coroutine is created in init_freeciv() but blocks before touching
// any Freeciv state until fc3ds_server_begin_new_game()/_begin_load_game()
// is called - so APP_START_MENU/APP_LOAD_PICKER must never call
// fc3ds_server_pump()/is_busy() or read any game state; they render their
// own screens and only transition to APP_GAME once a choice is made.
typedef enum { APP_START_MENU, APP_LOAD_PICKER, APP_GAME } AppState;

// Frames the "Saved" toast stays visible after a SAVE action-bar tap
// (§A.2) - ~1.5s at the 3DS's fixed 60fps.
#define SAVE_TOAST_FRAMES 90

#define MAX_BUILD_OPTIONS 200 // classic ruleset: <90 improvements + ~50 units
#define MAX_TECH_OPTIONS  100 // classic ruleset: <90 techs total
#define MAX_TECH_TREE_NODES 100 // every tech in the ruleset, not just researchable ones
#define MAX_GOV_OPTIONS    16 // classic ruleset: 6 governments; headroom for other rulesets
#define MAX_WORKLIST_ENTRIES 64 // matches common/worklist.h's MAX_LEN_WORKLIST
#define MAX_DIPL_PLAYERS 32 // headroom well above any realistic player count
#define DIPLO_GOLD_OFFER_AMOUNT 50 // fixed v1 offer - no numeric-entry widget exists (handoff.md §7.4)

// Shared hit-test for the row layout every bottom-panel scrollable list uses
// (build, research, government - handoff.md §1.3/§9): PANEL_LIST_Y down,
// PANEL_ROW_H per row, PANEL_ROWS_VISIBLE rows before the footer. Returns the
// tapped row's index into the list's [0, n) items, or -1 if py isn't over a
// populated row (below the last real item, or outside the list entirely).
static int hit_test_scroll_list(int py, int scroll, int n)
{
    int rel_y = py - PANEL_LIST_Y;
    if (rel_y < 0 || rel_y >= PANEL_ROW_H * PANEL_ROWS_VISIBLE) {
        return -1;
    }
    int row = scroll + rel_y / PANEL_ROW_H;
    return row < n ? row : -1;
}

// Adjusts tax or luxury by delta (a multiple of 10) and queues the rate
// change, keeping tax+lux+sci==100 by moving the balance to/from science.
// No-ops (doesn't queue) if either the adjusted rate or the resulting
// science rate would fall outside [0, civ->max_rate].
static void adjust_rate(const CivInfo *civ, bool tax_axis, int delta)
{
    int tax = civ->tax, lux = civ->lux;
    int *target = tax_axis ? &tax : &lux;
    int new_target = *target + delta;
    int new_sci = civ->sci - delta;
    if (new_target < 0 || new_target > civ->max_rate) return;
    if (new_sci < 0 || new_sci > civ->max_rate) return;
    *target = new_target;
    freeciv_request_set_rates(tax, lux, new_sci);
}

int main(int argc, char* argv[])
{
    // Redirect output to SD card to catch fatal errors
    freopen("sdmc:/freeciv_out.log", "w", stdout);
    freopen("sdmc:/freeciv_err.log", "w", stderr);
    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);

    u32 linearFree = osGetMemRegionFree(MEMREGION_BASE);
    u32 appFree = osGetMemRegionFree(MEMREGION_APPLICATION);
    printf("Linear free: %lu bytes\n", linearFree);
    printf("App free: %lu bytes\n", appFree);
    fflush(stdout);

    // On New3DS hardware, request the higher L2-cache/clock-speed mode
    // (804MHz CPU vs the Old3DS's 268MHz). No-op on Old3DS.
    bool isNew3DS = false;
    if (R_SUCCEEDED(APT_CheckNew3DS(&isNew3DS)) && isNew3DS) {
        osSetSpeedupEnable(true);
        printf("New3DS detected: clock speedup enabled\n");
    }
    fflush(stdout);

    // Initialize graphics
    gfxInitDefault();
    romfsInit();
    C3D_Init(C3D_DEFAULT_CMDBUF_SIZE);
    C2D_Init(C2D_DEFAULT_MAX_OBJECTS);
    C2D_Prepare();
    render_init();

    // Initialize Freeciv data path to point to romfs
    setenv("FREECIV_DATA_PATH", "romfs:/data", 1);

    g_spriteSheet = C2D_SpriteSheetLoad("romfs:/gfx/sprites.t3x");
    if (!g_spriteSheet) {
        printf("Failed to load sprites.t3x\n");
    } else {
        // Default bilinear filtering blends fully-transparent atlas pixels
        // (stored as RGB 255,255,255 / alpha 0) into their opaque neighbours,
        // showing up as a white halo around every masked sprite edge - most
        // visible on the coast overlay sprites. Point-sample instead; this is
        // also the crisper look for a pixel-art tileset anyway.
        C2D_Image img0 = C2D_SpriteSheetGetImage(g_spriteSheet, 0);
        if (img0.tex) {
            C3D_TexSetFilter(img0.tex, GPU_NEAREST, GPU_NEAREST);
        }
    }

    // Create screens
    C3D_RenderTarget* top = C2D_CreateScreenTarget(GFX_TOP, GFX_LEFT);
    C3D_RenderTarget* bottom = C2D_CreateScreenTarget(GFX_BOTTOM, GFX_LEFT);

    AppState app_state = APP_START_MENU;
    static SaveFile save_files[MAX_SAVE_FILES];
    int save_file_count = 0;
    int load_scroll = 0;
    int save_toast_frames = 0; // >0 while the "Saved" banner should show
    bool save_in_flight = false; // true from the SAV tap until the queued save's busy step ends

    int cam_x = 0;
    int cam_y = 0;
    int sel_x = -1, sel_y = -1;          // currently selected map tile (-1 = none)
    int selected_unit_id = -1;           // pov unit selected for movement (-1 = none)
    int last_touch_px = -1, last_touch_py = -1; // last raw stylus position

    // Goto-target sub-mode (handoff.md §2c): true from a GOTO action-bar tap
    // until the next map tap (destination) or a cancel (B, or re-tapping the
    // armed unit). Suppresses normal D-Pad unit control while active.
    bool ui_goto_armed = false;
    // Cached read-model snapshot of the armed unit's pending goto
    // destination (get_unit_goto_tile(), handoff.md §2b), refreshed only in
    // the !busy block below - same "hold last value across a busy frame"
    // contract as sel_x/sel_y. -1 = no pending route.
    int ui_goto_x = -1, ui_goto_y = -1;
    // True for the rest of a touch-down gesture once the goto-target branch
    // below has consumed its kDown edge (issued a goto or canceled). Without
    // this, the *next* held frame of the SAME physical tap - kDown already
    // false, but kHeld still true - falls through to the normal tile-arm
    // branch below (since ui_goto_armed just went false) and immediately
    // disarms the unit on the (typically empty) destination tile, discarding
    // the goto's own selection before it can ever be read back.
    bool touch_consumed_this_press = false;

    // Snapshot of the selected tile for the HUD. Persists across frames
    // where the server is busy (see fc3ds_server_is_busy() below) and the
    // snapshot can't be safely refreshed.
    TileInfo sel_info;
    bool have_sel = false;

    // City-screen / civ-control modal state (handoff.md goals A/B).
    UiMode ui_mode = UI_MAP;
    int ui_city_id = -1;
    int ui_scroll = 0; // list scroll offset, reused per modal
    int ui_garrison_sel = 0; // L/R-cycled garrison index, reset per city open (handoff.md §3)
    bool ui_worklist_queue_mode = false; // NOW (false) / QUEUE (true) toggle, reset per city open (handoff.md §6)
    int ui_dipl_stage = 1; // 1=player picker, 2=clause editor (handoff.md §7.4)
    int ui_dipl_counterpart = -1;
    char ui_dipl_counterpart_name[48] = "";
    // Tracks whether get_diplo_info() has EVER reported an active meeting
    // since ui_dipl_stage last became 2 - distinguishes "the just-queued
    // CMD_DIPLOMACY_INIT hasn't drained server-side yet" (active starts
    // false, seen_active still false - keep waiting) from "the meeting
    // really resolved/was canceled" (active went true then false - auto-
    // return to UI_MAP). Without this, the refresh block below would bounce
    // straight back to UI_MAP the same frame stage 2 opens.
    bool ui_dipl_seen_active = false;

    // Snapshots for whichever modal is open - filled once per !busy frame
    // (see the ui_mode blocks below), read-only outside that refresh.
    CityInfo ui_city_info;
    static BuildOption ui_build_opts[MAX_BUILD_OPTIONS];
    int ui_build_count = 0;
    CivInfo ui_civ_info;
    static TechOption ui_tech_opts[MAX_TECH_OPTIONS];
    int ui_tech_count = 0;
    static TechNode ui_tech_tree[MAX_TECH_TREE_NODES];
    int ui_tech_tree_count = 0;
    bool ui_tech_tree_view = false; // false=flat researchable list, true=tiered tree (handoff.md §8)
    static GovOption ui_gov_opts[MAX_GOV_OPTIONS];
    int ui_gov_count = 0;
    static WorklistEntry ui_worklist[MAX_WORKLIST_ENTRIES];
    int ui_worklist_count = 0;
    static PlayerOption ui_other_players[MAX_DIPL_PLAYERS];
    int ui_other_player_count = 0;
    DiploInfo ui_diplo_info;

    // Initialize the Freeciv game engine
    init_freeciv(isNew3DS);

    // Main loop
    while (aptMainLoop())
    {
        hidScanInput();
        u32 kDown = hidKeysDown();
        u32 kHeld = hidKeysHeld();

        // Mode as it was at the START of this frame. The modal touch handler
        // gates on this (not the live ui_mode) so that the same tap which
        // opens a city (map handler below flips UI_MAP -> UI_CITY mid-frame)
        // isn't ALSO consumed by the panel handler - which, on a revisited
        // city, would mis-fire a production change using the previous city's
        // stale build list.
        UiMode ui_mode_frame_start = ui_mode;

        if (kDown & KEY_START) {
            break; // fc3ds_server_shutdown() (below, after the loop) does the rest
        }

        // Start menu / load picker (handoff-nohw.md §A.3): the server
        // coroutine is parked before srv_init() until one of the "begin"
        // calls below releases it, so this branch must never call
        // fc3ds_server_pump()/is_busy() or touch any Freeciv game state -
        // it only handles its own touch input and renders its own screens,
        // then `continue`s past the entire game loop body below.
        if (app_state != APP_GAME) {
            if (kDown & KEY_TOUCH) {
                touchPosition touch;
                hidTouchRead(&touch);
                int px = (int)touch.px, py = (int)touch.py;

                if (app_state == APP_START_MENU) {
                    if (px >= STARTMENU_BTN_X && px < STARTMENU_BTN_X + STARTMENU_BTN_W) {
                        if (py >= STARTMENU_NEW_Y && py < STARTMENU_NEW_Y + STARTMENU_BTN_H) {
                            bool dev = (kHeld & KEY_L) && (kHeld & KEY_R); // hold both shoulders = dev quick game
                            fc3ds_server_begin_new_game(dev);
                            app_state = APP_GAME;
                        } else if (py >= STARTMENU_LOAD_Y && py < STARTMENU_LOAD_Y + STARTMENU_BTN_H) {
                            save_file_count = get_save_files(save_files, MAX_SAVE_FILES);
                            load_scroll = 0;
                            app_state = APP_LOAD_PICKER;
                        }
                    }
                } else { // APP_LOAD_PICKER
                    if (py >= PANEL_FOOTER_Y) {
                        app_state = APP_START_MENU;
                    } else {
                        int row = hit_test_scroll_list(py, load_scroll, save_file_count);
                        if (row >= 0) {
                            fc3ds_server_begin_load_game(save_files[row].filename);
                            app_state = APP_GAME;
                        }
                    }
                }
            }
            if (app_state == APP_LOAD_PICKER) {
                if (kDown & KEY_DUP)   load_scroll--;
                if (kDown & KEY_DDOWN) load_scroll++;
                int max_scroll = save_file_count > PANEL_ROWS_VISIBLE
                                 ? save_file_count - PANEL_ROWS_VISIBLE : 0;
                if (load_scroll < 0) load_scroll = 0;
                if (load_scroll > max_scroll) load_scroll = max_scroll;
                if (kDown & KEY_B) app_state = APP_START_MENU;
            }

            C3D_FrameBegin(C3D_FRAME_SYNCDRAW);

            C2D_TargetClear(top, C2D_Color32(0x14, 0x1C, 0x28, 0xFF));
            C2D_SceneBegin(top);
            C2D_SceneSize(GSP_SCREEN_WIDTH, GSP_SCREEN_HEIGHT_TOP, true);

            C2D_TargetClear(bottom, C2D_Color32(0x10, 0x10, 0x10, 0xFF));
            C2D_SceneBegin(bottom);
            C2D_SceneSize(GSP_SCREEN_WIDTH, GSP_SCREEN_HEIGHT_BOTTOM, true);
            if (app_state == APP_START_MENU) {
                render_start_menu();
            } else {
                render_load_picker(save_files, save_file_count, load_scroll);
            }

            C3D_FrameEnd(0);
            continue;
        }

        // A button: end the current turn (advance the game). Suppressed
        // inside a modal so a long turn step can't strand the panel behind
        // the busy overlay (handoff.md §7.3) - close the modal first.
        if (ui_mode == UI_MAP && (kDown & KEY_A)) {
            fc3ds_server_request_end_turn();
        }

        // SELECT: re-center the camera on your own units. A manual recovery
        // for the "stared at black fog" case - if auto-center ever misses (or
        // you pan off into unexplored territory), this snaps back to unit[0].
        // Gated on !fc3ds_server_is_busy(): find_pov_unit_tile() reads live
        // Freeciv state (pov->units, unit_tile()) directly, and this runs
        // BEFORE this frame's fc3ds_server_pump()/busy snapshot below - an
        // ungated call here can race the server thread mid-mutation (e.g. a
        // SELECT press during/just after world-gen), which plausibly
        // explains "SELECT doesn't help" reports on a freshly generated
        // world (hardware finding 2026-07-11, handoff.md) - every other
        // in-process game-state read in this file is gated the same way.
        if (ui_mode == UI_MAP && (kDown & KEY_SELECT) && !fc3ds_server_is_busy()) {
            int cx, cy;
            if (find_pov_unit_tile(&cx, &cy)) {
                cam_x = cx * 30 + 15 - 160;
                cam_y = cy * 30 + 15 - 120;
                sel_x = cx; sel_y = cy;
            }
        }

        // L/R: reach the civ-level control panels from the map. B closes
        // whichever modal is open (handled per-mode below).
        if (ui_mode == UI_MAP && (kDown & KEY_L)) {
            ui_mode = UI_RESEARCH;
            ui_scroll = 0;
            ui_tech_tree_view = false;
        }
        if (ui_mode == UI_MAP && (kDown & KEY_R)) {
            ui_mode = UI_RATES;
            ui_scroll = 0;
        }

        circlePosition pos;
        hidCircleRead(&pos);

        if (ui_mode == UI_CITY || ui_mode == UI_RESEARCH || ui_mode == UI_GOVERNMENT
            || ui_mode == UI_DIPLOMACY) {
            // D-Pad up/down scrolls the open list; B closes the panel.
            if (kDown & KEY_DUP)   ui_scroll--;
            if (kDown & KEY_DDOWN) ui_scroll++;
            if (kDown & KEY_B) {
                // B always fully exits to UI_MAP (universal escape hatch,
                // same as every other panel) - but from the diplomacy
                // clause editor, that must also cancel the meeting, or it'd
                // be orphaned server-side with no way back in (handoff.md
                // §7.4's "Close" footer button makes the same call).
                if (ui_mode == UI_DIPLOMACY && ui_dipl_stage == 2) {
                    freeciv_request_diplomacy_cancel(ui_dipl_counterpart);
                }
                ui_mode = UI_MAP;
            }

            // L/R (city only): cycle which garrisoned unit Y takes out. Free
            // to repurpose here - L/R only open Research/Rates from UI_MAP
            // (above), so they're unclaimed while a city is open.
            if (ui_mode == UI_CITY && ui_city_info.garrison_count > 0) {
                if (kDown & KEY_L) {
                    ui_garrison_sel = (ui_garrison_sel - 1 + ui_city_info.garrison_count)
                                      % ui_city_info.garrison_count;
                }
                if (kDown & KEY_R) {
                    ui_garrison_sel = (ui_garrison_sel + 1) % ui_city_info.garrison_count;
                }
            }

            // Y (city only): pull the L/R-highlighted garrisoned unit out
            // onto the map and arm it for D-Pad movement. A unit sharing a
            // city tile is otherwise unreachable, since tapping the tile
            // opens the city.
            if (ui_mode == UI_CITY && (kDown & KEY_Y)
                && ui_garrison_sel >= 0 && ui_garrison_sel < ui_city_info.garrison_count) {
                int uid = ui_city_info.garrison_ids[ui_garrison_sel];
                selected_unit_id = uid;
                int ux, uy;
                if (find_unit_tile_by_id(uid, &ux, &uy)) {
                    sel_x = ux; sel_y = uy;
                }
                ui_mode = UI_MAP; // back to the map with the unit armed
            }

            // Y (research only): toggle flat researchable list <-> tiered
            // tech-tree view (handoff.md §8.4). Free to repurpose here - Y
            // only takes a garrisoned unit out while UI_CITY is open (above).
            // Different scroll units between the two views (techs vs
            // tier-rows), so reset it rather than carry over a stale offset.
            if (ui_mode == UI_RESEARCH && (kDown & KEY_Y)) {
                ui_tech_tree_view = !ui_tech_tree_view;
                ui_scroll = 0;
            }
        } else if (ui_mode == UI_RATES) {
            if (kDown & KEY_B) ui_mode = UI_MAP;
        } else if (selected_unit_id >= 0 && ui_goto_armed) {
            // Goto-target sub-mode (handoff.md §2c): the next map tap below
            // supplies the destination, so D-Pad move/action input is
            // suppressed here - only B cancels back to plain unit control.
            if (kDown & KEY_B) {
                ui_goto_armed = false;
            }
        } else if (selected_unit_id >= 0) {
            // A unit is selected: D-Pad orders a one-tile move instead of
            // panning. Edge-triggered (kDown, not kHeld) so holding the
            // button doesn't rapid-fire moves and drain movement points -
            // one press, one tile.
            //
            // Diagonals are a physical D-Pad corner press, but real fingers
            // rarely close both switches in the exact same ~16ms frame - a
            // strict "both kDown this frame" check (tried first) made
            // diagonals feel unreliable in testing. Instead, the first axis
            // pressed (vertical or horizontal) is held pending for a short
            // window (DIAG_WINDOW_FRAMES); if the other axis arrives within
            // that window, the pair fires as one diagonal move instead of
            // two cardinal ones. If the window lapses with no pair, the
            // single cardinal move fires as normal - still one press, one
            // tile, just with a brief (~130ms) buffer to catch imprecise
            // corner presses.
            #define DIAG_WINDOW_FRAMES 8
            static int vert_pending = 0, horiz_pending = 0;
            static bool vert_is_up = false, horiz_is_right = false;

            bool up_p = kDown & KEY_DUP, down_p = kDown & KEY_DDOWN;
            bool left_p = kDown & KEY_DLEFT, right_p = kDown & KEY_DRIGHT;

            if (up_p || down_p) {
                if (horiz_pending > 0) {
                    bool is_up = up_p;
                    freeciv_request_unit_move(selected_unit_id,
                        is_up ? (horiz_is_right ? MOVE_DIR_NORTHEAST : MOVE_DIR_NORTHWEST)
                              : (horiz_is_right ? MOVE_DIR_SOUTHEAST : MOVE_DIR_SOUTHWEST));
                    horiz_pending = 0;
                } else {
                    vert_pending = DIAG_WINDOW_FRAMES;
                    vert_is_up = up_p;
                }
            }
            if (left_p || right_p) {
                if (vert_pending > 0) {
                    bool is_right = right_p;
                    freeciv_request_unit_move(selected_unit_id,
                        is_right ? (vert_is_up ? MOVE_DIR_NORTHEAST : MOVE_DIR_SOUTHEAST)
                                 : (vert_is_up ? MOVE_DIR_NORTHWEST : MOVE_DIR_SOUTHWEST));
                    vert_pending = 0;
                } else {
                    horiz_pending = DIAG_WINDOW_FRAMES;
                    horiz_is_right = right_p;
                }
            }
            if (vert_pending > 0 && --vert_pending == 0) {
                freeciv_request_unit_move(selected_unit_id, vert_is_up ? MOVE_DIR_NORTH : MOVE_DIR_SOUTH);
            }
            if (horiz_pending > 0 && --horiz_pending == 0) {
                freeciv_request_unit_move(selected_unit_id, horiz_is_right ? MOVE_DIR_EAST : MOVE_DIR_WEST);
            }

            // B: Fortify/Sentry toggle. X: context action (Found City for a
            // Settler, else Build Road/Irrigate/Mine for a Worker). Y:
            // Disband. All no-ops if the selected unit doesn't support them.
            if (kDown & KEY_B) freeciv_request_unit_action(selected_unit_id, UNIT_ACTION_FORTIFY_SENTRY);
            if (kDown & KEY_X) freeciv_request_unit_action(selected_unit_id, UNIT_ACTION_CONTEXT);
            if (kDown & KEY_Y) freeciv_request_unit_action(selected_unit_id, UNIT_ACTION_DISBAND);
        } else {
            if (kHeld & KEY_DUP)    cam_y -= 2;
            if (kHeld & KEY_DDOWN)  cam_y += 2;
            if (kHeld & KEY_DLEFT)  cam_x -= 2;
            if (kHeld & KEY_DRIGHT) cam_x += 2;
        }

        // Circle Pad analog panning (deadzone of 15) - always active on the
        // map, even with a unit selected, so the camera can still be
        // repositioned. No effect while a modal is open (map isn't drawn).
        if (ui_mode == UI_MAP) {
            if (abs(pos.dx) > 15) cam_x += pos.dx / 32;
            if (abs(pos.dy) > 15) cam_y -= pos.dy / 32;
        }

        // Let the server take one step if any input above queued work (end
        // turn, a move, an action) and it isn't already busy/paused. Must
        // run before the busy check below so a step queued THIS frame is
        // already reflected in `busy` - the server coroutine may start
        // mutating game state as early as this call.
        fc3ds_server_pump();
        bool busy = fc3ds_server_is_busy();
        if (!busy) save_in_flight = false; // the queued save's busy step has ended

        if (save_toast_frames > 0) save_toast_frames--;

        // Touch: pick a map tile. Overhead square grid => exact mapping.
        // Update while the stylus is held so dragging re-selects and the
        // debug crosshair tracks the pen. Skipped while busy - reads live
        // game state (get_tile_info) that's unsafe to touch mid-step.
        if (!busy && ui_mode == UI_MAP && (kHeld & KEY_TOUCH)) {
            touchPosition touch;
            hidTouchRead(&touch);
            last_touch_px = touch.px;
            last_touch_py = touch.py;

            if (kDown & KEY_TOUCH) {
                touch_consumed_this_press = false; // fresh press
            }

            // Action bar (handoff.md §2) lives on the bottom-screen right
            // edge and is checked FIRST, before any tile-select/arm logic,
            // so a tap on the strip never also arms a unit on the tile
            // underneath it. Edge-triggered (kDown) so opening a panel
            // doesn't re-fire every frame the stylus stays down.
            bool in_actionbar = touch.px >= ACTIONBAR_X && touch.px < ACTIONBAR_X + ACTIONBAR_BTN_W;
            if (in_actionbar) {
                if (kDown & KEY_TOUCH) {
                    if (touch.py >= ACTIONBAR_RES_Y && touch.py < ACTIONBAR_RES_Y + ACTIONBAR_BTN_H) {
                        ui_mode = UI_RESEARCH;
                        ui_scroll = 0;
                        ui_tech_tree_view = false;
                    } else if (touch.py >= ACTIONBAR_RATES_Y && touch.py < ACTIONBAR_RATES_Y + ACTIONBAR_BTN_H) {
                        ui_mode = UI_RATES;
                        ui_scroll = 0;
                    } else if (touch.py >= ACTIONBAR_GOV_Y && touch.py < ACTIONBAR_GOV_Y + ACTIONBAR_BTN_H) {
                        ui_mode = UI_GOVERNMENT;
                        ui_scroll = 0;
                    } else if (touch.py >= ACTIONBAR_DIPL_Y && touch.py < ACTIONBAR_DIPL_Y + ACTIONBAR_BTN_H) {
                        ui_mode = UI_DIPLOMACY;
                        ui_scroll = 0;
                        ui_dipl_stage = 1;
                    } else if (touch.py >= ACTIONBAR_SAVE_Y && touch.py < ACTIONBAR_SAVE_Y + ACTIONBAR_BTN_H) {
                        freeciv_request_save_game();
                        save_toast_frames = SAVE_TOAST_FRAMES;
                        save_in_flight = true;
                    } else if (touch.py >= ACTIONBAR_GOTO_Y && touch.py < ACTIONBAR_GOTO_Y + ACTIONBAR_BTN_H) {
                        // Only meaningful with a unit armed - a no-op tap
                        // otherwise, same as the SAV/etc buttons never
                        // checking preconditions either (handoff.md §2c).
                        if (selected_unit_id >= 0) {
                            ui_goto_armed = true;
                        }
                    }
                }
            } else if (ui_goto_armed) {
                // Goto-target sub-mode: the next map tap is the destination
                // (handoff.md §2c). Edge-triggered (kDown) so dragging the
                // stylus across several tiles doesn't fire multiple gotos.
                // Tapping the armed unit's own tile again cancels instead of
                // issuing a (meaningless) goto-to-self.
                if (kDown & KEY_TOUCH) {
                    int wx = (int)touch.px + cam_x;
                    int wy = (int)touch.py + cam_y;
                    int tx = (int)floorf(wx / 30.0f);
                    int ty = (int)floorf(wy / 30.0f);
                    int mw = 40, mh = 40;
                    get_map_dimensions(&mw, &mh);
                    // Native coords: fold a seam-crossing tap back onto the map.
                    if (map_wraps_x()) tx = ((tx % mw) + mw) % mw;
                    if (map_wraps_y()) ty = ((ty % mh) + mh) % mh;
                    if (tx >= 0 && ty >= 0 && tx < mw && ty < mh) {
                        TileInfo touched;
                        get_tile_info(tx, ty, &touched);
                        if (touched.unit_id == selected_unit_id) {
                            ui_goto_armed = false; // re-tapped the armed unit: cancel
                        } else {
                            freeciv_request_unit_goto(selected_unit_id, tx, ty);
                            ui_goto_armed = false;
                            sel_x = tx;
                            sel_y = ty;
                        }
                        // Otherwise the very next held frame of this same
                        // physical tap - kDown now false, ui_goto_armed now
                        // false too - would fall into the plain tile-arm
                        // branch below and immediately disarm the unit on
                        // what's typically an empty destination tile.
                        touch_consumed_this_press = true;
                    }
                }
            } else if (!touch_consumed_this_press) {
                int wx = (int)touch.px + cam_x; // world pixel under the stylus
                int wy = (int)touch.py + cam_y;
                int tx = (int)floorf(wx / 30.0f);
                int ty = (int)floorf(wy / 30.0f);
                int mw = 40, mh = 40;
                get_map_dimensions(&mw, &mh);
                // Native coords: fold a seam-crossing tap back onto the map.
                if (map_wraps_x()) tx = ((tx % mw) + mw) % mw;
                if (map_wraps_y()) ty = ((ty % mh) + mh) % mh;
                if (tx >= 0 && ty >= 0 && tx < mw && ty < mh) {
                    sel_x = tx;
                    sel_y = ty;

                    // A pov-owned city tile opens the city screen - takes
                    // priority over unit-arming on a tile that has both, since
                    // units are moved from the map and cities are managed from
                    // their own screen (handoff.md §3.4). Anything else (empty,
                    // enemy, a bare pov unit) falls through to the existing
                    // unit-arm/disarm rule - a single interaction mode, no
                    // separate cancel control needed.
                    TileInfo touched;
                    get_tile_info(tx, ty, &touched);
                    if (touched.city_owner >= 0 && is_pov_unit_owner(touched.city_owner)) {
                        ui_mode = UI_CITY;
                        ui_city_id = touched.city_id;
                        ui_scroll = 0;
                        ui_garrison_sel = 0;
                        ui_worklist_queue_mode = false;
                        selected_unit_id = -1;
                        ui_goto_armed = false;
                    } else if (touched.unit_id >= 0 && is_pov_unit_owner(touched.unit_owner)) {
                        selected_unit_id = touched.unit_id;
                    } else {
                        selected_unit_id = -1;
                        ui_goto_armed = false;
                    }
                }
            }
        }

        // Touch inside a modal panel: a single tap (kDown, not kHeld) hits
        // a list row or a footer button. Rects here must match what
        // render_city_panel()/render_research_panel()/render_rates_panel()
        // (render.c) actually draw - they share the PANEL_*/RATES_* layout
        // constants from render.h for exactly that reason.
        if (!busy && ui_mode_frame_start != UI_MAP && (kDown & KEY_TOUCH)) {
            touchPosition touch;
            hidTouchRead(&touch);
            int px = (int)touch.px, py = (int)touch.py;

            if (ui_mode == UI_CITY) {
                // NOW/QUEUE toggle lives in the header band, above the list/
                // footer rows below - checked first so it can never collide
                // with them (handoff.md §6.4).
                if (px >= CITY_TOGGLE_X && px < CITY_TOGGLE_X + CITY_TOGGLE_W
                    && py >= CITY_TOGGLE_Y && py < CITY_TOGGLE_Y + CITY_TOGGLE_H) {
                    ui_worklist_queue_mode = !ui_worklist_queue_mode;
                } else if (py >= PANEL_FOOTER_Y) {
                    if (px < 160) {
                        if (ui_city_info.can_buy) freeciv_request_city_buy(ui_city_id);
                    } else {
                        ui_mode = UI_MAP;
                    }
                } else if (px < CITY_WORKLIST_COL_X) {
                    // Buildables list (left column): NOW builds it immediately
                    // (existing behavior), QUEUE appends it to the worklist
                    // instead - same row, different ring command.
                    int row = hit_test_scroll_list(py, ui_scroll, ui_build_count);
                    if (row >= 0) {
                        if (ui_worklist_queue_mode) {
                            freeciv_request_city_worklist_append(ui_city_id, ui_build_opts[row].kind,
                                                                 ui_build_opts[row].value);
                        } else {
                            freeciv_request_city_production(ui_city_id, ui_build_opts[row].kind,
                                                            ui_build_opts[row].value);
                        }
                    }
                } else {
                    // Worklist column (right, "beside" the buildables list,
                    // unscrolled in v1): tapping a queued row removes it.
                    int row = hit_test_scroll_list(py, 0, ui_worklist_count);
                    if (row >= 0) {
                        freeciv_request_city_worklist_remove(ui_city_id, row);
                    }
                }
            } else if (ui_mode == UI_RESEARCH) {
                if (ui_tech_tree_view) {
                    int idx = techtree_hit_test(ui_tech_tree, ui_tech_tree_count, ui_scroll, px, py);
                    // state==1 is TECH_PREREQS_KNOWN (see TechNode's comment) -
                    // the only tappable box color; KNOWN/UNKNOWN no-op, same
                    // as the grey/dim boxes being drawn non-tappable.
                    if (idx >= 0 && ui_tech_tree[idx].state == 1) {
                        freeciv_request_set_research(ui_tech_tree[idx].tech);
                    } else if (idx < 0 && py >= PANEL_FOOTER_Y) {
                        ui_mode = UI_MAP;
                    }
                } else {
                    int row = hit_test_scroll_list(py, ui_scroll, ui_tech_count);
                    if (row >= 0) {
                        freeciv_request_set_research(ui_tech_opts[row].tech);
                    } else if (py >= PANEL_FOOTER_Y) {
                        ui_mode = UI_MAP;
                    }
                }
            } else if (ui_mode == UI_GOVERNMENT) {
                int row = hit_test_scroll_list(py, ui_scroll, ui_gov_count);
                if (row >= 0) {
                    if (ui_gov_opts[row].available) {
                        freeciv_request_set_government(ui_gov_opts[row].gov_id);
                    }
                } else if (py >= PANEL_FOOTER_Y) {
                    ui_mode = UI_MAP;
                }
            } else if (ui_mode == UI_DIPLOMACY) {
                if (ui_dipl_stage == 1) {
                    int row = hit_test_scroll_list(py, ui_scroll, ui_other_player_count);
                    if (row >= 0) {
                        ui_dipl_counterpart = ui_other_players[row].player_id;
                        snprintf(ui_dipl_counterpart_name, sizeof(ui_dipl_counterpart_name),
                                "%s", ui_other_players[row].name);
                        freeciv_request_diplomacy_init(ui_dipl_counterpart);
                        ui_dipl_stage = 2;
                        ui_dipl_seen_active = false;
                    } else if (py >= PANEL_FOOTER_Y) {
                        ui_mode = UI_MAP;
                    }
                } else if (py >= DIPLO_ADDCLAUSE_Y && py < DIPLO_ADDCLAUSE_Y + DIPLO_ADDCLAUSE_H) {
                    // Fixed add-clause menu - giver is always pov (§7.4 v1).
                    int slot_w = DIPLO_ADDCLAUSE_BTN_W + DIPLO_ADDCLAUSE_GAP;
                    int btn = px / slot_w;
                    int rel = px % slot_w;
                    if (btn >= 0 && btn < 4 && rel < DIPLO_ADDCLAUSE_BTN_W) {
                        switch (btn) {
                        case 0:
                            freeciv_request_diplomacy_add_clause(ui_dipl_counterpart, DIPLO_CLAUSE_PEACE, 0);
                            break;
                        case 1:
                            freeciv_request_diplomacy_add_clause(ui_dipl_counterpart, DIPLO_CLAUSE_CEASEFIRE, 0);
                            break;
                        case 2:
                            freeciv_request_diplomacy_add_clause(ui_dipl_counterpart, DIPLO_CLAUSE_EMBASSY, 0);
                            break;
                        case 3:
                            freeciv_request_diplomacy_add_clause(ui_dipl_counterpart, DIPLO_CLAUSE_GOLD,
                                                                 DIPLO_GOLD_OFFER_AMOUNT);
                            break;
                        }
                    }
                } else if (py >= PANEL_FOOTER_Y) {
                    if (px < DIPLO_FOOTER_SPLIT1) {
                        freeciv_request_diplomacy_accept(ui_dipl_counterpart);
                    } else if (px < DIPLO_FOOTER_SPLIT2) {
                        // Cancel: back to the player picker, per handoff.md §7.4.
                        freeciv_request_diplomacy_cancel(ui_dipl_counterpart);
                        ui_dipl_stage = 1;
                        ui_scroll = 0;
                    } else {
                        // Close: also cancels, to avoid an orphaned meeting
                        // with no "resume" entry point in v1 (§7.4's own
                        // recommendation).
                        freeciv_request_diplomacy_cancel(ui_dipl_counterpart);
                        ui_mode = UI_MAP;
                    }
                } else {
                    // Clause list - not independently scrollable in v1
                    // (matches render_diplomacy_panel()'s window), so scroll
                    // is always 0 here regardless of ui_scroll.
                    int row = hit_test_scroll_list(py, 0, ui_diplo_info.clause_count);
                    if (row >= 0) {
                        freeciv_request_diplomacy_remove_clause(ui_dipl_counterpart,
                                                                ui_diplo_info.clauses[row].type,
                                                                ui_diplo_info.clauses[row].value);
                    }
                }
            } else if (ui_mode == UI_RATES) {
                if (py >= RATES_TAX_Y && py < RATES_TAX_Y + RATES_ROW_TOUCH_H) {
                    if (px >= RATES_BTN_MINUS_X && px < RATES_BTN_MINUS_X + RATES_BTN_W) {
                        adjust_rate(&ui_civ_info, true, -10);
                    } else if (px >= RATES_BTN_PLUS_X && px < RATES_BTN_PLUS_X + RATES_BTN_W) {
                        adjust_rate(&ui_civ_info, true, 10);
                    }
                } else if (py >= RATES_LUX_Y && py < RATES_LUX_Y + RATES_ROW_TOUCH_H) {
                    if (px >= RATES_BTN_MINUS_X && px < RATES_BTN_MINUS_X + RATES_BTN_W) {
                        adjust_rate(&ui_civ_info, false, -10);
                    } else if (px >= RATES_BTN_PLUS_X && px < RATES_BTN_PLUS_X + RATES_BTN_W) {
                        adjust_rate(&ui_civ_info, false, 10);
                    }
                } else if (py >= PANEL_FOOTER_Y) {
                    ui_mode = UI_MAP;
                }
            }
        }

        // Handle game logic and inputs
        step_freeciv(kDown, kHeld);

        // The rest of this iteration only touches Freeciv game state
        // (unit/tile lookups, map walk) while the server is parked - see
        // the invariant in fc3ds_server.h. While busy, cam/sel/sel_info
        // simply keep last frame's values.
        if (!busy) {
            // Keep the selection glued to its unit as queued moves land on
            // the server thread.
            if (selected_unit_id >= 0) {
                int ux, uy;
                if (find_unit_tile_by_id(selected_unit_id, &ux, &uy)) {
                    sel_x = ux;
                    sel_y = uy;
                } else {
                    selected_unit_id = -1; // unit no longer exists
                    ui_goto_armed = false;
                }
            }
            if (!get_unit_goto_tile(selected_unit_id, &ui_goto_x, &ui_goto_y)) {
                ui_goto_x = ui_goto_y = -1;
            }

            // Auto-center the camera on the pov player's own first unit, ONCE,
            // as soon as it exists, and arm it for D-Pad movement. Units are
            // placed a little after the map is generated (and different
            // players' units can appear on different frames, pov's not
            // necessarily first), so keep retrying every non-busy frame until
            // find_pov_unit_tile() succeeds specifically for pov - not just
            // any player's unit. No give-up fallback: landing on an AI's tile
            // is useless (un-armed, and outside pov's fog of war so it renders
            // as solid black) - worse than waiting the extra second or two for
            // pov's own units, which every civ always eventually gets.
            static bool camera_centered = false;
            if (!camera_centered) {
                int cx = -1, cy = -1;
                if (find_pov_unit_tile(&cx, &cy)) {
                    // Center the 30px tile in the 320x240 bottom screen.
                    cam_x = cx * 30 + 15 - 160;
                    cam_y = cy * 30 + 15 - 120;
                    sel_x = cx; sel_y = cy; // select it so the HUD shows your unit
                    camera_centered = true;

                    // selected_unit_id must always track sel_x/sel_y's unit, or
                    // the "D-Pad: move" hint (keyed off the selected tile) would
                    // desync from whether D-Pad actually moves anything.
                    TileInfo centered;
                    get_tile_info(cx, cy, &centered);
                    if (centered.unit_id >= 0 && is_pov_unit_owner(centered.unit_owner)) {
                        selected_unit_id = centered.unit_id;
                    }
                }
            }

            // One-shot POV diagnostic: log who the client thinks "you" are
            // and whether the camera found your units, the first frame the
            // game is actually running. Lands in sdmc:/freeciv_out.log so a
            // "black fog" session can be diagnosed after the fact via FTP.
            static bool pov_logged = false;
            if (!pov_logged && get_game_turn() > 0) {
                char nm[48]; bool ai = false; int pidx = -1;
                int nu = get_pov_summary(nm, sizeof(nm), &ai, &pidx);
                int dx = -1, dy = -1;
                bool found = find_pov_unit_tile(&dx, &dy);
                printf("POV DIAG: player='%s' idx=%d is_ai=%d units=%d "
                       "found_unit_tile=%d (%d,%d) cam=(%d,%d)\n",
                       nm, pidx, (int)ai, nu, (int)found, dx, dy, cam_x, cam_y);
                fflush(stdout);
                pov_logged = true;
            }

            // Snapshot the selected tile's state for the HUD.
            if (sel_x >= 0) {
                get_tile_info(sel_x, sel_y, &sel_info);
                have_sel = true;
            } else {
                have_sel = false;
            }

            // Refresh whichever modal snapshot is open, once per !busy frame
            // (handoff.md §7.4 - the buildable/researchable set can change
            // between frames, e.g. a tech completing or a building
            // finishing). A city can also be captured/destroyed while its
            // screen is open; re-resolving by id every frame and dropping
            // back to UI_MAP on invalidation (§3.5) means no raw
            // struct city*/struct player* is ever held across frames.
            if (ui_mode == UI_CITY) {
                get_city_info(ui_city_id, &ui_city_info);
                if (!ui_city_info.valid) {
                    ui_mode = UI_MAP;
                } else {
                    ui_build_count = get_city_buildables(ui_city_id, ui_build_opts, MAX_BUILD_OPTIONS);
                    int max_scroll = ui_build_count > PANEL_ROWS_VISIBLE ? ui_build_count - PANEL_ROWS_VISIBLE : 0;
                    if (ui_scroll < 0) ui_scroll = 0;
                    if (ui_scroll > max_scroll) ui_scroll = max_scroll;
                    ui_worklist_count = get_city_worklist(ui_city_id, ui_worklist, MAX_WORKLIST_ENTRIES);

                    // Garrison may have shrunk (unit taken out, disbanded,
                    // city captured mid-refresh) - keep the L/R cursor in
                    // bounds so Y never indexes a stale/OOB entry (§3.5).
                    if (ui_city_info.garrison_count == 0) {
                        ui_garrison_sel = 0;
                    } else if (ui_garrison_sel >= ui_city_info.garrison_count) {
                        ui_garrison_sel = ui_city_info.garrison_count - 1;
                    }
                }
            } else if (ui_mode == UI_RESEARCH) {
                get_civ_info(&ui_civ_info);
                if (ui_tech_tree_view) {
                    ui_tech_tree_count = get_tech_tree(ui_tech_tree, MAX_TECH_TREE_NODES);
                    // Scroll counts tier-rows (a consecutive same-tier run in
                    // the tier-sorted array - see TechNode's comment), not
                    // techs, so clamp against the row count, not the tech count.
                    int tier_rows = 0, last_tier = -1;
                    for (int i = 0; i < ui_tech_tree_count; i++) {
                        if (ui_tech_tree[i].tier != last_tier) {
                            tier_rows++;
                            last_tier = ui_tech_tree[i].tier;
                        }
                    }
                    int max_scroll = tier_rows > PANEL_ROWS_VISIBLE ? tier_rows - PANEL_ROWS_VISIBLE : 0;
                    if (ui_scroll < 0) ui_scroll = 0;
                    if (ui_scroll > max_scroll) ui_scroll = max_scroll;
                } else {
                    ui_tech_count = get_researchable(ui_tech_opts, MAX_TECH_OPTIONS);
                    int max_scroll = ui_tech_count > PANEL_ROWS_VISIBLE ? ui_tech_count - PANEL_ROWS_VISIBLE : 0;
                    if (ui_scroll < 0) ui_scroll = 0;
                    if (ui_scroll > max_scroll) ui_scroll = max_scroll;
                }
            } else if (ui_mode == UI_RATES) {
                get_civ_info(&ui_civ_info);
            } else if (ui_mode == UI_GOVERNMENT) {
                get_civ_info(&ui_civ_info);
                ui_gov_count = get_governments(ui_gov_opts, MAX_GOV_OPTIONS);
                int max_scroll = ui_gov_count > PANEL_ROWS_VISIBLE ? ui_gov_count - PANEL_ROWS_VISIBLE : 0;
                if (ui_scroll < 0) ui_scroll = 0;
                if (ui_scroll > max_scroll) ui_scroll = max_scroll;
            } else if (ui_mode == UI_DIPLOMACY) {
                if (ui_dipl_stage == 1) {
                    ui_other_player_count = get_other_players(ui_other_players, MAX_DIPL_PLAYERS);
                    int max_scroll = ui_other_player_count > PANEL_ROWS_VISIBLE
                                     ? ui_other_player_count - PANEL_ROWS_VISIBLE : 0;
                    if (ui_scroll < 0) ui_scroll = 0;
                    if (ui_scroll > max_scroll) ui_scroll = max_scroll;
                } else {
                    get_diplo_info(ui_dipl_counterpart, &ui_diplo_info);
                    if (ui_diplo_info.active) {
                        ui_dipl_seen_active = true;
                    } else if (ui_dipl_seen_active) {
                        // The meeting was open and now isn't - resolved
                        // (both accepted) or canceled from the other side.
                        // Auto-return to the map (handoff.md §7.4 stage 3).
                        // If it's simply not-yet-open (CMD_DIPLOMACY_INIT
                        // queued but not yet drained server-side),
                        // ui_dipl_seen_active is still false and we keep
                        // waiting instead of bouncing back prematurely.
                        ui_mode = UI_MAP;
                    }
                }
            }
        }

        C3D_FrameBegin(C3D_FRAME_SYNCDRAW);

        // v2 busy-modal polish (handoff.md §4): the full-screen "Processing"
        // overlay is reserved for UI_MAP, where a busy step is genuinely
        // long (world-gen, end-turn) and there's no map to show instead. Any
        // other mode instead keeps rendering its last cached !busy snapshot
        // (no fresh reads happen here - see the `if (!busy)` block above)
        // with a small non-blocking corner indicator, so a Buy tap's brief
        // busy blip doesn't flash the panel away.
        bool full_overlay = busy && ui_mode == UI_MAP;
        bool show_busy_indicator = busy && ui_mode != UI_MAP;

        if (full_overlay) {
            // See fc3ds_server_is_ready() vs is_busy(): not-yet-ready means
            // this is the very first (map-gen) step. save_in_flight (set on
            // the SAV tap, main.c) distinguishes an on-demand save's busy
            // step from an ordinary end-turn step - both are equally
            // "busy", but "Processing turn..." would be a misleading label
            // for a save.
            ProcessingReason reason = !fc3ds_server_is_ready() ? PROC_LOADING
                                     : save_in_flight            ? PROC_SAVING
                                                                  : PROC_TURN;

            C2D_TargetClear(top, C2D_Color32(0x14, 0x1C, 0x28, 0xFF));
            C2D_SceneBegin(top);
            C2D_SceneSize(GSP_SCREEN_WIDTH, GSP_SCREEN_HEIGHT_TOP, true);
            render_processing_overlay(SCREEN_WIDTH_TOP, SCREEN_HEIGHT_TOP, reason);

            C2D_TargetClear(bottom, C2D_Color32(0x14, 0x1C, 0x28, 0xFF));
            C2D_SceneBegin(bottom);
            C2D_SceneSize(GSP_SCREEN_WIDTH, GSP_SCREEN_HEIGHT_BOTTOM, true);
            render_processing_overlay(SCREEN_WIDTH_BOT, SCREEN_HEIGHT_BOT, reason);
        } else {
            C2D_TargetClear(top, C2D_Color32(0x18, 0x24, 0x38, 0xFF)); // Dark navy HUD
            C2D_SceneBegin(top);
            // Force the tilted landscape projection. C2D_SceneSize swaps w/h
            // when tilt=true and expects the PORTRAIT framebuffer dims
            // (240x400 top, 240x320 bottom) to hit its cached, correct
            // projection; passing the landscape dims scales/clips the scene
            // on hardware.
            C2D_SceneSize(GSP_SCREEN_WIDTH, GSP_SCREEN_HEIGHT_TOP, true);
            switch (ui_mode) {
            case UI_CITY:
                render_city_top(&ui_city_info, ui_garrison_sel);
                break;
            case UI_RESEARCH:
                render_civ_top(&ui_civ_info, "Research");
                break;
            case UI_RATES:
                render_civ_top(&ui_civ_info, "Tax & Science");
                break;
            case UI_GOVERNMENT:
                render_civ_top(&ui_civ_info, "Government");
                break;
            case UI_DIPLOMACY:
                render_diplomacy_top(ui_dipl_stage, ui_dipl_counterpart_name, &ui_diplo_info);
                break;
            case UI_MAP:
            default:
                render_top_screen(have_sel ? &sel_info : NULL, sel_x, sel_y, selected_unit_id,
                                  last_touch_px, last_touch_py, ui_goto_armed, cam_x, cam_y);
                break;
            }
            if (show_busy_indicator) render_busy_indicator(SCREEN_WIDTH_TOP);

            C2D_TargetClear(bottom, C2D_Color32(0x10, 0x10, 0x10, 0xFF)); // Dark gray
            C2D_SceneBegin(bottom);
            C2D_SceneSize(GSP_SCREEN_WIDTH, GSP_SCREEN_HEIGHT_BOTTOM, true);
            switch (ui_mode) {
            case UI_CITY:
                render_city_panel(&ui_city_info, ui_build_opts, ui_build_count, ui_scroll,
                                  ui_worklist, ui_worklist_count, ui_worklist_queue_mode);
                break;
            case UI_RESEARCH:
                if (ui_tech_tree_view) {
                    render_tech_tree_panel(ui_tech_tree, ui_tech_tree_count, ui_scroll);
                } else {
                    render_research_panel(ui_tech_opts, ui_tech_count, ui_scroll);
                }
                break;
            case UI_RATES:
                render_rates_panel(&ui_civ_info);
                break;
            case UI_GOVERNMENT:
                render_government_panel(ui_gov_opts, ui_gov_count, ui_scroll, ui_civ_info.government_id);
                break;
            case UI_DIPLOMACY:
                render_diplomacy_panel(ui_dipl_stage, ui_dipl_counterpart_name,
                                       ui_other_players, ui_other_player_count, ui_scroll,
                                       &ui_diplo_info);
                break;
            case UI_MAP:
            default:
                render_bottom_screen(cam_x, cam_y, sel_x, sel_y,
                                     last_touch_px, last_touch_py, ui_goto_x, ui_goto_y);
                render_action_bar();
                if (save_toast_frames > 0) render_toast("Saved");
                break;
            }
            if (show_busy_indicator) render_busy_indicator(SCREEN_WIDTH_BOT);
        }

        C3D_FrameEnd(0);
    }

    // Exit path.
    //
    // fc3ds_server_shutdown() marks the game over, drives the (possibly
    // parked) server coroutine to consume it, and joins the thread
    // (bounded) so NO background thread is holding FS/SOC sessions or
    // touching romfs while we tear libctru down. __wrap_exit() turns the
    // server thread's own exit() into threadExit(). Then the normal return
    // does a clean libctru shutdown (proper APT close handshake), which
    // returns to the Homebrew Launcher without a crash or hang.
    fc3ds_server_shutdown();

    render_deinit();

    if (g_spriteSheet) {
        C2D_SpriteSheetFree(g_spriteSheet);
    }
    C2D_Fini();
    C3D_Fini();
    gfxExit();
    return 0;
}
