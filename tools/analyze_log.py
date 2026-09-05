#!/usr/bin/env python3
"""Per-side timeline analysis of a NEXT VISION referee log."""
import sys, re, collections

def analyze(path):
    lines = open(path).read().splitlines()
    i = lines.index("MAP")
    N, K = map(int, lines[i+1].split())
    sh = set(map(int, lines[i+4].split()[1:]))
    adj = [list(map(int, lines[i+5+r].split()[1:])) for r in range(N)]
    hq = {"LEFT": 0, "RIGHT": N-1}
    # per-turn state
    turn = 0; side = None; phase = None
    cmds = {"LEFT": collections.defaultdict(list), "RIGHT": collections.defaultdict(list)}
    trained = {"LEFT": [], "RIGHT": []}
    upg = {"LEFT": [], "RIGHT": []}
    hunger = {"LEFT": collections.Counter(), "RIGHT": collections.Counter()}
    dmg = {"LEFT": 0, "RIGHT": 0}
    siege = {"LEFT": [], "RIGHT": []}
    pos = {}  # warrior -> region
    hp = {}
    alive = {"LEFT": {}, "RIGHT": {}}
    army_ts = {"LEFT": {}, "RIGHT": {}}
    times = {"LEFT": 0, "RIGHT": 0}
    for ln in lines[i+5+N+1:]:
        m = re.match(r"^TURN (\d+)$", ln)
        if m: turn = int(m.group(1)); phase = "cmd"; continue
        m = re.match(r"^COMMAND (LEFT|RIGHT) START", ln)
        if m: side = m.group(1); continue
        if re.match(r"^COMMAND (LEFT|RIGHT) END", ln): side = None; continue
        if re.match(r"^TURN \d+ RESULT", ln): phase = "res"; continue
        if ln.startswith("END TURN"):
            for s in ("LEFT", "RIGHT"):
                army_ts[s][turn] = len(alive[s])
            continue
        if phase == "cmd" and side:
            cmds[side][turn].append(ln); continue
        if phase == "res":
            t = ln.split()
            if not t: continue
            if t[0] == "TIME":
                times["LEFT"] = max(times["LEFT"], int(t[2])); times["RIGHT"] = max(times["RIGHT"], int(t[5]))
            elif t[0] == "TRAIN":
                for w in t[1:]:
                    s = "LEFT" if w[0] == "A" else "RIGHT"
                    trained[s].append((turn, w)); alive[s][w] = 1; pos[w] = hq[s]
            elif t[0] == "UPGRADE":
                s = "LEFT" if t[1] == "A" else "RIGHT"; upg[s].append((turn, int(t[2])))
            elif t[0] == "MOVE":
                pos[t[1]] = int(t[2])
            elif t[0] == "DAMAGE":
                s = "LEFT" if t[2][0] == "A" else "RIGHT"
                if t[1] == "HUNGER": hunger[s][turn] += 1
                dmg[s] += int(t[3])
                hp[t[2]] = hp.get(t[2], 0) - int(t[3])
            elif t[0] == "SIEGE":
                s = "LEFT" if t[1] == "A" else "RIGHT"; siege[s].append((turn, int(t[2]), int(t[3])))
            elif t[0] == "RESULT":
                result = ln
    # deaths: we don't get them directly; approximate via warriors that stop appearing is hard. Skip.
    print(f"### {path}: {result}   N={N} K={K}  max TIME L/R = {times['LEFT']}/{times['RIGHT']} ms")
    for s in ("LEFT", "RIGHT"):
        first_build = {}
        for t, r in upg[s]:
            first_build.setdefault(r, t)
        bases = [(r, t) for r, t in first_build.items() if r != hq[s]]
        hq_ups = [t for t, r in upg[s] if r == hq[s]]
        tr = trained[s]
        idle = sum(1 for t in range(1, turn+1) if not cmds[s][t])
        print(f"  {s}: bases built {len(bases)} {sorted(bases, key=lambda x: x[1])}")
        print(f"        HQ upgrades at {hq_ups}; trains {len(tr)} (first 10: {[t for t,_ in tr[:10]]}); hunger events {sum(hunger[s].values())} on {len(hunger[s])} turns (first {sorted(hunger[s])[:5]})")
        print(f"        siege taken {len(siege[s])} rows on regions {sorted(set(r for _,r,_ in siege[s]))}; idle turns {idle}/{turn}")
        # move-command stats: how many single-warrior moves to far targets
        mv = [(t, c.split()[1], int(c.split()[2])) for t in cmds[s] for c in cmds[s][t] if c.startswith("MOVE")]
        big = collections.Counter(t for t, _, _ in mv)
        waves = [(t, n) for t, n in sorted(big.items()) if n >= 5]
        print(f"        move cmds {len(mv)}; waves(>=5 moves in one turn) {waves[:12]}")
        print(f"        army size @turn: " + " ".join(f"{t}:{army_ts[s].get(t,'-')}" for t in range(25, turn+1, 25)))

for p in sys.argv[1:]:
    analyze(p)
    print()
