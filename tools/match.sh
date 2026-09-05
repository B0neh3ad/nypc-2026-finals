#!/usr/bin/env bash
#
# match.sh — 봇 둘을 n번 붙이고 승패를 집계한다. 컴파일은 versus.sh 가 처리한다.
#
# 사용법:
#   tools/match.sh <games> <A> <B> [-j N] [--map-size N [K]] [testing-tool 옵션...]
#
#   <games>          대결 횟수
#   <A>,<B>          봇 (이름/파일명/경로, versus.sh 와 같은 규칙. 별칭 `sample`)
#   -j N             동시 실행 판 수 (기본 1). 2 이상이면 corepool.sh 로 코어를
#                    먼저 확보한 뒤 돌립니다 — 직접 & 로 띄우지 마세요.
#   --map-sweep      N 을 규정 전 구간(181~249 홀수 35개)에 균등하게 깔아 돌린다.
#                    K 는 각 N 에서 허용되는 홀수 범위 중 시드로 고른다.
#                    --map-size 와 함께 쓸 수 없다.
#   --map-size N [K] 실제 맵 크기 고정. K 를 생략하면 N 만 고정하고 유효한 K 를
#                    매 판 시드 기반으로 고릅니다. N/K 는 **실제 값**입니다
#                    (testing-tool 의 --NP/--KP 는 그 절반).
#
# 공정성을 위해 매 판 좌/우를 번갈아 배치합니다.
# 시드는 매 판 랜덤이고, SEED_BASE 를 주면 재현 가능합니다.
#
# 예시:
#   tools/match.sh 100 mybot sample
#   tools/match.sh 1000 mybot sample -j 6
#   tools/match.sh 200 mybot other --map-size 201
#   tools/match.sh 200 mybot other --map-size 201 15
#   SEED_BASE=1000 tools/match.sh 50 mybot sample     # 시드 1000,1001,... 재현
#
# 왜 -j 에 corepool 이 붙어 있나: 16코어를 넷이 나눠 쓰는데 한 판이 1초도 안 걸려서
# 순식간에 과점유가 납니다. 그러면 턴 100ms 제한에 가짜 타임아웃이 나고, 승률이
# 봇 실력이 아니라 그 순간 서버 부하를 재게 됩니다 (docs/COLLAB.md).
#
set -euo pipefail

SHARED="${NYPC_SHARED:-/srv/nypc}"
SELF="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/$(basename "${BASH_SOURCE[0]}")"
VERSUS="${NYPC_VERSUS:-$(dirname "$SELF")/versus.sh}"
COREPOOL="$SHARED/tools/corepool.sh"

die() { echo "error: $*" >&2; exit 1; }
is_pos_int() { [[ "$1" =~ ^[0-9]+$ ]] && [ "$1" -ge 1 ]; }

usage() { sed -n '3,30p' "$0" | sed 's/^# \{0,1\}//' >&2; exit 1; }

# ── 내부 모드: 한 판만 돌리고 "i seed N/K winner" 한 줄을 파일로 남긴다 ──────
if [ "${1:-}" = "--__game" ]; then
  shift
  i="$1"; seed="$2"; left="$3"; right="$4"
  left_label="$5"; right_label="$6"; outdir="$7"; np="$8"; kp="$9"
  shift 9
  log="$outdir/g$i.log"
  cmd=("$VERSUS" "$left" "$right" --seed "$seed" -l "$log")
  [ -n "$np" ] && cmd+=(--NP "$np" --KP "$kp")
  [ "$#" -eq 0 ] || cmd+=("$@")
  result_line="$(MATCH_QUIET=1 "${cmd[@]}" 2>/dev/null | grep '^RESULT' | tail -1 || true)"
  map_size="$(awk '/^MAP$/ { getline; print $1 "/" $2; exit }' "$log" 2>/dev/null || true)"
  [ -n "$map_size" ] || map_size="?"
  case "$(awk '{print $2}' <<<"$result_line")" in
    LEFT_WIN)  w="$left_label" ;;
    RIGHT_WIN) w="$right_label" ;;
    DRAW)      w="DRAW" ;;
    *)         w="ERR" ;;
  esac
  printf '%s\t%s\t%s\t%s\t%s\t%s\n' "$i" "$seed" "$map_size" "$left" "$right" "$w" \
    > "$outdir/r$i.tsv"
  # 로그는 즉시 버린다 — 수천 판이면 수백 MB (docs/COLLAB.md).
  # KEEP_LOGS=1 이면 이번 실행 폴더에 남긴다.
  if [ -n "${KEEP_LOGS:-}" ] && [ -n "${MATCH_LOGDIR:-}" ]; then
    mv -f "$log" "$MATCH_LOGDIR/" 2>/dev/null || true
  else
    rm -f "$log"
  fi
  exit 0
