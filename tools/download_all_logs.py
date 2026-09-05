#!/usr/bin/env python3
"""
NYPC 2026 배틀 로그 end-to-end 자동 다운로드 스크립트

흐름:
  1. 로그인 (자동 or 수동)
  2. /problems/2 → 중간 평가 탭
  3. 완료된 라운드를 순회하며 배틀 ID 추출
  4. 브라우저 세션으로 /api/v2/battle/{id}/log/1 호출 → presigned S3 URL
  5. S3에서 로그 다운로드 → logs/round_{N}/battle_{id}.log

실행:
  pip install playwright && playwright install chromium

  # 자동 로그인 (완전 비대화형)
  NYPC_ID=아이디 NYPC_PW=비밀번호 python3 download_all_logs.py

  # 수동 로그인 (브라우저에서 직접 로그인)
  python3 download_all_logs.py
"""

import json
import os
import urllib.request
from pathlib import Path

from dotenv import load_dotenv
from playwright.sync_api import sync_playwright, TimeoutError as PlaywrightTimeoutError

ROOT = Path(__file__).resolve().parent.parent  # 프로젝트 루트 (scripts/ 의 상위)
load_dotenv(ROOT / ".env")

HOME_URL    = "https://new.nypc.co.kr/ko/"
LOGIN_URL   = "https://new.nypc.co.kr/ko/login"
OUT_BASE    = ROOT / "logs"


# ── DOM 헬퍼 ────────────────────────────────────────────────────────────────

def extract_battle_ids(page) -> list[str]:
    return page.evaluate("""() => {
        const tables = document.querySelectorAll('table');
        for (const t of tables) {
            if (t.querySelector('th')?.textContent?.includes('대전 ID')) {
                return Array.from(t.querySelectorAll('tbody tr'))
                    .map(r => r.querySelector('td')?.textContent?.trim())
                    .filter(Boolean);
            }
        }
        return [];
    }""")


def fetch_presigned_urls(page, battle_ids: list[str]) -> list[dict]:
    return page.evaluate("""async (ids) => {
        const results = [];
        for (const id of ids) {
            const res = await fetch(
                `https://contest.nypc.co.kr/api/v2/battle/${id}/log/1`,
                { credentials: 'include' }
            );
            results.push({ id, status: res.status, content: await res.text() });
        }
        return results;
    }""", battle_ids)


# ── 다운로드 ─────────────────────────────────────────────────────────────────

def download_to_dir(items: list[dict], round_dir: Path) -> tuple[int, int]:
    round_dir.mkdir(parents=True, exist_ok=True)
    ok = err = 0
    for item in items:
        bid      = item["id"]
        out_path = round_dir / f"battle_{bid}.log"
        if out_path.exists():
            print(f"    SKIP  {bid}")
            ok += 1
            continue
        try:
            payload = json.loads(item["content"])
            urllib.request.urlretrieve(payload["url"], out_path)
            print(f"    OK    {bid}  ({out_path.stat().st_size:,} bytes)")
            ok += 1
        except Exception as e:
            print(f"    ERR   {bid}: {e}")
            err += 1
    return ok, err


# ── 메인 ─────────────────────────────────────────────────────────────────────

