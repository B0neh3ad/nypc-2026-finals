"use strict";

const RULES = {
  initialGold: 500,
  initialWarriors: 3,
  moveCost: 10,
  trainCost: 120,
  upkeepCost: 2,
  incomePerWorker: 15,
  maxHqLevel: 5,
  maxBaseLevel: 3,
  hqRepairCost: 1000,
  baseRepairCost: 500,
  hqHp: [10, 10, 15, 20, 25, 30],
  warriorHp: [4, 4, 5, 6, 7, 8],
  hqUpgradeCost: [0, 0, 600, 1200, 2400, 3600],
  baseHp: [6, 6, 12, 18],
  baseUpgradeCost: [0, 300, 600, 1000],
};

const TEAM = {
  A: { label: "A", side: "LEFT", color: "#d95345", soft: "rgba(217,83,69,.2)" },
  B: { label: "B", side: "RIGHT", color: "#3b82f6", soft: "rgba(59,130,246,.2)" },
};

const SCENE_LABELS = {
  command: "COMMAND",
  build: "BUILD",
  move: "MOVE",
  train: "TRAIN",
  battle: "BATTLE",
  upkeep: "END",
  idle: "IDLE",
};

const els = {};
const app = {
  manifest: [],
  manifestFiltered: [],
  rawText: "",
  parsed: null,
  replay: null,
  geometry: null,
  step: 0,
  playing: false,
  speed: 1,
  lastTick: 0,
  hoveredCell: null,
  focusedCell: null,
  options: {
    showEdges: true,
    showCellNumbers: false,
    showAllMoves: false,
  },
  baseViewBox: null,
  viewBox: null,
  drag: null,
  panels: {
    infoPinned: true,
    analysisPinned: false,
  },
  suppressClick: false,
};

document.addEventListener("DOMContentLoaded", init);

function init() {
  bindElements();
  bindEvents();
  initPanels();
  loadManifest();
  requestAnimationFrame(animationLoop);
}

function bindElements() {
  const ids = [
    "loadedName",
    "logSearch",
    "logSelect",
    "loadLogBtn",
    "fileInput",
    "resultPill",
    "analysisPanel",
    "infoPanel",
    "analysisPin",
    "infoPin",
    "toggleAnalysis",
    "toggleInfo",
    "optEdges",
    "optCellNumbers",
    "optAllMoves",
    "statusGrid",
    "chartHp",
    "chartGold",
    "chartWarriors",
    "mapSvg",
    "turnLabel",
    "sceneLabel",
    "teamAGold",
    "teamBGold",
    "teamAHq",
    "teamBHq",
    "fitMapBtn",
    "fullscreenBtn",
    "parseTextBtn",
    "copyLogBtn",
    "downloadLogBtn",
    "logText",
    "parseError",
    "turnDetails",
    "cellDetails",
    "jumpStartBtn",
    "stepBackBtn",
    "playBtn",
    "stepForwardBtn",
    "jumpEndBtn",
    "timeline",
    "timelineText",
    "dropOverlay",
  ];
  for (const id of ids) els[id] = document.getElementById(id);
}

function bindEvents() {
  els.logSearch.addEventListener("input", () => renderManifestOptions(els.logSearch.value));
  els.loadLogBtn.addEventListener("click", () => loadSelectedLog());
  els.logSelect.addEventListener("change", () => loadSelectedLog());
  els.fileInput.addEventListener("change", handleFileInput);
  els.parseTextBtn.addEventListener("click", () => setLogText(els.logText.value, "Pasted log", ""));
  els.copyLogBtn.addEventListener("click", copyCurrentLog);
  els.downloadLogBtn.addEventListener("click", downloadCurrentLog);

  els.optEdges.addEventListener("change", () => {
    app.options.showEdges = els.optEdges.checked;
    renderAll();
  });
  els.optCellNumbers.addEventListener("change", () => {
    app.options.showCellNumbers = els.optCellNumbers.checked;
    renderAll();
  });
  els.optAllMoves.addEventListener("change", () => {
    app.options.showAllMoves = els.optAllMoves.checked;
    renderAll();
  });

  els.fitMapBtn.addEventListener("click", fitMap);
  els.fullscreenBtn.addEventListener("click", toggleFullscreen);

  els.jumpStartBtn.addEventListener("click", jumpStart);
  els.jumpEndBtn.addEventListener("click", jumpEnd);
  els.stepBackBtn.addEventListener("click", stepBack);
  els.stepForwardBtn.addEventListener("click", stepForward);
  els.playBtn.addEventListener("click", togglePlay);
  els.timeline.addEventListener("input", () => {
    app.step = Number(els.timeline.value);
    renderAll();
  });

  document.querySelectorAll(".speed-btn").forEach((btn) => {
    btn.addEventListener("click", () => setSpeed(Number(btn.dataset.speed)));
  });

  els.mapSvg.addEventListener("mousemove", onMapMove);
  els.mapSvg.addEventListener("mouseleave", () => {
    app.hoveredCell = null;
    renderAll();
  });
  els.mapSvg.addEventListener("click", onMapClick);
  els.mapSvg.addEventListener("wheel", onMapWheel, { passive: false });
  els.mapSvg.addEventListener("pointerdown", onMapPointerDown);
  window.addEventListener("pointermove", onMapPointerMove);
  window.addEventListener("pointerup", onMapPointerUp);

  window.addEventListener("keydown", onKeyDown);
  window.addEventListener("dragover", onDragOver);
  window.addEventListener("dragleave", onDragLeave);
  window.addEventListener("drop", onDrop);

  document.querySelectorAll(".section-toggle").forEach((btn) => {
    btn.addEventListener("click", () => btn.closest(".collapsible").classList.toggle("open"));
  });
}

function initPanels() {
  els.infoPanel.classList.toggle("pinned", app.panels.infoPinned);
  els.analysisPanel.classList.toggle("pinned", app.panels.analysisPinned);
  const toggleInfo = () => {
    app.panels.infoPinned = !app.panels.infoPinned;
    els.infoPanel.classList.toggle("pinned", app.panels.infoPinned);
  };
  const toggleAnalysis = () => {
    app.panels.analysisPinned = !app.panels.analysisPinned;
    els.analysisPanel.classList.toggle("pinned", app.panels.analysisPinned);
  };
  els.toggleInfo.addEventListener("click", toggleInfo);
  els.infoPin.addEventListener("click", toggleInfo);
  els.toggleAnalysis.addEventListener("click", toggleAnalysis);
  els.analysisPin.addEventListener("click", toggleAnalysis);
}

async function loadManifest() {
  try {
    const res = await fetch("./logs-manifest.json", { cache: "no-store" });
    if (!res.ok) throw new Error(`manifest ${res.status}`);
    app.manifest = await res.json();
  } catch (err) {
    app.manifest = [];
    console.warn("Could not load log manifest", err);
  }
  renderManifestOptions("");
  const params = new URLSearchParams(window.location.search);
  const data = params.get("data");
  const log = params.get("log");
  if (data) {
    loadUrl(data, "URL log", data);
  } else if (log) {
    loadLogPath(log);
  } else {
    const preferred = app.manifest.find((item) => item.path === "logs/round_9/battle_85675.log") || app.manifest[0];
    if (preferred) loadLogPath(preferred.path);
  }
}

function renderManifestOptions(query) {
  const q = query.trim().toLowerCase();
  app.manifestFiltered = app.manifest.filter((item) => {
    const haystack = `${item.round} ${item.name} ${item.path} ${item.result || ""}`.toLowerCase();
    return !q || haystack.includes(q);
  });
  els.logSelect.innerHTML = app.manifestFiltered
    .map((item) => {
      const label = `${item.round} / ${item.name}${item.result ? ` / ${item.result}` : ""}`;
      return `<option value="${escapeHtml(item.path)}">${escapeHtml(label)}</option>`;
    })
    .join("");
}

