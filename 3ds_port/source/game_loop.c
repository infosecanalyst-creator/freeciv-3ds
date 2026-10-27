#include "game_loop.h"
#include "fc3ds_server.h"
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <malloc.h>
#include <3ds.h>

#include "fc_config.h"
#include "fc_types.h"
#include "srv_main.h"
#include "game.h"
#include "map.h"
#include "terrain.h"
#include "tile.h"
#include "unit.h"
#include "unittype.h"
#include "city.h"
#include "player.h"
#include "stdinhand.h"
#include "extras.h"
#include "unithand.h"
#include "unittools.h"
#include "actions.h"
#include "citytools.h"
#include "shared.h"
#include "research.h"
#include "tech.h"
#include "government.h"
#include "improvement.h"
#include "requirements.h"
#include "effects.h"
#include "hand_gen.h"
#include "diptreaty.h"
#include "savemain.h"
#include "pf_tools.h"
#include "movement.h"
#include "combat.h"
#include "events.h"
#include "specialist.h"
#include "spacerace.h"
#include "spaceship.h"
#include "string_vector.h"
#include <string.h>
#include <stdlib.h>
#include <dirent.h>
#include <sys/stat.h>

// POSIX Stubs missing from devkitARM for Freeciv
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>
uid_t getuid(void) { return 1000; }
uid_t geteuid(void) { return 1000; }
char *basename(char *path) {
    char *base = strrchr(path, '/');
    return base ? base + 1 : path;
}
clock_t clock(void) {
    return (clock_t)(osGetTime() / 10);
}

#define SOC_ALIGN      0x1000
#define SOC_BUFFERSIZE 0x100000
static u32 *soc_buffer = NULL;

void init_freeciv(bool is_new_3ds)
{
    // Initialize the SOC service so the Freeciv server's socket calls
    // (server_open_socket, and the fc_select() in its main loop) work.
    // Without this the server loop spins on an invalid socket and never
    // advances turns.
    soc_buffer = (u32 *)memalign(SOC_ALIGN, SOC_BUFFERSIZE);
    if (soc_buffer != NULL) {
        Result rc = socInit(soc_buffer, SOC_BUFFERSIZE);
        if (R_FAILED(rc)) {
            printf("socInit failed: 0x%08lX\n", rc);
        }
    } else {
        printf("Failed to allocate SOC buffer\n");
    }

    // Set the data path to our ROMFS data directory
    setenv("FREECIV_DATA_PATH", "romfs:/data", 1);
    setenv("HOME", "sdmc:/", 1);

    // Freeciv's "savename" setting is a bare filename (no path component
    // allowed - see is_safe_filename()), so save_game() writes relative to
    // the process cwd. HBL launches us with cwd = the .3dsx's own directory,
    // which would otherwise scatter freeciv-T*.sav.gz next to unrelated
    // homebrew. Redirect into a dedicated folder instead.
    mkdir("sdmc:/3ds", 0777);
    mkdir("sdmc:/3ds/freeciv", 0777);
    mkdir("sdmc:/3ds/freeciv/saves", 0777);
    chdir("sdmc:/3ds/freeciv/saves");

    // Spawn the (cooperatively-gated) Freeciv server coroutine thread -
    // see fc3ds_server.c/.h.
    fc3ds_server_start(is_new_3ds);
}

void step_freeciv(u32 kDown, u32 kHeld)
{
    // Handle inputs (to be implemented)
}

void deinit_freeciv(void)
{
    // Release the SOC service and its buffer on exit so the app can return
    // cleanly to the Homebrew Launcher / HOME menu.
    socExit();
    if (soc_buffer != NULL) {
        free(soc_buffer);
        soc_buffer = NULL;
    }
}

void get_map_dimensions(int *w, int *h)
{
    if (map_is_empty()) {
        *w = 40;
        *h = 40;
    } else {
        *w = wld.map.xsize;
        *h = wld.map.ysize;
    }
}

bool game_map_is_empty(void)
{
    return map_is_empty();
}

// --- Native-coordinate tile access -----------------------------------------
// The classic ruleset generates an ISO|HEX map. Its *map* coordinates form a
// sheared diamond that does NOT fit the port's square 0..xsize x 0..ysize
// render grid: a native tile (nat_x, nat_y) can have a map_x well past xsize
// (e.g. a unit at native (40,15) on a 48-wide map has map coords (48,15) via
// NATIVE_TO_MAP_POS), which the old map_pos_to_tile / index_to_map_pos path
// pushed clean off the drawable *and* selectable area - the root cause of the
// "SELECT finds nothing / units at the map edge" report (handoff.md re-diag).
// Native coordinates are always exactly the xsize x ysize rectangle the square
// renderer wants, so the whole port now operates in native space: tile lookups
// go through port_native_tile(), unit/goto positions through
// index_to_native_pos (not index_to_map_pos).
bool map_wraps_x(void)
{
    return !map_is_empty() && current_wrap_has_flag(WRAP_X);
}

bool map_wraps_y(void)
{
    return !map_is_empty() && current_wrap_has_flag(WRAP_Y);
}

// Resolve a native (nx, ny): wrap on any axis the topology wraps, reject
// out-of-range coordinates on a non-wrapping axis. One place owns every
// seam/edge decision so terrain, entities, neighbour matching, touch and goto
// all agree on what tile a coordinate names.
static struct tile *port_native_tile(int nx, int ny)
{
    if (map_is_empty()) {
        return NULL;
    }
    int xs = wld.map.xsize, ys = wld.map.ysize;
    if (current_wrap_has_flag(WRAP_X)) {
        nx = ((nx % xs) + xs) % xs;
    } else if (nx < 0 || nx >= xs) {
        return NULL;
    }
    if (current_wrap_has_flag(WRAP_Y)) {
        ny = ((ny % ys) + ys) % ys;
    } else if (ny < 0 || ny >= ys) {
        return NULL;
    }
    return native_pos_to_tile(&(wld.map), nx, ny);
}

TerrainType get_tile_type(int x, int y)
{
    // Check if the game has started and the map is initialized
    if (map_is_empty()) {
        return TERRAIN_OCEAN;
    }

    // Native-coordinate lookup (wraps/edge-rejects per topology).
    struct tile *ptile = port_native_tile(x, y);
    if (!ptile || !ptile->terrain) {
        return TERRAIN_OCEAN;
    }
    
    // Retrieve the terrain rule name and map it to our UI's TerrainType
    const char *terrain_name = terrain_rule_name(ptile->terrain);
    
    if (!terrain_name) {
        return TERRAIN_OCEAN;
    }
    
    if (strcasecmp(terrain_name, "Plains") == 0) return TERRAIN_PLAINS;
    if (strcasecmp(terrain_name, "Grassland") == 0) return TERRAIN_GRASSLAND;
    if (strcasecmp(terrain_name, "Forest") == 0) return TERRAIN_FOREST;
    if (strcasecmp(terrain_name, "Hills") == 0) return TERRAIN_HILLS;
    if (strcasecmp(terrain_name, "Mountains") == 0) return TERRAIN_MOUNTAINS;
    if (strcasecmp(terrain_name, "Tundra") == 0) return TERRAIN_TUNDRA;
    if (strcasecmp(terrain_name, "Arctic") == 0) return TERRAIN_ARCTIC;
    if (strcasecmp(terrain_name, "Glacier") == 0) return TERRAIN_ARCTIC;
    if (strcasecmp(terrain_name, "Inaccessible") == 0) return TERRAIN_MOUNTAINS;
    if (strcasecmp(terrain_name, "Swamp") == 0) return TERRAIN_SWAMP;
    if (strcasecmp(terrain_name, "Jungle") == 0) return TERRAIN_JUNGLE;
    if (strcasecmp(terrain_name, "Desert") == 0) return TERRAIN_DESERT;

    return TERRAIN_OCEAN;
}

int get_game_turn(void)
{
    if (map_is_empty()) {
        return 0;
    }
    return game.info.turn;
}

bool find_pov_unit_tile(int *x, int *y)
{
    if (map_is_empty()) {
        return false;
    }
    struct player *pov = get_pov_player();
    if (!pov) {
        return false;
    }
    // Read the pov player's OWN unit list directly rather than scanning the
    // map for a tile whose first-listed unit happens to be pov's. The old
    // map-scan returned false whenever pov's units were all stacked under
    // another player's unit (first-in-list != pov), or simply landed on a
    // scan-order edge case - and on that false, main.c's one-shot
    // auto-center never latched, leaving the camera at (0,0) staring at an
    // unexplored (black) corner. A player's own units are always visible to
    // it, so centering on unit[0] can never land in our own fog.
    if (unit_list_size(pov->units) == 0) {
        return false; // no units placed yet this frame - caller retries
    }
    struct unit *punit = unit_list_get(pov->units, 0);
    struct tile *ptile = punit ? unit_tile(punit) : NULL;
    if (!ptile) {
        return false;
    }
    index_to_native_pos(x, y, tile_index(ptile));
    return true;
}

bool find_unit_tile_by_id(int unit_id, int *x, int *y)
{
    if (map_is_empty() || unit_id < 0) {
        return false;
    }
    struct unit *punit = game_unit_by_number(unit_id);
    if (!punit) {
        return false;
    }
    struct tile *ptile = unit_tile(punit);
    if (!ptile) {
        return false;
    }
    index_to_native_pos(x, y, tile_index(ptile));
    return true;
}

bool get_unit_goto_tile(int unit_id, int *x, int *y)
{
    if (map_is_empty() || unit_id < 0) {
        return false;
    }
    struct unit *punit = game_unit_by_number(unit_id);
    // unit_has_orders() (not just a goto_tile != NULL check) is the real
    // "is there an active route" signal - goto_tile can be left non-NULL
    // (and, confirmed via a hardware save pull, pointing at the unit's OWN
    // current tile) with no active orders at all. Root cause not pinned
    // down (plausibly a residual from the AI-to-human unit handoff
    // start.serv does before turn 1 - see handoff.md), but this is the
    // first feature to ever read/render goto_tile, so any such pre-existing
    // stale value is now visible as a incorrect target marker where it
    // never was before.
    if (!punit || !unit_has_orders(punit) || !punit->goto_tile) {
        return false;
    }
    index_to_native_pos(x, y, tile_index(punit->goto_tile));
    return true;
}

// Pending-command ring: filled from the main (render/input) thread and
// drained from the server thread inside freeciv3ds_process_pending_actions(),
// which server/sernet.c calls once per server_sniff_all_input() loop
// iteration (see port-patches/freeciv-core.patch). Freeciv game state
// (struct unit/struct city/struct player/...) must only ever be mutated on
// the server thread, so the request functions themselves do nothing but
// record intent.
//
// A ring (not single slots) so rapid input - several taps/presses queued
// within the same busy step - is never dropped by one request overwriting
// another still-unconsumed one.
typedef enum {
    CMD_UNIT_MOVE,        // a=unit_id, b=direction8
    CMD_UNIT_GOTO,        // a=unit_id, b=dest tile index (tile_index())
    CMD_UNIT_ACTION,      // a=unit_id, b=UnitActionKind
    CMD_CITY_PRODUCTION,  // a=city_id, b=universals_n kind, c=value
    CMD_CITY_BUY,         // a=city_id
    CMD_SET_RESEARCH,     // a=tech id
    CMD_SET_GOAL,         // a=tech id (research goal: the path is followed automatically)
    CMD_SET_RATES,        // a=tax, b=lux, c=sci
    CMD_SET_GOVERNMENT,   // a=government id (Government_type_id)
    CMD_CITY_WORKLIST_APPEND,  // a=city_id, b=universals_n kind, c=value
    CMD_CITY_WORKLIST_REMOVE,  // a=city_id, b=index
    CMD_DIPLOMACY_INIT,          // a=counterpart player id
    CMD_DIPLOMACY_CANCEL,        // a=counterpart player id
    CMD_DIPLOMACY_ADD_CLAUSE,    // a=counterpart, b=clause_type, c=value
    CMD_DIPLOMACY_REMOVE_CLAUSE, // a=counterpart, b=clause_type, c=value
    CMD_DIPLOMACY_ACCEPT,        // a=counterpart player id
    CMD_SAVE_GAME,               // no payload; auto-named "3ds-T<turn>"
    CMD_UNIT_CITY_ACTION,        // a=unit_id, b=action id, c=city tile index
    CMD_CITY_TOGGLE_TILE,        // a=city_id, b=tile index
    CMD_CITY_CHANGE_SPECIALIST,  // a=city_id, b=from, c=to
    CMD_CITY_SELL,               // a=city_id, b=improvement id
    CMD_SPACESHIP_LAUNCH
} PendingCmdKind;

typedef struct {
    PendingCmdKind kind;
    int a, b, c, d;
} PendingCmd;

#define CMD_RING_SIZE 8
static PendingCmd s_cmd_ring[CMD_RING_SIZE];
// head: next slot the producer (main thread) will write.
// tail: next slot the consumer (server thread) will read.
// This runs only at the parked yield point with the main thread waiting on
// the next pump (see fc3ds_server.h), so plain volatile suffices on the
// current single-core config - no other producer/consumer overlap is
// possible. Revisit with a memory barrier if core-affinity ever changes that.
static int s_cmd_head = 0; // accessed with __atomic_* (acquire/release)
static int s_cmd_tail = 0;

