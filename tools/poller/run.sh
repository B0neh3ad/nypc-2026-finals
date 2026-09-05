#!/usr/bin/env bash
# 중간평가 폴러 실행. 로그는 ~/nypc-2026/nypc-contest-poller/poller.log
set -euo pipefail
cd "$(dirname "${BASH_SOURCE[0]}")"
VENV="$HOME/nypc-2026/.venv/bin/python"
[ -x "$VENV" ] || VENV=python3
exec "$VENV" poller.py "$@"
