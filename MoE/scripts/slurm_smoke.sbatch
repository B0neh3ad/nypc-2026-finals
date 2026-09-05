#!/bin/bash
#SBATCH --job-name=moe-smoke
#SBATCH --cpus-per-task=2
#SBATCH --mem=4G
#SBATCH --time=00:20:00
#SBATCH --output=work/slurm/%x-%j.out
#SBATCH --error=work/slurm/%x-%j.err
#
# 본 탐색을 던지기 전에 클러스터에서 배선만 확인한다 (컴파일러/심판기/패널/파이썬).
#   sbatch scripts/slurm_smoke.sbatch
set -euo pipefail
MOE_ROOT="${MOE_ROOT:-${SLURM_SUBMIT_DIR:-$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)}}"
cd "$MOE_ROOT"
PY="${MOE_PYTHON:-$HOME/.conda/envs/moe/bin/python}"
[ -x "$PY" ] || PY="$(command -v python3)"
echo "host=$(hostname)"; g++ --version | head -1; "$PY" -V
"$PY" run_search.py smoke --bucket 2
