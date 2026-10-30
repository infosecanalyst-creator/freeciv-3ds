// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 xPsycho999 (original Freeciv 3DS port)
// Copyright (C) 2026 the freeciv-3ds authors
#include "fc3ds_server.h"

#include <3ds.h>
#include <stdio.h>

#include "fc_config.h"
#include "fc_types.h"
#include "srv_main.h"
#include "map.h"
#include <string.h>

// `go` is signalled by the main thread to release the server for one step.
// `parked` is signalled by the server, sticky, when it re-parks, and is the
// SOLE authority for fc3ds_server_is_busy() - see that function. A bare
// volatile bool polled cross-thread would be a real data race the moment
// the server thread lands on a different physical core than main (New3DS
// has cores to spare, and threadCreate here uses affinity -1 == "OS's
// choice" - this is not merely a hypothetical future risk). LightEvent's
// Signal/Wait/TryWait carry the kernel-arbitration barriers that a plain
// volatile does not, so routing every busy/parked observation through this
// event (never through a separately-polled flag) closes that hole for free.
static LightEvent s_ev_go;
static LightEvent s_ev_parked;
static Thread     s_srv_thread;

static volatile bool s_paused       = false;
static volatile bool s_quitting     = false;
static volatile bool s_ready        = false;
static volatile bool s_work_pending = false;
static volatile bool s_failed       = false;
static volatile bool s_game_ended   = false;
static volatile bool s_step_in_flight = false; // pump released a step, not parked yet

// Start-choice gate (handoff-nohw.md §A.3): the server thread parks here,
// before touching anything Freeciv-specific, until the main thread calls
// fc3ds_server_begin_new_game()/_begin_load_game(). RESET_ONESHOT since it
// only ever fires once per app run - unlike s_ev_go, nothing needs to
// re-arm it.
static LightEvent s_ev_start_chosen;
static char s_pending_load_filename[512];
static bool s_pending_has_load = false;
static bool s_pending_is_dev = false;
static char s_pending_script[256];        // custom new-game script (setup screen), "" = default

static aptHookCookie s_apt_cookie;

// APT dispatches hooks on the main thread. fc3ds_server_pause() stops
// pump() from ever signalling `go` again, but that alone isn't enough: if
// suspend arrives while the server is mid-step (e.g. inside the FS call in
// save_game_auto(), srv_main.c) it's still mid-service-call the instant
// pause() returns and APT freezes the process - exactly the O1 condition
// this whole refactor exists to close. So if busy, block here (bounded)
// until the server actually reaches its next park before returning control
// to APT. Cheap in the common case: if already parked, LightEvent_WaitTimeout
// on a sticky-signalled event returns immediately.
static void fc3ds_apt_hook(APT_HookType hook, void *param)
{
    switch (hook) {
    case APTHOOK_ONSUSPEND:
    case APTHOOK_ONSLEEP:
        fc3ds_server_pause();
        // Only a step that is actually running needs waiting for (on the
        // title / setup / load screens nothing has been released).
        if (s_step_in_flight && !s_failed && LightEvent_WaitTimeout(&s_ev_parked, 5ULL * 1000 * 1000 * 1000) != 0) {
            // Timed out with the server still mid-step - a stuck step is a
            // separate bug class; don't hang APT suspend forever over it.
            printf("fc3ds_server: APT suspend timed out waiting for park\n");
        }
        break;
    case APTHOOK_ONRESTORE:
    case APTHOOK_ONWAKEUP:
        fc3ds_server_resume();
        break;
    default:
        break;
    }
}