static void enqueue_cmd(PendingCmdKind kind, int a, int b, int c, int d)
{
    int head = __atomic_load_n(&s_cmd_head, __ATOMIC_RELAXED);
    int next = (head + 1) % CMD_RING_SIZE;
    if (next == __atomic_load_n(&s_cmd_tail, __ATOMIC_ACQUIRE)) {
        // Ring full - drop. With one command queued per button press/tap and
        // CMD_RING_SIZE=8, this is not expected to trigger in practice.
        return;
    }
    s_cmd_ring[head] = (PendingCmd){ .kind = kind, .a = a, .b = b, .c = c, .d = d };
    // The engine runs on another core (New 3DS): publish the payload before
    // the new head becomes visible.
    __atomic_store_n(&s_cmd_head, next, __ATOMIC_RELEASE);
    fc3ds_server_mark_work_pending();
}

static enum direction8 move_dir_to_direction8(MoveDir dir)
{
    switch (dir) {
    case MOVE_DIR_NORTH:     return DIR8_NORTH;
    case MOVE_DIR_NORTHEAST: return DIR8_NORTHEAST;
    case MOVE_DIR_EAST:      return DIR8_EAST;
    case MOVE_DIR_SOUTHEAST: return DIR8_SOUTHEAST;
    case MOVE_DIR_SOUTH:     return DIR8_SOUTH;
    case MOVE_DIR_SOUTHWEST: return DIR8_SOUTHWEST;
    case MOVE_DIR_WEST:      return DIR8_WEST;
    case MOVE_DIR_NORTHWEST: return DIR8_NORTHWEST;
    }
    return DIR8_NORTH;
}

void freeciv_request_unit_move(int unit_id, MoveDir dir)
{
    enqueue_cmd(CMD_UNIT_MOVE, unit_id, (int)move_dir_to_direction8(dir), 0, 0);
}

void freeciv_request_unit_goto(int unit_id, int tile_x, int tile_y)
{
    if (map_is_empty()) {
        return;
    }
    // tile_index() is stable map topology, not per-turn game state - safe to
    // read from the main thread the same way get_tile_info()'s x/y ->
    // port_native_tile() lookup already is. tile_x/tile_y are native (the
    // whole port operates in native space); tile_index() is coordinate-system
    // agnostic, so the server-side drain handler needs no change. The unit/path
    // validity checks that depend on live game state happen there, never here.
    struct tile *dst = port_native_tile(tile_x, tile_y);
    if (!dst) {
        return;
    }
    enqueue_cmd(CMD_UNIT_GOTO, unit_id, tile_index(dst), 0, 0);
}

void freeciv_request_unit_action(int unit_id, UnitActionKind kind)
{
    enqueue_cmd(CMD_UNIT_ACTION, unit_id, (int)kind, 0, 0);
}

void freeciv_request_city_production(int city_id, int kind, int value)
{
    enqueue_cmd(CMD_CITY_PRODUCTION, city_id, kind, value, 0);
}

void freeciv_request_city_buy(int city_id)
{
    enqueue_cmd(CMD_CITY_BUY, city_id, 0, 0, 0);
}

void freeciv_request_set_research(int tech)
{
    enqueue_cmd(CMD_SET_RESEARCH, tech, 0, 0, 0);
}

void freeciv_request_set_goal(int tech)
{
    enqueue_cmd(CMD_SET_GOAL, tech, 0, 0, 0);
}

void freeciv_request_set_rates(int tax, int lux, int sci)
{
    enqueue_cmd(CMD_SET_RATES, tax, lux, sci, 0);
}

void freeciv_request_set_government(int government)
{
    enqueue_cmd(CMD_SET_GOVERNMENT, government, 0, 0, 0);
}

void freeciv_request_city_worklist_append(int city_id, int kind, int value)
{
    enqueue_cmd(CMD_CITY_WORKLIST_APPEND, city_id, kind, value, 0);
}

void freeciv_request_city_worklist_remove(int city_id, int index)
{
    enqueue_cmd(CMD_CITY_WORKLIST_REMOVE, city_id, index, 0, 0);
}

void freeciv_request_diplomacy_init(int counterpart)
{
    enqueue_cmd(CMD_DIPLOMACY_INIT, counterpart, 0, 0, 0);
}

void freeciv_request_diplomacy_cancel(int counterpart)
{
    enqueue_cmd(CMD_DIPLOMACY_CANCEL, counterpart, 0, 0, 0);
}

void freeciv_request_diplomacy_add_clause(int counterpart, int clause_type, int value)
{
    enqueue_cmd(CMD_DIPLOMACY_ADD_CLAUSE, counterpart, clause_type, value, 0);
}

void freeciv_request_diplomacy_remove_clause(int counterpart, int clause_type, int value)
{
    enqueue_cmd(CMD_DIPLOMACY_REMOVE_CLAUSE, counterpart, clause_type, value, 0);
}

void freeciv_request_diplomacy_accept(int counterpart)
{
    enqueue_cmd(CMD_DIPLOMACY_ACCEPT, counterpart, 0, 0, 0);
}

void freeciv_request_city_action(int unit_id, int action, int tx, int ty)
{
    if (map_is_empty()) return;
    struct tile *t = port_native_tile(tx, ty);
    if (t) enqueue_cmd(CMD_UNIT_CITY_ACTION, unit_id, action, tile_index(t), 0);
}

void freeciv_request_city_toggle_tile(int city_id, int tx, int ty)
{
    if (map_is_empty()) return;
    struct tile *t = port_native_tile(tx, ty);
    if (t) enqueue_cmd(CMD_CITY_TOGGLE_TILE, city_id, tile_index(t), 0, 0);
}

void freeciv_request_city_change_specialist(int city_id, int from, int to)
{
    enqueue_cmd(CMD_CITY_CHANGE_SPECIALIST, city_id, from, to, 0);
}

void freeciv_request_city_sell(int city_id, int building_id)
{
    enqueue_cmd(CMD_CITY_SELL, city_id, building_id, 0, 0);
}

void freeciv_request_save_game(void)
{
    enqueue_cmd(CMD_SAVE_GAME, 0, 0, 0, 0);
}

// Try each action id in order (ruleset-variant slots, e.g. ACTION_ATTACK vs
// ACTION_ATTACK2, are alternates - not to be applied together) against a
// city target; performs and returns TRUE on the first one the ruleset's
// action-enabler system allows. Mirrors ai/default/daitools.c's
// dai_unit_attack() dispatch, which is the reference for doing this without
// a client connection.
static bool try_city_actions(struct player *pov, struct unit *punit, struct city *tcity,
                              const action_id *ids, int n)
{
    for (int i = 0; i < n; i++) {
        if (is_action_enabled_unit_on_city(&(wld.map), ids[i], punit, tcity)) {
            unit_perform_action(pov, punit->id, tcity->id, 0, "", ids[i], ACT_REQ_PLAYER);
            return true;
        }
    }
    return false;
}


// Resolve "move punit onto dst" into attack/conquer-city/capture if dst is
// occupied by someone else, falling back to a plain move otherwise. This is
// what gives moving into an enemy the standard Freeciv "bump to attack" UX
// - there is deliberately no separate attack button.
// Sound-cue ring: written on the server thread, read on the main thread
// while the server is parked (same discipline as the command ring).
#define SOUND_RING 8
static char s_sound_ring[SOUND_RING][32];
static volatile int s_sound_head, s_sound_tail;

static void push_sound(const char *tag)
{
    int next = (s_sound_head + 1) % SOUND_RING;
    if (next == s_sound_tail) return;
    fc_strlcpy(s_sound_ring[s_sound_head], tag, sizeof(s_sound_ring[0]));
    s_sound_head = next;
}

// "f_" / "m_" + the unit's ruleset name in soundspec form ("Mech. Inf." -> "mech_inf").
static void push_unit_sound(char kind, const struct unit *punit)
{
    char tag[32];
    const char *rn = unit_rule_name(punit);
    int j = 0;
    tag[j++] = kind; tag[j++] = '_';
    for (int i = 0; rn && rn[i] && j < (int)sizeof(tag) - 1; i++) {
        char c = rn[i];
        if (c >= 'A' && c <= 'Z') c += 32;
        if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')) tag[j++] = c;
        else if (tag[j - 1] != '_') tag[j++] = '_';
    }
    while (j > 2 && tag[j - 1] == '_') j--;
    tag[j] = 0;
    push_sound(tag);
}

// ---------------------------------------------------------------- news
static GameMessage s_msgs[MESSAGE_LOG_SIZE];
static volatile int s_msg_total;

static int message_category(enum event_type e)
{
    switch (e) {
    case E_UNIT_LOST_ATT: case E_UNIT_WIN_ATT: case E_UNIT_LOST_DEF: case E_UNIT_WIN_DEF:
    case E_UNIT_LOST_MISC: case E_CITY_LOST: case E_CITY_CONQUERED: case E_DESTROYED:
    case E_NUKE: case E_CIVIL_WAR: case E_UPRISING: case E_HUT_BARB: case E_HUT_BARB_CITY_NEAR:
    case E_CITY_NUKED: case E_UNIT_ESCAPED:
        return MSG_COMBAT;
    case E_TECH_GAIN: case E_TECH_LEARNED: case E_TECH_LOST: case E_TECH_EMBASSY: case E_TECH_GOAL:
        return MSG_SCIENCE;
    case E_TREATY_ALLIANCE: case E_TREATY_BROKEN: case E_TREATY_CEASEFIRE: case E_TREATY_PEACE:
    case E_TREATY_SHARED_VISION: case E_TREATY_EMBASSY: case E_TREATY_SHARED_TILES:
    case E_DIPLOMACY: case E_FIRST_CONTACT: case E_DIPLOMATIC_INCIDENT:
    case E_MY_DIPLOMAT_BRIBE: case E_MY_DIPLOMAT_ESCAPE: case E_MY_DIPLOMAT_EMBASSY: case E_MY_DIPLOMAT_FAILED:
    case E_MY_DIPLOMAT_INCITE: case E_MY_DIPLOMAT_POISON: case E_MY_DIPLOMAT_SABOTAGE: case E_MY_DIPLOMAT_THEFT:
    case E_ENEMY_DIPLOMAT_BRIBE: case E_ENEMY_DIPLOMAT_EMBASSY: case E_ENEMY_DIPLOMAT_FAILED:
    case E_ENEMY_DIPLOMAT_INCITE: case E_ENEMY_DIPLOMAT_POISON: case E_ENEMY_DIPLOMAT_SABOTAGE:
    case E_ENEMY_DIPLOMAT_THEFT:
        return MSG_DIPLOMACY;
    case E_HUT_GOLD: case E_HUT_TECH: case E_HUT_MERC: case E_HUT_SETTLER: case E_HUT_CITY: case E_HUT_MAP:
    case E_HUT_BARB_KILLED: case E_ACHIEVEMENT: case E_UNIT_BECAME_VET: case E_REVOLT_DONE:
    case E_NEW_GOVERNMENT: case E_WONDER_BUILD: case E_CITY_LOVE:
        return MSG_GOOD;
    case E_CITY_CANTBUILD: case E_CITY_DISORDER: case E_CITY_FAMINE: case E_CITY_FAMINE_FEARED:
    case E_CITY_GROWTH: case E_CITY_MAY_SOON_GROW: case E_CITY_AQUEDUCT: case E_CITY_AQ_BUILDING:
    case E_CITY_NORMAL: case E_CITY_TRANSFER: case E_CITY_BUILD: case E_CITY_PRODUCTION_CHANGED:
    case E_WORKLIST: case E_IMP_BUY: case E_IMP_BUILD: case E_IMP_AUCTIONED: case E_IMP_AUTO: case E_IMP_SOLD:
    case E_UNIT_BUY: case E_UNIT_BUILT: case E_UNIT_BUILT_POP_COST: case E_POLLUTION: case E_CITY_PLAGUE:
    case E_DISASTER: case E_WONDER_OBSOLETE: case E_WONDER_STARTED: case E_WONDER_STOPPED:
    case E_WONDER_WILL_BE_BUILT: case E_CITY_RADIUS_SQ: case E_LOW_ON_FUNDS:
        return MSG_CITY;
    default:
        return MSG_OTHER;
    }
}

static bool message_wanted(enum event_type e)
{
    switch (e) {
    case E_CHAT_MSG: case E_CHAT_ERROR: case E_MESSAGE_WALL: case E_CONNECTION: case E_AI_DEBUG:
    case E_LOG_ERROR: case E_LOG_FATAL: case E_SETTING: case E_VOTE_NEW: case E_VOTE_RESOLVED:
    case E_VOTE_ABORTED: case E_DEPRECATION_WARNING: case E_TURN_BELL: case E_NEXT_YEAR:
    case E_NATION_SELECTED: case E_BEGINNER_HELP: case E_SCRIPT: case E_GAME_START:
        return false;
    default:
        return true;
    }
}

// Called from server/notify.c (see port-patches) for every event addressed
// to a player; runs on the server thread.
#define PROD_PENDING_MAX 16
static int s_prod_pending[PROD_PENDING_MAX];
static int s_prod_n;

int get_pending_production(char *name, int len, int *x, int *y)
{
    struct player *pov = get_pov_player();
    while (s_prod_n > 0) {
        struct city *pc = game_city_by_number(s_prod_pending[0]);
        if (pov && pc && city_owner(pc) == pov) {
            fc_strlcpy(name, city_name_get(pc), len);
            index_to_native_pos(x, y, tile_index(city_tile(pc)));
            return pc->id;
        }
        memmove(s_prod_pending, s_prod_pending + 1, (--s_prod_n) * sizeof(int)); // city gone
    }
    return -1;
}

