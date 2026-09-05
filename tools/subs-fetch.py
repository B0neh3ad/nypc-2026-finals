#!/usr/bin/env python3
"""subs-fetch.py — 제출별 '샘플 봇과의 대결' 로그를 내려받는다.

  tools/subs-fetch.py <manifest.json> [--out DIR]

매니페스트는 nypc-contest-poller/subs.py 가 만든다.
(라운드 로그와 같은 이유로 2단계다 — 세션 쿠키가 HttpOnly 라 서버에서 API 를
직접 못 부르고, 로그인 정보는 공유 폴더에 두지 않는다.)

산출물 (기본 ~/shared/runs/submissions/):
  sub_<id>/manifest.json                       그 제출의 메타
  sub_<id>/g<N>_sample<N>_<side>_<결과>.log     샘플 봇 5종과의 대결 로그
  index.tsv                                    제출별 요약 누적

이미 로그가 다 받아진 제출은 건너뛴다.
"""
import argparse
import gzip
import io
import json
import os
import re
import sys
import urllib.request
from pathlib import Path

SHARED = Path(os.environ.get("NYPC_SHARED", "/srv/nypc"))
DEFAULT_OUT = SHARED / "runs" / "submissions"
MY_TEAM = os.environ.get("NYPC_TEAM", "CPG")


def fetch(url):
    req = urllib.request.Request(url, headers={"User-Agent": "nypc-subs-fetch"})
    with urllib.request.urlopen(req, timeout=60) as r:
        raw = r.read()
        gz = (r.headers.get("Content-Encoding") or "").lower() == "gzip"
    if gz or raw[:2] == b"\x1f\x8b":
        try:
            raw = gzip.decompress(raw)
        except OSError:
            pass
    return raw


def parse_pgn(pgn):
    if not pgn:
        return None, None, None
    g = dict(re.findall(r'\[(\w+)\s+"([^"]*)"\]', pgn))
    return g.get("P1"), g.get("P2"), g.get("Result")


def outcome_of(p1, p2, res):
    if not res:
        return "?", "?"
    side = "left" if (p1 and MY_TEAM in p1) else ("right" if (p2 and MY_TEAM in p2) else "x")
    if res == "1/2-1/2":
        return "DRAW", side
    if res == "1-0":
        return ("WIN" if side == "left" else "LOSS"), side
    if res == "0-1":
        return ("WIN" if side == "right" else "LOSS"), side
    return res, side


def opp_name(p1, p2):
    other = p2 if (p1 and MY_TEAM in p1) else p1
    m = re.search(r"#?(\d+)", other or "")
    return ("sample%s" % m.group(1)) if m else re.sub(r"\W+", "_", other or "opp")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("manifest", type=Path)
    ap.add_argument("--out", type=Path, default=DEFAULT_OUT)
    ap.add_argument("--force", action="store_true", help="이미 받은 제출도 다시 받는다")
    args = ap.parse_args()

    m = json.loads(args.manifest.read_text(encoding="utf-8"))
    args.out.mkdir(parents=True, exist_ok=True)

    index = args.out / "index.tsv"
    if not index.exists():
        index.write_text("id\tsubmitted_at\tresult\tnote\tW\tD\tL\tlogs\n", encoding="utf-8")
    seen = {ln.split("\t")[0] for ln in index.read_text(encoding="utf-8").splitlines()[1:]}

    tot_new = tot_skip = tot_err = 0
    for s in m.get("submissions", []):
        sid = str(s["id"])
        sdir = args.out / f"sub_{sid}"
        logs = s.get("logs") or []
        if not args.force and sdir.is_dir() and len(list(sdir.glob("*.log"))) >= len(logs) > 0:
            print(f"SKIP  #{sid}  (이미 {len(list(sdir.glob('*.log')))}개)")
            tot_skip += 1
            continue

        sdir.mkdir(parents=True, exist_ok=True)
        w = d = l = 0
        names = []
        for lg in logs:
            p1, p2, res = parse_pgn(lg.get("pgn"))
            oc, side = outcome_of(p1, p2, res)
            name = f"g{lg['game']}_{opp_name(p1, p2)}_{side}_{oc}.log"
            out = sdir / name
            if out.exists() and out.stat().st_size > 0:
                names.append(name)
            else:
                try:
                    out.write_bytes(fetch(lg["url"]))
                    names.append(name)
                except Exception as e:
                    print(f"  ERR #{sid} g{lg['game']}: {e}")
                    tot_err += 1
                    continue
            if oc == "WIN":
                w += 1
            elif oc == "DRAW":
                d += 1
            elif oc == "LOSS":
                l += 1

        (sdir / "manifest.json").write_text(
            json.dumps(s, ensure_ascii=False, indent=2), encoding="utf-8")
        # 컴파일 실패 등으로 로그가 없는 제출. 표시해 두지 않으면 폴러가
        # 매 주기 이걸 다시 조회한다.
        # 단, 아직 채점 중인 제출은 표시하지 않는다. 표시하면 채점이 끝나도
        # 영구히 건너뛴다. 컴파일 실패처럼 **끝난** 상태만 표시한다.
        res = (s.get("result") or "")
        pending = any(k in res for k in ("대기", "채점", "진행"))
        if not logs and not pending:
            (sdir / ".no-logs").write_text("", encoding="utf-8")
        print(f"OK    #{sid}  {len(names)}개  {w}승 {d}무 {l}패  {s.get('result','')}")
        tot_new += 1
        if sid not in seen:
            with index.open("a", encoding="utf-8") as f:
                f.write("\t".join([sid, s.get("submitted_at", ""),
                                   (s.get("result", "") or "").replace("\t", " "),
                                   s.get("note", ""), str(w), str(d), str(l),
                                   str(len(names))]) + "\n")

    print(f"\n새로 받음 {tot_new} / 건너뜀 {tot_skip} / 실패 {tot_err}")
    print(f"  {args.out}")
    print(f"  {index}")
    if tot_err:
        sys.exit(1)


if __name__ == "__main__":
    main()
