# MoE — 맵 크기별 expert 파라미터 탐색

맵 크기 N 을 **5개 버킷**으로 나누고, 버킷마다 별도의 파라미터 한 벌(expert)을 찾는다.
점수는 `opponents/` 에 모아 둔 **우리 예전 봇들에 대한 평균 승률**이고,
그 점수를 **gradient boosting 대리모형**으로 학습해 다음에 시험할 파라미터를 고른다.
마지막에 5벌을 한 소스로 조립하면, 봇이 N 을 읽는 순간 자기 expert 를 고른다.

```
파라미터 후보 ──▶ -DNAME=v 로 컴파일 ──▶ 패널 전원과 대전 ──▶ score(평균 승률)
      ▲                                                          │
      └────── GBT 로 score 예측 + UCB 로 다음 후보 선택 ◀─────────┘
```

## 5분 사용법

```bash
cd ~/MoE
PY=~/.conda/envs/moe/bin/python          # numpy + scikit-learn 이 든 환경

# 1) 봇 소스의 튜닝 블록에서 파라미터 공간을 뽑는다 (config/space.json)
python3 run_search.py extract-space --src ~/shared/TEAM/round_13_1400/jyp_v4.cpp --force

# 2) 배선 확인 — 한 판만
$PY run_search.py smoke --bucket 2

# 3) 기본 파라미터의 점수(=기준선)를 본다
~/shared/tools/corepool.sh 6 -- $PY run_search.py eval --bucket 2 --jobs 6

# 4) 탐색 (로컬은 반드시 corepool 경유)
scripts/local_search.sh 2 6

# 5) 결과
$PY run_search.py report
$PY run_search.py export            # work/experts/params_moe.json
$PY run_search.py assemble          # work/experts/moe_bot.cpp  (제출용 한 벌)

# 6) 조립본이 진짜 나아졌는지, 탐색에 안 쓴 시드로 다시 잰다
~/shared/tools/corepool.sh 6 -- $PY run_search.py verify --jobs 6
```

## slurm 으로 던지기

버킷 하나 = 태스크 하나. 5개 버킷을 배열 작업으로 동시에 돌린다.

```bash
mkdir -p work/slurm
sbatch scripts/slurm_smoke.sbatch                       # 먼저 배선 확인
sbatch scripts/slurm_search.sbatch                      # 본 탐색 (array 0-4)
sbatch -p <partition> -A <account> scripts/slurm_search.sbatch
sbatch --export=ALL,MOE_TAG=panel2 scripts/slurm_search.sbatch   # 다른 패널/설정은 태그를 나눠서
```

- `--cpus-per-task` 가 그대로 동시 대전 수(`--jobs`)가 된다.
- 태스크끼리 파일을 공유하지 않는다(버킷마다 별도 jsonl). 죽으면 **같은 명령으로 재제출** —
  `work/trials/<tag>/bucketN.jsonl` 을 읽어 이어서 돈다.
- 클러스터에 `/srv/nypc` 가 없으면 `scripts/pack_for_cluster.sh` 로 심판기·봇·패널을
  `bundle/` 에 넣어 통째로 싸서 보낸다. 경로는 `MOE_REFEREE`, `NYPC_SHARED`,
  `MOE_PYTHON`, `MOE_WORK` 환경변수로도 덮어쓸 수 있다.
- scikit-learn 이 없는 노드에서는 순수 파이썬 GBT 로 자동 대체된다(느리지만 멈추지는 않는다).

## 구성

| 파일 | 하는 일 |
|---|---|
| `config/buckets.json` | 맵 크기 5구간. N=181\~249 홀수 35종을 7개씩 |
| `config/space.json` | 탐색할 파라미터와 범위 (`extract-space` 가 만든 초안 — **손으로 고쳐 쓰는 파일**) |
| `config/active_core.txt` | 그 중 실제로 켤 이름 목록 |
| `config/search.json` | 봇 소스, 판 수, GBT/UCB 설정, 예산 |
| `opponents/` | 점수를 재는 상대 패널 (우리 예전 봇들) |
| `moe/space.py` | `#ifndef/#define` 추출, 정규화 벡터 ↔ `-DNAME=v` |
| `moe/buckets.py` | 버킷 정의, 결정적 (seed, N, K) 게임 계획 |
| `moe/build.py` | `-D` 주입 컴파일 + 해시 캐시 (채점기와 같은 플래그) |
| `moe/evaluate.py` | 패널 전원과 대전 → score |
| `moe/surrogate.py` | GBT 대리모형 (sklearn 배깅 / 순수 파이썬 대체) |
| `moe/search.py` | 초기설계 → GBT+UCB 반복 → 상위 재평가 |
| `moe/assemble.py` | 5벌 → N 으로 expert 를 고르는 소스 한 벌 |

