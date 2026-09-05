/* ===== v7: 돌-style macro bot (2026-08-29, see the v7 block at the end) =====
   ========================================================================== */
/* ===== v5: NEXT VISION macro bot (2026-08-29) ==============================
   Infrastructure (parsing, fog tracking, intel) shared with v4_0.cpp; the
   strategy is new - see the v5 MACRO STRATEGY block at the end.
   ========================================================================== */
/* ===== NEXT VISION port (2026-08-29) =====================================
   Ported from the NEXT NATION qualifier build below.  Rule deltas applied:
     - 400 days (was 200), start gold 750 (was 500)
     - HQ upgrade 600/1000/2000/3000 (was 600/1200/2400/3600)
     - BASE build/upgrade 500/550/600 (was 300/600/1000)
     - N in 181..249 (was 51..109), K in ceil(sqrt N - 1)..floor(sqrt N + 4)
     - FOG OF WAR: the result block now carries ONLY our own UPGRADE/TRAIN/
       MOVE/DAMAGE/SIEGE events, followed by WARRIOR/BUILDING snapshots of
       everything inside our 2-hop vision.  read_turn_result() was rewritten:
         * own units/buildings are reconciled against the snapshot (authoritative)
         * enemy warriors = exactly the ones visible this turn
         * enemy buildings = last-known memory; an entry is dropped only when its
           zone is visible and the building is no longer reported there
         * S->visible[r] (2-hop union over own units) is available to strategy
     - endgame-anchored turn thresholds moved by LATE_TURN_SHIFT (= +200)
   Everything else (strategy, GA-tuned genes) is untouched and was tuned for
   the old economy/full information - see the NOTE block before decide().
   ========================================================================== */
/* species2b champion gen12 (job 1275764, 86758-base run) - SUBMISSION BUILD.
   VERIFIED 2026-07-06 at 1000-game rigor, 0 own WAs in 5000 games:
     a23 rush     0.823+-0.012  (gate 0.7 PASS - PROJECT RECORD)
     placeholder2 0.843+-0.012  (gate 0.6 PASS)
     g15off       0.874+-0.009  (gate 0.5 PASS)
     vs 85052 (gen33) 0.446 / vs 83616 (gen17) 0.489 - loses the macro
     mirror to the live entry; best pure defender measured. */
/* species2b champion gen17 (job 1275469, 82804-base run) - SUBMISSION BUILD.
   VERIFIED 2026-07-06 at 1000-game rigor, 0 own WAs in 4000 games:
     a23 rush     0.767+-0.013  (gate 0.7 PASS)
     placeholder2 0.885+-0.010  (gate 0.6 PASS; parent 82804: 0.428)
     g15off       0.925+-0.007  (gate 0.5 PASS)
     vs 82804 (live submission) head-to-head: 0.662 (528W 266D 205L)
   Base: user upload lineage 82804 economy + GA-retuned defense genes. */
/* species2b champion gen8 (job 1275167, doctrine run) - SUBMISSION BUILD.
   VERIFIED 2026-07-05 (50-game panels, 0 own WAs): placeholder2 0.700+-0.065
   PASS(0.6), g15off 0.860+-0.038 PASS(0.5), a23rushB2 0.870+-0.046 PASS(0.7),
   vs 76784 head-to-head 0.440. Doctrine pinned: parity train, HQ exact-sim,
   attack predictor, no-expand-while-outnumbered, rescue margin.
   Anchor-route ON (MIN_ATTACKERS=22), rush OFF. */
/* species2b center-first anchor: once CENTER BASE is ours, the first attack anchor is CENTER; later anchors may change normally. */
#define _POSIX_C_SOURCE 200809L
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(__GNUC__) || defined(__clang__)
#define MAYBE_UNUSED __attribute__((unused))
#else
#define MAYBE_UNUSED
#endif

enum {
  MAX_TURN = 400,         /* maximum turn (days) */
  LEGACY_MAX_TURN = 200,  /* qualifier game length the genes were tuned on */
  START_GOLD = 750,       /* initial gold */
  VISION_HOPS = 2,        /* fog of war: every own unit/building sees 2 hops */
  START_WARRIORS = 3,     /* initial warriors */
  MOVE_COST = 10,         /* move cost */
  TRAIN_COST = 120,       /* train cost */
  WORK_INCOME = 15,       /* income per warrior */
  UPKEEP_PER_WARRIOR = 2, /* upkeep per warrior */
  HQ_MAX_LEVEL = 5,       /* HQ max level */
  BASE_MAX_LEVEL = 3,     /* base max level */
  HQ_HEAL_COST = 1000,    /* HQ fix cost */
  BASE_HEAL_COST = 500,   /* base fix cost */
};

typedef struct {
  int upgrade_cost;
  int warrior_hp;
  int hp;
  int turret;
  int train_cap;
  int work_cap;
} HqLevelEntry;

typedef struct {
  int cost;
  int hp;
  int turret;
  int work_cap;
} BaseLevelEntry;

static const HqLevelEntry HQ_LEVELS[HQ_MAX_LEVEL + 1] = {
    {0, 0, 0, 0, 0, 0},     {0, 4, 10, 1, 1, 1},    {600, 5, 15, 2, 1, 2},
    {1000, 6, 20, 2, 2, 3}, {2000, 7, 25, 3, 2, 4}, {3000, 8, 30, 3, 3, 5},
};
static const BaseLevelEntry BASE_LEVELS[BASE_MAX_LEVEL + 1] = {
    {0, 0, 0, 0},
    {500, 6, 1, 1},
    {550, 12, 1, 2},
    {600, 18, 2, 3},
};

/* Turn thresholds that mean "this many days before the END of the game" were
   tuned on a 200-day game.  Shift them so they keep the same distance from the
   turn limit.  Opening/mid-game thresholds are left where they were. */
#define LATE_TURN_SHIFT (MAX_TURN - LEGACY_MAX_TURN)

typedef enum { SIDE_LEFT = 0, SIDE_RIGHT = 1 } Side;
typedef enum { BTYPE_HQ = 0, BTYPE_BASE = 1 } BType;
typedef enum { WSTATE_STATIONARY = 0, WSTATE_MOVING = 1 } WState;

static Side opposite(Side s) { return s == SIDE_LEFT ? SIDE_RIGHT : SIDE_LEFT; }
static char side_char(Side s) { return s == SIDE_LEFT ? 'A' : 'B'; }
static Side parse_side_char(char c) { return c == 'A' ? SIDE_LEFT : SIDE_RIGHT; }

typedef struct {
  Side side;
  int num;
} WarriorId;

static int wid_eq(WarriorId a, WarriorId b) {
  return a.side == b.side && a.num == b.num;
}

typedef struct {
  WarriorId id;
  int region;
  int hp;
  WState state;
  int target;
} Warrior;

typedef struct {
  int region;
  Side side;
  BType type;
  int level;
  int hp;
  int last_seen_turn; /* enemy buildings: last turn the zone was visible (memory) */
} Building;

static int building_current_hp(const Building *b) {
  return b->type == BTYPE_HQ ? HQ_LEVELS[b->level].hp : BASE_LEVELS[b->level].hp;
}
static int building_work_cap(const Building *b) {
  return b->type == BTYPE_HQ ? HQ_LEVELS[b->level].work_cap
                             : BASE_LEVELS[b->level].work_cap;
}
static int building_max_level(const Building *b) {
  return b->type == BTYPE_HQ ? HQ_MAX_LEVEL : BASE_MAX_LEVEL;
}
static int building_upgrade_cost(const Building *b) {
  return b->type == BTYPE_HQ ? HQ_LEVELS[b->level + 1].upgrade_cost
                             : BASE_LEVELS[b->level + 1].cost;
}
static void building_apply_upgrade(Building *b) {
  b->level += 1;
  b->hp = building_current_hp(b);
}

#define VEC_PUSH(vec, item)                                                    \
  do {                                                                         \
    if ((vec).len == (vec).cap) {                                              \
      (vec).cap = (vec).cap ? (vec).cap * 2 : 8;                               \
      (vec).data = (__typeof__((vec).data))realloc((vec).data, (size_t)(vec).cap * sizeof(*(vec).data)); \
    }                                                                          \
    (vec).data[(vec).len++] = (item);                                          \
  } while (0)

typedef struct {
  WarriorId id;
  int target;
} Move;

typedef struct {
  Warrior *data;
  int len, cap;
} WarriorVec;
typedef struct {
  Building *data;
  int len, cap;
} BuildingVec;
typedef struct {
  Move *data;
  int len, cap;
} MoveVec;
typedef struct {
  int *data;
  int len, cap;
} IntVec;

typedef struct {
  int N, K;
  long long *x, *y;
  IntVec strongholds;
  IntVec *adj;

  Side my_side;
  int my_hq;
  int opp_hq;
} GameMap;

typedef struct {
  int gold;
  int my_countdown;
  int opp_countdown;
  WarriorVec warriors;     /* own: all alive.  enemy: only the ones visible now */
  BuildingVec buildings;   /* own: all.  enemy: last-known memory (see header) */
  unsigned char *visible;  /* [N] 1 if zone is inside our 2-hop vision (end of turn) */
  int *zone_seen_turn;     /* [N] last turn the zone was visible, -1 = never */
  int turn;                /* last completed day (0 before the first result) */
  int enemy_max_suffix;    /* largest enemy warrior number ever observed:
                              the enemy has trained at least (this - 3) warriors */
  Side my_side;            /* copy of GameMap::my_side for helpers that only get S */
} GameState;

typedef struct {
  int train_n;
  MoveVec moves;
  IntVec upgrades;
} Actions;

/* ---- input helpers ---- */

static char *readln(void) {
  static char *buf = NULL;
  static size_t cap = 0;
  ssize_t len = getline(&buf, &cap, stdin);
  if (len < 0)
    exit(0);
  while (len > 0 && (buf[len - 1] == '\n' || buf[len - 1] == '\r'))
    buf[--len] = '\0';
  return buf;
}

static char **tokens(const char *line, int *count) {
  static char *storage = NULL;
  static size_t storage_cap = 0;
  static char **items = NULL;
  static int items_cap = 0;

  size_t len = strlen(line) + 1;
  if (len > storage_cap) {
    storage_cap = len;
    storage = (char *)realloc(storage, storage_cap);
  }
  memcpy(storage, line, len);

  int n = 0;
  char *save = NULL;
  for (char *t = strtok_r(storage, " \t", &save); t;
       t = strtok_r(NULL, " \t", &save)) {
    if (n == items_cap) {
      items_cap = items_cap ? items_cap * 2 : 16;
      items = (char **)realloc(items, (size_t)items_cap * sizeof(char *));
    }
    items[n++] = t;
  }
  *count = n;
  return items;
}

static int cmp_int(const void *a, const void *b) {
  int x = *(const int *)a, y = *(const int *)b;
  return (x > y) - (x < y);
}

static WarriorId parse_warrior(const char *tok) {
  WarriorId id;
  id.side = parse_side_char(tok[0]);
  id.num = atoi(tok + 1);
  return id;
}

static int hq_of(const GameMap *M, Side s) {
  return (s == SIDE_LEFT) ? 0 : M->N - 1;
}

static Building make_base(int region, Side s) {
  Building b = {region, s, BTYPE_BASE, 1, BASE_LEVELS[1].hp};
  return b;
}

static Building *find_building(GameState *S, int region) {
  for (int i = 0; i < S->buildings.len; ++i)
    if (S->buildings.data[i].region == region)
      return &S->buildings.data[i];
  return NULL;
}

static Warrior *find_warrior(GameState *S, WarriorId id) {
  for (int i = 0; i < S->warriors.len; ++i)
    if (wid_eq(S->warriors.data[i].id, id))
      return &S->warriors.data[i];
  return NULL;
}

/* ---- fog of war ---- */

static int region_visible(const GameState *S, int region) {
  return S->visible != NULL && region >= 0 && S->visible[region];
}

/* Union of VISION_HOPS-hop neighbourhoods of every own warrior and building.
   Matches the referee (_hop_set over the adjacency list, not Euclidean). */
static void compute_visibility(GameState *S, const GameMap *M) {
  int N = M->N;
  if (S->visible == NULL)
    S->visible = (unsigned char *)malloc((size_t)N);
  if (S->zone_seen_turn == NULL) {
    S->zone_seen_turn = (int *)malloc((size_t)N * sizeof(int));
    for (int r = 0; r < N; ++r) S->zone_seen_turn[r] = -1;
  }
  memset(S->visible, 0, (size_t)N);
  int *stamp = (int *)calloc((size_t)N, sizeof(int));
  int *queue = (int *)malloc((size_t)N * sizeof(int));
  int *depth = (int *)malloc((size_t)N * sizeof(int));
  int gen = 0;
  for (int pass = 0; pass < 2; ++pass) {
    int cnt = pass == 0 ? S->warriors.len : S->buildings.len;
    for (int i = 0; i < cnt; ++i) {
      int start;
      if (pass == 0) {
        const Warrior *w = &S->warriors.data[i];
        if (w->id.side != M->my_side) continue;
        start = w->region;
      } else {
        const Building *b = &S->buildings.data[i];
        if (b->side != M->my_side) continue;
        start = b->region;
      }
      if (start < 0 || start >= N) continue;
      ++gen;
      int head = 0, tail = 0;
      queue[tail] = start; depth[tail++] = 0; stamp[start] = gen;
      S->visible[start] = 1;
      while (head < tail) {
        int u = queue[head], d = depth[head]; ++head;
        if (d >= VISION_HOPS) continue;
        for (int k = 0; k < M->adj[u].len; ++k) {
          int v = M->adj[u].data[k];
          if (stamp[v] == gen) continue;
          stamp[v] = gen;
          S->visible[v] = 1;
          queue[tail] = v; depth[tail++] = d + 1;
        }
      }
    }
  }
  free(stamp); free(queue); free(depth);
  for (int r = 0; r < N; ++r)
    if (S->visible[r]) S->zone_seen_turn[r] = S->turn;
}