void dismiss_pending_production(int city_id)
{
    for (int i = 0; i < s_prod_n; i++) {
        if (s_prod_pending[i] == city_id) {
            memmove(s_prod_pending + i, s_prod_pending + i + 1, (s_prod_n - i - 1) * sizeof(int));
            s_prod_n--;
            return;
        }
    }
}

bool research_needs_choice(void)
{
    struct player *pov = get_pov_player();
    if (!pov) return false;
    const struct research *r = research_get(pov);
    return r && (r->researching == A_UNSET || r->researching == A_UNKNOWN);
}

void freeciv3ds_event_hook(const struct packet_chat_msg *packet, int pidx)
{
    struct player *pov = get_pov_player();
    if (!pov || !packet) return;
    if (pidx >= 0 && pidx != player_index(pov)) return;
    enum event_type e = packet->event;
    if (!message_wanted(e)) return;

    GameMessage *m = &s_msgs[s_msg_total % MESSAGE_LOG_SIZE];
    // Strip Freeciv's [c fg=...]...[/c] markup. Links are self-closing tags
    // whose visible text is their name attribute: [l tgt="city" id=5 name="Rome" /]
    int j = 0;
    for (const char *p = packet->message; *p && j < (int)sizeof(m->text) - 1; p++) {
        if (*p != '[') { m->text[j++] = *p; continue; }
        const char *end = strchr(p, ']');
        if (!end) break;
        if (p[1] == 'l' && p[2] == ' ') {
            const char *nm = strstr(p, "name=\"");
            if (nm && nm < end) {
                for (nm += 6; *nm && *nm != '"' && nm < end && j < (int)sizeof(m->text) - 1; nm++)
                    m->text[j++] = *nm;
            }
        }
        p = end;
    }
    m->text[j] = 0;
    if (!j) return;
    m->turn = game.info.turn;
    m->category = message_category(e);
    m->x = m->y = -1;
    if (packet->tile >= 0) {
        struct tile *t = index_to_tile(&(wld.map), packet->tile);
        if (t) index_to_native_pos(&m->x, &m->y, tile_index(t));
    }
    s_msg_total++;

    // A city finished something (or its plan ran out): queue it for the
    // smart End Turn button's "Choose production" step.
    // (A plain unit just repeats, which is what players expect; settlers
    // and other units that cost population do prompt.)
    if ((e == E_IMP_BUILD || e == E_UNIT_BUILT_POP_COST || e == E_WORKLIST
         || e == E_IMP_AUTO || e == E_CITY_CANTBUILD) && packet->tile >= 0) {
        struct tile *t = index_to_tile(&(wld.map), packet->tile);
        struct city *pc = t ? tile_city(t) : NULL;
        if (pc && city_owner(pc) == pov) {
            bool have = false;
            for (int i = 0; i < s_prod_n; i++) if (s_prod_pending[i] == pc->id) have = true;
            if (!have && s_prod_n < PROD_PENDING_MAX) s_prod_pending[s_prod_n++] = pc->id;
        }
    }

    // Hear it when you're attacked, and when a wonder is finished.
    if (e == E_UNIT_LOST_DEF || e == E_UNIT_WIN_DEF || e == E_CITY_LOST) push_sound("f_generic");
    else if (e == E_WONDER_BUILD) push_sound("w_generic");
}

int get_message_total(void) { return s_msg_total; }

// ---------------------------------------------------------------- civilopedia
static void cat(char *out, int len, const char *fmt, ...)
{
    int used = strlen(out);
    if (used >= len - 1) return;
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(out + used, len - used, fmt, ap);
    va_end(ap);
}

static void cat_help(char *out, int len, const struct strvec *help)
{
    if (!help) return;
    strvec_iterate(help, str) {
        cat(out, len, "\n%s\n", str);
    } strvec_iterate_end;
}

static void cat_reqs(char *out, int len, const struct requirement_vector *reqs, const char *label)
{
    bool first = true;
    requirement_vector_iterate(reqs, preq) {
        if (preq->source.kind == VUT_ADVANCE && preq->present) {
            cat(out, len, "%s%s", first ? label : ", ", advance_name_translation(preq->source.value.advance));
            first = false;
        }
    } requirement_vector_iterate_end;
    if (!first) cat(out, len, "\n");
}

int get_pedia_list(int kind, PediaEntry *out, int max)
{
    int n = 0;
    if (game.control.num_unit_types <= 0) return 0;   // no ruleset loaded yet
    switch (kind) {
    case PEDIA_UNITS:
        unit_type_iterate(ut) {
            if (n >= max) break;
            out[n].id = utype_number(ut);
            fc_strlcpy(out[n].name, utype_name_translation(ut), sizeof(out[n].name));
            n++;
        } unit_type_iterate_end;
        break;
    case PEDIA_BUILDINGS:
    case PEDIA_WONDERS:
        improvement_iterate(pi) {
            if (n >= max) break;
            if ((kind == PEDIA_WONDERS) != is_wonder(pi)) continue;
            out[n].id = improvement_number(pi);
            fc_strlcpy(out[n].name, improvement_name_translation(pi), sizeof(out[n].name));
            n++;
        } improvement_iterate_end;
        break;
    case PEDIA_TECHS:
        advance_iterate(pa) {
            if (n >= max) break;
            out[n].id = advance_number(pa);
            fc_strlcpy(out[n].name, advance_name_translation(pa), sizeof(out[n].name));
            n++;
        } advance_iterate_end;
        break;
    case PEDIA_GOVS:
        governments_iterate(pg) {
            if (n >= max) break;
            out[n].id = government_number(pg);
            fc_strlcpy(out[n].name, government_name_translation(pg), sizeof(out[n].name));
            n++;
        } governments_iterate_end;
        break;
    }
    // alphabetical
    for (int i = 1; i < n; i++) {
        PediaEntry t = out[i];
        int j = i - 1;
        while (j >= 0 && strcasecmp(out[j].name, t.name) > 0) { out[j + 1] = out[j]; j--; }
        out[j + 1] = t;
    }
    return n;
}

// The entry's picture: Freeciv graphic tag (+ fallback) for buildings,
// wonders, techs and governments; for units, the unit's rule name (the client
// already has unit sprites). Returns true for a unit.
bool get_pedia_graphic(int kind, int id, char *tag, int tlen, char *alt, int alen)
{
    tag[0] = alt[0] = 0;
    switch (kind) {
    case PEDIA_UNITS: {
        const struct unit_type *ut = utype_by_number(id);
        if (ut) fc_strlcpy(tag, utype_rule_name(ut), tlen);
        return true;
    }
    case PEDIA_BUILDINGS:
    case PEDIA_WONDERS: {
        const struct impr_type *pi = improvement_by_number(id);
        if (pi) { fc_strlcpy(tag, pi->graphic_str, tlen); fc_strlcpy(alt, pi->graphic_alt, alen); }
        break;
    }
    case PEDIA_TECHS: {
        const struct advance *pa = advance_by_number(id);
        if (pa) { fc_strlcpy(tag, pa->graphic_str, tlen); fc_strlcpy(alt, pa->graphic_alt, alen); }
        break;
    }
    case PEDIA_GOVS: {
        const struct government *pg = government_by_number(id);
        if (pg) { fc_strlcpy(tag, pg->graphic_str, tlen); fc_strlcpy(alt, pg->graphic_alt, alen); }
        break;
    }
    }
    return false;
}

void get_pedia_text(int kind, int id, char *out, int len)
{
    out[0] = 0;
    switch (kind) {
    case PEDIA_UNITS: {
        const struct unit_type *ut = utype_by_number(id);
        if (!ut) return;
        cat(out, len, "Cost %d   Attack %d   Defense %d   Moves %s\n", utype_build_shield_cost_base(ut),
            ut->attack_strength, ut->defense_strength, move_points_text(ut->move_rate, TRUE));
        cat(out, len, "Hit points %d   Firepower %d\n", ut->hp, ut->firepower);
        cat_reqs(out, len, &ut->build_reqs, "Needs: ");
        if (ut->obsoleted_by) cat(out, len, "Replaced by: %s\n", utype_name_translation(ut->obsoleted_by));
        cat_help(out, len, ut->helptext);
        break;
    }
    case PEDIA_BUILDINGS:
    case PEDIA_WONDERS: {
        const struct impr_type *pi = improvement_by_number(id);
        if (!pi) return;
        cat(out, len, "Cost %d   Upkeep %d gold\n", impr_base_build_shield_cost(pi), pi->upkeep);
        cat_reqs(out, len, &pi->reqs, "Needs: ");
        cat_help(out, len, pi->helptext);
        break;
    }
    case PEDIA_TECHS: {
        const struct advance *pa = advance_by_number(id);
        if (!pa) return;
        const struct advance *r1 = advance_requires(pa, AR_ONE), *r2 = advance_requires(pa, AR_TWO);
        if (r1 && advance_number(r1) != A_NONE) {
            cat(out, len, "Needs: %s", advance_name_translation(r1));
            if (r2 && advance_number(r2) != A_NONE) cat(out, len, ", %s", advance_name_translation(r2));
            cat(out, len, "\n");
        }
        // what it allows
        bool first = true;
        unit_type_iterate(ut) {
            requirement_vector_iterate(&ut->build_reqs, preq) {
                if (preq->source.kind == VUT_ADVANCE && preq->source.value.advance == pa && preq->present) {
                    cat(out, len, "%s%s", first ? "Allows: " : ", ", utype_name_translation(ut));
                    first = false;
                }
            } requirement_vector_iterate_end;
        } unit_type_iterate_end;
        improvement_iterate(pi) {
            requirement_vector_iterate(&pi->reqs, preq) {
                if (preq->source.kind == VUT_ADVANCE && preq->source.value.advance == pa && preq->present) {
                    cat(out, len, "%s%s", first ? "Allows: " : ", ", improvement_name_translation(pi));
                    first = false;
                }
            } requirement_vector_iterate_end;
        } improvement_iterate_end;
        governments_iterate(pg) {
            requirement_vector_iterate(&pg->reqs, preq) {
                if (preq->source.kind == VUT_ADVANCE && preq->source.value.advance == pa && preq->present) {
                    cat(out, len, "%s%s", first ? "Allows: " : ", ", government_name_translation(pg));
                    first = false;
                }
            } requirement_vector_iterate_end;
        } governments_iterate_end;
        if (!first) cat(out, len, "\n");
        // Techs it opens the way to (most classic techs have no help text, so
        // this is often the most useful line).
        first = true;
        advance_iterate(pt) {
            if (advance_requires(pt, AR_ONE) == pa || advance_requires(pt, AR_TWO) == pa) {
                cat(out, len, "%s%s", first ? "Leads to: " : ", ", advance_name_translation(pt));
                first = false;
            }
        } advance_iterate_end;
        if (!first) cat(out, len, "\n");
        {
            struct player *pov = get_pov_player();
            const struct research *rs = pov ? research_get(pov) : NULL;
            if (rs) {
                if (research_invention_state(rs, advance_number(pa)) == TECH_KNOWN)
                    cat(out, len, "You know this.\n");
                else
                    cat(out, len, "Research cost: %d bulbs\n",
                        research_total_bulbs_required(rs, advance_number(pa), FALSE));
            }
        }
        cat_help(out, len, pa->helptext);
        break;
    }
    case PEDIA_GOVS: {
        const struct government *pg = government_by_number(id);
        if (!pg) return;
        cat_reqs(out, len, &pg->reqs, "Needs: ");
        cat_help(out, len, pg->helptext);
        break;
    }
    }
}

void freeciv_request_spaceship_launch(void)
{
    enqueue_cmd(CMD_SPACESHIP_LAUNCH, 0, 0, 0, 0);
}

void get_spaceship_info(SpaceshipInfo *out)
{
    memset(out, 0, sizeof(*out));
    struct player *pov = get_pov_player();
    if (!pov) return;
    const struct player_spaceship *s = &pov->spaceship;
    out->state = s->state;
    out->structurals = s->structurals;
    out->components = s->components;
    out->modules = s->modules;
    out->population = s->population;
    out->success_pct = (int)(s->success_rate * 100.0 + 0.5);
    out->travel_tenths = (int)(s->travel_time * 10.0 + 0.5);
    out->launch_year = s->launch_year;
    out->can_launch = s->state == SSHIP_STARTED && s->success_rate > 0.0;
}

bool get_game_result(bool *won, int *score, int *turn)
{
    if (map_is_empty()) return false;
    struct player *pov = get_pov_player();
    if (!pov) return false;
    bool over = server_state() == S_S_OVER || !pov->is_alive;
    if (!over) return false;
    *won = pov->is_alive && pov->is_winner;
    *score = pov->score.game;
    *turn = game.info.turn;
    return true;
}

bool get_message(int index, GameMessage *out)
{
    int total = s_msg_total;
    if (index < 0 || index >= total || index < total - MESSAGE_LOG_SIZE) return false;
    *out = s_msgs[index % MESSAGE_LOG_SIZE];
    return true;
}