def main():
    nypc_id = os.environ.get("NYPC_ID")
    nypc_pw = os.environ.get("NYPC_PW")
    OUT_BASE.mkdir(parents=True, exist_ok=True)

    with sync_playwright() as p:
        browser = p.chromium.launch(headless=False)
        page    = browser.new_page()

        login_pwd = page.locator("input[type='password']")

        # 1. 로그인 페이지로 이동 → 리다이렉트로 로그인 상태 판별
        #    로그인 상태면 /ko/login → /ko/ 로 튕기고, 로그아웃이면 폼이 그대로 남는다.
        page.goto(LOGIN_URL)
        page.wait_for_load_state("domcontentloaded")
        page.wait_for_timeout(2000)  # 리다이렉트 정착 대기

        if "login" in page.url:
            # 로그아웃 상태 → 로그인
            login_pwd.wait_for(state="visible", timeout=30_000)
            if not (nypc_id and nypc_pw):
                print("NYPC_ID/NYPC_PW 없음 — 브라우저에서 수동 로그인 (180초 대기)")
            else:
                page.locator("input[placeholder*='아이디']").fill(nypc_id)
                login_pwd.fill(nypc_pw)
                page.get_by_role("button", name="로그인").click()
            # 로그인 완료를 폼이 사라질 때까지 확실히 기다린다 (정착 후 이동)
            login_pwd.wait_for(state="hidden", timeout=180_000)
            page.wait_for_url(lambda u: "login" not in u, timeout=30_000)
            print("✓ 로그인 완료")
        else:
            print("✓ 이미 로그인 상태")

        # 2. /ko/ 에서 'NYPC 아레나 바로가기' 클릭 → contest 랜딩 (SSO 인증 발동)
        if page.url.rstrip("/") != HOME_URL.rstrip("/"):
            page.goto(HOME_URL)
        page.get_by_role("button", name="NYPC 아레나 바로가기").click()

        # 3. 문제 목록에서 NEXT NATION(/problems/2) 클릭 → 문제 페이지
        next_nation = page.locator('a[href="/problems/2"]').first
        next_nation.wait_for(state="visible", timeout=30_000)
        next_nation.click()

        # 4. 중간 평가 탭 진입
        eval_tab = page.get_by_role("tab", name="중간 평가")
        eval_tab.wait_for(state="visible", timeout=60_000)
        eval_tab.click()

        # "라운드 시각" 헤더를 가진 테이블만 라운드 목록으로 본다
        def round_table():
            return page.locator(
                "table", has=page.get_by_role("columnheader", name="라운드 시각")
            )

        def expand_all_rounds():
            see_all = page.locator("text=모든 라운드 보기")
            see_all.wait_for(state="visible", timeout=30_000)
            see_all.click()
            round_table().locator("tbody tr").first.wait_for(
                state="visible", timeout=30_000
            )

        # 3. 라운드 목록 전체 표시 후, 완료된(예정 아님) 라운드 행 수 파악
        expand_all_rounds()

        def done_rows():
            return round_table().locator("tbody tr").filter(
                has_not=page.locator("text=예정")
            )

        n = done_rows().count()
        print(f"\n완료된 라운드 {n}개 발견\n")

        total_ok = total_err = 0

        for i in range(n):
            row         = done_rows().nth(i)
            round_label = row.inner_text().split("\n")[0].strip()
            print(f"[{i+1}/{n}] {round_label}")

            row.click()
            # 상세 진입 — 대전 ID 테이블 렌더를 기다린다
            page.get_by_role("columnheader", name="대전 ID").wait_for(
                state="visible", timeout=30_000
            )

            # 라운드 번호 파싱 — 상세 제목 "중간평가 #6 · ..." 헤딩에서 추출
            title     = page.get_by_role("heading", name="중간평가").first.inner_text()
            round_num = next(
                (w.lstrip("#") for w in title.split() if w.startswith("#")),
                str(i + 1),
            )

            battle_ids = extract_battle_ids(page)
            print(f"  라운드 #{round_num} · 배틀 {len(battle_ids)}개")

            if battle_ids:
                items     = fetch_presigned_urls(page, battle_ids)
                round_dir = OUT_BASE / f"round_{round_num}"
                ok, err   = download_to_dir(items, round_dir)
                total_ok  += ok
                total_err += err

            # 목록으로 복귀 후 다시 펼친다 (복귀 시 접힐 수 있음)
            page.locator("text=돌아가기").click()
            round_table().wait_for(state="visible", timeout=30_000)
            if page.locator("text=모든 라운드 보기").is_visible():
                expand_all_rounds()

        print(f"\n전체 완료: {total_ok}개 성공  {total_err}개 실패")
        browser.close()


if __name__ == "__main__":
    main()