function loadSelectedLog() {
  const path = els.logSelect.value;
  if (path) loadLogPath(path);
}

function loadLogPath(path) {
  const entry = app.manifest.find((item) => item.path === path);
  const label = entry ? `${entry.round} / ${entry.name}` : path;
  const url = path.startsWith("http://") || path.startsWith("https://") ? path : `../${path}`;
  loadUrl(url, label, path);
}

async function loadUrl(url, label, path) {
  try {
    setParseError("");
    const res = await fetch(url, { cache: "no-store" });
    if (!res.ok) throw new Error(`HTTP ${res.status}`);
    const text = await res.text();
    setLogText(text, label, path);
  } catch (err) {
    setParseError(`Failed to load ${label}: ${err.message}`);
  }
}

function handleFileInput(event) {
  const file = event.target.files && event.target.files[0];
  if (!file) return;
  file.text().then((text) => setLogText(text, file.name, file.name)).catch((err) => setParseError(err.message));
  event.target.value = "";
}

function setLogText(text, label, path) {
  try {
    const parsed = parseLog(text);
    const replay = buildReplay(parsed);
    app.rawText = text;
    app.parsed = parsed;
    app.replay = replay;
    app.geometry = buildGeometry(parsed.map);
    app.step = 0;
    app.focusedCell = null;
    app.hoveredCell = null;
    app.baseViewBox = expandBox(app.geometry.box, 0.06);
    app.viewBox = { ...app.baseViewBox };
    els.loadedName.textContent = path || label;
    els.logText.value = text;
    setParseError("");
    renderAll();
  } catch (err) {
    setParseError(err.message);
    throw err;
  }
}

function setParseError(message) {
  if (!message) {
    els.parseError.hidden = true;
    els.parseError.textContent = "";
    return;
  }
  els.parseError.hidden = false;
  els.parseError.textContent = message;
}

function parseLog(raw) {
  const lines = raw
    .replace(/\r/g, "")
    .split("\n")
    .map((line) => line.trim())
    .filter(Boolean);
  if (!lines.length) throw new Error("Log is empty.");

  let index = 0;
  const players = { A: "", B: "" };
  while (index < lines.length && lines[index] !== "MAP") {
    const match = lines[index].match(/^\[(LEFT|RIGHT|P1|P2)\s+"(.*)"\]$/);
    if (match) {
      if (match[1] === "LEFT" || match[1] === "P1") players.A = match[2];
      if (match[1] === "RIGHT" || match[1] === "P2") players.B = match[2];
    }
    index++;
  }
  if (lines[index] !== "MAP") throw new Error("MAP section was not found.");
  index++;

  const [N, K] = parseNumbers(lines[index++]);
  if (!Number.isInteger(N) || !Number.isInteger(K)) throw new Error("Invalid map size line.");
  const x = parseNumbers(lines[index++]);
  const y = parseNumbers(lines[index++]);
  if (x.length !== N || y.length !== N) throw new Error(`Expected ${N} x/y coordinates.`);

  const strongholdParts = lines[index++].split(/\s+/);
  if (strongholdParts[0] !== "STRONGHOLDS") throw new Error("STRONGHOLDS line was not found.");
  let outposts = strongholdParts.slice(1).map(Number);
  if (outposts.length === K + 1 && outposts[0] === K) outposts = outposts.slice(1);
  if (outposts.length !== K) throw new Error(`Expected ${K} strongholds, got ${outposts.length}.`);

  const adj = [];
  for (let i = 0; i < N; i++) {
    const row = parseNumbers(lines[index++]);
    const count = row[0];
    const neighbors = row.slice(1);
    if (neighbors.length !== count) throw new Error(`Invalid adjacency row for cell ${i}.`);
    adj.push(neighbors);
  }
  if (lines[index] !== "END MAP") throw new Error("END MAP line was not found.");
  index++;

  const map = {
    N,
    K,
    x,
    y,
    outposts,
    outpostSet: new Set(outposts),
    hqA: 0,
    hqB: N - 1,
    adj,
  };

  const turns = [];
  let result = null;
  while (index < lines.length) {
    const line = lines[index];
    if (line.startsWith("RESULT ")) {
      result = parseFinalResult(line);
      index++;
      continue;
    }
    const turnMatch = line.match(/^TURN\s+(\d+)$/);
    if (!turnMatch) {
      index++;
      continue;
    }
    const turn = {
      turn: Number(turnMatch[1]),
      commands: { A: [], B: [] },
      result: { upgrade: [], train: [], move: [], damage: [], siege: [] },
      time: null,
      incomplete: false,
    };
    index++;

    while (index < lines.length) {
      const commandMatch = lines[index].match(/^COMMAND\s+(LEFT|RIGHT)\s+START$/);
      if (!commandMatch) break;
      const team = commandMatch[1] === "LEFT" ? "A" : "B";
      index++;
      while (index < lines.length && lines[index] !== `COMMAND ${commandMatch[1]} END`) {
        const command = parseCommand(lines[index]);
        if (command) turn.commands[team].push(command);
        index++;
      }
      if (lines[index] !== `COMMAND ${commandMatch[1]} END`) throw new Error(`Unclosed command block at turn ${turn.turn}.`);
      index++;
    }

    if (lines[index] === `TURN ${turn.turn} RESULT`) {
      index++;
      while (index < lines.length) {
        const entry = lines[index];
        if (entry === `END TURN ${turn.turn}`) {
          index++;
          break;
        }
        if (entry.startsWith("RESULT ")) {
          result = parseFinalResult(entry);
          break;
        }
        index = parseResultLine(lines, index, turn);
      }
    } else {
      turn.incomplete = true;
    }
    turns.push(turn);
  }

  return { players, map, turns, result, rawText: raw };
}

function parseNumbers(line) {
  return line.split(/\s+/).filter(Boolean).map(Number);
}

function parseCommand(line) {
  const parts = line.split(/\s+/);
  if (parts[0] === "MOVE" && parts.length >= 3) {
    return { kind: "MOVE", student: parts[1], dest: Number(parts[2]), raw: line };
  }
  if (parts[0] === "TRAIN" && parts.length >= 2) {
    return { kind: "TRAIN", n: Number(parts[1]), raw: line };
  }
  if (parts[0] === "UPGRADE" && parts.length >= 2) {
    return { kind: "UPGRADE", dest: Number(parts[1]), raw: line };
  }
  return null;
}

