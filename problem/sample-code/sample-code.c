#define _POSIX_C_SOURCE 200809L
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
  MAX_TURN = 400,         /* maximum turn (days) */
  START_GOLD = 750,       /* initial gold */
  START_WARRIORS = 3,     /* initial warriors */
  MOVE_COST = 10,         /* move cost */
  TRAIN_COST = 120,       /* train cost */
  WORK_INCOME = 15,       /* income per warrior */
  UPKEEP_PER_WARRIOR = 2, /* upkeep per warrior */
  HQ_MAX_LEVEL = 5,       /* HQ max level */
  BASE_MAX_LEVEL = 3,     /* base max level */
  HQ_HEAL_COST = 1000,    /* HQ fix cost */
  BASE_HEAL_COST = 500,   /* base fix cost */
  HOP_VISION = 2,         /* vision radius shared by all units */
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
      (vec).data = realloc((vec).data, (size_t)(vec).cap * sizeof(*(vec).data)); \
    }                                                                          \
    (vec).data[(vec).len++] = (item);                                          \
  } while (0)

typedef struct {
  WarriorId id;
  int target;
} Move;

typedef struct {
  WarriorId id;
  int damage;
} Damage;

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
  Damage *data;
  int len, cap;
} DamageVec;
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
  WarriorVec warriors;
  BuildingVec buildings;
  IntVec visible;
} GameState;

