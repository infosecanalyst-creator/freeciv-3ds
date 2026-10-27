#ifndef GAME_LOOP_H
#define GAME_LOOP_H

#include <3ds.h>

typedef enum {
    TERRAIN_OCEAN,
    TERRAIN_GRASSLAND,
    TERRAIN_PLAINS,
    TERRAIN_DESERT,
    TERRAIN_TUNDRA,
    TERRAIN_SWAMP,
    TERRAIN_JUNGLE,
    TERRAIN_HILLS,
    TERRAIN_FOREST,
    TERRAIN_MOUNTAINS,
    TERRAIN_ARCTIC
} TerrainType;

// 8-way move directions for freeciv_request_unit_move(), independent of
// Freeciv's own direction8 enum so main.c doesn't need any freeciv headers.
typedef enum {
    MOVE_DIR_NORTH,
    MOVE_DIR_NORTHEAST,
    MOVE_DIR_EAST,
    MOVE_DIR_SOUTHEAST,
    MOVE_DIR_SOUTH,
    MOVE_DIR_SOUTHWEST,
    MOVE_DIR_WEST,
    MOVE_DIR_NORTHWEST
} MoveDir;

// Context-dependent unit actions for freeciv_request_unit_action().
typedef enum {
    // Toggle: Fortify if not already fortified/fortifying, else Sentry.
    UNIT_ACTION_FORTIFY_SENTRY,
    // Whatever the selected unit can usefully do in place: Found City for a
    // Settler, else Build Road/Irrigate/Mine (first available) for a Worker.
    // No-op if the unit supports none of these.
    UNIT_ACTION_CONTEXT,
    // Immediately destroys the unit (no shield recovery - a direct wipe,
    // not the ruleset action-enabler-gated Disband Unit).
    UNIT_ACTION_DISBAND,
    // Single-purpose actions offered by the touch unit menu (each is a no-op
    // if the unit can't do it there - get_unit_info() reports which apply).
    UNIT_ACTION_FORTIFY,
    UNIT_ACTION_SENTRY,
    UNIT_ACTION_ROAD,
    UNIT_ACTION_IRRIGATE,
    UNIT_ACTION_MINE,
    UNIT_ACTION_EXPLORE,
    UNIT_ACTION_FOUND_CITY,
    UNIT_ACTION_JOIN_CITY,
    UNIT_ACTION_WAKE,        // cancel orders / current activity
    UNIT_ACTION_PILLAGE,
    UNIT_ACTION_AUTO_WORK,   // hand the unit to Freeciv's auto-worker
    UNIT_ACTION_COUNT
} UnitActionKind;

// Everything the top screen shows about one unit, plus which
// UnitActionKind values apply right now (bit (1 << kind) in `actions`).
typedef struct {
    bool valid;
    int  id;
    int  owner;              // player index
    bool mine;               // owned by the pov player
    char name[48];           // translated unit type name
    char rule_name[48];      // ruleset name (sprite lookup)
    int  hp, hp_max;
    int  attack, defense, firepower;
    int  veteran;            // 0 = green
    char moves[24];          // "2", "1 1/3", ...
    int  moves_left;         // move fragments, 0 = done for this turn
    char activity[32];       // "Idle", "Fortified", "Mine", ...
    bool idle;               // ACTIVITY_IDLE and no orders
    bool has_orders;
    char home[48];           // home city name, "" if none
    int  x, y;               // native tile
    unsigned actions;        // bitmask of UnitActionKind
} UnitInfo;

// One unit in a tile's stack (for picking which one to command).
typedef struct {
    int  id;
    int  owner;
    char name[48];           // translated
    char rule_name[48];
    bool can_move;           // moves left and idle
    bool mine;               // owned by the player (snapshot: safe to use mid-step)
} UnitBrief;


