#!/usr/bin/env python3
"""
중간평가 라운드 ↔ 제출 파일 매핑 생성 스크립트 (end-to-end)

흐름:
  1. 로그인 (자동 or 수동) — download_all_*.py 와 동일
  2. NYPC 아레나 → /problems/2 → '중간 평가' 탭, '모든 라운드 보기' 펼치기
  3. 라운드 메타데이터(라운드ID·제목·submissionId·시각·성적)를 클라이언트 상태에서 추출
  4. 각 라운드를 그 라운드에 사용된 내 제출(submissionId) 및 로컬 제출 파일과 매핑
  5. round_submission_mapping.json / .csv 작성

매핑 근거:
  각 〈VS: 전국의 참가자〉 라운드는 '라운드 시각' 기준으로 등록된 내 최종 제출 1개
  (round.submissionId)를 사용한다. 따라서
      round  →  round.submissionId  →  submissions/{id}.{ext}
  로 매핑된다. (배틀 상세를 열 필요 없이 라운드 목록만으로 구성된다.)

실행:
  pip install playwright python-dotenv && playwright install chromium
  NYPC_ID=아이디 NYPC_PW=비밀번호 python3 build_round_mapping.py
  # 또는 수동 로그인
  python3 build_round_mapping.py
"""

import csv
import json
import os
from datetime import datetime, timezone, timedelta
from pathlib import Path

from dotenv import load_dotenv
from playwright.sync_api import sync_playwright

ROOT = Path(__file__).resolve().parent.parent  # 프로젝트 루트 (scripts/ 의 상위)
load_dotenv(ROOT / ".env")

HOME_URL = "https://new.nypc.co.kr/ko/"
LOGIN_URL = "https://new.nypc.co.kr/ko/login"
SUBS = ROOT / "submissions"
KST = timezone(timedelta(hours=9))


# ── 브라우저 컨텍스트에서 실행할 JS ──────────────────────────────────────────

# '모든 라운드 보기'(목록 펼치기) 클릭
_EXPAND_ROUNDS = """() => {
    const e = [...document.querySelectorAll('*')].find(el =>
        [...el.childNodes].some(n => n.nodeType === 3
            && n.textContent.includes('모든 라운드 보기')));
    if (e && e.offsetParent) { e.click(); return true; }
    return false;
}"""

# 라운드 테이블 fiber 에서 라운드 메타데이터 배열 추출
_READ_ROUNDS = """() => {
    const t = [...document.querySelectorAll('table')].find(tb => {
        const h = (tb.querySelector('thead')?.innerText
                || tb.querySelector('tr')?.innerText || '');
        return h.includes('라운드 시각');
    });
    if (!t) return null;
    const key = Object.keys(t).find(k => k.startsWith('__reactFiber$'));
    let f = t[key], depth = 0, arr = null;
    const looks = v => Array.isArray(v) && v.length >= 1
        && v.some(x => x && typeof x === 'object'
            && 'title' in x && 'submissionId' in x && 'beginAt' in x);
    while (f && depth < 60 && !arr) {
        let h = f.memoizedState, hi = 0;
        while (h && hi < 40) { if (looks(h.memoizedState)) { arr = h.memoizedState; break; } h = h.next; hi++; }
        f = f.return; depth++;
    }
    if (!arr) return null;
    return arr.map(r => ({
        id: r.id, title: r.title, status: r.status, submissionId: r.submissionId,
        beginAt: r.beginAt, myRank: r.myRank, myPerformance: r.myPerformance,
        participants: r.totalParticipants, qualifyingScore: r.qualifyingScore,
    }));
}"""


# ── 로그인 / 내비게이션 ──────────────────────────────────────────────────────

