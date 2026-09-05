#!/usr/bin/env python3
import sys, collections
from replay import replay, tt
from rules import hops
S = tt.Side
def run(path, who="LEFT"):
    side = S.LEFT if who=="LEFT" else S.RIGHT; opp=side.opp
    ev=[]; armystart=None; stage=collections.Counter(); scout=[]
    def hook(when, day, st, m, subs, cm):
        nonlocal armystart
        if when!="pre": return
        hqr=tt.hq_region(m,side); ohq=tt.hq_region(m,opp)
        h0=hops(m,hqr); h1=hops(m,ohq); dj=tt.dijkstra_from(m,hqr)
        sub=subs[side]
        own={b.region:b for b in st.buildings.values() if b.side is side}
        ws=[w for w in st.warriors.values() if w.side is side]
        slots=sum(b.work_cap() for b in own.values())
        if armystart is None and len(ws)>slots+2 and day>50: armystart=(day,len(ws),slots,len(own)-1,st.gold[side.value])
        if sub:
            for r in sub.upgrades:
                b=st.buildings.get(r)
                if b is None:
                    free=[s for s in m.strongholds if s not in st.buildings]
                    bydj=sorted(free,key=lambda s:(dj[s],s))
                    ev.append(f"T{day} BUILD {r} h{h0[r]}/{h1[r]} rank{bydj.index(r)} nb{len(own)-1} g{st.gold[side.value]}")
                elif b.kind is tt.BKind.HQ:
                    ev.append(f"T{day} HQ{b.level}->{b.level+1} nb{len(own)-1} W{len(ws)} g{st.gold[side.value]}")
                elif b.level<3 and not any(e.startswith(f"T") and f"BASE{b.level}->" in e for e in ev[-3:]):
                    ev.append(f"T{day} BASE{b.level}->{b.level+1}@{r} nb{len(own)-1} g{st.gold[side.value]}")
            for sfx,tgt in sub.moves:
                if h1[tgt]<=2 and tgt not in st.buildings: scout.append((day,tgt,h1[tgt]))
        if 100<=day<=250:
            for w in ws:
                if w.region not in own and w.moving_target is None: stage[(w.region,h0[w.region],h1[w.region])]+=1
    st,m,res=replay(path,hook)
    print(f"### {path.split('/')[-2]} {res} N={m.N} K={m.K} armystart(day,W,slots,nb,gold)={armystart}")
    for e in ev: print("   ",e)
    print("   staging(zone,h0,h1):",stage.most_common(5))
    print("   scout moves:",scout[:6])
if __name__=="__main__":
    for p in sys.argv[1:]: run(p)
