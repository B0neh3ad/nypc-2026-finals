'use strict';
const fs = require('fs');

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

const HQ_LEVELS = [
  { upgrade_cost: 0,    warrior_hp: 0, hp: 0,  turret: 0, train_cap: 0, work_cap: 0 },
  { upgrade_cost: 0,    warrior_hp: 4, hp: 10, turret: 1, train_cap: 1, work_cap: 1 },
  { upgrade_cost: 600,  warrior_hp: 5, hp: 15, turret: 2, train_cap: 1, work_cap: 2 },
  { upgrade_cost: 1000, warrior_hp: 6, hp: 20, turret: 2, train_cap: 2, work_cap: 3 },
  { upgrade_cost: 2000, warrior_hp: 7, hp: 25, turret: 3, train_cap: 2, work_cap: 4 },
  { upgrade_cost: 3000, warrior_hp: 8, hp: 30, turret: 3, train_cap: 3, work_cap: 5 },
];
const BASE_LEVELS = [
  { cost: 0,    hp: 0,  turret: 0, work_cap: 0 },
  { cost: 500,  hp: 6,  turret: 1, work_cap: 1 },
  { cost: 550, hp: 12, turret: 1, work_cap: 2 },
  { cost: 600, hp: 18, turret: 2, work_cap: 3 },
];

const Side = { LEFT: 'LEFT', RIGHT: 'RIGHT' };
const BType = { HQ: 'HQ', BASE: 'BASE' };
const WState = { STATIONARY: 0, MOVING: 1 };

const oppositeSide = s => (s === Side.LEFT ? Side.RIGHT : Side.LEFT);
const sideChar = s => (s === Side.LEFT ? 'A' : 'B');
const parseSideChar = c => (c === 'A' ? Side.LEFT : Side.RIGHT);

const parseWarrior = tok => ({ side: parseSideChar(tok[0]), num: parseInt(tok.slice(1), 10) });
const formatWarrior = id => `${sideChar(id.side)}${id.num}`;
const warriorIdEq = (a, b) => a.side === b.side && a.num === b.num;

const hqOf = (M, s) => (s === Side.LEFT ? 0 : M.N - 1);

const makeBase = (region, s) => ({
  region, side: s, type: BType.BASE, level: 1, hp: BASE_LEVELS[1].hp,
});

const buildingCurrentHp = b =>
  b.type === BType.HQ ? HQ_LEVELS[b.level].hp : BASE_LEVELS[b.level].hp;

const buildingWorkCap = b =>
  b.type === BType.HQ ? HQ_LEVELS[b.level].work_cap : BASE_LEVELS[b.level].work_cap;

const buildingMaxLevel = b => (b.type === BType.HQ ? HQ_MAX_LEVEL : BASE_MAX_LEVEL);

const upgradeCost = b =>
  b.type === BType.HQ
    ? HQ_LEVELS[b.level + 1].upgrade_cost
    : BASE_LEVELS[b.level + 1].cost;

const applyUpgrade = b => {
  b.level += 1;
  b.hp = buildingCurrentHp(b);
};

const readBuf = Buffer.alloc(1);
const readln = () => {
  let line = '';
  for (;;) {
    const n = fs.readSync(0, readBuf, 0, 1, null);
    if (n === 0) process.exit(0);
    const ch = readBuf.toString('utf8');
    if (ch === '\n') return line;
    line += ch;
  }
};

const readTokens = () => readln().split(/\s+/).filter(s => s.length > 0);

const findBuilding = (S, region) => S.buildings.find(b => b.region === region) ?? null;
const findWarrior = (S, id) => S.warriors.find(w => warriorIdEq(w.id, id)) ?? null;

