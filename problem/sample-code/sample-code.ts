import * as fs from "fs";

const MAX_TURN = 400;          // maximum turn (days)
const START_GOLD = 750;        // initial gold
const START_WARRIORS = 3;      // initial warriors
const MOVE_COST = 10;          // move cost
const TRAIN_COST = 120;        // train cost
const WORK_INCOME = 15;        // income per warrior
const UPKEEP_PER_WARRIOR = 2;  // upkeep per warrior
const HQ_MAX_LEVEL = 5;        // HQ max level
const BASE_MAX_LEVEL = 3;      // base max level
const HQ_HEAL_COST = 1000;     // HQ fix cost
const BASE_HEAL_COST = 500;    // base fix cost
const HOP_VISION = 2;          // vision radius shared by all units

interface HqLevelEntry {
  upgrade_cost: number;
  warrior_hp: number;
  hp: number;
  turret: number;
  train_cap: number;
  work_cap: number;
}

interface BaseLevelEntry {
  cost: number;
  hp: number;
  turret: number;
  work_cap: number;
}

const HQ_LEVELS: readonly HqLevelEntry[] = [
  { upgrade_cost: 0,    warrior_hp: 0, hp: 0,  turret: 0, train_cap: 0, work_cap: 0 },
  { upgrade_cost: 0,    warrior_hp: 4, hp: 10, turret: 1, train_cap: 1, work_cap: 1 },
  { upgrade_cost: 600,  warrior_hp: 5, hp: 15, turret: 2, train_cap: 1, work_cap: 2 },
  { upgrade_cost: 1000, warrior_hp: 6, hp: 20, turret: 2, train_cap: 2, work_cap: 3 },
  { upgrade_cost: 2000, warrior_hp: 7, hp: 25, turret: 3, train_cap: 2, work_cap: 4 },
  { upgrade_cost: 3000, warrior_hp: 8, hp: 30, turret: 3, train_cap: 3, work_cap: 5 },
];

const BASE_LEVELS: readonly BaseLevelEntry[] = [
  { cost: 0,    hp: 0,  turret: 0, work_cap: 0 },
  { cost: 500,  hp: 6,  turret: 1, work_cap: 1 },
  { cost: 550, hp: 12, turret: 1, work_cap: 2 },
  { cost: 600, hp: 18, turret: 2, work_cap: 3 },
];

type Side = "LEFT" | "RIGHT";
type BuildingType = "HQ" | "BASE";
type WarriorState = "STATIONARY" | "MOVING";

function opposite(s: Side): Side {
  return s === "LEFT" ? "RIGHT" : "LEFT";
}

function sideChar(s: Side): string {
  return s === "LEFT" ? "A" : "B";
}

function parseSideChar(c: string): Side {
  return c === "A" ? "LEFT" : "RIGHT";
}

interface WarriorId {
  side: Side;
  num: number;
}

function warriorIdEq(a: WarriorId, b: WarriorId): boolean {
  return a.side === b.side && a.num === b.num;
}

function parseWarrior(tok: string): WarriorId {
  return { side: parseSideChar(tok[0]), num: parseInt(tok.slice(1), 10) };
}

function formatWarrior(id: WarriorId): string {
  return sideChar(id.side) + id.num;
}

interface Warrior {
  id: WarriorId;
  region: number;
  hp: number;
  state: WarriorState;
  target: number;
}

interface Building {
  region: number;
  side: Side;
  type: BuildingType;
  level: number;
  hp: number;
}

function buildingCurrentHp(b: Building): number {
  return b.type === "HQ" ? HQ_LEVELS[b.level].hp : BASE_LEVELS[b.level].hp;
}

function buildingWorkCap(b: Building): number {
  return b.type === "HQ" ? HQ_LEVELS[b.level].work_cap : BASE_LEVELS[b.level].work_cap;
}

function applyUpgrade(b: Building): void {
  b.level += 1;
  b.hp = buildingCurrentHp(b);
}

function upgradeCost(b: Building): number {
  return b.type === "HQ"
    ? HQ_LEVELS[b.level + 1].upgrade_cost
    : BASE_LEVELS[b.level + 1].cost;
}

function maxLevel(b: Building): number {
  return b.type === "HQ" ? HQ_MAX_LEVEL : BASE_MAX_LEVEL;
}

function makeBase(region: number, s: Side): Building {
  return { region, side: s, type: "BASE", level: 1, hp: BASE_LEVELS[1].hp };
}

interface GameMap {
  N: number;
  K: number;
  x: number[];
  y: number[];
  strongholds: number[];
  adj: number[][];
  my_side: Side;
  my_hq: number;
  opp_hq: number;
}

interface GameState {
  gold: number;
  my_countdown: number;
  opp_countdown: number;
  warriors: Warrior[];
  buildings: Building[];
  visible: number[];
}

