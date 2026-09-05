#!/usr/bin/env bash
# setup-toolchain.sh — 채점 환경과 같은 컴파일러/라이브러리를 이 서버에 설치합니다.
#
#   (관리자만) ssh nypc  →  sudo /srv/nypc/tools/setup-toolchain.sh
#   팀원 계정에는 sudo 가 없습니다. ubuntu 계정으로 실행하세요.
#
# 이미 설치돼 있으면 아무것도 하지 않습니다. 인스턴스를 새로 만들었거나
# tools/doctor.sh 가 Boost/gcc 를 못 찾을 때만 돌리세요. Boost 빌드에 10~15분 걸립니다.
#
# 설치 내용 (2026-08-28 기준 주최측 공지):
#   gcc/g++ 14.2.0        (apt: noble-updates)
#   Boost   1.90.0        소스 빌드 → /opt/boost/gcc  (채점기와 같은 경로)
set -euo pipefail

JUDGE_GCC=14
BOOST_VER=1.90.0
BOOST_UNDER=1_90_0
PREFIX=/opt/boost/gcc
URL="https://archives.boost.io/release/${BOOST_VER}/source/boost_${BOOST_UNDER}.tar.gz"

if [[ "${1:-}" == "-h" || "${1:-}" == "--help" ]]; then
  sed -n '2,12p' "$0" | sed 's/^# \{0,1\}//'; exit 0
fi
[[ "${EUID}" -eq 0 ]] || {
  echo "root 로 실행해야 합니다. 팀원 계정에는 sudo 가 없습니다." >&2
  echo "관리자가 ubuntu 계정으로: ssh nypc → sudo $0" >&2
  exit 2; }

echo "== gcc/g++ ${JUDGE_GCC} =="
if ! command -v "g++-${JUDGE_GCC}" >/dev/null; then
  DEBIAN_FRONTEND=noninteractive apt-get update -qq
  DEBIAN_FRONTEND=noninteractive apt-get install -y -qq "g++-${JUDGE_GCC}" "gcc-${JUDGE_GCC}"
fi
update-alternatives --install /usr/bin/g++ g++ "/usr/bin/g++-${JUDGE_GCC}" 140 >/dev/null
update-alternatives --install /usr/bin/gcc gcc "/usr/bin/gcc-${JUDGE_GCC}" 140 >/dev/null
update-alternatives --set g++ "/usr/bin/g++-${JUDGE_GCC}" >/dev/null
update-alternatives --set gcc "/usr/bin/gcc-${JUDGE_GCC}" >/dev/null
echo "  $(g++ --version | head -1)"

echo "== Boost ${BOOST_VER} =="
WANT_LIB_VER="1_${BOOST_VER#1.}"; WANT_LIB_VER="${WANT_LIB_VER%.0}"   # 1.90.0 -> 1_90
have=""
[[ -r "${PREFIX}/include/boost/version.hpp" ]] && \
  have="$(grep -m1 "^#define BOOST_LIB_VERSION" "${PREFIX}/include/boost/version.hpp" | grep -oE '1_[0-9]+' || true)"
if [[ "$have" == "$WANT_LIB_VER" ]]; then
  echo "  이미 설치돼 있습니다: ${PREFIX} (${have})"
elif [[ -n "$have" ]]; then
  echo "  다른 버전이 있습니다: ${have} (원하는 것: ${WANT_LIB_VER})" >&2
  echo "  ${PREFIX} 를 치우고 다시 실행하세요." >&2
  exit 1
else
  echo "  소스 빌드 시작 (10~15분)..."
  BUILD="$(mktemp -d /tmp/boost-build-XXXXXX)"
  trap 'rm -rf "$BUILD"' EXIT
  cd "$BUILD"
  curl -fsSL -o src.tar.gz "$URL"
  tar xf src.tar.gz
  cd "boost_${BOOST_UNDER}"
  ./bootstrap.sh --prefix="$PREFIX" --with-toolset=gcc >/dev/null
  echo "using gcc : ${JUDGE_GCC} : /usr/bin/g++-${JUDGE_GCC} ;" > ./user-config.jam
  ./b2 install \
    --user-config=./user-config.jam --prefix="$PREFIX" \
    toolset="gcc-${JUDGE_GCC}" cxxstd=20 variant=release \
    link=static,shared threading=multi runtime-link=shared \
    -j"$(nproc)" -d0
  echo "  설치됨: ${PREFIX}"
fi

echo
echo "확인: tools/doctor.sh"