function parseResultLine(lines, index, turn) {
  const line = lines[index];
  const parts = line.split(/\s+/);
  const kind = parts[0];
  if (kind === "TIME") {
    turn.time = {
      leftMs: Number(parts[2]),
      leftRemain: Number(parts[3]),
      rightMs: Number(parts[5]),
      rightRemain: Number(parts[6]),
    };
    return index + 1;
  }
  if (kind === "UPGRADE") {
    if (parts[1] === "A" || parts[1] === "B") {
      turn.result.upgrade.push({ team: parts[1], dest: Number(parts[2]) });
      return index + 1;
    }
    const count = Number(parts[1]);
    for (let i = 0; i < count; i++) {
      const row = lines[index + 1 + i].split(/\s+/);
      turn.result.upgrade.push({ team: row[0], dest: Number(row[1]) });
    }
    return index + 1 + count;
  }
  if (kind === "TRAIN") {
    if (parts[1] && /^[AB]\d+$/.test(parts[1])) {
      turn.result.train.push(...parts.slice(1));
      return index + 1;
    }
    const count = Number(parts[1]);
    if (count > 0) {
      turn.result.train.push(...lines[index + 1].split(/\s+/));
      return index + 2;
    }
    return index + 1;
  }
  if (kind === "MOVE") {
    if (parts[1] && /^[AB]\d+$/.test(parts[1])) {
      turn.result.move.push({ student: parts[1], dest: Number(parts[2]) });
      return index + 1;
    }
    const count = Number(parts[1]);
    for (let i = 0; i < count; i++) {
      const row = lines[index + 1 + i].split(/\s+/);
      turn.result.move.push({ student: row[0], dest: Number(row[1]) });
    }
    return index + 1 + count;
  }
  if (kind === "DAMAGE") {
    if (Number.isNaN(Number(parts[1]))) {
      turn.result.damage.push({ cause: parts[1], student: parts[2], damage: Number(parts[3]) });
      return index + 1;
    }
    const count = Number(parts[1]);
    for (let i = 0; i < count; i++) {
      const row = lines[index + 1 + i].split(/\s+/);
      turn.result.damage.push({ cause: row[0], student: row[1], damage: Number(row[2]) });
    }
    return index + 1 + count;
  }
  if (kind === "SIEGE") {
    if (parts[1] === "A" || parts[1] === "B") {
      turn.result.siege.push({ team: parts[1], dest: Number(parts[2]), damage: Number(parts[3]) });
      return index + 1;
    }
    const count = Number(parts[1]);
    for (let i = 0; i < count; i++) {
      const row = lines[index + 1 + i].split(/\s+/);
      turn.result.siege.push({ team: row[0], dest: Number(row[1]), damage: Number(row[2]) });
    }
    return index + 1 + count;
  }
  return index + 1;
}

function parseFinalResult(line) {
  const [, winner = "", reason = ""] = line.split(/\s+/);
  return { winner, reason, raw: line };
}

function buildReplay(parsed) {
  const shortest = computeShortestPaths(parsed.map);
  let state = initialGameState(parsed.map);
  const frames = [];
  for (const turn of parsed.turns) {
    const frame = buildFrame(state, turn, parsed.map, shortest.next);
    frames.push(frame);
    state = cloneGameState(frame.after.upkeep);
  }

  const trend = [];
  let cumIncomeA = 0;
  let cumIncomeB = 0;
  let cumSpentA = 0;
  let cumSpentB = 0;
  let trainedA = RULES.initialWarriors;
  let trainedB = RULES.initialWarriors;
  for (const frame of frames) {
    const end = frame.after.upkeep;
    const hqA = end.buildings[parsed.map.hqA];
    const hqB = end.buildings[parsed.map.hqB];
    const incomeA = sumByTeam(frame.incomes, "A", "amount");
    const incomeB = sumByTeam(frame.incomes, "B", "amount");
    const spentA = frame.spent.A;
    const spentB = frame.spent.B;
    cumIncomeA += incomeA;
    cumIncomeB += incomeB;
    cumSpentA += spentA;
    cumSpentB += spentB;
    trainedA += frame.trains.A ? frame.trains.A.ids.length : 0;
    trainedB += frame.trains.B ? frame.trains.B.ids.length : 0;
    trend.push({
      turn: frame.turn,
      resA: end.resources.A,
      resB: end.resources.B,
      incomeA,
      incomeB,
      cumIncomeA,
      cumIncomeB,
      cumSpentA,
      cumSpentB,
      studA: countStudents(end, "A"),
      studB: countStudents(end, "B"),
      cumStudA: trainedA,
      cumStudB: trainedB,
      hqA: hqA ? hqA.hp : 0,
      hqB: hqB ? hqB.hp : 0,
      outpostsA: countOutposts(end, parsed.map, "A"),
      outpostsB: countOutposts(end, parsed.map, "B"),
    });
  }

  const steps = [];
  frames.forEach((frame, turnIndex) => {
    const scenes = frame.scenes.length ? frame.scenes : ["upkeep"];
    scenes.forEach((scene) => steps.push({ turnIndex, scene }));
  });
  if (!steps.length) steps.push({ turnIndex: 0, scene: "idle" });
  return { parsed, frames, trend, steps, shortest };
}

function initialGameState(map) {
  const buildings = new Array(map.N).fill(null);
  buildings[map.hqA] = { team: "A", isHQ: true, level: 1, hp: hqMaxHp(1), maxHp: hqMaxHp(1) };
  buildings[map.hqB] = { team: "B", isHQ: true, level: 1, hp: hqMaxHp(1), maxHp: hqMaxHp(1) };
  const students = new Map();
  const studentsByCell = Array.from({ length: map.N }, () => []);
  for (let i = 1; i <= RULES.initialWarriors; i++) {
    addStudent({ students, studentsByCell }, `A${i}`, "A", map.hqA, warriorMaxHp(1));
    addStudent({ students, studentsByCell }, `B${i}`, "B", map.hqB, warriorMaxHp(1));
  }
  return {
    turn: 0,
    resources: { A: RULES.initialGold, B: RULES.initialGold },
    buildings,
    students,
    studentsByCell,
  };
}