// Snapshot of what occupies a single map tile, read from the in-process
// Freeciv server state. Owner fields are player indices, or -1 if absent.
typedef struct {
    TerrainType terrain;
    int  unit_owner;   // player index of the top unit, or -1
    int  unit_id;      // server-side id of the top unit, or -1
    int  num_units;
    int  unit_hp, unit_hp_max;   // top unit's hit points
    bool unit_done;    // top unit is pov's and has no moves left / is busy
    char unit_badge;   // 'F' fortified, 'S' sentry, 'W' working, 'E' exploring, 'G' goto, 0 none
    int  city_owner;   // player index of the city, or -1
    int  city_id;      // server-side id of the city, or -1 (pass to get_city_info())
    int  city_size;
    char unit_name[48];
    char city_name[48];
    char city_prod[24];  // your cities only: what it is building ("" otherwise)
    int  city_prod_turns;

    // Extras/Overlays
    bool has_river;
    bool has_road;
    bool has_rail;
    bool has_maglev;
    bool has_mine;
    bool has_irrigation;
    bool has_farmland;
    bool has_village;   // goody hut
    bool has_pollution;
    bool has_fallout;

    char resource_tag[48]; // ruleset graphic tag for resource, empty if none

    // Vision / Fog of War
    int visibility;    // 0 = unknown, 1 = fogged (known unseen), 2 = seen (fully visible)
    int darkness_idx;  // 4-bit cardinal mask (0 to 15) for transition to unknown

    // Borders
    int tile_owner;    // player index of owner, or -1
    bool border_n;
    bool border_s;
    bool border_e;
    bool border_w;
} TileInfo;

typedef struct {
    TerrainType terrain;
    int visibility;
    int unit_owner;
    int city_owner;
} MinimapTileInfo;


// Cap on how many garrisoned units a CityInfo snapshot can enumerate
// individually (handoff.md §3) - population loop clamps to this.
#define CITY_MAX_GARRISON 16

// Snapshot of a pov-owned city's display state, read from the in-process
// Freeciv server state. Mirrors TileInfo's contract: filled only while
// !fc3ds_server_is_busy(), never held across frames as a raw pointer.
typedef struct {
    bool valid;              // false if city_id no longer resolves / not pov's
    int  city_id;
    char name[48];
    int  owner;              // player index
    int  size;
    int  food_stock, food_needed;
    int  shield_stock;
    int  surplus[6];         // indexed by O_FOOD,O_SHIELD,O_TRADE,O_GOLD,O_LUXURY,O_SCIENCE
    char producing[48];
    int  prod_cost;
    int  prod_turns;
    int  grow_turns;         // <0 => not growing (shrinking or stagnant)
    bool can_buy;
    int  buy_cost;
    int  garrison_count;                          // # of pov-owned units in the city, clamped to CITY_MAX_GARRISON
    int  garrison_ids[CITY_MAX_GARRISON];          // server-side unit ids, parallel to garrison_names
    char garrison_names[CITY_MAX_GARRISON][32];
} CityInfo;

// One entry in a city's list of currently-buildable units/improvements.
typedef struct {
    int  kind;        // enum universals_n: VUT_IMPROVEMENT or VUT_UTYPE
    int  value;        // improvement_number() / utype_number()
    char name[48];
    int  cost;
    int  turns;        // turns to build at the city's current shield rate
    bool is_current;   // matches pcity->production
} BuildOption;

// One entry in a city's worklist (handoff.md §6) - same (kind, value) shape
// as BuildOption, since a worklist entry is a struct universal exactly like
// a buildable option is.
typedef struct {
    int  kind;
    int  value;
    char name[48];
} WorklistEntry;

// Snapshot of the pov player's civilization-level state (economy + research).
typedef struct {
    int  gold;
    int  tax, lux, sci;
    int  max_rate;
    int  researching;        // tech id; A_UNSET/A_FUTURE are valid values
    char research_name[48];
    int  bulbs, bulbs_needed;
    int  research_turns;     // -1 if undeterminable (no science surplus)
    int  government_id;         // Government_type_id of the current government
    char government_name[48];
    int  target_government_id;  // Government_type_id mid-revolution is switching to, -1 if none
    int  revolution_turns;      // turns until target_government_id takes effect, 0 if none
} CivInfo;

// One entry in the list of governments the pov player could revolt to
// (handoff.md §5). available mirrors can_change_to_government() so the
// panel can grey out (and refuse taps on) ones the player can't switch to.
typedef struct {
    int  gov_id;
    char name[48];
    bool available;
} GovOption;

// One entry in the list of techs the pov player could switch research to.
typedef struct {
    int  tech;
    char name[48];
} TechOption;