interface Actions {
  trainCount: number;
  moves: [WarriorId, number][];
  upgrades: number[];
}

function findBuilding(S: GameState, region: number): Building | undefined {
  return S.buildings.find(b => b.region === region);
}

function findWarrior(S: GameState, id: WarriorId): Warrior | undefined {
  return S.warriors.find(w => warriorIdEq(w.id, id));
}

function hqOf(M: GameMap, s: Side): number {
  return s === "LEFT" ? 0 : M.N - 1;
}

function computeVisible(S: GameState, M: GameMap): number[] {
  const visible = new Set<number>();
  const addHops = (start: number, radius: number): void => {
    const seen = new Set<number>([start]);
    let frontier = [start];
    for (let hop = 0; hop < radius; hop++) {
      const next: number[] = [];
      for (const region of frontier) {
        for (const neighbor of M.adj[region]) {
          if (!seen.has(neighbor)) {
            seen.add(neighbor);
            next.push(neighbor);
          }
        }
      }
      frontier = next;
    }
    for (const region of seen) visible.add(region);
  };
  for (const w of S.warriors)
    if (w.id.side === M.my_side) addHops(w.region, HOP_VISION);
  for (const b of S.buildings)
    if (b.side === M.my_side) addHops(b.region, HOP_VISION);
  return [...visible].sort((a, b) => a - b);
}

const readln = (() => {
  const buf = Buffer.alloc(4096);
  let pending = "";
  return (): string => {
    while (true) {
      const nl = pending.indexOf("\n");
      if (nl !== -1) {
        const line = pending.slice(0, nl);
        pending = pending.slice(nl + 1);
        return line;
      }
      let n: number;
      try {
        n = fs.readSync(0, buf, 0, buf.length, null);
      } catch {
        process.exit(0);
      }
      if (n === 0) process.exit(0);
      pending += buf.slice(0, n).toString("utf8");
    }
  };
})();

function readTokens(): string[] {
  return readln().split(/\s+/).filter(s => s.length > 0);
}

function parseInit(): [GameMap, GameState] {
  const M: GameMap = {
    N: 0, K: 0,
    x: [], y: [],
    strongholds: [],
    adj: [],
    my_side: "LEFT",
    my_hq: 0,
    opp_hq: 0,
  };

  const tReady = readTokens();
  M.my_side = tReady[1] === "LEFT" ? "LEFT" : "RIGHT";

  const tSize = readTokens();
  M.N = parseInt(tSize[0], 10);
  M.K = parseInt(tSize[1], 10);

  const tX = readTokens(); // x_0 x_1 ... x_{N-1}
  M.x = tX.map(v => parseInt(v, 10));

  const tY = readTokens(); // y_0 y_1 ... y_{N-1}
  M.y = tY.map(v => parseInt(v, 10));

  const tSH = readTokens(); // K strongholds
  M.strongholds = tSH.map(v => parseInt(v, 10)).sort((a, b) => a - b);

  M.adj = [];
  for (let r = 0; r < M.N; r++) {
    const tAdj = readTokens(); // deg n_1 n_2 ...
    const deg = parseInt(tAdj[0], 10);
    const nb: number[] = [];
    for (let j = 0; j < deg; j++) nb.push(parseInt(tAdj[1 + j], 10));
    nb.sort((a, b) => a - b);
    M.adj.push(nb);
  }

  M.my_hq = hqOf(M, M.my_side);
  M.opp_hq = hqOf(M, opposite(M.my_side));

  const S: GameState = {
    gold: START_GOLD,
    my_countdown: 5,
    opp_countdown: 5,
    warriors: [],
    buildings: [],
    visible: [],
  };

  const opp = opposite(M.my_side);
  for (let sfx = 1; sfx <= START_WARRIORS; sfx++) {
    S.warriors.push({ id: { side: M.my_side, num: sfx }, region: M.my_hq, hp: HQ_LEVELS[1].warrior_hp, state: "STATIONARY", target: 0 });
    S.warriors.push({ id: { side: opp, num: sfx }, region: M.opp_hq, hp: HQ_LEVELS[1].warrior_hp, state: "STATIONARY", target: 0 });
  }
  S.buildings.push({ region: 0,       side: "LEFT",  type: "HQ", level: 1, hp: HQ_LEVELS[1].hp });
  S.buildings.push({ region: M.N - 1, side: "RIGHT", type: "HQ", level: 1, hp: HQ_LEVELS[1].hp });

  process.stdout.write("OK\n");
  return [M, S];
}

function readTurnStart(): number | null {
  const line = readln();
  if (line === "FINISH") return null;
  const t = line.split(/\s+/);
  return parseInt(t[2], 10);
}

