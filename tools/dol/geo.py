import sys, collections
from replay import replay, tt
from rules import hops
S=tt.Side
path=sys.argv[1]; who=sys.argv[2]; days=set(map(int,sys.argv[3:]))
side=S.LEFT if who=="LEFT" else S.RIGHT; opp=side.opp
def hook(when, day, st, m, subs, cm):
    if when!="pre" or day not in days: return
    hqr=tt.hq_region(m,side); ohq=tt.hq_region(m,opp); h0=hops(m,hqr); h1=hops(m,ohq)
    vis=set()
    for w in st.warriors.values():
        if w.side is side: vis|=tt._hop_set(w.region,2,m.adj)
    for b in st.buildings.values():
        if b.side is side: vis|=tt._hop_set(b.region,2,m.adj)
    own=[(b.region,h0[b.region],h1[b.region],b.level) for b in st.buildings.values() if b.side is side and b.kind is tt.BKind.BASE]
    en=[(b.region,h0[b.region],h1[b.region],b.level,b.hp,b.region in vis, sum(1 for w in st.warriors.values() if w.side is opp and w.region==b.region)) for b in st.buildings.values() if b.side is opp and b.kind is tt.BKind.BASE]
    free=[(r,h0[r],h1[r]) for r in m.strongholds if r not in st.buildings]
    stacks=collections.Counter(w.region for w in st.warriors.values() if w.side is side)
    print(f"T{day} HQ-HQ hops {h0[ohq]}")
    print("  own bases (r,h_my,h_opp,lvl):",sorted(own,key=lambda x:x[2]))
    print("  enemy bases (r,h_my,h_opp,lvl,hp,visible,defenders):",sorted(en,key=lambda x:x[1]))
    print("  free strongholds:",sorted(free,key=lambda x:x[1]))
    print("  my stacks (zone:n) top:",[(r,n,h0[r],h1[r]) for r,n in stacks.most_common(4)])
replay(path,hook)