// One entry in the full tech tree (handoff.md §8) - every tech in the
// ruleset, not just the currently-researchable subset TechOption covers.
// get_tech_tree() returns these tier-sorted (stable, ascending) so a
// consecutive run sharing the same tier is one displayable row - render.c's
// tiered tree view and its hit-test rely on that ordering, not just the
// tier field itself.
typedef struct {
    int  tech;
    char name[48];
    int  state;   // enum tech_state: TECH_UNKNOWN=0, TECH_PREREQS_KNOWN=1, TECH_KNOWN=2
    int  tier;    // 1 + max(tier of AR_ONE/AR_TWO prereqs), 0 prereqs -> tier 1
} TechNode;

// One entry in the list of other players the pov player could open a
// diplomacy meeting with (handoff.md §7.2). Excludes pov itself and
// barbarians (get_other_players()'s job, not the caller's).
typedef struct {
    int  player_id;
    char name[48];
    int  diplstate; // enum diplstate_type: DS_ARMISTICE=0,WAR=1,CEASEFIRE=2,PEACE=3,ALLIANCE=4,NO_CONTACT=5
} PlayerOption;

// One clause in an open treaty (handoff.md §7.2/7.4). desc is a
// precomputed short label ("Peace", "50 Gold", ...) so the panel never
// needs freeciv's per-clause-type formatting logic.
#define DIPLO_MAX_CLAUSES 16
typedef struct {
    int  type;   // enum clause_type (common/diptreaty.h) - network-protocol-stable ids
    int  value;
    char desc[48];
} ClauseInfo;

// Clause type ids for freeciv_request_diplomacy_add_clause()/_remove_clause()
// - mirror common/diptreaty.h's enum clause_type, which that header documents
// as "used in the network protocol" (so these numeric values are stable).
// Only the four v1-supported clause types (handoff.md §7.4) are named here;
// ClauseInfo.type on the read side may be any of the full enum's values (an
// AI counterpart, or a future pass, could propose others) - get_diplo_info()
// renders those generically rather than main.c needing to know every id.
#define DIPLO_CLAUSE_GOLD      1
#define DIPLO_CLAUSE_CEASEFIRE 5
#define DIPLO_CLAUSE_PEACE     6
#define DIPLO_CLAUSE_EMBASSY   9

// Snapshot of the treaty meeting (if any) between the pov player and
// counterpart_id. active=false (rest zeroed) if no meeting is open -
// find_treaty() returning NULL, whether because none was ever opened or
// because the last one resolved/was canceled (handoff.md §7.4 stage 3).
typedef struct {
    bool active;
    int  counterpart_id;
    bool accept_self, accept_other;
    int  clause_count;
    ClauseInfo clauses[DIPLO_MAX_CLAUSES];
} DiploInfo;

void init_freeciv(bool is_new_3ds);
void step_freeciv(u32 kDown, u32 kHeld);
void deinit_freeciv(void);

// Queue an on-demand save of the current game, auto-named "3ds-T<turn>"
// (same convention as the existing autosaves). Runs on the server thread
// via save_game() (freeciv/server/savegame/savemain.c) - see
// handoff-nohw.md §A.2. No-op (queues nothing useful, but harmless) if no
// game is running yet.
void freeciv_request_save_game(void);

// One entry in the saves-directory listing for the load file-picker
// (handoff-nohw.md §A.4). filename is a bare name suitable for passing
// straight to fc3ds_server_begin_load_game() (fc3ds_server.h) - it is
// resolved against the same saves-dir cwd save_game() itself writes into.
typedef struct {
    char filename[64];
} SaveFile;

// Enumerate sdmc:/3ds/freeciv/saves for *.sav.gz files, newest-first by
// mtime. Returns the count written, capped at max. Plain filesystem read -
// safe to call regardless of fc3ds_server_is_busy() (no Freeciv game state
// touched).
int get_save_files(SaveFile *out, int max);

// Server lifecycle (start/pump/busy/ready/end-turn/shutdown/pause) lives in
// fc3ds_server.h - see that header for the cooperative-scheduling contract
// the main loop codes against.

void get_map_dimensions(int *w, int *h);

// True when the Freeciv map structure has not been created yet (map_is_empty())
// - e.g. during world generation on Turn 1 of a New Game, or before any game
// is loaded. render.c uses this to defer texture allocation until real map
// dimensions are available, preventing the 40x40 race-condition fallback.
bool game_map_is_empty(void);

// Whether the map wraps on each axis (WRAPX/WRAPY topology flags). The port
// renders/selects in native coordinates; callers use these to wrap seam-crossing
// neighbour lookups, touch mapping and the camera. False before the map exists.
bool map_wraps_x(void);
bool map_wraps_y(void);

