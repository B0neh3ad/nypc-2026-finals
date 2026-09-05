#!/usr/bin/env python3
"""
NYPC 2026 중간평가 상세 정보를 라운드별 JSON으로 저장하는 스크립트.

흐름:
  1. 로그인 (자동 or 수동) - 기존 download_all_*.py 와 동일
  2. NYPC 아레나 → /problems/2 → '중간 평가' 탭
  3. 완료된 중간평가 라운드 순회
  4. 각 라운드의 제출/성적/대전 목록과 대전 상세 모달의 실행 시간/메모리 파싱
  5. round_evaluations/round_{N}.json 및 index.json 저장

실행:
  pip install playwright python-dotenv && playwright install chromium

  # 자동 로그인
  NYPC_ID=아이디 NYPC_PW=비밀번호 python3 export_round_evaluations.py

  # 수동 로그인
  python3 export_round_evaluations.py

  # 특정 라운드만
  python3 export_round_evaluations.py --round 27
"""

import argparse
import json
import os
import re
from datetime import datetime, timedelta, timezone
from pathlib import Path

from playwright.sync_api import TimeoutError as PlaywrightTimeoutError
from playwright.sync_api import sync_playwright

try:
    from dotenv import load_dotenv
except ModuleNotFoundError:
    def load_dotenv(path):
        if not path.exists():
            return False
        for line in path.read_text(encoding="utf-8").splitlines():
            line = line.strip()
            if not line or line.startswith("#") or "=" not in line:
                continue
            key, value = line.split("=", 1)
            os.environ.setdefault(key.strip(), value.strip().strip("'\""))
        return True


ROOT = Path(__file__).resolve().parent.parent
load_dotenv(ROOT / ".env")

HOME_URL = "https://new.nypc.co.kr/ko/"
LOGIN_URL = "https://new.nypc.co.kr/ko/login"
OUT_DIR = ROOT / "round_evaluations"
KST = timezone(timedelta(hours=9))


# ── 브라우저 컨텍스트 JS ────────────────────────────────────────────────────

_EXPAND_ROUNDS = """() => {
    const e = [...document.querySelectorAll('*')].find(el =>
        [...el.childNodes].some(n => n.nodeType === 3
            && n.textContent.includes('모든 라운드 보기')));
    if (e && e.offsetParent) { e.click(); return true; }
    return false;
}"""


_READ_ROUNDS = """() => {
    const t = [...document.querySelectorAll('table')].find(tb => {
        const h = (tb.querySelector('thead')?.innerText
                || tb.querySelector('tr')?.innerText || '');
        return h.includes('라운드 시각');
    });
    if (!t) return null;

    const key = Object.keys(t).find(k => k.startsWith('__reactFiber$'));
    let f = key ? t[key] : null, depth = 0, arr = null;
    const looks = v => Array.isArray(v) && v.length >= 1
        && v.some(x => x && typeof x === 'object'
            && 'title' in x && 'submissionId' in x && 'beginAt' in x);

    while (f && depth < 60 && !arr) {
        let h = f.memoizedState, hi = 0;
        while (h && hi < 40) {
            if (looks(h.memoizedState)) { arr = h.memoizedState; break; }
            h = h.next;
            hi++;
        }
        f = f.return;
        depth++;
    }
    if (!arr) return null;

    return arr.map(r => ({
        id: r.id,
        title: r.title,
        status: r.status,
        submissionId: r.submissionId,
        beginAt: r.beginAt,
        myRank: r.myRank,
        myPerformance: r.myPerformance,
        participants: r.totalParticipants,
        qualifyingScore: r.qualifyingScore,
    }));
}"""


