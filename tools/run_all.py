#!/usr/bin/env python3
"""
NYPC 2026 전체 파이프라인 오케스트레이터

세 스크립트를 순서대로 실행한다:
  1. download_all_logs.py        — 중간평가 배틀 로그 다운로드        → logs/
  2. download_all_submissions.py — '모든 제출 목록' 제출 코드 다운로드 → submissions/
  3. build_round_mapping.py      — 라운드 ↔ 제출 파일 매핑 생성       → round_submission_mapping.{json,csv}

순서가 중요하다: 매핑(3)은 다운로드된 submissions/(2)와 결합하므로 반드시 뒤에 둔다.
각 스크립트는 자체적으로 로그인(NYPC_ID/NYPC_PW 자동 또는 수동)을 수행한다.

실행:
  pip install playwright python-dotenv && playwright install chromium
  NYPC_ID=아이디 NYPC_PW=비밀번호 python3 run_all.py

  # 특정 단계만: 인자로 스크립트 번호(1~3)를 넘긴다
  python3 run_all.py 2 3       # 제출 다운로드 + 매핑만
"""

import subprocess
import sys
import time
from pathlib import Path

BASE = Path(__file__).parent

STEPS = [
    ("배틀 로그 다운로드", "download_all_logs.py"),
    ("제출 코드 다운로드", "download_all_submissions.py"),
    ("라운드 ↔ 제출 매핑 생성", "build_round_mapping.py"),
]


def run_step(num, label, script):
    print("\n" + "=" * 70)
    print(f"[{num}/{len(STEPS)}] {label}  ({script})")
    print("=" * 70, flush=True)
    t0 = time.time()
    # 같은 파이썬 인터프리터로, 실시간 출력이 보이도록 그대로 상속
    result = subprocess.run([sys.executable, str(BASE / script)], cwd=str(BASE))
    dt = time.time() - t0
    if result.returncode != 0:
        print(f"\n✗ [{num}] '{script}' 실패 (exit {result.returncode}, {dt:.0f}s)")
        return False
    print(f"\n✓ [{num}] '{script}' 완료 ({dt:.0f}s)")
    return True


def main():
    # 인자로 단계 번호를 받으면 해당 단계만 실행 (기본: 전체)
    args = sys.argv[1:]
    if args:
        try:
            selected = sorted({int(a) for a in args})
        except ValueError:
            print(f"사용법: python3 run_all.py [1..{len(STEPS)} ...]")
            sys.exit(2)
    else:
        selected = list(range(1, len(STEPS) + 1))

    print(f"실행 단계: {selected}")
    start = time.time()
    for num in selected:
        if not (1 <= num <= len(STEPS)):
            print(f"단계 번호 무시: {num} (1~{len(STEPS)} 범위 밖)")
            continue
        label, script = STEPS[num - 1]
        if not run_step(num, label, script):
            print("\n파이프라인 중단.")
            sys.exit(1)

    print("\n" + "=" * 70)
    print(f"전체 파이프라인 완료 ({time.time() - start:.0f}s)")
    print("  logs/  submissions/  round_submission_mapping.{json,csv}")
    print("=" * 70)


if __name__ == "__main__":
    main()
