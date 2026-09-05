#!/usr/bin/env python3
"""라운드 결과 요약 — results.tsv + 로그의 종료 사유/마지막 턴을 붙인다."""
import csv, io, re, sys, os

d = sys.argv[1] if len(sys.argv) > 1 else "."
os.chdir(d)
rows = list(csv.DictReader(io.open("results.tsv", encoding="utf-8"), delimiter="\t"))
out = []
for r in rows:
    txt = io.open(r["log"], encoding="utf-8", errors="replace").read()
    res = re.search(r"^RESULT (\S+) (\S+)", txt, re.M)
    turns = re.findall(r"^TURN (\d+)$", txt, re.M)
    out.append((r["opponent"], int(r["opp_perf"] or 0), r["outcome"], r["side"],
                res.group(2) if res else "?", int(turns[-1]) if turns else 0))

order = {"WIN": 0, "DRAW": 1, "LOSS": 2}
out.sort(key=lambda x: (order.get(x[2], 3), x[5]))

print("%-36s %5s  %-5s %-6s %-14s %4s" % ("상대", "perf", "결과", "진영", "사유", "턴"))
print("-" * 78)
for o in out:
    print("%-36s %5d  %-5s %-6s %-14s %4d" % o)

print()
lost_early = [o for o in out if o[2] == "LOSS" and o[4] == "HQ_DESTROYED"]
print("본부 파괴로 패배: %d판, 최단 %d턴" % (
    len(lost_early), min([o[5] for o in lost_early]) if lost_early else 0))
print("400턴 완주: %d판" % len([o for o in out if o[5] == 400]))