_READ_ROUND_DETAIL = """(expectedSubmissionId) => {
    const clean = s => (s || '').replace(/\\s+/g, ' ').trim();
    const textOf = el => clean(el?.innerText || el?.textContent || '');
    const tables = [...document.querySelectorAll('table')];

    const findTable = (...needles) => tables.find(t => {
        const h = textOf(t.querySelector('thead') || t.querySelector('tr'));
        return needles.every(n => h.includes(n));
    });

    const heading = [...document.querySelectorAll('h1,h2,h3,h4')]
        .map(textOf).find(t => t.includes('중간평가')) || null;

    const submissionTable = findTable('#', '제출 시각', '결과', '노트');
    let submission = null;
    if (submissionTable) {
        const rows = [...submissionTable.querySelectorAll('tbody tr')];
        const expected = expectedSubmissionId == null ? null : String(expectedSubmissionId);
        const row = expected
            ? rows.find(r => textOf(r.querySelector('td')) === expected)
            : rows[0] || null;
        if (row) {
            const cells = [...row.querySelectorAll('td')];
            submission = {
                id: Number(textOf(cells[0])) || null,
                submitted_at_kst_text: textOf(cells[1]),
                result_text: textOf(cells[2]),
                note: textOf(cells[3]),
            };
        }
    }

    const battleTable = findTable('대전 ID', '상대 팀', '상대 퍼포먼스', '내 전적');
    const battles = [];
    if (battleTable) {
        for (const row of battleTable.querySelectorAll('tbody tr')) {
            const cells = [...row.querySelectorAll('td')];
            if (cells.length < 4) continue;
            const battleId = textOf(cells[0]);
            if (!/^\\d+$/.test(battleId)) continue;
            const perfText = textOf(cells[2]);
            const perfMatch = perfText.match(/(\\d+)\\s*$/);
            battles.push({
                battle_id: battleId,
                opponent_team: textOf(cells[1]),
                opponent_performance: perfMatch ? Number(perfMatch[1]) : null,
                opponent_performance_text: perfText,
                my_record: textOf(cells[3]),
            });
        }
    }

    return { heading, submission, battles };
}"""


_READ_SUBMISSION_ROWS = """() => {
    const clean = s => (s || '').replace(/\\s+/g, ' ').trim();
    const textOf = el => clean(el?.innerText || el?.textContent || '');

    let best = null, bestRows = -1;
    for (const table of [...document.querySelectorAll('table')]) {
        const head = textOf(table.querySelector('thead') || table.querySelector('tr'));
        if (!(head.includes('제출 시각') && head.includes('결과') && head.includes('노트'))) {
            continue;
        }
        const rows = [...table.querySelectorAll('tbody tr')]
            .filter(row => /^\\d+$/.test(textOf(row.querySelector('td'))));
        if (rows.length > bestRows) {
            best = table;
            bestRows = rows.length;
        }
    }
    if (!best) return [];

    return [...best.querySelectorAll('tbody tr')].map(row => {
        const cells = [...row.querySelectorAll('td')];
        const idText = textOf(cells[0]);
        if (!/^\\d+$/.test(idText)) return null;
        return {
            id: Number(idText),
            submitted_at_kst_text: textOf(cells[1]),
            result_text: textOf(cells[2]),
            note: textOf(cells[3]),
        };
    }).filter(Boolean);
}"""


_CLICK_NEXT_SUBMISSION_PAGE = """() => {
    const clean = s => (s || '').replace(/\\s+/g, ' ').trim();
    const visible = el => {
        const style = window.getComputedStyle(el);
        const rect = el.getBoundingClientRect();
        return style.visibility !== 'hidden'
            && style.display !== 'none'
            && rect.width > 0
            && rect.height > 0;
    };
    const disabled = el => el.disabled
        || el.getAttribute('aria-disabled') === 'true'
        || /disabled/i.test(String(el.className || ''));

    const controls = [...document.querySelectorAll('button,a')]
        .filter(el => visible(el) && !disabled(el));
    const labelOf = el => clean([
        el.getAttribute('aria-label'),
        el.getAttribute('title'),
        el.innerText,
        el.textContent,
    ].filter(Boolean).join(' '));

    const direct = controls.find(el => {
        const label = labelOf(el);
        return /^(다음|Next|>|›|»)$/.test(label) || /다음|next/i.test(label);
    });
    if (direct) {
        direct.click();
        return true;
    }

    const numbered = controls
        .map((el, idx) => ({ el, idx, label: labelOf(el) }))
        .filter(x => /^\\d+$/.test(x.label));
    const current = numbered.find(x => x.el.getAttribute('aria-current') === 'page'
        || /active|selected|current/i.test(String(x.el.className || '')));
    if (current) {
        const cur = Number(current.label);
        const next = numbered.find(x => Number(x.label) === cur + 1);
        if (next) {
            next.el.click();
            return true;
        }
    }

    return false;
}"""