// Current game turn (0 before the map/game exists).
int get_game_turn(void);

// Terrain mock logic for the initial UI
TerrainType get_tile_type(int x, int y);

// Fill *out with the terrain/unit/city state of tile (x, y).
void get_tile_info(int x, int y, TileInfo *out);
void get_minimap_tile_info(int x, int y, MinimapTileInfo *out);

// Fill *out with the given city's display state, if it exists and is owned
// by the pov player; otherwise out->valid = false and the rest is zeroed.
// !busy only - see the read-model contract in TileInfo's comment above.
void get_city_info(int city_id, CityInfo *out);

// Fill out[0..return) with what city_id can currently build (units then
// improvements). Returns the count written, capped at max. !busy only.
int get_city_buildables(int city_id, BuildOption *out, int max);

// Fill out[0..return) with city_id's current worklist, in build order.
// Returns the count written, capped at max. !busy only.
int get_city_worklist(int city_id, WorklistEntry *out, int max);

// Queue appending (kind, value) - a BuildOption's pair, see above - to the
// end of city_id's worklist. No-ops server-side if kind/value don't resolve
// to a real improvement/unit type.
void freeciv_request_city_worklist_append(int city_id, int kind, int value);

// Queue removing the worklist entry at index (0 = next after current
// production) from city_id's worklist. No-ops server-side if index is out
// of range.
void freeciv_request_city_worklist_remove(int city_id, int index);

// Fill *out with the pov player's economy/research state. !busy only.
void get_civ_info(CivInfo *out);

// Fill out[0..return) with the techs the pov player could set as their
// current research target right now. Returns the count written, capped at
// max. !busy only.
int get_researchable(TechOption *out, int max);

// Fill out[0..return) with every tech in the ruleset (handoff.md §8),
// tier-sorted ascending - see TechNode's comment for why the sort matters.
// Returns the count written, capped at max. !busy only. 100% additive on
// the write side: tapping a PREREQS_KNOWN node still uses the existing
// freeciv_request_set_research(), no new command needed.
int get_tech_tree(TechNode *out, int max);

// Fill out[0..return) with every government in the ruleset, marking which
// ones the pov player could currently revolt to. Returns the count written,
// capped at max. !busy only.
int get_governments(GovOption *out, int max);

// Queue a government-revolution request for the pov player. No-ops
// server-side (via handle_player_change_government()) if government isn't
// a valid id or can_change_to_government() rejects it.
void freeciv_request_set_government(int government);

// Fill out[0..return) with every other living player (excludes pov and
// barbarians), for the diplomacy player picker (handoff.md §7.2/7.4).
// Returns the count written, capped at max. !busy only.
int get_other_players(PlayerOption *out, int max);

// Fill *out with the pov/counterpart treaty meeting's state, if any. !busy
// only. See DiploInfo's comment for what active=false means.
void get_diplo_info(int counterpart_id, DiploInfo *out);

// Queue opening a diplomacy meeting with counterpart. No-ops server-side
// (via handle_diplomacy_init_meeting_req()) if one is already open, no
// contact exists, or diplomacy is otherwise blocked (e.g. EFT_NO_DIPLOMACY).
void freeciv_request_diplomacy_init(int counterpart);

// Queue canceling the pov/counterpart meeting, if one is open. No-op
// (via handle_diplomacy_cancel_meeting_req()) otherwise.
void freeciv_request_diplomacy_cancel(int counterpart);

// Queue adding a clause to the pov/counterpart meeting, with pov always as
// the giver (handoff.md §7.4's v1 clause set is either a symmetric pact -
// Peace/Ceasefire, where "giver" doesn't matter - or a one-directional gift
// *from* pov - Embassy/Gold). No-ops server-side (via
// handle_diplomacy_create_clause_req()) if no meeting is open or the clause
// is illegal (duplicate, already-held embassy, etc).
void freeciv_request_diplomacy_add_clause(int counterpart, int clause_type, int value);

// Queue removing a clause matching (clause_type, value, pov-as-giver) from
// the pov/counterpart meeting. No-op (via
// handle_diplomacy_remove_clause_req()) if no such clause exists.
void freeciv_request_diplomacy_remove_clause(int counterpart, int clause_type, int value);