function readTurnResult(S: GameState, M: GameMap, submitted: Actions): void {
  for (const region of submitted.upgrades) {
    const b = findBuilding(S, region);
    if (b === undefined) {
      S.gold -= BASE_LEVELS[1].cost;
    } else {
      if (b.level >= maxLevel(b)) {
        const cost = b.type === "HQ" ? HQ_HEAL_COST : BASE_HEAL_COST;
        S.gold -= cost;
        b.hp = buildingCurrentHp(b);
      } else {
        S.gold -= upgradeCost(b);
        applyUpgrade(b);
      }
    }
  }

  const built = new Set(submitted.upgrades);

  for (const [id, target] of submitted.moves) {
    const b = findBuilding(S, target);
    const own = built.has(target) || (b !== undefined && b.side === M.my_side);
    const cost = own ? 0 : MOVE_COST;
    S.gold -= cost;
    const w = findWarrior(S, id);
    if (w !== undefined) {
      w.state = "MOVING";
      w.target = target;
    }
  }

  S.gold -= TRAIN_COST * submitted.trainCount;

  const lineAfterActions = readln();
  if (lineAfterActions === "FINISH") process.exit(0);

  const tCountdown = readTokens();
  S.my_countdown = parseInt(tCountdown[2], 10);
  S.opp_countdown = parseInt(tCountdown[4], 10);

  // UPGRADE
  const tUpgrade = readTokens(); // "UPGRADE N"
  const upgradeN = parseInt(tUpgrade[1], 10);
  for (let i = 0; i < upgradeN; i++) {
    const r = readTokens(); // "<A|B> <region>"
    const region = parseInt(r[1], 10);
    const b = findBuilding(S, region);
    if (b === undefined) {
      S.buildings.push(makeBase(region, M.my_side));
    } else if (b.level >= maxLevel(b)) {
      b.hp = buildingCurrentHp(b);
    } else {
      applyUpgrade(b);
    }
  }

  // TRAIN
  const tTrain = readTokens(); // "TRAIN N"
  const trainN = parseInt(tTrain[1], 10);
  if (trainN > 0) {
    const ids = readTokens();
    for (let i = 0; i < trainN; i++) {
      const wid = parseWarrior(ids[i]);
      const hqBuilding = findBuilding(S, M.my_hq);
      const hqLevel = hqBuilding !== undefined ? hqBuilding.level : 1;
      S.warriors.push({ id: wid, region: M.my_hq, hp: HQ_LEVELS[hqLevel].warrior_hp, state: "STATIONARY", target: 0 });
    }
  }

  // MOVE
  const tMove = readTokens(); // "MOVE N"
  const moveN = parseInt(tMove[1], 10);
  for (let i = 0; i < moveN; i++) {
    const r = readTokens();
    const wid = parseWarrior(r[0]);
    const region = parseInt(r[1], 10);
    const w = findWarrior(S, wid);
    if (w !== undefined) {
      w.region = region;
      if (w.state === "MOVING" && w.region === w.target) {
        w.state = "STATIONARY";
      }
    }
  }

  // DAMAGE
  const starved: [WarriorId, number][] = [];
  const tDamage = readTokens(); // "DAMAGE N"
  const damageN = parseInt(tDamage[1], 10);
  for (let i = 0; i < damageN; i++) {
    const r = readTokens(); // "<cause> <id> <damage>"
    const wid = parseWarrior(r[1]);
    const damage = parseInt(r[2], 10);
    if (r[0] === "HUNGER") {
      starved.push([wid, damage]);
      continue;
    }
    const w = findWarrior(S, wid);
    if (w !== undefined) w.hp -= damage;
  }
  S.warriors = S.warriors.filter(w => w.hp > 0);

  // SIEGE
  const tSiege = readTokens(); // "SIEGE N"
  const siegeN = parseInt(tSiege[1], 10);
  for (let i = 0; i < siegeN; i++) {
    const r = readTokens();
    const region = parseInt(r[1], 10);
    const damage = parseInt(r[2], 10);
    const b = findBuilding(S, region);
    if (b !== undefined) b.hp -= damage;
  }
  S.buildings = S.buildings.filter(b => b.hp > 0);

  S.visible = computeVisible(S, M);

  // WARRIOR
  const tWarrior = readTokens(); // "WARRIOR W"
  const warriorW = parseInt(tWarrior[1], 10);
  const seenWarriors: Warrior[] = [];
  for (let i = 0; i < warriorW; i++) {
    const r = readTokens(); // "<id> <region> <hp>"
    const wid = parseWarrior(r[0]);
    if (wid.side === M.my_side) continue;
    seenWarriors.push({ id: wid, region: parseInt(r[1], 10), hp: parseInt(r[2], 10), state: "STATIONARY", target: 0 });
  }
  S.warriors = S.warriors.filter(w => w.id.side === M.my_side).concat(seenWarriors);

  // BUILDING
  const tBuilding = readTokens(); // "BUILDING B"
  const buildingB = parseInt(tBuilding[1], 10);
  const seenBuildings: Building[] = [];
  for (let i = 0; i < buildingB; i++) {
    const r = readTokens(); // "<side> <region> <kind> <level> <hp>"
    const side = parseSideChar(r[0][0]);
    if (side === M.my_side) continue;
    const btype: BuildingType = r[2] === "HQ" ? "HQ" : "BASE";
    seenBuildings.push({ region: parseInt(r[1], 10), side, type: btype, level: parseInt(r[3], 10), hp: parseInt(r[4], 10) });
  }
  S.buildings = S.buildings.filter(b => b.side === M.my_side).concat(seenBuildings);

  readln(); // "END"

  let income = 0;
  for (const b of S.buildings) {
    if (b.side !== M.my_side) continue;
    let count = 0;
    for (const w of S.warriors) {
      if (w.id.side === M.my_side && w.region === b.region) count++;
    }
    income += WORK_INCOME * Math.min(count, buildingWorkCap(b));
  }
  S.gold += income;

  let alive = 0;
  for (const w of S.warriors) if (w.id.side === M.my_side) alive++;
  S.gold -= UPKEEP_PER_WARRIOR * Math.min(alive, Math.floor(S.gold / UPKEEP_PER_WARRIOR));

  for (const [wid, damage] of starved) {
    const w = findWarrior(S, wid);
    if (w !== undefined) w.hp -= damage;
  }
  S.warriors = S.warriors.filter(w => w.hp > 0);
}

