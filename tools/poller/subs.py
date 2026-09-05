#!/usr/bin/env python3
"""제출 내역의 '모든 제출 목록' 에서, 각 제출의 샘플 봇 5종 대결 로그를 받는다.

    ~/nypc-2026/nypc-contest-poller/run_subs.sh

라운드 로그 폴러(poller.py)와 같은 구조다 — 로그인·아레나 진입 코드를 그대로 쓴다.
차이는 '중간 평가' 탭이 아니라 '제출 내역' 탭을 읽는다는 것뿐이다.

이미 서버에 로그가 받아진 제출은 **API 호출조차 하지 않고** 건너뛴다.
제출이 수십 건이고 각각 5판이라, 매번 전부 조회하면 낭비다.
"""
import json
import os
import re
import subprocess
import sys
import time
from datetime import datetime, timezone
from pathlib import Path

from playwright.sync_api import sync_playwright

sys.path.insert(0, str(Path(__file__).resolve().parent))
from poller import (PROBLEM_URL, SSH_HOST, close_dialogs, enter_arena,  # noqa: E402
                    load_env, log, login, ssh)

SHARED_REMOTE = os.environ.get("NYPC_SHARED_REMOTE", "/srv/nypc")
OUT_REMOTE = f"{SHARED_REMOTE}/runs/submissions"
POLL_SEC = int(os.environ.get("SUBS_POLL_SEC", "0"))  # 0 이면 1회만


def already_done():
    """서버에 로그가 하나라도 있는 제출 id 집합."""
    r = ssh(f"ls -d {OUT_REMOTE}/sub_*/ 2>/dev/null")
    out = set()
    for p in r.stdout.split():
        m = re.search(r"sub_(\d+)/?$", p.strip())
        if m:
            sid = m.group(1)
            n = ssh(f"ls {OUT_REMOTE}/sub_{sid}/*.log 2>/dev/null | wc -l; "
                    f"test -f {OUT_REMOTE}/sub_{sid}/.no-logs && echo NOLOG")
            txt = n.stdout.strip().split()
            try:
                if (txt and int(txt[0]) >= 5) or "NOLOG" in txt:
                    out.add(sid)
            except ValueError:
                pass
    return out


def open_submissions_tab(page):
    close_dialogs(page)
    page.goto(PROBLEM_URL)
    page.wait_for_load_state("domcontentloaded")
    page.wait_for_timeout(1500)
    tab = page.get_by_role("tab", name="제출 내역")
    try:
        tab.wait_for(state="visible", timeout=8_000)
    except Exception:
        enter_arena(page)
        page.goto(PROBLEM_URL)
        page.wait_for_load_state("domcontentloaded")
        page.wait_for_timeout(1500)
        tab = page.get_by_role("tab", name="제출 내역")
        tab.wait_for(state="visible", timeout=30_000)
    tab.click()
    page.wait_for_timeout(1500)


def scrape(page, skip_ids):
    """제출 목록 + 각 제출의 샘플 봇 대결 프리사인드 URL."""
    return page.evaluate(
        """async (skip) => {
      const skipSet = new Set(skip);
      // '모든 제출 목록' = '#/제출 시각/...' 표 중 행이 가장 많은 것.
      // 페이지에는 같은 머리글의 표가 여럿 있다('제출' 탭의 대표답안 등).
      let t = null;
      for (const x of document.querySelectorAll('table')) {
        const h = [...x.querySelectorAll('th')].map(y => y.innerText.trim());
        if (h[0] === '#' && h.some(y => y.startsWith('제출 시각'))) {
          if (!t || x.querySelectorAll('tbody tr').length > t.querySelectorAll('tbody tr').length)
            t = x;
        }
      }
      if (!t) return { error: 'no table' };

      const subs = [];
      for (const r of t.querySelectorAll('tbody tr')) {
        const c = [...r.querySelectorAll('td')].map(x => x.innerText.trim());
        if (!c.length || !/^\\d+$/.test(c[0])) continue;   // 안내문 행은 건너뜀
        subs.push({ id: c[0], submitted_at: c[1],
                    result: (c[2] || '').replace(/\\n/g, ' '),
                    note: c[3] || '' });
      }

      for (const s of subs) {
        if (skipSet.has(s.id)) { s.skipped = true; continue; }
        s.logs = [];
        for (let g = 1; g <= 10; g++) {
          const res = await fetch(`/api/v2/submission/${s.id}/log/${g}`,
                                  { credentials: 'include' });
          if (!res.ok) break;
          const j = await res.json();
          s.logs.push({ game: g, url: j.url, pgn: j.pgn || null });
        }
      }
      return { subs };
    }""",
        sorted(skip_ids),
    )


def one_pass(page):
    done = already_done()
    log(f"이미 받은 제출: {len(done)}건")

    open_submissions_tab(page)
    data = scrape(page, done)

    if data.get("error"):
        log("제출 목록 표를 못 찾았습니다: " + data["error"])
        return

    subs = data["subs"]
    todo = [s for s in subs if not s.get("skipped")]
    log(f"목록 {len(subs)}건 중 새로 받을 것 {len(todo)}건")
    if not todo:
        return

    manifest = {"fetched_at": datetime.now(timezone.utc).isoformat(), "submissions": todo}
    tmp = Path("/tmp/nypc-subs.json")
    tmp.write_text(json.dumps(manifest, ensure_ascii=False, indent=2), encoding="utf-8")
    subprocess.run(["scp", "-q", "-o", "BatchMode=yes", str(tmp),
                    f"{SSH_HOST}:/tmp/"], check=True)
    r = ssh(f"{SHARED_REMOTE}/tools/subs-fetch.py /tmp/nypc-subs.json")
    print(r.stdout, flush=True)
    if r.returncode:
        print(r.stderr, file=sys.stderr, flush=True)


def main():
    load_env()
    with sync_playwright() as p:
        browser = p.chromium.launch(headless=True)
        page = browser.new_page()
        login(page)
        if not POLL_SEC:
            one_pass(page)
            browser.close()
            return
        log(f"폴링 시작 — {POLL_SEC}초 간격")
        while True:
            try:
                one_pass(page)
            except Exception as e:
                log(f"오류: {type(e).__name__}: {e}")
                try:
                    login(page)          # 세션 만료 등
                except Exception as e2:
                    log(f"재로그인 실패: {e2}")
            time.sleep(POLL_SEC)


if __name__ == "__main__":
    main()
