#!/usr/bin/env bash
#
# roundrobin.sh — 폴더 안의 봇들을 전부 맞붙이고 순위표를 낸다.
#
#   tools/roundrobin.sh <폴더> [판수] [match.sh 추가옵션...]
#
#   <폴더>   .cpp/.c/.py 를 봇으로 본다. 하위 폴더는 안 본다.
#   [판수]   각 쌍당 판수. 기본 105. **35의 배수**여야 맵 스윕이 균등하다.
#
# 예시:
#   tools/roundrobin.sh ~/shared/TEAM/round_6_1215
#   tools/roundrobin.sh ~/shared/TEAM/round_6_1215 210
#   tools/roundrobin.sh ~/shared/TEAM/round_6_1215 105 -j 8
#
# 기본으로 --map-sweep 을 켜서 N=181..249 전 구간에 균등하게 깐다.
# 내용이 같은 파일(복사본)은 자동으로 한쪽만 남긴다 — 같은 봇끼리 붙여봐야
# 50:50 만 나오고 시간만 쓴다. 남긴 쪽과 버린 쪽은 표에 적어 준다.
#
set -euo pipefail

SHARED="${NYPC_SHARED:-/srv/nypc}"
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
MATCH="$HERE/match.sh"
JOBS_DEFAULT="${JOBS:-6}"

die() { echo "error: $*" >&2; exit 1; }
usage() { sed -n '3,20p' "$0" | sed 's/^# \{0,1\}//' >&2; exit 1; }

[ $# -ge 1 ] || usage
case "$1" in -h|--help) usage ;; esac

DIR="$1"; shift
[ -d "$DIR" ] || die "폴더가 없습니다: $DIR"
DIR="$(cd "$DIR" && pwd)"

GAMES=105
if [ $# -ge 1 ] && [[ "$1" =~ ^[0-9]+$ ]]; then GAMES="$1"; shift; fi
EXTRA=("$@")

# -j 가 없으면 기본값을 넣는다
has_j=0
for a in ${EXTRA[@]+"${EXTRA[@]}"}; do [ "$a" = "-j" ] && has_j=1; done
[ "$has_j" = 1 ] || EXTRA+=(-j "$JOBS_DEFAULT")

# ── 봇 수집 + 중복 제거 ──────────────────────────────────────────────────
mapfile -t FILES < <(find "$DIR" -maxdepth 1 -type f \
  \( -name '*.cpp' -o -name '*.cc' -o -name '*.cxx' -o -name '*.c' -o -name '*.py' \) \
  | sort)
[ "${#FILES[@]}" -ge 2 ] || die "봇이 2개 미만입니다 ($DIR)"

declare -A SEEN_BY_SUM
BOTS=(); DUPS=()
for f in "${FILES[@]}"; do
  sum="$(md5sum "$f" | cut -d' ' -f1)"
  name="$(basename "${f%.*}")"
  if [ -n "${SEEN_BY_SUM[$sum]:-}" ]; then
    DUPS+=("$name = ${SEEN_BY_SUM[$sum]}")
  else
    SEEN_BY_SUM[$sum]="$name"
    BOTS+=("$name")
  fi
done

echo "폴더:   $DIR"
echo "봇:     ${#BOTS[@]}개 — ${BOTS[*]}"
if [ "${#DUPS[@]}" -gt 0 ]; then
  echo "중복:   ${#DUPS[@]}개 제외 (내용 동일)"
  for d in "${DUPS[@]}"; do echo "          $d"; done
fi
n=${#BOTS[@]}
pairs=$(( n * (n - 1) / 2 ))
echo "대전:   ${pairs}쌍 × ${GAMES}판 = $(( pairs * GAMES ))판"
if [ $(( GAMES % 35 )) -ne 0 ]; then
  echo "경고:   판수가 35의 배수가 아니라 맵 크기 분포가 균등하지 않습니다"
fi
echo

export BOT_PATH="$DIR"

declare -A W L D
RESULT_ROWS=()

for ((i = 0; i < n; i++)); do
  for ((j = i + 1; j < n; j++)); do
    A="${BOTS[$i]}"; B="${BOTS[$j]}"
    echo "── $A  vs  $B ──────────────────────────────"
    out="$("$MATCH" "$GAMES" "$A" "$B" --map-sweep ${EXTRA[@]+"${EXTRA[@]}"} 2>/dev/null || true)"
    tsv="$(printf '%s\n' "$out" | awk '/판별 결과/{print $NF}')"
    if [ -z "$tsv" ] || [ ! -f "$tsv" ]; then
      echo "  (결과 파일을 못 찾음 — 건너뜀)"; echo; continue
    fi
    read -r aw bw dr < <(awk -F'\t' -v a="$A" -v b="$B" '
      NR>1 { if ($6==a) x++; else if ($6==b) y++; else if ($6=="DRAW") z++ }
      END { print x+0, y+0, z+0 }' "$tsv")
    dec=$(( aw + bw ))
    rate="n/a"; se="n/a"
    if [ "$dec" -gt 0 ]; then
      rate="$(awk "BEGIN{printf \"%.1f\", 100*$aw/$dec}")"
      se="$(awk "BEGIN{p=$aw/$dec; printf \"%.1f\", 100*sqrt(p*(1-p)/$dec)}")"
    fi
    printf '  %s %d승 / %s %d승 / 무 %d   → %s 판정승률 %s%% ±%s%%p\n' \
      "$A" "$aw" "$B" "$bw" "$dr" "$A" "$rate" "$se"
    echo
    W[$A]=$(( ${W[$A]:-0} + aw )); L[$A]=$(( ${L[$A]:-0} + bw )); D[$A]=$(( ${D[$A]:-0} + dr ))
    W[$B]=$(( ${W[$B]:-0} + bw )); L[$B]=$(( ${L[$B]:-0} + aw )); D[$B]=$(( ${D[$B]:-0} + dr ))
    RESULT_ROWS+=("$(printf '%s\t%s\t%s\t%s\t%s\t%s\t%s' "$A" "$B" "$aw" "$bw" "$dr" "$rate" "$se")")
  done
done

# ── 순위표 ───────────────────────────────────────────────────────────────
echo "================ 종합 순위 ($((pairs * GAMES))판) ================"
printf '  %-40s %5s %5s %5s  %s\n' "봇" "승" "무" "패" "판정승률"
for b in "${BOTS[@]}"; do
  w=${W[$b]:-0}; l=${L[$b]:-0}; d=${D[$b]:-0}; dec=$(( w + l ))
  r=0; [ "$dec" -gt 0 ] && r="$(awk "BEGIN{printf \"%.1f\", 100*$w/$dec}")"
  printf '%s\t%s\t%s\t%s\t%s\n' "$r" "$b" "$w" "$d" "$l"
done | sort -rn | while IFS=$'\t' read -r r b w d l; do
  printf '  %-40s %5s %5s %5s  %s%%\n' "$b" "$w" "$d" "$l" "$r"
done

echo
echo "쌍별 상세:"
printf '  %-30s %-30s %5s %5s %5s %8s\n' "A" "B" "A승" "B승" "무" "A승률"
for row in ${RESULT_ROWS[@]+"${RESULT_ROWS[@]}"}; do
  IFS=$'\t' read -r a b aw bw dr rate se <<< "$row"
  printf '  %-30s %-30s %5s %5s %5s %7s%%\n' "$a" "$b" "$aw" "$bw" "$dr" "$rate"
done
echo
echo "전체 기록: $SHARED/runs/matches.tsv"
