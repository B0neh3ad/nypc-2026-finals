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
      if (b->level >= max_level(*b)) {
        int cost = (b->type == BType::HQ) ? HQ_HEAL_COST : BASE_HEAL_COST;
        S.gold -= cost;
        b->hp = b->current_hp();
      } else {
        S.gold -= upgrade_cost(*b);
        apply_upgrade(*b);
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

//////////////////////////////////
//// WRITE YOUR STRATEGY HERE ////
//////////////////////////////////
/* ===== MASS RUSH (2026-08-29) =============================================
   "N명 모아서 한 번에 보내는" 봇. staged_rush(7명, T5)와 달리 병력을 모으려면
   경제가 필요해서 구조가 완전히 다르다.

   왜 그런가 — 규칙 산수:
     훈련 120g, 식비 2g/명/일, 노동 15 × min(구역 아군수, 건물 노동가능수).
     본부 Lv1은 훈련 1/턴·노동 1명뿐이라 수입이 15/일이다.
     15명이면 식비만 30/일이라 본부 하나로는 **적자**다.
     그래서 먼저 거점에 기지를 지어 노동 슬롯을 늘려야 병력이 쌓인다.

   단계:
     1) 확장  — 가까운 미점유 거점으로 전사를 보내 기지 건설, 노동 배치
     2) 축적  — 금화 여유가 있으면 매턴 훈련(본부 훈련 상한까지)
     3) 집결  — 총 병력이 MASS_TARGET 이 되면 전방 아군 기지로 모은다
                (목적지가 아군 건물이면 이동 비용 0 — 대군 집결이 공짜다)
     4) 돌격  — 집결 완료 시 전원 적 본부로. 이후 증원은 계속 흘려보낸다

   손잡이:
     MASS_TARGET            모을 병력 수 (15 / 30)
     MASS_MIN_BASES         이 수만큼 기지를 먼저 확보하고 축적 시작
     MASS_WORKERS_PER_BASE  기지당 남길 노동 인원
     MASS_FAILSAFE_TURN     이 턴이면 목표 미달이라도 돌격
     MASS_HQ_UPGRADE        1이면 여유 시 본부 업그레이드(훈련 상한·노동 증가)
   ========================================================================= */

#ifndef MASS_TARGET
#define MASS_TARGET 15
#endif
#ifndef MASS_MIN_BASES
#define MASS_MIN_BASES 4
#endif
#ifndef MASS_WORKERS_PER_BASE
#define MASS_WORKERS_PER_BASE 1
#endif
#ifndef MASS_FAILSAFE_TURN
#define MASS_FAILSAFE_TURN 220
#endif
#ifndef MASS_HQ_UPGRADE
#define MASS_HQ_UPGRADE 1
#endif
#ifndef MASS_GOLD_FLOOR
#define MASS_GOLD_FLOOR 60 /* 식비용 최소 잔고 */
#endif

#ifndef MASS_SAFETY
#define MASS_SAFETY 20 /* 회계 오차 여유 */
#endif

static int g_mass_phase = 0; /* 0 확장·축적, 1 집결, 2 돌격 */
static int g_rally = -1;

static const Building *my_building_at(const GameState &S, const GameMap &M, int r) {
  for (const auto &b : S.buildings)
    if (b.region == r && b.side == M.my_side) return &b;
  return nullptr;
}
static bool enemy_warrior_at(const GameState &S, const GameMap &M, int r) {
  for (const auto &w : S.warriors)
    if (w.region == r && w.hp > 0 && w.id.side != M.my_side) return true;
  return false;
}
static int my_army(const GameState &S, const GameMap &M) {
  int n = 0;
  for (const auto &w : S.warriors)
    if (w.id.side == M.my_side && w.hp > 0) ++n;
  return n;
}
/* 적 본부에 가장 가까운 내 건물 = 집결지. 아군 건물이라 이동이 무료다. */
static int pick_rally(const GameState &S, const GameMap &M, const Paths &P) {
  int best = M.my_hq;
  double bd = P.dist[M.opp_hq][M.my_hq];
  for (const auto &b : S.buildings) {
    if (b.side != M.my_side) continue;
    double d = P.dist[M.opp_hq][b.region];
    if (d < bd) { bd = d; best = b.region; }
  }
  return best;
}

static Actions decide(const GameState &S, const GameMap &M, const Paths &P,
                      int turn) {
  Actions a;
  int gold = S.gold;
  /* 이동은 목적지가 아군 건물이 아니면 10g. 잔고를 넘겨 명령하면 규칙 위반(WA)이라
     한 판을 통째로 잃는다. 집결/돌격에서 이걸 빼먹어 T225 에 죽었다. */
  auto try_move = [&](const WarriorId &id, int dst) -> bool {
    bool free_move = false;
    for (const auto &b : S.buildings)
      if (b.region == dst && b.side == M.my_side) { free_move = true; break; }
    if (!free_move) {
      if (gold < MOVE_COST) return false;
      gold -= MOVE_COST;
    }
    a.moves.emplace_back(id, dst);
    return true;
  };

  /* 내 건물별로 노동 인원을 세어 둔다 */
  std::vector<int> worker(M.N, 0);
  for (const auto &w : S.warriors)
    if (w.id.side == M.my_side && w.hp > 0 && w.state == WState::STATIONARY)
      worker[w.region]++;

  int my_bases = 0;
  for (const auto &b : S.buildings)
    if (b.side == M.my_side && b.type == BType::BASE) ++my_bases;

  /* ── 1) 건설: 내 전사가 있고 적이 없는 미점유 거점 ────────────────── */
  for (int r : M.strongholds) {
    if (gold < BASE_LEVELS[1].cost) break;
    if (my_building_at(S, M, r) != nullptr) continue;
    if (enemy_warrior_at(S, M, r)) continue;
    if (worker[r] <= 0) continue;
    a.upgrades.push_back(r);
    gold -= BASE_LEVELS[1].cost;
    ++my_bases;
  }

#if MASS_HQ_UPGRADE
  /* 본부 업그레이드: 기지가 충분히 깔린 뒤에만. 훈련 상한과 노동이 늘어난다. */
  {
    const Building *hq = my_building_at(S, M, M.my_hq);
    if (hq != nullptr && hq->level < HQ_MAX_LEVEL && my_bases >= MASS_MIN_BASES &&
        g_mass_phase == 0 && gold >= HQ_LEVELS[hq->level + 1].upgrade_cost + 400) {
      bool dup = false;
      for (int r : a.upgrades) if (r == M.my_hq) dup = true;
      if (!dup) {
        a.upgrades.push_back(M.my_hq);
        gold -= HQ_LEVELS[hq->level + 1].upgrade_cost;
      }
    }
  }
#endif

  /* ── 단계 전이 ──────────────────────────────────────────────────── */
  int army = my_army(S, M);
  if (g_mass_phase == 0 && (army >= MASS_TARGET || turn >= MASS_FAILSAFE_TURN)) {
    g_mass_phase = 1;
    g_rally = pick_rally(S, M, P);
  }

  /* ── 3) 확장 이동: 아직 축적 단계면 가까운 미점유 거점으로 한 명씩 ── */
  if (g_mass_phase == 0) {
    for (int r : M.strongholds) {
      if (my_building_at(S, M, r) != nullptr) continue;
      if (enemy_warrior_at(S, M, r)) continue;
      if (worker[r] > 0) continue;              /* 이미 가는 중/도착 */
      /* 본부에 여유 인원이 있으면 보낸다 (노동 1명은 남긴다) */
      const Warrior *pick = nullptr;
      int home = 0;
      for (const auto &w : S.warriors) {
        if (w.id.side != M.my_side || w.hp <= 0) continue;
        if (w.state != WState::STATIONARY || w.region != M.my_hq) continue;
        if (++home > 1 && pick == nullptr) pick = &w;
      }
      if (pick == nullptr) break;
      /* 이동 비용: 목적지가 아군 건물이 아니면 10. 거점은 아직 비어 있으므로 유료.
         이걸 빼먹으면 다음 턴 건설에서 잔고가 모자라 WA 로 즉사한다. */
      if (gold < MOVE_COST + MASS_SAFETY) break;
      if (!try_move(pick->id, r)) break;
      worker[M.my_hq]--;
      break;                                     /* 한 턴에 한 곳씩 */
    }
  }

  /* 이번 턴에 돌격으로 전이했는지 — 전이 턴에 5)까지 돌면 같은 전사에게
     명령이 두 번 나가 규칙 위반(WA)이 된다. 실제로 T220에 그렇게 죽었다. */
  bool just_struck = false;

  /* ── 4) 집결: 기지 노동 인원만 남기고 전부 집결지로 (아군 건물=무료) ─ */
  if (g_mass_phase == 1) {
    if (g_rally < 0) g_rally = pick_rally(S, M, P);
    std::vector<int> keep(M.N, 0);
    for (const auto &b : S.buildings)
      if (b.side == M.my_side && b.region != g_rally)
        keep[b.region] = MASS_WORKERS_PER_BASE;

    int at_rally = 0;
    for (const auto &w : S.warriors) {
      if (w.id.side != M.my_side || w.hp <= 0) continue;
      if (w.state != WState::STATIONARY) continue;
      if (w.region == g_rally) { ++at_rally; continue; }
      if (keep[w.region] > 0) { keep[w.region]--; continue; }
      try_move(w.id, g_rally);
    }
    if (at_rally >= MASS_TARGET || turn >= MASS_FAILSAFE_TURN) {
      a.moves.clear();
      gold = S.gold;
      for (int r : a.upgrades)
        gold -= (r == M.my_hq) ? 0 : BASE_LEVELS[1].cost;
      for (const auto &w : S.warriors)
        if (w.id.side == M.my_side && w.hp > 0 &&
            w.state == WState::STATIONARY && w.region == g_rally)
          try_move(w.id, M.opp_hq);
      g_mass_phase = 2;
      just_struck = true;
    }
  }

  /* ── 5) 돌격 이후: 집결지·본부의 여유 병력을 계속 전선으로 ─────────── */
  if (g_mass_phase == 2 && !just_struck) {
    std::vector<int> keep(M.N, 0);
    for (const auto &b : S.buildings)
      if (b.side == M.my_side) keep[b.region] = MASS_WORKERS_PER_BASE;
    for (const auto &w : S.warriors) {
      if (w.id.side != M.my_side || w.hp <= 0) continue;
      if (w.state != WState::STATIONARY) continue;
      if (keep[w.region] > 0) { keep[w.region]--; continue; }
      try_move(w.id, M.opp_hq);
    }
  }
  /* ── 6) 훈련: 이번 턴 지출을 모두 반영한 잔고로 (게임 순서: 건설->이동->훈련) ─
        집결/돌격 단계의 이동도 비용이 든다. 목적지가 아군 건물이면 무료. */
  {
    const Building *hq = my_building_at(S, M, M.my_hq);
    int cap = hq != nullptr ? HQ_LEVELS[hq->level].train_cap : 1;
    int n = 0;
    while (n < cap && gold >= TRAIN_COST + MASS_GOLD_FLOOR + MASS_SAFETY) {
      ++n; gold -= TRAIN_COST;
    }
    a.train_n = n;
  }
  return a;
}

int main() {
  GameMap M;
  GameState S;
  parse_init(M, S);
  Paths P = calculate_paths(M);

  int turn;
  while (read_turn_start(turn)) {
    Actions a = decide(S, M, P, turn);
    emit_command();
    emit_actions(a);
    emit_end();
    read_turn_result(S, M, a);
  }
  return 0;
}
