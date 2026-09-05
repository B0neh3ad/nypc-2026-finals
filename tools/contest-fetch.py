#!/usr/bin/env python3
"""contest-fetch.py — 중간평가 매니페스트를 받아 대회 로그와 전적을 내려받는다.

  tools/contest-fetch.py <manifest.json> [--out DIR]

매니페스트는 `tools/contest-collect.js` 를 브라우저 콘솔에서 돌려 만든다.
(세션 쿠키가 HttpOnly 라 서버에서 API 를 직접 못 부르고, 로그인 정보는 공유
폴더에 두지 않는다 — docs/COLLAB.md. 프리사인드 URL 은 7일 유효하다.)

산출물 (기본 ~/shared/runs/contest/):
  round_<N>/battle_<id>_g<n>_<left|right>.log
                                   압축 해제된 대회 로그(우리 진영이 파일명에).
                                   시뮬레이터에 그대로 붙여넣기
  round_<N>/manifest.json          받은 매니페스트 원본 (제출 코드 정보 포함)
  round_<N>/submission_<id>.<ext>  그 라운드에 쓰인 대표 답안 소스
  round_<N>/results.tsv            전적 표 (상대·퍼포먼스·승패·진영)
  round_<N>.tar.gz                 그 라운드 폴더 통째 압축본
  standings.tsv                    라운드별 점수·순위 누적

표준 라이브러리만 쓴다.
"""

import argparse
import gzip
import io
import json
import os
import re
import sys
import tarfile
import urllib.request
from pathlib import Path

SHARED = Path(os.environ.get("NYPC_SHARED", "/srv/nypc"))
DEFAULT_OUT = SHARED / "runs" / "contest"
MY_TEAM = os.environ.get("NYPC_TEAM", "CPG")


def fetch(url: str) -> bytes:
    """프리사인드 URL 에서 받아온다. gzip 이면 푼다."""
    req = urllib.request.Request(url, headers={"User-Agent": "nypc-contest-fetch"})
    with urllib.request.urlopen(req, timeout=60) as r:
        raw = r.read()
        encoded = (r.headers.get("Content-Encoding") or "").lower() == "gzip"
    # urllib 은 Content-Encoding 을 자동으로 풀지 않는다. 매직 넘버로도 확인한다.
    if encoded or raw[:2] == b"\x1f\x8b":
        try:
            raw = gzip.decompress(raw)
        except OSError:
            pass  # 이미 풀린 상태면 그대로 둔다
    return raw


def guess_ext(text):
    """확장자는 API 가 안 알려준다. 내용으로 판별한다."""
    head = text[:4000]
    if "#include" in head or "std::" in head:
        return "cpp"
    if "def " in head or "import " in head or "sys.stdin" in head:
        return "py"
    if "fn main" in head or "let mut" in head:
        return "rs"
    return "txt"


def fetch_submission_code(sub_info, rdir):
    """대표 답안 소스를 내려받아 round 폴더에 둔다. 파일명을 돌려준다."""
    if not sub_info or not sub_info.get("code_url"):
        return None
    sid = sub_info.get("id", "unknown")
    try:
        raw = fetch(sub_info["code_url"])
    except Exception as e:
        print(f"  제출 코드 실패 {sid}: {e}")
        return None
    text = raw.decode("utf-8", errors="replace")
    name = f"submission_{sid}.{guess_ext(text)}"
    out = rdir / name
    if out.exists() and out.stat().st_size == len(raw):
        print(f"  SKIP  제출코드 {name}")
        return name
    out.write_bytes(raw)
    print(f"  OK    제출코드 {name}  {len(raw):,} bytes")
    return name


def parse_pgn(pgn):
    """[P1 "Team A"] [P2 "Team B"] [Result "1-0"] -> (p1, p2, result)"""
    if not pgn:
        return None, None, None
    g = dict(re.findall(r'\[(\w+)\s+"([^"]*)"\]', pgn))
    return g.get("P1"), g.get("P2"), g.get("Result")