// ---------------------------------------------------------------- odds / routes
bool get_attack_odds(int unit_id, int tx, int ty, AttackOdds *out)
{
    memset(out, 0, sizeof(*out));
    if (map_is_empty()) return false;
    struct unit *punit = game_unit_by_number(unit_id);
    struct tile *ptile = port_native_tile(tx, ty);
    struct player *pov = get_pov_player();
    if (!punit || !ptile || !pov || !ptile->units || unit_list_size(ptile->units) == 0) return false;
    bool enemy = false;
    unit_list_iterate(ptile->units, u) {
        if (unit_owner(u) != pov) enemy = true;
    } unit_list_iterate_end;
    if (!enemy) return false;
    const struct civ_map *nmap = &(wld.map);
    struct action *att = action_by_number(ACTION_ATTACK);
    struct unit *def = get_defender(nmap, punit, ptile, att);
    if (!def) return false;
    fc_strlcpy(out->attacker, unit_name_translation(punit), sizeof(out->attacker));
    fc_strlcpy(out->defender, unit_name_translation(def), sizeof(out->defender));
    fc_strlcpy(out->defender_rule, unit_rule_name(def), sizeof(out->defender_rule));
    out->defender_owner = player_index(unit_owner(def));
    out->def_hp = def->hp; out->def_hp_max = unit_type_get(def)->hp;
    out->att_hp = punit->hp; out->att_hp_max = unit_type_get(punit)->hp;
    double w = unit_win_chance(nmap, punit, def, att);
    out->win_pct = (int)(w * 100.0 + 0.5);
    out->can_attack = is_action_enabled_unit_on_stack(nmap, ACTION_ATTACK, punit, ptile)
                   || is_action_enabled_unit_on_stack(nmap, ACTION_ATTACK2, punit, ptile);
    return true;
}

// Every tile the unit can still reach this turn (Advance Wars-style range),
// excluding its own. Positions come out cheapest first, so stop at turn 1.
int get_move_range(int unit_id, short *xs, short *ys, int max)
{
    if (map_is_empty()) return 0;
    struct unit *punit = game_unit_by_number(unit_id);
    if (!punit || punit->moves_left <= 0) return 0;
    struct pf_parameter parameter;
    pft_fill_unit_parameter(&parameter, &(wld.map), punit);
    struct pf_map *pfm = pf_map_new(&parameter);
    int n = 0;
    // Freeciv's pos.turn is total cost / move rate, so a step that uses the
    // last move point already reads as turn 1. Compare the cost to the
    // unit's moves left instead (cheapest first, so stop at the first miss).
    pf_map_positions_iterate(pfm, pos, FALSE) {
        if ((int)pos.total_MC > punit->moves_left || n >= max) break;
        int x, y;
        index_to_native_pos(&x, &y, tile_index(pos.tile));
        xs[n] = (short)x; ys[n] = (short)y;
        n++;
    } pf_map_positions_iterate_end;
    pf_map_destroy(pfm);
    return n;
}

void get_route_preview(int unit_id, int tx, int ty, RoutePreview *out)
{
    memset(out, 0, sizeof(*out));
    if (map_is_empty()) return;
    struct unit *punit = game_unit_by_number(unit_id);
    struct tile *dst = port_native_tile(tx, ty);
    if (!punit || !dst) return;
    struct pf_parameter parameter;
    pft_fill_unit_parameter(&parameter, &(wld.map), punit);
    struct pf_map *pfm = pf_map_new(&parameter);
    struct pf_path *path = pf_map_path(pfm, dst);
    if (path && path->length > 0) {
        out->reachable = true;
        // Turns beyond this one (0 = arrives this turn); pos.turn over-counts
        // by one when the move uses exactly the remaining move points.
        {
            int mc = (int)path->positions[path->length - 1].total_MC;
            int rate = unit_move_rate(punit);
            out->turns = mc <= punit->moves_left ? 0
                         : (rate > 0 ? 1 + (mc - punit->moves_left - 1) / rate : 99);
        }
        // Keep at most ROUTE_MAX points, always including the last.
        int step = (path->length + ROUTE_MAX - 1) / ROUTE_MAX;
        if (step < 1) step = 1;
        for (unsigned i = 0; i < path->length && out->n < ROUTE_MAX; i += step) {
            index_to_native_pos(&out->x[out->n], &out->y[out->n], tile_index(path->positions[i].tile));
            out->n++;
        }
        int lx, ly;
        index_to_native_pos(&lx, &ly, tile_index(path->positions[path->length - 1].tile));
        if (out->n > 0 && (out->x[out->n - 1] != lx || out->y[out->n - 1] != ly)) {
            if (out->n < ROUTE_MAX) out->n++;
            out->x[out->n - 1] = lx;
            out->y[out->n - 1] = ly;
        }
    }
    if (path) pf_path_destroy(path);
    pf_map_destroy(pfm);
}

static const struct { action_id act; const char *label; } k_city_actions[] = {
    { ACTION_HELP_WONDER,             "Help Wonder" },
    { ACTION_TRADE_ROUTE,             "Trade Route" },
    { ACTION_MARKETPLACE,             "Sell Goods" },
    { ACTION_DISBAND_UNIT_RECOVER,    "Disband (shields)" },
    { ACTION_UPGRADE_UNIT,            "Upgrade" },
    { ACTION_HOME_CITY,               "Make Home City" },
    { ACTION_ESTABLISH_EMBASSY_STAY,  "Embassy" },
    { ACTION_ESTABLISH_EMBASSY,       "Embassy (stay)" },
    { ACTION_SPY_INVESTIGATE_CITY,    "Investigate" },
    { ACTION_INV_CITY_SPEND,          "Investigate" },
    { ACTION_SPY_STEAL_TECH_ESC,      "Steal Tech" },
    { ACTION_SPY_STEAL_TECH,          "Steal Tech" },
    { ACTION_SPY_SABOTAGE_CITY_ESC,   "Sabotage" },
    { ACTION_SPY_SABOTAGE_CITY,       "Sabotage" },
    { ACTION_SPY_INCITE_CITY_ESC,     "Incite Revolt" },
    { ACTION_SPY_INCITE_CITY,         "Incite Revolt" },
    { ACTION_SPY_STEAL_GOLD_ESC,      "Steal Gold" },
    { ACTION_SPY_STEAL_GOLD,          "Steal Gold" },
    { ACTION_SPY_POISON_ESC,          "Poison Water" },
    { ACTION_SPY_POISON,              "Poison Water" },
    { ACTION_STEAL_MAPS_ESC,          "Steal Maps" },
    { ACTION_STEAL_MAPS,              "Steal Maps" },
};

int get_city_actions(int unit_id, int tx, int ty, CityAction *out, int max)
{
    if (map_is_empty()) return 0;
    struct unit *punit = game_unit_by_number(unit_id);
    struct tile *t = port_native_tile(tx, ty);
    struct city *pcity = t ? tile_city(t) : NULL;
    if (!punit || !pcity) return 0;
    int n = 0;
    for (unsigned i = 0; i < ARRAY_SIZE(k_city_actions) && n < max; i++) {
        if (!is_action_enabled_unit_on_city(&(wld.map), k_city_actions[i].act, punit, pcity)) continue;
        // One entry per label (the "escape" variant is listed first and wins).
        bool dup = false;
        for (int j = 0; j < n; j++) if (!strcmp(out[j].label, k_city_actions[i].label)) dup = true;
        if (dup) continue;
        out[n].action = k_city_actions[i].act;
        fc_strlcpy(out[n].label, k_city_actions[i].label, sizeof(out[n].label));
        n++;
    }
    return n;
}

int get_city_tiles(int city_id, CityTileInfo *out, int max, int *radius)
{
    *radius = 0;
    if (map_is_empty()) return 0;
    struct city *pcity = game_city_by_number(city_id);
    struct player *pov = get_pov_player();
    if (!pcity || city_owner(pcity) != pov) return 0;
    const struct civ_map *nmap = &(wld.map);
    int rsq = city_map_radius_sq_get(pcity);
    int n = 0;
    city_map_iterate(rsq, cindex, cx, cy) {
        struct tile *t = city_map_to_tile(nmap, pcity->tile, rsq, cx, cy);
        if (!t || n >= max) continue;
        CityTileInfo *c = &out[n++];
        c->rx = CITY_ABS2REL(cx);
        c->ry = CITY_ABS2REL(cy);
        if (abs(c->rx) > *radius) *radius = abs(c->rx);
        if (abs(c->ry) > *radius) *radius = abs(c->ry);
        index_to_native_pos(&c->x, &c->y, tile_index(t));
        c->food = city_tile_output_now(pcity, t, O_FOOD);
        c->shield = city_tile_output_now(pcity, t, O_SHIELD);
        c->trade = city_tile_output_now(pcity, t, O_TRADE);
        c->terrain = get_tile_type(c->x, c->y);
        if (t == pcity->tile) c->state = 2;
        else if (tile_worked(t) == pcity) c->state = 1;
        else if (city_can_work_tile(pcity, t)) c->state = 0;
        else c->state = 3;
        (void)cindex;
    } city_map_iterate_end;
    return n;
}

int get_city_specialists(int city_id, SpecialistInfo *out, int max)
{
    if (map_is_empty()) return 0;
    struct city *pcity = game_city_by_number(city_id);
    if (!pcity) return 0;
    int n = 0;
    specialist_type_iterate(sp) {
        if (n >= max) break;
        out[n].id = sp;
        fc_strlcpy(out[n].name, specialist_plural_translation(specialist_by_number(sp)), sizeof(out[n].name));
        out[n].count = pcity->specialists[sp];
        n++;
    } specialist_type_iterate_end;
    return n;
}

int get_city_buildings(int city_id, BuildingInfo *out, int max)
{
    if (map_is_empty()) return 0;
    struct city *pcity = game_city_by_number(city_id);
    if (!pcity) return 0;
    int n = 0;
    city_built_iterate(pcity, pimprove) {
        if (n >= max) break;
        out[n].id = improvement_number(pimprove);
        fc_strlcpy(out[n].name, improvement_name_translation(pimprove), sizeof(out[n].name));
        out[n].can_sell = can_city_sell_building(pcity, pimprove) && !pcity->did_sell;
        out[n].sell_gold = impr_sell_gold(pimprove);
        n++;
    } city_built_iterate_end;
    return n;
}

bool game_pop_sound(char *tag, int len)
{
    if (s_sound_tail == s_sound_head) return false;
    fc_strlcpy(tag, s_sound_ring[s_sound_tail], len);
    s_sound_tail = (s_sound_tail + 1) % SOUND_RING;
    return true;
}

static void move_or_fight(struct player *pov, struct unit *punit, struct tile *dst)
{
    // A manual D-Pad move overrides any pending goto route (handoff.md §2d/
    // test script #4) - without this, a stale punit->goto_tile/orders list
    // would keep rendering a "ghost" target marker after the player took
    // over. free_unit_orders() (common/unit.c) is the same clear used by
    // handle_unit_orders() itself before installing a new route; it no-ops
    // if the unit has no orders.
    free_unit_orders(punit);

    struct city *tcity = tile_city(dst);
    if (tcity && city_owner(tcity) != pov) {
        static const action_id conquer_ids[] = {
            ACTION_CONQUER_CITY_SHRINK, ACTION_CONQUER_CITY_SHRINK2,
            ACTION_CONQUER_CITY_SHRINK3, ACTION_CONQUER_CITY_SHRINK4
        };
        if (try_city_actions(pov, punit, tcity, conquer_ids, ARRAY_SIZE(conquer_ids))) {
            return;
        }
    }
    if (dst->units && unit_list_size(dst->units) > 0) {
        static const action_id combat_ids[] = {
            ACTION_CAPTURE_UNITS, ACTION_ATTACK, ACTION_ATTACK2
        };
        for (int i = 0; i < (int)ARRAY_SIZE(combat_ids); i++) {
            if (is_action_enabled_unit_on_stack(&(wld.map), combat_ids[i], punit, dst)) {
                push_unit_sound('f', punit);   // before: the attacker may not survive
                unit_perform_action(pov, punit->id, tile_index(dst), 0, "", combat_ids[i], ACT_REQ_PLAYER);
                return;
            }
        }

    }
    // Same validated path handle_unit_orders()/ORDER_MOVE uses server-side
    // (server/unittools.c) - movement points, terrain, ZOC etc are all
    // checked inside. move_do_not_act=TRUE: a plain move, no action menu -
    // safe here since the enabler-controlled cases above were already tried.
    unit_move_handling(punit, dst, TRUE);
}

// UNIT_ACTION_FORTIFY_SENTRY: Fortify if not already fortified/fortifying,
// else Sentry. unit_activity_handling() silently no-ops if the activity
// isn't valid for this unit (e.g. a naval unit can't fortify) - no need to
// pre-check here.
static void do_fortify_sentry(struct unit *punit)
{
    if (punit->activity == ACTIVITY_FORTIFIED || punit->activity == ACTIVITY_FORTIFYING) {
        unit_activity_handling(punit, ACTIVITY_SENTRY, ACTION_NONE);
    } else {
        unit_activity_handling(punit, ACTIVITY_FORTIFYING, activity_default_action(ACTIVITY_FORTIFYING));
    }
}

