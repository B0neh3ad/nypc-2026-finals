#!/usr/bin/env bash
#
# match.sh — 두 제출 코드를 n번 대결시키고 승패를 집계한다.
#            컴파일은 versus.sh 가 알아서 처리한다.
#
# 사용법:
#   ./match.sh <games> <A> <B> [--map-size N [K]] [testing-tool 추가 옵션...]
#
#   <games>       대결 횟수
#   <A>,<B>       제출 코드 (번호/파일명/경로, versus.sh 와 동일한 규칙)
#   --map-size N [K]
#                 실제 맵 크기를 고정한다. K 를 생략하면 N 만 고정하고
#                 유효한 K 는 매 판 seed 기반으로 고른다. 각 판은 같은
#                 크기의 다른 랜덤 맵으로 생성된다.
#
# 공정성을 위해 매 판 좌/우 진영을 번갈아 배치한다.
# 각 판은 서로 다른 랜덤 시드로 진행된다(고정하려면 SEED_BASE 환경변수 사용).
# 맵 크기는 MAP_N/MAP_K 환경변수로도 고정할 수 있다.
#
# 예시:
#   ./match.sh 10 32148 33108
#   ./match.sh 20 32148 25006
#   ./match.sh 50 32148 33108 --map-size 75
#   ./match.sh 50 32148 33108 --map-size 75 13
#   MAP_N=75 ./match.sh 50 32148 33108
#   MAP_N=75 MAP_K=13 ./match.sh 50 32148 33108
#   SEED_BASE=1000 ./match.sh 10 32148 33108   # 시드 1000,1001,... 재현 가능
#
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
VERSUS="$ROOT/versus.sh"

die() { echo "error: $*" >&2; exit 1; }

is_positive_int() {
  [[ "$1" =~ ^[0-9]+$ ]] && [ "$1" -ge 1 ]
}

usage() {
  echo "usage: $0 <games> <A> <B> [--map-size N [K]] [testing-tool options...]" >&2
  exit 1
}

[ $# -ge 3 ] || usage
[ -x "$VERSUS" ] || die "versus.sh not found or not executable: $VERSUS"

GAMES="$1"; A="$2"; B="$3"; shift 3
is_positive_int "$GAMES" || die "games must be a positive integer"
GAMES=$((10#$GAMES))

MAP_ACTUAL_N="${MAP_N:-}"
MAP_ACTUAL_K="${MAP_K:-}"
TOOL_ARGS=()

while [ "$#" -gt 0 ]; do
  case "$1" in
    --map-size)
      [ "$#" -ge 2 ] || die "--map-size requires actual N"
      MAP_ACTUAL_N="$2"
      MAP_ACTUAL_K=""
      shift 2
      if [ "$#" -gt 0 ] && is_positive_int "$1"; then
        MAP_ACTUAL_K="$1"
        shift
      fi
      ;;
    --)
      shift
      TOOL_ARGS+=("$@")
      break
      ;;
    *)
      TOOL_ARGS+=("$1")
      shift
      ;;
  esac
done