function buildFrame(previous, turn, map, nextHop) {
  const start = cloneGameState(previous);
  start.turn = turn.turn;
  const command = cloneGameState(start);
  const commandMoves = new Map();
  const commandAnims = [];
  const commandCount = { A: turn.commands.A.length, B: turn.commands.B.length };

  for (const team of ["A", "B"]) {
    let recordedAt = 0;
    for (const commandLine of turn.commands[team]) {
      if (commandLine.kind !== "MOVE") continue;
      commandMoves.set(commandLine.student, commandLine.dest);
      const student = command.students.get(commandLine.student);
      if (!student || student.team !== team) continue;
      student.destination = commandLine.dest;
      commandAnims.push({
        student: commandLine.student,
        team,
        from: student.cell,
        destination: commandLine.dest,
        path: shortestPath(nextHop, student.cell, commandLine.dest),
        recordedAt: recordedAt++,
      });
    }
  }

  const build = cloneGameState(command);
  const builds = [];
  const spent = { A: 0, B: 0 };
  for (const upgrade of turn.result.upgrade) {
    const before = build.buildings[upgrade.dest];
    const own = before && before.team === upgrade.team ? before : null;
    const isNew = !own;
    const isHQ = own ? own.isHQ : false;
    const maxLevel = isHQ ? RULES.maxHqLevel : RULES.maxBaseLevel;
    let level;
    let cost;
    if (isNew) {
      level = 1;
      cost = RULES.baseUpgradeCost[1];
    } else if (own.level < maxLevel) {
      level = own.level + 1;
      cost = isHQ ? hqUpgradeCost(level) : baseUpgradeCost(level);
    } else {
      level = own.level;
      cost = isHQ ? RULES.hqRepairCost : RULES.baseRepairCost;
    }
    const maxHp = isHQ ? hqMaxHp(level) : baseMaxHp(level);
    build.buildings[upgrade.dest] = { team: upgrade.team, isHQ, level, hp: maxHp, maxHp };
    build.resources[upgrade.team] = Math.max(0, build.resources[upgrade.team] - cost);
    spent[upgrade.team] += cost;
    builds.push({ team: upgrade.team, dest: upgrade.dest, spent: cost, newLevel: level, isNew });
  }

  const move = cloneGameState(build);
  const moves = [];
  for (const [studentId, dest] of commandMoves) {
    const student = move.students.get(studentId);
    if (!student) continue;
    const building = move.buildings[dest];
    const isFree = building && building.team === student.team;
    if (!isFree) {
      move.resources[student.team] = Math.max(0, move.resources[student.team] - RULES.moveCost);
      spent[student.team] += RULES.moveCost;
    }
  }
  for (const moveResult of turn.result.move) {
    const student = move.students.get(moveResult.student);
    if (!student) continue;
    const from = student.cell;
    removeFromCell(move, from, moveResult.student);
    student.cell = moveResult.dest;
    addToCell(move, moveResult.dest, moveResult.student);
    if (student.destination === moveResult.dest) student.destination = null;
    moves.push({ student: moveResult.student, team: student.team, from, to: moveResult.dest });
  }

  const train = cloneGameState(move);
  const trains = { A: null, B: null };
  const trainIds = { A: [], B: [] };
  for (const studentId of turn.result.train) {
    const team = studentId[0];
    if (team !== "A" && team !== "B") continue;
    trainIds[team].push(studentId);
    const hqCell = team === "A" ? map.hqA : map.hqB;
    const hq = train.buildings[hqCell];
    const hp = warriorMaxHp(hq ? hq.level : 1);
    addStudent(train, studentId, team, hqCell, hp);
  }
  for (const team of ["A", "B"]) {
    if (!trainIds[team].length) continue;
    const cost = RULES.trainCost * trainIds[team].length;
    train.resources[team] = Math.max(0, train.resources[team] - cost);
    spent[team] += cost;
    trains[team] = { team, ids: trainIds[team], spent: cost };
  }

  const battle = cloneGameState(train);
  const battleCells = new Set();
  const damageByStudent = new Map();
  for (const damage of turn.result.damage) {
    if (damage.cause === "HUNGER") continue;
    damageByStudent.set(damage.student, (damageByStudent.get(damage.student) || 0) + damage.damage);
  }
  const damages = [];
  for (const [studentId, amount] of damageByStudent) {
    const student = battle.students.get(studentId);
    if (!student) continue;
    const hpFrom = student.hp;
    const hpTo = hpFrom - amount;
    damages.push({ student: studentId, team: student.team, cell: student.cell, hpFrom, hpTo });
    battleCells.add(student.cell);
    student.hp = hpTo;
    if (student.hp <= 0) {
      removeFromCell(battle, student.cell, studentId);
      battle.students.delete(studentId);
    }
  }
  const captures = [];
  for (const siege of turn.result.siege) {
    const building = battle.buildings[siege.dest];
    if (!building) continue;
    const hpFrom = building.hp;
    const hpTo = hpFrom - siege.damage;
    building.hp = hpTo;
    battleCells.add(siege.dest);
    const destroyed = hpTo <= 0;
    captures.push({ team: siege.team, dest: siege.dest, hpFrom, hpTo, destroyed });
    if (destroyed) battle.buildings[siege.dest] = null;
  }

  const income = cloneGameState(battle);
  const incomes = [];
  if (!turn.incomplete) {
    for (let cell = 0; cell < income.buildings.length; cell++) {
      const building = income.buildings[cell];
      if (!building) continue;
      const workers = income.studentsByCell[cell].filter((id) => {
        const student = income.students.get(id);
        return student && student.team === building.team;
      }).length;
      const activeWorkers = Math.min(workers, building.level);
      if (activeWorkers > 0) {
        const amount = RULES.incomePerWorker * activeWorkers;
        income.resources[building.team] += amount;
        incomes.push({ team: building.team, cell, amount });
      }
    }
  }

  const upkeep = cloneGameState(income);
  const upkeeps = [];
  const hunger = new Map();
  for (const damage of turn.result.damage) {
    if (damage.cause === "HUNGER") {
      hunger.set(damage.student, (hunger.get(damage.student) || 0) + damage.damage);
    }
  }
  const upkeepByCell = new Map();
  for (const [studentId, student] of upkeep.students) {
    if (hunger.has(studentId)) continue;
    upkeep.resources[student.team] = Math.max(0, upkeep.resources[student.team] - RULES.upkeepCost);
    spent[student.team] += RULES.upkeepCost;
    const key = `${student.team}:${student.cell}`;
    upkeepByCell.set(key, (upkeepByCell.get(key) || 0) + RULES.upkeepCost);
  }
  for (const [key, amount] of upkeepByCell) {
    const [team, cellText] = key.split(":");
    upkeeps.push({ team, cell: Number(cellText), amount });
  }
  for (const [studentId, amount] of hunger) {
    const student = upkeep.students.get(studentId);
    if (!student) continue;
    student.hp -= amount;
    battleCells.add(student.cell);
    if (student.hp <= 0) {
      removeFromCell(upkeep, student.cell, studentId);
      upkeep.students.delete(studentId);
    }
  }

  const scenes = [];
  if (commandAnims.length || commandCount.A || commandCount.B) scenes.push("command");
  if (builds.length) scenes.push("build");
  if (moves.length) scenes.push("move");
  if (trains.A || trains.B) scenes.push("train");
  if (damages.length || captures.length || hunger.size) scenes.push("battle");

  return {
    turn: turn.turn,
    start,
    after: { command, build, move, train, battle, income, upkeep },
    rawCommands: turn.commands,
    commandAnims,
    builds,
    moves,
    trains,
    battles: { damages, cells: battleCells, hunger },
    captures,
    incomes,
    upkeeps,
    time: turn.time,
    scenes,
    spent,
  };
}

function cloneGameState(state) {
  return {
    turn: state.turn,
    resources: { ...state.resources },
    buildings: state.buildings.map((building) => (building ? { ...building } : null)),
    students: new Map(Array.from(state.students, ([id, student]) => [id, { ...student }])),
    studentsByCell: state.studentsByCell.map((ids) => ids.slice()),
  };
}

function addStudent(state, id, team, cell, hp) {
  state.students.set(id, { id, team, cell, hp, maxHp: hp, destination: null });
  addToCell(state, cell, id);
}

function addToCell(state, cell, id) {
  if (!state.studentsByCell[cell]) state.studentsByCell[cell] = [];
  if (!state.studentsByCell[cell].includes(id)) state.studentsByCell[cell].push(id);
}

function removeFromCell(state, cell, id) {
  const bucket = state.studentsByCell[cell];
  if (!bucket) return;
  const idx = bucket.indexOf(id);
  if (idx >= 0) bucket.splice(idx, 1);
}

function computeShortestPaths(map) {
  const N = map.N;
  const inf = Number.POSITIVE_INFINITY;
  const dist = Array.from({ length: N }, () => new Array(N).fill(inf));
  const next = Array.from({ length: N }, () => new Array(N).fill(-1));
  for (let i = 0; i < N; i++) {
    dist[i][i] = 0;
    next[i][i] = i;
    for (const j of map.adj[i]) {
      const cost = cellDistance(map, i, j);
      if (cost < dist[i][j] || (cost === dist[i][j] && j < next[i][j])) {
        dist[i][j] = cost;
        next[i][j] = j;
      }
    }
  }
  for (let k = 0; k < N; k++) {
    for (let i = 0; i < N; i++) {
      if (i === k || !Number.isFinite(dist[i][k])) continue;
      for (let j = 0; j < N; j++) {
        if (j === k || !Number.isFinite(dist[k][j])) continue;
        const candidate = dist[i][k] + dist[k][j];
        const hop = next[i][k];
        if (candidate < dist[i][j] || (candidate === dist[i][j] && hop >= 0 && hop < next[i][j])) {
          dist[i][j] = candidate;
          next[i][j] = hop;
        }
      }
    }
  }
  return { dist, next };
}

function shortestPath(next, from, to) {
  if (from === to) return [from];
  if (!next[from] || next[from][to] < 0) return [from, to];
  const path = [from];
  let cur = from;
  let guard = 0;
  while (cur !== to && guard++ < 1000) {
    cur = next[cur][to];
    if (cur < 0) break;
    path.push(cur);
  }
  return path;
}