/* Fog of war: "we know of no enemy base" only means "cleared" once every
   stronghold has actually been looked at.  Before that it is just ignorance. */
static int all_strongholds_observed(const GameState *S, const GameMap *M) {
  if (S->zone_seen_turn == NULL) return 0;
  for (int i = 0; i < M->strongholds.len; ++i)
    if (S->zone_seen_turn[M->strongholds.data[i]] < 0) return 0;
  return 1;
}

static void note_enemy_suffix(GameState *S, const GameMap *M, WarriorId id) {
  if (id.side != M->my_side && id.num > S->enemy_max_suffix)
    S->enemy_max_suffix = id.num;
}

/* Lower bound on how many warriors the enemy has trained so far (IDs are
   sequential per side).  Deaths are not observable, so this is NOT an army
   size - use it as a floor when the visible count is obviously too small. */
static int enemy_trained_at_least(const GameState *S) {
  int n = S->enemy_max_suffix - START_WARRIORS;
  return n < 0 ? 0 : n;
}

/* ===== NEXT VISION: enemy intel under fog of war =============================
   What we can know without seeing the whole map:
     - trained count: warrior numbers are sequential (B7 => at least 4 trained)
     - HQ level:      a warrior is trained with HQ_LEVELS[level].warrior_hp, so the
                      largest hp ever seen on an enemy warrior bounds the level
     - deaths:        a warrior standing in a zone that also holds an ENEMY
                      warrior cannot move (referee: apply_move_step).  So if
                      enemy E shared zone X with one of our warriors at the end
                      of turn t, E is still in X on the morning of t+1; when X
                      is visible at the end of t+1 and E is gone, E died.
                      Every combat death of an enemy happens in a zone that
                      holds our warrior or building, i.e. inside our vision.
   Hunger deaths are invisible -> the alive estimate is an upper-ish bound.  */
#define EI_MAX_ID 10000
static int g_ei_last_seen_turn[EI_MAX_ID];
static int g_ei_last_region[EI_MAX_ID];
static unsigned char g_ei_dead[EI_MAX_ID];
static int g_ei_confirmed_dead = 0;
static int g_ei_max_hp_seen = HQ_LEVELS[1].warrior_hp;
static int g_ei_initialized = 0;

enum { EI_ZONE_MAX_E = 64 };
typedef struct {
  int region;
  int ours_before;   /* our warriors in the zone at the end of the previous turn */
  int turret;        /* our building's turret power in the zone (0 if none) */
  int sealed;        /* no enemy warrior in any adjacent zone -> nobody can join */
  int n;             /* enemies pinned in the zone */
  int num[EI_ZONE_MAX_E];
  int hp[EI_ZONE_MAX_E];
} EiZone;

static void enemy_intel_init(void) {
  if (g_ei_initialized) return;
  for (int i = 0; i < EI_MAX_ID; ++i) {
    g_ei_last_seen_turn[i] = -1;
    g_ei_last_region[i] = -1;
    g_ei_dead[i] = 0;
  }
  g_ei_confirmed_dead = 0;
  g_ei_max_hp_seen = HQ_LEVELS[1].warrior_hp;
  g_ei_initialized = 1;
}

/* call BEFORE the state is overwritten by a new result block: enemies that
   share a zone with one of our warriors right now cannot leave that zone.
   One record per such zone. */
static int enemy_intel_collect_contacts(const GameState *S, const GameMap *M,
                                        EiZone *out, int cap) {
  int cnt = 0;
  for (int i = 0; i < S->warriors.len; ++i) {
    const Warrior *e = &S->warriors.data[i];
    if (e->id.side == S->my_side) continue;
    int x = e->region;
    int z = -1;
    for (int k = 0; k < cnt; ++k) if (out[k].region == x) { z = k; break; }
    if (z < 0) {
      int ours = 0;
      for (int j = 0; j < S->warriors.len; ++j) {
        const Warrior *w = &S->warriors.data[j];
        if (w->id.side == S->my_side && w->region == x) ++ours;
      }
      if (ours == 0) continue;
      if (cnt >= cap) continue;
      z = cnt++;
      out[z].region = x;
      out[z].ours_before = ours;
      out[z].n = 0;
      out[z].turret = 0;
      for (int b = 0; b < S->buildings.len; ++b) {
        const Building *bd = &S->buildings.data[b];
        if (bd->region != x || bd->side != S->my_side) continue;
        out[z].turret = bd->type == BTYPE_HQ ? HQ_LEVELS[bd->level].turret
                                             : BASE_LEVELS[bd->level].turret;
      }
      /* sealed: our warrior at x sees every neighbour of x; if none holds an
         enemy, no enemy can enter x on the next move phase */
      int sealed = 1;
      for (int k = 0; k < M->adj[x].len && sealed; ++k) {
        int v = M->adj[x].data[k];
        for (int j = 0; j < S->warriors.len; ++j) {
          const Warrior *w = &S->warriors.data[j];
          if (w->id.side != S->my_side && w->region == v) { sealed = 0; break; }
        }
      }
      out[z].sealed = sealed;
    }
    if (out[z].n < EI_ZONE_MAX_E) {
      out[z].num[out[z].n] = e->id.num;
      out[z].hp[out[z].n] = e->hp;
      out[z].n++;
    } else {
      out[z].sealed = 0; /* too many to simulate exactly */
    }
  }
  return cnt;
}

/* call AFTER the snapshot has been applied and S->visible recomputed */
static void enemy_intel_after_snapshot(GameState *S, const GameMap *M,
                                       EiZone *zones, int n_zones,
                                       const int *my_arrivals) {
  enemy_intel_init();
  for (int i = 0; i < S->warriors.len; ++i) {
    const Warrior *e = &S->warriors.data[i];
    if (e->id.side == M->my_side) continue;
    if (e->id.num >= 0 && e->id.num < EI_MAX_ID) {
      g_ei_last_seen_turn[e->id.num] = S->turn;
      g_ei_last_region[e->id.num] = e->region;
    }
    if (e->hp > g_ei_max_hp_seen) g_ei_max_hp_seen = e->hp;
  }
  for (int c = 0; c < n_zones; ++c) {
    EiZone *z = &zones[c];
    int x = z->region;
    if (region_visible(S, x)) {
      /* observation: pinned enemy not reported in a visible zone => dead */
      for (int k = 0; k < z->n; ++k) {
        int num = z->num[k];
        if (num < 0 || num >= EI_MAX_ID || g_ei_dead[num]) continue;
        if (g_ei_last_seen_turn[num] == S->turn) continue;
        g_ei_dead[num] = 1;
        ++g_ei_confirmed_dead;
      }
    } else if (z->sealed) {
      /* zone dropped out of vision (our warriors there died), but the fight
         was sealed: exactly (ours_before + our arrivals) warrior attacks plus
         our turret hit the pinned enemies, always the lowest hp / lowest number
         first.  Replay it. */
      int hits = z->ours_before + (my_arrivals ? my_arrivals[x] : 0) + z->turret;
      while (hits-- > 0) {
        int best = -1;
        for (int k = 0; k < z->n; ++k) {
          if (z->hp[k] <= 0) continue;
          if (best < 0 || z->hp[k] < z->hp[best] ||
              (z->hp[k] == z->hp[best] && z->num[k] < z->num[best])) best = k;
        }
        if (best < 0) break;
        z->hp[best]--;
      }
      for (int k = 0; k < z->n; ++k) {
        int num = z->num[k];
        if (z->hp[k] > 0 || num < 0 || num >= EI_MAX_ID || g_ei_dead[num]) continue;
        g_ei_dead[num] = 1;
        ++g_ei_confirmed_dead;
      }
    }
  }
  /* HQ level implied by trained-warrior hp: keep the memory entry at least
     that high (level only ever goes up). */
  int implied = g_ei_max_hp_seen - HQ_LEVELS[1].warrior_hp + 1;
  if (implied > HQ_MAX_LEVEL) implied = HQ_MAX_LEVEL;
  Building *ohq = find_building(S, M->opp_hq);
  if (ohq != NULL && ohq->side != M->my_side && ohq->type == BTYPE_HQ &&
      !region_visible(S, M->opp_hq) && ohq->level < implied) {
    ohq->level = implied;
    ohq->hp = HQ_LEVELS[implied].hp;   /* upgrade refills hp; unknown since */
  }
}

static int enemy_visible_count(const GameState *S) {
  int cnt = 0;
  for (int i = 0; i < S->warriors.len; ++i)
    if (S->warriors.data[i].id.side != S->my_side) ++cnt;
  return cnt;
}

/* best estimate of the enemy's living army: everyone ever trained minus the
   deaths we could confirm, never below what we can see right now */
static int enemy_alive_estimate(const GameState *S) {
  int est = START_WARRIORS + enemy_trained_at_least(S) - g_ei_confirmed_dead;
  int vis = enemy_visible_count(S);
  if (est < vis) est = vis;
  /* Trained warriors we have not seen yet are invisible to the id bound.
     While nobody of ours watches the enemy HQ (the scout is not in place),
     fall back on the mirror prior: on a point-symmetric map assume the enemy
     army is at least as large as ours.  With the HQ in view the id bound is
     exact for everything trained since. */
  int opp_hq = (S->my_side == SIDE_LEFT) ? -1 : 0;
  if (opp_hq < 0) {
    /* my_side LEFT: enemy HQ is the last zone; we do not have N here, so use
       the highest-numbered zone among known enemy buildings of type HQ */
    for (int i = 0; i < S->buildings.len; ++i)
      if (S->buildings.data[i].side != S->my_side && S->buildings.data[i].type == BTYPE_HQ)
        opp_hq = S->buildings.data[i].region;
  }
  if (opp_hq >= 0 && !region_visible(S, opp_hq)) {
    int mine = 0;
    for (int i = 0; i < S->warriors.len; ++i)
      if (S->warriors.data[i].id.side == S->my_side) ++mine;
    if (est < mine) est = mine;
  }
  return est;
}

static int enemy_hq_level_estimate(const GameState *S, const GameMap *M) {
  int lvl = 1;
  const Building *ohq = NULL;
  for (int i = 0; i < S->buildings.len; ++i)
    if (S->buildings.data[i].region == M->opp_hq) { ohq = &S->buildings.data[i]; break; }
  if (ohq != NULL && ohq->side != M->my_side) lvl = ohq->level;
  int implied = g_ei_max_hp_seen - HQ_LEVELS[1].warrior_hp + 1;
  if (implied > HQ_MAX_LEVEL) implied = HQ_MAX_LEVEL;
  return lvl > implied ? lvl : implied;
}

static int enemy_total_hp_estimate(const GameState *S, const GameMap *M) {
  int vis_hp = 0, vis_n = 0;
  for (int i = 0; i < S->warriors.len; ++i) {
    const Warrior *w = &S->warriors.data[i];
    if (w->id.side == M->my_side) continue;
    vis_hp += w->hp; ++vis_n;
  }
  int unseen = enemy_alive_estimate(S) - vis_n;
  if (unseen < 0) unseen = 0;
  return vis_hp + unseen * HQ_LEVELS[enemy_hq_level_estimate(S, M)].warrior_hp;
}

static void parse_init(GameMap *M, GameState *S) {
  memset(M, 0, sizeof(*M));
  memset(S, 0, sizeof(*S));

  {
    int n;
    char **t = tokens(readln(), &n);
    M->my_side = (strcmp(t[1], "LEFT") == 0) ? SIDE_LEFT : SIDE_RIGHT;
  }
  {
    int n;
    char **t = tokens(readln(), &n);
    M->N = atoi(t[0]);
    M->K = atoi(t[1]);
  }
  M->x = (long long *)malloc((size_t)M->N * sizeof(long long));
  M->y = (long long *)malloc((size_t)M->N * sizeof(long long));
  {
    int n;
    char **t = tokens(readln(), &n); /* x_0 x_1 ... x_{N-1} */
    for (int i = 0; i < M->N; ++i)
      M->x[i] = atoll(t[i]);
  }
  {
    int n;
    char **t = tokens(readln(), &n); /* y_0 y_1 ... y_{N-1} */
    for (int i = 0; i < M->N; ++i)
      M->y[i] = atoll(t[i]);
  }
  {
    int n;
    char **t = tokens(readln(), &n); /* K strongholds */
    for (int i = 0; i < n; ++i)
      VEC_PUSH(M->strongholds, atoi(t[i]));
    qsort(M->strongholds.data, (size_t)M->strongholds.len,
          sizeof(int), cmp_int);
  }
  M->adj = (IntVec *)calloc((size_t)M->N, sizeof(IntVec));
  for (int r = 0; r < M->N; ++r) {
    int n;
    char **t = tokens(readln(), &n); /* deg n_1 n_2 ... */
    int deg = atoi(t[0]);
    for (int j = 0; j < deg; ++j)
      VEC_PUSH(M->adj[r], atoi(t[1 + j]));
    qsort(M->adj[r].data, (size_t)M->adj[r].len, sizeof(int), cmp_int);
  }

  M->my_hq = hq_of(M, M->my_side);
  M->opp_hq = hq_of(M, opposite(M->my_side));

  S->gold = START_GOLD;
  S->my_countdown = 5;
  S->opp_countdown = 5;
  Side opp = opposite(M->my_side);
  for (int sfx = 1; sfx <= START_WARRIORS; ++sfx) {
    Warrior w1 = {{M->my_side, sfx}, M->my_hq, HQ_LEVELS[1].warrior_hp,
                  WSTATE_STATIONARY, 0};
    Warrior w2 = {{opp, sfx}, M->opp_hq, HQ_LEVELS[1].warrior_hp,
                  WSTATE_STATIONARY, 0};
    VEC_PUSH(S->warriors, w1);
    VEC_PUSH(S->warriors, w2);
  }
  Building hq_l = {hq_of(M, SIDE_LEFT), SIDE_LEFT, BTYPE_HQ, 1, HQ_LEVELS[1].hp};
  Building hq_r = {hq_of(M, SIDE_RIGHT), SIDE_RIGHT, BTYPE_HQ, 1,
                   HQ_LEVELS[1].hp};
  VEC_PUSH(S->buildings, hq_l);
  VEC_PUSH(S->buildings, hq_r);
  S->turn = 0;
  S->enemy_max_suffix = START_WARRIORS;
  S->my_side = M->my_side;
  enemy_intel_init();
  compute_visibility(S, M);

  printf("OK\n");
  fflush(stdout);
}

