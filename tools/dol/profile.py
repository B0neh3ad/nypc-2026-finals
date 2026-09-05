#!/usr/bin/env python3
import sys, collections
from replay import replay, tt
S = tt.Side

def profile(path, who="LEFT", every=10, maxturn=400):
    side = S.LEFT if who == "LEFT" else S.RIGHT
    opp = side.opp
    rows = []; events = []
    pregold = {}
    def hook(when, day, st, m, subs, cm):
        u = side.value
        if when == "pre":
            pregold[day] = st.gold[u]
            sub = subs[side]
            if sub:
                for r in sub.upgrades:
                    b = st.buildings.get(r)
                    kind = "BUILD" if b is None else ("HQ%d->%d" % (b.level, b.level+1) if b.kind is tt.BKind.HQ else ("BASE%d->%d" % (b.level, b.level+1) if b.level < 3 else "REPAIR"))
                    events.append((day, f"{kind}@{r}", "gold", st.gold[u]))
            return
        if day % every == 0 or day == 1:
            hqr = tt.hq_region(m, side); ohq = tt.hq_region(m, opp)
            own_b = {b.region: b for b in st.buildings.values() if b.side is side}
            hq = own_b.get(hqr)
            ws = [w for w in st.warriors.values() if w.side is side]
            at_hq = sum(1 for w in ws if w.region == hqr)
            at_base = sum(1 for w in ws if w.region in own_b and w.region != hqr)
            dist_own = tt.dijkstra_from(m, hqr); dist_opp = tt.dijkstra_from(m, ohq)
            fwd = sum(1 for w in ws if dist_opp[w.region] < dist_own[w.region])
            moving = sum(1 for w in ws if w.moving_target is not None)
            bl = collections.Counter(b.level for b in own_b.values() if b.kind is tt.BKind.BASE)
            ows = [w for w in st.warriors.values() if w.side is opp]
            obl = collections.Counter(b.level for b in st.buildings.values() if b.side is opp and b.kind is tt.BKind.BASE)
            rows.append(f"T{day:3d} gold {st.gold[u]:5d} W {len(ws):3d} (hq {at_hq:2d} base {at_base:2d} fwd {fwd:2d} mv {moving:2d}) HQ L{hq.level if hq else 0} hp{hq.hp if hq else 0:2d} bases {dict(sorted(bl.items()))} | opp W {len(ows):3d} bases {dict(sorted(obl.items()))} gold {st.gold[1-u]}")
    st, m, res = replay(path, hook)
    print(f"### {path}  {res}  N={m.N} K={m.K}  {who}=돌")
    for r in rows: print("  " + r)
    print("  events:")
    for e in events: print("   ", e)
    return st, m
if __name__ == "__main__":
    profile(sys.argv[1], sys.argv[2] if len(sys.argv) > 2 else "LEFT", int(sys.argv[3]) if len(sys.argv) > 3 else 10)
