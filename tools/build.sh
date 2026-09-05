#!/usr/bin/env bash
# build.sh — 채점기와 같은 방식으로 컴파일합니다.
#
#   tools/build.sh <source.cpp> [-o out] [-m judge|native|static] [-- 추가옵션...]
#
# 모드:
#   judge    (기본) 채점기와 동일한 코드 생성: -march=znver4 (AMD EPYC 4세대).
#            우리 Intel 머신에서도 실행됩니다. 시간 측정은 이걸로 하세요.
#   native   우리 EC2에 맞춤(-march=native = sapphirerapids). 여기서 제일 빠르지만
#            채점기와 코드가 다릅니다. 빠른 기능 확인용.
#   static   judge 와 같되 -static. **이 인스턴스를 떠나는 바이너리는 반드시 이것.**
#            EC2의 libstdc++(GLIBCXX_3.4.32)가 더 오래된 배포판에는 없어서
#            동적 링크 바이너리는 그런 머신에서 실행 자체가 실패합니다.
set -euo pipefail

CXX=g++
BOOST_INC=/opt/boost/gcc/include
BOOST_LIB=/opt/boost/gcc/lib

SRC=""; OUT=""; MODE=judge; EXTRA=()
while [[ $# -gt 0 ]]; do
  case "$1" in
    -o) OUT="$2"; shift 2 ;;
    -m|--mode) MODE="$2"; shift 2 ;;
    --) shift; EXTRA=("$@"); break ;;
    -h|--help) sed -n '2,17p' "$0" | sed 's/^# \{0,1\}//'; exit 0 ;;
    -*) EXTRA+=("$1"); shift ;;
    *)  SRC="$1"; shift ;;
  esac
done

[[ -n "$SRC" ]] || { echo "사용법: $0 <source.cpp> [-o out] [-m judge|native|static]" >&2; exit 2; }
[[ -f "$SRC" ]] || { echo "파일이 없습니다: $SRC" >&2; exit 2; }
[[ -n "$OUT" ]] || OUT="$(basename "${SRC%.*}")"

ver="$("$CXX" -dumpfullversion 2>/dev/null || echo 0)"
[[ "$ver" == "14.2.0" ]] || echo "경고: g++ ${ver} 입니다. 채점기는 14.2.0 입니다." >&2
[[ -d "$BOOST_INC" ]] || echo "경고: ${BOOST_INC} 가 없습니다 (Boost 1.90.0 미설치?)" >&2

# 채점기 명령:
#   g++ -std=gnu++20 -O2 -DONLINE_JUDGE -DNYPC -Wall -Wextra -march=native -mtune=native \
#       -o main main.cpp -I/opt/boost/gcc/include -L/opt/boost/gcc/lib
COMMON=(-std=gnu++20 -O2 -DONLINE_JUDGE -DNYPC -Wall -Wextra)

case "$MODE" in
  judge)   ARCH=(-march=znver4 -mtune=znver4); LINK=() ;;
  native)  ARCH=(-march=native -mtune=native); LINK=() ;;
  static)  ARCH=(-march=znver4 -mtune=znver4); LINK=(-static) ;;
  *) echo "알 수 없는 모드: $MODE (judge|native|static)" >&2; exit 2 ;;
esac

set -x
"$CXX" "${COMMON[@]}" "${ARCH[@]}" "${LINK[@]}" -o "$OUT" "$SRC" \
  -I"$BOOST_INC" -L"$BOOST_LIB" ${EXTRA[@]+"${EXTRA[@]}"}
{ set +x; } 2>/dev/null

echo "빌드됨: ${OUT}  (mode=${MODE})"
[[ "$MODE" == "static" ]] && file "$OUT" | grep -q "statically linked" \
  && echo "정적 링크 확인됨 — 다른 머신으로 넘겨도 됩니다"
exit 0