static int read_turn_start(int *turn_index) {
  char *line = readln();
  if (strcmp(line, "FINISH") == 0)
    return 0;
  int n;
  char **t = tokens(line, &n);
  *turn_index = atoi(t[2]);
  return 1;
}

static void read_turn_result(GameState *S, const GameMap *M,
                             const Actions *submitted) {
  /* fog intel: enemies pinned by our warriors at the end of the previous turn */
  static EiZone ei_zones[256];
  int n_ei_zones = enemy_intel_collect_contacts(S, M, ei_zones, 256);
  int *my_arrivals = (int *)calloc((size_t)M->N, sizeof(int));
  for (int i = 0; i < submitted->upgrades.len; ++i) {
    int region = submitted->upgrades.data[i];
    Building *b = find_building(S, region);
    if (b == NULL) {
      S->gold -= BASE_LEVELS[1].cost;
      Building nb = make_base(region, M->my_side);
      VEC_PUSH(S->buildings, nb);
    } else if (b->level >= building_max_level(b)) {
      S->gold -= (b->type == BTYPE_HQ) ? HQ_HEAL_COST : BASE_HEAL_COST;
      b->hp = building_current_hp(b);
    } else {
      S->gold -= building_upgrade_cost(b);
      building_apply_upgrade(b);
    }
  }

  for (int i = 0; i < submitted->moves.len; ++i) {
    Move mv = submitted->moves.data[i];
    Building *b = find_building(S, mv.target);
    int cost = (b != NULL && b->side == M->my_side) ? 0 : MOVE_COST;
    S->gold -= cost;
    Warrior *w = find_warrior(S, mv.id);
    if (w != NULL) {
      w->state = WSTATE_MOVING;
      w->target = mv.target;
    }
  }

  S->gold -= TRAIN_COST * submitted->train_n;

  {
    char *line = readln();
    if (strcmp(line, "FINISH") == 0)
      exit(0);
    int n;
    char **t = tokens(line, &n); /* "TURN T" */
    if (n >= 2) S->turn = atoi(t[1]);
  }
  {
    int n;
    char **t = tokens(readln(), &n);
    S->my_countdown = atoi(t[2]);
    S->opp_countdown = atoi(t[4]);
  }
  /* NEXT VISION: the five event sections below list ONLY OUR OWN events.
     They are applied as before (they only ever touched our side anyway); the
     WARRIOR/BUILDING snapshots that follow are authoritative and overwrite
     whatever these left inconsistent. */
  /* UPGRADE (own) */
  {
    int n;
    char **t = tokens(readln(), &n); /* "UPGRADE N" */
    int count = atoi(t[1]);
    for (int i = 0; i < count; ++i) {
      int m;
      char **r = tokens(readln(), &m); /* "<A|B> <region>" */
      Side s = parse_side_char(r[0][0]);
      int region = atoi(r[1]);
      Building *b = find_building(S, region);
      if (b == NULL) {
        Building nb = make_base(region, s);
        VEC_PUSH(S->buildings, nb);
      } else if (b->side != M->my_side) {
        if (b->level >= building_max_level(b))
          b->hp = building_current_hp(b);
        else
          building_apply_upgrade(b);
      }
    }
  }
  /* TRAIN (own) */
  {
    int n;
    char **t = tokens(readln(), &n); /* "TRAIN N" */
    int count = atoi(t[1]);
    if (count > 0) {
      int m;
      char **ids = tokens(readln(), &m);
      for (int i = 0; i < count; ++i) {
        WarriorId id = parse_warrior(ids[i]);
        int hq_region = hq_of(M, id.side);
        Building *hq_b = find_building(S, hq_region);
        int hq_level = (hq_b != NULL) ? hq_b->level : 1;
        Warrior w = {id, hq_region, HQ_LEVELS[hq_level].warrior_hp,
                     WSTATE_STATIONARY, 0};
        if (find_warrior(S, id) == NULL)
          VEC_PUSH(S->warriors, w);
      }
    }
  }
  /* MOVE (own) */
  {
    int n;
    char **t = tokens(readln(), &n); /* "MOVE N" */
    int count = atoi(t[1]);
    for (int i = 0; i < count; ++i) {
      int m;
      char **r = tokens(readln(), &m);
      WarriorId id = parse_warrior(r[0]);
      int region = atoi(r[1]);
      if (region >= 0 && region < M->N) ++my_arrivals[region];
      Warrior *w = find_warrior(S, id);
      if (w != NULL) {
        w->region = region;
        if (id.side == M->my_side && w->state == WSTATE_MOVING &&
            w->region == w->target)
          w->state = WSTATE_STATIONARY;
      }
    }
  }
  /* DAMAGE (own) */
  {
    int n;
    char **t = tokens(readln(), &n); /* "DAMAGE N" */
    int count = atoi(t[1]);
    for (int i = 0; i < count; ++i) {
      int m;
      char **r = tokens(readln(), &m);
      WarriorId id = parse_warrior(r[1]);
      int damage = atoi(r[2]);
      Warrior *w = find_warrior(S, id);
      if (w != NULL)
        w->hp -= damage;
    }
  }
  /* SIEGE (own) */
  {
    int n;
    char **t = tokens(readln(), &n); /* "SIEGE N" */
    int count = atoi(t[1]);
    for (int i = 0; i < count; ++i) {
      int m;
      char **r = tokens(readln(), &m);
      int region = atoi(r[1]);
      int damage = atoi(r[2]);
      Building *b = find_building(S, region);
      if (b != NULL)
        b->hp -= damage;
    }
  }
  /* WARRIOR / BUILDING snapshots (everything inside our vision, end of turn) */
  WarriorVec wsnap;
  BuildingVec bsnap;
  memset(&wsnap, 0, sizeof(wsnap));
  memset(&bsnap, 0, sizeof(bsnap));
  {
    int n;
    char **t = tokens(readln(), &n); /* "WARRIOR W" */
    int count = atoi(t[1]);
    for (int i = 0; i < count; ++i) {
      int m;
      char **r = tokens(readln(), &m); /* "<id> <region> <hp>" */
      Warrior w = {parse_warrior(r[0]), atoi(r[1]), atoi(r[2]),
                   WSTATE_STATIONARY, 0};
      VEC_PUSH(wsnap, w);
      note_enemy_suffix(S, M, w.id);
    }
  }
  {
    int n;
    char **t = tokens(readln(), &n); /* "BUILDING B" */
    int count = atoi(t[1]);
    for (int i = 0; i < count; ++i) {
      int m;
      char **r = tokens(readln(), &m); /* "<side> <region> <HQ|BASE> <level> <hp>" */
      Building b;
      b.side = parse_side_char(r[0][0]);
      b.region = atoi(r[1]);
      b.type = (strcmp(r[2], "HQ") == 0) ? BTYPE_HQ : BTYPE_BASE;
      b.level = atoi(r[3]);
      b.hp = atoi(r[4]);
      b.last_seen_turn = S->turn;
      VEC_PUSH(bsnap, b);
    }
  }
  (void)readln(); /* "END" */

  /* --- warriors: own = reconciled with snapshot (dead ones vanish);
         enemy = exactly the visible ones (movement state is unknown) --- */
  {
    int kept = 0;
    for (int i = 0; i < S->warriors.len; ++i) {
      Warrior w = S->warriors.data[i];
      if (w.id.side != M->my_side) continue; /* rebuilt from snapshot */
      int found = 0;
      for (int j = 0; j < wsnap.len; ++j) {
        if (!wid_eq(wsnap.data[j].id, w.id)) continue;
        w.region = wsnap.data[j].region;
        w.hp = wsnap.data[j].hp;
        if (w.state == WSTATE_MOVING && w.region == w.target)
          w.state = WSTATE_STATIONARY;
        found = 1;
        break;
      }
      if (found && w.hp > 0)
        S->warriors.data[kept++] = w;
    }
    S->warriors.len = kept;
    for (int j = 0; j < wsnap.len; ++j) {
      const Warrior *sw = &wsnap.data[j];
      if (sw->hp <= 0) continue;
      if (sw->id.side == M->my_side) {
        if (find_warrior(S, sw->id) == NULL) /* should not happen; be safe */
          VEC_PUSH(S->warriors, *sw);
      } else {
        VEC_PUSH(S->warriors, *sw);
      }
    }
  }

  /* --- own buildings: snapshot is authoritative --- */
  {
    int kept = 0;
    for (int i = 0; i < S->buildings.len; ++i) {
      Building b = S->buildings.data[i];
      if (b.side != M->my_side) { S->buildings.data[kept++] = b; continue; }
      int found = 0;
      for (int j = 0; j < bsnap.len; ++j) {
        const Building *sb = &bsnap.data[j];
        if (sb->side != M->my_side || sb->region != b.region) continue;
        b.type = sb->type; b.level = sb->level; b.hp = sb->hp;
        b.last_seen_turn = S->turn;
        found = 1;
        break;
      }
      if (found && b.hp > 0)
        S->buildings.data[kept++] = b;
    }
    S->buildings.len = kept;
    for (int j = 0; j < bsnap.len; ++j) {
      const Building *sb = &bsnap.data[j];
      if (sb->side != M->my_side || sb->hp <= 0) continue;
      if (find_building(S, sb->region) == NULL)
        VEC_PUSH(S->buildings, *sb);
    }
  }

  /* vision is a function of our own units at end of turn */
  compute_visibility(S, M);

  /* --- enemy buildings: memory.  Visible zone + not reported => gone.
         Not visible => keep the last-known entry.  Enemy HQ is never dropped
         (if it were destroyed the game would already be over). --- */
  {
    int kept = 0;
    for (int i = 0; i < S->buildings.len; ++i) {
      Building b = S->buildings.data[i];
      if (b.side == M->my_side) { S->buildings.data[kept++] = b; continue; }
      int found = 0;
      for (int j = 0; j < bsnap.len; ++j) {
        const Building *sb = &bsnap.data[j];
        if (sb->side == M->my_side || sb->region != b.region) continue;
        b.type = sb->type; b.level = sb->level; b.hp = sb->hp;
        b.last_seen_turn = S->turn;
        found = 1;
        break;
      }
      if (found) {
        if (b.hp > 0 || b.type == BTYPE_HQ) S->buildings.data[kept++] = b;
      } else if (!region_visible(S, b.region) || b.type == BTYPE_HQ) {
        S->buildings.data[kept++] = b; /* stale but unknown: keep */
      }
      /* else: visible zone, no building reported => destroyed */
    }
    S->buildings.len = kept;
    for (int j = 0; j < bsnap.len; ++j) {
      const Building *sb = &bsnap.data[j];
      if (sb->side == M->my_side) continue;
      if (find_building(S, sb->region) == NULL)
        VEC_PUSH(S->buildings, *sb);
    }
  }
  free(wsnap.data);
  free(bsnap.data);
  enemy_intel_after_snapshot(S, M, ei_zones, n_ei_zones, my_arrivals);
  free(my_arrivals);

  int income = 0;
  for (int bi = 0; bi < S->buildings.len; ++bi) {
    const Building *b = &S->buildings.data[bi];
    if (b->side != M->my_side)
      continue;
    int count = 0;
    for (int wi = 0; wi < S->warriors.len; ++wi) {
      const Warrior *w = &S->warriors.data[wi];
      if (w->id.side == M->my_side && w->region == b->region)
        ++count;
    }
    int cap = building_work_cap(b);
    income += WORK_INCOME * (count < cap ? count : cap);
  }
  S->gold += income;

  int alive = 0;
  for (int wi = 0; wi < S->warriors.len; ++wi)
    if (S->warriors.data[wi].id.side == M->my_side)
      ++alive;
  S->gold -= UPKEEP_PER_WARRIOR * alive;
  if (S->gold < 0)
    S->gold = 0;
}

typedef struct {
  int N;
  double **dist;
  int **nxt;
} Paths;

static double euclid_ceil(const GameMap *M, int u, int v) {
  double dx = (double)(M->x[u] - M->x[v]);
  double dy = (double)(M->y[u] - M->y[v]);
  return ceil(sqrt(dx * dx + dy * dy));
}

