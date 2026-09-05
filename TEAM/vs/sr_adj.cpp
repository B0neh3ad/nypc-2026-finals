/* staged_rush 변형: {'RUSH_STAGE_PICK': 1} */
#define RUSH_STAGE_PICK 1
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
/* ===== STAGED RUSH (2026-08-29, rev2) =====================================
   round_13~15에서 Dorillas / NEXT NATION / Re:Entropy 가 쓴 신형 러시 재현
   + 강화 손잡이. 기본값은 관측 로그와 동일하게 동작한다.

   실측 시퀀스: T1~5 훈련 -> T5 7명을 적 본부 인근 거점으로 -> 전원 도착 시
   본부 재돌격 (T11~20, 맵 크기에 따라 자동).

   손잡이:
     RUSH_STAGED 0              구형 직행 러시로 동작
     RUSH_TRAIN_TURNS / RUSH_LAUNCH_TURN   파티 크기·출격 턴
     RUSH_STAGE_PICK            0=본부 최근접 거점(관측 그대로)
                                1=본부 인접 일반 구역(경유->본부가 한 걸음)
                                2=본부에서 두 번째로 가까운 거점
     RUSH_STRAGGLERS_OK         낙오 허용 — 이 수만큼 덜 모여도 돌격
     RUSH_FORWARD_REINF         1=새 훈련병을 경유지/본부로 계속 파견 (증원 스트림)
     RUSH_HOME_WORKERS          본부 노동 잔류 인원 (증원 파견 시 유지 수)
   ========================================================================= */

#ifndef RUSH_TRAIN_TURNS
#define RUSH_TRAIN_TURNS 5
#endif
#ifndef RUSH_LAUNCH_TURN
#define RUSH_LAUNCH_TURN 5
#endif
#ifndef RUSH_KEEP_HOME
#define RUSH_KEEP_HOME 0 /* T5 훈련병이 본부 노동을 맡는다 */
#endif
#ifndef RUSH_STAGED
#define RUSH_STAGED 1
#endif
#ifndef RUSH_REDIRECT_FAILSAFE_TURN
#define RUSH_REDIRECT_FAILSAFE_TURN 30
#endif
#ifndef RUSH_REINFORCE
#define RUSH_REINFORCE 1
#endif
#ifndef RUSH_STAGE_PICK
#define RUSH_STAGE_PICK 0
#endif
#ifndef RUSH_STRAGGLERS_OK
#define RUSH_STRAGGLERS_OK 0
#endif
#ifndef RUSH_FORWARD_REINF
#define RUSH_FORWARD_REINF 0
#endif
#ifndef RUSH_HOME_WORKERS
#define RUSH_HOME_WORKERS 1
#endif

static int g_stage_region = -1;
static int g_redirected = 0;
static std::vector<WarriorId> g_party;

static int pick_stage_region(const GameMap &M, const Paths &P) {
#if RUSH_STAGE_PICK == 1
  /* 적 본부 인접 일반 구역: 재돌격 후 한 걸음에 진입 -> 반응 시간 최소 */
  int best = -1;
  double bd = std::numeric_limits<double>::infinity();
  for (int r : M.adj[M.opp_hq]) {
    double d = P.dist[M.my_hq][r];   /* 우리 쪽에서 가기 쉬운 것 */
    if (d < bd) { bd = d; best = r; }
  }
  if (best >= 0) return best;
#endif
  /* 거점 중 적 본부 최근접 (PICK==2 면 두 번째) */
  int b1 = -1, b2 = -1;
  double d1 = std::numeric_limits<double>::infinity(), d2 = d1;
  for (int i = 0; i < (int)M.strongholds.size(); ++i) {
    int r = M.strongholds[i];
    double d = P.dist[M.opp_hq][r];
    if (d < d1) { d2 = d1; b2 = b1; d1 = d; b1 = r; }
    else if (d < d2) { d2 = d; b2 = r; }
  }
#if RUSH_STAGE_PICK == 2
  if (b2 >= 0) return b2;
#endif
  return b1 >= 0 ? b1 : M.opp_hq;
}

static Actions decide(const GameState &S, const GameMap &M, const Paths &P,
                      int turn) {
  Actions a;
  const int my_hq = M.my_hq;

  if (turn <= RUSH_TRAIN_TURNS)
    a.train_n = 1;

  if (turn == RUSH_LAUNCH_TURN) {
    if (g_stage_region < 0)
      g_stage_region = RUSH_STAGED ? pick_stage_region(M, P) : M.opp_hq;
    std::vector<const Warrior *> mine;
    for (const auto &w : S.warriors)
      if (w.id.side == M.my_side && w.hp > 0)
        mine.push_back(&w);
    std::sort(mine.begin(), mine.end(),
              [](const Warrior *x, const Warrior *y) {
                return x->id.num < y->id.num;
              });
    int keep = RUSH_KEEP_HOME;
    for (const Warrior *w : mine) {
      if (keep > 0 && w->region == my_hq) { --keep; continue; }
      a.moves.emplace_back(w->id, g_stage_region);
      g_party.push_back(w->id);
    }
    return a;
  }

  /* 재돌격: 파티가 (낙오 허용 내로) 모이면 본부로 */
  if (RUSH_STAGED && !g_redirected && turn > RUSH_LAUNCH_TURN) {
    int arrived = 0, alive = 0;
    for (const auto &id : g_party)
      for (const auto &w : S.warriors)
        if (w.id == id && w.hp > 0) {
          ++alive;
          if (w.region == g_stage_region && w.state == WState::STATIONARY)
            ++arrived;
        }
    if (alive > 0 && (arrived >= alive - RUSH_STRAGGLERS_OK ||
                      turn >= RUSH_REDIRECT_FAILSAFE_TURN)) {
      for (const auto &id : g_party)
        for (const auto &w : S.warriors)
          if (w.id == id && w.hp > 0 && w.state == WState::STATIONARY)
            a.moves.emplace_back(w.id, M.opp_hq);
      g_redirected = 1;
      return a;
    }
  }

  if (turn > RUSH_LAUNCH_TURN) {
    if (RUSH_REINFORCE && S.gold >= TRAIN_COST)
      a.train_n = 1;
#if RUSH_FORWARD_REINF
    /* 증원 스트림: 본부 노동 인원만 남기고 새 병력을 전선으로 */
    int home = 0;
    std::vector<const Warrior *> spare;
    for (const auto &w : S.warriors) {
      if (w.id.side != M.my_side || w.hp <= 0) continue;
      bool in_party = false;
      for (const auto &id : g_party)
        if (w.id == id) { in_party = true; break; }
      if (in_party) continue;
      if (w.state == WState::STATIONARY && w.region == my_hq) {
        ++home;
        if (home > RUSH_HOME_WORKERS) spare.push_back(&w);
      }
    }
    int tgt = g_redirected ? M.opp_hq : g_stage_region;
    if (tgt < 0) tgt = M.opp_hq;
    for (const Warrior *w : spare)
      a.moves.emplace_back(w->id, tgt);
#endif
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