typedef struct {
  int train_n;
  MoveVec moves;
  IntVec upgrades;
} Actions;

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
    storage = realloc(storage, storage_cap);
  }
  memcpy(storage, line, len);

  int n = 0;
  char *save = NULL;
  for (char *t = strtok_r(storage, " \t", &save); t;
       t = strtok_r(NULL, " \t", &save)) {
    if (n == items_cap) {
      items_cap = items_cap ? items_cap * 2 : 16;
      items = realloc(items, (size_t)items_cap * sizeof(char *));
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

static void add_visible_hops(const GameMap *M, int start, int radius,
                             unsigned char *visible) {
  unsigned char *seen = calloc((size_t)M->N, 1);
  int *frontier = malloc((size_t)M->N * sizeof(int));
  int *next = malloc((size_t)M->N * sizeof(int));
  int frontier_len = 1;
  frontier[0] = start;
  seen[start] = 1;
  visible[start] = 1;
  for (int hop = 0; hop < radius; ++hop) {
    int next_len = 0;
    for (int i = 0; i < frontier_len; ++i) {
      IntVec neighbors = M->adj[frontier[i]];
      for (int j = 0; j < neighbors.len; ++j) {
        int neighbor = neighbors.data[j];
        if (!seen[neighbor]) {
          seen[neighbor] = 1;
          visible[neighbor] = 1;
          next[next_len++] = neighbor;
        }
      }
    }
    int *tmp = frontier;
    frontier = next;
    next = tmp;
    frontier_len = next_len;
  }
  free(seen);
  free(frontier);
  free(next);
}

static void compute_visible(GameState *S, const GameMap *M) {
  unsigned char *visible = calloc((size_t)M->N, 1);
  for (int i = 0; i < S->warriors.len; ++i) {
    Warrior *w = &S->warriors.data[i];
    if (w->id.side == M->my_side)
      add_visible_hops(M, w->region, HOP_VISION, visible);
  }
  for (int i = 0; i < S->buildings.len; ++i) {
    Building *b = &S->buildings.data[i];
    if (b->side == M->my_side)
      add_visible_hops(M, b->region, HOP_VISION, visible);
  }
  S->visible.len = 0;
  for (int region = 0; region < M->N; ++region)
    if (visible[region])
      VEC_PUSH(S->visible, region);
  free(visible);
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
  M->x = malloc((size_t)M->N * sizeof(long long));
  M->y = malloc((size_t)M->N * sizeof(long long));
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
  M->adj = calloc((size_t)M->N, sizeof(IntVec));
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
  for (int i = 0; i < submitted->upgrades.len; ++i) {
    int region = submitted->upgrades.data[i];
    Building *b = find_building(S, region);
    if (b == NULL) {
      S->gold -= BASE_LEVELS[1].cost;
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
    int own = (b != NULL && b->side == M->my_side);
    for (int j = 0; !own && j < submitted->upgrades.len; ++j)
      own = (submitted->upgrades.data[j] == mv.target);
    int cost = own ? 0 : MOVE_COST;
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
  }
  {
    int n;
    char **t = tokens(readln(), &n);
    S->my_countdown = atoi(t[2]);
    S->opp_countdown = atoi(t[4]);
  }
  /* UPGRADE */
  {
    int n;
    char **t = tokens(readln(), &n); /* "UPGRADE N" */
    int count = atoi(t[1]);
    for (int i = 0; i < count; ++i) {
      int m;
      char **r = tokens(readln(), &m); /* "<A|B> <region>" */
      int region = atoi(r[1]);
      Building *b = find_building(S, region);
      if (b == NULL) {
        Building nb = make_base(region, M->my_side);
        VEC_PUSH(S->buildings, nb);
      } else if (b->level >= building_max_level(b)) {
        b->hp = building_current_hp(b);
      } else {
        building_apply_upgrade(b);
      }
    }
  }
  /* TRAIN */
  {
    int n;
    char **t = tokens(readln(), &n); /* "TRAIN N" */
    int count = atoi(t[1]);
    if (count > 0) {
      int m;
      char **ids = tokens(readln(), &m);
      for (int i = 0; i < count; ++i) {
        WarriorId id = parse_warrior(ids[i]);
        Building *hq_b = find_building(S, M->my_hq);
        int hq_level = (hq_b != NULL) ? hq_b->level : 1;
        Warrior w = {id, M->my_hq, HQ_LEVELS[hq_level].warrior_hp,
                     WSTATE_STATIONARY, 0};
        VEC_PUSH(S->warriors, w);
      }
    }
  }
  /* MOVE */
  {
    int n;
    char **t = tokens(readln(), &n); /* "MOVE N" */
    int count = atoi(t[1]);
    for (int i = 0; i < count; ++i) {
      int m;
      char **r = tokens(readln(), &m);
      WarriorId id = parse_warrior(r[0]);
      int region = atoi(r[1]);
      Warrior *w = find_warrior(S, id);
      if (w != NULL) {
        w->region = region;
        if (w->state == WSTATE_MOVING && w->region == w->target)
          w->state = WSTATE_STATIONARY;
      }
    }
  }
  /* DAMAGE */
  DamageVec starved;
  memset(&starved, 0, sizeof(starved));
  {
    int n;
    char **t = tokens(readln(), &n); /* "DAMAGE N" */
    int count = atoi(t[1]);
    for (int i = 0; i < count; ++i) {
      int m;
      char **r = tokens(readln(), &m); /* "<cause> <id> <damage>" */
      WarriorId id = parse_warrior(r[1]);
      int damage = atoi(r[2]);
      if (strcmp(r[0], "HUNGER") == 0) {
        Damage d = {id, damage};
        VEC_PUSH(starved, d);
        continue;
      }
      Warrior *w = find_warrior(S, id);
      if (w != NULL)
        w->hp -= damage;
    }
    int kept = 0;
    for (int i = 0; i < S->warriors.len; ++i)
      if (S->warriors.data[i].hp > 0)
        S->warriors.data[kept++] = S->warriors.data[i];
    S->warriors.len = kept;
  }
  /* SIEGE */
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
    int kept = 0;
    for (int i = 0; i < S->buildings.len; ++i)
      if (S->buildings.data[i].hp > 0)
        S->buildings.data[kept++] = S->buildings.data[i];
    S->buildings.len = kept;
  }
  compute_visible(S, M);
  /* WARRIOR */
  {
    int n;
    char **t = tokens(readln(), &n); /* "WARRIOR W" */
    int count = atoi(t[1]);
    int kept = 0;
    for (int i = 0; i < S->warriors.len; ++i)
      if (S->warriors.data[i].id.side == M->my_side)
        S->warriors.data[kept++] = S->warriors.data[i];
    S->warriors.len = kept;
    for (int i = 0; i < count; ++i) {
      int m;
      char **r = tokens(readln(), &m); /* "<id> <region> <hp>" */
      WarriorId id = parse_warrior(r[0]);
      if (id.side == M->my_side)
        continue;
      Warrior w = {id, atoi(r[1]), atoi(r[2]), WSTATE_STATIONARY, 0};
      VEC_PUSH(S->warriors, w);
    }
  }
  /* BUILDING */
  {
    int n;
    char **t = tokens(readln(), &n); /* "BUILDING B" */
    int count = atoi(t[1]);
    int kept = 0;
    for (int i = 0; i < S->buildings.len; ++i)
      if (S->buildings.data[i].side == M->my_side)
        S->buildings.data[kept++] = S->buildings.data[i];
    S->buildings.len = kept;
    for (int i = 0; i < count; ++i) {
      int m;
      char **r = tokens(readln(), &m); /* "<side> <region> <kind> <level> <hp>" */
      Side side = parse_side_char(r[0][0]);
      if (side == M->my_side)
        continue;
      BType btype = (strcmp(r[2], "HQ") == 0) ? BTYPE_HQ : BTYPE_BASE;
      Building b = {atoi(r[1]), side, btype, atoi(r[3]), atoi(r[4])};
      VEC_PUSH(S->buildings, b);
    }
  }
  (void)readln(); /* "END" */

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
  int fed = S->gold / UPKEEP_PER_WARRIOR;
  if (fed > alive)
    fed = alive;
  S->gold -= UPKEEP_PER_WARRIOR * fed;

  for (int i = 0; i < starved.len; ++i) {
    Warrior *w = find_warrior(S, starved.data[i].id);
    if (w != NULL)
      w->hp -= starved.data[i].damage;
  }
  free(starved.data);
  {
    int kept = 0;
    for (int i = 0; i < S->warriors.len; ++i)
      if (S->warriors.data[i].hp > 0)
        S->warriors.data[kept++] = S->warriors.data[i];
    S->warriors.len = kept;
  }
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
  P.dist = malloc((size_t)N * sizeof(double *));
  P.nxt = malloc((size_t)N * sizeof(int *));
  for (int i = 0; i < N; ++i) {
    P.dist[i] = malloc((size_t)N * sizeof(double));
    P.nxt[i] = malloc((size_t)N * sizeof(int));
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

static int next_step(const Paths *P, int u, int v) { return P->nxt[u][v]; }

static int path(const Paths *P, int u, int v, int *out) {
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

/*////////////////////////////////
//// WRITE YOUR STRATEGY HERE ////
////////////////////////////////*/
static Actions decide(const GameState *S, const GameMap *M, const Paths *P,
                      int turn) {
  (void)P;
  Actions a;
  memset(&a, 0, sizeof(a));
  if (turn == 1) {
    for (int i = 0; i < S->warriors.len; ++i) {
      const Warrior *w = &S->warriors.data[i];
      if (w->id.side != M->my_side)
        continue;
      Move mv = {w->id, M->opp_hq};
      VEC_PUSH(a.moves, mv);
    }
  }
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
    free(a.moves.data);
    free(a.upgrades.data);
  }
  return 0;
}