static Paths calculate_paths(const GameMap *M) {
  int N = M->N;
  Paths P;
  P.N = N;
  P.dist = (double **)malloc((size_t)N * sizeof(double *));
  P.nxt = (int **)malloc((size_t)N * sizeof(int *));
  for (int i = 0; i < N; ++i) {
    P.dist[i] = (double *)malloc((size_t)N * sizeof(double));
    P.nxt[i] = (int *)malloc((size_t)N * sizeof(int));
    for (int j = 0; j < N; ++j) {
      P.dist[i][j] = INFINITY;
      P.nxt[i][j] = -1;
    }
    P.dist[i][i] = 0.0;
    P.nxt[i][i] = i;
  }
  for (int u = 0; u < N; ++u) {
    for (int k = 0; k < M->adj[u].len; ++k) {
      int v = M->adj[u].data[k];
      double w = euclid_ceil(M, u, v);
      if (w < P.dist[u][v])
        P.dist[u][v] = w;
    }
  }

  for (int k = 0; k < N; ++k) {
    for (int u = 0; u < N; ++u) {
      if (isinf(P.dist[u][k]))
        continue;
      for (int v = 0; v < N; ++v) {
        double cand = P.dist[u][k] + P.dist[k][v];
        if (cand < P.dist[u][v])
          P.dist[u][v] = cand;
      }
    }
  }

  for (int u = 0; u < N; ++u) {
    for (int v = 0; v < N; ++v) {
      if (u == v || isinf(P.dist[u][v]))
        continue;
      double best_score = INFINITY;
      for (int k = 0; k < M->adj[u].len; ++k) {
        int nb = M->adj[u].data[k];
        if (isinf(P.dist[nb][v]))
          continue;
        double score = euclid_ceil(M, u, nb) + P.dist[nb][v];
        if (score < best_score) {
          best_score = score;
          P.nxt[u][v] = nb;
        }
      }
    }
  }
  return P;
}

/*
 * Returns the next step on the path from u to v.
 * If the path is not reachable, returns -1.
 */
static MAYBE_UNUSED int next_step(const Paths *P, int u, int v) { return P->nxt[u][v]; }

/*
 * Returns the path from u to v.
 * If the path is not reachable, returns an empty path.
 */
static MAYBE_UNUSED int path(const Paths *P, int u, int v, int *out) {
  if (P->nxt[u][v] == -1)
    return 0;
  int len = 0;
  out[len++] = u;
  while (u != v) {
    u = P->nxt[u][v];
    out[len++] = u;
  }
  return len;
}

static char *format_warrior(WarriorId id) {
  static char buf[16];
  snprintf(buf, sizeof(buf), "%c%d", side_char(id.side), id.num);
  return buf;
}

static void emit_actions(const Actions *a) {
  printf("COMMAND\n");
  for (int i = 0; i < a->moves.len; ++i)
    printf("MOVE %s %d\n", format_warrior(a->moves.data[i].id),
           a->moves.data[i].target);
  for (int i = 0; i < a->upgrades.len; ++i)
    printf("UPGRADE %d\n", a->upgrades.data[i]);
  if (a->train_n > 0)
    printf("TRAIN %d\n", a->train_n);
  printf("END\n");
  fflush(stdout);
}


/*//////////////////////////////////////////////////////////////////////////////
//// v5 MACRO STRATEGY (NEXT VISION, 2026-08-29) ////////////////////////////////
////
//// Written from the four website logs (2.txt..5.txt) in which the qualifier
//// bot lost 0-4.  The winner's recipe was always the same and it is what this
//// file does:
////   1. claim a new stronghold every ~10 turns, one worker each, nearest first
////   2. fill every work slot (that is the whole income), upgrade bases/HQ
////   3. keep the HQ safe with an exact-ish threat comparison, evacuate workers
////   4. once the economy is up, mass an army at a forward base and attack
////      known enemy bases (then their HQ) as ONE stack, never in dribbles
////   5. scout the enemy HQ so the fog estimates are exact
//// Moves to our own buildings are free - all worker/army shuffling uses that.
//////////////////////////////////////////////////////////////////////////////*/

static int min_int(int a, int b) { return a < b ? a : b; }
static int max_int(int a, int b) { return a > b ? a : b; }
static const int INF_HOPS = 1000000000;

static const Warrior *find_warrior_const(const GameState *S, WarriorId id) {
  for (int i = 0; i < S->warriors.len; ++i)
    if (wid_eq(S->warriors.data[i].id, id)) return &S->warriors.data[i];
  return NULL;
}
static const Building *find_building_const(const GameState *S, int region) {
  for (int i = 0; i < S->buildings.len; ++i)
    if (S->buildings.data[i].region == region) return &S->buildings.data[i];
  return NULL;
}
static int is_stronghold(const GameMap *M, int region) {
  int l = 0, r = M->strongholds.len - 1;
  while (l <= r) {
    int m = (l + r) >> 1, v = M->strongholds.data[m];
    if (v == region) return 1;
    if (v < region) l = m + 1; else r = m - 1;
  }
  return 0;
}
static int building_turret(const Building *b) {
  return b->type == BTYPE_HQ ? HQ_LEVELS[b->level].turret : BASE_LEVELS[b->level].turret;
}
static int action_has_move_warrior(const Actions *a, WarriorId id) {
  for (int i = 0; i < a->moves.len; ++i)
    if (wid_eq(a->moves.data[i].id, id)) return 1;
  return 0;
}
static int action_has_upgrade(const Actions *a, int region) {
  for (int i = 0; i < a->upgrades.len; ++i)
    if (a->upgrades.data[i] == region) return 1;
  return 0;
}
static void push_move(Actions *a, WarriorId id, int target) {
  Move mv = {id, target};
  VEC_PUSH(a->moves, mv);
}
/* engine walk length (turns) from u to v */
static int walk_len(const Paths *P, int u, int v) {
  if (u < 0 || v < 0 || u >= P->N || v >= P->N || P->nxt[u][v] < 0) return INF_HOPS;
  int hops = 0, cur = u;
  while (cur != v && hops <= P->N + 5) { cur = P->nxt[cur][v]; if (cur < 0) return INF_HOPS; ++hops; }
  return hops;
}
static void bfs_hops_from(const GameMap *M, int src, int *out) {
  for (int r = 0; r < M->N; ++r) out[r] = -1;
  int *q = (int *)malloc((size_t)M->N * sizeof(int));
  int h = 0, t = 0;
  q[t++] = src; out[src] = 0;
  while (h < t) {
    int u = q[h++];
    for (int k = 0; k < M->adj[u].len; ++k) {
      int v = M->adj[u].data[k];
      if (out[v] < 0) { out[v] = out[u] + 1; q[t++] = v; }
    }
  }
  free(q);
}
static int adjacent(const GameMap *M, int u, int v) {
  for (int k = 0; k < M->adj[u].len; ++k) if (M->adj[u].data[k] == v) return 1;
  return 0;
}

/* ---------------------------------------------------------------- tunables */
#ifndef V5_HOME_GUARD_EARLY
#define V5_HOME_GUARD_EARLY 1        /* extra bodies kept at HQ before turn 100 */
#endif
#ifndef V5_HOME_GUARD_LATE
#define V5_HOME_GUARD_LATE 2
#endif
#ifndef V5_ARMY_TRAIN_START
#define V5_ARMY_TRAIN_START 90       /* before: train only workers/claimers */
#endif
#ifndef V5_ATTACK_MIN_STACK
#define V5_ATTACK_MIN_STACK 16
#endif
#ifndef V5_HQ_ATTACK_MIN_STACK
#define V5_HQ_ATTACK_MIN_STACK 22
#endif
#ifndef V5_MAX_CLAIMS_IN_FLIGHT
#define V5_MAX_CLAIMS_IN_FLIGHT 2
#endif
#ifndef V5_CONTESTED_HOPS
#define V5_CONTESTED_HOPS 2          /* stronghold within this of an enemy building = contested */
#endif
#ifndef V5_SCOUT_MIN_ARMY
#define V5_SCOUT_MIN_ARMY 4
#endif
#ifndef V5_SCOUT_MAX_DEATHS_PER_100
#define V5_SCOUT_MAX_DEATHS_PER_100 3
#endif
#ifndef V5_DEBUG
#define V5_DEBUG 0
#endif
#define DBG(...) do { if (V5_DEBUG) { fprintf(stderr, __VA_ARGS__); } } while (0)

/* ---------------------------------------------------------------- per-turn context */
enum { ZMAX = 256, IDMAX = EI_MAX_ID };
static int g_hop_my[ZMAX], g_hop_opp[ZMAX];
static int g_my_stat[ZMAX], g_my_any[ZMAX], g_my_hp[ZMAX], g_my_arriving[ZMAX];
static int g_en_cnt[ZMAX], g_en_hp[ZMAX], g_en_near2_hp[ZMAX];
static int g_ctx_ready = 0;
static int g_claim_of[IDMAX];        /* warrior number -> stronghold it is claiming, -1 */
static int g_role_scout = -1;        /* scout warrior number */
static int g_scout_post = -1, g_scout_planned = -1, g_scout_last_death_post = -1;
static int g_scout_next_dispatch = 0, g_scout_deaths = 0, g_scout_death_turns[64];
static IntVec g_scout_cands;
static int g_attack_target = -1;     /* current army objective (zone) or -1 */
static int g_role_sentinel = -1;     /* v7: warrior sitting on the centre stronghold */
static int g_centre = -1;
static int g_sentinel_block = -1;    /* stronghold issue_builds must leave alone this turn */
static int g_attack_launch_turn = -1000;
static int g_launch_reserve = 0;     /* gold being saved to launch a ready stack */
static int g_upgrade_reserve = 0;    /* gold being saved for the next wanted upgrade */

static void ctx_init(const GameMap *M) {
  if (g_ctx_ready) return;
  g_ctx_ready = 1;
  bfs_hops_from(M, M->my_hq, g_hop_my);
  bfs_hops_from(M, M->opp_hq, g_hop_opp);
  for (int i = 0; i < IDMAX; ++i) g_claim_of[i] = -1;
}

static void ctx_scan(const GameState *S, const GameMap *M) {
  for (int r = 0; r < M->N; ++r) {
    g_my_stat[r] = g_my_any[r] = g_my_hp[r] = g_my_arriving[r] = 0;
    g_en_cnt[r] = g_en_hp[r] = g_en_near2_hp[r] = 0;
  }
  for (int i = 0; i < S->warriors.len; ++i) {
    const Warrior *w = &S->warriors.data[i];
    if (w->region < 0 || w->region >= M->N) continue;
    if (w->id.side == M->my_side) {
      g_my_any[w->region]++; g_my_hp[w->region] += w->hp;
      if (w->state == WSTATE_STATIONARY) g_my_stat[w->region]++;
      else if (w->target >= 0 && w->target < M->N) g_my_arriving[w->target]++;
    } else {
      g_en_cnt[w->region]++; g_en_hp[w->region] += w->hp;
    }
  }
  /* enemy hp within 2 hops of each zone (for threat checks) */
  for (int r = 0; r < M->N; ++r) {
    if (g_en_cnt[r] == 0) continue;
    /* r itself */
    g_en_near2_hp[r] += g_en_hp[r];
    for (int k = 0; k < M->adj[r].len; ++k) {
      int v = M->adj[r].data[k];
      g_en_near2_hp[v] += g_en_hp[r];
      for (int j = 0; j < M->adj[v].len; ++j) {
        int u = M->adj[v].data[j];
        if (u == r || adjacent(M, u, r)) continue;
        g_en_near2_hp[u] += g_en_hp[r];   /* may double count via two paths: fine (conservative) */
      }
    }
  }
}

static int my_income(const GameState *S, const GameMap *M) {
  int inc = 0;
  for (int i = 0; i < S->buildings.len; ++i) {
    const Building *b = &S->buildings.data[i];
    if (b->side != M->my_side) continue;
    inc += WORK_INCOME * min_int(g_my_stat[b->region], building_work_cap(b));
  }
  return inc;
}
static int my_alive(const GameState *S, const GameMap *M) {
  int n = 0;
  for (int i = 0; i < S->warriors.len; ++i) if (S->warriors.data[i].id.side == M->my_side) ++n;
  return n;
}
static int my_base_count(const GameState *S, const GameMap *M) {
  int n = 0;
  for (int i = 0; i < S->buildings.len; ++i)
    if (S->buildings.data[i].side == M->my_side && S->buildings.data[i].type == BTYPE_BASE) ++n;
  return n;
}
static int my_hq_level(const GameState *S, const GameMap *M) {
  const Building *b = find_building_const(S, M->my_hq);
  return (b && b->side == M->my_side) ? b->level : 1;
}

/* ---------------------------------------------------------------- combat sim
   One zone.  Side A attacks side B which may own a building (hp/turret) there.
   Returns 1 if A kills every B warrior (and the building if present) within
   max_days, 0 otherwise.  *a_hp_left receives A's remaining total hp. */
