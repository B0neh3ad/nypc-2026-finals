#!/usr/bin/env python3
"""
NYPC 2026 '모든 제출 목록' 제출 코드 일괄 다운로드 스크립트

흐름:
  1. 로그인 (자동 or 수동) — download_all_logs.py 와 동일
  2. NYPC 아레나 바로가기 → /problems/2 (NEXT NATION) 문제 페이지 (SSO 인증 발동)
  3. '제출 내역' 탭 → '모든 제출 목록' 테이블의 제출 행을 순회
  4. 각 행을 클릭 → 상세의 '제출한 코드' 우측 다운로드 버튼 클릭
  5. 사이트가 내려주는 파일명({제출번호}.{확장자})을 그대로 사용해
     submissions/{제출번호}.{확장자} 로 저장

설계 메모(앞선 수동 작업에서 얻은 교훈):
  · 행 클릭은 '신뢰된' 이벤트라야 상세가 열린다. JS element.click() 은 안 먹히므로
    Playwright 네이티브 클릭(page.locator(...).click())을 쓴다.
  · S3 객체는 octet-stream 이라 확장자 정보가 없다. 언어/확장자는 사이트가 지정한
    다운로드 파일명(download.suggested_filename)을 그대로 사용한다.
    (내용 기반 추론은 C 스타일로 작성한 C++20 을 .c 로 오분류한다.)
  · 헤더(#/제출 시각/결과/노트/수정)가 같은 테이블이 '최종 제출 후보 목록'과
    '모든 제출 목록' 둘이라, tbody 행 수가 가장 많은 테이블을 '모든 제출 목록'으로 본다.
  · 본부 첫 칸이 숫자인 행만 실제 제출이다(나머지는 안내 배너 행).

실행:
  pip install playwright python-dotenv && playwright install chromium

  # 자동 로그인 (완전 비대화형)
  NYPC_ID=아이디 NYPC_PW=비밀번호 python3 download_all_submissions.py

  # 수동 로그인 (브라우저에서 직접 로그인)
  python3 download_all_submissions.py
"""

import os
from pathlib import Path

from dotenv import load_dotenv
from playwright.sync_api import sync_playwright

ROOT = Path(__file__).resolve().parent.parent  # 프로젝트 루트 (scripts/ 의 상위)
load_dotenv(ROOT / ".env")

HOME_URL    = "https://new.nypc.co.kr/ko/"
LOGIN_URL   = "https://new.nypc.co.kr/ko/login"
PROBLEM_URL = "https://contest.nypc.co.kr/problems/2"
OUT_DIR     = ROOT / "submissions"


# ── DOM 헬퍼 (브라우저 컨텍스트에서 실행) ────────────────────────────────────

# '모든 제출 목록' = 헤더에 '제출 시각','결과'가 있는 테이블 중 tbody 행이 가장 많은 것.
_FIND_LIST_TABLE = """() => {
    const tables = [...document.querySelectorAll('table')];
    let best = null, bestRows = -1;
    for (const t of tables) {
        const head = (t.querySelector('thead')?.innerText
                   || t.querySelector('tr')?.innerText || '');
        if (head.includes('제출 시각') && head.includes('결과')) {
            const n = t.querySelectorAll('tbody tr').length;
            if (n > bestRows) { bestRows = n; best = t; }
        }
    }
    return best;
}"""


def extract_submission_ids(page) -> list[str]:
    """'모든 제출 목록'에서 첫 칸이 숫자인(=실제 제출) 행의 제출번호를 추출."""
    return page.evaluate("""() => {
        const findTable = %s;
        const t = findTable();
        if (!t) return [];
        return [...t.querySelectorAll('tbody tr')]
            .map(r => r.querySelector('td')?.textContent.trim())
            .filter(s => /^\\d+$/.test(s));
    }""" % _FIND_LIST_TABLE)


def tag_submission_row(page, sid: str) -> bool:
    """'모든 제출 목록'에서 해당 제출 행에 임시 id(__dl_row)를 달고 화면 중앙으로 스크롤."""
    return page.evaluate("""(id) => {
        document.getElementById('__dl_row')?.removeAttribute('id');
        const findTable = %s;
        const t = findTable();
        if (!t) return false;
        const row = [...t.querySelectorAll('tbody tr')]
            .find(r => r.querySelector('td')?.textContent.trim() === id);
        if (!row) return false;
        row.id = '__dl_row';
        row.scrollIntoView({ block: 'center' });
        return true;
    }""" % _FIND_LIST_TABLE, sid)


def tag_code_download_button(page) -> bool:
    """상세의 '제출한 코드' 라벨 옆 다운로드 버튼에 임시 id(__code_dl)를 단다."""
    return page.evaluate("""() => {
        const label = [...document.querySelectorAll('*')].find(e =>
            [...e.childNodes].some(n => n.nodeType === 3
                && n.textContent.includes('제출한 코드')));
        if (!label) return false;
        let cont = label;
        for (let i = 0; i < 5; i++) {
            cont = cont.parentElement;
            if (cont && cont.querySelector('button')) break;
        }
        const btn = cont && cont.querySelector('button');
        if (!btn) return false;
        btn.id = '__code_dl';
        return true;
    }""")


