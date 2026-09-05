#!/usr/bin/env python3
"""이미 받은 라운드에 제출 코드를 뒤늦게 채워 넣는다.

  ./run.sh backfill            # 전부
  BACKFILL_ROUNDS=1,3 ./run.sh backfill

poller.py 의 로그인·네비게이션을 그대로 쓴다. 라운드 상세에 들어가 대표 답안
정보와 코드 URL 만 뽑고, 기존 manifest 에 합쳐 서버의 contest-fetch.py 를
다시 돌린다. 배틀 로그는 이미 있으니 SKIP 되고 코드만 새로 받는다.
"""
import json
import os
import subprocess
import sys
from pathlib import Path

from playwright.sync_api import sync_playwright

sys.path.insert(0, str(Path(__file__).resolve().parent))
from poller import (SSH_HOST, load_env, log, login, open_eval_tab,  # noqa: E402
                    list_rounds, scrape_round, ssh)


def main():
    load_env()
    want = os.environ.get("BACKFILL_ROUNDS", "")
    want = {int(x) for x in want.split(",") if x.strip()} if want else None

    r = ssh("ls -d ~/shared/runs/contest/round_*/ 2>/dev/null")
    have = []
    for line in r.stdout.split():
        try:
            have.append(int(line.rstrip("/").rsplit("_", 1)[1]))
        except Exception:
            pass
    have.sort()
    log(f"서버에 있는 라운드: {have}")

    with sync_playwright() as p:
        browser = p.chromium.launch(headless=True)
        page = browser.new_page()
        login(page)
        open_eval_tab(page)
        rounds = list_rounds(page)

        for r_ in rounds:
            try:
                data = scrape_round(page, r_["when"])
            except Exception as e:
                log(f"  {r_['when']} 건너뜀: {e}")
                open_eval_tab(page)
                continue
            n = data["round"]
            if n not in have or (want and n not in want):
                open_eval_tab(page)
                continue
            sub = data.get("submission")
            if not sub or not sub.get("code_url"):
                log(f"  라운드 #{n}: 제출 정보 없음")
                open_eval_tab(page)
                continue

            # 서버의 기존 manifest 를 읽어 submission 만 합친다
            got = ssh(f"cat ~/shared/runs/contest/round_{n}/manifest.json")
            if got.returncode:
                log(f"  라운드 #{n}: manifest 없음")
                open_eval_tab(page)
                continue
            m = json.loads(got.stdout)
            m["submission"] = sub
            tmp = Path(f"/tmp/nypc-round-{n}.json")
            tmp.write_text(json.dumps(m, ensure_ascii=False, indent=2), encoding="utf-8")
            subprocess.run(["scp", "-q", "-o", "BatchMode=yes", str(tmp),
                            f"{SSH_HOST}:/tmp/"], check=True)
            log(f"라운드 #{n}: 제출 #{sub['id']} — 코드 받는 중")
            out = ssh(f"~/shared/tools/contest-fetch.py /tmp/nypc-round-{n}.json")
            print(out.stdout.strip().splitlines()[-4:] and
                  "\n".join(out.stdout.strip().splitlines()[-4:]), flush=True)
            open_eval_tab(page)

        browser.close()


if __name__ == "__main__":
    main()
