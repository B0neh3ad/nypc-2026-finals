#!/usr/bin/env python3
"""중간평가 폴러 — 새 라운드가 집계되면 자동으로 로그·전적을 받아 서버에 넣는다.

Claude 세션 상태와 무관하게 백그라운드에서 계속 돈다.

실행:
    ~/nypc-2026/nypc-contest-poller/run.sh

자격증명은 **각자 홈**의 ~/.nypc.env 에서 읽는다 (공유 폴더에 두지 않는다):
    NYPC_ID=아이디
    NYPC_PW=비밀번호

동작:
  1. 로그인 (헤드리스)
  2. /problems/1 → '중간 평가' 탭 → 완료 라운드 목록
  3. 서버 standings.tsv 에 없는 라운드가 있으면 상세 진입
  4. 대전 ID → /api/v2/battle/{id}/log/{n} 로 프리사인드 URL 획득
  5. 매니페스트를 서버로 보내고 contest-fetch.py 실행
  6. POLL_SEC 만큼 자고 반복
"""

import json
import os
import re
import subprocess
import sys
import time
from datetime import datetime
from pathlib import Path

from playwright.sync_api import sync_playwright

SSH_HOST = os.environ.get("NYPC_SSH", "nypc-jinsu")
POLL_SEC = int(os.environ.get("POLL_SEC", "180"))
TALLY_RETRY = int(os.environ.get("TALLY_RETRY", "3"))     # 집계 중 재시도 횟수
TALLY_WAIT = int(os.environ.get("TALLY_WAIT", "60"))      # 재시도 간격(초)
PROBLEM_URL = "https://contest.nypc.co.kr/problems/1"
LOGIN_URL = "https://new.nypc.co.kr/ko/login"
HOME_URL = "https://new.nypc.co.kr/ko/"
STATE = Path.home() / ".nypc-poller-state.json"


def log(msg):
    print(f"[{datetime.now():%H:%M:%S}] {msg}", flush=True)


def load_env():
    """자격증명은 각자 홈의 파일에서만 읽는다. 공유 폴더에는 두지 않는다."""
    for env in (Path.home() / ".nypc.env",
                Path.home() / "nypc-2026" / "예선" / ".env"):
        if not env.exists():
            continue
        for line in env.read_text(encoding="utf-8").splitlines():
            line = line.strip()
            if line and not line.startswith("#") and "=" in line:
                k, v = line.split("=", 1)
                os.environ.setdefault(k.strip(), v.strip().strip("'\""))
        if os.environ.get("NYPC_ID") and os.environ.get("NYPC_PW"):
            log(f"자격증명: {env}")
            return
    log("경고: NYPC_ID/NYPC_PW 를 찾지 못했습니다 (~/.nypc.env)")


def ssh(cmd):
    return subprocess.run(["ssh", "-o", "BatchMode=yes", SSH_HOST, cmd],
                          capture_output=True, text=True)


def done_rounds():
    """서버에 이미 받은 (라운드번호 집합, 라운드시각 헤딩 문자열 목록).

    시각으로 대조하는 게 핵심이다. 라운드 번호는 상세에 들어가야만 알 수 있어서,
    번호로 판단하려면 이미 받은 라운드까지 매번 열어봐야 한다.
    """
    r = ssh("cat ~/shared/runs/contest/standings.tsv 2>/dev/null")
    nums, headings = set(), []
    for line in r.stdout.splitlines()[1:]:
        f = line.split("\t")
        if f and f[0].isdigit():
            # 순위 칸이 '집계 중'이면 그때 받은 건 부분 데이터다.
            # 받음으로 치지 않아야 집계 완료 후 나머지 판을 채운다.
            tallying = len(f) > 2 and "집계 중" in f[2]
            if tallying:
                continue
            nums.add(int(f[0]))
            if len(f) > 1:
                headings.append(f[1])
    return nums, headings


# ── 브라우저 ────────────────────────────────────────────────────────────