function computeVisible(S, M) {
  const visible = new Set();
  const addHops = (start, radius) => {
    const seen = new Set([start]);
    let frontier = [start];
    for (let hop = 0; hop < radius; hop++) {
      const next = [];
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
    if (w.id.side === M.mySide) addHops(w.region, HOP_VISION);
  for (const b of S.buildings)
    if (b.side === M.mySide) addHops(b.region, HOP_VISION);
  return [...visible].sort((a, b) => a - b);
}

function parseInit() {
  const M = { N: 0, K: 0, x: [], y: [], strongholds: [], adj: [], mySide: Side.LEFT, myHq: 0, oppHq: 0 };

  const ready = readTokens();
  M.mySide = ready[1] === 'LEFT' ? Side.LEFT : Side.RIGHT;

  const dims = readTokens();
  M.N = parseInt(dims[0], 10);
  M.K = parseInt(dims[1], 10);

  M.x = readTokens().map(Number); // x_0 x_1 ... x_{N-1}
  M.y = readTokens().map(Number); // y_0 y_1 ... y_{N-1}

  M.strongholds = readTokens().map(Number).sort((a, b) => a - b); // K strongholds

  M.adj = [];
  for (let r = 0; r < M.N; r++) {
    const t = readTokens(); // deg n_1 n_2 ...
    const deg = parseInt(t[0], 10);
    const nb = [];
    for (let j = 0; j < deg; j++) nb.push(parseInt(t[1 + j], 10));
    nb.sort((a, b) => a - b);
    M.adj.push(nb);
  }

  M.myHq = hqOf(M, M.mySide);
  M.oppHq = hqOf(M, oppositeSide(M.mySide));

  const S = {
    gold: START_GOLD,
    myCountdown: 5,
    oppCountdown: 5,
    warriors: [],
    buildings: [],
    visible: [],
  };

  const opp = oppositeSide(M.mySide);
  for (let sfx = 1; sfx <= START_WARRIORS; sfx++) {
    S.warriors.push({ id: { side: M.mySide, num: sfx }, region: M.myHq, hp: HQ_LEVELS[1].warrior_hp, state: WState.STATIONARY, target: 0 });
    S.warriors.push({ id: { side: opp, num: sfx }, region: M.oppHq, hp: HQ_LEVELS[1].warrior_hp, state: WState.STATIONARY, target: 0 });
  }
  S.buildings.push({ region: 0, side: Side.LEFT, type: BType.HQ, level: 1, hp: HQ_LEVELS[1].hp });
  S.buildings.push({ region: M.N - 1, side: Side.RIGHT, type: BType.HQ, level: 1, hp: HQ_LEVELS[1].hp });

  process.stdout.write('OK\n');
  return { M, S };
}

function readTurnStart() {
  const line = readln();
  if (line === 'FINISH') return null;
  const t = line.split(/\s+/);
  return parseInt(t[2], 10);
}

function readTurnResult(S, M, submitted) {
  for (const region of submitted.upgrades) {
    const b = findBuilding(S, region);
    if (b === null) {
      S.gold -= BASE_LEVELS[1].cost;
    } else {
      if (b.level >= buildingMaxLevel(b)) {
        const cost = b.type === BType.HQ ? HQ_HEAL_COST : BASE_HEAL_COST;
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
    const own = built.has(target) || (b !== null && b.side === M.mySide);
    const cost = own ? 0 : MOVE_COST;
    S.gold -= cost;
    const w = findWarrior(S, id);
    if (w !== null) {
      w.state = WState.MOVING;
      w.target = target;
    }
  }

  S.gold -= TRAIN_COST * submitted.trainN;

  const turnHeader = readln();
  if (turnHeader === 'FINISH') process.exit(0);
  const countdown = readTokens();
  S.myCountdown = parseInt(countdown[2], 10);
  S.oppCountdown = parseInt(countdown[4], 10);

  // UPGRADE
  const upgradeLine = readTokens(); // "UPGRADE N"
  const upgradeN = parseInt(upgradeLine[1], 10);
  for (let i = 0; i < upgradeN; i++) {
    const r = readTokens(); // "<A|B> <region>"
    const region = parseInt(r[1], 10);
    const b = findBuilding(S, region);
    if (b === null) {
      S.buildings.push(makeBase(region, M.mySide));
    } else {
      if (b.level >= buildingMaxLevel(b)) {
        b.hp = buildingCurrentHp(b);
      } else {
        applyUpgrade(b);
      }
    }
  }

  // TRAIN
  const trainLine = readTokens(); // "TRAIN N"
  const trainN = parseInt(trainLine[1], 10);
  if (trainN > 0) {
    const ids = readTokens();
    for (let i = 0; i < trainN; i++) {
      const wid = parseWarrior(ids[i]);
      const hqB = findBuilding(S, M.myHq);
      const hqLevel = hqB !== null ? hqB.level : 1;
      S.warriors.push({ id: wid, region: M.myHq, hp: HQ_LEVELS[hqLevel].warrior_hp, state: WState.STATIONARY, target: 0 });
    }
  }

  // MOVE
  const moveLine = readTokens(); // "MOVE N"
  const moveN = parseInt(moveLine[1], 10);
  for (let i = 0; i < moveN; i++) {
    const r = readTokens();
    const wid = parseWarrior(r[0]);
    const region = parseInt(r[1], 10);
    const w = findWarrior(S, wid);
    if (w !== null) {
      w.region = region;
      if (w.state === WState.MOVING && w.region === w.target) {
        w.state = WState.STATIONARY;
      }
    }
  }

  // DAMAGE
  const starved = [];
  const damageLine = readTokens(); // "DAMAGE N"
  const damageN = parseInt(damageLine[1], 10);
  for (let i = 0; i < damageN; i++) {
    const r = readTokens(); // "<cause> <id> <damage>"
    const wid = parseWarrior(r[1]);
    const damage = parseInt(r[2], 10);
    if (r[0] === 'HUNGER') {
      starved.push([wid, damage]);
      continue;
    }
    const w = findWarrior(S, wid);
    if (w !== null) w.hp -= damage;
  }
  S.warriors = S.warriors.filter(w => w.hp > 0);

  // SIEGE
  const siegeLine = readTokens(); // "SIEGE N"
  const siegeN = parseInt(siegeLine[1], 10);
  for (let i = 0; i < siegeN; i++) {
    const r = readTokens();
    const region = parseInt(r[1], 10);
    const damage = parseInt(r[2], 10);
    const b = findBuilding(S, region);
    if (b !== null) b.hp -= damage;
  }
  S.buildings = S.buildings.filter(b => b.hp > 0);

  S.visible = computeVisible(S, M);

  // WARRIOR
  const warriorLine = readTokens(); // "WARRIOR W"
  const warriorW = parseInt(warriorLine[1], 10);
  const seenWarriors = [];
  for (let i = 0; i < warriorW; i++) {
    const r = readTokens(); // "<id> <region> <hp>"
    const wid = parseWarrior(r[0]);
    if (wid.side === M.mySide) continue;
    seenWarriors.push({ id: wid, region: parseInt(r[1], 10), hp: parseInt(r[2], 10), state: WState.STATIONARY, target: 0 });
  }
  S.warriors = S.warriors.filter(w => w.id.side === M.mySide).concat(seenWarriors);

  // BUILDING
  const buildingLine = readTokens(); // "BUILDING B"
  const buildingB = parseInt(buildingLine[1], 10);
  const seenBuildings = [];
  for (let i = 0; i < buildingB; i++) {
    const r = readTokens(); // "<side> <region> <kind> <level> <hp>"
    const s = parseSideChar(r[0][0]);
    if (s === M.mySide) continue;
    const btype = r[2] === 'HQ' ? BType.HQ : BType.BASE;
    seenBuildings.push({ region: parseInt(r[1], 10), side: s, type: btype, level: parseInt(r[3], 10), hp: parseInt(r[4], 10) });
  }
  S.buildings = S.buildings.filter(b => b.side === M.mySide).concat(seenBuildings);

  readln(); // "END"

  let income = 0;
  for (const b of S.buildings) {
    if (b.side !== M.mySide) continue;
    let count = 0;
    for (const w of S.warriors) {
      if (w.id.side === M.mySide && w.region === b.region) count++;
    }
    income += WORK_INCOME * Math.min(count, buildingWorkCap(b));
  }
  S.gold += income;

  const alive = S.warriors.filter(w => w.id.side === M.mySide).length;
  S.gold -= UPKEEP_PER_WARRIOR * Math.min(alive, Math.floor(S.gold / UPKEEP_PER_WARRIOR));

  for (const [wid, damage] of starved) {
    const w = findWarrior(S, wid);
    if (w !== null) w.hp -= damage;
  }
  S.warriors = S.warriors.filter(w => w.hp > 0);
}

const euclidCeil = (M, u, v) => {
  const dx = M.x[u] - M.x[v];
  const dy = M.y[u] - M.y[v];
  return Math.ceil(Math.sqrt(dx * dx + dy * dy));
};

function calculatePaths(M) {
  const N = M.N;
  const INF = Infinity;
  const dist = Array.from({ length: N }, () => new Float64Array(N).fill(INF));
  const nxt = Array.from({ length: N }, () => new Int32Array(N).fill(-1));

  for (let i = 0; i < N; i++) {
    dist[i][i] = 0.0;
    nxt[i][i] = i;
  }
  for (let u = 0; u < N; u++) {
    for (const v of M.adj[u]) {
      const w = euclidCeil(M, u, v);
      if (w < dist[u][v]) dist[u][v] = w;
    }
  }

  for (let k = 0; k < N; k++) {
    const dk = dist[k];
    for (let u = 0; u < N; u++) {
      const du = dist[u];
      const duk = du[k];
      if (duk === INF) continue;
      for (let v = 0; v < N; v++) {
        const cand = duk + dk[v];
        if (cand < du[v]) du[v] = cand;
      }
    }
  }

  for (let u = 0; u < N; u++) {
    const du = dist[u];
    for (let v = 0; v < N; v++) {
      if (u === v || du[v] === INF) continue;
      let bestScore = INF;
      for (const nb of M.adj[u]) {
        if (dist[nb][v] === INF) continue;
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

const nextStep = (P, u, v) => P.nxt[u][v];

const getPath = (P, u, v) => {
  if (P.nxt[u][v] === -1) return [];
  const out = [u];
  while (u !== v) {
    u = P.nxt[u][v];
    out.push(u);
  }
  return out;
};

function emitActions(a) {
  const lines = ['COMMAND'];
  for (const [id, target] of a.moves) {
    lines.push(`MOVE ${formatWarrior(id)} ${target}`);
  }
  for (const r of a.upgrades) lines.push(`UPGRADE ${r}`);
  if (a.trainN > 0) lines.push(`TRAIN ${a.trainN}`);
  lines.push('END');
  process.stdout.write(lines.join('\n') + '\n');
}

//////////////////////////////////
//// WRITE YOUR STRATEGY HERE ////
//////////////////////////////////
function decide(S, M, P, turn) {
  const a = { trainN: 0, moves: [], upgrades: [] };
  if (turn === 1) {
    for (const w of S.warriors) {
      if (w.id.side !== M.mySide) continue;
      a.moves.push([w.id, M.oppHq]);
    }
  }
  return a;
}

function main() {
  const { M, S } = parseInit();
  const P = calculatePaths(M);

  let turn;
  while ((turn = readTurnStart()) !== null) {
    const a = decide(S, M, P, turn);
    emitActions(a);
    readTurnResult(S, M, a);
  }
}

main();