def login(page, nypc_id, nypc_pw):
    login_pwd = page.locator("input[type='password']")
    page.goto(LOGIN_URL)
    page.wait_for_load_state("domcontentloaded")
    page.wait_for_timeout(2000)
    if "login" in page.url:
        login_pwd.wait_for(state="visible", timeout=30_000)
        if not (nypc_id and nypc_pw):
            print("NYPC_ID/NYPC_PW 없음 — 브라우저에서 수동 로그인 (180초 대기)")
        else:
            page.locator("input[placeholder*='아이디']").fill(nypc_id)
            login_pwd.fill(nypc_pw)
            page.get_by_role("button", name="로그인").click()
        login_pwd.wait_for(state="hidden", timeout=180_000)
        page.wait_for_url(lambda u: "login" not in u, timeout=30_000)
        print("✓ 로그인 완료")
    else:
        print("✓ 이미 로그인 상태")


def open_eval_tab(page):
    if page.url.rstrip("/") != HOME_URL.rstrip("/"):
        page.goto(HOME_URL)
    page.get_by_role("button", name="NYPC 아레나 바로가기").click()
    next_nation = page.locator('a[href="/problems/2"]').first
    next_nation.wait_for(state="visible", timeout=30_000)
    next_nation.click()
    page.wait_for_url("**/problems/2", timeout=30_000)
    tab = page.get_by_role("tab", name="중간 평가")
    tab.wait_for(state="visible", timeout=30_000)
    tab.click()
    page.get_by_text("모든 라운드 보기").first.wait_for(state="visible", timeout=30_000)
    page.evaluate(_EXPAND_ROUNDS)
    page.wait_for_timeout(1000)


# ── 헬퍼 ─────────────────────────────────────────────────────────────────────

def submission_file(sid):
    if sid is None:
        return None
    hits = sorted(SUBS.glob(f"{sid}.*"))
    return hits[0].name if hits else None


# ── 메인 ─────────────────────────────────────────────────────────────────────

def main():
    nypc_id = os.environ.get("NYPC_ID")
    nypc_pw = os.environ.get("NYPC_PW")

    with sync_playwright() as p:
        browser = p.chromium.launch(headless=False)
        page = browser.new_page()

        login(page, nypc_id, nypc_pw)
        open_eval_tab(page)

        rounds = page.evaluate(_READ_ROUNDS) or []
        # '완료'이고 submissionId 가 있는 라운드만 매핑 (예정/미참여 제외)
        mapped = [r for r in rounds
                  if r["status"] == "finished" and r["submissionId"] is not None]
        mapped.sort(key=lambda r: r["id"], reverse=True)

        rows = []
        for r in mapped:
            sid = r["submissionId"]
            sfile = submission_file(sid)
            rows.append({
                "round": r["title"],
                "round_id": r["id"],
                "round_time_kst": datetime.fromtimestamp(r["beginAt"] / 1000, KST).isoformat(),
                "submission_id": sid,
                "submission_file": f"submissions/{sfile}" if sfile else None,
                "file_available": bool(sfile),
                "my_rank": r["myRank"],
                "my_performance": r["myPerformance"],
                "participants": r["participants"],
            })

        doc = {
            "description": "중간평가 〈VS: 전국의 참가자〉 라운드 ↔ 내 제출 코드 파일 매핑",
            "mapping_basis": "round → round.submissionId → submissions/{id}.{ext}",
            "generated_at": datetime.now(KST).isoformat(),
            "round_count": len(rows),
            "rounds": rows,
        }
        (ROOT / "round_submission_mapping.json").write_text(
            json.dumps(doc, ensure_ascii=False, indent=2))
        with (ROOT / "round_submission_mapping.csv").open("w", newline="") as f:
            w = csv.DictWriter(f, fieldnames=[
                "round", "round_id", "round_time_kst",
                "submission_id", "submission_file", "file_available",
                "my_rank", "my_performance", "participants"])
            w.writeheader()
            w.writerows(rows)

        have = sum(1 for r in rows if r["file_available"])
        print(f"\n생성 완료: {len(rows)}개 라운드 매핑 (로컬 제출 파일 보유 {have}개)")
        for r in rows:
            f = Path(r["submission_file"]).name if r["submission_file"] else "(파일 없음)"
            print(f"  {r['round']:<14} round_id={r['round_id']}  sub={r['submission_id']}  {f}")
        print("\n  → round_submission_mapping.json")
        print("  → round_submission_mapping.csv")
        browser.close()


if __name__ == "__main__":
    main()
