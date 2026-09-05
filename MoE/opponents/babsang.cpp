#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

constexpr int MAX_TURN = 400;         // maximum turn (days)
constexpr int START_GOLD = 750;       // initial gold
constexpr int START_WARRIORS = 3;     // initial warriors
constexpr int MOVE_COST = 10;         // move cost
constexpr int TRAIN_COST = 120;       // train cost
constexpr int WORK_INCOME = 15;       // income per warrior
constexpr int UPKEEP_PER_WARRIOR = 2; // upkeep per warrior
constexpr int HQ_MAX_LEVEL = 5;       // HQ max level
constexpr int BASE_MAX_LEVEL = 3;     // base max level
constexpr int HQ_HEAL_COST = 1000;    // HQ fix cost
constexpr int BASE_HEAL_COST = 500;   // base fix cost
constexpr int HOP_VISION = 2;         // vision radius shared by all units

struct HqLevelEntry {
  int upgrade_cost;
  int warrior_hp;
  int hp;
  int turret;
  int train_cap;
  int work_cap;
};

struct BaseLevelEntry {
  int cost;
  int hp;
  int turret;
  int work_cap;
};

constexpr HqLevelEntry HQ_LEVELS[HQ_MAX_LEVEL + 1] = {
    {0, 0, 0, 0, 0, 0},     {0, 4, 10, 1, 1, 1},    {600, 5, 15, 2, 1, 2},
    {1000, 6, 20, 2, 2, 3}, {2000, 7, 25, 3, 2, 4}, {3000, 8, 30, 3, 3, 5},
};
constexpr BaseLevelEntry BASE_LEVELS[BASE_MAX_LEVEL + 1] = {
    {0, 0, 0, 0},
    {500, 6, 1, 1},
    {550, 12, 1, 2},
    {600, 18, 2, 3},
};

enum class Side : int { LEFT = 0, RIGHT = 1 };
enum class BType : int { HQ, BASE };
enum class WState : int { STATIONARY, MOVING };

inline Side opposite(Side s) {
  return s == Side::LEFT ? Side::RIGHT : Side::LEFT;
}
inline char side_char(Side s) { return s == Side::LEFT ? 'A' : 'B'; }
inline Side parse_side_char(char c) {
  return c == 'A' ? Side::LEFT : Side::RIGHT;
}

struct WarriorId {
  Side side = Side::LEFT;
  int num = 0;
  bool operator==(const WarriorId &o) const {
    return side == o.side && num == o.num;
  }
};

struct Warrior {
  WarriorId id;
  int region = 0;
  int hp = 0;
  WState state = WState::STATIONARY;
  int target = 0;
};

struct Building {
  int region = 0;
  Side side = Side::LEFT;
  BType type = BType::HQ;
  int level = 1;
  int hp = 10;

  int current_hp() const {
    return type == BType::HQ ? HQ_LEVELS[level].hp : BASE_LEVELS[level].hp;
  }
  int work_cap() const {
    return type == BType::HQ ? HQ_LEVELS[level].work_cap
                             : BASE_LEVELS[level].work_cap;
  }
};

struct GameMap {
  int N = 0, K = 0;
  std::vector<long long> x, y;
  std::vector<int> strongholds;
  std::vector<std::vector<int>> adj;

  Side my_side = Side::LEFT;
  int my_hq = 0;
  int opp_hq = 0;
};

struct GameState {
  int gold = START_GOLD;
  int my_countdown = 5;
  int opp_countdown = 5;
  std::vector<Warrior> warriors;
  std::vector<Building> buildings;
  std::vector<int> visible;
};

struct Actions {
  int train_n = 0;
  std::vector<std::pair<WarriorId, int>> moves;
  std::vector<int> upgrades;
};

static std::string readln() {
  std::string s;
  if (!std::getline(std::cin, s))
    std::exit(0);
  return s;
}

static std::vector<std::string> tokens(const std::string &s) {
  std::vector<std::string> out;
  std::istringstream is(s);
  for (std::string t; is >> t;)
    out.push_back(t);
  return out;
}

static WarriorId parse_warrior(const std::string &tok) {
  assert(!tok.empty() && (tok[0] == 'A' || tok[0] == 'B'));
  WarriorId id;
  id.side = parse_side_char(tok[0]);
  id.num = std::stoi(tok.substr(1));
  return id;
}

static std::string format_warrior(WarriorId id) {
  std::string s;
  s.push_back(side_char(id.side));
  s += std::to_string(id.num);
  return s;
}

static int hq_of(const GameMap &M, Side s) {
  return (s == Side::LEFT) ? 0 : M.N - 1;
}

static Building make_base(int region, Side s) {
  return Building{region, s, BType::BASE, 1, BASE_LEVELS[1].hp};
}

static void apply_upgrade(Building &b) {
  b.level += 1;
  b.hp = b.current_hp();
}

static int upgrade_cost(const Building &b) {
  if (b.type == BType::HQ)
    return HQ_LEVELS[b.level + 1].upgrade_cost;
  else
    return BASE_LEVELS[b.level + 1].cost;
}

static int max_level(const Building &b) {
  return b.type == BType::HQ ? HQ_MAX_LEVEL : BASE_MAX_LEVEL;
}

