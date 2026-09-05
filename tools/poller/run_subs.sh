#!/usr/bin/env bash
# 제출별 샘플 봇 대결 로그 다운로드. 이미 받은 제출은 건너뛴다.
set -euo pipefail
cd "$(dirname "${BASH_SOURCE[0]}")"
VENV="$HOME/nypc-2026/.venv/bin/python"
[ -x "$VENV" ] || VENV=python3
exec "$VENV" subs.py "$@"