_TAG_ROUND_ROW = """(rowIndex) => {
    document.getElementById('__round_eval_row')?.removeAttribute('id');
    const table = [...document.querySelectorAll('table')].find(t => {
        const h = (t.querySelector('thead')?.innerText
                || t.querySelector('tr')?.innerText || '');
        return h.includes('라운드 시각');
    });
    if (!table) return false;
    const rows = [...table.querySelectorAll('tbody tr')]
        .filter(r => !r.innerText.includes('예정'));
    const row = rows[rowIndex];
    if (!row) return false;
    row.id = '__round_eval_row';
    row.scrollIntoView({ block: 'center' });
    return true;
}"""


_TAG_BATTLE_BUTTON = """(battleId) => {
    document.getElementById('__battle_detail_btn')?.removeAttribute('id');
    const clean = s => (s || '').replace(/\\s+/g, ' ').trim();
    const table = [...document.querySelectorAll('table')].find(t => {
        const h = clean(t.querySelector('thead')?.innerText
             || t.querySelector('tr')?.innerText || '');
        return h.includes('대전 ID') && h.includes('결과 및 로그');
    });
    if (!table) return false;
    const row = [...table.querySelectorAll('tbody tr')]
        .find(r => clean(r.querySelector('td')?.innerText) === String(battleId));
    if (!row) return false;
    const btn = row.querySelector('button');
    if (!btn) return false;
    btn.id = '__battle_detail_btn';
    row.scrollIntoView({ block: 'center' });
    return true;
}"""


_READ_BATTLE_DIALOG = """(battleId) => {
    const clean = s => (s || '').replace(/\\s+/g, ' ').trim();
    const compact = s => clean(s).replace(/\\s+/g, '');
    const textOf = el => clean(el?.innerText || el?.textContent || '');

    const parseRunCell = (cell) => {
        const text = textOf(cell);
        const timeMatch = text.match(/(\\d+(?:\\.\\d+)?)\\s*초/);
        const memoryMatch = text.match(/([\\d,]+(?:\\.\\d+)?)\\s*MiB/);
        let scoreText = text;
        if (timeMatch) scoreText = scoreText.replace(timeMatch[0], '');
        if (memoryMatch) scoreText = scoreText.replace(memoryMatch[0], '');
        scoreText = scoreText.replace(/\\s+/g, '');
        scoreText = scoreText.replace(/^(\\d+)\\.(\\d+)$/, '$1.$2');

        return {
            score_text: scoreText || null,
            score: scoreText ? Number(scoreText) : null,
            time_sec: timeMatch ? Number(timeMatch[1]) : null,
            time_text: timeMatch ? timeMatch[0].replace(/\\s+/g, '') : null,
            memory_mib: memoryMatch ? Number(memoryMatch[1].replace(/,/g, '')) : null,
            memory_text: memoryMatch ? memoryMatch[0].replace(/\\s+/g, '') : null,
            raw_text: text,
        };
    };

    const sameRun = (a, b) => !!a && !!b
        && a.score_text === b.score_text
        && a.time_text === b.time_text
        && a.memory_text === b.memory_text;

    const withSide = (side, stats) => stats ? {
        side,
        score_text: stats.score_text,
        score: stats.score,
        time_sec: stats.time_sec,
        time_text: stats.time_text,
        memory_mib: stats.memory_mib,
        memory_text: stats.memory_text,
        raw_text: stats.raw_text,
    } : null;

    const dialogs = [...document.querySelectorAll('[role="dialog"]')];
    const dialog = dialogs.find(d => textOf(d).includes(`대전 상세 #${battleId}`))
        || dialogs[dialogs.length - 1];
    if (!dialog) return { compile_status: null, games: [], error: 'dialog not found' };

    const compileStatus = textOf(dialog.querySelector('strong')) || null;
    const table = [...dialog.querySelectorAll('table')].find(t => {
        const h = textOf(t.querySelector('thead') || t.querySelector('tr'));
        return h.includes('배틀') && h.includes('내 결과')
            && h.includes('1P') && h.includes('2P');
    });
    if (!table) {
        return { compile_status: compileStatus, games: [], error: 'result table not found' };
    }

    const games = [];
    for (const row of table.querySelectorAll('tbody tr')) {
        const cells = [...row.querySelectorAll('td')];
        if (cells.length < 5) continue;

        const gameNo = Number(textOf(cells[0]));
        const myResult = textOf(cells[1]);
        const logStats = parseRunCell(cells[2]);
        const p1 = parseRunCell(cells[cells.length - 2]);
        const p2 = parseRunCell(cells[cells.length - 1]);

        let mySide = null;
        if (sameRun(logStats, p1) && !sameRun(logStats, p2)) mySide = '1P';
        else if (sameRun(logStats, p2) && !sameRun(logStats, p1)) mySide = '2P';
        else if (sameRun(logStats, p1)) mySide = '1P';
        else if (sameRun(logStats, p2)) mySide = '2P';

        games.push({
            game: Number.isFinite(gameNo) ? gameNo : null,
            my_result: myResult,
            log: logStats,
            p1,
            p2,
            my_side: mySide,
            my_code: mySide === '1P' ? withSide('1P', p1)
                : mySide === '2P' ? withSide('2P', p2) : null,
            opponent_code: mySide === '1P' ? withSide('2P', p2)
                : mySide === '2P' ? withSide('1P', p1) : null,
            raw_text: compact(row.innerText),
        });
    }

    return { compile_status: compileStatus, games };
}"""