static std::vector<int> compute_visible(const GameState &S, const GameMap &M) {
  std::vector<char> visible(M.N, false);
  auto add_hops = [&](int start, int radius) {
    std::vector<char> seen(M.N, false);
    std::vector<int> frontier{start};
    seen[start] = true;
    visible[start] = true;
    for (int hop = 0; hop < radius; ++hop) {
      std::vector<int> next;
      for (int region : frontier) {
        for (int neighbor : M.adj[region]) {
          if (!seen[neighbor]) {
            seen[neighbor] = true;
            visible[neighbor] = true;
            next.push_back(neighbor);
          }
        }
      }
      frontier = std::move(next);
    }
  };
  for (const auto &w : S.warriors)
    if (w.id.side == M.my_side)
      add_hops(w.region, HOP_VISION);
  for (const auto &b : S.buildings)
    if (b.side == M.my_side)
      add_hops(b.region, HOP_VISION);

  std::vector<int> result;
  for (int region = 0; region < M.N; ++region)
    if (visible[region])
      result.push_back(region);
  return result;
}

static void parse_init(GameMap &M, GameState &S) {
  {
    auto t = tokens(readln());
    assert(t.size() >= 2 && t[0] == "READY");
    M.my_side = (t[1] == "LEFT") ? Side::LEFT : Side::RIGHT;
  }
  {
    auto t = tokens(readln());
    M.N = std::stoi(t.at(0));
    M.K = std::stoi(t.at(1));
  }
  M.x.assign(M.N, 0);
  M.y.assign(M.N, 0);
  {
    auto t = tokens(readln()); // x_0 x_1 ... x_{N-1}
    for (int i = 0; i < M.N; ++i)
      M.x[i] = std::stoll(t.at(i));
  }
  {
    auto t = tokens(readln()); // y_0 y_1 ... y_{N-1}
    for (int i = 0; i < M.N; ++i)
      M.y[i] = std::stoll(t.at(i));
  }
  {
    auto t = tokens(readln()); // K strongholds
    M.strongholds.clear();
    M.strongholds.reserve(t.size());
    for (const auto &s : t)
      M.strongholds.push_back(std::stoi(s));
    std::sort(M.strongholds.begin(), M.strongholds.end());
  }
  M.adj.assign(M.N, {});
  for (int r = 0; r < M.N; ++r) {
    auto t = tokens(readln()); // deg n_1 n_2 ...
    int deg = std::stoi(t.at(0));
    auto &nb = M.adj[r];
    nb.reserve(deg);
    for (int j = 0; j < deg; ++j)
      nb.push_back(std::stoi(t.at(1 + j)));
    std::sort(nb.begin(), nb.end());
  }

  M.my_hq = hq_of(M, M.my_side);
  M.opp_hq = hq_of(M, opposite(M.my_side));

  S = GameState{};
  S.gold = START_GOLD;
  Side opp = opposite(M.my_side);
  for (int sfx = 1; sfx <= START_WARRIORS; ++sfx) {
    S.warriors.push_back(Warrior{.id = WarriorId{M.my_side, sfx},
                                 .region = M.my_hq,
                                 .hp = HQ_LEVELS[1].warrior_hp});
    S.warriors.push_back(Warrior{.id = WarriorId{opp, sfx},
                                 .region = M.opp_hq,
                                 .hp = HQ_LEVELS[1].warrior_hp});
  }
  S.buildings.push_back(Building{hq_of(M, Side::LEFT), Side::LEFT, BType::HQ, 1,
                                 HQ_LEVELS[1].hp});
  S.buildings.push_back(Building{hq_of(M, Side::RIGHT), Side::RIGHT, BType::HQ,
                                 1, HQ_LEVELS[1].hp});

  std::cout << "OK" << std::endl;
}

static bool read_turn_start(int &turn_index) {
  std::string line = readln();
  if (line == "FINISH")
    return false;
  auto t = tokens(line);
  assert(!t.empty() && t[0] == "START");
  turn_index = std::stoi(t.at(2));
  return true;
}

static Building *find_building(GameState &S, int region) {
  for (auto &b : S.buildings)
    if (b.region == region)
      return &b;
  return nullptr;
}

static Warrior *find_warrior(GameState &S, WarriorId id) {
  for (auto &w : S.warriors)
    if (w.id == id)
      return &w;
  return nullptr;
}