// Queue toggling the pov player's acceptance of the pov/counterpart treaty
// (via handle_diplomacy_accept_treaty_req() - a TOGGLE, not a one-way set:
// calling this while already accepted un-accepts). No-op if no meeting is
// open. If both sides end up accepted, the treaty resolves server-side
// immediately and the meeting closes (DiploInfo.active goes false).
void freeciv_request_diplomacy_accept(int counterpart);

// Find the first map tile that contains a point-of-view-owned unit. Returns
// false if the pov player has no units yet (e.g. not placed this frame) or
// no pov player exists.
bool find_pov_unit_tile(int *x, int *y);
// Sound cues raised by the engine side (combat, a city founded); the main
// thread pops them each frame and plays them. Tags are Freeciv soundspec tags.
bool game_pop_sound(char *tag, int len);
// True once the game has ended (or your civilization is gone).
bool get_game_result(bool *won, int *score, int *turn);

// ---- Spaceship ----
typedef struct {
    int  state;                 // 0 none, 1 building, 2 launched, 3 arrived
    int  structurals, components, modules;
    int  population;
    int  success_pct;
    int  travel_tenths;         // travel time in tenths of years
    int  launch_year;
    bool can_launch;
} SpaceshipInfo;
void get_spaceship_info(SpaceshipInfo *out);
void freeciv_request_spaceship_launch(void);

// ---- Civilopedia (ruleset help) ----
typedef enum { PEDIA_UNITS, PEDIA_BUILDINGS, PEDIA_WONDERS, PEDIA_TECHS, PEDIA_GOVS, PEDIA_KINDS } PediaKind;
typedef struct { int id; char name[48]; } PediaEntry;
int get_pedia_list(int kind, PediaEntry *out, int max);
// Stats then the ruleset's help text, '\n' between paragraphs.
void get_pedia_text(int kind, int id, char *out, int len);
bool get_pedia_graphic(int kind, int id, char *tag, int tlen, char *alt, int alen);

// ---- News: Freeciv's event messages for the pov player ----
typedef enum { MSG_OTHER, MSG_COMBAT, MSG_CITY, MSG_SCIENCE, MSG_DIPLOMACY, MSG_GOOD } MsgCategory;
typedef struct {
    int  turn;
    int  x, y;          // native tile, -1 if none
    int  category;      // MsgCategory
    char text[160];     // markup stripped
} GameMessage;
// Total messages received so far (a running counter, never reset).
int get_message_total(void);
// Smart End Turn button. A city that finished something this turn, oldest
// first: returns its id (or -1) and fills its name and map position.
int get_pending_production(char *name, int len, int *x, int *y);
void dismiss_pending_production(int city_id);
// True when nothing is being researched (the player should pick).
bool research_needs_choice(void);
// Tiles the unit can still reach this turn (not its own); returns the count.
int get_move_range(int unit_id, short *xs, short *ys, int max);
// Copies message number `index` (0 = first ever) if it's still in the log.
bool get_message(int index, GameMessage *out);
#define MESSAGE_LOG_SIZE 128

// ---- Combat odds (read-only, while parked) ----
typedef struct {
    char attacker[48], defender[48];
    char defender_rule[48];
    int  defender_owner;
    int  def_hp, def_hp_max, att_hp, att_hp_max;
    int  win_pct;       // 0..100
    bool can_attack;    // the ruleset allows attacking there now (at war, etc.)
} AttackOdds;
// false if (tx, ty) holds no units unit_id could fight.
bool get_attack_odds(int unit_id, int tx, int ty, AttackOdds *out);

// ---- Route preview ----
#define ROUTE_MAX 64
typedef struct { int n; int x[ROUTE_MAX], y[ROUTE_MAX]; int turns; bool reachable; } RoutePreview;
void get_route_preview(int unit_id, int tx, int ty, RoutePreview *out);

// ---- Actions aimed at a city (caravans, diplomats, upgrades...) ----
typedef struct { int action; char label[32]; } CityAction;
// What unit_id could do to the city on (tx, ty) right now - its own tile
// (help wonder, disband for shields, upgrade, home city) or an adjacent one
// (embassy, sabotage, trade route...). Returns how many were written.
int get_city_actions(int unit_id, int tx, int ty, CityAction *out, int max);
void freeciv_request_city_action(int unit_id, int action, int tx, int ty);