static void fc3ds_server_coro(void *arg)
{
    // Block until the main thread's start menu picks New Game or Load Game
    // - see fc3ds_server_begin_new_game()/_begin_load_game() below. Nothing
    // Freeciv-specific has run yet (srv_init() hasn't even been called), so
    // this costs nothing beyond the wait itself.
    LightEvent_Wait(&s_ev_start_chosen);
    if (s_quitting) return; // app closed before a game was picked

    srv_init(); // resets srvarg, including load_filename - must run first

    if (s_pending_has_load) {
        // Bare filename, same contract save_game() itself uses - resolved
        // against the saves-dir cwd (init_freeciv(), game_loop.c).
        strncpy(srvarg.load_filename, s_pending_load_filename,
                sizeof(srvarg.load_filename) - 1);
        srvarg.load_filename[sizeof(srvarg.load_filename) - 1] = '\0';
    }

    init_game_seed();

    // romfs/start.serv is fresh-game-only setup (create the Human player,
    // aitoggle it from its default AI control, start) - srv_main() re-runs
    // whatever script is configured unconditionally on every launch, load
    // included. Blindly reusing start.serv for a load re-applies
    // `aitoggle Human` to a player who is already human, flipping them BACK
    // to AI-controlled (confirmed via a hardware save pull: the reloaded
    // player0 gained the "ai" PLRF_AI flag that a pre-load save of the same
    // game didn't have - see handoff.md). load.serv is the load-only
    // equivalent: just `start`, which is still required (srv_main() won't
    // leave pregame without it) but is safe to re-run since start_game()
    // (server/srv_main.c) does nothing but flip force_end_of_sniff - no
    // player/map state is touched, loaded or not.
    srvarg.script_filename = s_pending_has_load ? "romfs:/load.serv"
                            : s_pending_is_dev   ? "romfs:/dev.serv"
                            : s_pending_script[0] ? s_pending_script
                            : "romfs:/start.serv";

    // Never returns: falls out via server_quit() -> exit(), which
    // __wrap_exit() (main.c) turns into threadExit() for this thread while
    // fc3ds_server_is_quitting() is true.
    srv_main();
}

void fc3ds_server_start(bool is_new_3ds)
{
    LightEvent_Init(&s_ev_go, RESET_ONESHOT);
    LightEvent_Init(&s_ev_parked, RESET_STICKY);
    LightEvent_Init(&s_ev_start_chosen, RESET_ONESHOT);

    s_paused = false;
    s_quitting = false;
    s_ready = false;
    s_work_pending = false;
    s_pending_has_load = false;
    s_pending_is_dev = false;
    s_pending_load_filename[0] = '\0';

    s32 priority = 0;
    svcGetThreadPriority(&priority, CUR_THREAD_HANDLE);
    int dest_core = is_new_3ds ? 2 : -1;
    s_srv_thread = threadCreate(fc3ds_server_coro, NULL, 512 * 1024, priority + 1, dest_core, false);
    if (!s_srv_thread && dest_core != -1)
        s_srv_thread = threadCreate(fc3ds_server_coro, NULL, 512 * 1024, priority + 1, -1, false);
    if (!s_srv_thread) {
        printf("fc3ds_server: could not create the engine thread\n");
        s_failed = true;
    }

    // Registered once; the pause/resume it drives is a no-op if a suspend
    // arrives before the server thread reaches its first park (fc3ds_server_
    // pump() just won't tick it, same as any other paused window).
    aptHook(&s_apt_cookie, fc3ds_apt_hook, NULL);
}

void fc3ds_server_begin_new_game(bool dev)
{
    s_pending_has_load = false;
    s_pending_is_dev = dev;
    LightEvent_Signal(&s_ev_start_chosen);
}

void fc3ds_server_begin_new_game_script(const char *script_path)
{
    s_pending_has_load = false;
    s_pending_is_dev = false;
    strncpy(s_pending_script, script_path, sizeof(s_pending_script) - 1);
    s_pending_script[sizeof(s_pending_script) - 1] = '\0';
    LightEvent_Signal(&s_ev_start_chosen);
}

void fc3ds_server_begin_load_game(const char *filename)
{
    strncpy(s_pending_load_filename, filename, sizeof(s_pending_load_filename) - 1);
    s_pending_load_filename[sizeof(s_pending_load_filename) - 1] = '\0';
    s_pending_has_load = true;
    LightEvent_Signal(&s_ev_start_chosen);
}

bool fc3ds_server_is_ready(void)
{
    return s_ready;
}

bool fc3ds_server_is_busy(void)
{
    // TryWait on a RESET_STICKY event just checks current state (doesn't
    // consume it) and carries the arbitration barrier plain volatile reads
    // lack - see the s_ev_parked comment up top. Not signalled == the
    // server hasn't reached the park point since the last time pump()
    // cleared it (i.e. still busy, or hasn't parked for the first time yet).
    return !LightEvent_TryWait(&s_ev_parked);
}

void fc3ds_server_mark_work_pending(void)
{
    s_work_pending = true;
}