_CLOSE_BATTLE_DIALOG = """(battleId) => {
    const clean = s => (s || '').replace(/\\s+/g, ' ').trim();
    const dialogs = [...document.querySelectorAll('[role="dialog"]')];
    const dialog = dialogs.find(d => clean(d.innerText).includes(`대전 상세 #${battleId}`))
        || dialogs[dialogs.length - 1];
    if (!dialog) return false;
    const button = [...dialog.querySelectorAll('button')]
        .find(b => clean(b.innerText).includes('닫기'));
    if (!button) return false;
    button.click();
    return true;
}"""


_CLOSE_VISIBLE_DIALOGS = """() => {
    const clean = s => (s || '').replace(/\\s+/g, ' ').trim();
    let closed = 0;
    for (const dialog of [...document.querySelectorAll('[role="dialog"]')].reverse()) {
        const style = window.getComputedStyle(dialog);
        if (style.visibility === 'hidden' || style.display === 'none') continue;
        const buttons = [...dialog.querySelectorAll('button')];
        const button = buttons.find(b => {
            const label = clean(b.getAttribute('aria-label') || b.innerText);
            return label.includes('닫기') || label.includes('Close') || label.includes('확인');
        }) || buttons[buttons.length - 1];
        if (button) {
            button.click();
            closed++;
        }
    }
    return closed;
}"""


# ── 일반 헬퍼 ────────────────────────────────────────────────────────────────

def parse_round_number(title: str | None) -> int | None:
    if not title:
        return None
    m = re.search(r"#(\d+)", title)
    return int(m.group(1)) if m else None


def is_main_round(title: str | None) -> bool:
    return bool(title and title.startswith("중간평가 #"))


def kst_iso_from_ms(epoch_ms: int | float | None) -> str | None:
    if epoch_ms is None:
        return None
    return datetime.fromtimestamp(epoch_ms / 1000, KST).isoformat()


def compact_result(rank: int | None, participants: int | None) -> str | None:
    if rank is None or participants is None:
        return None
    return f"{rank}위 / {participants}팀"


def round_table(page):
    return page.locator(
        "table", has=page.get_by_role("columnheader", name="라운드 시각")
    )


def expand_all_rounds(page):
    if page.locator("text=모든 라운드 보기").is_visible():
        page.evaluate(_EXPAND_ROUNDS)
    round_table(page).locator("tbody tr").first.wait_for(
        state="visible", timeout=30_000
    )


def close_visible_dialogs(page):
    for _ in range(5):
        closed = page.evaluate(_CLOSE_VISIBLE_DIALOGS)
        if not closed:
            return
        page.wait_for_timeout(300)