function cellDistance(map, a, b) {
  const dx = map.x[a] - map.x[b];
  const dy = map.y[a] - map.y[b];
  return Math.ceil(Math.sqrt(dx * dx + dy * dy));
}

function hqMaxHp(level) {
  return RULES.hqHp[level] || RULES.hqHp[RULES.hqHp.length - 1];
}

function warriorMaxHp(level) {
  return RULES.warriorHp[level] || RULES.warriorHp[RULES.warriorHp.length - 1];
}

function baseMaxHp(level) {
  return RULES.baseHp[level] || RULES.baseHp[RULES.baseHp.length - 1];
}

function hqUpgradeCost(level) {
  return RULES.hqUpgradeCost[level] || 0;
}

function baseUpgradeCost(level) {
  return RULES.baseUpgradeCost[level] || 0;
}

function sumByTeam(items, team, key) {
  return items.reduce((sum, item) => (item.team === team ? sum + item[key] : sum), 0);
}

function countStudents(state, team) {
  let count = 0;
  for (const student of state.students.values()) {
    if (student.team === team) count++;
  }
  return count;
}

function countOutposts(state, map, team) {
  let count = 0;
  for (const cell of map.outposts) {
    const building = state.buildings[cell];
    if (building && building.team === team) count++;
  }
  return count;
}

function buildGeometry(map) {
  const minX = Math.min(...map.x);
  const maxX = Math.max(...map.x);
  const minY = Math.min(...map.y);
  const maxY = Math.max(...map.y);
  const width = maxX - minX || 1;
  const height = maxY - minY || 1;
  const margin = Math.max(width, height) * 0.22;
  const box = {
    x: minX - margin,
    y: minY - margin,
    width: width + margin * 2,
    height: height + margin * 2,
  };
  const points = map.x.map((x, i) => [x, map.y[i]]);
  const cells = points.map((point, i) => {
    let poly = [
      [box.x, box.y],
      [box.x + box.width, box.y],
      [box.x + box.width, box.y + box.height],
      [box.x, box.y + box.height],
    ];
    for (let j = 0; j < points.length; j++) {
      if (i === j) continue;
      poly = clipVoronoi(poly, point, points[j]);
      if (!poly.length) break;
    }
    return poly;
  });
  const avgRadius = averageNeighborDistance(map) * 0.42;
  return { cells, points, box, avgRadius };
}

function clipVoronoi(poly, p, q) {
  const a = 2 * (q[0] - p[0]);
  const b = 2 * (q[1] - p[1]);
  const c = q[0] * q[0] + q[1] * q[1] - p[0] * p[0] - p[1] * p[1];
  return clipPolygon(poly, a, b, c);
}

function clipPolygon(poly, a, b, c) {
  const out = [];
  const eps = 1e-7;
  for (let i = 0; i < poly.length; i++) {
    const start = poly[i];
    const end = poly[(i + 1) % poly.length];
    const ds = a * start[0] + b * start[1] - c;
    const de = a * end[0] + b * end[1] - c;
    const startInside = ds <= eps;
    const endInside = de <= eps;
    if (startInside && endInside) {
      out.push(end);
    } else if (startInside && !endInside) {
      out.push(intersectSegment(start, end, ds, de));
    } else if (!startInside && endInside) {
      out.push(intersectSegment(start, end, ds, de), end);
    }
  }
  return out;
}

function intersectSegment(start, end, ds, de) {
  const t = ds / (ds - de);
  return [start[0] + (end[0] - start[0]) * t, start[1] + (end[1] - start[1]) * t];
}

function averageNeighborDistance(map) {
  let total = 0;
  let count = 0;
  for (let i = 0; i < map.N; i++) {
    for (const j of map.adj[i]) {
      if (j <= i) continue;
      total += Math.hypot(map.x[i] - map.x[j], map.y[i] - map.y[j]);
      count++;
    }
  }
  return count ? total / count : 600;
}

function expandBox(box, ratio) {
  return {
    x: box.x - box.width * ratio,
    y: box.y - box.height * ratio,
    width: box.width * (1 + ratio * 2),
    height: box.height * (1 + ratio * 2),
  };
}

function renderAll() {
  renderControls();
  renderMap();
  renderPanels();
  renderCharts();
}

function renderControls() {
  const replay = app.replay;
  const max = replay ? Math.max(0, replay.steps.length - 1) : 0;
  app.step = Math.max(0, Math.min(app.step, max));
  els.timeline.max = String(max);
  els.timeline.value = String(app.step);
  els.timelineText.textContent = replay ? `${app.step + 1} / ${max + 1}` : "0 / 0";
  els.playBtn.textContent = app.playing ? "Pause" : "Play";

  const frameInfo = getCurrentFrameInfo();
  const frame = frameInfo.frame;
  const scene = frameInfo.scene;
  els.turnLabel.textContent = frame ? `Turn ${frame.turn}` : "Turn 0";
  els.sceneLabel.textContent = SCENE_LABELS[scene] || "IDLE";

  const displayState = getDisplayState();
  const map = app.parsed && app.parsed.map;
  if (displayState && map) {
    const hqA = displayState.buildings[map.hqA];
    const hqB = displayState.buildings[map.hqB];
    els.teamAGold.textContent = formatNumber(displayState.resources.A);
    els.teamBGold.textContent = formatNumber(displayState.resources.B);
    els.teamAHq.textContent = hqA ? `HQ ${hqA.hp}/${hqA.maxHp}` : "HQ destroyed";
    els.teamBHq.textContent = hqB ? `HQ ${hqB.hp}/${hqB.maxHp}` : "HQ destroyed";
  }

  renderResultPill();
}

function renderResultPill() {
  const result = app.parsed && app.parsed.result;
  els.resultPill.className = "result-pill";
  if (!result) {
    els.resultPill.textContent = "RESULT";
    return;
  }
  els.resultPill.textContent = result.raw.replace("RESULT ", "");
  if (result.winner === "LEFT_WIN") els.resultPill.classList.add("left");
  else if (result.winner === "RIGHT_WIN") els.resultPill.classList.add("right");
  else if (result.winner === "DRAW") els.resultPill.classList.add("draw");
}

function getCurrentFrameInfo() {
  if (!app.replay || !app.replay.frames.length) return { frame: null, step: null, scene: "idle" };
  const step = app.replay.steps[app.step] || app.replay.steps[0];
  return {
    frame: app.replay.frames[step.turnIndex],
    step,
    scene: step.scene || "idle",
  };
}

function getDisplayState() {
  const { frame, scene } = getCurrentFrameInfo();
  if (!frame) return null;
  if (scene === "idle" || scene === "upkeep") return frame.after.upkeep;
  return frame.after[scene] || frame.after.upkeep;
}

function renderMap() {
  if (!app.replay || !app.geometry || !app.viewBox) {
    els.mapSvg.innerHTML = "";
    return;
  }

  const map = app.parsed.map;
  const state = getDisplayState();
  const { frame, scene } = getCurrentFrameInfo();
  const eventCells = frame ? cellsForFrame(frame) : new Set();
  const vb = app.viewBox;
  els.mapSvg.setAttribute("viewBox", `${vb.x} ${vb.y} ${vb.width} ${vb.height}`);

  const chunks = [];
  chunks.push(`<defs>${svgMarkers()}</defs>`);
  chunks.push(`<g class="cells-layer">${renderCells(map, state, eventCells)}</g>`);
  chunks.push(`<g class="edges-layer">${app.options.showEdges ? renderEdges(map) : ""}</g>`);
  chunks.push(`<g class="paths-layer">${renderPaths(map, state, frame, scene)}</g>`);
  chunks.push(`<g class="fx-layer">${renderFx(map, frame, scene, eventCells)}</g>`);
  chunks.push(`<g class="building-layer">${renderBuildings(map, state)}</g>`);
  chunks.push(`<g class="warrior-layer">${renderWarriors(map, state)}</g>`);
  if (app.options.showCellNumbers) chunks.push(`<g class="label-layer">${renderCellLabels(map)}</g>`);
  els.mapSvg.innerHTML = chunks.join("");
}