fi

[ $# -ge 3 ] || usage
case "${1:-}" in -h|--help) usage ;; esac
[ -x "$VERSUS" ] || die "versus.sh 가 없거나 실행 권한이 없습니다: $VERSUS"

GAMES="$1"; A="$2"; B="$3"; shift 3
is_pos_int "$GAMES" || die "games 는 1 이상의 정수여야 합니다"
GAMES=$((10#$GAMES))

JOBS="${JOBS:-1}"
SWEEP="${MAP_SWEEP:-0}"
MAP_ACTUAL_N="${MAP_N:-}"
MAP_ACTUAL_K="${MAP_K:-}"
TOOL_ARGS=()

while [ "$#" -gt 0 ]; do
  case "$1" in
    -j) [ "$#" -ge 2 ] || die "-j 뒤에 숫자가 필요합니다"; JOBS="$2"; shift 2 ;;
    --map-sweep) SWEEP=1; shift ;;
    --map-size)
      [ "$#" -ge 2 ] || die "--map-size 뒤에 실제 N 이 필요합니다"
      MAP_ACTUAL_N="$2"; MAP_ACTUAL_K=""; shift 2
      if [ "$#" -gt 0 ] && is_pos_int "$1"; then MAP_ACTUAL_K="$1"; shift; fi ;;
    --) shift; TOOL_ARGS+=("$@"); break ;;
    *)  TOOL_ARGS+=("$1"); shift ;;
  esac
done
is_pos_int "$JOBS" || die "-j 는 1 이상의 정수여야 합니다"