// UNIT_ACTION_CONTEXT: Found City for a Settler, else Build Road/Irrigate/
// Mine (first available extra) for a Worker. No-op if neither applies.
static void do_context_action(struct player *pov, struct unit *punit)
{
    struct tile *ptile = unit_tile(punit);

    if (unit_can_do_action(punit, ACTION_FOUND_CITY)) {
        unit_do_action(pov, punit->id, tile_index(ptile), 0,
                       city_name_suggestion(pov, ptile), ACTION_FOUND_CITY);
        return;
    }

    static const enum extra_cause causes[] = { EC_ROAD, EC_IRRIGATION, EC_MINE };
    static const enum unit_activity activities[] = { ACTIVITY_GEN_ROAD, ACTIVITY_IRRIGATE, ACTIVITY_MINE };
    for (int i = 0; i < ARRAY_SIZE(causes); i++) {
        struct extra_type *target = next_extra_for_tile(ptile, causes[i], pov, punit);
        if (target) {
            unit_activity_handling_targeted(punit, activities[i], &target,
                                            activity_default_action(activities[i]));
            return;
        }
    }
}

static const enum extra_cause k_work_causes[3] = { EC_ROAD, EC_IRRIGATION, EC_MINE };
static const enum unit_activity k_work_acts[3] = { ACTIVITY_GEN_ROAD, ACTIVITY_IRRIGATE, ACTIVITY_MINE };

// Road/irrigation/mine target the unit could start on its tile now, or NULL.
static struct extra_type *work_target(struct player *pov, struct unit *punit, int i)
{
    struct extra_type *t = next_extra_for_tile(unit_tile(punit), k_work_causes[i], pov, punit);
    if (t && can_unit_do_activity_targeted(&(wld.map), punit, k_work_acts[i],
                                           activity_default_action(k_work_acts[i]), t)) {
        return t;
    }
    return NULL;
}

static unsigned unit_action_mask(struct player *pov, struct unit *punit)
{
    unsigned m = (1u << UNIT_ACTION_DISBAND);
    const struct civ_map *nmap = &(wld.map);
    struct tile *ptile = unit_tile(punit);
    if (punit->activity != ACTIVITY_FORTIFIED && punit->activity != ACTIVITY_FORTIFYING
        && can_unit_do_activity(nmap, punit, ACTIVITY_FORTIFYING, activity_default_action(ACTIVITY_FORTIFYING))) {
        m |= 1u << UNIT_ACTION_FORTIFY;
    }
    if (punit->activity != ACTIVITY_SENTRY
        && can_unit_do_activity(nmap, punit, ACTIVITY_SENTRY, activity_default_action(ACTIVITY_SENTRY))) {
        m |= 1u << UNIT_ACTION_SENTRY;
    }
    if (punit->activity != ACTIVITY_EXPLORE
        && can_unit_do_activity(nmap, punit, ACTIVITY_EXPLORE, activity_default_action(ACTIVITY_EXPLORE))) {
        m |= 1u << UNIT_ACTION_EXPLORE;
    }
    if (work_target(pov, punit, 0)) m |= 1u << UNIT_ACTION_ROAD;
    if (work_target(pov, punit, 1)) m |= 1u << UNIT_ACTION_IRRIGATE;
    if (work_target(pov, punit, 2)) m |= 1u << UNIT_ACTION_MINE;
    if (is_action_enabled_unit_on_tile(nmap, ACTION_FOUND_CITY, punit, ptile, NULL)) {
        m |= 1u << UNIT_ACTION_FOUND_CITY;
    }
    struct city *pcity = tile_city(ptile);
    if (pcity && is_action_enabled_unit_on_city(nmap, ACTION_JOIN_CITY, punit, pcity)) {
        m |= 1u << UNIT_ACTION_JOIN_CITY;
    }
    if (punit->activity != ACTIVITY_IDLE || unit_has_orders(punit) || punit->ssa_controller != SSA_NONE) {
        m |= 1u << UNIT_ACTION_WAKE;
    }
    if (punit->ssa_controller != SSA_AUTOWORKER && can_unit_do_autoworker(punit)) {
        m |= 1u << UNIT_ACTION_AUTO_WORK;
    }
    if (punit->activity != ACTIVITY_PILLAGE
        && can_unit_do_activity(nmap, punit, ACTIVITY_PILLAGE, ACTION_PILLAGE)) {
        m |= 1u << UNIT_ACTION_PILLAGE;
    }
    return m;
}

void get_unit_info(int unit_id, UnitInfo *out)
{
    memset(out, 0, sizeof(*out));
    out->id = unit_id;
    if (map_is_empty() || unit_id < 0) return;
    struct unit *punit = game_unit_by_number(unit_id);
    struct player *pov = get_pov_player();
    if (!punit || !unit_tile(punit)) return;
    const struct unit_type *ut = unit_type_get(punit);
    out->valid = true;
    out->owner = player_index(unit_owner(punit));
    out->mine = pov && unit_owner(punit) == pov;
    fc_strlcpy(out->name, utype_name_translation(ut), sizeof(out->name));
    fc_strlcpy(out->rule_name, utype_rule_name(ut), sizeof(out->rule_name));
    out->hp = punit->hp;
    out->hp_max = ut->hp;
    out->attack = ut->attack_strength;
    out->defense = ut->defense_strength;
    out->firepower = ut->firepower;
    out->veteran = punit->veteran;
    out->moves_left = punit->moves_left;
    fc_strlcpy(out->moves, move_points_text(punit->moves_left, TRUE), sizeof(out->moves));
    out->has_orders = unit_has_orders(punit);
    fc_strlcpy(out->activity, punit->ssa_controller == SSA_AUTOWORKER ? "Auto Work"
                              : out->has_orders ? "Under orders" : get_activity_text(punit->activity),
               sizeof(out->activity));
    out->idle = punit->activity == ACTIVITY_IDLE && !out->has_orders;
    struct city *home = game_city_by_number(punit->homecity);
    if (home) fc_strlcpy(out->home, city_name_get(home), sizeof(out->home));
    index_to_native_pos(&out->x, &out->y, tile_index(unit_tile(punit)));
    if (out->mine) out->actions = unit_action_mask(pov, punit);
}

int get_tile_units(int x, int y, UnitBrief *out, int max)
{
    if (map_is_empty() || max <= 0) return 0;
    struct tile *ptile = port_native_tile(x, y);
    struct player *pov = get_pov_player();
    if (!ptile || !ptile->units) return 0;
    int n = 0;
    for (int pass = 0; pass < 2; pass++) {
        unit_list_iterate(ptile->units, punit) {
            bool mine = pov && unit_owner(punit) == pov;
            if ((pass == 0) != mine || n >= max) continue;
            UnitBrief *b = &out[n++];
            b->id = punit->id;
            b->owner = player_index(unit_owner(punit));
            fc_strlcpy(b->name, unit_name_translation(punit), sizeof(b->name));
            fc_strlcpy(b->rule_name, unit_rule_name(punit), sizeof(b->rule_name));
            b->mine = mine;
            b->can_move = punit->moves_left > 0 && punit->activity == ACTIVITY_IDLE && !unit_has_orders(punit);
        } unit_list_iterate_end;
    }
    return n;
}

bool find_next_idle_unit(int after_id, const int *skip, int nskip, int *id, int *x, int *y)
{
    if (map_is_empty()) return false;
    struct player *pov = get_pov_player();
    if (!pov) return false;
    int n = unit_list_size(pov->units);
    if (n == 0) return false;
    int start = 0;
    for (int i = 0; i < n; i++) {
        if (unit_list_get(pov->units, i)->id == after_id) { start = i + 1; break; }
    }
    for (int k = 0; k < n; k++) {
        struct unit *punit = unit_list_get(pov->units, (start + k) % n);
        if (!punit || punit->moves_left <= 0 || punit->activity != ACTIVITY_IDLE
            || unit_has_orders(punit) || punit->ssa_controller != SSA_NONE || !unit_tile(punit)) continue;
        bool skipped = false;
        for (int j = 0; j < nskip; j++) if (skip[j] == punit->id) { skipped = true; break; }
        if (skipped) continue;
        *id = punit->id;
        index_to_native_pos(x, y, tile_index(unit_tile(punit)));
        return true;
    }
    return false;
}

