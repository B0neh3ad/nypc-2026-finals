#!/usr/bin/env bash
#
# versus.sh — 봇 둘을 한 판 붙인다. C/C++는 필요할 때만 자동 컴파일한다.
#
# 사용법:
#   tools/versus.sh <LEFT> <RIGHT> [testing-tool 추가 옵션...]
#
# <LEFT>, <RIGHT> 는 다음 중 아무 형태나 가능:
#   mybot            (탐색 경로에서 mybot.cpp / mybot.py ... 자동 탐색)
#   mybot.cpp        (탐색 경로 안의 파일명)
#   ~/bots/foo.py    (직접 경로)
#   sample           (주최측 파이썬 샘플 봇의 별칭)
#
# 탐색 경로는 BOT_PATH (콜론 구분, 기본 `.:/srv/nypc/TEAM:$HOME/bots:/srv/nypc/bots`).
# 팀원 홈도 읽기는 되므로 /home/<id>/bots 를 직접 적어도 됩니다.
#
# 예시:
#   tools/versus.sh mybot sample                 # 랜덤 맵
#   tools/versus.sh mybot sample --seed 42       # 시드 고정
#   tools/versus.sh mybot sample -l /tmp/log.txt # 로그 파일 지정
#   tools/versus.sh mybot sample --NP 100 --KP 8 # 맵 크기 지정 (절반 값!)
#
# 컴파일은 tools/build.sh 를 씁니다 — 채점기와 같은 코드 생성(-march=znver4).
# 손으로 g++ 를 치면 다른 코드가 나와서 시간 측정이 무의미해집니다
# (docs/TOOLCHAIN.md "함정 둘").
#
set -euo pipefail

SHARED="${NYPC_SHARED:-/srv/nypc}"
TOOL="${NYPC_TOOL:-$SHARED/problem/nation-fr-providing/testing-tool.py}"
BUILD_SH="$SHARED/tools/build.sh"
BUILD_DIR="${NYPC_BUILD_DIR:-${TMPDIR:-/tmp}/nypc-build-$USER}"
BOT_PATH="${BOT_PATH:-.:$SHARED/TEAM:$HOME/bots:$SHARED/bots}"

PYTHON="${PYTHON:-python3}"
BUILD_MODE="${BUILD_MODE:-judge}"

die() { echo "error: $*" >&2; exit 1; }

usage() {
  sed -n '3,26p' "$0" | sed 's/^# \{0,1\}//' >&2
  exit 1
}

[ $# -ge 2 ] || usage
case "${1:-}" in -h|--help) usage ;; esac
[ -f "$TOOL" ] || die "testing-tool 이 없습니다: $TOOL  (NYPC_TOOL 로 지정 가능)"

# 인자를 실제 소스 파일 경로로 해석한다.
resolve_source() {
  local arg="$1"
  # 주최측 샘플 별칭
  if [ "$arg" = "sample" ]; then
    echo "$SHARED/problem/nation-fr-providing/sample-code.py"; return
  fi
  # 직접 경로
  if [ -f "$arg" ]; then echo "$arg"; return; fi
  local dir
  while IFS= read -r dir; do
    [ -n "$dir" ] || continue
    [ -f "$dir/$arg" ] && { echo "$dir/$arg"; return; }
    local ext
    for ext in cpp cc cxx c py; do
      [ -f "$dir/$arg.$ext" ] && { echo "$dir/$arg.$ext"; return; }
    done
  done <<< "${BOT_PATH//:/$'\n'}"
  die "'$arg' 소스를 못 찾았습니다 (탐색: $BOT_PATH, 별칭: sample)"
}

# 소스보다 실행파일이 최신이면 재컴파일을 생략한다.
compile_native() {
  local src="$1"
  mkdir -p "$BUILD_DIR"
  local base key out abs
  base="$(basename "$src")"
  # 같은 파일명이 여러 폴더(TEAM/old, TEAM/round_*, TEAM/vs)에 있을 수 있다.
  # 경로까지 키에 넣지 않으면 서로 다른 소스가 같은 실행파일을 덮어쓰고,
  # 더 오래된 소스로 바꿔 돌릴 때 mtime 비교가 재빌드를 건너뛰어
  # 엉뚱한 바이너리로 측정하게 된다.
  abs="$(cd "$(dirname "$src")" && pwd)/$base"
  key="$(printf '%s' "$abs" | md5sum | cut -c1-8)"
  out="$BUILD_DIR/${base%.*}-$key"
  if [ ! -x "$out" ] || [ "$src" -nt "$out" ]; then
    echo ">> compiling $base ($BUILD_MODE)" >&2
    if [ -x "$BUILD_SH" ]; then
      "$BUILD_SH" "$src" -o "$out" -m "$BUILD_MODE" >&2 || die "compile failed: $src"
    else
      echo ">> build.sh 없음 — g++ 로 대체합니다 (채점기와 코드가 다를 수 있음)" >&2
      g++ -std=gnu++20 -O2 -DONLINE_JUDGE -DNYPC -Wall -Wextra \
          -o "$out" "$src" >&2 || die "compile failed: $src"
    fi
  fi
  CMD="$out"
}

# 소스를 실행 명령으로 바꾼다. 결과는 전역 CMD.
build_cmd() {
  local src="$1" ext="${1##*.}"
  case "$ext" in
    py)          CMD="$PYTHON $src" ;;
    c|cpp|cc|cxx) compile_native "$src" ;;
    *)           die "지원하지 않는 소스 형식: .$ext ($src)" ;;
  esac
}

LEFT_SRC="$(resolve_source "$1")"; shift
RIGHT_SRC="$(resolve_source "$1")"; shift

build_cmd "$LEFT_SRC";  LEFT_CMD="$CMD"
build_cmd "$RIGHT_SRC"; RIGHT_CMD="$CMD"

if [ -z "${MATCH_QUIET:-}" ]; then
  echo ">> LEFT  = $LEFT_SRC" >&2
  echo ">> RIGHT = $RIGHT_SRC" >&2
fi

# 컴파일만 하고 끝낸다 (match.sh 가 병렬 팬아웃 전에 부름).
# 이게 없으면 -j N 에서 N 개 프로세스가 같은 실행파일을 동시에 씁니다.
if [ -n "${VERSUS_BUILD_ONLY:-}" ]; then exit 0; fi

exec "$PYTHON" "$TOOL" -a "$LEFT_CMD" -b "$RIGHT_CMD" "$@"
