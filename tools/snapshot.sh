#!/usr/bin/env bash
# snapshot.sh — 공유 폴더를 2분마다 자동 커밋. cron이 돌립니다.
#
# 공유 폴더에서는 두 사람이 같은 파일을 고치면 나중 저장이 조용히 덮어씁니다.
# 이 스냅샷이 그걸 되돌릴 유일한 수단입니다. 팀원은 커밋을 신경 쓸 필요가 없습니다.
set -uo pipefail

WORK="${NYPC_SHARED:-/srv/nypc}"
cd "$WORK" 2>/dev/null || exit 0
git rev-parse --git-dir >/dev/null 2>&1 || exit 0

# 사람이 손으로 git 작업 중이면(rebase/merge/index.lock) 건드리지 않습니다.
GITDIR="$(git rev-parse --git-dir)"
for f in index.lock MERGE_HEAD REBASE_HEAD rebase-merge rebase-apply; do
  [[ -e "${GITDIR}/${f}" ]] && exit 0
done

git add -A >/dev/null 2>&1 || exit 0
git diff --cached --quiet && exit 0   # 바뀐 게 없으면 조용히 끝

git -c user.name="snapshot" -c user.email="snapshot@nypc.local" \
    commit -q -m "snapshot $(date '+%Y-%m-%d %H:%M:%S')" >/dev/null 2>&1
