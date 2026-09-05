#!/usr/bin/env bash
# corepool.sh — 공유 EC2의 CPU 코어 세마포어.
#
# 아무 명령이나 감쌉니다. 대회 문제나 채점기와는 무관한 범용 도구입니다.
#
#   tools/corepool.sh 6 -- ./whatever --flags     # 코어 6개 확보 후 실행
#   tools/corepool.sh --status                    # 현재 점유 현황
#   NYPC_COREPOOL_TIMEOUT=600 tools/corepool.sh 8 -- ...   # 대기 상한(초)
#
# 왜 필요한가: 16 vCPU를 4명이 나눠 쓰는데 각자 병렬로 돌리면 초과 구독이 되고,
# 턴 시간 제한이 있는 대국은 가짜 타임아웃을 냅니다. 그러면 승률이 봇 실력이
# 아니라 그 순간 서버 부하를 재게 됩니다 — 조용히 틀리므로 제일 위험합니다.
set -euo pipefail

POOL_DIR="${NYPC_COREPOOL_DIR:-/srv/nypc/runs/.corepool}"
POOL_SIZE="${NYPC_COREPOOL_SIZE:-14}"      # 16코어 중 2개는 셸/에디터/git 몫
TIMEOUT="${NYPC_COREPOOL_TIMEOUT:-1800}"

init_pool() {
  mkdir -p "$POOL_DIR"
  chmod 1777 "$POOL_DIR" 2>/dev/null || true
  for i in $(seq 1 "$POOL_SIZE"); do
    [[ -e "$POOL_DIR/slot.$i" ]] || : > "$POOL_DIR/slot.$i"
    chmod 666 "$POOL_DIR/slot.$i" 2>/dev/null || true
  done
}

show_status() {
  init_pool
  local held=0 free=0
  for i in $(seq 1 "$POOL_SIZE"); do
    if flock -n "$POOL_DIR/slot.$i" true 2>/dev/null; then
      free=$((free + 1))
    else
      held=$((held + 1))
    fi
  done
  printf 'corepool %s: %d / %d 사용 중, %d 여유\n' "$POOL_DIR" "$held" "$POOL_SIZE" "$free"
  if [[ "$held" -gt 0 ]] && command -v fuser >/dev/null 2>&1; then
    echo "점유 프로세스:"
    { fuser "$POOL_DIR"/slot.* 2>/dev/null || true; } | tr ' ' '\n' | sed '/^$/d' | sort -u \
      | while read -r pid; do
          ps -o pid=,user=,etime=,args= -p "$pid" 2>/dev/null | cut -c1-110 || true
        done
  fi
  return 0
}

usage() {
  sed -n '2,12p' "$0" | sed 's/^# \{0,1\}//'
  exit 2
}

[[ $# -eq 0 ]] && usage
if [[ "$1" == "--status" ]]; then show_status; exit 0; fi
if [[ "$1" == "-h" || "$1" == "--help" ]]; then usage; fi

WANT="$1"; shift
[[ "$WANT" =~ ^[0-9]+$ ]] || { echo "코어 수는 정수여야 합니다: $WANT" >&2; exit 2; }
[[ "$WANT" -ge 1 ]] || { echo "코어 수는 1 이상이어야 합니다" >&2; exit 2; }
if [[ "$WANT" -gt "$POOL_SIZE" ]]; then
  echo "요청 $WANT 코어 > 풀 크기 $POOL_SIZE. 이런 작업은 여기서 돌리지 말고 jinsu 또는 jinyoung에게 문의하세요." >&2
  exit 2
fi
[[ "${1:-}" == "--" ]] || { echo "명령 앞에 -- 를 넣으세요" >&2; usage; }
shift
[[ $# -gt 0 ]] || { echo "실행할 명령이 없습니다" >&2; exit 2; }

init_pool

declare -a HELD_FDS=()
release_all() {
  for fd in "${HELD_FDS[@]:-}"; do [[ -n "$fd" ]] && eval "exec ${fd}>&-" 2>/dev/null || true; done
  HELD_FDS=()
}
trap release_all EXIT INT TERM

deadline=$(( $(date +%s) + TIMEOUT ))
announced=0
while :; do
  release_all
  for i in $(seq 1 "$POOL_SIZE"); do
    [[ "${#HELD_FDS[@]}" -ge "$WANT" ]] && break
    exec {fd}>"$POOL_DIR/slot.$i" || continue
    if flock -n "$fd"; then
      HELD_FDS+=("$fd")
    else
      eval "exec ${fd}>&-" 2>/dev/null || true
    fi
  done
  [[ "${#HELD_FDS[@]}" -ge "$WANT" ]] && break
  if [[ "$announced" -eq 0 ]]; then
    printf 'corepool: %d 코어 대기 중 (현재 %d 확보, 풀 %d)...\n' \
      "$WANT" "${#HELD_FDS[@]}" "$POOL_SIZE" >&2
    announced=1
  fi
  if [[ "$(date +%s)" -ge "$deadline" ]]; then
    release_all
    echo "corepool: ${TIMEOUT}초 안에 ${WANT} 코어를 확보하지 못했습니다." >&2
    echo "  --status 로 누가 쓰는지 보거나, jinsu 또는 jinyoung에게 문의하세요." >&2
    exit 75
  fi
  sleep 2
done

"$@"