static void read_turn_result(GameState &S, const GameMap &M,
                             const Actions &submitted) {
  for (int region : submitted.upgrades) {
    Building *b = find_building(S, region);
    if (b == nullptr) {
      S.gold -= BASE_LEVELS[1].cost;
    } else {
      // NOTE: only pay here. The sample also applied the upgrade here AND
      // again in the UPGRADE result section (double level-up). The result
      // section is authoritative, so the state change happens there only.
      if (b->level >= max_level(*b)) {
        int cost = (b->type == BType::HQ) ? HQ_HEAL_COST : BASE_HEAL_COST;
        S.gold -= cost;
      } else {
        S.gold -= upgrade_cost(*b);
      }
    }
  }

  const auto &built = submitted.upgrades;

  for (const auto &[id, target] : submitted.moves) {
    Building *b = find_building(S, target);
    bool own = std::find(built.begin(), built.end(), target) != built.end() ||
               (b != nullptr && b->side == M.my_side);
    int cost = own ? 0 : MOVE_COST;
    S.gold -= cost;
    if (Warrior *w = find_warrior(S, id)) {
      w->state = WState::MOVING;
      w->target = target;
    }
  }

  S.gold -= TRAIN_COST * submitted.train_n;

  {
    std::string line = readln();
    if (line == "FINISH")
      std::exit(0);
    auto t = tokens(line);
    assert(!t.empty() && t[0] == "TURN");
  }
  {
    auto t = tokens(readln());
    S.my_countdown = std::stoi(t.at(2));
    S.opp_countdown = std::stoi(t.at(4));
  }
  // UPGRADE
  {
    auto t = tokens(readln()); // "UPGRADE N"
    int n = std::stoi(t.at(1));
    for (int i = 0; i < n; ++i) {
      auto r = tokens(readln()); // "<A|B> <region>"
      int region = std::stoi(r.at(1));
      Building *b = find_building(S, region);
      if (b == nullptr) {
        S.buildings.push_back(make_base(region, M.my_side));
      } else if (b->level >= max_level(*b)) {
        b->hp = b->current_hp();
      } else {
        apply_upgrade(*b);
      }
    }
  }
  // TRAIN
  {
    auto t = tokens(readln()); // "TRAIN N"
    int n = std::stoi(t.at(1));
    if (n > 0) {
      auto ids = tokens(readln());
      for (int i = 0; i < n; ++i) {
        WarriorId id = parse_warrior(ids.at(i));
        Building *hq_b = find_building(S, M.my_hq);
        int hq_level = (hq_b != nullptr) ? hq_b->level : 1;
        S.warriors.push_back(Warrior{.id = id,
                                     .region = M.my_hq,
                                     .hp = HQ_LEVELS[hq_level].warrior_hp});
      }
    }
  }
  // MOVE
  {
    auto t = tokens(readln()); // "MOVE N"
    int n = std::stoi(t.at(1));
    for (int i = 0; i < n; ++i) {
      auto r = tokens(readln());
      WarriorId id = parse_warrior(r.at(0));
      int region = std::stoi(r.at(1));
      if (Warrior *w = find_warrior(S, id)) {
        w->region = region;
        if (w->state == WState::MOVING && w->region == w->target) {
          w->state = WState::STATIONARY;
        }
      }
    }
  }
  // DAMAGE
  std::vector<std::pair<WarriorId, int>> starved;
  {
    auto t = tokens(readln()); // "DAMAGE N"
    int n = std::stoi(t.at(1));
    for (int i = 0; i < n; ++i) {
      auto r = tokens(readln()); // "<cause> <id> <damage>"
      WarriorId id = parse_warrior(r.at(1));
      int damage = std::stoi(r.at(2));
      if (r.at(0) == "HUNGER") {
        starved.emplace_back(id, damage);
        continue;
      }
      if (Warrior *w = find_warrior(S, id))
        w->hp -= damage;
    }
    S.warriors.erase(std::remove_if(S.warriors.begin(), S.warriors.end(),
                                    [](const Warrior &w) { return w.hp <= 0; }),
                     S.warriors.end());
  }
  // SIEGE
  {
    auto t = tokens(readln()); // "SIEGE N"
    int n = std::stoi(t.at(1));
    for (int i = 0; i < n; ++i) {
      auto r = tokens(readln());
      int region = std::stoi(r.at(1));
      int damage = std::stoi(r.at(2));
      if (Building *b = find_building(S, region))
        b->hp -= damage;
    }
    S.buildings.erase(
        std::remove_if(S.buildings.begin(), S.buildings.end(),
                       [](const Building &b) { return b.hp <= 0; }),
        S.buildings.end());
  }
  S.visible = compute_visible(S, M);
  // WARRIOR
  {
    auto t = tokens(readln()); // "WARRIOR W"
    int w_count = std::stoi(t.at(1));
    std::vector<Warrior> seen_warriors;
    for (int i = 0; i < w_count; ++i) {
      auto r = tokens(readln()); // "<id> <region> <hp>"
      WarriorId id = parse_warrior(r.at(0));
      if (id.side == M.my_side)
        continue;
      seen_warriors.push_back(
          Warrior{.id = id, .region = std::stoi(r.at(1)), .hp = std::stoi(r.at(2))});
    }
    S.warriors.erase(std::remove_if(S.warriors.begin(), S.warriors.end(),
                                    [&](const Warrior &w) {
                                      return w.id.side != M.my_side;
                                    }),
                     S.warriors.end());
    for (auto &w : seen_warriors)
      S.warriors.push_back(w);
  }
  // BUILDING
  {
    auto t = tokens(readln()); // "BUILDING B"
    int b_count = std::stoi(t.at(1));
    std::vector<Building> seen_buildings;
    for (int i = 0; i < b_count; ++i) {
      auto r = tokens(readln()); // "<side> <region> <kind> <level> <hp>"
      Side s = parse_side_char(r.at(0)[0]);
      if (s == M.my_side)
        continue;
      BType bt = (r.at(2) == "HQ") ? BType::HQ : BType::BASE;
      seen_buildings.push_back(Building{std::stoi(r.at(1)), s, bt,
                                        std::stoi(r.at(3)), std::stoi(r.at(4))});
    }
    S.buildings.erase(std::remove_if(S.buildings.begin(), S.buildings.end(),
                                     [&](const Building &b) {
                                       return b.side != M.my_side;
                                     }),
                      S.buildings.end());
    for (auto &b : seen_buildings)
      S.buildings.push_back(b);
  }
  (void)readln(); // "END"

  int income = 0;
  for (const auto &b : S.buildings) {
    if (b.side != M.my_side)
      continue;
    int count = 0;
    for (const auto &w : S.warriors) {
      if (w.id.side == M.my_side && w.region == b.region)
        ++count;
    }
    income += WORK_INCOME * std::min(count, b.work_cap());
  }
  S.gold += income;

  int alive = 0;
  for (const auto &w : S.warriors)
    if (w.id.side == M.my_side)
      ++alive;
  S.gold -=
      UPKEEP_PER_WARRIOR * std::min(alive, S.gold / UPKEEP_PER_WARRIOR);

  for (const auto &[id, damage] : starved) {
    if (Warrior *w = find_warrior(S, id))
      w->hp -= damage;
  }
  S.warriors.erase(std::remove_if(S.warriors.begin(), S.warriors.end(),
                                  [](const Warrior &w) { return w.hp <= 0; }),
                   S.warriors.end());
}

struct Paths {
  std::vector<std::vector<double>> dist;
  std::vector<std::vector<int>> nxt;
};

