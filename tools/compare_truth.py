import sys, re
side = sys.argv[2]  # LEFT or RIGHT
truth = {}; bot = {}; intel = {}
for line in open(sys.argv[1]):
    m = re.match(r"TRUTH (\w+) t=(\d+) (.*)", line.strip())
    if m and m.group(1) == side: truth[int(m.group(2))] = m.group(3)
    m = re.match(r"# Debug (\w+): BOT t=(\d+) (.*)", line.strip())
    if m and m.group(1) == side: bot[int(m.group(2))] = m.group(3)
    m = re.match(r"# Debug (\w+): INTEL t=(\d+) (.*)", line.strip())
    if m and m.group(1) == side: intel[int(m.group(2))] = m.group(3)
def parse(s):
    d = dict(re.findall(r"(\w+)=(\S*)", s)); return d
bad = 0; checked = 0
for t in sorted(truth):
    if t not in bot: continue
    T, B = parse(truth[t]), parse(bot[t]); checked += 1
    errs = []
    norm = lambda v: ",".join(sorted(v.split(","))) if v else ""
    for k in ("gold", "vis", "W", "B", "EW"):
        if norm(T.get(k, "")) != norm(B.get(k, "")): errs.append(f"{k}: truth={T.get(k)} bot={B.get(k)}")
    # enemy buildings: bot entries flagged 'v' (visible) must equal truth EB exactly; 'm' (memory) entries must be in non-visible zones
    bot_vis = ",".join(e.rsplit(":",1)[0] for e in B.get("EB","").split(",") if e.endswith(":v"))
    if norm(bot_vis) != norm(T.get("EB","")): errs.append(f"EB(visible): truth={T.get('EB')} bot={bot_vis}")
    if errs:
        bad += 1
        if bad <= 8: print(f"t={t}: " + " | ".join(errs))
print(f"{side}: checked {checked} turns, mismatching turns: {bad}")
errs=[]; hqerr=0; n=0
for t in sorted(truth):
    if t not in intel: continue
    T=parse(truth[t]); I=parse(intel[t]); n+=1
    errs.append(int(I["est_alive"])-int(T["EA"]))
    if int(I["est_hq_lvl"]) > int(T["EHQ"]): hqerr+=1
if n:
    import statistics
    print(f"{side}: army estimate error (est-true) over {n} turns: mean={statistics.mean(errs):+.2f} min={min(errs)} max={max(errs)} |err|<=1: {sum(1 for e in errs if abs(e)<=1)/n:.0%}   HQ-level over-estimates: {hqerr}")
    print("   sample (t:true/est):", " ".join(f"{t}:{parse(truth[t])['EA']}/{parse(intel[t])['est_alive']}" for t in sorted(truth) if t in intel and t%40==0))