## 설계에서 중요한 것 세 가지

**1. 모든 후보가 같은 맵·같은 시드에서 평가된다.**
`buckets.game_plan()` 이 (seed, N, K) 를 결정적으로 만들고, 한 쌍은 같은 맵을 좌/우 바꿔
두 번 둔다. 후보 사이의 비교가 짝지어지므로 승률 차이가 맵 운이 아니라 파라미터 차이가 된다.
대신 **그 시드 집합에 과적합**할 수 있어서, 마지막 `reeval` 단계는 시드를 옮겨 다시 잰다
(winner's curse 보정 — `reference/docs/STRATEGY.md` 의 방법).

**2. score 는 승률의 평균이라 노이즈가 크다.**
상대 4명 × 12판 = 48판이면 표준오차가 대략 ±0.07 이다. 그보다 작은 차이는 의미가 없다.
판 수를 늘리거나(`games_per_opponent`), 상위 후보만 다시 재는 쪽이 낫다.

**3. 100% 지는 상대는 정보를 주지 않는다.**
평가에서 어떤 상대에게 승률이 0.00 이나 1.00 으로 박혀 있으면 그 상대는 기울기를 못 만든다.
패널은 **비슷한 실력대**로 채우고, 격차가 큰 상대는 `opponent_weights` 로 낮추거나 빼라.

실제로 지금 패널이 그렇다 — jyp_v4 는 inhyuk_v4·sehyeon_v3 에 0.00, sample 에 1.00 이라
승률만 보면 파라미터를 아무리 흔들어도 점수가 안 움직인다. 그래서 score 에 아주 작은
미세 신호를 얹었다:

    score = 평균 승률 + tiebreak_weight x (게임이 끝난 시점 기반 0~1 점수)

지면 오래 버틸수록, 이기면 빨리 끝낼수록 높다. 가중치는 기본 0.05 — 승패 한 판(1/판수)보다
훨씬 작아서 순위를 뒤집지 못하고, 승률이 완전히 같을 때만 갈라 준다.
`tiebreak_weight: 0` 으로 두면 순수 평균 승률이 된다.

## 비용 감각 (이 EC2 실측, 2026-08-29)

- 후보 하나 = 컴파일 약 5초 + 대전 48판 약 5초(jobs=6) → **약 10초**
- 즉 버킷당 300 후보 ≈ 50분. 5버킷을 슬럼에서 동시에 돌리면 한 시간 안쪽.
- **컴파일이 대전보다 비싸다.** `work/bin/` 해시 캐시가 같은 조합의 재컴파일을 막는다.

## 주의

- 로컬 실행은 **반드시 `corepool.sh` 경유**. 16코어를 넷이 나눠 쓰는데 대전이 초당 수 판씩
  돌아서, 코어를 안 잡으면 턴 100ms 제한에 가짜 타임아웃이 나고 승률이 서버 부하를 재게 된다.
- 패널이나 파라미터 공간을 바꾸면 score 의 의미가 바뀐다. `--tag` 를 새로 주고 새로 시작해라.
- `assemble` 은 `#define X v` 를 `#define X (g_moe->X)` 로 바꾸는 방식이라, `#if X` 처럼
  전처리기에서 쓰이는 이름은 버킷별로 다르게 갈 수 없다. 그런 이름은 컴파일러가 잡아 주고
  자동으로 고정(pin)되며, 무엇이 고정됐는지 결과에 찍힌다.

## 지금 상태 (2026-08-29 검증)

전 구간을 실제로 한 번 돌려 확인했다: `extract-space`(209개 추출/20개 활성) → `smoke` →
`eval` → `search`(버킷 5개) → `report` → `export` → `assemble` → `verify`.
조립본은 컴파일되고, `moe_set_map(M->N)` 이 N 을 읽는 자리에 정확히 들어갔으며,
N=181 과 N=249 에서 서로 다른 expert 로 정상 대국했다. 고정(pin)된 이름은 0개였다.

`work/` 는 전부 재생성물이라 지워도 된다(시행 기록 `work/trials/<tag>/` 만 빼고).
검증에 쓴 장난감 결과는 지웠으니, 첫 `report` 는 비어 있는 게 정상이다.

**아직 안 한 것**: 진짜 예산으로 돌린 탐색. 프레임워크만 서 있는 상태다.