void freeciv3ds_process_pending_actions(void)
{
    if (map_is_empty()) {
        return;
    }
    struct player *pov = get_pov_player();
    if (!pov) {
        return;
    }

    int tail;
    while ((tail = __atomic_load_n(&s_cmd_tail, __ATOMIC_RELAXED)) != __atomic_load_n(&s_cmd_head, __ATOMIC_ACQUIRE)) {
        PendingCmd cmd = s_cmd_ring[tail];
        __atomic_store_n(&s_cmd_tail, (tail + 1) % CMD_RING_SIZE, __ATOMIC_RELEASE);

        switch (cmd.kind) {
        case CMD_UNIT_MOVE: {
            struct unit *punit = game_unit_by_number(cmd.a);
            if (punit && unit_owner(punit) == pov) {
                struct tile *dst = mapstep(&(wld.map), unit_tile(punit), (enum direction8)cmd.b);
                if (dst) {
                    move_or_fight(pov, punit, dst);
                }
            }
            break;
        }
        case CMD_UNIT_GOTO: {
            struct unit *punit = game_unit_by_number(cmd.a);
            struct tile *dst_tile = index_to_tile(&(wld.map), cmd.b);
            if (punit && unit_owner(punit) == pov && dst_tile) {
                // Mirror the client's goto->orders flow (client/goto.c's
                // make_path_orders()/send_goto_path()) rather than stepping
                // the unit ourselves: build a path with the server's own
                // pathfinder, convert it straight to a packet_unit_orders,
                // and let handle_unit_orders() (server/unithand.c) validate
                // and install it - that's the exact call the client's
                // packet would have driven server-side, and it already
                // calls execute_orders() for us (handoff.md §2a).
                struct pf_parameter parameter;
                pft_fill_unit_parameter(&parameter, &(wld.map), punit);
                struct pf_map *pfm = pf_map_new(&parameter);
                struct pf_path *path = pf_map_path(pfm, dst_tile);

                // NULL (no path at all) and a length-1 path (already there)
                // both mean "nothing to order" - handoff.md §5's "unreachable
                // tile" failure mode is exactly this NULL case, so it must be
                // checked before any deref.
                if (path && path->length > 1) {
                    // packet_unit_orders embeds a MAX_LEN_ROUTE(2000)-entry
                    // array by value (~48KB) - static so this doesn't cost
                    // stack on every drain, matching the static snapshot
                    // arrays already used for UI lists elsewhere in this port.
                    static struct packet_unit_orders pkt;
                    memset(&pkt, 0, sizeof(pkt));
                    pkt.unit_id = punit->id;
                    pkt.src_tile = tile_index(unit_tile(punit));
                    pkt.repeat = FALSE;
                    pkt.vigilant = FALSE;

                    struct tile *old_tile = path->positions[0].tile;
                    int len = 0;
                    for (int i = 0; i < path->length - 1 && len < MAX_LEN_ROUTE; i++) {
                        struct tile *new_tile = path->positions[i + 1].tile;
                        if (same_pos(new_tile, old_tile)) {
                            // A path can repeat a tile to mean "wait here"
                            // (e.g. a fuel-limited unit refueling) - see
                            // ai/default/daitools.c's immediate_destination().
                            pkt.orders[len].order = ORDER_FULL_MP;
                            pkt.orders[len].dir = DIR8_ORIGIN;
                        } else {
                            pkt.orders[len].order = ORDER_MOVE;
                            pkt.orders[len].dir =
                                get_direction_for_step(&(wld.map), old_tile, new_tile);
                        }
                        pkt.orders[len].activity = ACTIVITY_LAST;
                        pkt.orders[len].target = NO_TARGET;
                        pkt.orders[len].sub_target = NO_TARGET;
                        pkt.orders[len].action = ACTION_NONE;
                        old_tile = new_tile;
                        len++;
                    }
                    pkt.length = len;
                    pkt.dest_tile = tile_index(old_tile);

                    handle_unit_orders(pov, &pkt);
                }
                if (path) {
                    pf_path_destroy(path);
                }
                pf_map_destroy(pfm);
            }
            break;
        }
        case CMD_UNIT_ACTION: {
            struct unit *punit = game_unit_by_number(cmd.a);
            if (punit && unit_owner(punit) == pov) {
                switch ((UnitActionKind)cmd.b) {
                case UNIT_ACTION_FORTIFY_SENTRY:
                    do_fortify_sentry(punit);
                    break;
                case UNIT_ACTION_CONTEXT:
                    do_context_action(pov, punit);
                    break;
                case UNIT_ACTION_DISBAND:
                    // Direct wipe, not the action-enabler-gated Disband Unit -
                    // always available, no shield recovery. Simple MVP semantics.
                    wipe_unit(punit, ULR_DISBANDED, NULL);
                    break;
                case UNIT_ACTION_FORTIFY:
                    unit_activity_handling(punit, ACTIVITY_FORTIFYING, activity_default_action(ACTIVITY_FORTIFYING));
                    break;
                case UNIT_ACTION_SENTRY:
                    unit_activity_handling(punit, ACTIVITY_SENTRY, activity_default_action(ACTIVITY_SENTRY));
                    break;
                case UNIT_ACTION_EXPLORE:
                    free_unit_orders(punit);
                    unit_activity_handling(punit, ACTIVITY_EXPLORE, activity_default_action(ACTIVITY_EXPLORE));
                    break;
                case UNIT_ACTION_ROAD:
                case UNIT_ACTION_IRRIGATE:
                case UNIT_ACTION_MINE: {
                    int i = cmd.b - UNIT_ACTION_ROAD;
                    struct extra_type *t = work_target(pov, punit, i);
                    if (t) {
                        unit_activity_handling_targeted(punit, k_work_acts[i], &t,
                                                        activity_default_action(k_work_acts[i]));
                    }
                    break;
                }
                case UNIT_ACTION_FOUND_CITY:
                    if (is_action_enabled_unit_on_tile(&(wld.map), ACTION_FOUND_CITY, punit, unit_tile(punit), NULL)) {
                        push_sound("w_generic");
                        unit_do_action(pov, punit->id, tile_index(unit_tile(punit)), 0,
                                       city_name_suggestion(pov, unit_tile(punit)), ACTION_FOUND_CITY);
                    }
                    break;
                case UNIT_ACTION_JOIN_CITY: {
                    struct city *pcity = tile_city(unit_tile(punit));
                    if (pcity && is_action_enabled_unit_on_city(&(wld.map), ACTION_JOIN_CITY, punit, pcity)) {
                        unit_do_action(pov, punit->id, pcity->id, 0, "", ACTION_JOIN_CITY);
                    }
                    break;
                }
                case UNIT_ACTION_WAKE:
                    free_unit_orders(punit);
                    if (punit->ssa_controller != SSA_NONE)
                        handle_unit_server_side_agent_set(pov, punit->id, SSA_NONE);
                    unit_activity_handling(punit, ACTIVITY_IDLE, ACTION_NONE);
                    break;
                case UNIT_ACTION_AUTO_WORK:
                    // Freeciv's auto-worker picks and does terrain work every
                    // turn from now on (it runs at turn change).
                    free_unit_orders(punit);
                    handle_unit_server_side_agent_set(pov, punit->id, SSA_AUTOWORKER);
                    break;
                case UNIT_ACTION_PILLAGE: {
                    struct extra_type *t = NULL;   // NULL: the server picks what to pillage
                    unit_activity_handling_targeted(punit, ACTIVITY_PILLAGE, &t, ACTION_PILLAGE);
                    break;
                }
                default:
                    break;
                }
            }
            break;
        }
        case CMD_UNIT_CITY_ACTION: {
            struct unit *punit = game_unit_by_number(cmd.a);
            struct tile *t = index_to_tile(&(wld.map), cmd.c);
            struct city *pcity = t ? tile_city(t) : NULL;
            if (punit && unit_owner(punit) == pov && pcity
                && is_action_enabled_unit_on_city(&(wld.map), cmd.b, punit, pcity)) {
                free_unit_orders(punit);
                unit_perform_action(pov, punit->id, pcity->id, 0, "", cmd.b, ACT_REQ_PLAYER);
            }
            break;
        }
        case CMD_CITY_TOGGLE_TILE: {
            struct city *pcity = game_city_by_number(cmd.a);
            struct tile *t = index_to_tile(&(wld.map), cmd.b);
            if (pcity && t && city_owner(pcity) == pov) {
                if (tile_worked(t) == pcity) handle_city_make_specialist(pov, cmd.a, cmd.b);
                else handle_city_make_worker(pov, cmd.a, cmd.b);
            }
            break;
        }
        case CMD_CITY_CHANGE_SPECIALIST:
            handle_city_change_specialist(pov, cmd.a, cmd.b, cmd.c);
            break;
        case CMD_CITY_SELL:
            handle_city_sell(pov, cmd.a, cmd.b);
            break;
        case CMD_SPACESHIP_LAUNCH:
            handle_spaceship_launch(pov);
            break;
        case CMD_CITY_PRODUCTION:
            // Self-validating: no-ops on an illegal kind/value or a
            // non-pov/nonexistent city_id.
            handle_city_change(pov, cmd.a, cmd.b, cmd.c);
            break;
        case CMD_CITY_BUY:
            handle_city_buy(pov, cmd.a);
            break;
        case CMD_SET_RESEARCH:
            handle_player_research(pov, cmd.a);
            break;
        case CMD_SET_GOAL:
            handle_player_tech_goal(pov, cmd.a);
            break;
        case CMD_SET_RATES:
            handle_player_rates(pov, cmd.a, cmd.b, cmd.c);
            break;
        case CMD_SET_GOVERNMENT:
            // Self-validating: no-ops if cmd.a isn't a real government or
            // can_change_to_government() rejects it (tech/wonder reqs unmet).
            handle_player_change_government(pov, (Government_type_id)cmd.a);
            break;
        case CMD_CITY_WORKLIST_APPEND: {
            // One layer below handle_city_worklist() (server/cityhand.c),
            // which takes a whole worklist by pointer and doesn't fit the
            // ring's scalar payload - same pattern as unit_move_handling()
            // vs handle_unit_orders() elsewhere in this file (handoff.md
            // §6.1). Its only other effect, send_city_info(), is a no-op
            // network sync with no connected client, same as every other
            // handle_city_*() call this port already bypasses that way.
            struct city *pcity = player_city_by_number(pov, cmd.a);
            if (pcity) {
                struct universal prod = universal_by_number((enum universals_n)cmd.b, cmd.c);
                if (universals_n_is_valid(prod.kind)) {
                    worklist_append(&pcity->worklist, &prod);
                }
            }
            break;
        }
        case CMD_CITY_WORKLIST_REMOVE: {
            struct city *pcity = player_city_by_number(pov, cmd.a);
            if (pcity && cmd.b >= 0 && cmd.b < worklist_length(&pcity->worklist)) {
                worklist_remove(&pcity->worklist, cmd.b);
            }
            break;
        }
        // Diplomacy (handoff.md §7.3): each is a direct one-line call into
        // the matching network-facing handler - there's no lower-level
        // helper to bypass to here, since the treaty state machine itself
        // *is* the logic (§7.1's verify-before-ship note). Every handler
        // self-validates (null/self counterpart, no meeting open, illegal
        // clause, etc) and no-ops rather than crashing.
        case CMD_DIPLOMACY_INIT:
            handle_diplomacy_init_meeting_req(pov, cmd.a);
            break;
        case CMD_DIPLOMACY_CANCEL:
            handle_diplomacy_cancel_meeting_req(pov, cmd.a);
            break;
        case CMD_DIPLOMACY_ADD_CLAUSE:
            // giver is always pov - see freeciv_request_diplomacy_add_clause()'s comment.
            handle_diplomacy_create_clause_req(pov, cmd.a, player_number(pov),
                                               (enum clause_type)cmd.b, cmd.c);
            break;
        case CMD_DIPLOMACY_REMOVE_CLAUSE:
            handle_diplomacy_remove_clause_req(pov, cmd.a, player_number(pov),
                                               (enum clause_type)cmd.b, cmd.c);
            break;
        case CMD_DIPLOMACY_ACCEPT:
            handle_diplomacy_accept_treaty_req(pov, cmd.a);
            break;
        case CMD_SAVE_GAME: {
            // Auto-named by turn, matching the autosave convention -
            // handoff-nohw.md §A.2 defers a name-entry keyboard to a later
            // pass. cwd is already sdmc:/3ds/freeciv/saves (init_freeciv(),
            // game_loop.c), so this bare filename lands there.
            char name[64];
            fc_snprintf(name, sizeof(name), "3ds-T%04d-Y%d", game.info.turn, game.info.year); // read back by save_file_label()
            save_game(name, "User request", FALSE);
            break;
        }
        }
    }
}

struct player *get_pov_player(void)
{
    if (map_is_empty()) return NULL;

    // Try to find the first human player
    players_iterate(pplayer) {
        if (!is_ai(pplayer) && !is_barbarian(pplayer)) {
            return pplayer;
        }
    } players_iterate_end;

    // Fallback: first alive player
    players_iterate(pplayer) {
        if (pplayer->is_alive) {
            return pplayer;
        }
    } players_iterate_end;

    return player_by_number(0);
}

bool is_pov_unit_owner(int unit_owner_index)
{
    struct player *pov = get_pov_player();
    return pov && unit_owner_index == player_index(pov);
}

// Diagnostic: who does the client think "you" are, and do you have units?
// Returns the pov player's unit count (-1 if no pov). Fills name/is_ai/index.
// Used to log why the camera can end up staring at black fog (POV DIAG).
int get_pov_summary(char *name_out, int name_len, bool *is_ai_out, int *index_out)
{
    struct player *pov = get_pov_player();
    if (!pov) {
        if (name_out && name_len > 0) fc_strlcpy(name_out, "(none)", name_len);
        if (is_ai_out) *is_ai_out = false;
        if (index_out) *index_out = -1;
        return -1;
    }
    if (name_out && name_len > 0) {
        const char *nm = player_name(pov);
        fc_strlcpy(name_out, nm ? nm : "?", name_len);
    }
    if (is_ai_out)  *is_ai_out = is_ai(pov);
    if (index_out)  *index_out = player_index(pov);
    return unit_list_size(pov->units);
}

static bool is_tile_unknown_at(int x, int y, struct player *pov)
{
    struct tile *ptile = port_native_tile(x, y);
    if (!ptile) return false;
    return tile_get_known(ptile, pov) == TILE_UNKNOWN;
}

void get_tile_info(int x, int y, TileInfo *out)
{
    memset(out, 0, sizeof(*out));
    out->terrain = get_tile_type(x, y);
    out->unit_owner = -1;
    out->unit_id = -1;
    out->city_owner = -1;
    out->city_id = -1;

    struct tile *ptile = port_native_tile(x, y);
    if (!ptile) {
        return;
    }

    struct city *pcity = tile_city(ptile);
    if (pcity) {
        out->city_owner = player_index(city_owner(pcity));
        out->city_id = pcity->id;
        out->city_size = city_size_get(pcity);
        const char *cname = city_name_get(pcity);
        if (cname) {
            fc_strlcpy(out->city_name, cname, sizeof(out->city_name));
        }
        struct player *pov_c = get_pov_player();
        if (pov_c && city_owner(pcity) == pov_c) {
            const char *pn = city_production_name_translation(pcity);
            if (pn) fc_strlcpy(out->city_prod, pn, sizeof(out->city_prod));
            out->city_prod_turns = city_production_turns_to_build(pcity, TRUE);
        }
    }

    if (ptile->units) {
        out->num_units = unit_list_size(ptile->units);
        if (out->num_units > 0) {
            struct unit *punit = unit_list_get(ptile->units, 0);
            if (punit) {
                out->unit_owner = player_index(unit_owner(punit));
                out->unit_id = punit->id;
                out->unit_hp = punit->hp;
                out->unit_hp_max = unit_type_get(punit)->hp;
                switch (punit->activity) {
                case ACTIVITY_FORTIFIED: case ACTIVITY_FORTIFYING: out->unit_badge = 'F'; break;
                case ACTIVITY_SENTRY:  out->unit_badge = 'S'; break;
                case ACTIVITY_EXPLORE: out->unit_badge = 'E'; break;
                case ACTIVITY_IDLE:    out->unit_badge = unit_has_orders(punit) ? 'G' : 0; break;
                default:               out->unit_badge = 'W'; break;
                }
                if (punit->ssa_controller == SSA_AUTOWORKER) out->unit_badge = 'A';
                struct player *pov_p = get_pov_player();
                out->unit_done = pov_p && unit_owner(punit) == pov_p
                                 && (punit->moves_left <= 0 || out->unit_badge != 0);
                const char *uname = unit_rule_name(punit);
                if (uname) {
                    fc_strlcpy(out->unit_name, uname, sizeof(out->unit_name));
                }
            }
        }
    }

    // Populate extras/overlays
    extra_type_iterate(pextra) {
        if (tile_has_extra(ptile, pextra)) {
            const char *rname = extra_rule_name(pextra);
            if (strcasecmp(rname, "Road") == 0) out->has_road = true;
            else if (strcasecmp(rname, "Railroad") == 0) out->has_rail = true;
            else if (strcasecmp(rname, "Maglev") == 0) out->has_maglev = true;
            else if (strcasecmp(rname, "River") == 0) out->has_river = true;
            else if (strcasecmp(rname, "Mine") == 0) out->has_mine = true;
            else if (strcasecmp(rname, "Irrigation") == 0) out->has_irrigation = true;
            else if (strcasecmp(rname, "Farmland") == 0) out->has_farmland = true;
            else if (strcasecmp(rname, "Hut") == 0) out->has_village = true;
            else if (strcasecmp(rname, "Pollution") == 0) out->has_pollution = true;
            else if (strcasecmp(rname, "Fallout") == 0) out->has_fallout = true;
        }
    } extra_type_iterate_end;

    if (ptile->resource) {
        const char *gtag = ptile->resource->graphic_str;
        if (gtag) {
            fc_strlcpy(out->resource_tag, gtag, sizeof(out->resource_tag));
        }
    }

    // Populate vision / FOW
    struct player *pov = get_pov_player();
    if (!pov) {
        out->visibility = 2; // Seen
        out->darkness_idx = 0;
    } else {
        out->visibility = tile_get_known(ptile, pov);

        // Calculate darkness_idx (transitions to unexplored)
        int N = is_tile_unknown_at(x, y - 1, pov);
        int S = is_tile_unknown_at(x, y + 1, pov);
        int E = is_tile_unknown_at(x + 1, y, pov);
        int W = is_tile_unknown_at(x - 1, y, pov);
        out->darkness_idx = N | (E << 1) | (S << 2) | (W << 3);
    }

    // Populate borders
    out->tile_owner = ptile->owner ? player_index(ptile->owner) : -1;
    if (out->tile_owner != -1) {
        struct tile *n_tile = mapstep(&(wld.map), ptile, DIR8_NORTH);
        int n_owner = (n_tile && n_tile->owner) ? player_index(n_tile->owner) : -1;
        out->border_n = (n_owner != out->tile_owner);

        struct tile *s_tile = mapstep(&(wld.map), ptile, DIR8_SOUTH);
        int s_owner = (s_tile && s_tile->owner) ? player_index(s_tile->owner) : -1;
        out->border_s = (s_owner != out->tile_owner);

        struct tile *e_tile = mapstep(&(wld.map), ptile, DIR8_EAST);
        int e_owner = (e_tile && e_tile->owner) ? player_index(e_tile->owner) : -1;
        out->border_e = (e_owner != out->tile_owner);

        struct tile *w_tile = mapstep(&(wld.map), ptile, DIR8_WEST);
        int w_owner = (w_tile && w_tile->owner) ? player_index(w_tile->owner) : -1;
        out->border_w = (w_owner != out->tile_owner);
    }
}