static int sim_zone(const int *a_hp0, int an, int a_turret,
                    const int *b_hp0, int bn, int b_turret, int b_bld_hp,
                    int max_days, int *a_hp_left) {
  int ahp[512], bhp[512];
  an = min_int(an, 512); bn = min_int(bn, 512);
  for (int i = 0; i < an; ++i) ahp[i] = a_hp0[i];
  for (int i = 0; i < bn; ++i) bhp[i] = b_hp0[i];
  int bld = b_bld_hp;
  for (int day = 0; day < max_days; ++day) {
    int a_cnt = 0, b_cnt = 0;
    for (int i = 0; i < an; ++i) if (ahp[i] > 0) ++a_cnt;
    for (int i = 0; i < bn; ++i) if (bhp[i] > 0) ++b_cnt;
    if (a_cnt == 0) break;
    if (b_cnt == 0 && bld <= 0) { int s = 0; for (int i = 0; i < an; ++i) if (ahp[i] > 0) s += ahp[i]; if (a_hp_left) *a_hp_left = s; return 1; }
    /* turrets first */
    for (int t = 0; t < a_turret; ++t) { int w = -1; for (int i = 0; i < bn; ++i) if (bhp[i] > 0 && (w < 0 || bhp[i] < bhp[w])) w = i; if (w >= 0) bhp[w]--; }
    for (int t = 0; t < b_turret; ++t) { int w = -1; for (int i = 0; i < an; ++i) if (ahp[i] > 0 && (w < 0 || ahp[i] < ahp[w])) w = i; if (w >= 0) ahp[w]--; }
    /* warriors: counts include units zeroed by turrets this day */
    for (int t = 0; t < a_cnt; ++t) {
      int w = -1; for (int i = 0; i < bn; ++i) if (bhp[i] > 0 && (w < 0 || bhp[i] < bhp[w])) w = i;
      if (w >= 0) bhp[w]--; else if (bld > 0) bld--;
    }
    for (int t = 0; t < b_cnt; ++t) {
      int w = -1; for (int i = 0; i < an; ++i) if (ahp[i] > 0 && (w < 0 || ahp[i] < ahp[w])) w = i;
      if (w >= 0) ahp[w]--;
    }
  }
  int s = 0; for (int i = 0; i < an; ++i) if (ahp[i] > 0) s += ahp[i];
  if (a_hp_left) *a_hp_left = s;
  int b_alive = 0; for (int i = 0; i < bn; ++i) if (bhp[i] > 0) ++b_alive;
  return (b_alive == 0 && bld <= 0) ? 1 : 0;
}

/* gather hp lists */
static int collect_hp(const GameState *S, Side side, int region, int only_stationary, int *out, int cap) {
  int n = 0;
  for (int i = 0; i < S->warriors.len && n < cap; ++i) {
    const Warrior *w = &S->warriors.data[i];
    if (w->id.side != side || w->region != region) continue;
    if (only_stationary && w->state != WSTATE_STATIONARY) continue;
    out[n++] = w->hp;
  }
  return n;
}

/* ---------------------------------------------------------------- scout */
static void scout_prepare(const GameMap *M, const Paths *P) {
  if (g_scout_cands.len > 0 || g_scout_cands.cap > 0) return;
  int N = M->N;
  unsigned char *onpath = (unsigned char *)calloc((size_t)N, 1);
  for (int i = 0; i <= M->strongholds.len; ++i) {
    int t = (i < M->strongholds.len) ? M->strongholds.data[i] : M->my_hq;
    int u = M->opp_hq, guard = 0;
    while (u != t && u >= 0 && guard++ < N) { onpath[u] = 1; u = P->nxt[u][t]; }
  }
  typedef struct { int r; long long score; } Cand;
  Cand c[ZMAX]; int nc = 0;
  for (int pass = 0; pass < 2 && nc == 0; ++pass) {
    for (int r = 0; r < N; ++r) {
      if (g_hop_opp[r] != VISION_HOPS) continue;
      if (is_stronghold(M, r) || r == M->my_hq || r == M->opp_hq) continue;
      if (pass == 0 && onpath[r]) continue;
      int bad_nb = 0, sh_nb = 0;
      for (int k = 0; k < M->adj[r].len; ++k) {
        if (onpath[M->adj[r].data[k]]) ++bad_nb;
        if (is_stronghold(M, M->adj[r].data[k])) ++sh_nb;
      }
      c[nc].r = r;
      c[nc].score = (pass ? 1000000 : 0) + sh_nb * 100000 + bad_nb * 10000 + g_hop_my[r] * 10 + M->adj[r].len;
      ++nc;
    }
  }
  for (int i = 0; i < nc; ++i)
    for (int j = i; j > 0 && c[j].score < c[j - 1].score; --j) { Cand t = c[j]; c[j] = c[j - 1]; c[j - 1] = t; }
  for (int i = 0; i < nc; ++i) VEC_PUSH(g_scout_cands, c[i].r);
  if (g_scout_cands.cap == 0) { g_scout_cands.cap = 1; g_scout_cands.data = (int *)malloc(sizeof(int)); }
  free(onpath);
  DBG("SCOUT candidates:"); for (int i = 0; i < g_scout_cands.len; ++i) DBG(" %d", g_scout_cands.data[i]); DBG("\n");
}

/* zones a lone scout must never step on: any stronghold that is not ours
   (a turret may be there), any known enemy building, any zone holding or
   adjacent to a visible enemy warrior */
static int scout_zone_forbidden(const GameState *S, const GameMap *M, int z) {
  const Building *b = find_building_const(S, z);
  if (b != NULL && b->side != M->my_side) return 1;
  if (is_stronghold(M, z) && (b == NULL || b->side != M->my_side)) return 1;
  if (g_en_cnt[z] > 0) return 1;
  for (int k = 0; k < M->adj[z].len; ++k) if (g_en_cnt[M->adj[z].data[k]] > 0) return 1;
  return 0;
}
static int scout_zone_threatened(const GameState *S, const GameMap *M, int z) {
  (void)S;
  if (g_en_cnt[z] > 0) return 1;
  for (int k = 0; k < M->adj[z].len; ++k) if (g_en_cnt[M->adj[z].data[k]] > 0) return 1;
  return 0;
}
/* next MOVE target on a safe route from `from` to `dest`: BFS over allowed
   zones, then the farthest waypoint whose ENGINE path stays on allowed zones.
   Returns -1 if no safe route exists. */
static int scout_next_leg(const GameState *S, const GameMap *M, const Paths *P, int from, int dest) {
  int N = M->N;
  static int prev[ZMAX], dist[ZMAX], q[ZMAX];
  static unsigned char ok[ZMAX];
  for (int r = 0; r < N; ++r) { ok[r] = !scout_zone_forbidden(S, M, r); dist[r] = -1; prev[r] = -1; }
  ok[from] = 1; ok[dest] = 1;
  int h = 0, t = 0; q[t++] = from; dist[from] = 0;
  while (h < t) {
    int u = q[h++];
    if (u == dest) break;
    for (int k = 0; k < M->adj[u].len; ++k) {
      int v = M->adj[u].data[k];
      if (!ok[v] || dist[v] >= 0) continue;
      dist[v] = dist[u] + 1; prev[v] = u; q[t++] = v;
    }
  }
  if (dist[dest] < 0) return -1;
  int path[ZMAX], len = 0;
  for (int u = dest; u >= 0; u = prev[u]) path[len++] = u;   /* dest .. from */
  /* farthest waypoint w (walking back from dest) such that engine path from->w uses only ok zones */
  for (int i = 0; i < len - 1; ++i) {
    int w = path[i];
    int cur = from, good = 1, guard = 0;
    while (cur != w && guard++ < N) { cur = P->nxt[cur][w]; if (cur < 0 || !ok[cur]) { good = 0; break; } }
    if (good) return w;
  }
  return -1;
}
static int scout_pick_post(const GameState *S, const GameMap *M, int avoid) {
  for (int i = 0; i < g_scout_cands.len; ++i) {
    int r = g_scout_cands.data[i];
    if (r == avoid && g_scout_cands.len > 1) continue;
    if (scout_zone_forbidden(S, M, r)) continue;
    return r;
  }
  return -1;
}
static void issue_scout(Actions *a, const GameState *S, const GameMap *M, const Paths *P,
                        int *budget, int turn, int hq_threatened) {
  g_scout_planned = -1;
  scout_prepare(M, P);
  if (g_scout_cands.len == 0) return;
  WarriorId sid = {M->my_side, g_role_scout};
  const Warrior *sc = g_role_scout >= 0 ? find_warrior_const(S, sid) : NULL;
  if (g_role_scout >= 0 && sc == NULL) {
    if (g_scout_deaths < 64) g_scout_death_turns[g_scout_deaths] = turn;
    ++g_scout_deaths;
    g_scout_last_death_post = g_scout_post;
    g_role_scout = -1; g_scout_post = -1;
    g_scout_next_dispatch = turn + 2;
    int recent = 0;
    for (int i = 0; i < g_scout_deaths && i < 64; ++i) if (turn - g_scout_death_turns[i] <= 100) ++recent;
    if (recent >= V5_SCOUT_MAX_DEATHS_PER_100) g_scout_next_dispatch = turn + 60;
    DBG("SCOUT t=%d died (post %d) deaths=%d next=%d\n", turn, g_scout_last_death_post, g_scout_deaths, g_scout_next_dispatch);
  }
  if (sc == NULL) {
    if (hq_threatened || turn < g_scout_next_dispatch || *budget < MOVE_COST) return;
    if (my_alive(S, M) < V5_SCOUT_MIN_ARMY) return;
    int post = scout_pick_post(S, M, g_scout_last_death_post);
    if (post < 0) return;
    /* newest stationary spare body at the HQ (never the last one) */
    const Warrior *best = NULL;
    for (int i = 0; i < S->warriors.len; ++i) {
      const Warrior *w = &S->warriors.data[i];
      if (w->id.side != M->my_side || w->state != WSTATE_STATIONARY || w->region != M->my_hq) continue;
      if (action_has_move_warrior(a, w->id) || g_claim_of[w->id.num] >= 0) continue;
      if (g_my_stat[M->my_hq] < 2) continue;
      if (best == NULL || w->id.num > best->id.num) best = w;
    }
    if (best == NULL) return;
    int leg = scout_next_leg(S, M, P, best->region, post);
    if (leg < 0) return;
    g_role_scout = best->id.num; g_scout_post = post;
    push_move(a, best->id, leg); *budget -= MOVE_COST; g_scout_planned = leg;
    DBG("SCOUT t=%d dispatch %c%d -> post %d via %d\n", turn, side_char(best->id.side), best->id.num, post, leg);
    return;
  }
  if (sc->state != WSTATE_STATIONARY) return;
  int here = sc->region;
  if (scout_zone_threatened(S, M, here)) {
    if (*budget < MOVE_COST) return;
    int to = scout_pick_post(S, M, here);
    if (to >= 0 && to != here) {
      int leg = scout_next_leg(S, M, P, here, to);
      if (leg >= 0) { push_move(a, sc->id, leg); *budget -= MOVE_COST; g_scout_planned = leg; g_scout_post = to; DBG("SCOUT t=%d kite %d -> %d via %d\n", turn, here, to, leg); return; }
    }
    int bestv = -1;
    for (int k = 0; k < M->adj[here].len; ++k) {
      int v = M->adj[here].data[k];
      if (scout_zone_threatened(S, M, v)) continue;
      const Building *b = find_building_const(S, v);
      if (b != NULL && b->side != M->my_side) continue;
      if (is_stronghold(M, v) && (b == NULL || b->side != M->my_side)) continue;
      if (bestv < 0 || g_hop_opp[v] > g_hop_opp[bestv]) bestv = v;
    }
    if (bestv >= 0) { push_move(a, sc->id, bestv); *budget -= MOVE_COST; g_scout_planned = bestv; DBG("SCOUT t=%d flee %d -> %d\n", turn, here, bestv); }
    return;
  }
  if (here != g_scout_post) {
    if (*budget < MOVE_COST) return;
    if (scout_zone_forbidden(S, M, g_scout_post)) {
      int alt = scout_pick_post(S, M, g_scout_post);
      if (alt < 0) return;
      g_scout_post = alt;
    }
    int leg = scout_next_leg(S, M, P, here, g_scout_post);
    if (leg < 0 || leg == here) return;
    push_move(a, sc->id, leg); *budget -= MOVE_COST; g_scout_planned = leg;
  }
}

/* ---------------------------------------------------------------- roles */
static int warrior_is_reserved(const Warrior *w) {
  return w->id.num == g_role_scout || w->id.num == g_role_sentinel || g_claim_of[w->id.num] >= 0;
}

/* stationary, unassigned, not moving this turn */
static int warrior_free(const Actions *a, const Warrior *w, const GameMap *M) {
  if (w->id.side != M->my_side || w->state != WSTATE_STATIONARY) return 0;
  if (action_has_move_warrior(a, w->id)) return 0;
  if (warrior_is_reserved(w)) return 0;
  return 1;
}

/* how many stationary free warriors stand at region (after already-issued moves) */
static int free_count_at(const GameState *S, const GameMap *M, const Actions *a, int region) {
  int n = 0;
  for (int i = 0; i < S->warriors.len; ++i) {
    const Warrior *w = &S->warriors.data[i];
    if (w->region == region && warrior_free(a, w, M)) ++n;
  }
  return n;
}

/* pull up to `need` free warriors from `from` (keeping `keep` there) to `to`.
   Returns number moved. Prefers lowest hp bodies for worker duty (hp irrelevant
   for work), highest hp for army duty (army=1). */
static int pull_from(Actions *a, const GameState *S, const GameMap *M, int from, int to,
                     int need, int keep, int army, int *budget) {
  int moved = 0;
  const Building *tb = find_building_const(S, to);
  int cost = (tb != NULL && tb->side == M->my_side) ? 0 : MOVE_COST;
  while (moved < need) {
    if (free_count_at(S, M, a, from) <= keep) break;
    if (*budget < cost) break;
    const Warrior *best = NULL;
    for (int i = 0; i < S->warriors.len; ++i) {
      const Warrior *w = &S->warriors.data[i];
      if (w->region != from || !warrior_free(a, w, M)) continue;
      if (best == NULL || (army ? w->hp > best->hp : w->hp < best->hp)) best = w;
    }
    if (best == NULL) break;
    push_move(a, best->id, to); *budget -= cost; ++moved;
  }
  return moved;
}