def login(page):
    page.goto(LOGIN_URL)
    page.wait_for_load_state("domcontentloaded")
    page.wait_for_timeout(2000)
    if "login" in page.url:
        uid, pw = os.environ.get("NYPC_ID"), os.environ.get("NYPC_PW")
        if not (uid and pw):
            raise SystemExit("~/.nypc.env 에 NYPC_ID / NYPC_PW 가 필요합니다")
        page.locator("input[placeholder*='아이디']").fill(uid)
        page.locator("input[type='password']").fill(pw)
        page.get_by_role("button", name="로그인").click()
        page.locator("input[type='password']").wait_for(state="hidden", timeout=120_000)
        page.wait_for_url(lambda u: "login" not in u, timeout=30_000)
    log("로그인 상태 확인")


def enter_arena(page):
    """new.nypc.co.kr 에서 '아레나 바로가기' 를 눌러야 contest 쪽 SSO 가 발동한다.
    이걸 건너뛰고 contest URL 로 직행하면 홈으로 튕긴다."""
    page.goto(HOME_URL)
    page.wait_for_load_state("domcontentloaded")
    page.wait_for_timeout(1500)
    btn = page.get_by_role("button", name="NYPC 아레나 바로가기")
    btn.wait_for(state="visible", timeout=30_000)
    btn.click()
    page.wait_for_url(lambda u: "contest.nypc.co.kr" in u, timeout=30_000)
    page.wait_for_timeout(1500)


def close_dialogs(page):
    """열린 모달을 닫는다. radix 다이얼로그가 떠 있으면 그 아래 클릭이 전부 막힌다."""
    for _ in range(3):
        if not page.locator("[role=dialog][data-state=open]").count():
            return
        page.keyboard.press("Escape")
        page.wait_for_timeout(400)


def open_eval_tab(page):
    close_dialogs(page)
    page.goto(PROBLEM_URL)
    page.wait_for_load_state("domcontentloaded")
    page.wait_for_timeout(1500)
    tab = page.get_by_role("tab", name="중간 평가")
    try:
        tab.wait_for(state="visible", timeout=8_000)
    except Exception:
        enter_arena(page)
        page.goto(PROBLEM_URL)
        page.wait_for_load_state("domcontentloaded")
        page.wait_for_timeout(1500)
        tab = page.get_by_role("tab", name="중간 평가")
        tab.wait_for(state="visible", timeout=30_000)
    tab.click()
    page.wait_for_timeout(1200)
    see_all = page.locator("text=모든 라운드 보기")
    if see_all.count() and see_all.first.is_visible():
        see_all.first.click()
        page.wait_for_timeout(1000)


def list_rounds(page):
    """라운드 목록. tallying=True 면 아직 집계 중이라 판이 일부만 보인다."""
    return page.evaluate("""() => {
      const t = [...document.querySelectorAll('table')].find(x =>
        [...x.querySelectorAll('th')].some(h => h.innerText.trim().startsWith('라운드 시각')));
      if (!t) return [];
      return [...t.querySelectorAll('tbody tr')].map(r => {
        const c = [...r.querySelectorAll('td')].map(x => x.innerText.trim());
        return { when: c[0] || '',
                 result: (c[1] || '').replace(/\\n/g, ' '),
                 tallying: (c[1] || '').includes('집계 중') };
      }).filter(r => r.when && r.result && !r.result.includes('예정'));
    }""")