def login(page, nypc_id: str | None, nypc_pw: str | None):
    login_pwd = page.locator("input[type='password']")
    page.goto(LOGIN_URL)
    page.wait_for_load_state("domcontentloaded")
    page.wait_for_timeout(2000)

    if "login" in page.url:
        login_pwd.wait_for(state="visible", timeout=30_000)
        if not (nypc_id and nypc_pw):
            print("NYPC_ID/NYPC_PW 없음 - 브라우저에서 수동 로그인 (180초 대기)")
        else:
            page.locator("input[placeholder*='아이디']").fill(nypc_id)
            login_pwd.fill(nypc_pw)
            page.get_by_role("button", name="로그인").click()
        login_pwd.wait_for(state="hidden", timeout=180_000)
        page.wait_for_url(lambda u: "login" not in u, timeout=30_000)
        print("로그인 완료")
    else:
        print("이미 로그인 상태")


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
    page.get_by_text("모든 라운드 보기").first.wait_for(
        state="visible", timeout=30_000
    )
    close_visible_dialogs(page)
    expand_all_rounds(page)
    page.wait_for_timeout(1000)


def open_submission_tab(page):
    if page.url.rstrip("/") != HOME_URL.rstrip("/"):
        page.goto(HOME_URL)
    page.get_by_role("button", name="NYPC 아레나 바로가기").click()

    next_nation = page.locator('a[href="/problems/2"]').first
    next_nation.wait_for(state="visible", timeout=30_000)
    next_nation.click()
    page.wait_for_url("**/problems/2", timeout=30_000)

    tab = page.get_by_role("tab", name="제출 내역")
    tab.wait_for(state="visible", timeout=30_000)
    tab.click()
    page.get_by_text("모든 제출 목록").first.wait_for(
        state="visible", timeout=30_000
    )
    page.wait_for_timeout(1000)


def click_round_row(page, row_index: int):
    close_visible_dialogs(page)
    if not page.evaluate(_TAG_ROUND_ROW, row_index):
        raise RuntimeError(f"라운드 목록 {row_index}번째 행을 찾지 못함")
    page.locator("#__round_eval_row").click()
    page.get_by_role("columnheader", name="대전 ID").wait_for(
        state="visible", timeout=30_000
    )


def back_to_round_list(page):
    page.locator("text=돌아가기").click()
    round_table(page).wait_for(state="visible", timeout=30_000)
    expand_all_rounds(page)
    page.wait_for_timeout(300)


def read_battle_detail(page, battle_id: str) -> dict:
    if not page.evaluate(_TAG_BATTLE_BUTTON, battle_id):
        raise RuntimeError("결과 및 로그 버튼을 찾지 못함")

    page.locator("#__battle_detail_btn").click()
    try:
        page.get_by_role("heading", name=f"대전 상세 #{battle_id}").wait_for(
            state="visible", timeout=30_000
        )
        return page.evaluate(_READ_BATTLE_DIALOG, battle_id)
    finally:
        closed = page.evaluate(_CLOSE_BATTLE_DIALOG, battle_id)
        if closed:
            try:
                page.get_by_role("heading", name=f"대전 상세 #{battle_id}").wait_for(
                    state="hidden", timeout=10_000
                )
            except PlaywrightTimeoutError:
                pass


def load_submission_lookup(page, target_ids: set[int]) -> dict[int, dict]:
    """제출 내역 탭의 모든 제출 목록을 페이지별로 훑어 target submission 메타를 찾는다."""
    if not target_ids:
        return {}

    open_submission_tab(page)
    found: dict[int, dict] = {}
    seen_pages: set[tuple[int, ...]] = set()

    for page_no in range(1, 200):
        rows = page.evaluate(_READ_SUBMISSION_ROWS) or []
        ids_on_page = tuple(r["id"] for r in rows)
        if ids_on_page in seen_pages:
            break
        seen_pages.add(ids_on_page)

        for row in rows:
            sid = row.get("id")
            if sid in target_ids:
                found[sid] = row

        if target_ids.issubset(found):
            break

        if not page.evaluate(_CLICK_NEXT_SUBMISSION_PAGE):
            break
        page.wait_for_timeout(800)

    missing = sorted(target_ids - set(found))
    print(
        f"제출 메타 조회: {len(found)}/{len(target_ids)}개 발견"
        + (f" (누락: {missing})" if missing else "")
    )
    return found


