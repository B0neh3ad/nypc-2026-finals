#!/usr/bin/env python3
import sys, collections
from replay import replay, tt
S = tt.Side
def hops(m, src):
    d=[-1]*m.N; d[src]=0; q=[src]
    for u in q:
        for v in m.adj[u]:
            if d[v]<0: d[v]=d[u]+1; q.append(v)
    return d
def run(path, who="LEFT"):
    side = S.LEFT if who=="LEFT" else S.RIGHT; opp=side.opp
    claims=[]; trains=[]
    def hook(when, day, st, m, subs, cm):
        if when!="pre": return
        hqr=tt.hq_region(m,side); ohq=tt.hq_region(m,opp)
        h0=hops(m,hqr); h1=hops(m,ohq); dj=tt.dijkstra_from(m,hqr)
        sub=subs[side]
        own={b.region:b for b in st.buildings.values() if b.side is side}
        ws=[w for w in st.warriors.values() if w.side is side]
        if sub:
            for r in sub.upgrades:
                if r not in st.buildings:
                    # rank among unowned strongholds by hop and by path len
                    free=[s for s in m.strongholds if s not in st.buildings]
                    byhop=sorted(free,key=lambda s:(h0[s],s)); bydj=sorted(free,key=lambda s:(dj[s],s))
                    claims.append((day,r,h0[r],h1[r],byhop.index(r),bydj.index(r),len(free), st.gold[side.value]))
            # training
            slots=sum(b.work_cap() for b in own.values())
            hq=own.get(hqr)
            trains.append((day, st.gold[side.value], sub.train_n if sub.has_train else 0, len(own)-1, len(ws), slots, hq.level if hq else 0))
    st,m,res=replay(path,hook)
    print(f"### {path.split('/')[-2]} {res} N={m.N} K={m.K}")
    print(" claims (day, region, hopsFromOwnHQ, hopsFromEnemyHQ, rankByHop, rankByPathLen, nFree, gold):")
    for c in claims: print("   ",c)
    # training summary: first 130 turns compressed
    print(" train (day gold n bases W slots hq):")
    prev=None
    for t in trains:
        if t[2]>0 or (t[0]%10==0): print("   ",t)
    return claims,trains
if __name__=="__main__":
    for p in sys.argv[1:]: run(p)