/* ---------------------------------------------------------------- defense */
static int hq_defense(Actions *a, const GameState *S, const GameMap *M, const Paths *P, int *budget, int turn) {
  (void)turn;
  const Building *hq = find_building_const(S, M->my_hq);
  if (hq == NULL) return 0;
  /* enemy strength that can reach the HQ within 3 turns */
  int en_hp = 0, en_n = 0;
  int ehp[512]; int en = 0;
  for (int i = 0; i < S->warriors.len; ++i) {
    const Warrior *w = &S->warriors.data[i];
    if (w->id.side == M->my_side) continue;
    if (g_hop_my[w->region] > 8) continue;
    en_hp += w->hp; ++en_n; if (en < 512) ehp[en++] = w->hp;
  }
  if (en_n == 0) return 0;
  int mhp[512]; int mn = collect_hp(S, M->my_side, M->my_hq, 0, mhp, 512);
  int left = 0;
  int hold = sim_zone(mhp, mn, building_turret(hq), ehp, en, 0, 0, 30, &left);
  /* need a margin: hold with at least 30% hp left */
  int my_hp_total = 0; for (int i = 0; i < mn; ++i) my_hp_total += mhp[i];
  int safe = hold && left * 10 >= my_hp_total * 3;
  DBG("DEF t=%d enemy near HQ n=%d hp=%d | home n=%d hp=%d hold=%d left=%d\n", turn, en_n, en_hp, mn, my_hp_total, hold, left);
  if (safe) return 0;
  /* recall everything free within 6 turns of the HQ (free move) */
  for (int i = 0; i < S->warriors.len; ++i) {
    const Warrior *w = &S->warriors.data[i];
    if (!warrior_free(a, w, M) || w->region == M->my_hq) continue;
    if (walk_len(P, w->region, M->my_hq) > 14) continue;
    push_move(a, w->id, M->my_hq);
  }
  /* train the maximum */
  int cap = HQ_LEVELS[hq->level].train_cap;
  int n = min_int(cap, *budget / TRAIN_COST);
  if (n > a->train_n) { *budget -= TRAIN_COST * (n - a->train_n); a->train_n = n; }
  return 1;
}

/* workers standing on a base about to be overrun run home (free move) */
static void base_evacuation(Actions *a, const GameState *S, const GameMap *M, int *budget) {
  (void)budget;
  for (int i = 0; i < S->buildings.len; ++i) {
    const Building *b = &S->buildings.data[i];
    if (b->side != M->my_side || b->type != BTYPE_BASE) continue;
    int r = b->region;
    if (g_en_cnt[r] > 0) continue;           /* already engaged: cannot move anyway */
    int threat = 0; int ehp[512]; int en = 0;
    for (int j = 0; j < S->warriors.len; ++j) {
      const Warrior *w = &S->warriors.data[j];
      if (w->id.side == M->my_side) continue;
      if (w->region == r || adjacent(M, w->region, r)) { threat += w->hp; if (en < 512) ehp[en++] = w->hp; }
    }
    if (threat == 0) continue;
    int mhp[512]; int mn = collect_hp(S, M->my_side, r, 0, mhp, 512);
    int left = 0;
    int hold = sim_zone(mhp, mn, building_turret(b), ehp, en, 0, 0, 20, &left);
    if (hold) continue;
    for (int j = 0; j < S->warriors.len; ++j) {
      const Warrior *w = &S->warriors.data[j];
      if (w->region != r || !warrior_free(a, w, M)) continue;
      push_move(a, w->id, M->my_hq);
    }
    DBG("EVAC base %d (threat hp %d vs ours %d)\n", r, threat, mn);
  }
}

/* ---------------------------------------------------------------- economy */
static int claims_in_flight(const GameState *S, const GameMap *M) {
  int n = 0;
  for (int i = 0; i < S->warriors.len; ++i) {
    const Warrior *w = &S->warriors.data[i];
    if (w->id.side != M->my_side) continue;
    int t = g_claim_of[w->id.num];
    if (t < 0) continue;
    const Building *b = find_building_const(S, t);
    if (b != NULL) { g_claim_of[w->id.num] = -1; continue; }   /* built (by anyone): claim over */
    ++n;
  }
  return n;
}
static int stronghold_claimed_by_me(const GameState *S, const GameMap *M, int r) {
  for (int i = 0; i < S->warriors.len; ++i) {
    const Warrior *w = &S->warriors.data[i];
    if (w->id.side == M->my_side && g_claim_of[w->id.num] == r) return 1;
  }
  return 0;
}
static int enemy_building_within(const GameState *S, const GameMap *M, int r, int hops) {
  static int d[ZMAX];
  bfs_hops_from(M, r, d);
  for (int i = 0; i < S->buildings.len; ++i) {
    const Building *b = &S->buildings.data[i];
    if (b->side != M->my_side && d[b->region] >= 0 && d[b->region] <= hops) return 1;
  }
  return 0;
}

/* build on strongholds where a claimer/army stands */
static void issue_builds(Actions *a, const GameState *S, const GameMap *M, int *budget) {
  for (int i = 0; i < M->strongholds.len; ++i) {
    int r = M->strongholds.data[i];
    if (find_building_const(S, r) != NULL) continue;
    if (g_my_stat[r] == 0 || g_en_cnt[r] > 0) continue;
    if (r == g_sentinel_block) continue;
    if (action_has_upgrade(a, r)) continue;
    if (*budget < BASE_LEVELS[1].cost) continue;
    VEC_PUSH(a->upgrades, r); *budget -= BASE_LEVELS[1].cost;
    /* claim complete */
    for (int j = 0; j < S->warriors.len; ++j) {
      const Warrior *w = &S->warriors.data[j];
      if (w->id.side == M->my_side && g_claim_of[w->id.num] == r) g_claim_of[w->id.num] = -1;
    }
    DBG("BUILD base at %d\n", r);
  }
}

/* ===================================================================== v7
   "돌"-style macro (2026-08-29), reverse-engineered from the round 11-14
   contest logs of team 돌:
     - turn 1: HQ -> L2, one warrior walks to the CENTRE stronghold and sits
       there (denies the enemy's centre base, sees the middle of the map)
     - one claimer at a time -> nearest free stronghold, base built the turn
       the gold reaches 500; every base gets exactly its work slots filled
     - HQ -> L3 after ~6 bases / turn 85, claims paused while saving
     - from ~turn 105 the HQ trains every turn; the army gathers at the
       forward base and hits enemy bases in small groups
     - base L2 (rear first) trickles from turn 110, L3 from ~190,
       HQ L4/L5 whenever the bank allows; everything L3 + HQ5 by ~280
     - late: 3 warriors/day and one all-in on the enemy HQ
   ======================================================================= */
#ifndef V7_HQ3_MIN_BASES
#define V7_HQ3_MIN_BASES 6
#endif
#ifndef V7_HQ3_MIN_TURN
#define V7_HQ3_MIN_TURN 85
#endif
#ifndef V7_ARMY_START
#define V7_ARMY_START 105
#endif
#ifndef V7_BASE2_START
#define V7_BASE2_START 110
#endif
#ifndef V7_BASE3_START
#define V7_BASE3_START 190
#endif
#ifndef V7_HQ4_START
#define V7_HQ4_START 170
#endif
#ifndef V7_ARMY_MID
#define V7_ARMY_MID 20               /* standing army turn 105..200 */
#endif
#ifndef V7_ARMY_LATE
#define V7_ARMY_LATE 28              /* turn 200..270 */
#endif
#ifndef V7_RALLY_MAX_HOME
#define V7_RALLY_MAX_HOME 4
#endif
#ifndef V7_HQ5_START
#define V7_HQ5_START 230
#endif
#ifndef V7_GROUP_MIN
#define V7_GROUP_MIN 4
#endif
#ifndef V7_RAID_RANGE
#define V7_RAID_RANGE 6              /* walk turns from the rally a raid may go */
#endif
#ifndef V7_ALLIN_TURN
#define V7_ALLIN_TURN 290
#endif
#ifndef V7_ALLIN_STACK
#define V7_ALLIN_STACK 40
#endif
#ifndef V7_CLAIMS_LATE
#define V7_CLAIMS_LATE 2             /* claimers in flight once rich (turn >= 150) */
#endif
#ifndef V7_CENTRE_FREE_TURN
#define V7_CENTRE_FREE_TURN 150      /* before this the centre is only denied, not built */
#endif

/* choose next stronghold to claim (nearest by engine walk from `from`) */
static int choose_claim_target(const GameState *S, const GameMap *M, const Paths *P, int from, int aggressive, int turn) {
  int best = -1; int best_wl = INF_HOPS;
  for (int i = 0; i < M->strongholds.len; ++i) {
    int r = M->strongholds.data[i];
    if (find_building_const(S, r) != NULL) continue;
    if (stronghold_claimed_by_me(S, M, r)) continue;
    if (g_en_near2_hp[r] > 0) continue;
    if (r == g_centre && turn < V7_CENTRE_FREE_TURN && g_role_sentinel >= 0) continue;
    int enemy_side = g_hop_opp[r] < g_hop_my[r];
    if (!aggressive && enemy_side) continue;
    if (enemy_building_within(S, M, r, 1)) continue;
    int wl = walk_len(P, from, r);
    if (wl >= INF_HOPS) continue;
    if (wl < best_wl || (wl == best_wl && r < best)) { best = r; best_wl = wl; }
  }
  return best;
}

/* one claimer (two once rich): walks when the 500 will be there on arrival */
static int issue_claims(Actions *a, const GameState *S, const GameMap *M, const Paths *P, int *budget,
                        int income_net, int *want_train, int turn, int reserve) {
  int inflight = claims_in_flight(S, M);
  int issued = 0;
  int max_inflight = (turn >= 150 && *budget >= 1500) ? V7_CLAIMS_LATE : 1;
  int hq_lvl = my_hq_level(S, M);
  int hq_keep = HQ_LEVELS[hq_lvl].work_cap;
  int aggressive = turn >= 100;
  while (inflight < max_inflight) {
    int target = choose_claim_target(S, M, P, M->my_hq, aggressive, turn);
    if (target < 0) break;
    /* source: a surplus free body at an own building nearest to the target */
    const Warrior *best = NULL; int best_len = INF_HOPS; int need_replace = 0;
    for (int i = 0; i < S->warriors.len; ++i) {
      const Warrior *w = &S->warriors.data[i];
      if (!warrior_free(a, w, M)) continue;
      const Building *b = find_building_const(S, w->region);
      if (b == NULL || b->side != M->my_side) continue;
      int keep = (w->region == M->my_hq) ? hq_keep : building_work_cap(b);
      if (free_count_at(S, M, a, w->region) <= keep) continue;
      int wl = walk_len(P, w->region, target);
      if (wl < best_len) { best_len = wl; best = w; }
    }
    if (best == NULL) {
      /* 돌: take an HQ worker and train its replacement the same morning */
      const Building *hq = find_building_const(S, M->my_hq);
      int cap = (hq && hq->side == M->my_side) ? HQ_LEVELS[hq->level].train_cap : 0;
      if (a->train_n < cap && free_count_at(S, M, a, M->my_hq) > 0 && *budget >= TRAIN_COST + MOVE_COST) {
        for (int i = 0; i < S->warriors.len; ++i) {
          const Warrior *w = &S->warriors.data[i];
          if (w->region != M->my_hq || !warrior_free(a, w, M)) continue;
          if (best == NULL || w->hp < best->hp) best = w;
        }
        if (best) { best_len = walk_len(P, M->my_hq, target); need_replace = 1; }
      }
    }
    if (best == NULL) { *want_train = 1; break; }
    /* affordability on arrival */
    int have = *budget - reserve - MOVE_COST - (need_replace ? TRAIN_COST : 0);
    if (have + income_net * min_int(best_len, 6) < BASE_LEVELS[1].cost * (inflight + 1)) break;
    if (*budget < MOVE_COST + (need_replace ? TRAIN_COST : 0)) break;
    push_move(a, best->id, target); *budget -= MOVE_COST;
    if (need_replace) { a->train_n += 1; *budget -= TRAIN_COST; }
    g_claim_of[best->id.num] = target;
    ++inflight; ++issued;
    DBG("CLAIM t=%d %c%d -> %d (%d turns)%s\n", turn, side_char(best->id.side), best->id.num, target, best_len, need_replace ? " +replacement" : "");
  }
  return issued;
}

/* fill work slots: for each own building with an open slot pull the nearest surplus body */
static void issue_worker_fill(Actions *a, const GameState *S, const GameMap *M, const Paths *P, int *budget, int *open_slots_out) {
  int open_total = 0;
  int hq_lvl = my_hq_level(S, M);
  int hq_keep = HQ_LEVELS[hq_lvl].work_cap;
  for (int i = 0; i < S->buildings.len; ++i) {
    const Building *b = &S->buildings.data[i];
    if (b->side != M->my_side) continue;
    int r = b->region;
    int cap = building_work_cap(b);
    int have = free_count_at(S, M, a, r) + g_my_arriving[r];
    /* stationary reserved bodies (claimers waiting, scout) don't work here but do count for income;
       count all stationary for the slot check */
    have = max_int(have, g_my_stat[r] + g_my_arriving[r]);
    int need = cap - have;
    if (need <= 0) continue;
    /* sources: other own buildings with surplus (HQ first) */
    for (int s = 0; s < S->buildings.len && need > 0; ++s) {
      const Building *sb = &S->buildings.data[s];
      if (sb->side != M->my_side || sb->region == r) continue;
      int keep = (sb->region == M->my_hq) ? hq_keep : building_work_cap(sb);
      int moved = pull_from(a, S, M, sb->region, r, need, keep, 0, budget);
      need -= moved;
    }
    (void)P;
    open_total += max_int(0, need);
  }
  *open_slots_out = open_total;
}