def resolve_submission(round_meta: dict, detail: dict, submission_lookup: dict[int, dict]):
    sid = round_meta.get("submissionId")
    if sid in submission_lookup:
        return submission_lookup[sid]

    submission = detail.get("submission")
    if submission and submission.get("id") == sid:
        return submission

    return {
        "id": sid,
        "submitted_at_kst_text": None,
        "result_text": None,
        "note": "",
    }


def update_existing_round_files(
    out_dir: Path,
    targets: list[tuple[int, dict]],
    submission_lookup: dict[int, dict],
):
    changed = []
    for _, round_meta in targets:
        round_number = parse_round_number(round_meta.get("title"))
        sid = round_meta.get("submissionId")
        if round_number is None or sid not in submission_lookup:
            continue

        out_path = out_dir / f"round_{round_number}.json"
        if not out_path.exists():
            continue
        doc = json.loads(out_path.read_text(encoding="utf-8"))
        old = doc.get("submission") or {}
        new = submission_lookup[sid]
        if old != new:
            doc["submission"] = new
            out_path.write_text(
                json.dumps(doc, ensure_ascii=False, indent=2) + "\n",
                encoding="utf-8",
            )
            changed.append((round_number, old.get("id"), sid, new.get("note")))

    index_path = out_dir / "index.json"
    if index_path.exists():
        index = json.loads(index_path.read_text(encoding="utf-8"))
        for row in index.get("rounds", []):
            rn = row.get("round_number")
            for _, round_meta in targets:
                if parse_round_number(round_meta.get("title")) == rn:
                    row["submission_id"] = round_meta.get("submissionId")
                    break
        index_path.write_text(
            json.dumps(index, ensure_ascii=False, indent=2) + "\n",
            encoding="utf-8",
        )

    return changed


def build_round_doc(
    round_meta: dict,
    detail: dict,
    submission_lookup: dict[int, dict] | None = None,
) -> dict:
    round_title = detail.get("heading") or round_meta.get("title")
    round_number = parse_round_number(round_title)
    submission = resolve_submission(round_meta, detail, submission_lookup or {})

    return {
        "round": round_title,
        "round_number": round_number,
        "round_id": round_meta.get("id"),
        "round_time_kst": kst_iso_from_ms(round_meta.get("beginAt")),
        "submission": submission,
        "result": {
            "rating": round_meta.get("myPerformance"),
            "rank": round_meta.get("myRank"),
            "participants": round_meta.get("participants"),
            "rank_text": compact_result(
                round_meta.get("myRank"), round_meta.get("participants")
            ),
        },
        "battles": detail.get("battles") or [],
    }


def export_round(
    page,
    round_meta: dict,
    row_index: int,
    out_dir: Path,
    submission_lookup: dict[int, dict] | None = None,
) -> dict:
    click_round_row(page, row_index)
    try:
        detail = page.evaluate(_READ_ROUND_DETAIL, round_meta.get("submissionId"))
        doc = build_round_doc(round_meta, detail, submission_lookup)
        battles = doc["battles"]

        print(f"  {doc['round']} - 대전 {len(battles)}개")
        for idx, battle in enumerate(battles, 1):
            bid = battle["battle_id"]
            try:
                battle_detail = read_battle_detail(page, bid)
                battle.update(battle_detail)
                print(f"    [{idx}/{len(battles)}] OK  battle {bid}")
            except Exception as e:
                battle["detail_error"] = str(e)
                print(f"    [{idx}/{len(battles)}] ERR battle {bid}: {e}")

        round_number = doc["round_number"] or round_meta.get("id")
        out_path = out_dir / f"round_{round_number}.json"
        out_path.write_text(json.dumps(doc, ensure_ascii=False, indent=2) + "\n")
        try:
            doc["_output_file"] = str(out_path.relative_to(ROOT))
        except ValueError:
            doc["_output_file"] = str(out_path)
        return doc
    finally:
        back_to_round_list(page)