def scrape_round(page, when):
    """라운드 시각으로 행을 찾아 클릭한다.

    인덱스로 찾으면 안 된다 — 집계 중인 행이 목록 맨 위에 있다가 사라지면서
    번호가 밀리고, 엉뚱한 라운드에 들어가거나 아직 집계 중인 행을 눌러 멈춘다.
    시각 문자열은 라운드마다 유일하므로 그걸 키로 쓴다.
    """
    m = re.search(r"\d{1,2}:\d{2}:\d{2}", when)
    if not m:
        raise RuntimeError(f"시각을 못 읽음: {when}")
    hhmmss = m.group(0)

    # 아직 집계 중이면 들어가지 않는다.
    pending = page.evaluate("""(when) => {
      const t = [...document.querySelectorAll('table')].find(x =>
        [...x.querySelectorAll('th')].some(h => h.innerText.trim().startsWith('라운드 시각')));
      if (!t) return true;
      for (const r of t.querySelectorAll('tbody tr')) {
        const c = [...r.querySelectorAll('td')];
        if (c.length && c[0].innerText.trim() === when)
          return r.innerText.includes('예정') || r.innerText.includes('집계 중');
      }
      return true;
    }""", when)
    if pending:
        raise RuntimeError(f"행을 못 찾았거나 아직 집계 중: {when}")

    # JS 의 .click() 은 React 핸들러를 못 깨우므로 Playwright 의 실제 클릭을 쓴다.
    # 단, 페이지 전체에서 시각 텍스트를 찾으면 열려 있는 모달 안의 것을 집어
    # 클릭이 가로막힌다. 반드시 라운드 테이블 안으로 범위를 좁힌다.
    close_dialogs(page)
    table = page.locator("table").filter(
        has=page.get_by_role("columnheader", name="라운드 시각"))
    table.locator("tbody tr").filter(has_text=hhmmss).first.click(timeout=15_000)
    page.get_by_role("columnheader", name="대전 ID").wait_for(state="visible", timeout=30_000)
    page.wait_for_timeout(800)

    data = page.evaluate("""async () => {
      const findT = h => [...document.querySelectorAll('table')].find(t =>
        [...t.querySelectorAll('th')].some(x => x.innerText.trim().startsWith(h)));
      const heading = [...document.querySelectorAll('h1,h2,h3,h4')]
        .map(h => h.innerText.trim()).find(t => t.includes('중간평가')) || '';
      const bt = findT('대전 ID');
      const battles = [...bt.querySelectorAll('tbody tr')].map(r => {
        const c = [...r.querySelectorAll('td')].map(x => x.innerText.trim());
        return { battle_id: c[0], opponent: c[1], opp_perf: c[2], my_record: c[3] };
      }).filter(b => /^\\d+$/.test(b.battle_id));
      for (const b of battles) {
        b.logs = [];
        for (let g = 1; g <= 10; g++) {
          const res = await fetch(`/api/v2/battle/${b.battle_id}/log/${g}`, {credentials:'include'});
          if (!res.ok) break;
          const j = await res.json();
          b.logs.push({ game: g, url: j.url, pgn: j.pgn || null });
        }
      }
      // 그 라운드의 대표 답안.
      // 주의: 페이지에는 '#/제출 시각/...' 표가 여럿이다 — '제출' 탭의 현재
      // 대표답안, '제출 내역'의 전체 목록, 그리고 라운드 상세의 것. 앞에서부터
      // 찾으면 항상 최신 제출을 집는다. 라운드 상세의 표는 '대전 ID' 표 바로
      // 앞에 있으므로, 그 기준으로 거꾸로 찾는다.
      let submission = null;
      const all = [...document.querySelectorAll('table')];
      const bIdx = all.indexOf(bt);
      let st = null;
      for (let k = bIdx - 1; k >= 0; k--) {
        const hs = [...all[k].querySelectorAll('th')].map(x => x.innerText.trim());
        if (hs[0] === '#' && hs.some(h => h.startsWith('제출 시각'))) { st = all[k]; break; }
      }
      if (st) {
        for (const r of st.querySelectorAll('tbody tr')) {
          const c = [...r.querySelectorAll('td')].map(x => x.innerText.trim());
          if (c.length >= 3 && /^\\d+$/.test(c[0])) {
            submission = { id: c[0], submitted_at: c[1],
                           result: (c[2] || '').replace(/\\n/g, ' '),
                           note: c[3] || '' };
            break;
          }
        }
      }
      if (submission) {
        const res = await fetch(`/api/v2/submission/${submission.id}/code`,
                                {credentials:'include'});
        if (res.ok) submission.code_url = (await res.json()).url;
      }

      const rt = findT('라운드 시각');
      return { heading, battles, submission,
        rounds_overview: rt ? [...rt.querySelectorAll('tbody tr')].map(r =>
          [...r.querySelectorAll('td')].map(x => x.innerText.trim())) : [] };
    }""")

    m = re.search(r"#(\d+)", data["heading"])
    data["round"] = int(m.group(1)) if m else 0
    data["fetched_at"] = datetime.utcnow().isoformat() + "Z"

    back = page.locator("text=돌아가기")
    if back.count():
        back.first.click()
        page.wait_for_timeout(900)
    return data


