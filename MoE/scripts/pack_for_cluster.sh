#!/usr/bin/env bash
# 슬럼 클러스터로 통째로 옮길 tarball 을 만든다.
# 심판기와 상대 패널 소스를 bundle/ 에 같이 넣으므로, 저쪽에 /srv/nypc 가 없어도 돈다.
#
#   scripts/pack_for_cluster.sh [out.tar.gz]
set -euo pipefail
MOE_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SHARED="${NYPC_SHARED:-/srv/nypc}"
OUT="${1:-$MOE_ROOT/work/moe-bundle-$(date +%Y%m%dT%H%M%S).tar.gz}"

rm -rf "$MOE_ROOT/bundle"; mkdir -p "$MOE_ROOT/bundle"
cp "$SHARED/problem/nation-fr-providing/testing-tool.py" "$MOE_ROOT/bundle/"
# 후보 봇 소스도 같이 (config/search.json 의 bot_src 가 절대경로면 저쪽에서 못 찾는다)
BOT="$(python3 -c "import json;print(json.load(open('$MOE_ROOT/config/search.json'))['bot_src'])")"
cp "$BOT" "$MOE_ROOT/bundle/$(basename "$BOT")"
echo "bundle/: testing-tool.py, $(basename "$BOT")"

tar czf "$OUT" -C "$(dirname "$MOE_ROOT")" \
    --exclude="MoE/work/bin" --exclude="MoE/work/slurm" --exclude="MoE/__pycache__" \
    --exclude="MoE/moe/__pycache__" "$(basename "$MOE_ROOT")"
echo "만들어짐: $OUT"
cat <<'MSG'

저쪽에서:
  tar xzf moe-bundle-*.tar.gz && cd MoE
  # 봇 경로를 번들 것으로 돌린다
  python3 - <<'PY'
import json; c=json.load(open('config/search.json'))
import glob, os
c['bot_src']=os.path.abspath(glob.glob('bundle/*.c*')[0]); json.dump(c,open('config/search.json','w'),indent=2)
PY
  conda create -y -n moe python=3.12.3 numpy scikit-learn     # 없으면 순수 파이썬으로도 돈다
  mkdir -p work/slurm && sbatch scripts/slurm_smoke.sbatch    # 먼저 배선 확인
  sbatch scripts/slurm_search.sbatch
MSG
