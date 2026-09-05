#!/usr/bin/env bash
#
# versus.sh — 두 제출 코드를 대결시킨다. C/C++는 필요 시 자동 컴파일한다.
#
# 사용법:
#   ./versus.sh <LEFT> <RIGHT> [testing-tool 추가 옵션...]
#
# <LEFT>, <RIGHT> 는 다음 중 아무 형태나 가능:
#   32148            (submissions/32148.* 를 자동 탐색)
#   32148.cpp        (submissions/ 안의 파일명)
#   path/to/foo.py   (직접 경로)
#
# 예시:
#   ./versus.sh 32148 33108                 # 랜덤 맵으로 대결
#   ./versus.sh 32148 33108 --seed 42       # 시드 고정
#   ./versus.sh 32148 33108 -i input.txt    # 맵 파일 지정
#   ./versus.sh 32148 33108 -l mylog.txt    # 로그 파일 지정
#
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SUB_DIR="$ROOT/submissions"
TOOL="$ROOT/nation-providing/testing-tool.py"
BUILD_DIR="$ROOT/.build"

CXX="${CXX:-g++}"
CC="${CC:-gcc}"
PYTHON="${PYTHON:-python3}"
CXXFLAGS="${CXXFLAGS:--O2 -std=c++17}"
CFLAGS="${CFLAGS:--O2}"

die() { echo "error: $*" >&2; exit 1; }

usage() {
  echo "usage: $0 <LEFT> <RIGHT> [testing-tool options...]" >&2
  echo "  e.g. $0 32148 33108 --seed 42 -l log.txt" >&2
  exit 1
}

[ $# -ge 2 ] || usage
[ -f "$TOOL" ] || die "testing-tool not found: $TOOL"

# 주어진 인자를 실제 소스 파일 경로로 해석한다.
resolve_source() {
  local arg="$1"
  # 1) 직접 경로로 존재하면 그대로 사용
  if [ -f "$arg" ]; then echo "$arg"; return; fi
  # 2) submissions/ 안의 파일명으로 존재하면 사용
  if [ -f "$SUB_DIR/$arg" ]; then echo "$SUB_DIR/$arg"; return; fi
  # 3) 확장자 없는 이름이면 submissions/ 에서 확장자 자동 탐색
  local hit
  for ext in cpp cc cxx c py; do
    if [ -f "$SUB_DIR/$arg.$ext" ]; then echo "$SUB_DIR/$arg.$ext"; return; fi
  done
  hit=$(find "$SUB_DIR" -maxdepth 1 -type f -name "$arg.*" 2>/dev/null | head -1)
  [ -n "$hit" ] && { echo "$hit"; return; }
  die "source not found for '$arg' (looked in ./ and $SUB_DIR)"
}

# 소스를 실행 명령으로 변환한다. C/C++는 필요 시 컴파일한다.
# 결과 명령은 전역 변수 CMD 에 담는다.
build_cmd() {
  local src="$1"
  local ext="${src##*.}"
  case "$ext" in
    py)
      CMD="$PYTHON \"$src\""
      ;;
    c)
      compile_native "$src" "$CC" "$CFLAGS"
      ;;
    cpp|cc|cxx)
      compile_native "$src" "$CXX" "$CXXFLAGS"
      ;;
    *)
      die "unsupported source type: .$ext ($src)"
      ;;
  esac
}

# 소스보다 실행파일이 최신이면 재컴파일을 생략한다.
compile_native() {
  local src="$1" compiler="$2" flags="$3"
  mkdir -p "$BUILD_DIR"
  local base; base="$(basename "$src")"
  local out="$BUILD_DIR/${base%.*}"
  if [ ! -x "$out" ] || [ "$src" -nt "$out" ]; then
    echo ">> compiling $base" >&2
    # shellcheck disable=SC2086
    $compiler $flags -o "$out" "$src" || die "compile failed: $src"
  fi
  CMD="\"$out\""
}

LEFT_SRC="$(resolve_source "$1")"; shift
RIGHT_SRC="$(resolve_source "$1")"; shift

build_cmd "$LEFT_SRC";  LEFT_CMD="$CMD"
build_cmd "$RIGHT_SRC"; RIGHT_CMD="$CMD"

echo ">> LEFT  = $LEFT_SRC" >&2
echo ">> RIGHT = $RIGHT_SRC" >&2
echo ">> running match..." >&2

exec "$PYTHON" "$TOOL" -a "$LEFT_CMD" -b "$RIGHT_CMD" "$@"