# ── 내비게이션 ───────────────────────────────────────────────────────────────

def login(page, nypc_id, nypc_pw):
    login_pwd = page.locator("input[type='password']")

    # 로그인 페이지로 이동 → 리다이렉트로 로그인 상태 판별
    # 로그인 상태면 /ko/login → /ko/ 로 튕기고, 로그아웃이면 폼이 그대로 남는다.
    page.goto(LOGIN_URL)
    page.wait_for_load_state("domcontentloaded")
    page.wait_for_timeout(2000)  # 리다이렉트 정착 대기

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


def open_problem_submissions(page):
    """아레나 → /problems/2 → '제출 내역' 탭까지 진입."""
    # /ko/ 에서 'NYPC 아레나 바로가기' 클릭 → contest 랜딩 (SSO 인증 발동)
    if page.url.rstrip("/") != HOME_URL.rstrip("/"):
        page.goto(HOME_URL)
    page.get_by_role("button", name="NYPC 아레나 바로가기").click()

    # 문제 목록에서 NEXT NATION(/problems/2) 진입
    next_nation = page.locator('a[href="/problems/2"]').first
    next_nation.wait_for(state="visible", timeout=30_000)
    next_nation.click()
    page.wait_for_url("**/problems/2", timeout=30_000)

    # '제출 내역' 탭 진입
    tab = page.get_by_role("tab", name="제출 내역")
    tab.wait_for(state="visible", timeout=30_000)
    tab.click()

    # '모든 제출 목록' 렌더 대기
    page.get_by_text("모든 제출 목록").first.wait_for(state="visible", timeout=30_000)
    page.wait_for_timeout(1000)


def go_back_to_list(page):
    """상세에서 '돌아가기'로 목록 복귀."""
    back = page.get_by_text("돌아가기").first
    if back.count() and back.is_visible():
        back.click()
    page.get_by_text("모든 제출 목록").first.wait_for(state="visible", timeout=15_000)
    page.wait_for_timeout(200)


# ── 메인 ─────────────────────────────────────────────────────────────────────

def main():
    nypc_id = os.environ.get("NYPC_ID")
    nypc_pw = os.environ.get("NYPC_PW")
    OUT_DIR.mkdir(parents=True, exist_ok=True)

    with sync_playwright() as p:
        browser = p.chromium.launch(headless=False)
        page    = browser.new_page(accept_downloads=True)

        login(page, nypc_id, nypc_pw)
        open_problem_submissions(page)

        ids = extract_submission_ids(page)
        print(f"\n'모든 제출 목록' 제출 {len(ids)}개 발견\n")

        ok = skip = err = 0
        for i, sid in enumerate(ids, 1):
            # 이미 받은 제출은 건너뛴다
            if list(OUT_DIR.glob(f"{sid}.*")):
                print(f"[{i}/{len(ids)}] SKIP  {sid}")
                skip += 1
                continue

            try:
                # 1) 행 클릭 → 상세 진입 (신뢰된 클릭이라야 열린다)
                if not tag_submission_row(page, sid):
                    print(f"[{i}/{len(ids)}] ERR   {sid}: 행을 찾지 못함")
                    err += 1
                    continue
                page.locator("#__dl_row td").first.click()
                page.get_by_text("제출한 코드").first.wait_for(
                    state="visible", timeout=15_000
                )

                # 2) '제출한 코드' 다운로드 버튼 클릭 → 다운로드 캡처
                if not tag_code_download_button(page):
                    print(f"[{i}/{len(ids)}] ERR   {sid}: 다운로드 버튼 없음")
                    err += 1
                    go_back_to_list(page)
                    continue
                with page.expect_download() as dl_info:
                    page.locator("#__code_dl").click()
                download = dl_info.value

                # 3) 사이트가 지정한 확장자를 살려 {제출번호}.{확장자} 로 저장
                suffix   = Path(download.suggested_filename).suffix or ".txt"
                out_path = OUT_DIR / f"{sid}{suffix}"
                download.save_as(out_path)
                print(f"[{i}/{len(ids)}] OK    {out_path.name}"
                      f"  ({out_path.stat().st_size:,} bytes)")
                ok += 1

            except Exception as e:
                print(f"[{i}/{len(ids)}] ERR   {sid}: {e}")
                err += 1
            finally:
                # 목록으로 복귀 (상세가 열려 있으면 닫는다)
                try:
                    go_back_to_list(page)
                except Exception:
                    pass

        print(f"\n전체 완료: {ok}개 성공  {skip}개 건너뜀  {err}개 실패"
              f"  → {OUT_DIR}")
        browser.close()


if __name__ == "__main__":
    main()
