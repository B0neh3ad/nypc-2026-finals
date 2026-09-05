#!/usr/bin/env bash
# doctor.sh — 이 서버의 컴파일 환경이 채점 환경과 맞는지 확인합니다.
# 아무것도 고치지 않습니다. 아침에 한 번, 뭔가 이상할 때 한 번 돌리세요.
set -uo pipefail

JUDGE_GCC="14.2.0"
JUDGE_BOOST="1_90"
BOOST_INC=/opt/boost/gcc/include
BOOST_LIB=/opt/boost/gcc/lib
SHARED="${NYPC_SHARED:-/srv/nypc}"
fail=0
ok()   { printf '  \033[32mOK\033[0m   %s\n' "$*"; }
bad()  { printf '  \033[31mFAIL\033[0m %s\n' "$*"; fail=1; }
warn() { printf '  \033[33mWARN\033[0m %s\n' "$*"; }

echo "== 컴파일러 =="
if command -v g++ >/dev/null; then
  ver="$(g++ -dumpfullversion 2>/dev/null || echo ?)"
  [[ "$ver" == "$JUDGE_GCC" ]] && ok "g++ ${ver} (채점기와 동일)" \
    || bad "g++ ${ver} — 채점기는 ${JUDGE_GCC}. 통합자에게 알리세요(관리 작업은 ubuntu 계정)"
else
  bad "g++ 가 없습니다"
fi

echo "== Boost =="
if [[ -r "${BOOST_INC}/boost/version.hpp" ]]; then
  bver="$(grep -m1 "^#define BOOST_LIB_VERSION" "${BOOST_INC}/boost/version.hpp" | grep -oE '1_[0-9]+')"
  [[ "$bver" == "$JUDGE_BOOST" ]] && ok "Boost ${bver} @ ${BOOST_INC}" \
    || bad "Boost ${bver} — 채점기는 ${JUDGE_BOOST}"
  n="$(ls "${BOOST_LIB}"/*.a 2>/dev/null | wc -l)"
  [[ "$n" -gt 0 ]] && ok "정적 라이브러리 ${n}개 @ ${BOOST_LIB}" || bad "${BOOST_LIB} 에 .a 가 없습니다"
else
  bad "${BOOST_INC} 가 없습니다 — tools/setup-toolchain.sh 를 돌리세요"
fi

echo "== 채점기 명령 그대로 빌드/실행 =="
tmp="$(mktemp -d)"; trap 'rm -rf "$tmp"' EXIT
cat > "${tmp}/main.cpp" <<'CPP'
#include <boost/version.hpp>
#include <boost/algorithm/string.hpp>
#include <format>
#include <iostream>
#include <string>
#include <vector>
int main(int argc, char** argv) {
  std::vector<std::string> p; std::string s = "a,b,c";
  boost::split(p, s, boost::is_any_of(","));
  std::cout << std::format("{} {} {}\n", BOOST_LIB_VERSION, p.size(), argc > 1 ? argv[1] : "-");
  return 0;
}
CPP
if (cd "$tmp" && g++ -std=gnu++20 -O2 -DONLINE_JUDGE -DNYPC -Wall -Wextra \
      -march=native -mtune=native -o main main.cpp \
      -I"$BOOST_INC" -L"$BOOST_LIB" ) 2>"${tmp}/err"; then
  out="$(cd "$tmp" && ./main NYPC 2>&1)"; rc=$?
  [[ $rc -eq 0 ]] && ok "빌드·실행 성공 (\"${out}\")" || bad "실행 실패 rc=${rc}: ${out}"
else
  bad "빌드 실패: $(head -2 "${tmp}/err" | tr '\n' ' ')"
fi

echo "== -march=native 가 여기서 뜻하는 것 =="
arch="$(g++ -march=native -Q --help=target 2>/dev/null | awk '/^[[:space:]]+-march=/{print $2; exit}')"
if [[ "$arch" == "znver4" ]]; then
  ok "native = znver4 — 채점기와 같은 머신 종류입니다"
else
  warn "native = ${arch:-?} — 채점기는 znver4(AMD EPYC 4세대)입니다."
  warn "시간 측정은 'tools/build.sh <src>' (기본 judge 모드 = znver4) 로 하세요."
fi

echo "== 정적 빌드 =="
if (cd "$tmp" && "${SHARED}/tools/build.sh" main.cpp -o main_static -m static >/dev/null 2>&1); then
  file "${tmp}/main_static" | grep -q "statically linked" \
    && ok "정적 링크 바이너리 생성 가능 (다른 머신 전달용)" \
    || bad "정적 링크가 안 됐습니다"
else
  bad "tools/build.sh -m static 실패"
fi

echo "== 파이썬 / conda =="
sysver="$(python3 -c 'import sys;print(".".join(map(str,sys.version_info[:3])))' 2>/dev/null || echo ?)"
if [[ "$sysver" == "3.12.3" ]]; then ok "시스템 python3 ${sysver} (채점기와 동일)"
else warn "시스템 python3 ${sysver} — 채점기는 3.12.3"; fi
if command -v conda >/dev/null; then
  ok "conda $(conda --version 2>/dev/null | awk '{print $2}') @ /opt/miniconda3"
  nenv="$(conda env list 2>/dev/null | grep -c "/.conda/envs/" || true)"
  ok "내 환경 ${nenv}개 (~/.conda/envs — 팀원과 격리됨)"
  if [[ "$(type -t conda)" != "function" ]]; then
    warn "이 셸에는 conda activate 가 없습니다 — 스크립트에서는 'conda run -n <env>' 를 쓰세요"
  fi
else
  warn "conda 가 PATH 에 없습니다"
fi

echo "== 공유 환경 =="
[[ -d "$SHARED" && -w "$SHARED" ]] && ok "${SHARED} 쓰기 가능" || bad "${SHARED} 에 쓸 수 없습니다"
[[ -L "$HOME/shared" ]] && ok "~/shared 링크 있음" || warn "~/shared 링크가 없습니다"
id -nG | tr ' ' '\n' | grep -qx nypc && ok "그룹 nypc 소속" || bad "그룹 nypc 에 없습니다 (통합자에게 문의)"
if [[ -x "${SHARED}/tools/corepool.sh" ]]; then
  ok "$("${SHARED}/tools/corepool.sh" --status 2>/dev/null | head -1)"
else
  bad "corepool.sh 가 없습니다"
fi
cd "$SHARED" 2>/dev/null && git rev-parse --git-dir >/dev/null 2>&1 \
  && ok "스냅샷 이력 있음 (최근: $(git log -1 --format=%cd --date=format:'%m-%d %H:%M' 2>/dev/null))" \
  || warn "git 이력을 읽지 못했습니다"

echo
[[ $fail -eq 0 ]] && echo "전부 정상입니다." || echo "FAIL 항목을 먼저 해결하세요. docs/TOOLCHAIN.md 참고."
exit "$fail"