def push(manifest):
    n = manifest["round"]
    tmp = Path(f"/tmp/nypc-round-{n}.json")
    tmp.write_text(json.dumps(manifest, ensure_ascii=False, indent=2), encoding="utf-8")
    # 원격에서는 /tmp 대신 홈에 둔다. /tmp 는 sticky 라, 다른 계정(예: ubuntu)이
    # 같은 이름으로 먼저 만들어 두면 덮어쓰지 못하고 Permission denied 가 난다.
    subprocess.run(["scp", "-q", "-o", "BatchMode=yes", str(tmp),
                    f"{SSH_HOST}:~/.nypc-round-{n}.json"], check=True)
    r = ssh(f"~/shared/tools/contest-fetch.py ~/.nypc-round-{n}.json")
    print(r.stdout, flush=True)
    if r.returncode:
        print(r.stderr, file=sys.stderr, flush=True)
    s = ssh(f"python3 ~/shared/tools/contest-summary.py ~/shared/runs/contest/round_{n}")
    print(s.stdout, flush=True)


def main():
    load_env()
    log(f"폴러 시작 — {POLL_SEC}초 간격, 서버 {SSH_HOST}")
    with sync_playwright() as p:
        browser = p.chromium.launch(headless=True)
        page = browser.new_page()
        login(page)
        while True:
            try:
                nums, headings = done_rounds()
                open_eval_tab(page)
                rounds = list_rounds(page)
                # 이미 받은 라운드의 헤딩에 시각 문자열이 들어 있다.
                todo = [r for r in rounds
                        if not any(r["when"] in h for h in headings)]
                got = False
                for r in todo:
                    if r.get("tallying"):
                        # 집계가 끝나기 전에 받으면 판이 일부만 들어온다.
                        # (실제로 round_14 가 22판 중 14판만 받힌 적이 있다)
                        ok = False
                        for k in range(1, TALLY_RETRY + 1):
                            log(f"집계 중: {r['when']} — {TALLY_WAIT}초 대기 "
                                f"({k}/{TALLY_RETRY})")
                            time.sleep(TALLY_WAIT)
                            open_eval_tab(page)
                            fresh = [x for x in list_rounds(page)
                                     if x["when"] == r["when"]]
                            if fresh and not fresh[0].get("tallying"):
                                r = fresh[0]
                                ok = True
                                break
                        if not ok:
                            log(f"  아직 집계 중 — 다음 주기로 미룸: {r['when']}")
                            continue
                    log(f"새 라운드 후보: {r['when']} ({r['result']}) — 상세 진입")
                    try:
                        data = scrape_round(page, r["when"])
                    except Exception as e:
                        log(f"  건너뜀: {e}")
                        open_eval_tab(page)   # 목록으로 복귀
                        continue
                    if data["round"] in nums:
                        log(f"  이미 받은 라운드 #{data['round']} — 건너뜀")
                        continue
                    log(f"새 라운드 #{data['round']} · 배틀 {len(data['battles'])}개 — 다운로드")
                    push(data)
                    nums.add(data["round"])
                    got = True
                if not got:
                    log(f"새 라운드 없음 (받은 라운드 {sorted(nums)})")
            except Exception as e:
                log(f"오류: {type(e).__name__}: {e}")
                try:
                    login(page)   # 세션 만료 등 — 재로그인 시도
                except Exception as e2:
                    log(f"재로그인 실패: {e2}")
            time.sleep(POLL_SEC)


if __name__ == "__main__":
    main()