/* ---------------------------------------------------------------- upgrades (돌 order) */
static int pick_base_for_upgrade(const GameState *S, const GameMap *M, const Actions *a, int lvl) {
  /* rear-most (safest) base of level lvl whose slots are staffed */
  int best = -1;
  for (int i = 0; i < S->buildings.len; ++i) {
    const Building *b = &S->buildings.data[i];
    if (b->side != M->my_side || b->type != BTYPE_BASE || b->level != lvl) continue;
    int r = b->region;
    if (action_has_upgrade(a, r) || g_my_stat[r] == 0 || g_en_cnt[r] > 0) continue;
    if (g_my_stat[r] + g_my_arriving[r] < BASE_LEVELS[lvl].work_cap) continue;
    if (best < 0 || g_hop_opp[r] > g_hop_opp[best]) best = r;
  }
  return best;
}

static void issue_upgrades(Actions *a, const GameState *S, const GameMap *M, int *budget, int turn, int reserve) {
  int bases = my_base_count(S, M);
  int spend = *budget - reserve;
  g_upgrade_reserve = 0;
  const Building *hq = find_building_const(S, M->my_hq);
  if (hq == NULL || hq->side != M->my_side) return;
  int hq_ok = g_my_stat[M->my_hq] > 0 && g_en_cnt[M->my_hq] == 0 && !action_has_upgrade(a, M->my_hq);
  /* HQ2: turn 1 */
  if (hq->level == 1 && hq_ok) {
    if (*budget >= HQ_LEVELS[2].upgrade_cost) { VEC_PUSH(a->upgrades, M->my_hq); *budget -= HQ_LEVELS[2].upgrade_cost; spend -= HQ_LEVELS[2].upgrade_cost; DBG("UPGRADE t=%d HQ -> 2\n", turn); }
    else g_upgrade_reserve = HQ_LEVELS[2].upgrade_cost;
    return;
  }
  /* HQ3: after ~6 bases or turn 85; everything else waits */
  if (hq->level == 2 && (bases >= V7_HQ3_MIN_BASES || turn >= V7_HQ3_MIN_TURN)) {
    if (hq_ok && *budget >= HQ_LEVELS[3].upgrade_cost) { VEC_PUSH(a->upgrades, M->my_hq); *budget -= HQ_LEVELS[3].upgrade_cost; DBG("UPGRADE t=%d HQ -> 3\n", turn); }
    else g_upgrade_reserve = HQ_LEVELS[3].upgrade_cost;
    return;
  }
  if (hq->level < 3) return;
  /* HQ4 from turn 170, HQ5 from turn 230: saved for ahead of base upgrades */
  {
    int want = (hq->level == 3 && turn >= V7_HQ4_START) ? 4 : (hq->level == 4 && turn >= V7_HQ5_START) ? 5 : 0;
    if (want) {
      int cost = HQ_LEVELS[want].upgrade_cost;
      if (hq_ok && spend >= cost) { VEC_PUSH(a->upgrades, M->my_hq); *budget -= cost; spend -= cost; DBG("UPGRADE t=%d HQ -> %d\n", turn, want); }
      else { g_upgrade_reserve = cost; return; }
    }
  }
  /* base L1 -> L2 (rear first), one per turn */
  if (turn >= V7_BASE2_START) {
    int r = pick_base_for_upgrade(S, M, a, 1);
    if (r >= 0 && spend >= BASE_LEVELS[2].cost) { VEC_PUSH(a->upgrades, r); *budget -= BASE_LEVELS[2].cost; spend -= BASE_LEVELS[2].cost; DBG("UPGRADE t=%d base %d -> 2\n", turn, r); }
  }
  /* base L2 -> L3 once no L1 base is left (or late) */
  if (turn >= V7_BASE3_START) {
    int l1 = 0;
    for (int i = 0; i < S->buildings.len; ++i) { const Building *b = &S->buildings.data[i]; if (b->side == M->my_side && b->type == BTYPE_BASE && b->level == 1) ++l1; }
    if (l1 == 0 || turn >= 230) {
      int r = pick_base_for_upgrade(S, M, a, 2);
      if (r >= 0 && spend >= BASE_LEVELS[3].cost) { VEC_PUSH(a->upgrades, r); *budget -= BASE_LEVELS[3].cost; spend -= BASE_LEVELS[3].cost; DBG("UPGRADE t=%d base %d -> 3\n", turn, r); }
    }
  }
  /* max-level repairs: endgame tiebreak, or badly damaged and rich */
  hq = find_building_const(S, M->my_hq);
  if (hq && hq->level == HQ_MAX_LEVEL && hq->hp < HQ_LEVELS[HQ_MAX_LEVEL].hp && !action_has_upgrade(a, M->my_hq) &&
      g_my_stat[M->my_hq] > 0 && g_en_cnt[M->my_hq] == 0 &&
      (turn >= MAX_TURN - 3 || hq->hp <= HQ_LEVELS[HQ_MAX_LEVEL].hp / 2) && *budget >= HQ_HEAL_COST) {
    VEC_PUSH(a->upgrades, M->my_hq); *budget -= HQ_HEAL_COST;
  }
}

/* ---------------------------------------------------------------- sentinel (centre) */
static void issue_sentinel(Actions *a, const GameState *S, const GameMap *M, const Paths *P, int *budget, int turn) {
  (void)P;
  if (g_centre < 0) {
    g_centre = (M->N - 1) / 2;
    if (!is_stronghold(M, g_centre)) {           /* should not happen: R starts as {N'} */
      int best = -1;
      for (int i = 0; i < M->strongholds.len; ++i) { int r = M->strongholds.data[i]; if (best < 0 || abs(g_hop_my[r] - g_hop_opp[r]) < abs(g_hop_my[best] - g_hop_opp[best])) best = r; }
      g_centre = best;
    }
  }
  g_sentinel_block = -1;
  if (turn == 1) {
    const Warrior *pick = NULL;
    for (int i = 0; i < S->warriors.len; ++i) {
      const Warrior *w = &S->warriors.data[i];
      if (w->id.side != M->my_side || w->region != M->my_hq || w->state != WSTATE_STATIONARY) continue;
      if (pick == NULL || w->id.num < pick->id.num) pick = w;
    }
    if (pick && *budget >= MOVE_COST) { g_role_sentinel = pick->id.num; push_move(a, pick->id, g_centre); *budget -= MOVE_COST; DBG("SENTINEL %c%d -> %d\n", side_char(pick->id.side), pick->id.num, g_centre); }
    return;
  }
  if (g_role_sentinel < 0) return;
  WarriorId sid = {M->my_side, g_role_sentinel};
  const Warrior *w = find_warrior_const(S, sid);
  if (w == NULL) { g_role_sentinel = -1; DBG("SENTINEL died t=%d\n", turn); return; }
  if (turn >= V7_CENTRE_FREE_TURN) { g_role_sentinel = -1; return; }   /* becomes a normal body */
  if (w->state != WSTATE_STATIONARY) return;
  if (w->region == g_centre) {
    g_sentinel_block = g_centre;
    /* outnumbered next to us: fall back to the nearest own building (free) */
    int adj_hp = 0, adj_n = 0;
    for (int k = 0; k < M->adj[g_centre].len; ++k) { int v = M->adj[g_centre].data[k]; adj_hp += g_en_hp[v]; adj_n += g_en_cnt[v]; }
    if (adj_n >= 2 || (adj_n >= 1 && adj_hp > w->hp)) {
      int best = -1, best_wl = INF_HOPS;
      for (int i = 0; i < S->buildings.len; ++i) {
        const Building *b = &S->buildings.data[i];
        if (b->side != M->my_side) continue;
        int wl = walk_len(P, g_centre, b->region);
        if (wl < best_wl) { best_wl = wl; best = b->region; }
      }
      if (best >= 0) { push_move(a, w->id, best); DBG("SENTINEL t=%d falls back to %d\n", turn, best); }
    }
    return;
  }
  /* away from the centre: go back when it is quiet */
  if (g_en_near2_hp[g_centre] == 0 && g_en_cnt[g_centre] == 0 && *budget >= MOVE_COST + 50) {
    const Building *cb = find_building_const(S, g_centre);
    if (cb != NULL && cb->side != M->my_side) { g_role_sentinel = -1; return; }   /* enemy base there now */
    push_move(a, w->id, g_centre); *budget -= MOVE_COST;
  }
}

/* ---------------------------------------------------------------- army */
static int home_guard(const GameState *S, int turn) {
  if (turn < 120) return 0;
  int g = 1 + enemy_alive_estimate(S) / 5;
  return min_int(g, 12);
}
/* rally = own base closest to the enemy that is still within V7_RALLY_MAX_HOME
   hops of our HQ (the stack must be able to come home when a wave shows up) */
static int rally_zone(const GameState *S, const GameMap *M, int turn) {
  int best = M->my_hq, best_h = g_hop_opp[M->my_hq];
  if (turn < V7_ARMY_START) return best;
  for (int i = 0; i < S->buildings.len; ++i) {
    const Building *b = &S->buildings.data[i];
    if (b->side != M->my_side || b->type != BTYPE_BASE) continue;
    if (g_hop_opp[b->region] < best_h) { best_h = g_hop_opp[b->region]; best = b->region; }
  }
  return best;
}


/* raid: the stack at the rally hits the nearest enemy base it can beat with
   the defenders it can SEE (plus a base's own staffing when it is out of
   sight).  The enemy HQ is only attacked with the paranoid v5 estimate
   (unseen enemies count as defenders) or in the all-in window. */
static int army_attack(Actions *a, const GameState *S, const GameMap *M, const Paths *P, int *budget, int turn, int rally) {
  const Building *rb = find_building_const(S, rally);
  int keep = (rb && rb->side == M->my_side) ? building_work_cap(rb) : 0;
  if (rally == M->my_hq) keep += home_guard(S, turn);
  int stack_hp[512]; int stack_n = 0;
  const Warrior *stack[512];
  for (int i = 0; i < S->warriors.len; ++i) {
    const Warrior *w = &S->warriors.data[i];
    if (w->region != rally || !warrior_free(a, w, M)) continue;
    if (stack_n < 512) { stack[stack_n] = w; stack_hp[stack_n] = w->hp; ++stack_n; }
  }
  for (int i = 0; i < stack_n; ++i)
    for (int j = i + 1; j < stack_n; ++j)
      if (stack_hp[j] > stack_hp[i]) { int t = stack_hp[i]; stack_hp[i] = stack_hp[j]; stack_hp[j] = t; const Warrior *tw = stack[i]; stack[i] = stack[j]; stack[j] = tw; }
  stack_n -= keep;
  if (turn % 25 == 0) DBG("ARMY t=%d rally=%d stack=%d (keep %d)\n", turn, rally, stack_n, keep);
  if (stack_n < V7_GROUP_MIN) return 0;
  int allin = (turn >= V7_ALLIN_TURN && stack_n >= V7_ALLIN_STACK) || turn >= MAX_TURN - 40;
  int ehp = HQ_LEVELS[enemy_hq_level_estimate(S, M)].warrior_hp;
  int best = -1; int best_len = INF_HOPS; int best_is_hq = 0;
  for (int i = 0; i < S->buildings.len; ++i) {
    const Building *b = &S->buildings.data[i];
    if (b->side == M->my_side) continue;
    int r = b->region;
    int wl = walk_len(P, rally, r);
    if (wl >= INF_HOPS) continue;
    int is_hq = b->type == BTYPE_HQ;
    if (!is_hq && wl > V7_RAID_RANGE && !allin) continue;
    /* fog: a remembered base may have been upgraded since we saw it - only
       raid what we can see right now (the all-in ignores this) */
    if (!is_hq && !allin && !region_visible(S, r)) continue;
    int dhp[512]; int dn = 0;
    {
      static int d2[ZMAX];
      bfs_hops_from(M, r, d2);
      for (int j = 0; j < S->warriors.len; ++j) {
        const Warrior *w = &S->warriors.data[j];
        if (w->id.side == M->my_side) continue;
        if (d2[w->region] >= 0 && d2[w->region] <= 2) { if (dn < 512) dhp[dn++] = w->hp; }
      }
    }
    int bld_hp = b->hp, bld_turret = building_turret(b);
    if (!region_visible(S, r)) {
      int cap = building_work_cap(b) + 1;
      for (int j = 0; j < cap && dn < 512; ++j) dhp[dn++] = ehp;
      if (!is_hq && turn >= 80) { bld_hp = BASE_LEVELS[BASE_MAX_LEVEL].hp; bld_turret = BASE_LEVELS[BASE_MAX_LEVEL].turret; }
    }
    if (is_hq) {
      int unseen = enemy_alive_estimate(S) - enemy_visible_count(S);
      if (allin) unseen /= 2;
      for (int j = 0; j < unseen && dn < 512; ++j) dhp[dn++] = ehp;
      int extra = min_int(wl, 12) * HQ_LEVELS[b->level].train_cap;
      for (int j = 0; j < extra && dn < 512; ++j) dhp[dn++] = ehp;
      if (stack_n < V5_HQ_ATTACK_MIN_STACK) continue;
    }
    int left = 0;
    int win = sim_zone(stack_hp, stack_n, 0, dhp, dn, bld_turret, bld_hp, 40, &left);
    int tot = 0; for (int j = 0; j < stack_n; ++j) tot += stack_hp[j];
    int need = allin ? 3 : 6;
    if (turn % 25 == 0) DBG("  target %d (%s lvl%d hp%d) defenders=%d win=%d left=%d/%d wl=%d\n", r, is_hq ? "HQ" : "BASE", b->level, b->hp, dn, win, left, tot, wl);
    if (!win || left * 10 < tot * need) continue;
    if (wl < best_len || (is_hq && !best_is_hq && wl <= best_len + 4)) { best = r; best_len = wl; best_is_hq = is_hq; }
  }
  if (best < 0) return 0;
  int cost = MOVE_COST * stack_n;
  if (*budget < cost) { g_launch_reserve = cost; return 0; }
  for (int i = 0; i < stack_n; ++i) push_move(a, stack[i]->id, best);
  *budget -= cost;
  g_attack_target = best; g_attack_launch_turn = turn;
  DBG("ATTACK t=%d %d bodies %d -> %d (%d turns)%s\n", turn, stack_n, rally, best, best_len, best_is_hq ? " HQ" : "");
  return 1;
}

