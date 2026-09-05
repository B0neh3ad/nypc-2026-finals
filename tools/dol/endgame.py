import sys, collections
from replay import replay, tt
from rules import hops
S=tt.Side
path=sys.argv[1]; who=sys.argv[2]; t0=int(sys.argv[3]); t1=int(sys.argv[4])
side=S.LEFT if who=="LEFT" else S.RIGHT; opp=side.opp
def hook(when, day, st, m, subs, cm):
    if when!="pre" or day<t0 or day>t1: return
    hqr=tt.hq_region(m,side); h0=hops(m,hqr)
    ws=[w for w in st.warriors.values() if w.side is side]
    ows=[w for w in st.warriors.values() if w.side is opp]
    hq=st.buildings.get(hqr)
    dist=collections.Counter()
    for w in ws: dist[min(h0[w.region],9)]+=1
    en=collections.Counter()
    for w in ows:
        if h0[w.region]<=6: en[h0[w.region]]+=1
    sub=subs[side]
    mv=len(sub.moves) if sub else 0; tr=sub.train_n if sub and sub.has_train else 0
    print(f"T{day} gold {st.gold[side.value]} hq_hp {hq.hp if hq else 0} W {len(ws)} byhop {dict(sorted(dist.items()))} | enemy near(hop:n) {dict(sorted(en.items()))} | cmds mv{mv} tr{tr} up{sub.upgrades if sub else []}")
replay(path,hook)