void get_minimap_tile_info(int x, int y, MinimapTileInfo *out)
{
    out->terrain = get_tile_type(x, y);
    out->visibility = 2; // Default to Seen if no POV player exists
    out->unit_owner = -1;
    out->city_owner = -1;

    struct tile *ptile = port_native_tile(x, y);
    if (!ptile) {
        return;
    }

    struct player *pov = get_pov_player();
    if (pov) {
        out->visibility = tile_get_known(ptile, pov);
    }

    if (out->visibility > 0) {
        struct city *pcity = tile_city(ptile);
        if (pcity) {
            out->city_owner = player_index(city_owner(pcity));
        }

        if (out->visibility == 2 && ptile->units && unit_list_size(ptile->units) > 0) {
            struct unit *punit = unit_list_get(ptile->units, 0);
            if (punit) {
                out->unit_owner = player_index(unit_owner(punit));
            }
        }
    }
}

void get_city_info(int city_id, CityInfo *out)
{
    memset(out, 0, sizeof(*out));
    out->city_id = city_id;

    struct player *pov = get_pov_player();
    if (!pov) {
        return;
    }
    struct city *pcity = player_city_by_number(pov, city_id);
    if (!pcity) {
        return;
    }

    out->valid = true;
    out->owner = player_index(pov);
    const char *cname = city_name_get(pcity);
    if (cname) {
        fc_strlcpy(out->name, cname, sizeof(out->name));
    }
    out->size = city_size_get(pcity);
    out->food_stock = pcity->food_stock;
    out->food_needed = city_granary_size(out->size);
    out->shield_stock = pcity->shield_stock;
    for (int i = 0; i < O_LAST; i++) {
        out->surplus[i] = pcity->surplus[i];
    }
    const char *pname = city_production_name_translation(pcity);
    if (pname) {
        fc_strlcpy(out->producing, pname, sizeof(out->producing));
    }
    out->prod_cost = city_production_build_shield_cost(pcity);
    out->prod_turns = city_production_turns_to_build(pcity, TRUE);
    out->grow_turns = city_turns_to_grow(pcity);
    out->happy = pcity->feel[CITIZEN_HAPPY][FEELING_FINAL];
    out->content = pcity->feel[CITIZEN_CONTENT][FEELING_FINAL];
    out->unhappy = pcity->feel[CITIZEN_UNHAPPY][FEELING_FINAL];
    out->angry = pcity->feel[CITIZEN_ANGRY][FEELING_FINAL];
    out->disorder = pcity->anarchy > 0 || city_unhappy(pcity);

    // Mirrors really_handle_city_buy()'s (server/cityhand.c) validity checks,
    // without performing the buy - so the panel can hide/disable the Buy
    // button before the player taps it, matching what the server would
    // actually accept.
    out->buy_cost = city_production_buy_gold_cost(pcity);
    out->can_buy = pcity->turn_founded != game.info.turn
                   && !pcity->did_buy
                   && !city_production_is_genus(pcity, IG_CONVERT)
                   && !(VUT_UTYPE == pcity->production.kind && pcity->anarchy != 0)
                   && out->buy_cost > 0
                   && out->buy_cost <= pov->economic.gold;

    // Garrison: the pov's units standing on the city tile, kept as parallel
    // id/name arrays (not just a joined string) so the city screen can both
    // display and resolve-to-take-out a specific entry, not just the first
    // (handoff.md §3). Clamped to CITY_MAX_GARRISON - large stacks just don't
    // enumerate past that; garrison_count reflects the clamped total.
    out->garrison_count = 0;
    struct tile *ctile = city_tile(pcity);
    if (ctile && ctile->units) {
        unit_list_iterate(ctile->units, punit) {
            if (unit_owner(punit) != pov) {
                continue;
            }
            if (out->garrison_count >= CITY_MAX_GARRISON) {
                break;
            }
            int idx = out->garrison_count;
            out->garrison_ids[idx] = punit->id;
            const char *un = unit_name_translation(punit);
            fc_strlcpy(out->garrison_names[idx], un ? un : "?", sizeof(out->garrison_names[idx]));
            const char *st = punit->activity == ACTIVITY_FORTIFIED || punit->activity == ACTIVITY_FORTIFYING ? "Fortified"
                           : punit->activity == ACTIVITY_SENTRY ? "Sentry"
                           : punit->activity != ACTIVITY_IDLE ? "Working"
                           : unit_has_orders(punit) ? "On the move"
                           : punit->moves_left > 0 ? "Ready" : "Done";
            fc_strlcpy(out->garrison_status[idx], st, sizeof(out->garrison_status[idx]));
            out->garrison_count++;
        } unit_list_iterate_end;
    }
}

int get_city_buildables(int city_id, BuildOption *out, int max)
{
    struct player *pov = get_pov_player();
    if (!pov) {
        return 0;
    }
    struct city *pcity = player_city_by_number(pov, city_id);
    if (!pcity) {
        return 0;
    }

    int n = 0;
    unit_type_iterate(putype) {
        if (n >= max) break;
        if (can_city_build_unit_now(&(wld.map), pcity, putype, RPT_CERTAIN)) {
            out[n].kind = VUT_UTYPE;
            out[n].value = utype_number(putype);
            const char *uname = utype_name_translation(putype);
            fc_strlcpy(out[n].name, uname ? uname : "?", sizeof(out[n].name));
            out[n].cost = utype_build_shield_cost(pcity, pov, putype);
            {
                struct universal tgt = { .kind = VUT_UTYPE, .value.utype = putype };
                out[n].turns = city_turns_to_build(pcity, &tgt, TRUE);
            }
            out[n].is_current = (pcity->production.kind == VUT_UTYPE
                                  && pcity->production.value.utype == putype);
            n++;
        }
    } unit_type_iterate_end;

    improvement_iterate(pimprove) {
        if (n >= max) break;
        if (can_city_build_improvement_now(pcity, pimprove, RPT_CERTAIN)) {
            out[n].kind = VUT_IMPROVEMENT;
            out[n].value = improvement_number(pimprove);
            const char *iname = improvement_name_translation(pimprove);
            fc_strlcpy(out[n].name, iname ? iname : "?", sizeof(out[n].name));
            // Coinage turns shields into gold; its nominal cost (999) means nothing.
            out[n].cost = improvement_has_flag(pimprove, IF_GOLD) ? -1
                                                                   : impr_build_shield_cost(pcity, pimprove);
            {
                struct universal tgt = { .kind = VUT_IMPROVEMENT, .value.building = pimprove };
                out[n].turns = out[n].cost < 0 ? 0 : city_turns_to_build(pcity, &tgt, TRUE);
            }
            out[n].is_current = (pcity->production.kind == VUT_IMPROVEMENT
                                  && pcity->production.value.building == pimprove);
            n++;
        }
    } improvement_iterate_end;

    return n;
}

int get_city_worklist(int city_id, WorklistEntry *out, int max)
{
    struct player *pov = get_pov_player();
    if (!pov) {
        return 0;
    }
    struct city *pcity = player_city_by_number(pov, city_id);
    if (!pcity) {
        return 0;
    }

    int n = 0;
    int len = worklist_length(&pcity->worklist);
    for (int i = 0; i < len && n < max; i++) {
        struct universal prod;
        if (!worklist_peek_ith(&pcity->worklist, &prod, i)) {
            break;
        }
        out[n].kind = prod.kind;
        out[n].value = (prod.kind == VUT_UTYPE) ? utype_number(prod.value.utype)
                                                 : improvement_number(prod.value.building);
        universal_name_translation(&prod, out[n].name, sizeof(out[n].name));
        n++;
    }
    return n;
}

void get_civ_info(CivInfo *out)
{
    memset(out, 0, sizeof(*out));

    struct player *pov = get_pov_player();
    if (!pov) {
        return;
    }

    out->gold = pov->economic.gold;
    out->gold_per_turn = player_get_expected_income(pov);
    out->tax = pov->economic.tax;
    out->lux = pov->economic.luxury;
    out->sci = pov->economic.science;
    out->max_rate = get_player_bonus(pov, EFT_MAX_RATES);

    struct research *research = research_get(pov);
    out->researching = research->researching;
    out->goal = research->tech_goal;
    if (research->tech_goal != A_UNSET && research->tech_goal != A_NONE
        && research_invention_state(research, research->tech_goal) != TECH_KNOWN) {
        const char *gname = research_advance_name_translation(research, research->tech_goal);
        if (gname) fc_strlcpy(out->goal_name, gname, sizeof(out->goal_name));
    }
    const char *rname = research_advance_name_translation(research, research->researching);
    if (rname) {
        fc_strlcpy(out->research_name, rname, sizeof(out->research_name));
    }
    out->bulbs = research->bulbs_researched;
    out->bulbs_needed = research_total_bulbs_required(research, research->researching, FALSE);

    int bulbs_per_turn = 0;
    city_list_iterate(pov->cities, pcity) {
        bulbs_per_turn += pcity->surplus[O_SCIENCE];
    } city_list_iterate_end;

    if (out->bulbs >= out->bulbs_needed) {
        out->research_turns = 0;
    } else if (bulbs_per_turn > 0) {
        out->research_turns = (out->bulbs_needed - out->bulbs + bulbs_per_turn - 1) / bulbs_per_turn;
    } else {
        out->research_turns = -1; // no science surplus - undeterminable
    }

    struct government *gov = government_of_player(pov);
    out->government_id = gov ? government_number(gov) : -1;
    const char *gname = gov ? government_name_translation(gov) : NULL;
    if (gname) {
        fc_strlcpy(out->government_name, gname, sizeof(out->government_name));
    }

    // pplayer->target_government is set for the whole revolution (including
    // its anarchy period) and cleared back to NULL the instant
    // government_change() lands the new government - see server/plrhand.c.
    if (pov->target_government) {
        out->target_government_id = government_number(pov->target_government);
        int turns = pov->revolution_finishes - game.info.turn;
        out->revolution_turns = turns > 0 ? turns : 0;
    } else {
        out->target_government_id = -1;
        out->revolution_turns = 0;
    }
}

int get_governments(GovOption *out, int max)
{
    struct player *pov = get_pov_player();
    if (!pov) {
        return 0;
    }

    int n = 0;
    governments_iterate(pgov) {
        if (n >= max) break;
        out[n].gov_id = government_number(pgov);
        const char *gname = government_name_translation(pgov);
        fc_strlcpy(out[n].name, gname ? gname : "?", sizeof(out[n].name));
        out[n].available = can_change_to_government(pov, pgov);
        n++;
    } governments_iterate_end;

    return n;
}

int get_other_players(PlayerOption *out, int max)
{
    struct player *pov = get_pov_player();
    if (!pov) {
        return 0;
    }

    int n = 0;
    players_iterate(pplayer) {
        if (n >= max) break;
        if (pplayer == pov || is_barbarian(pplayer) || !pplayer->is_alive) {
            continue;
        }
        if (player_diplstate_get(pov, pplayer)->type == DS_NO_CONTACT) {
            continue; // never met: no meeting is possible
        }
        out[n].player_id = player_number(pplayer);
        const char *nm = player_name(pplayer);
        fc_strlcpy(out[n].name, nm ? nm : "?", sizeof(out[n].name));
        out[n].diplstate = (int)player_diplstate_get(pov, pplayer)->type;
        n++;
    } players_iterate_end;

    return n;
}