/* bodies that finished an attack (standing on a zone that is not our building) come back or build */
static void army_regroup(Actions *a, const GameState *S, const GameMap *M, const Paths *P, int *budget, int rally) {
  (void)P;
  for (int r = 0; r < M->N; ++r) {
    if (g_my_stat[r] == 0 || r == rally) continue;
    const Building *b = find_building_const(S, r);
    if (b != NULL && b->side == M->my_side) continue;   /* at an own building: handled by worker logic */
    if (g_en_cnt[r] > 0) continue;                       /* engaged */
    if (b != NULL) continue;                             /* still an enemy building here: keep sieging */
    if (is_stronghold(M, r)) continue;                   /* issue_builds will build; then it is ours */
    /* free bodies on a plain zone (finished fight / killed base) -> rally (free if rally is ours) */
    int n = free_count_at(S, M, a, r);
    if (n == 0) continue;
    pull_from(a, S, M, r, rally, n, 0, 1, budget);
  }
}

/* idle HQ surplus walks to the rally (free) */
static void army_to_rally(Actions *a, const GameState *S, const GameMap *M, int *budget, int turn, int rally) {
  if (rally == M->my_hq) return;
  int hq_keep = HQ_LEVELS[my_hq_level(S, M)].work_cap + home_guard(S, turn);
  int n = free_count_at(S, M, a, M->my_hq) - hq_keep;
  if (n <= 0) return;
  pull_from(a, S, M, M->my_hq, rally, n, hq_keep, 1, budget);
  /* also consolidate surplus from other bases */
  for (int i = 0; i < S->buildings.len; ++i) {
    const Building *b = &S->buildings.data[i];
    if (b->side != M->my_side || b->type != BTYPE_BASE || b->region == rally) continue;
    int keep = building_work_cap(b);
    int m = free_count_at(S, M, a, b->region) - keep;
    if (m > 0) pull_from(a, S, M, b->region, rally, m, keep, 1, budget);
  }
}


/* ---------------------------------------------------------------- training */
/* bodies that are neither workers, claimers nor scouts */
static int my_army_size(const GameState *S, const GameMap *M) {
  int slots = 0;
  for (int i = 0; i < S->buildings.len; ++i) {
    const Building *b = &S->buildings.data[i];
    if (b->side == M->my_side) slots += building_work_cap(b);
  }
  int n = my_alive(S, M) - slots - (g_role_scout >= 0 ? 1 : 0) - (g_role_sentinel >= 0 ? 1 : 0) - claims_in_flight(S, M);
  return max_int(n, 0);
}
/* fixed schedule (the mirror prior in enemy_alive_estimate() would chase itself);
   visible enemy bodies on our half raise it */
static int army_target(const GameState *S, int turn) {
  if (turn < V7_ARMY_START) return 0;
  if (turn >= 270) return 1000;
  int t = (turn < 200) ? V7_ARMY_MID : V7_ARMY_LATE;
  int seen = 0;
  for (int i = 0; i < S->warriors.len; ++i) {
    const Warrior *w = &S->warriors.data[i];
    if (w->id.side == S->my_side) continue;
    if (g_hop_my[w->region] <= g_hop_opp[w->region] + 2) ++seen;
  }
  return max_int(t, seen + seen / 4 + 4);
}

static void issue_training(Actions *a, const GameState *S, const GameMap *M, int *budget, int turn,
                           int open_slots, int want_claimer, int army_reserve) {
  const Building *hq = find_building_const(S, M->my_hq);
  if (hq == NULL || hq->side != M->my_side) return;
  int cap = HQ_LEVELS[hq->level].train_cap;
  if (a->train_n >= cap) return;
  int alive = my_alive(S, M);
  int n = a->train_n;
  /* workers/claimer/scout: always (they pay for themselves) */
  int wanted = open_slots + want_claimer + (g_role_scout < 0 && turn >= 60 && alive >= V5_SCOUT_MIN_ARMY - 1 ? 1 : 0);
  while (n < cap && wanted > 0) {
    if (*budget < TRAIN_COST) break;
    ++n; --wanted; *budget -= TRAIN_COST;
  }
  /* army */
  int army = my_army_size(S, M);
  int target = army_target(S, turn);
  int income = my_income(S, M);
  while (n < cap && army < target) {
    if (*budget - TRAIN_COST < army_reserve) break;
    int net = income - UPKEEP_PER_WARRIOR * (alive + n + 1);
    if (net < 0 && net < -(*budget - TRAIN_COST) / 50) break;     /* the bank carries the upkeep for 50 days */
    ++n; ++army; *budget -= TRAIN_COST;
  }
  a->train_n = n;
}

/* ---------------------------------------------------------------- finalize */
static void finalize_actions(Actions *a, const GameState *S, const GameMap *M) {
  /* drop moves for non-stationary / unknown warriors and no-op moves */
  int wnew = 0;
  for (int i = 0; i < a->moves.len; ++i) {
    const Warrior *w = find_warrior_const(S, a->moves.data[i].id);
    if (w == NULL || w->state != WSTATE_STATIONARY) continue;
    if (w->region == a->moves.data[i].target) continue;
    if (a->moves.data[i].target < 0 || a->moves.data[i].target >= M->N) continue;
    /* keep the first move per warrior */
    int dup = 0;
    for (int j = 0; j < wnew; ++j) if (wid_eq(a->moves.data[j].id, w->id)) { dup = 1; break; }
    if (dup) continue;
    a->moves.data[wnew++] = a->moves.data[i];
  }
  a->moves.len = wnew;
  /* scout: only its own planned move */
  if (g_role_scout >= 0) {
    int k = 0;
    for (int i = 0; i < a->moves.len; ++i) {
      const Move *mv = &a->moves.data[i];
      if (mv->id.num == g_role_scout && mv->target != g_scout_planned) continue;
      a->moves.data[k++] = a->moves.data[i];
    }
    a->moves.len = k;
  }
  /* UPGRADE legality: a stationary own warrior must remain in the zone */
  int unew = 0;
  for (int u = 0; u < a->upgrades.len; ++u) {
    int r = a->upgrades.data[u];
    int stays = 0, cancel = -1;
    for (int i = 0; i < S->warriors.len && !stays; ++i) {
      const Warrior *w = &S->warriors.data[i];
      if (w->id.side != M->my_side || w->region != r || w->state != WSTATE_STATIONARY) continue;
      int moved = 0;
      for (int j = 0; j < a->moves.len; ++j) if (wid_eq(a->moves.data[j].id, w->id)) { moved = 1; cancel = j; break; }
      if (!moved) stays = 1;
    }
    if (!stays && cancel >= 0) {
      for (int j = cancel; j + 1 < a->moves.len; ++j) a->moves.data[j] = a->moves.data[j + 1];
      a->moves.len--; stays = 1;
    }
    if (stays && g_en_cnt[r] == 0) a->upgrades.data[unew++] = r;
  }
  a->upgrades.len = unew;
  /* gold sanity: construction is paid first, then moves, then training */
  long long cost = 0;
  for (int u = 0; u < a->upgrades.len; ++u) {
    const Building *b = find_building_const(S, a->upgrades.data[u]);
    if (b == NULL) cost += BASE_LEVELS[1].cost;
    else if (b->level >= building_max_level(b)) cost += (b->type == BTYPE_HQ) ? HQ_HEAL_COST : BASE_HEAL_COST;
    else cost += building_upgrade_cost(b);
  }
  if (cost > S->gold) { a->upgrades.len = 0; cost = 0; }   /* should not happen; be safe */
  {
    int k = 0;
    for (int i = 0; i < a->moves.len; ++i) {
      const Building *b = find_building_const(S, a->moves.data[i].target);
      int c = (b != NULL && b->side == M->my_side) ? 0 : MOVE_COST;
      if (cost + c > S->gold) continue;
      cost += c; a->moves.data[k++] = a->moves.data[i];
    }
    a->moves.len = k;
  }
  {
    const Building *hq = find_building_const(S, M->my_hq);
    int cap = (hq && hq->side == M->my_side) ? HQ_LEVELS[hq->level].train_cap : 0;
    if (a->train_n > cap) a->train_n = cap;
    while (a->train_n > 0 && cost + (long long)TRAIN_COST * a->train_n > S->gold) a->train_n--;
  }
}


/* ---------------------------------------------------------------- decide */
static Actions decide(const GameState *S, const GameMap *M, const Paths *P, int turn) {
  Actions a;
  memset(&a, 0, sizeof(a));
  ctx_init(M);
  ctx_scan(S, M);
  int budget = S->gold;

  /* 1. defense */
  int hq_threat = hq_defense(&a, S, M, P, &budget, turn);
  base_evacuation(&a, S, M, &budget);

  /* 2. sentinel + scout */
  issue_sentinel(&a, S, M, P, &budget, turn);
  issue_scout(&a, S, M, P, &budget, turn, hq_threat);

  /* 3. build where claimers stand */
  issue_builds(&a, S, M, &budget);

  /* reserve: bases about to be built by arriving claimers */
  int reserve = 0;
  for (int i = 0; i < S->warriors.len; ++i) {
    const Warrior *w = &S->warriors.data[i];
    if (w->id.side != M->my_side) continue;
    int t = g_claim_of[w->id.num];
    if (t < 0 || find_building_const(S, t) != NULL) continue;
    if (walk_len(P, w->region, t) <= 3) reserve += BASE_LEVELS[1].cost;
  }

  int rally = rally_zone(S, M, turn);
  {
    int half_hp = 0;
    for (int i = 0; i < S->warriors.len; ++i) {
      const Warrior *w = &S->warriors.data[i];
      if (w->id.side == M->my_side) continue;
      if (g_hop_my[w->region] < g_hop_opp[w->region] && g_hop_my[w->region] <= 8) half_hp += w->hp;
    }
    if (half_hp >= 12) { rally = M->my_hq; DBG("HOME MODE t=%d enemy hp on our half %d\n", turn, half_hp); }
  }

  /* 4. army */
  g_launch_reserve = 0;
  if (!hq_threat) {
    army_attack(&a, S, M, P, &budget, turn, rally);
    army_regroup(&a, S, M, P, &budget, rally);
  }

  /* 5. economy: HQ2/HQ3 first, then claims, then the rest of the upgrades */
  int open_slots = 0;
  issue_worker_fill(&a, S, M, P, &budget, &open_slots);
  int want_claimer = 0;
  int income_net = my_income(S, M) - UPKEEP_PER_WARRIOR * my_alive(S, M);
  int army_reserve = (turn >= V7_ARMY_START && my_army_size(S, M) < army_target(S, turn)) ? TRAIN_COST : 0;
  issue_upgrades(&a, S, M, &budget, turn, reserve + army_reserve);
  int claim_reserve = reserve + g_upgrade_reserve;
  if (!hq_threat && g_upgrade_reserve == 0) issue_claims(&a, S, M, P, &budget, income_net, &want_claimer, turn, claim_reserve);
  else if (!hq_threat && turn < V7_ARMY_START && g_upgrade_reserve > 0) {
    /* saving for HQ3: still claim when the bank covers both */
    issue_claims(&a, S, M, P, &budget, income_net, &want_claimer, turn, claim_reserve);
  }

  /* 6. idle bodies to the rally (under threat the rally is the HQ: free recall) */
  if (turn >= 40) army_to_rally(&a, S, M, &budget, turn, hq_threat ? M->my_hq : rally);

  /* 7. training */
  issue_training(&a, S, M, &budget, turn, open_slots, want_claimer, g_upgrade_reserve + g_launch_reserve);

  finalize_actions(&a, S, M);
  return a;
}

int main(void) {
  GameMap M;
  GameState S;
  parse_init(&M, &S);
  Paths P = calculate_paths(&M);
  int turn;
  while (read_turn_start(&turn)) {
    Actions a = decide(&S, &M, &P, turn);
    emit_actions(&a);
    read_turn_result(&S, &M, &a);
#ifdef DEBUG_STATE
    {
      int mine = 0, seen_enemy = 0, my_bases = 0, known_enemy_bases = 0, vis = 0;
      for (int i = 0; i < S.warriors.len; ++i)
        if (S.warriors.data[i].id.side == M.my_side) ++mine; else ++seen_enemy;
      for (int i = 0; i < S.buildings.len; ++i)
        if (S.buildings.data[i].type == BTYPE_BASE) {
          if (S.buildings.data[i].side == M.my_side) ++my_bases; else ++known_enemy_bases;
        }
      for (int r = 0; r < M.N; ++r) vis += S.visible[r];
      fprintf(stderr, "STATE t=%d gold=%d mine=%d seen_enemy=%d my_bases=%d known_enemy_bases=%d vis=%d/%d income=%d hq=%d est_enemy=%d\n",
              S.turn, S.gold, mine, seen_enemy, my_bases, known_enemy_bases, vis, M.N,
              my_income(&S, &M), my_hq_level(&S, &M), enemy_alive_estimate(&S));
    }
#endif
    free(a.moves.data);
    free(a.upgrades.data);
  }
  return 0;
}