// ---- City tiles, specialists and buildings ----
typedef struct {
    int rx, ry;          // offset from the city centre (-r..r)
    int x, y;            // native tile
    int food, shield, trade;
    int state;           // 0 free, 1 worked by this city, 2 the centre, 3 not available
    int terrain;         // TerrainType
} CityTileInfo;
#define CITY_TILES_MAX 64
int get_city_tiles(int city_id, CityTileInfo *out, int max, int *radius);
typedef struct { int id; char name[32]; int count; } SpecialistInfo;
int get_city_specialists(int city_id, SpecialistInfo *out, int max);
typedef struct { int id; char name[48]; bool can_sell; int sell_gold; } BuildingInfo;
int get_city_buildings(int city_id, BuildingInfo *out, int max);
void freeciv_request_city_toggle_tile(int city_id, int tx, int ty);   // work it / free it
void freeciv_request_city_change_specialist(int city_id, int from, int to);
void freeciv_request_city_sell(int city_id, int building_id);
// Fills *out for unit_id (out->valid=false if it no longer exists).
void get_unit_info(int unit_id, UnitInfo *out);
// The units on tile (x, y), pov's first; returns how many were written.
int get_tile_units(int x, int y, UnitBrief *out, int max);
// The next pov unit after after_id (in unit-list order, wrapping) that is
// idle with moves left and not in skip[0..nskip); false if none.
bool find_next_idle_unit(int after_id, const int *skip, int nskip, int *id, int *x, int *y);

// Find the current tile of the unit with the given server-side id. Returns
// false (leaving *x/*y untouched) if the unit no longer exists.
bool find_unit_tile_by_id(int unit_id, int *x, int *y);

// Read the given unit's pending goto destination (handoff.md §2b), if any.
// Returns false (leaving *x/*y untouched) if the unit doesn't exist or has
// no goto_tile set (no pending route). Same !is_busy()-gated snapshot
// contract as find_unit_tile_by_id() above.
bool get_unit_goto_tile(int unit_id, int *x, int *y);

// Queue a one-tile move for the given unit. Applied asynchronously on the
// server thread (within ~1s) via freeciv3ds_process_pending_actions() -
// Freeciv game state must only ever be mutated on the server thread, never
// directly from the render/input thread this is called from.
void freeciv_request_unit_move(int unit_id, MoveDir dir);

// Queue a context action for the given unit - see UnitActionKind. Applied
// the same way and with the same server-thread-only constraint as
// freeciv_request_unit_move() above.
void freeciv_request_unit_action(int unit_id, UnitActionKind kind);

// Queue a multi-turn goto: build a path with the server's own pathfinder
// and hand it to the unit as orders (handoff.md §2a), applied the same way
// as freeciv_request_unit_move() above. (tile_x, tile_y) is the destination
// in map coordinates (same space as sel_x/sel_y). No-op if the unit doesn't
// exist, isn't pov's, or no path exists to the destination - all decided
// server-side in the drain handler, never here.
void freeciv_request_unit_goto(int unit_id, int tile_x, int tile_y);

// Queue a production change for a pov-owned city. kind/value are an
// (enum universals_n, id) pair - see BuildOption. Applied asynchronously via
// handle_city_change() on the server thread, same contract as the unit
// requests above.
void freeciv_request_city_production(int city_id, int kind, int value);

// Queue a buy-current-production for a pov-owned city. No-ops server-side
// (via handle_city_buy()) if not affordable/already bought/etc.
void freeciv_request_city_buy(int city_id);

// Queue a research target change for the pov player. tech is a Tech_type_id
// (or A_FUTURE). No-ops server-side (via handle_player_research()) if the
// tech's prereqs aren't met.
void freeciv_request_set_research(int tech);

// Queue a tax/luxury/science rate change for the pov player. No-ops
// server-side (via handle_player_rates()) unless tax+lux+sci==100 and each
// is within the government's max rate.
void freeciv_request_set_rates(int tax, int lux, int sci);

// True if the tile's top unit (per TileInfo.unit_owner) belongs to the
// point-of-view player - i.e. it's a unit the player can select and move.
bool is_pov_unit_owner(int unit_owner_index);

// Direct server state helpers for overlays
bool has_road_at(int x, int y);
bool has_rail_at(int x, int y);
bool has_maglev_at(int x, int y);
bool has_river_at(int x, int y);

struct player *get_pov_player(void);

// Diagnostic: fills the pov player's name (up to name_len), whether it's AI,
// and its player index; returns its unit count (-1 if there is no pov player).
int get_pov_summary(char *name_out, int name_len, bool *is_ai_out, int *index_out);

#endif