function svgMarkers() {
  return `
    <marker id="arrowA" viewBox="0 0 10 10" refX="9" refY="5" markerWidth="7" markerHeight="7" orient="auto-start-reverse">
      <path d="M 0 0 L 10 5 L 0 10 z" fill="${TEAM.A.color}"></path>
    </marker>
    <marker id="arrowB" viewBox="0 0 10 10" refX="9" refY="5" markerWidth="7" markerHeight="7" orient="auto-start-reverse">
      <path d="M 0 0 L 10 5 L 0 10 z" fill="${TEAM.B.color}"></path>
    </marker>
  `;
}

function renderCells(map, state, eventCells) {
  return app.geometry.cells
    .map((poly, i) => {
      if (!poly.length) return "";
      const building = state.buildings[i];
      const classes = ["cell"];
      if (map.outpostSet.has(i)) classes.push("outpost", "empty-stronghold");
      if (i === map.hqA || i === map.hqB) classes.push("hq");
      if (building && building.team === "A") classes.push("team-a-fill");
      if (building && building.team === "B") classes.push("team-b-fill");
      if (i === app.hoveredCell) classes.push("hovered");
      if (i === app.focusedCell) classes.push("focused");
      if (eventCells.has(i)) classes.push("event-cell");
      return `<polygon data-cell="${i}" class="${classes.join(" ")}" points="${pointsAttr(poly)}"></polygon>`;
    })
    .join("");
}

function renderEdges(map) {
  const lines = [];
  for (let i = 0; i < map.N; i++) {
    for (const j of map.adj[i]) {
      if (j <= i) continue;
      lines.push(`<line class="edge" x1="${map.x[i]}" y1="${map.y[i]}" x2="${map.x[j]}" y2="${map.y[j]}"></line>`);
    }
  }
  return lines.join("");
}

function renderPaths(map, state, frame, scene) {
  if (!frame) return "";
  const pathItems = [];
  if (scene === "command") {
    pathItems.push(...frame.commandAnims);
  }
  const focusCell = app.focusedCell ?? app.hoveredCell;
  if (app.options.showAllMoves || focusCell != null) {
    for (const student of state.students.values()) {
      if (student.destination == null) continue;
      if (!app.options.showAllMoves && student.cell !== focusCell) continue;
      pathItems.push({
        student: student.id,
        team: student.team,
        from: student.cell,
        destination: student.destination,
        path: shortestPath(app.replay.shortest.next, student.cell, student.destination),
      });
    }
  }
  const seen = new Set();
  const rendered = [];
  for (const item of pathItems) {
    const key = `${item.student}:${item.path.join("-")}`;
    if (seen.has(key) || item.path.length < 2) continue;
    seen.add(key);
    const points = item.path.map((cell) => `${map.x[cell]},${map.y[cell]}`).join(" ");
    const marker = item.team === "A" ? "arrowA" : "arrowB";
    rendered.push(`<polyline class="path team-${item.team.toLowerCase()}" marker-end="url(#${marker})" points="${points}"></polyline>`);
    for (const cell of item.path) {
      rendered.push(`<circle class="path-dot" cx="${map.x[cell]}" cy="${map.y[cell]}" r="${app.geometry.avgRadius * 0.07}" fill="${TEAM[item.team].color}"></circle>`);
    }
  }
  return rendered.join("");
}

function renderFx(map, frame, scene, eventCells) {
  if (!frame) return "";
  const items = [];
  if (scene === "battle") {
    for (const cell of eventCells) {
      items.push(`<circle class="battle-ring" cx="${map.x[cell]}" cy="${map.y[cell]}" r="${app.geometry.avgRadius * 0.55}"></circle>`);
    }
    for (const siege of frame.captures) {
      items.push(`<circle class="siege-ring" cx="${map.x[siege.dest]}" cy="${map.y[siege.dest]}" r="${app.geometry.avgRadius * 0.7}"></circle>`);
    }
  }
  if (scene === "upkeep" || scene === "idle") {
    for (const income of frame.incomes || []) {
      items.push(`<circle class="income-dot" cx="${map.x[income.cell]}" cy="${map.y[income.cell]}" r="${app.geometry.avgRadius * 0.16}"></circle>`);
    }
  }
  return items.join("");
}

function renderBuildings(map, state) {
  const r = app.geometry.avgRadius * 0.21;
  const hpHeight = r * 0.25;
  const hpWidth = r * 2.25;
  const parts = [];
  for (let cell = 0; cell < state.buildings.length; cell++) {
    const building = state.buildings[cell];
    if (!building) continue;
    const cx = map.x[cell];
    const cy = map.y[cell] - app.geometry.avgRadius * 0.2;
    const shape = building.isHQ ? hexPoints(cx, cy, r * 1.35) : hexPoints(cx, cy, r);
    const teamClass = building.team === "A" ? "team-a" : "team-b";
    const hpRatio = Math.max(0, Math.min(1, building.hp / building.maxHp));
    const barX = cx - hpWidth / 2;
    const barY = cy + r * 1.45;
    parts.push(`<polygon data-cell="${cell}" class="building ${teamClass}" points="${pointsAttr(shape)}"></polygon>`);
    parts.push(`<text class="building-label" x="${cx}" y="${cy}">${building.level}</text>`);
    parts.push(`<rect class="hp-bg" x="${barX}" y="${barY}" width="${hpWidth}" height="${hpHeight}" rx="${hpHeight / 2}"></rect>`);
    parts.push(`<rect class="hp-fill" x="${barX}" y="${barY}" width="${hpWidth * hpRatio}" height="${hpHeight}" rx="${hpHeight / 2}"></rect>`);
  }
  return parts.join("");
}

function renderWarriors(map, state) {
  const groups = new Map();
  for (const [id, student] of state.students) {
    const key = `${student.cell}:${student.team}`;
    if (!groups.has(key)) groups.set(key, []);
    groups.get(key).push(student);
  }
  const r = app.geometry.avgRadius * 0.16;
  const parts = [];
  for (const [key, students] of groups) {
    const [cellText, team] = key.split(":");
    const cell = Number(cellText);
    const offset = team === "A" ? -r * 1.25 : r * 1.25;
    const cx = map.x[cell] + offset;
    const cy = map.y[cell] + app.geometry.avgRadius * 0.25;
    const teamClass = team === "A" ? "team-a" : "team-b";
    const label = String(students.length);
    parts.push(`<circle data-cell="${cell}" class="warrior ${teamClass}" cx="${cx}" cy="${cy}" r="${r}"></circle>`);
    parts.push(`<text class="warrior-label" x="${cx}" y="${cy}">${label}</text>`);
  }
  return parts.join("");
}

function renderCellLabels(map) {
  return map.x
    .map((x, i) => `<text data-cell="${i}" class="cell-label" x="${x}" y="${map.y[i]}">${i}</text>`)
    .join("");
}

function cellsForFrame(frame) {
  const cells = new Set();
  for (const build of frame.builds || []) cells.add(build.dest);
  for (const move of frame.moves || []) {
    cells.add(move.from);
    cells.add(move.to);
  }
  if (frame.trains.A) cells.add(app.parsed.map.hqA);
  if (frame.trains.B) cells.add(app.parsed.map.hqB);
  for (const cell of frame.battles.cells || []) cells.add(cell);
  return cells;
}