// Short human-readable label for one treaty clause (handoff.md §7.2's
// DiploInfo.clauses[].desc) - covers the full enum, not just the four
// types this port's UI can add (§7.4), since an AI counterpart's own
// proposal (e.g. a first-contact ceasefire, dai_diplomacy_suggest()) can
// appear in a treaty the pov player didn't initiate.
static void describe_clause(int type, int value, char *buf, size_t bufsz)
{
    switch (type) {
    case CLAUSE_ADVANCE: {
        const char *nm = advance_rule_name(advance_by_number(value));
        snprintf(buf, bufsz, "Tech: %s", nm ? nm : "?");
        break;
    }
    case CLAUSE_GOLD:
        snprintf(buf, bufsz, "%d Gold", value);
        break;
    case CLAUSE_MAP:
        fc_strlcpy(buf, "Map", bufsz);
        break;
    case CLAUSE_SEAMAP:
        fc_strlcpy(buf, "Sea Map", bufsz);
        break;
    case CLAUSE_CITY: {
        struct city *pcity = game_city_by_number(value);
        const char *nm = pcity ? city_name_get(pcity) : NULL;
        snprintf(buf, bufsz, "City: %s", nm ? nm : "?");
        break;
    }
    case CLAUSE_CEASEFIRE:
        fc_strlcpy(buf, "Ceasefire", bufsz);
        break;
    case CLAUSE_PEACE:
        fc_strlcpy(buf, "Peace", bufsz);
        break;
    case CLAUSE_ALLIANCE:
        fc_strlcpy(buf, "Alliance", bufsz);
        break;
    case CLAUSE_VISION:
        fc_strlcpy(buf, "Shared Vision", bufsz);
        break;
    case CLAUSE_EMBASSY:
        fc_strlcpy(buf, "Embassy", bufsz);
        break;
    case CLAUSE_SHARED_TILES:
        fc_strlcpy(buf, "Shared Tiles", bufsz);
        break;
    default:
        fc_strlcpy(buf, "?", bufsz);
        break;
    }
}

void get_diplo_info(int counterpart_id, DiploInfo *out)
{
    memset(out, 0, sizeof(*out));
    out->counterpart_id = counterpart_id;

    struct player *pov = get_pov_player();
    struct player *other = player_by_number(counterpart_id);
    if (!pov || !other) {
        return;
    }

    struct treaty *ptreaty = find_treaty(pov, other);
    if (!ptreaty) {
        return; // no meeting open - active stays false
    }

    out->active = true;
    if (ptreaty->plr0 == pov) {
        out->accept_self = ptreaty->accept0;
        out->accept_other = ptreaty->accept1;
    } else {
        out->accept_self = ptreaty->accept1;
        out->accept_other = ptreaty->accept0;
    }

    int n = 0;
    clause_list_iterate(ptreaty->clauses, pclause) {
        if (n >= DIPLO_MAX_CLAUSES) break;
        out->clauses[n].type = (int)pclause->type;
        out->clauses[n].value = pclause->value;
        describe_clause(pclause->type, pclause->value,
                        out->clauses[n].desc, sizeof(out->clauses[n].desc));
        n++;
    } clause_list_iterate_end;
    out->clause_count = n;
}

int get_researchable(TechOption *out, int max)
{
    struct player *pov = get_pov_player();
    if (!pov) {
        return 0;
    }
    struct research *research = research_get(pov);
    int bpt = 0;
    city_list_iterate(pov->cities, pcity) {
        bpt += pcity->surplus[O_SCIENCE];
    } city_list_iterate_end;

    int n = 0;
    advance_iterate(padvance) {
        if (n >= max) break;
        Tech_type_id tech = advance_number(padvance);
        if (research_invention_state(research, tech) == TECH_PREREQS_KNOWN) {
            out[n].tech = tech;
            const char *tname = research_advance_name_translation(research, tech);
            fc_strlcpy(out[n].name, tname ? tname : "?", sizeof(out[n].name));
            out[n].current = research->researching == tech;
            int need = research_total_bulbs_required(research, tech, FALSE);
            if (out[n].current) need -= research->bulbs_researched;
            if (need < 0) need = 0;
            out[n].turns = bpt > 0 ? (need + bpt - 1) / bpt : -1;
            n++;
        }
    } advance_iterate_end;

    return n;
}

// Tier memo for get_tech_tree()'s compute_tech_tier() below - sized to
// A_LAST (common/tech.h) so any ruleset's real tech ids fit regardless of
// how many techs it defines. Recomputed from scratch (memset) on every
// get_tech_tree() call rather than invalidated incrementally - cheap at
// ~90 techs and this project's established pattern (every get_*_info()
// recomputes its derived state fresh each !busy frame rather than tracking
// staleness).
static int  s_tech_tier[A_LAST];
static bool s_tech_tier_done[A_LAST];

// tier(A_NONE) = 0 (the "no prerequisite" root); tier(t) = 1 + max(tier of
// its AR_ONE/AR_TWO prereqs, each treated as 0 if A_NONE). Freeciv's tech
// graph is a DAG by construction (a tech can only require techs that exist
// before it), so plain memoized recursion terminates without needing an
// in-progress cycle guard.
static int compute_tech_tier(Tech_type_id tech)
{
    if (tech == A_NONE) {
        return 0;
    }
    if (s_tech_tier_done[tech]) {
        return s_tech_tier[tech];
    }
    int t1 = compute_tech_tier(advance_required(tech, AR_ONE));
    int t2 = compute_tech_tier(advance_required(tech, AR_TWO));
    int tier = 1 + (t1 > t2 ? t1 : t2);
    s_tech_tier[tech] = tier;
    s_tech_tier_done[tech] = true;
    return tier;
}

int get_tech_tree(TechNode *out, int max)
{
    struct player *pov = get_pov_player();
    if (!pov) {
        return 0;
    }
    struct research *research = research_get(pov);

    memset(s_tech_tier_done, 0, sizeof(s_tech_tier_done));

    int n = 0;
    advance_iterate(padvance) {
        if (n >= max) break;
        Tech_type_id tech = advance_number(padvance);
        out[n].tech = tech;
        const char *tname = research_advance_name_translation(research, tech);
        fc_strlcpy(out[n].name, tname ? tname : "?", sizeof(out[n].name));
        out[n].state = (int)research_invention_state(research, tech);
        out[n].tier = compute_tech_tier(tech);
        n++;
    } advance_iterate_end;

    // Stable insertion sort by tier - keeps each tier's techs in their
    // original (tech-id) relative order while making every same-tier run
    // contiguous, which is what render.c's row-walk (handoff.md §8.4)
    // depends on. n is small (~90 for classic), so O(n^2) is negligible.
    for (int i = 1; i < n; i++) {
        TechNode key = out[i];
        int j = i - 1;
        while (j >= 0 && out[j].tier > key.tier) {
            out[j + 1] = out[j];
            j--;
        }
        out[j + 1] = key;
    }

    return n;
}

static bool is_savegame_name(const char *name)
{
    static const char *SUFFIX = ".sav.gz";
    const size_t suffix_len = 7; // strlen(".sav.gz")
    size_t len = strlen(name);
    return len > suffix_len && strcmp(name + len - suffix_len, SUFFIX) == 0;
}

// --- Load-order index (port-owned recency truth) ----------------------------
// The picker must show the most-recently-written saves first, but directory
// enumeration order does NOT track recency on the 3DS FAT card: overwriting an
// existing file (which every autosave "freeciv-T%04d-..." and every manual
// "3ds-T<turn>" does, turn-for-turn, across sessions) rewrites it in place
// WITHOUT moving its directory entry, so an early-turn save stays pinned at its
// oldest slot forever and can never appear in a "newest N" list (confirmed on
// hardware: freeciv-T0001-auto sat at directory position 0 of 93). st_mtime was
// tried twice and found non-discriminating on this archive.
//
// So the port keeps its OWN append-only order index at .saveindex - one
// "<size> <name>" line per observed state, most-recent last. Every picker open
// reconciles it: a save whose on-disk SIZE differs from (or is absent from) its
// latest index record is a fresh write and gets appended, pushing it to
// "newest". Size is a reliable directory-tracked property (unlike the mtimes),
// and a re-saved file's size practically always changes (different map / turn /
// unit set), so this captures autosaves and manual saves alike with no
// server-core hook. We then emit the newest `max` distinct still-present names
// and rewrite the index compacted (latest record per existing file) so it stays
// bounded and never lists a deleted file.
#define SAVE_INDEX_PATH  "sdmc:/3ds/freeciv/saves/.saveindex"
#define SAVE_INDEX_CAP   2048 // must cover every save on the card, or old ones would read as new
#define SAVE_EXIST_CAP   2048 // files on the card (autosaves pile up: one per turn)

typedef struct { char name[64]; long size; } SaveIndexEntry;

static long saves_file_size(const char *dir, const char *name)
{
    char path[128];
    fc_snprintf(path, sizeof(path), "%s/%s", dir, name);
    struct stat st;
    return (stat(path, &st) == 0) ? (long)st.st_size : -1;
}

int get_save_files(SaveFile *out, int max)
{
    static const char *SAVES_DIR = "sdmc:/3ds/freeciv/saves";
    static SaveIndexEntry idx[SAVE_INDEX_CAP];
    static char existing[SAVE_EXIST_CAP][64];
    int nidx = 0, nexist = 0;

    // 1. Load the existing order index (oldest-first).
    FILE *f = fopen(SAVE_INDEX_PATH, "r");
    if (f) {
        char line[160];
        while (nidx < SAVE_INDEX_CAP && fgets(line, sizeof(line), f)) {
            long sz;
            char nm[64];
            if (sscanf(line, "%ld %63s", &sz, nm) == 2) {
                idx[nidx].size = sz;
                fc_strlcpy(idx[nidx].name, nm, sizeof(idx[nidx].name));
                nidx++;
            }
        }
        fclose(f);
    }

    // 2. Scan the directory. Record every real save name (readdir is the
    //    authoritative existence check), and append a fresh index record when a
    //    file is new or its size changed since its latest record.
    DIR *dir = opendir(SAVES_DIR);
    if (!dir) {
        return 0;
    }
    struct dirent *ent;
    while ((ent = readdir(dir)) != NULL) {
        if (!is_savegame_name(ent->d_name)) {
            continue;
        }
        if (nexist < SAVE_EXIST_CAP) {
            fc_strlcpy(existing[nexist++], ent->d_name, sizeof(existing[0]));
        }
        long dsize = saves_file_size(SAVES_DIR, ent->d_name);
        long last = 0;
        bool found = false;
        for (int i = nidx - 1; i >= 0; i--) {
            if (strcmp(idx[i].name, ent->d_name) == 0) {
                last = idx[i].size;
                found = true;
                break;
            }
        }
        // New file -> record it (size may be -1 if stat hiccuped; it still gets
        // an entry so it can never silently vanish from the picker). Known file
        // -> only bump to newest on a real, positive size change.
        bool bump = !found || (dsize >= 0 && dsize != last);
        if (bump) {
            if (nidx == SAVE_INDEX_CAP) { // full: forget the oldest record, keep the newest
                memmove(idx, idx + 1, (SAVE_INDEX_CAP - 1) * sizeof(idx[0]));
                nidx--;
            }
            idx[nidx].size = dsize;
            fc_strlcpy(idx[nidx].name, ent->d_name, sizeof(idx[nidx].name));
            nidx++;
        }
    }
    closedir(dir);

    // 3. Emit the newest `max` DISTINCT names that still exist (newest first).
    int n = 0;
    for (int i = nidx - 1; i >= 0 && n < max; i--) {
        bool dup = false;
        for (int j = 0; j < n; j++) {
            if (strcmp(out[j].filename, idx[i].name) == 0) { dup = true; break; }
        }
        if (dup) {
            continue;
        }
        bool exists = false;
        for (int e = 0; e < nexist; e++) {
            if (strcmp(existing[e], idx[i].name) == 0) { exists = true; break; }
        }
        if (!exists) {
            continue;
        }
        fc_strlcpy(out[n].filename, idx[i].name, sizeof(out[n].filename));
        n++;
    }

    // 4. Rewrite the index compacted: latest record per still-existing file, in
    //    chronological order.
    f = fopen(SAVE_INDEX_PATH, "w");
    if (f) {
        for (int i = 0; i < nidx; i++) {
            bool superseded = false;
            for (int k = i + 1; k < nidx; k++) {
                if (strcmp(idx[k].name, idx[i].name) == 0) { superseded = true; break; }
            }
            if (superseded) {
                continue;
            }
            bool exists = false;
            for (int e = 0; e < nexist; e++) {
                if (strcmp(existing[e], idx[i].name) == 0) { exists = true; break; }
            }
            if (!exists) {
                continue;
            }
            fprintf(f, "%ld %s\n", idx[i].size, idx[i].name);
        }
        fclose(f);
    }

    return n;
}

bool has_road_at(int x, int y)
{
    struct tile *ptile = port_native_tile(x, y);
    if (!ptile) return false;
    static struct extra_type *road_extra = NULL;
    if (!road_extra) road_extra = extra_type_by_rule_name("Road");
    return road_extra && tile_has_extra(ptile, road_extra);
}

bool has_rail_at(int x, int y)
{
    struct tile *ptile = port_native_tile(x, y);
    if (!ptile) return false;
    static struct extra_type *rail_extra = NULL;
    if (!rail_extra) rail_extra = extra_type_by_rule_name("Railroad");
    return rail_extra && tile_has_extra(ptile, rail_extra);
}

bool has_maglev_at(int x, int y)
{
    struct tile *ptile = port_native_tile(x, y);
    if (!ptile) return false;
    static struct extra_type *maglev_extra = NULL;
    if (!maglev_extra) maglev_extra = extra_type_by_rule_name("Maglev");
    return maglev_extra && tile_has_extra(ptile, maglev_extra);
}

bool has_river_at(int x, int y)
{
    struct tile *ptile = port_native_tile(x, y);
    if (!ptile) return false;
    static struct extra_type *river_extra = NULL;
    if (!river_extra) river_extra = extra_type_by_rule_name("River");
    return river_extra && tile_has_extra(ptile, river_extra);
}