def parse_args():
    parser = argparse.ArgumentParser(
        description="NYPC 2026 중간평가 상세 정보를 라운드별 JSON으로 저장합니다."
    )
    parser.add_argument(
        "--round",
        type=int,
        dest="round_number",
        help="특정 중간평가 번호만 저장합니다. 예: --round 27",
    )
    parser.add_argument(
        "--limit",
        type=int,
        help="최신 완료 라운드부터 지정한 개수만 저장합니다.",
    )
    parser.add_argument(
        "--out-dir",
        type=Path,
        default=OUT_DIR,
        help=f"출력 디렉터리 (기본값: {OUT_DIR})",
    )
    parser.add_argument(
        "--headless",
        action="store_true",
        help="브라우저를 headless 모드로 실행합니다.",
    )
    parser.add_argument(
        "--refresh-submissions-only",
        action="store_true",
        help="배틀 상세 재수집 없이 기존 round JSON의 제출 시각/결과/노트만 갱신합니다.",
    )
    return parser.parse_args()


def main():
    args = parse_args()
    out_dir = args.out_dir
    if not out_dir.is_absolute():
        out_dir = (Path.cwd() / out_dir).resolve()
    out_dir.mkdir(parents=True, exist_ok=True)

    nypc_id = os.environ.get("NYPC_ID")
    nypc_pw = os.environ.get("NYPC_PW")

    with sync_playwright() as p:
        browser = p.chromium.launch(headless=args.headless)
        page = browser.new_page()

        login(page, nypc_id, nypc_pw)
        open_eval_tab(page)

        rounds = page.evaluate(_READ_ROUNDS) or []
        mapped = [
            r for r in rounds
            if r.get("status") == "finished" and r.get("submissionId") is not None
            and is_main_round(r.get("title"))
        ]
        mapped.sort(key=lambda r: r["id"], reverse=True)
        targets = list(enumerate(mapped))

        if args.round_number is not None:
            targets = [
                (row_index, r) for row_index, r in targets
                if parse_round_number(r.get("title")) == args.round_number
            ]
        if args.limit is not None:
            targets = targets[:args.limit]

        target_submission_ids = {
            r["submissionId"] for _, r in targets
            if r.get("submissionId") is not None
        }
        submission_lookup = load_submission_lookup(page, target_submission_ids)

        if args.refresh_submissions_only:
            changed = update_existing_round_files(out_dir, targets, submission_lookup)
            print(f"\n제출 메타 갱신 완료: {len(changed)}개 파일 변경")
            for rn, old_sid, sid, note in changed:
                print(f"  round_{rn}: {old_sid} -> {sid}  note={note!r}")
            browser.close()
            return

        open_eval_tab(page)

        print(f"완료된 중간평가 {len(targets)}개 export 시작")

        exported = []
        for row_index, round_meta in targets:
            try:
                doc = export_round(page, round_meta, row_index, out_dir, submission_lookup)
                exported.append(doc)
            except Exception as e:
                print(f"  ERR {round_meta.get('title')}: {e}")
                try:
                    if page.locator("text=돌아가기").is_visible():
                        back_to_round_list(page)
                except Exception:
                    pass

        index = {
            "description": "NYPC 2026 중간평가 상세 JSON export index",
            "generated_at": datetime.now(KST).isoformat(),
            "round_count": len(exported),
            "rounds": [
                {
                    "round": d.get("round"),
                    "round_number": d.get("round_number"),
                    "round_id": d.get("round_id"),
                    "round_time_kst": d.get("round_time_kst"),
                    "submission_id": (d.get("submission") or {}).get("id"),
                    "rating": (d.get("result") or {}).get("rating"),
                    "rank": (d.get("result") or {}).get("rank"),
                    "participants": (d.get("result") or {}).get("participants"),
                    "battle_count": len(d.get("battles") or []),
                    "output_file": d.get("_output_file"),
                }
                for d in exported
            ],
        }
        (out_dir / "index.json").write_text(
            json.dumps(index, ensure_ascii=False, indent=2) + "\n"
        )

        print(f"\n생성 완료: {len(exported)}개 라운드 -> {out_dir}")
        print("  index.json")
        for d in exported:
            print(f"  {d['_output_file']}")

        browser.close()


if __name__ == "__main__":
    main()
