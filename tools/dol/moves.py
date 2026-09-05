#!/usr/bin/env python3
import sys, collections
from replay import replay, tt
S = tt.Side
def run(path, who="LEFT", t0=1, t1=400):
    side = S.LEFT if who=="LEFT" else S.RIGHT; opp = side.opp
    def hook(when, day, st, m, subs, cm):
        if when != "pre" or day < t0 or day > t1: return
        sub = subs[side]
        if not sub or not sub.moves: return
        shs = set(m.strongholds)
        hqr = tt.hq_region(m, side); ohq = tt.hq_region(m, opp)
        cls = collections.defaultdict(list)
        for sfx, tgt in sub.moves:
            w = st.warriors.get(tt.wkey(side, sfx))
            b = st.buildings.get(tgt)
            if tgt == ohq: k = "ENEMY_HQ"
            elif b and b.side is opp: k = f"ENEMY_BASE L{b.level} hp{b.hp}"
            elif b and b.side is side: k = "OWN_BLDG" if tgt != hqr else "OWN_HQ"
            elif tgt in shs: k = "NEUTRAL_SH"
            else: k = "FIELD"
            cls[(k, tgt)].append((f"{who[0]}{sfx}", w.region if w else -1, w.hp if w else -1))
        ws = [w for w in st.warriors.values() if w.side is side]
        ows = [w for w in st.warriors.values() if w.side is opp]
        # enemy warriors visible? just list enemy counts by region near targets
        print(f"T{day} gold {st.gold[side.value]} W {len(ws)} oppW {len(ows)}")
        for (k, tgt), lst in sorted(cls.items(), key=lambda x: -len(x[1])):
            srcs = collections.Counter(s for _, s, _ in lst)
            ecount = sum(1 for w in ows if w.region == tgt)
            print(f"   {len(lst):2d} -> {tgt:3d} {k:22s} enemy@tgt {ecount}  from {dict(srcs)}")
    replay(path, hook)
if __name__ == "__main__":
    run(sys.argv[1], sys.argv[2], int(sys.argv[3]), int(sys.argv[4]))
