# tools/

| 도구 | 하는 일 | 언제 |
|---|---|---|
| `versus.sh` | **봇 둘을 한 판** 붙임. C/C++ 는 `build.sh` 로 자동 컴파일 | 리플레이 하나 볼 때 |
| `match.sh` | **n판 돌려 승패 집계.** `-j` 로 병렬(corepool 자동) | 강한지 약한지 판단할 때 |
| `doctor.sh` | 이 서버가 채점 환경과 맞는지 점검. 아무것도 안 고침 | **아침에 한 번**, 이상할 때 |
| `build.sh` | 채점기와 같은 방식으로 컴파일 (judge/native/static) | 매번 |
| `corepool.sh` | EC2 코어 세마포어. 병렬 작업은 반드시 이걸로 감쌈 | 병렬 실행 때마다 |
| `setup-toolchain.sh` | gcc 14.2.0 + Boost 1.90.0 설치 | 인스턴스를 새로 만들었을 때만 |
| `ctx.sh` | 대화형 모델에 붙여넣을 컨텍스트 생성 | 새 대화 시작할 때 |
| `laptop/nypc-setup` | **노트북용.** 접속 설정 + 점검 (처음 한 번) | 온보딩 |
| `laptop/nypc-push` | **노트북용.** 소스 → 서버 업로드 → 컴파일 | scp 대신 쓰면 편함 |
| `snapshot.sh` | 공유 폴더 2분 자동 커밋. cron 이 돌림 | 건드릴 일 없음 |

## 대국 도구 빠른 사용법

```bash
cd ~/shared/tools
./versus.sh mybot sample --seed 42 -l /tmp/log.txt   # 한 판 + 로그
./match.sh 200 mybot sample -j 6                     # 200판 집계, 6판 동시
./match.sh 200 mybot other --map-size 201            # 맵 크기 고정 (실제 N)
```

봇 이름 해석 순서는 `.` → `~/shared/TEAM` → `~/bots` (`BOT_PATH` 로 변경).
`sample` 은 주최측 파이썬 샘플 봇의 별칭입니다. 직접 경로도 됩니다.

두 스크립트는 예선 때 쓰던 것을 본선용으로 고친 것입니다. 바뀐 점:

- 심판기 경로가 `problem/nation-fr-providing/` 로 바뀜
- 맵 크기 검증이 본선 규칙(`181 ≤ N ≤ 249`, `ceil(√N−1) ≤ K ≤ floor(√N+4)`)
- 컴파일을 `build.sh` 에 위임 — 손으로 `g++` 를 치면 채점기와 다른 코드가 나옵니다
  ([../docs/TOOLCHAIN.md](../docs/TOOLCHAIN.md) "함정 둘")
- `-j` 병렬 + corepool 자동 확보, 로그 기본 삭제(`KEEP_LOGS=1` 로 유지)
- 요약에 **표준오차** 표기 — 20판 60% 는 ±11%p 라 차이의 근거가 못 됩니다

## 배틀 결과는 자동으로 쌓입니다

`match.sh` 는 실행할 때마다 결과를 `runs/` 아래에 남깁니다. 아무것도 안 해도 됩니다.

```
runs/matches.tsv              실행 한 줄 = 한 행. 누적 색인 — 이걸 grep 하세요
runs/games/<runid>.tsv        그 실행의 판별 결과 (판·시드·맵·진영·승자)
runs/logs/<runid>/            리플레이 원본. KEEP_LOGS=1 일 때만
```

`runs/` 는 `.gitignore` 에 있어서 2분 스냅샷에 안 들어갑니다. 즉 이력에 안 남으니
**결론은 따로 팀에 공유**하세요. 파일은 팀원 모두 읽고 쓸 수 있습니다.

```bash
column -t -s $'\t' ~/shared/runs/matches.tsv            # 전체 보기
grep new_rule_baseline ~/shared/runs/matches.tsv         # 특정 봇만
awk -F'\t' '$13>0.6' ~/shared/runs/matches.tsv          # 승률 60% 넘은 실행
```

리플레이는 기본적으로 안 남깁니다 — 한 판에 60KB라 1000판이면 60MB입니다.
한 판을 눈으로 볼 거면 `versus.sh` 로 따로 돌리는 편이 낫고, 배치 전체를 남겨야
하면 `KEEP_LOGS=1` 을 주세요. 적재를 끄려면 `NO_RECORD=1`.

## 아직 없는 것

채점·비교·제출 도구는 아직 없습니다. 실제 전략이 생긴 뒤에 만듭니다.

참고 자료: `../reference/` — 예선 레포. **참고용이고, 애매하면 새로 씁니다**
([../docs/REFERENCE.md](../docs/REFERENCE.md)).

읽을 문서: [../docs/PROBLEM-TOOLING.md](../docs/PROBLEM-TOOLING.md) (문제·심판기),
[../docs/TOOLCHAIN.md](../docs/TOOLCHAIN.md) (컴파일),
[../docs/COLLAB.md](../docs/COLLAB.md) (협업).

리플레이는 `../simulator/` (정적 미러) 또는 원본
<https://d1thb30t7rs13h.cloudfront.net/>.

무거운 작업은 이 인스턴스에서 돌리지 말고 **jinsu 또는 jinyoung에게 문의하세요.**