interface Paths {
  dist: number[][];
  nxt: number[][];
}

function euclidCeil(M: GameMap, u: number, v: number): number {
  const dx = M.x[u] - M.x[v];
  const dy = M.y[u] - M.y[v];
  return Math.ceil(Math.sqrt(dx * dx + dy * dy));
}

function calculatePaths(M: GameMap): Paths {
  const N = M.N;
  const dist: number[][] = Array.from({ length: N }, () => new Array(N).fill(Infinity));
  const nxt: number[][] = Array.from({ length: N }, () => new Array(N).fill(-1));

  for (let i = 0; i < N; i++) {
    dist[i][i] = 0;
    nxt[i][i] = i;
  }
  for (let u = 0; u < N; u++) {
    for (const v of M.adj[u]) {
      const w = euclidCeil(M, u, v);
      if (w < dist[u][v]) dist[u][v] = w;
    }
  }

  for (let k = 0; k < N; k++) {
    for (let u = 0; u < N; u++) {
      if (dist[u][k] === Infinity) continue;
      for (let v = 0; v < N; v++) {
        const cand = dist[u][k] + dist[k][v];
        if (cand < dist[u][v]) dist[u][v] = cand;
      }
    }
  }

  for (let u = 0; u < N; u++) {
    for (let v = 0; v < N; v++) {
      if (u === v || dist[u][v] === Infinity) continue;
      let bestScore = Infinity;
      for (const nb of M.adj[u]) {
        if (dist[nb][v] === Infinity) continue;
        const score = euclidCeil(M, u, nb) + dist[nb][v];
        if (score < bestScore) {
          bestScore = score;
          nxt[u][v] = nb;
        }
      }
    }
  }

  return { dist, nxt };
}

function nextStep(P: Paths, u: number, v: number): number {
  return P.nxt[u][v];
}

function path(P: Paths, u: number, v: number): number[] {
  if (P.nxt[u][v] === -1) return [];
  const out: number[] = [u];
  while (u !== v) {
    u = P.nxt[u][v];
    out.push(u);
  }
  return out;
}

function emit(a: Actions): void {
  let out = "COMMAND\n";
  for (const [id, target] of a.moves) {
    out += `MOVE ${formatWarrior(id)} ${target}\n`;
  }
  for (const r of a.upgrades) {
    out += `UPGRADE ${r}\n`;
  }
  if (a.trainCount > 0) {
    out += `TRAIN ${a.trainCount}\n`;
  }
  out += "END\n";
  process.stdout.write(out);
}

//////////////////////////////////
//// WRITE YOUR STRATEGY HERE ////
//////////////////////////////////
function decide(S: GameState, M: GameMap, P: Paths, turn: number): Actions {
  const a: Actions = { trainCount: 0, moves: [], upgrades: [] };
  if (turn === 1) {
    for (const w of S.warriors) {
      if (w.id.side !== M.my_side) continue;
      a.moves.push([w.id, M.opp_hq]);
    }
  }
  return a;
}

function main(): void {
  const [M, S] = parseInit();
  const P = calculatePaths(M);

  let turn: number | null;
  while ((turn = readTurnStart()) !== null) {
    const a = decide(S, M, P, turn);
    emit(a);
    readTurnResult(S, M, a);
  }
}

main();
