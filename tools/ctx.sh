#!/usr/bin/env bash
# ctx.sh — 대화형 모델에 붙여넣을 컨텍스트를 만듭니다.
# (ChatGPT, Codex, 에이전트 — 어디든. 새 대화를 시작할 때 한 번.)
#
#   ~/shared/tools/ctx.sh                 기본 (환경 + 확인된 규칙 + 시도한 것)
#   ~/shared/tools/ctx.sh --bot <파일>    현재 봇 소스 포함
#   ~/shared/tools/ctx.sh --problem       문제 설명 원문 포함 (길어집니다)
#
# 노트북에서 클립보드로 바로:
#   ssh nypc '~/shared/tools/ctx.sh' | pbcopy
#
# 이 저장소를 못 보는 대화창에 쓸 때 편합니다. 손으로 설명하면 빠뜨리기 쉽고,
# 빠뜨리면 컴파일 안 되는 코드나 이미 버린 아이디어가 다시 나옵니다.
# 필수는 아닙니다 — 직접 설명하는 게 편하면 그렇게 하세요.
set -euo pipefail
SHARED="${NYPC_SHARED:-/srv/nypc}"
BOT=""; WANT_PROBLEM=0

while [[ $# -gt 0 ]]; do
  case "$1" in
    --bot) BOT="$2"; shift 2 ;;
    --problem) WANT_PROBLEM=1; shift ;;
    -h|--help) sed -n '2,12p' "$0" | sed 's/^# \{0,1\}//'; exit 0 ;;
    *) echo "알 수 없는 옵션: $1" >&2; exit 2 ;;
  esac
done

cat <<'EOF'
# 실행 환경 (반드시 지켜서 코드를 써 주세요)

- 컴파일: g++ 14.2.0, `-std=gnu++20`. C++23 기능은 쓰지 마세요.
- 컴파일 명령(채점기와 동일):
  g++ -std=gnu++20 -O2 -DONLINE_JUDGE -DNYPC -Wall -Wextra -march=native -mtune=native \
      -o main main.cpp -I/opt/boost/gcc/include -L/opt/boost/gcc/lib
- 실행: `./main NYPC` — 인자 하나가 붙습니다.
- 외부 라이브러리는 Boost 1.90.0 만 있습니다. 그 외 서드파티 라이브러리는 못 씁니다.
- 소스는 파일 하나, 1 MiB 이하. 프로그램은 종료 코드 0 으로 끝나야 합니다.
- 리눅스입니다. Windows API, MSVC 헤더, 비표준 함수는 쓰지 마세요.
- 답변은 전체 소스를 하나의 ```cpp 블록```으로 주세요. 부분 수정본 말고 전체로요.

EOF

section() { # 제목, 파일, 시작표시
  local title="$1" file="$2"
  [[ -r "$file" ]] || return 0
  printf '# %s\n\n' "$title"
  cat "$file"
  printf '\n\n'
}


if [[ "$WANT_PROBLEM" -eq 1 ]]; then
  found=0
  for f in "${SHARED}"/problem/*.md "${SHARED}"/problem/*.txt; do
    [[ -r "$f" ]] || continue
    printf '# 문제 설명 (%s)\n\n' "$(basename "$f")"; cat "$f"; printf '\n\n'; found=1
  done
  [[ "$found" -eq 1 ]] || echo "# 문제 설명: ${SHARED}/problem/ 에 .md/.txt 가 없습니다" >&2
fi

if [[ -n "$BOT" ]]; then
  [[ -r "$BOT" ]] || { echo "봇 소스를 읽을 수 없습니다: $BOT" >&2; exit 2; }
  printf '# 현재 봇 소스 (%s)\n\n```cpp\n' "$(basename "$BOT")"
  cat "$BOT"
  printf '\n```\n\n'
fi

cat <<'EOF'
# 부탁

위 제약을 지켜 개선안을 제시해 주세요. 무엇을 왜 바꿨는지 세 줄 이내로 먼저 쓰고,
그 다음 전체 소스를 하나의 코드 블록으로 주세요.
EOF