void fc3ds_server_request_end_turn(void)
{
    // Guard matches the pre-refactor freeciv_end_turn(): no game yet, no
    // turn to end.
    if (!map_is_empty()) {
        force_end_of_sniff = TRUE;
        s_work_pending = true;
    }
}

void fc3ds_server_pump(void)
{
    if (s_paused || s_quitting || fc3ds_server_is_busy()) {
        return; // don't tick a paused, quitting, or already-running server
    }
    if (!s_work_pending) {
        return; // nothing to do - stay parked, zero CPU
    }
    s_work_pending = false;
    s_step_in_flight = true;
    // Consume the park signal before releasing the server - otherwise
    // is_busy() would keep reporting "parked" (stale) the instant the
    // server starts running again, since RESET_STICKY doesn't self-clear.
    LightEvent_Clear(&s_ev_parked);
    LightEvent_Signal(&s_ev_go);
}

void fc3ds_server_pause(void)
{
    s_paused = true;
}

void fc3ds_server_resume(void)
{
    s_paused = false;
}

bool fc3ds_server_is_quitting(void)
{
    return s_quitting;
}

void fc3ds_server_shutdown(void)
{
    s_quitting = true;
    srvarg.exit_on_end = TRUE;
    set_server_state(S_S_OVER);
    force_end_of_sniff = TRUE;

    // Unconditionally allow ticking so a suspend that arrived just before
    // quit doesn't deadlock the join below.
    s_paused = false;

    // If the server is currently parked (waiting on s_ev_go), wake it so it
    // can observe S_S_OVER and unwind to server_quit(). If it's already
    // busy (mid-step), it's already running free and will reach the same
    // outcome on its own - server_state() != S_S_RUNNING now, so the next
    // fc3ds_server_yield() call returns immediately instead of parking.
    if (!fc3ds_server_is_busy()) {
        LightEvent_Clear(&s_ev_parked);
        LightEvent_Signal(&s_ev_go);
    }
    LightEvent_Signal(&s_ev_start_chosen); // still on the title screen: let it return

    // A failed engine thread is parked in an endless sleep: don't wait on it.
    if (s_srv_thread && !s_failed)
        threadJoin(s_srv_thread, 5ULL * 1000 * 1000 * 1000);
    // s_quitting stays set: if the join timed out, the engine thread's later
    // exit() must still end only that thread (see __wrap_exit).
}

void fc3ds_server_wait_parked(int ms)
{
    LightEvent_WaitTimeout(&s_ev_parked, (u64)ms * 1000 * 1000);
}

bool fc3ds_server_on_server_thread(void)
{
    return s_srv_thread && threadGetCurrent() == s_srv_thread;
}

void fc3ds_server_mark_failed(void)
{
    s_failed = true;
}

bool fc3ds_server_failed(void)
{
    return s_failed;
}

bool fc3ds_server_game_ended(void)
{
    return s_game_ended;
}

void fc3ds_server_game_over_park(void)
{
    if (s_quitting) return;
    s_game_ended = true;
    // Park for good with the final state intact, so the UI can show the
    // result; any further steps the UI releases just park again.
    while (!s_quitting) {
        s_step_in_flight = false;
        LightEvent_Signal(&s_ev_parked);
        LightEvent_Wait(&s_ev_go);
    }
}

// Called from freeciv/server/sernet.c's server_sniff_all_input(), on the
// server thread, once per sniff loop iteration (see
// port-patches/freeciv-core.patch). This is the ONE yield/park point: it
// only blocks during the interactive S_S_RUNNING turn loop, so setup
// (srv_prepare, pregame, srv_ready's map generation) and the postgame/quit
// tail run through untouched, exactly as they did in the free-running
// model.
void fc3ds_server_yield(void)
{
    if (server_state() != S_S_RUNNING) {
        return;
    }

    if (!s_ready) {
        s_ready = true; // first RUNNING park == map generated, units placed
    }
    // Publishes this step's game-state writes to whichever core observes
    // fc3ds_server_is_busy() next - see the s_ev_parked comment up top.
    s_step_in_flight = false;
    LightEvent_Signal(&s_ev_parked);

    LightEvent_Wait(&s_ev_go); // PARK - zero CPU until the main loop ticks us
}
