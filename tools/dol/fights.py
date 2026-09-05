import sys, collections
from replay import replay, tt
S=tt.Side
path=sys.argv[1]; t0=int(sys.argv[2]); t1=int(sys.argv[3])
def hook(when, day, st, m, subs, cm):
    if when!="post" or day<t0 or day>t1: return
    # zones with both sides or where damage happened this turn
    zones=collections.defaultdict(lambda:[0,0,0,0])
    for w in st.warriors.values():
        z=zones[w.region]; z[w.side.value]+=1; z[2+w.side.value]+=w.hp
    dmg=collections.Counter()
    for ln in cm["res"]:
        t=ln.split()
        if t[0]=="DAMAGE": dmg[t[2][0]]+=int(t[3])
    hot=[(r,v) for r,v in zones.items() if v[0] and v[1]]
    bl={r:(b.side.letter,b.kind.value[0],b.level,b.hp) for r,b in st.buildings.items()}
    print(f"T{day} dmgA {dmg['A']} dmgB {dmg['B']} W A{sum(1 for w in st.warriors.values() if w.side is S.LEFT)} B{sum(1 for w in st.warriors.values() if w.side is S.RIGHT)} contested: " + " ".join(f"z{r}[A{v[0]}/{v[2]}hp B{v[1]}/{v[3]}hp {bl.get(r,'')}]" for r,v in hot))
replay(path,hook)