function pointsAttr(points) {
  return points.map((p) => `${round(p[0])},${round(p[1])}`).join(" ");
}

function hexPoints(cx, cy, r) {
  const pts = [];
  for (let i = 0; i < 6; i++) {
    const angle = Math.PI / 6 + (Math.PI * 2 * i) / 6;
    pts.push([cx + Math.cos(angle) * r, cy + Math.sin(angle) * r]);
  }
  return pts;
}

function round(value) {
  return Math.round(value * 100) / 100;
}

function renderPanels() {
  renderStatusGrid();
  renderTurnDetails();
  renderCellDetails();
}

function renderStatusGrid() {
  if (!app.replay) {
    els.statusGrid.innerHTML = `<div class="empty">No log</div>`;
    return;
  }
  const state = getDisplayState();
  const map = app.parsed.map;
  const hqA = state.buildings[map.hqA];
  const hqB = state.buildings[map.hqB];
  const stats = [
    ["A Gold", formatNumber(state.resources.A), "team-a"],
    ["B Gold", formatNumber(state.resources.B), "team-b"],
    ["A Warriors", countStudents(state, "A"), "team-a"],
    ["B Warriors", countStudents(state, "B"), "team-b"],
    ["A Outposts", countOutposts(state, map, "A"), "team-a"],
    ["B Outposts", countOutposts(state, map, "B"), "team-b"],
    ["A HQ", hqA ? `${hqA.hp}/${hqA.maxHp}` : "0", "team-a"],
    ["B HQ", hqB ? `${hqB.hp}/${hqB.maxHp}` : "0", "team-b"],
  ];
  els.statusGrid.innerHTML = stats
    .map(([label, value, tone]) => `<div class="stat-box ${tone}"><span>${label}</span><strong>${value}</strong></div>`)
    .join("");
}

function renderTurnDetails() {
  if (!app.replay) {
    els.turnDetails.innerHTML = `<div class="empty">No log</div>`;
    return;
  }
  const { frame } = getCurrentFrameInfo();
  if (!frame) {
    els.turnDetails.innerHTML = `<div class="empty">No turn</div>`;
    return;
  }

  const commandCards = ["A", "B"].map((team) => renderCommandCard(frame, team)).join("");
  const events = [];
  for (const build of frame.builds) events.push(["Build", `${build.team} #${build.dest} Lv.${build.newLevel} -${build.spent}`]);
  if (frame.trains.A) events.push(["Train", `${frame.trains.A.ids.join(" ")} -${frame.trains.A.spent}`]);
  if (frame.trains.B) events.push(["Train", `${frame.trains.B.ids.join(" ")} -${frame.trains.B.spent}`]);
  for (const move of frame.moves.slice(0, 18)) events.push(["Move", `${move.student} #${move.from} -> #${move.to}`]);
  if (frame.moves.length > 18) events.push(["Move", `+${frame.moves.length - 18} more`]);
  for (const damage of frame.battles.damages) events.push(["Damage", `${damage.student} ${damage.hpFrom} -> ${damage.hpTo}`]);
  for (const capture of frame.captures) events.push(["Siege", `${capture.team} #${capture.dest} ${capture.hpFrom} -> ${capture.hpTo}`]);
  for (const income of frame.incomes) events.push(["Income", `${income.team} #${income.cell} +${income.amount}`]);
  const eventRows = events.length
    ? `<div class="event-list">${events.map(([k, v]) => `<div class="event-row"><b>${k}</b><span>${escapeHtml(v)}</span></div>`).join("")}</div>`
    : `<div class="empty">No visible events in this turn.</div>`;
  const time = frame.time
    ? `<div class="event-row"><b>Time</b><span>LEFT ${frame.time.leftMs}ms / ${frame.time.leftRemain}, RIGHT ${frame.time.rightMs}ms / ${frame.time.rightRemain}</span></div>`
    : "";

  els.turnDetails.innerHTML = `${commandCards}${time}${eventRows}`;
}

function renderCommandCard(frame, team) {
  const rows = frame.rawCommands[team].map((command) => {
    if (command.kind === "MOVE") {
      const afterBuild = frame.after.build;
      const building = afterBuild.buildings[command.dest];
      const free = building && building.team === team;
      return ["Move", command.student, `#${command.dest}`, free ? "FREE" : `-${RULES.moveCost}`];
    }
    if (command.kind === "TRAIN") return ["Train", command.n, "", `-${command.n * RULES.trainCost}`];
    const build = frame.builds.find((item) => item.team === team && item.dest === command.dest);
    return ["Upgrade", `#${command.dest}`, "", build ? `-${build.spent}` : ""];
  });
  const body = rows.length
    ? rows
        .map(
          (row) =>
            `<tr><td>${escapeHtml(row[0])}</td><td>${escapeHtml(String(row[1]))}</td><td>${escapeHtml(String(row[2] || "-"))}</td><td>${escapeHtml(String(row[3] || ""))}</td></tr>`,
        )
        .join("")
    : `<tr><td colspan="4" class="empty">No commands</td></tr>`;
  return `
    <div class="command-card team-${team.toLowerCase()}">
      <table>
        <thead><tr><th>${team}</th><th>Arg 1</th><th>Arg 2</th><th>Cost</th></tr></thead>
        <tbody>${body}</tbody>
      </table>
    </div>
  `;
}

function renderCellDetails() {
  if (!app.replay) {
    els.cellDetails.innerHTML = `<div class="empty">No log</div>`;
    return;
  }
  const cell = app.focusedCell ?? app.hoveredCell;
  if (cell == null) {
    els.cellDetails.innerHTML = `<div class="empty">Hover over or click a cell to see its details.</div>`;
    return;
  }
  const state = getDisplayState();
  const map = app.parsed.map;
  const building = state.buildings[cell];
  const warriors = (state.studentsByCell[cell] || [])
    .map((id) => state.students.get(id))
    .filter(Boolean)
    .sort(compareStudents);
  const warriorBadges = warriors.length
    ? `<div class="badge-list">${warriors
        .map(
          (student) =>
            `<span class="badge team-${student.team.toLowerCase()}">${student.id} ${student.hp}/${student.maxHp}${student.destination != null ? ` -> #${student.destination}` : ""}</span>`,
        )
        .join("")}</div>`
    : "None";
  const adjBadges = `<div class="badge-list">${map.adj[cell].map((id) => `<span class="badge">#${id}</span>`).join("")}</div>`;
  const type = cell === map.hqA || cell === map.hqB ? "HQ" : map.outpostSet.has(cell) ? "Outpost" : "Field";
  const buildingText = building
    ? `<span class="badge team-${building.team.toLowerCase()}">${building.isHQ ? "HQ" : "Base"} ${building.team} Lv.${building.level} ${building.hp}/${building.maxHp}</span>`
    : "None";
  els.cellDetails.innerHTML = `
    <dl class="detail-grid">
      <dt>Cell</dt><dd><span class="badge">#${cell} ${type}</span></dd>
      <dt>Center</dt><dd><span class="badge">(${map.x[cell]}, ${map.y[cell]})</span></dd>
      <dt>Adjacent</dt><dd>${adjBadges}</dd>
      <dt>Building</dt><dd>${buildingText}</dd>
      <dt>Warriors</dt><dd>${warriorBadges}</dd>
    </dl>
  `;
}

function compareStudents(a, b) {
  if (a.team !== b.team) return a.team.localeCompare(b.team);
  return Number(a.id.slice(1)) - Number(b.id.slice(1));
}