def my_outcome(p1, p2, result):
    """내 팀 기준 승/무/패와 진영을 돌려준다."""
    if not result:
        return "?", "?"
    side = "LEFT" if p1 and MY_TEAM in p1 else ("RIGHT" if p2 and MY_TEAM in p2 else "?")
    if result == "1/2-1/2":
        return "DRAW", side
    if result == "1-0":
        return ("WIN" if side == "LEFT" else "LOSS"), side
    if result == "0-1":
        return ("WIN" if side == "RIGHT" else "LOSS"), side
    return result, side


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("manifest", type=Path)
    ap.add_argument("--out", type=Path, default=DEFAULT_OUT)
    ap.add_argument("--archive", action="store_true",
                    help="새로 받은 게 없어도 tar.gz 를 다시 만든다")
    ap.add_argument("--no-archive", action="store_true",
                    help="압축하지 않는다")
    args = ap.parse_args()

    m = json.loads(args.manifest.read_text(encoding="utf-8"))
    rnd = m.get("round", "x")
    rdir = args.out / f"round_{rnd}"
    rdir.mkdir(parents=True, exist_ok=True)
    (rdir / "manifest.json").write_text(
        json.dumps(m, ensure_ascii=False, indent=2), encoding="utf-8")

    print(f"라운드 #{rnd} — {m.get('heading','')}")
    print(f"받는 곳: {rdir}\n")

    rows, ok, skip, err = [], 0, 0, 0
    w = d = l = 0

    for b in m.get("battles", []):
        bid = b["battle_id"]
        for lg in b.get("logs", []) or []:
            p1, p2, res = parse_pgn(lg.get("pgn"))
            outcome, side = my_outcome(p1, p2, res)
            # 파일명에 우리 진영을 박아둔다. 중간평가 성적이 진영에 따라
            # 갈리는지 보려면 파일 목록만 훑어도 되게.
            side_sfx = {"LEFT": "left", "RIGHT": "right"}.get(side, "x")
            out = rdir / f"battle_{bid}_g{lg['game']}_{side_sfx}.log"

            if out.exists() and out.stat().st_size > 0:
                print(f"  SKIP  {bid} g{lg['game']}")
                skip += 1
            else:
                try:
                    data = fetch(lg["url"])
                    out.write_bytes(data)
                    print(f"  OK    {bid} g{lg['game']}  {len(data):>9,} bytes  "
                          f"{outcome:<5} vs {b['opponent']}")
                    ok += 1
                except Exception as e:
                    print(f"  ERR   {bid} g{lg['game']}: {e}")
                    err += 1
                    continue

            rows.append([bid, str(lg["game"]), b.get("opponent", ""),
                         b.get("opp_perf", ""), outcome, side,
                         res or "", out.name])
            if outcome == "WIN":
                w += 1
            elif outcome == "DRAW":
                d += 1
            elif outcome == "LOSS":
                l += 1

    # 전적 표
    tsv = rdir / "results.tsv"
    with tsv.open("w", encoding="utf-8") as f:
        f.write("battle_id\tgame\topponent\topp_perf\toutcome\tside\tpgn_result\tlog\n")
        for r in rows:
            f.write("\t".join(r) + "\n")

    # 라운드별 점수·순위 누적
    stand = args.out / "standings.tsv"
    if not stand.exists():
        stand.write_text("round\theading\tscore_rank\twin\tdraw\tloss\tfetched_at\n",
                         encoding="utf-8")
    score_rank = ""
    for row in m.get("rounds_overview", []):
        if row and m.get("heading", "").split("·")[-1].strip() in " ".join(row):
            score_rank = row[1] if len(row) > 1 else ""
            break
    existing = stand.read_text(encoding="utf-8")
    line = (f"{rnd}\t{m.get('heading','')}\t{score_rank}\t{w}\t{d}\t{l}\t"
            f"{m.get('fetched_at','')}\n")
    # 같은 라운드 행이 있으면 갱신한다. 집계 중에 받은 부분 데이터가
    # 나중에 완전한 값으로 덮여야 한다.
    lines = existing.splitlines(True)
    hit = [i for i, ln in enumerate(lines) if ln.split("\t")[0] == str(rnd)]
    if hit:
        lines[hit[0]] = line
        stand.write_text("".join(lines), encoding="utf-8")
    else:
        with stand.open("a", encoding="utf-8") as f:
            f.write(line)

    # 그 라운드에 실제로 제출된 코드도 함께 보관한다.
    sub_info = m.get("submission") or {}
    code_name = fetch_submission_code(sub_info, rdir)
    if code_name:
        sub_info["file"] = code_name
        m["submission"] = sub_info
        ok += 1   # 새 파일이 생겼으니 압축을 다시 만든다
        (rdir / "manifest.json").write_text(
            json.dumps(m, ensure_ascii=False, indent=2), encoding="utf-8")

    # 압축: 새로 받은 게 있을 때만 다시 만든다.
    arc = None
    if not args.no_archive and (ok > 0 or args.archive):
        arc = args.out / f"round_{rnd}.tar.gz"
        tmp = arc.with_suffix(".tar.gz.part")
        with tarfile.open(tmp, "w:gz") as tf:
            tf.add(rdir, arcname=rdir.name)
        tmp.replace(arc)   # 중간에 죽어도 깨진 압축본이 남지 않게

    print(f"\n다운로드: {ok} 신규 / {skip} 이미있음 / {err} 실패")
    print(f"전적: {w}승 {d}무 {l}패")
    if sub_info.get("id"):
        print(f"제출: #{sub_info['id']}  {sub_info.get('submitted_at','')}  "
              f"{sub_info.get('result','')}  -> {sub_info.get('file','(코드 없음)')}")
    print(f"  {tsv}")
    print(f"  {stand}")
    if arc:
        print(f"  {arc}  ({arc.stat().st_size:,} bytes)")
    if err:
        sys.exit(1)


if __name__ == "__main__":
    main()
