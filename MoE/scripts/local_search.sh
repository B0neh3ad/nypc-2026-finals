#!/usr/bin/env bash
# 이 EC2 에서 돌릴 때. 반드시 corepool 로 코어를 확보한다 —
# 안 그러면 턴 100ms 제한에 가짜 타임아웃이 나서 승률이 아니라 서버 부하를 재게 된다.
#
#   scripts/local_search.sh <bucket> [jobs] [-- 추가 인자...]
set -euo pipefail
MOE_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$MOE_ROOT"
BUCKET="${1:?bucket 번호(0-4)를 주세요}"; shift || true
JOBS="${1:-6}"; [[ "${1:-}" =~ ^[0-9]+$ ]] && shift || true
[ "${1:-}" = "--" ] && shift || true
PY="${MOE_PYTHON:-$HOME/.conda/envs/moe/bin/python}"
[ -x "$PY" ] || PY="$(command -v python3)"
COREPOOL="${NYPC_SHARED:-/srv/nypc}/tools/corepool.sh"
exec "$COREPOOL" "$JOBS" -- "$PY" run_search.py --tag "${MOE_TAG:-default}" \
     search --bucket "$BUCKET" --jobs "$JOBS" "$@"