static double euclid_ceil(const GameMap &M, int u, int v) {
  double dx = (double)(M.x[u] - M.x[v]);
  double dy = (double)(M.y[u] - M.y[v]);
  return std::ceil(std::sqrt(dx * dx + dy * dy));
}

static Paths calculate_paths(const GameMap &M) {
  const double INF = std::numeric_limits<double>::infinity();
  Paths P;
  P.dist.assign(M.N, std::vector<double>(M.N, INF));
  P.nxt.assign(M.N, std::vector<int>(M.N, -1));

  for (int i = 0; i < M.N; ++i) {
    P.dist[i][i] = 0.0;
    P.nxt[i][i] = i;
  }
  for (int u = 0; u < M.N; ++u) {
    for (int v : M.adj[u]) {
      double w = euclid_ceil(M, u, v);
      if (w < P.dist[u][v])
        P.dist[u][v] = w;
    }
  }

  for (int k = 0; k < M.N; ++k) {
    for (int u = 0; u < M.N; ++u) {
      if (P.dist[u][k] == INF)
        continue;
      for (int v = 0; v < M.N; ++v) {
        double cand = P.dist[u][k] + P.dist[k][v];
        if (cand < P.dist[u][v])
          P.dist[u][v] = cand;
      }
    }
  }

  for (int u = 0; u < M.N; ++u) {
    for (int v = 0; v < M.N; ++v) {
      if (u == v || P.dist[u][v] == INF)
        continue;
      double best_score = INF;
      for (int nb : M.adj[u]) {
        if (P.dist[nb][v] == INF)
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

static int next_step(const Paths &P, int u, int v) { return P.nxt[u][v]; }

static std::vector<int> path(const Paths &P, int u, int v) {
  std::vector<int> out;
  if (P.nxt[u][v] == -1)
    return out;
  out.push_back(u);
  while (u != v) {
    u = P.nxt[u][v];
    out.push_back(u);
  }
  return out;
}

static void emit_command() { std::cout << "COMMAND\n"; }

static void emit_actions(const Actions &a) {
  for (const auto &[id, target] : a.moves) {
    std::cout << "MOVE " << format_warrior(id) << ' ' << target << '\n';
  }
  for (int r : a.upgrades) {
    std::cout << "UPGRADE " << r << '\n';
  }
  if (a.train_n > 0) {
    std::cout << "TRAIN " << a.train_n << '\n';
  }
}

static void emit_end() { std::cout << "END" << std::endl; }

//////////////////////////////////////////////////////////////////////////////
// BABSANG — reproduction of the "따뜻한밥상" contest bot, reverse-engineered
// from 14 mid-evaluation replays (runs/contest/round_*/battle_*.log).
//
// Observed recipe (rounds 5-13, the stable version, perf 1650-2200):
//   T1      : UPGRADE HQ (->L2), TRAIN 1, two warriors -> centre stronghold.
//   T2-~88  : expansion. HQ keeps work_cap laborers. One claim in flight at a
//             time: MOVE a HQ laborer to the nearest unclaimed stronghold on
//             our half, TRAIN 1 to replace him.  Base built (L1 only, never
//             upgraded) as soon as gold >= 500.  ~7 bases by T88.
//   T88+    : army phase. TRAIN 1 every turn, everyone rallies at the centre
//             base. Save for HQ L3 once ~5 army warriors exist.
//   waves   : stack 14 at centre -> 13 go to enemy HQ (first, blind).
//             afterwards: nearest known enemy base with a force the combat
//             sim says is enough (~21-25 vs L3 base), return to centre when
//             the base is dead; enemy HQ with all-but-one when no enemy base
//             is known. Small groups go defend own bases when enemies show up.
//////////////////////////////////////////////////////////////////////////////
#include <map>
#include <set>

#ifndef BS_ARMY_TURN
#define BS_ARMY_TURN 88        // expansion -> army phase
#endif
#ifndef BS_HQ_STACK
#define BS_HQ_STACK 14         // centre stack that triggers the enemy-HQ wave
#endif
#ifndef BS_HQ_SAVE_AFTER
#define BS_HQ_SAVE_AFTER 5     // army warriors trained before saving for HQ L3
#endif
#ifndef BS_KEEP
#define BS_KEEP 0.40           // fraction of hp that must survive in the sim
#endif
#ifndef BS_ALLIN_TURN
#define BS_ALLIN_TURN 372      // everything to the enemy HQ from here
#endif
#ifndef BS_BASE_DEF
#define BS_BASE_DEF 3          // defenders assumed at an enemy base we cannot see
#endif
#ifndef BS_WAVE_MARGIN
#define BS_WAVE_MARGIN 2       // extra warriors on top of (building hp + defenders)
#endif
#ifndef BS_DEBUG
#define BS_DEBUG 0
#endif

enum class Role { HQ_LABOR, CLAIM, BASE_LABOR, ARMY, ATTACK, DEFEND };

struct Assign {
  Role role = Role::ARMY;
  int zone = -1;   // claim target / base zone / attack target / defended zone
};

struct Brain {
  std::map<int, Assign> roles;              // warrior num -> assignment
  std::map<int, Building> enemy_bld;        // last-known enemy buildings
  std::vector<int> dist_me, dist_opp, dist_c; // path costs (int)
  std::vector<std::vector<int>> hop;        // hop distances
  int centre = 0;
  bool army_phase = false;
  int army_trained = 0;
  int enemy_hq_level = 3;                   // assumed when unseen
  bool hq_wave_sent = false;
  std::map<int, int> defend_quiet;          // zone -> turns without enemies
  std::map<int, int> seen_near;             // enemy zone -> most enemies seen within 2 hops
};
static Brain B;

static std::vector<int> bfs_hops(const GameMap &M, int s) {
  std::vector<int> d(M.N, -1);
  std::vector<int> q{s};
  d[s] = 0;
  for (size_t i = 0; i < q.size(); ++i) {
    int u = q[i];
    for (int v : M.adj[u])
      if (d[v] < 0) { d[v] = d[u] + 1; q.push_back(v); }
  }
  return d;
}

static void brain_init(const GameMap &M, const Paths &P) {
  B.centre = (M.N - 1) / 2;
  B.dist_me.assign(M.N, 0); B.dist_opp.assign(M.N, 0); B.dist_c.assign(M.N, 0);
  for (int z = 0; z < M.N; ++z) {
    B.dist_me[z] = (int)P.dist[M.my_hq][z];
    B.dist_opp[z] = (int)P.dist[M.opp_hq][z];
    B.dist_c[z] = (int)P.dist[B.centre][z];
  }
  B.hop.resize(M.N);
  for (int z = 0; z < M.N; ++z) B.hop[z] = bfs_hops(M, z);
}

// ---------------------------------------------------------------- helpers
static const Building *bld_at(const GameState &S, int z) {
  for (const auto &b : S.buildings) if (b.region == z) return &b;
  return nullptr;
}
static const Building *my_bld_at(const GameState &S, const GameMap &M, int z) {
  const Building *b = bld_at(S, z);
  return (b && b->side == M.my_side) ? b : nullptr;
}
static bool is_visible(const GameState &S, int z) {
  return std::binary_search(S.visible.begin(), S.visible.end(), z);
}
static int enemies_in(const GameState &S, const GameMap &M, int z) {
  int c = 0;
  for (const auto &w : S.warriors) if (w.id.side != M.my_side && w.region == z) ++c;
  return c;
}
static std::vector<int> enemy_hps_within(const GameState &S, const GameMap &M, int z, int hops) {
  std::vector<int> v;
  for (const auto &w : S.warriors)
    if (w.id.side != M.my_side && B.hop[z][w.region] <= hops) v.push_back(w.hp);
  return v;
}
static int my_warrior_hp_now(const GameState &S, const GameMap &M) {
  const Building *hq = my_bld_at(S, M, M.my_hq);
  return HQ_LEVELS[hq ? hq->level : 1].warrior_hp;
}

// Combat simulation of `att` (our hp list) attacking a zone holding `def`
// (enemy warrior hps), a building with `bhp` hit points and `turret` shots.
// Returns true if the building dies; `rem` = our remaining hp.
static bool sim_fight(std::vector<int> att, std::vector<int> def, int bhp, int turret, int &rem) {
  auto hit_lowest = [](std::vector<int> &v) {
    int best = -1;
    for (size_t i = 0; i < v.size(); ++i)
      if (v[i] > 0 && (best < 0 || v[i] < v[best])) best = (int)i;
    if (best >= 0) v[best]--;
    return best >= 0;
  };
  for (int t = 0; t < 40; ++t) {
    if (att.empty()) break;
    int a = (int)att.size(), d = (int)def.size();
    for (int k = 0; k < turret; ++k) hit_lowest(att);
    for (int k = 0; k < d; ++k) hit_lowest(att);
    for (int k = 0; k < a; ++k) {
      if (!hit_lowest(def)) bhp--;
    }
    att.erase(std::remove_if(att.begin(), att.end(), [](int h) { return h <= 0; }), att.end());
    def.erase(std::remove_if(def.begin(), def.end(), [](int h) { return h <= 0; }), def.end());
    if (bhp <= 0) { rem = 0; for (int h : att) rem += h; return true; }
  }
  rem = 0; for (int h : att) rem += h;
  return false;
}
// Smallest attacker count that kills the target and keeps BS_KEEP of its hp.
static int required_force(int myhp, const std::vector<int> &def, int bhp, int turret, int cap = 60) {
  for (int n = 1; n <= cap; ++n) {
    std::vector<int> att(n, myhp);
    int rem = 0;
    if (sim_fight(att, def, bhp, turret, rem) && rem >= BS_KEEP * n * myhp) return n;
  }
  return 1 << 20;
}

// ---------------------------------------------------------------- decide
static Actions decide(const GameState &S, const GameMap &M, const Paths &P, int turn) {
  Actions a;
  int budget = S.gold - 5;  // small safety margin against tracking drift
  const int myhp = my_warrior_hp_now(S, M);

  // ---- own warriors (stationary ones can take orders) -----------------------
  std::vector<const Warrior *> mine;
  std::set<int> alive;
  for (const auto &w : S.warriors)
    if (w.id.side == M.my_side) { mine.push_back(&w); alive.insert(w.id.num); }
  for (auto it = B.roles.begin(); it != B.roles.end();)
    it = alive.count(it->first) ? std::next(it) : B.roles.erase(it);
  for (const auto *w : mine)
    if (!B.roles.count(w->id.num)) B.roles[w->id.num] = Assign{Role::HQ_LABOR, M.my_hq};

  // ---- enemy building memory ------------------------------------------------
  for (int z : S.visible) B.enemy_bld.erase(z);
  for (const auto &b : S.buildings)
    if (b.side != M.my_side) {
      B.enemy_bld[b.region] = b;
      if (b.type == BType::HQ) B.enemy_hq_level = b.level;
      int near = (int)enemy_hps_within(S, M, b.region, 2).size();
      B.seen_near[b.region] = std::max(near, B.seen_near[b.region] / 2);  // decays when it looks empty
    }
  if (!B.enemy_bld.count(M.opp_hq)) {
    Building hq{M.opp_hq, opposite(M.my_side), BType::HQ, B.enemy_hq_level, HQ_LEVELS[B.enemy_hq_level].hp};
    B.enemy_bld[M.opp_hq] = hq;   // always assumed alive (else the game is over)
  }
  const Building *my_hq = my_bld_at(S, M, M.my_hq);
  const int hq_level = my_hq ? my_hq->level : 1;
  const int hq_cap = HQ_LEVELS[hq_level].work_cap;

  std::set<int> upgraded_zones;
  auto upgrade = [&](int z, int cost) {
    a.upgrades.push_back(z); upgraded_zones.insert(z); budget -= cost;
  };
  std::set<int> moved;
  auto move = [&](const Warrior *w, int dest) {
    bool own = my_bld_at(S, M, dest) != nullptr || upgraded_zones.count(dest);
    int cost = own ? 0 : MOVE_COST;
    if (budget < cost) return false;
    a.moves.emplace_back(w->id, dest); moved.insert(w->id.num); budget -= cost;
    return true;
  };
  auto can_order = [&](const Warrior *w) {
    return w->state == WState::STATIONARY && !moved.count(w->id.num);
  };

  // ---- rally point ----------------------------------------------------------
  int rally = B.centre;
  if (B.enemy_bld.count(B.centre) && !my_bld_at(S, M, B.centre)) {
    rally = M.my_hq;
    int best = 1 << 30;
    for (const auto &b : S.buildings)
      if (b.side == M.my_side && b.type == BType::BASE && B.dist_c[b.region] < best) {
        best = B.dist_c[b.region]; rally = b.region;
      }
  }

  // ---- turn 1 ---------------------------------------------------------------
  if (turn == 1) {
    upgrade(M.my_hq, HQ_LEVELS[2].upgrade_cost);
    a.train_n = 1; budget -= TRAIN_COST;
    int sent = 0;
    for (const auto *w : mine) {
      if (sent >= 2) break;
      if (move(w, B.centre)) {
        B.roles[w->id.num] = Assign{sent == 0 ? Role::CLAIM : Role::ARMY, B.centre};
        ++sent;
      }
    }
    return a;
  }

  if (!B.army_phase && turn >= BS_ARMY_TURN) B.army_phase = true;

  // ---- 0. the centre is claimed first, always --------------------------------
  if (!bld_at(S, B.centre) && !B.enemy_bld.count(B.centre)) {
    bool has_claim = false;
    for (const auto &[num, as] : B.roles) if (as.role == Role::CLAIM && as.zone == B.centre) has_claim = true;
    if (!has_claim) {
      const Warrior *pick = nullptr;
      for (const auto *w : mine) {
        const Assign &as = B.roles[w->id.num];
        if (as.role == Role::ARMY && (w->region == B.centre || (pick == nullptr && can_order(w))))
          if (pick == nullptr || w->region == B.centre) pick = w;
      }
      if (pick) B.roles[pick->id.num] = Assign{Role::CLAIM, B.centre};
    }
  }

  // ---- 1. builds on claimed strongholds ------------------------------------
  for (const auto *w : mine) {
    Assign &as = B.roles[w->id.num];
    if (as.role != Role::CLAIM) continue;
    if (B.enemy_bld.count(as.zone)) { as = Assign{Role::ARMY, rally}; continue; }
    if (my_bld_at(S, M, as.zone)) { as = Assign{Role::BASE_LABOR, as.zone}; continue; }
    if (w->region != as.zone) continue;
    if (enemies_in(S, M, as.zone) > 0) continue;
    if (budget >= BASE_LEVELS[1].cost && !upgraded_zones.count(as.zone)) {
      upgrade(as.zone, BASE_LEVELS[1].cost);
      as = Assign{Role::BASE_LABOR, as.zone};
    }
  }

  // ---- 2. HQ -> L3 once the army phase is running ---------------------------
  bool saving_hq = B.army_phase && hq_level < 3 && B.army_trained >= BS_HQ_SAVE_AFTER;
  if (saving_hq && budget >= HQ_LEVELS[3].upgrade_cost && enemies_in(S, M, M.my_hq) == 0 &&
      !upgraded_zones.count(M.my_hq)) {
    bool someone = false;
    for (const auto *w : mine) if (w->region == M.my_hq) someone = true;
    if (someone) { upgrade(M.my_hq, HQ_LEVELS[3].upgrade_cost); saving_hq = false; }
  }

  // ---- 3. group bookkeeping -------------------------------------------------
  // members of attack groups: return when the target is gone, retarget when
  // the fight at the target is hopeless.
  std::map<int, std::vector<const Warrior *>> attack_groups, defend_groups;
  for (const auto *w : mine) {
    const Assign &as = B.roles[w->id.num];
    if (as.role == Role::ATTACK) attack_groups[as.zone].push_back(w);
    if (as.role == Role::DEFEND) defend_groups[as.zone].push_back(w);
  }
  for (auto &[tz, ws] : attack_groups) {
    bool target_alive = B.enemy_bld.count(tz) > 0;
    bool all_there = true;
    for (const auto *w : ws) if (w->region != tz || w->state != WState::STATIONARY) all_there = false;
    if (!target_alive) {
      for (const auto *w : ws) B.roles[w->id.num] = Assign{Role::ARMY, rally};
      continue;
    }
    if (!all_there) continue;
    // fight in progress at the target: evaluate with what we can see
    std::vector<int> att, def = enemy_hps_within(S, M, tz, 0);
    for (const auto *w : ws) att.push_back(w->hp);
    const Building &tb = B.enemy_bld[tz];
    int rem = 0;
    bool ok = sim_fight(att, def, tb.hp, tb.type == BType::HQ ? HQ_LEVELS[tb.level].turret
                                                                : BASE_LEVELS[tb.level].turret, rem);
    if (ok) continue;
    // hopeless: go for the nearest known enemy base we can beat, else go home
    int best = -1, bestd = 1 << 30;
    for (const auto &[z, b] : B.enemy_bld) {
      if (b.type == BType::HQ || z == tz) continue;
      std::vector<int> d2 = enemy_hps_within(S, M, z, 2);
      if ((int)d2.size() < B.seen_near[z]) d2.resize(B.seen_near[z], HQ_LEVELS[B.enemy_hq_level].warrior_hp);
      int rem2 = 0, tot = 0;
      for (int h : att) tot += h;
      if (!sim_fight(att, d2, b.hp, BASE_LEVELS[b.level].turret, rem2) || rem2 < BS_KEEP * tot) continue;
      int d = (int)P.dist[tz][z];
      if (d < bestd) { bestd = d; best = z; }
    }
    for (const auto *w : ws) B.roles[w->id.num] = best >= 0 ? Assign{Role::ATTACK, best} : Assign{Role::ARMY, rally};
  }
  // defenders: go home when the zone has been quiet for a few turns
  for (auto &[dz, ws] : defend_groups) {
    int near = (int)enemy_hps_within(S, M, dz, 2).size();
    int &q = B.defend_quiet[dz];
    q = near > 0 ? 0 : q + 1;
    if (q >= 3 || !my_bld_at(S, M, dz)) {
      for (const auto *w : ws) B.roles[w->id.num] = Assign{Role::ARMY, rally};
    }
  }

  // ---- 4. the centre stack ----------------------------------------------------
  std::vector<const Warrior *> stack;   // orderable ARMY warriors at the rally
  for (const auto *w : mine) {
    const Assign &as = B.roles[w->id.num];
    if (as.role == Role::ARMY && w->region == rally && can_order(w)) stack.push_back(w);
  }
  std::sort(stack.begin(), stack.end(), [](const Warrior *x, const Warrior *y) { return x->hp > y->hp; });
  auto dispatch = [&](int n, Role role, int zone) {
    int sent = 0;
    for (const auto *w : stack) {
      if (sent >= n) break;
      if (!can_order(w)) continue;
      if (!move(w, zone)) break;
      B.roles[w->id.num] = Assign{role, zone};
      ++sent;
    }
    stack.erase(std::remove_if(stack.begin(), stack.end(),
                               [&](const Warrior *w) { return moved.count(w->id.num) > 0; }),
                stack.end());
    return sent;
  };

  // ---- 5. defence: enemies near an own building -----------------------------
  for (const auto &b : S.buildings) {
    if (b.side != M.my_side || b.region == rally) continue;
    std::vector<int> near = enemy_hps_within(S, M, b.region, 2);
    if (near.empty()) continue;
    int have = 0;
    for (const auto *w : mine)
      if (B.roles[w->id.num].role == Role::DEFEND && B.roles[w->id.num].zone == b.region) ++have;
    int total = 0; for (int h : near) total += h;
    int need = (total + myhp - 1) / myhp + 1 - have;
    if (need > 0 && !stack.empty()) dispatch(std::min(need, (int)stack.size()), Role::DEFEND, b.region);
  }

  // ---- 6. waves -------------------------------------------------------------
  bool saving_wave = false;   // a wave is due but we cannot pay its move cost yet
  if (turn >= BS_ALLIN_TURN) {
    dispatch((int)stack.size(), Role::ATTACK, M.opp_hq);
  } else if (B.army_phase && attack_groups.empty() && !stack.empty()) {
    // nearest known enemy base (from the rally) we can beat
    int best = -1, bestd = 1 << 30, bestn = 0;
    for (const auto &[z, b] : B.enemy_bld) {
      if (b.type == BType::HQ) continue;
      std::vector<int> def = enemy_hps_within(S, M, z, 2);
      int assume = std::max({(int)def.size(), B.seen_near[z], BS_BASE_DEF});
      if ((int)def.size() < assume) def.resize(assume, HQ_LEVELS[B.enemy_hq_level].warrior_hp);
      int n = required_force(myhp, def, b.hp, BASE_LEVELS[b.level].turret, (int)stack.size() - 1);
      n = std::max(n, b.hp + assume + BS_WAVE_MARGIN);   // observed: ~21-25 vs an L3 base
      if (n > (int)stack.size() - 1) continue;
      int d = (int)P.dist[rally][z];
      if (d < bestd) { bestd = d; best = z; bestn = n; }
    }
    if (best >= 0) {
      int n = std::min((int)stack.size() - 1, bestn);
      if (budget >= MOVE_COST * n) dispatch(n, Role::ATTACK, best);
      else saving_wave = true;
    } else {
      bool any_base_known = false;
      for (const auto &[z, b] : B.enemy_bld) if (b.type != BType::HQ) any_base_known = true;
      if (!any_base_known && (int)stack.size() >= BS_HQ_STACK) {
        std::vector<int> def = enemy_hps_within(S, M, M.opp_hq, 2);
        int assume = 4;
        if ((int)def.size() < assume) def.resize(assume, HQ_LEVELS[B.enemy_hq_level].warrior_hp);
        const Building &hq = B.enemy_bld[M.opp_hq];
        int n = required_force(myhp, def, hq.hp, HQ_LEVELS[hq.level].turret, (int)stack.size() - 1);
        if (n <= (int)stack.size() - 1) {
          int k = (int)stack.size() - 1;
          if (budget >= MOVE_COST * k) { dispatch(k, Role::ATTACK, M.opp_hq); B.hq_wave_sent = true; }
          else saving_wave = true;
        }
      }
    }
  }

  // ---- 7. HQ laborers / claims / training -----------------------------------
  std::vector<const Warrior *> at_hq;
  for (const auto *w : mine)
    if (w->region == M.my_hq && can_order(w) && B.roles[w->id.num].role == Role::HQ_LABOR) at_hq.push_back(w);
  std::sort(at_hq.begin(), at_hq.end(), [](const Warrior *x, const Warrior *y) { return x->id.num < y->id.num; });

  bool claim_in_flight = false;
  for (const auto &[num, as] : B.roles) if (as.role == Role::CLAIM) claim_in_flight = true;

  int next_claim = -1;
  if (!B.army_phase && !claim_in_flight) {
    int bestd = 1 << 30;
    for (int z : M.strongholds) {
      if (z == B.centre || bld_at(S, z) || B.enemy_bld.count(z)) continue;
      if (B.dist_me[z] >= B.dist_opp[z]) continue;
      bool taken = false;
      for (const auto &[num, as] : B.roles) if (as.zone == z && (as.role == Role::CLAIM || as.role == Role::BASE_LABOR)) taken = true;
      if (taken) continue;
      if (B.dist_me[z] < bestd) { bestd = B.dist_me[z]; next_claim = z; }
    }
  }

  int train = 0;
  // income guard: never train into starvation
  int income = 0, count = 0;
  for (const auto &b : S.buildings) {
    if (b.side != M.my_side) continue;
    int c = 0;
    for (const auto *w : mine) if (w->region == b.region) ++c;
    income += WORK_INCOME * std::min(c, b.work_cap());
  }
  for (const auto *w : mine) ++count;
  auto can_feed = [&](int extra) { return income >= UPKEEP_PER_WARRIOR * (count + extra); };

  if (!B.army_phase) {
    if (next_claim >= 0 && budget >= TRAIN_COST + MOVE_COST && can_feed(1)) {
      train = 1;
      // send a laborer now; the trainee replaces him this evening
      if (!at_hq.empty()) {
        const Warrior *w = at_hq.back(); at_hq.pop_back();
        if (move(w, next_claim)) B.roles[w->id.num] = Assign{Role::CLAIM, next_claim};
      }
    }
  } else if (!saving_hq && !saving_wave) {
    int cap = HQ_LEVELS[hq_level].train_cap;
    while (train < cap && budget >= TRAIN_COST * (train + 1) && can_feed(train + 1)) ++train;
  }
  if (train > 0) { a.train_n = train; budget -= TRAIN_COST * train; B.army_trained += B.army_phase ? train : 0; }

  // spare HQ warriors (beyond work cap after today's training) join the army
  int hq_after = (int)at_hq.size() + train;
  for (const auto *w : mine)   // non-laborers standing at HQ also count
    if (w->region == M.my_hq && B.roles[w->id.num].role != Role::HQ_LABOR && !moved.count(w->id.num)) ++hq_after;
  int spare = hq_after - hq_cap;
  while (spare > 0 && !at_hq.empty()) {
    const Warrior *w = at_hq.back(); at_hq.pop_back();
    if (move(w, rally)) { B.roles[w->id.num] = Assign{Role::ARMY, rally}; --spare; }
    else break;
  }

  // ---- 8. everybody else: ARMY warriors not at the rally walk there ----------
  for (const auto *w : mine) {
    const Assign &as = B.roles[w->id.num];
    if (!can_order(w)) continue;
    if (as.role == Role::ARMY && w->region != rally) move(w, rally);
    else if (as.role == Role::CLAIM && w->region != as.zone) move(w, as.zone);
    else if (as.role == Role::ATTACK && w->region != as.zone) move(w, as.zone);
    else if (as.role == Role::DEFEND && w->region != as.zone) move(w, as.zone);
    else if (as.role == Role::BASE_LABOR && w->region != as.zone) move(w, as.zone);
  }
#if BS_DEBUG
  std::cerr << "T" << turn << " gold=" << S.gold << " army=" << mine.size() << " stack=" << stack.size()
            << " phase=" << B.army_phase << " rally=" << rally << " enemy_bld=" << B.enemy_bld.size() << "\n";
#endif
  return a;
}

int main() {
  GameMap M;
  GameState S;
  parse_init(M, S);
  Paths P = calculate_paths(M);
  brain_init(M, P);

  int turn;
  while (read_turn_start(turn)) {
    Actions a = decide(S, M, P, turn);
    emit_command();
    emit_actions(a);
    emit_end();
    read_turn_result(S, M, a);
#if BS_DEBUG
    {
      std::string W, Bs, EB, EW;
      std::vector<const Warrior *> ws;
      for (const auto &w : S.warriors) ws.push_back(&w);
      std::sort(ws.begin(), ws.end(), [](const Warrior *x, const Warrior *y) { return x->id.num < y->id.num; });
      for (const auto *w : ws) {
        std::string e = format_warrior(w->id) + ":" + std::to_string(w->region) + ":" + std::to_string(w->hp);
        (w->id.side == M.my_side ? W : EW) += (w->id.side == M.my_side ? W : EW).empty() ? e : "," + e;
      }
      for (const auto &b : S.buildings) {
        std::string e = std::to_string(b.region) + ":" + (b.type == BType::HQ ? "HQ" : "BASE") + ":" +
                        std::to_string(b.level) + ":" + std::to_string(b.hp);
        if (b.side == M.my_side) Bs += Bs.empty() ? e : "," + e;
        else EB += (EB.empty() ? e : "," + e) + ":v";
      }
      std::cerr << "BOT t=" << turn << " gold=" << S.gold << " vis=" << S.visible.size() << " W=" << W << " B=" << Bs << " EB=" << EB << " EW=" << EW << "\n";
    }
#endif
  }
  return 0;
}