function renderCharts() {
  if (!app.replay || !app.replay.trend.length) {
    for (const svg of [els.chartHp, els.chartGold, els.chartWarriors]) svg.innerHTML = "";
    return;
  }
  renderLineChart(els.chartHp, app.replay.trend, ["hqA", "hqB"]);
  renderLineChart(els.chartGold, app.replay.trend, ["resA", "resB"]);
  renderLineChart(els.chartWarriors, app.replay.trend, ["studA", "studB"]);
}

function renderLineChart(svg, data, keys) {
  const width = 320;
  const height = 92;
  const pad = 12;
  const values = keys.flatMap((key) => data.map((d) => d[key]));
  const min = Math.min(0, ...values);
  const max = Math.max(1, ...values);
  const xFor = (i) => pad + (data.length <= 1 ? 0 : (i / (data.length - 1)) * (width - pad * 2));
  const yFor = (value) => height - pad - ((value - min) / (max - min || 1)) * (height - pad * 2);
  const lines = keys
    .map((key) => {
      const team = key.endsWith("A") ? "A" : "B";
      const points = data.map((d, i) => `${round(xFor(i))},${round(yFor(d[key]))}`).join(" ");
      return `<polyline fill="none" stroke="${TEAM[team].color}" stroke-width="2" points="${points}"></polyline>`;
    })
    .join("");
  svg.innerHTML = `<line x1="${pad}" y1="${height - pad}" x2="${width - pad}" y2="${height - pad}" stroke="rgba(255,255,255,.16)"></line>${lines}`;
}

function onMapMove(event) {
  const target = event.target.closest("[data-cell]");
  const cell = target ? Number(target.dataset.cell) : null;
  if (cell !== app.hoveredCell) {
    app.hoveredCell = Number.isInteger(cell) ? cell : null;
    renderAll();
  }
}

function onMapClick(event) {
  if (app.suppressClick) {
    app.suppressClick = false;
    return;
  }
  const target = event.target.closest("[data-cell]");
  if (!target) return;
  const cell = Number(target.dataset.cell);
  app.focusedCell = app.focusedCell === cell ? null : cell;
  renderAll();
}

function onMapWheel(event) {
  if (!app.viewBox) return;
  event.preventDefault();
  const rect = els.mapSvg.getBoundingClientRect();
  const px = (event.clientX - rect.left) / rect.width;
  const py = (event.clientY - rect.top) / rect.height;
  const scale = event.deltaY < 0 ? 0.86 : 1.16;
  const nextWidth = app.viewBox.width * scale;
  const nextHeight = app.viewBox.height * scale;
  app.viewBox = {
    x: app.viewBox.x + (app.viewBox.width - nextWidth) * px,
    y: app.viewBox.y + (app.viewBox.height - nextHeight) * py,
    width: nextWidth,
    height: nextHeight,
  };
  renderMap();
}

function onMapPointerDown(event) {
  if (!app.viewBox) return;
  els.mapSvg.setPointerCapture?.(event.pointerId);
  app.drag = {
    x: event.clientX,
    y: event.clientY,
    viewBox: { ...app.viewBox },
    moved: false,
  };
}

function onMapPointerMove(event) {
  if (!app.drag || !app.viewBox) return;
  const rect = els.mapSvg.getBoundingClientRect();
  const dx = ((event.clientX - app.drag.x) / rect.width) * app.drag.viewBox.width;
  const dy = ((event.clientY - app.drag.y) / rect.height) * app.drag.viewBox.height;
  if (Math.abs(event.clientX - app.drag.x) + Math.abs(event.clientY - app.drag.y) > 3) app.drag.moved = true;
  app.viewBox = {
    ...app.drag.viewBox,
    x: app.drag.viewBox.x - dx,
    y: app.drag.viewBox.y - dy,
  };
  renderMap();
}

function onMapPointerUp() {
  if (app.drag && app.drag.moved) app.suppressClick = true;
  window.setTimeout(() => {
    app.drag = null;
  }, 0);
}

function fitMap() {
  if (!app.baseViewBox) return;
  app.viewBox = { ...app.baseViewBox };
  renderMap();
}

function toggleFullscreen() {
  if (document.fullscreenElement) document.exitFullscreen();
  else document.documentElement.requestFullscreen?.();
}

function jumpStart() {
  app.step = 0;
  renderAll();
}

function jumpEnd() {
  if (!app.replay) return;
  app.step = Math.max(0, app.replay.steps.length - 1);
  renderAll();
}

function stepBack() {
  app.step = Math.max(0, app.step - 1);
  renderAll();
}

function stepForward() {
  if (!app.replay) return;
  app.step = Math.min(app.replay.steps.length - 1, app.step + 1);
  renderAll();
}

function togglePlay() {
  app.playing = !app.playing;
  app.lastTick = 0;
  renderControls();
}

function setSpeed(speed) {
  app.speed = speed;
  document.querySelectorAll(".speed-btn").forEach((btn) => btn.classList.toggle("active", Number(btn.dataset.speed) === speed));
}

function animationLoop(now) {
  if (app.playing && app.replay) {
    if (!app.lastTick) app.lastTick = now;
    const elapsed = now - app.lastTick;
    const interval = 750 / app.speed;
    if (elapsed >= interval) {
      const inc = Math.max(1, Math.floor(elapsed / interval));
      app.step += inc;
      if (app.step >= app.replay.steps.length - 1) {
        app.step = app.replay.steps.length - 1;
        app.playing = false;
      }
      app.lastTick = now;
      renderAll();
    }
  }
  requestAnimationFrame(animationLoop);
}

function onKeyDown(event) {
  const target = event.target;
  if (target && (target.tagName === "INPUT" || target.tagName === "TEXTAREA" || target.isContentEditable)) return;
  if (event.key === "ArrowLeft") {
    event.preventDefault();
    event.shiftKey ? jumpStart() : stepBack();
  } else if (event.key === "ArrowRight") {
    event.preventDefault();
    event.shiftKey ? jumpEnd() : stepForward();
  } else if (event.key === " ") {
    event.preventDefault();
    togglePlay();
  } else if (/^[1-5]$/.test(event.key)) {
    setSpeed([0.5, 1, 2, 4, 8][Number(event.key) - 1]);
  }
}

function onDragOver(event) {
  event.preventDefault();
  els.dropOverlay.hidden = false;
}

function onDragLeave(event) {
  if (event.relatedTarget == null || event.target === document.body) els.dropOverlay.hidden = true;
}

function onDrop(event) {
  event.preventDefault();
  els.dropOverlay.hidden = true;
  const file = event.dataTransfer && event.dataTransfer.files && event.dataTransfer.files[0];
  if (!file) return;
  file.text().then((text) => setLogText(text, file.name, file.name)).catch((err) => setParseError(err.message));
}

async function copyCurrentLog() {
  if (!app.rawText) return;
  try {
    await navigator.clipboard.writeText(app.rawText);
  } catch (err) {
    setParseError(`Copy failed: ${err.message}`);
  }
}

function downloadCurrentLog() {
  if (!app.rawText) return;
  const blob = new Blob([app.rawText], { type: "text/plain;charset=utf-8" });
  const url = URL.createObjectURL(blob);
  const anchor = document.createElement("a");
  const stamp = new Date().toISOString().replace(/[:.]/g, "-");
  anchor.href = url;
  anchor.download = `next-nation-log-${stamp}.log`;
  document.body.appendChild(anchor);
  anchor.click();
  anchor.remove();
  URL.revokeObjectURL(url);
}

function formatNumber(value) {
  return Math.round(value).toLocaleString("en-US");
}

function escapeHtml(value) {
  return String(value)
    .replace(/&/g, "&amp;")
    .replace(/</g, "&lt;")
    .replace(/>/g, "&gt;")
    .replace(/"/g, "&quot;");
}
