#!/bin/bash
#SBATCH --job-name=moe-search
#SBATCH --array=0-4                    # 버킷 5개 = 태스크 5개 (expert 하나씩)
#SBATCH --cpus-per-task=16
#SBATCH --mem=16G
#SBATCH --time=12:00:00
#SBATCH --output=work/slurm/%x-%A_%a.out
#SBATCH --error=work/slurm/%x-%A_%a.err
#
# 제출:
#   cd ~/MoE && mkdir -p work/slurm && sbatch scripts/slurm_search.sbatch
#   sbatch --export=ALL,MOE_TAG=panel2 scripts/slurm_search.sbatch      # 태그 바꿔 새 실행
#   sbatch -p <partition> -A <account> scripts/slurm_search.sbatch      # 큐 지정
#
# 태스크 하나 = 버킷 하나 = GBT 탐색 한 줄기. 태스크끼리 파일을 공유하지 않으므로
# (버킷마다 별도 jsonl) 서로 간섭하지 않는다. 중간에 죽어도 같은 명령으로 재제출하면
# work/trials/<tag>/bucketN.jsonl 을 읽어 이어서 돈다.
set -euo pipefail

MOE_ROOT="${MOE_ROOT:-${SLURM_SUBMIT_DIR:-$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)}}"
cd "$MOE_ROOT"

# 파이썬: conda 환경 moe (numpy/scikit-learn). 없으면 순수 파이썬 GBT 로 자동 대체된다.
PY="${MOE_PYTHON:-$HOME/.conda/envs/moe/bin/python}"
[ -x "$PY" ] || PY="$(command -v python3)"

JOBS="${MOE_JOBS:-${SLURM_CPUS_PER_TASK:-4}}"
BUCKET="${SLURM_ARRAY_TASK_ID:-0}"

echo "host=$(hostname) bucket=$BUCKET jobs=$JOBS py=$PY tag=${MOE_TAG:-default}"
"$PY" -c 'import sklearn,numpy;print("sklearn",sklearn.__version__)' || echo "sklearn 없음 -> 순수 파이썬 GBT"

exec "$PY" run_search.py --tag "${MOE_TAG:-default}" search \
     --bucket "$BUCKET" --jobs "$JOBS" ${MOE_EXTRA_ARGS:-}