MAP_NP=""
MAP_KP=""
MAP_K_LO=""
MAP_K_HI=""
if [ -n "$MAP_ACTUAL_N$MAP_ACTUAL_K" ]; then
  [ -n "$MAP_ACTUAL_N" ] || die "set MAP_N when setting MAP_K"
  is_positive_int "$MAP_ACTUAL_N" || die "map N must be a positive integer"
  MAP_ACTUAL_N=$((10#$MAP_ACTUAL_N))
  (( MAP_ACTUAL_N % 2 == 1 )) || die "map N must be odd"
  (( MAP_ACTUAL_N >= 51 && MAP_ACTUAL_N <= 109 )) || die "map N must be in [51, 109]"

  MAP_K_LO=$(( (3 * MAP_ACTUAL_N + 19) / 20 ))
  (( MAP_K_LO % 2 == 1 )) || MAP_K_LO=$((MAP_K_LO + 1))
  MAP_K_HI=$(( MAP_ACTUAL_N / 5 ))
  (( MAP_K_HI % 2 == 1 )) || MAP_K_HI=$((MAP_K_HI - 1))

  MAP_NP=$(( (MAP_ACTUAL_N - 1) / 2 ))
  if [ -n "$MAP_ACTUAL_K" ]; then
    is_positive_int "$MAP_ACTUAL_K" || die "map K must be a positive integer"
    MAP_ACTUAL_K=$((10#$MAP_ACTUAL_K))
    (( MAP_ACTUAL_K % 2 == 1 )) || die "map K must be odd"
    (( MAP_ACTUAL_K >= MAP_K_LO && MAP_ACTUAL_K <= MAP_K_HI )) || die "map K must be odd and in [$MAP_K_LO, $MAP_K_HI] for N=$MAP_ACTUAL_N"
    MAP_KP=$(( (MAP_ACTUAL_K - 1) / 2 ))
  fi
fi

LOG_DIR="$(mktemp -d "${TMPDIR:-/tmp}/match.XXXXXX")"
trap 'rm -rf "$LOG_DIR"' EXIT

a_wins=0; b_wins=0; draws=0; errors=0

printf '%-5s %-12s %-9s %-6s %-6s %s\n' "game" "seed" "map(N/K)" "LEFT" "RIGHT" "winner"
printf '%s\n' "------------------------------------------------------------"

for ((i=1; i<=GAMES; i++)); do
  # 시드: SEED_BASE 가 있으면 재현 가능, 없으면 랜덤
  if [ -n "${SEED_BASE:-}" ]; then
    seed=$((SEED_BASE + i - 1))
  else
    seed=$RANDOM$RANDOM
  fi

  # 진영 번갈아 배치: 홀수 판은 A=LEFT, 짝수 판은 B=LEFT
  if (( i % 2 == 1 )); then
    left="$A"; right="$B"; left_label="A"; right_label="B"
  else
    left="$B"; right="$A"; left_label="B"; right_label="A"
  fi

  log="$LOG_DIR/g$i.log"
  # versus.sh 는 마지막 줄에 "RESULT <outcome> <reason>" 를 stdout 으로 출력
  cmd=("$VERSUS" "$left" "$right" --seed "$seed" -l "$log")
  if [ -n "$MAP_NP" ]; then
    map_kp="$MAP_KP"
    if [ -z "$map_kp" ]; then
      k_count=$(( (MAP_K_HI - MAP_K_LO) / 2 + 1 ))
      map_k=$(( MAP_K_LO + 2 * ((10#$seed) % k_count) ))
      map_kp=$(( (map_k - 1) / 2 ))
    fi
    cmd+=(--NP "$MAP_NP" --KP "$map_kp")
  fi
  [ "${#TOOL_ARGS[@]}" -eq 0 ] || cmd+=("${TOOL_ARGS[@]}")
  result_line="$("${cmd[@]}" 2>/dev/null | grep '^RESULT' | tail -1)"
  map_size="$(awk '/^MAP$/ { getline; print $1 "/" $2; exit }' "$log" 2>/dev/null || true)"
  [ -n "$map_size" ] || map_size="?"

  outcome="$(awk '{print $2}' <<<"$result_line")"
  case "$outcome" in
    LEFT_WIN)  winner_label="$left_label" ;;
    RIGHT_WIN) winner_label="$right_label" ;;
    DRAW)      winner_label="DRAW" ;;
    *)         winner_label="ERR" ;;
  esac

  case "$winner_label" in
    A)    a_wins=$((a_wins+1));  wname="$A" ;;
    B)    b_wins=$((b_wins+1));  wname="$B" ;;
    DRAW) draws=$((draws+1));    wname="DRAW" ;;
    *)    errors=$((errors+1));  wname="ERR" ;;
  esac

  printf '%-5s %-12s %-9s %-6s %-6s %s\n' "$i" "$seed" "$map_size" "$left" "$right" "$wname"
done

total=$((a_wins + b_wins + draws))
pct() { [ "$total" -gt 0 ] && awk "BEGIN{printf \"%.1f\", $1*100/$total}" || echo "0.0"; }

echo
echo "================ SUMMARY ($GAMES games) ================"
printf '  %-10s %3d wins  (%s%%)\n' "$A" "$a_wins" "$(pct "$a_wins")"
printf '  %-10s %3d wins  (%s%%)\n' "$B" "$b_wins" "$(pct "$b_wins")"
printf '  %-10s %3d\n' "DRAW" "$draws"
[ "$errors" -gt 0 ] && printf '  %-10s %3d  (see logs)\n' "ERROR" "$errors"

if [ "$a_wins" -gt "$b_wins" ]; then
  echo ">> WINNER: $A"
elif [ "$b_wins" -gt "$a_wins" ]; then
  echo ">> WINNER: $B"
else
  echo ">> TIE"
fi
