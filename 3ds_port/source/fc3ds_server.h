#ifndef FC3DS_SERVER_H
#define FC3DS_SERVER_H

#include <3ds.h>

// Cooperative-scheduling controller for the in-process Freeciv server.
//
// The server runs on its own OS thread (a "coroutine": it keeps its own C
// stack across yields), but that thread is strictly gated - it only
// executes between fc3ds_server_pump() signalling it to go and the next
// time it parks. Exactly one of {main thread, server coroutine} is
// runnable at any instant. See handoff.md for the full design.
//
// Freeciv game state (struct unit/struct tile/wld.map/...) must only be
// read or mutated while fc3ds_server_is_busy() is false.

// ---- lifecycle ----

// Create the server thread. It initializes nothing Freeciv-specific yet -
// it blocks immediately, waiting for the main thread to call one of the two
// "begin" functions below (handoff-nohw.md §A.3's start-menu gate). Once
// released, it runs setup (net/ruleset-or-load/map-gen) to completion
// asynchronously, with no further gating, then parks at the first
// interactive turn boundary.
void fc3ds_server_start(bool is_new_3ds);

// ---- start choice (New Game / Load Game) ----
//
// Exactly one of these must be called, exactly once, to release the server
// thread past its initial block - normally from a start-menu screen the
// main thread shows before either call (see main.c's APP_START_MENU /
// APP_LOAD_PICKER states). Safe to call before the server thread has
// necessarily reached its wait point: the release is a sticky signal, not a
// rendezvous.

// Begin a fresh game: same setup Freeciv always did (ruleset load + map
// generation). dev selects romfs:/dev.serv (tiny map, 1 AI, tech pre-granted
// for a non-Anarchy government - see handoff.md) instead of the normal
// romfs:/start.serv, for fast on-device testing of features that are slow
// to reach on the default game (AI contact, non-Anarchy revolution).
void fc3ds_server_begin_new_game(bool dev);
// Same, but the server runs script_path (a .serv file written by the New
// Game setup screen) instead of romfs:/start.serv.
void fc3ds_server_begin_new_game_script(const char *script_path);

// Begin by loading an existing save instead of generating a new game.
// filename is a bare name (no path component - matches save_game()'s own
// contract), resolved against the saves-dir cwd init_freeciv() already
// chdir'd into. Copied internally; the caller's buffer need not outlive the
// call. Internally sets srvarg.load_filename so srv_prepare()
// (freeciv/server/srv_main.c) takes its existing "load instead of
// pregame-generate" branch - no other server-side code path changes.
void fc3ds_server_begin_load_game(const char *filename);

// True once the server has parked for the first time in S_S_RUNNING - i.e.
// the map is generated and starting units are placed. Drop any "loading"
// UI once this is true.
bool fc3ds_server_is_ready(void);

// Deterministic quit: mark the game over, drive the coroutine to consume
// it, and join the thread (bounded). Safe to call whether the server is
// currently parked or mid-step. Must be called before the main thread
// tears down libctru.
void fc3ds_server_shutdown(void);

// ---- stepping (call once per frame from the main loop) ----

// If work is queued and the server isn't paused or already busy, let it
// run one step. Never blocks.
void fc3ds_server_pump(void);

// True while the server is executing a step (not parked). The main loop
// must not touch Freeciv game state while this is true - show a
// "Processing..." indicator instead of rendering the map.
bool fc3ds_server_is_busy(void);

// ---- work submission ----

// Ends the current phase/turn (equivalent of the old freeciv_end_turn()).
void fc3ds_server_request_end_turn(void);

// Marks that a step should run on the next pump - called by
// freeciv_request_unit_move()/freeciv_request_unit_action() in
// game_loop.c after they enqueue their request.
void fc3ds_server_mark_work_pending(void);

// ---- pause (HOME / APT) ----

// Forbid pump() from ticking the server. Called from the APT ONSUSPEND/
// ONSLEEP hook. Since ticking is the only way the server thread ever runs,
// once paused it is guaranteed to be parked (no FS/SOC call in flight) and
// the OS may freeze the process safely.
void fc3ds_server_pause(void);

// Re-allow ticking. Called from the APT ONRESTORE/ONWAKEUP hook.
void fc3ds_server_resume(void);

// True from the point fc3ds_server_shutdown() is called until the server
// thread has actually exited. Queried by main.c's __wrap_exit() so the
// server thread's exit() can be turned into a plain threadExit() instead
// of tearing down the whole process.
bool fc3ds_server_is_quitting(void);

// Wait up to ms for a just-released step to park (short steps finish fast).
void fc3ds_server_wait_parked(int ms);
// True on the engine's own thread (used by __wrap_exit/__wrap_abort).
bool fc3ds_server_on_server_thread(void);
// The engine hit a fatal error (bad save, abort): it will never park again.
void fc3ds_server_mark_failed(void);
bool fc3ds_server_failed(void);
// The game has ended (victory, defeat, spacerace, end turn): the engine is
// parked in S_S_OVER with the final state still readable.
bool fc3ds_server_game_ended(void);
// Called by the patched srv_main() right after srv_scores().
void fc3ds_server_game_over_park(void);

#endif // FC3DS_SERVER_H
