#!/usr/bin/env python3
"""Replay a NEXT VISION referee log through the official engine (testing-tool.py)
to recover full hidden state per turn (gold, positions, hp, buildings)."""
import sys, re, importlib.util, collections
spec = importlib.util.spec_from_file_location("tt", "/srv/nypc/problem/nation-fr-providing/testing-tool.py")
tt = importlib.util.module_from_spec(spec); spec.loader.exec_module(tt)

def parse_log(path):
    lines = open(path).read().splitlines()
    i = lines.index("MAP")
    N, K = map(int, lines[i+1].split())
    maplines = [lines[i+1], lines[i+2], lines[i+3], " ".join(lines[i+4].split()[1:])]
    maplines += lines[i+5:i+5+N]
    m = tt.read_map(maplines)
    turns = {}  # turn -> {"LEFT": [...], "RIGHT": [...], "res": [...]}
    turn = 0; side = None; phase = None; result = None
    for ln in lines[i+5+N+1:]:
        mm = re.match(r"^TURN (\d+)$", ln)
        if mm: turn = int(mm.group(1)); turns[turn] = {"LEFT": [], "RIGHT": [], "res": []}; phase = "cmd"; continue
        mm = re.match(r"^COMMAND (LEFT|RIGHT) START", ln)
        if mm: side = mm.group(1); continue
        if re.match(r"^COMMAND (LEFT|RIGHT) END", ln): side = None; continue
        if re.match(r"^TURN \d+ RESULT", ln): phase = "res"; continue
        if ln.startswith("END TURN"): continue
        if ln.startswith("RESULT"): result = ln; continue
        if phase == "cmd" and side: turns[turn][side].append(ln)
        elif phase == "res": turns[turn]["res"].append(ln)
    return m, turns, result

def replay(path, hook=None):
    """hook(day, st, m, rb_l, rb_r, cmds) called after each day."""
    m, turns, result = parse_log(path)
    st = tt.init_state(m)
    S = tt.Side
    for day in sorted(turns):
        st.day = day
        cm = turns[day]
        rb_l, rb_r = tt.ResultBlock(), tt.ResultBlock()
        subs = {}
        for side, rb in ((S.LEFT, rb_l), (S.RIGHT, rb_r)):
            try:
                sub = tt.parse_block(side, cm[side.name_str])
                subs[side] = sub
            except tt.WaError as e:
                subs[side] = None
        if hook: hook("pre", day, st, m, subs, cm)
        ok = True
        n = {}
        for side, rb in ((S.LEFT, rb_l), (S.RIGHT, rb_r)):
            try:
                tt.apply_upgrades(st, m, side, subs[side], rb)
                tt.apply_moves(st, m, side, subs[side])
                n[side] = tt.apply_train_charge(st, side, subs[side])
            except tt.WaError as e:
                print("WA", day, side, e.msg); ok = False; n[side] = 0
        siege = {}
        tt.apply_day_movement(st, m, rb_l, rb_r)
        tt.spawn_trained(st, S.LEFT, n[S.LEFT], rb_l)
        tt.spawn_trained(st, S.RIGHT, n[S.RIGHT], rb_r)
        tt.apply_day_combat(st, rb_l, rb_r, siege)
        tt.apply_day_siege(st, rb_l, rb_r, siege)
        tt.apply_evening_work(st)
        tt.apply_evening_upkeep(st, rb_l, rb_r)
        # verify against log
        _, up, mv, dmg, sg = tt._merge_results(rb_l, rb_r)
        exp = [l for l in cm["res"] if l.split()[0] in ("UPGRADE","MOVE","DAMAGE","SIEGE","TRAIN")]
        got = [f"UPGRADE {s.letter} {r}" for r, s in up]
        rec = sorted(rb_l.trained_keys + rb_r.trained_keys, key=tt.wkey_sort)
        if rec: got.append("TRAIN " + " ".join(tt.id_str(k) for k in rec))
        got += [f"MOVE {tt.id_str(k)} {r}" for k, r in mv]
        got += [f"DAMAGE {c} {tt.id_str(k)} {d}" for c, k, d in dmg]
        got += [f"SIEGE {s.letter} {r} {d}" for s, r, d in sg]
        if exp != got:
            print(f"MISMATCH day {day}\n exp {exp}\n got {got}"); 
        if hook: hook("post", day, st, m, subs, cm)
        if tt.any_hq_destroyed(st): break
    return st, m, result

if __name__ == "__main__":
    for p in sys.argv[1:]:
        st, m, res = replay(p)
        print(p, res, "gold", st.gold, "warriors", collections.Counter(w.side.name for w in st.warriors.values()))