# ── 맵 크기: 본선 규칙 ────────────────────────────────────────────────────
#   181 <= N <= 249 (홀수),  ceil(sqrt(N)-1) <= K <= floor(sqrt(N)+4) (홀수)
MAP_NP=""; MAP_KP=""; MAP_K_LO=""; MAP_K_HI=""
if [ -n "$MAP_ACTUAL_N$MAP_ACTUAL_K" ]; then
  [ -n "$MAP_ACTUAL_N" ] || die "MAP_K 를 줄 때는 MAP_N 도 주세요"
  is_pos_int "$MAP_ACTUAL_N" || die "맵 N 은 양의 정수여야 합니다"
  MAP_ACTUAL_N=$((10#$MAP_ACTUAL_N))
  (( MAP_ACTUAL_N % 2 == 1 )) || die "맵 N 은 홀수여야 합니다"
  (( MAP_ACTUAL_N >= 181 && MAP_ACTUAL_N <= 249 )) || die "맵 N 은 [181, 249] 이어야 합니다"

  read -r MAP_K_LO MAP_K_HI < <(awk -v n="$MAP_ACTUAL_N" 'BEGIN{
    r = sqrt(n)
    lo = r - 1; lo = (lo == int(lo)) ? lo : int(lo) + 1     # ceil
    hi = int(r + 4)                                          # floor
    if (lo % 2 == 0) lo += 1
    if (hi % 2 == 0) hi -= 1
    print lo, hi
  }')

  MAP_NP=$(( (MAP_ACTUAL_N - 1) / 2 ))
  if [ -n "$MAP_ACTUAL_K" ]; then
    MAP_ACTUAL_K=$((10#$MAP_ACTUAL_K))
    (( MAP_ACTUAL_K % 2 == 1 )) || die "맵 K 는 홀수여야 합니다"
    (( MAP_ACTUAL_K >= MAP_K_LO && MAP_ACTUAL_K <= MAP_K_HI )) \
      || die "N=$MAP_ACTUAL_N 이면 K 는 홀수이고 [$MAP_K_LO, $MAP_K_HI] 이어야 합니다"
    MAP_KP=$(( (MAP_ACTUAL_K - 1) / 2 ))
  fi
fi

if [ "$SWEEP" = 1 ] && [ -n "$MAP_NP" ]; then
  die "--map-sweep 와 --map-size 는 함께 쓸 수 없습니다"
fi

# 스윕: NP 90~124 (N=181~249 홀수) 각각의 허용 K 범위를 미리 구해 둔다.
declare -A SW_KLO SW_KHI
SW_N=0
if [ "$SWEEP" = 1 ]; then
  while read -r np klo khi; do
    SW_KLO[$np]=$klo; SW_KHI[$np]=$khi
  done < <(awk 'BEGIN{
    for (np=90; np<=124; np++) {
      n = 2*np+1; r = sqrt(n)
      lo = r - 1; lo = (lo == int(lo)) ? lo : int(lo) + 1
      hi = int(r + 4)
      if (lo % 2 == 0) lo += 1
      if (hi % 2 == 0) hi -= 1
      print np, lo, hi
    }}')
  SW_N=35
fi

OUT_DIR="$(mktemp -d "${TMPDIR:-/tmp}/match.XXXXXX")"
trap 'rm -rf "$OUT_DIR"' EXIT

# 결과 적재 위치. runs/ 는 .gitignore 에 있어서 2분 스냅샷에 안 들어갑니다.
RUNS="${NYPC_RUNS:-$SHARED/runs}"
RUNID="$(date +%Y%m%dT%H%M%S)-$USER-$$"

# ── 판별 인자 목록을 만들어 xargs 로 흘린다 ───────────────────────────────
build_jobs() {
  local i seed left right ll rl kp k_count map_k
  for ((i=1; i<=GAMES; i++)); do
    if [ -n "${SEED_BASE:-}" ]; then seed=$((SEED_BASE + i - 1)); else seed=$RANDOM$RANDOM; fi
    if (( i % 2 == 1 )); then left="$A"; right="$B"; ll="A"; rl="B"
    else                     left="$B"; right="$A"; ll="B"; rl="A"; fi
    kp=""; np="$MAP_NP"
    if [ "$SWEEP" = 1 ]; then
      np=$(( 90 + (i - 1) % SW_N ))
      klo=${SW_KLO[$np]}; khi=${SW_KHI[$np]}
      k_count=$(( (khi - klo) / 2 + 1 ))
      map_k=$(( klo + 2 * ((10#$seed) % k_count) ))
      kp=$(( (map_k - 1) / 2 ))
    elif [ -n "$MAP_NP" ]; then
      kp="$MAP_KP"
      if [ -z "$kp" ]; then
        k_count=$(( (MAP_K_HI - MAP_K_LO) / 2 + 1 ))
        map_k=$(( MAP_K_LO + 2 * ((10#$seed) % k_count) ))
        kp=$(( (map_k - 1) / 2 ))
      fi
    fi
    printf '%s\0%s\0%s\0%s\0%s\0%s\0%s\0%s\0%s\0' \
      "$i" "$seed" "$left" "$right" "$ll" "$rl" "$OUT_DIR" "$np" "$kp"
    for a in ${TOOL_ARGS[@]+"${TOOL_ARGS[@]}"}; do printf '%s\0' "$a"; done
  done
}

run_all() {
  local n_args=$(( 9 + ${#TOOL_ARGS[@]} ))
  build_jobs | xargs -0 -n "$n_args" -P "$JOBS" "$SELF" --__game
}

echo ">> $GAMES games,  A=$A  B=$B,  -j $JOBS" >&2
if [ "$JOBS" -gt 1 ] && [ -x "$COREPOOL" ] && [ -z "${MATCH_IN_POOL:-}" ]; then
  echo ">> corepool 로 코어 $JOBS 개 확보 중..." >&2
  export MATCH_IN_POOL=1
  # 파싱하면서 소비한 맵 크기를 자식에게 넘긴다 (자식이 env 로 다시 읽음).
  # 이게 없으면 -j 2 이상에서 --map-size 가 조용히 무시됩니다.
  if [ -n "$MAP_ACTUAL_N" ]; then export MAP_N="$MAP_ACTUAL_N"; fi
  if [ -n "$MAP_ACTUAL_K" ]; then export MAP_K="$MAP_ACTUAL_K"; fi
  export MAP_SWEEP="$SWEEP"
  exec "$COREPOOL" "$JOBS" -- "$SELF" "$GAMES" "$A" "$B" -j "$JOBS" \
       ${TOOL_ARGS[@]+"${TOOL_ARGS[@]}"}
fi
[ "$JOBS" -eq 1 ] || [ -x "$COREPOOL" ] || \
  echo ">> 경고: corepool.sh 가 없어 그냥 병렬 실행합니다" >&2

if [ -n "${KEEP_LOGS:-}" ]; then
  MATCH_LOGDIR="$RUNS/logs/$RUNID"; mkdir -p "$MATCH_LOGDIR"; export MATCH_LOGDIR
fi

# 팬아웃 전에 한 번만 컴파일한다 (병렬 컴파일 경합 방지).
VERSUS_BUILD_ONLY=1 MATCH_QUIET=1 "$VERSUS" "$A" "$B" \
  || die "컴파일 실패 - 위 메시지를 보세요"

run_all

# ── 집계 ──────────────────────────────────────────────────────────────────
a_wins=0; b_wins=0; draws=0; errors=0

mkdir -p "$RUNS/games" 2>/dev/null || true
GAMES_TSV="$RUNS/games/$RUNID.tsv"
printf 'game\tseed\tmap\tleft\tright\twinner\n' > "$GAMES_TSV" 2>/dev/null || GAMES_TSV=""

printf '%-5s %-12s %-9s %-14s %-14s %s\n' "game" "seed" "map(N/K)" "LEFT" "RIGHT" "winner"
printf '%s\n' "---------------------------------------------------------------------"
for ((i=1; i<=GAMES; i++)); do
  [ -f "$OUT_DIR/r$i.tsv" ] || { errors=$((errors+1)); continue; }
  IFS=$'\t' read -r gi seed msize left right w < "$OUT_DIR/r$i.tsv"
  case "$w" in
    A)    a_wins=$((a_wins+1)); wname="$A" ;;
    B)    b_wins=$((b_wins+1)); wname="$B" ;;
    DRAW) draws=$((draws+1));   wname="DRAW" ;;
    *)    errors=$((errors+1)); wname="ERR" ;;
  esac
  printf '%-5s %-12s %-9s %-14s %-14s %s\n' "$gi" "$seed" "$msize" "$left" "$right" "$wname"
  [ -z "$GAMES_TSV" ] || printf '%s\t%s\t%s\t%s\t%s\t%s\n' \
    "$gi" "$seed" "$msize" "$left" "$right" "$wname" >> "$GAMES_TSV"
done

total=$((a_wins + b_wins + draws))
pct() { [ "$total" -gt 0 ] && awk "BEGIN{printf \"%.1f\", $1*100/$total}" || echo "0.0"; }
# 승률의 표준오차(무승부 제외 이항). 표본이 작으면 차이라고 부를 수 없습니다.
se() { awk -v a="$a_wins" -v b="$b_wins" 'BEGIN{
  n=a+b; if(n==0){print "n/a"; exit} p=a/n; printf "%.1f", 100*sqrt(p*(1-p)/n) }'; }

echo
echo "================ SUMMARY ($GAMES games) ================"
printf '  %-14s %4d wins  (%s%%)\n' "$A" "$a_wins" "$(pct "$a_wins")"
printf '  %-14s %4d wins  (%s%%)\n' "$B" "$b_wins" "$(pct "$b_wins")"
printf '  %-14s %4d\n' "DRAW" "$draws"
[ "$errors" -gt 0 ] && printf '  %-14s %4d\n' "ERROR" "$errors"
printf '  %-14s ±%s%%p (무승부 제외 승률 기준)\n' "표준오차" "$(se)"

if [ "$a_wins" -gt "$b_wins" ]; then echo ">> WINNER: $A"
elif [ "$b_wins" -gt "$a_wins" ]; then echo ">> WINNER: $B"
else echo ">> TIE"; fi

# ── 적재: 실행 한 줄 = 한 행. 이게 나중에 grep 할 대상입니다. ─────────────
if [ -z "${NO_RECORD:-}" ]; then
  INDEX="$RUNS/matches.tsv"
  map_desc="random"
  [ "$SWEEP" = 1 ] && map_desc="sweep(N=181..249)"
  [ -z "$MAP_ACTUAL_N" ] || map_desc="N=$MAP_ACTUAL_N"
  [ -z "$MAP_ACTUAL_K" ] || map_desc="$map_desc,K=$MAP_ACTUAL_K"
  rate="$(awk -v a="$a_wins" -v b="$b_wins" 'BEGIN{n=a+b; if(n==0){print "n/a"; exit}
            printf "%.3f", a/n }')"
  if [ ! -f "$INDEX" ]; then
    printf 'when\tuser\tA\tB\tgames\tjobs\tmap\tseed_base\tA_win\tB_win\tdraw\terr\tA_rate\tse_pp\trunid\n' \
      > "$INDEX" 2>/dev/null || true
  fi
  printf '%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\n' \
    "$(date +%Y-%m-%dT%H:%M:%S)" "$USER" "$A" "$B" "$GAMES" "$JOBS" "$map_desc" \
    "${SEED_BASE:--}" "$a_wins" "$b_wins" "$draws" "$errors" "$rate" "$(se)" "$RUNID" \
    >> "$INDEX" 2>/dev/null || true

  echo
  echo "기록됨:"
  [ -z "$GAMES_TSV" ] || echo "  판별 결과  $GAMES_TSV"
  echo "  누적 색인  $INDEX"
  [ -z "${MATCH_LOGDIR:-}" ] || echo "  리플레이   $MATCH_LOGDIR"
fi
