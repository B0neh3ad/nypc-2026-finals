# NEXT VISION — 문제 자료와 로컬 대전 도구

문제 전문: [docs/problem-1-next-vision.md](problem-1-next-vision.md)
자료 위치: `~/shared/problem/`

```
problem/
  nation-fr-providing.zip          공식 CLI 툴 원본
  nation-fr-providing/
    testing-tool.py                인터랙터 (심판)
    README.md                      ★ 문제 페이지에 없는 내용 있음 — 반드시 읽을 것
    config.ini                     실행 설정
    sample-code.py                 파이썬 샘플 봇
  sample-code/                     12개 언어 샘플 봇 (C, C++, Java, Rust, Go, ...)
```

## 바로 돌려보기

가장 쉬운 길은 `tools/` 의 두 스크립트입니다 (예선 것을 본선용으로 고침).

```bash
cd ~/shared/tools
./versus.sh mybot sample --seed 42 -l /tmp/log.txt   # 한 판 + 로그
./match.sh 200 mybot sample -j 6                     # 200판 집계
./match.sh 200 mybot other --map-size 201            # 맵 크기 고정
```

봇 이름은 `.` -> `~/shared/TEAM` -> `~/bots` 순으로 찾습니다. `sample` 은
주최측 파이썬 샘플의 별칭입니다. 자세한 건 [../tools/README.md](../tools/README.md).

리플레이는 `~/shared/simulator/` 에 정적 미러를 떠 뒀습니다
([../simulator/README.md](../simulator/README.md)) — 원본은
<https://d1thb30t7rs13h.cloudfront.net/>.

## 심판기를 직접 부르기

```bash
cd ~/shared/problem/nation-fr-providing
python3 testing-tool.py --seed 42 -l /tmp/log.txt \
  -a "python3 sample-code.py P1" -b "python3 sample-code.py P2"
```

맵 지정 방법 3가지(앞이 우선): `-i <맵파일>` → `--seed S` → `--NP/--KP`.
`--NP`/`--KP`는 **절반** 값입니다 (실제 `N = 2*NP+1`).

**C++ 봇을 손으로 `g++` 해서 붙이지 마세요.** 채점기와 다른 코드가 나와서 시간
측정이 무의미해집니다([TOOLCHAIN.md](TOOLCHAIN.md) "함정 둘"). `versus.sh` 가
`build.sh` 를 거쳐 컴파일해 줍니다.

## 검증된 사실 (2026-08-29 서버에서 실측)

- 400턴 자기대전 1판 = **0.36초** (파이썬 샘플끼리). 반복 실험 저렴함.
- `g++ 14.2.0` 설치됨. 12,000줄짜리 C++ 봇이 `build.sh` 로 약 4초에 빌드됨.
- 샘플 봇 기본 전략은 "1턴에 전사 3명 적 본부로 진군" 뿐 → 항상 `DRAW TURN_LIMIT`.
  `decide()` 하나만 채우면 되게 되어 있고, **상태 추적·시야 계산·최단경로가 이미 구현되어 있다**
  (`Paths`, `next_step`, `compute_visible`). 밑바닥부터 짜지 말 것.
- 서버에 `unzip` 없음. `python3 -c "import zipfile; zipfile.ZipFile(f).extractall()"` 쓸 것.

## README.md와 문제 페이지의 관계

**둘 사이에 모순은 없다.** README는 대부분 문제 페이지와 같은 규칙을 영어로 더 풀어 쓴 것이다.
아래 항목은 문제 페이지에도 이미 있으니, README만 보고 "새 규칙"이라고 착각하지 말 것:

- 시야가 hop 거리 2라는 것 → 문제 페이지 섹션 4에 정의되어 있다
  ("인접한 구역 간의 거리는 1이며, 두 구역의 거리는 … 가장 짧은 것의 길이").
  단, **이동 경로 계산은 유클리드 거리의 올림**을 쓴다(섹션 3). 이 둘을 헷갈리지 말 것.
- 결과 블록 이벤트 섹션이 내 것만 온다는 것 → 문제 페이지에 "**아군**"으로 명시되어 있다.
- `damage`가 잃은 체력이라는 것 → 문제 페이지의 "입힌 피해량"과 같은 말이다.
- WARRIOR/BUILDING에 내 유닛이 전부 포함된다는 것 → 유닛은 자기 구역을 항상 보므로
  문제 페이지의 "시야 안에 있는 모든"과 동치다.

### 정말 README에만 있는 것

1. **결과 블록 각 섹션의 행 정렬 순서** (게임 규칙 중 유일하게 추가된 정보)
   - `UPGRADE`, `SIEGE`: 구역 번호 오름차순
   - `MOVE`, `WARRIOR`: 전사 ID순 (`A*` 먼저, 그다음 숫자 오름차순)
   - `BUILDING`: 구역 번호 오름차순
   - `DAMAGE`: TURRET → COMBAT → HUNGER 그룹 순, 그룹 안에서 전사 ID순
   - 다만 `N`개 줄을 읽어 자기 자료구조에 넣으면 그만이라, 순서에 의존하는 코드를
     짜지 않는 한 실익은 없다.
2. **로그 파일 포맷** (게임 규칙 아님, 툴 기능)
   - 봇이 stderr에 찍은 줄은 로그에 `# Debug <LEFT/RIGHT>: <msg>`로 남는다 — 디버깅에 쓸 것.
   - 로그 마지막 줄 `RESULT <LEFT_WIN|RIGHT_WIN|DRAW> <HQ_DESTROYED|TURN_LIMIT|WA>`.
     대량 대전 결과 집계는 이 줄만 grep하면 된다.

## 이 심판기는 예선 심판기와 96.2% 같습니다

2026-08-29 실측:

```
reference/engine/nation-providing/testing-tool.py   1599줄   (7월 예선)
problem/nation-fr-providing/testing-tool.py        1641줄   (본선)
                                    유사도 96.2%, 바뀐 줄 124개
```

CLI 인자·config 키·로그 포맷·와이어 프로토콜이 동일합니다.

바뀐 124줄의 정체는 두 가지입니다.

1. **상수 재조정** — 시작 금화 500→750, 200일→400일, 맵 N 51~109→181~249,
   기지 비용 300/600/1000→500/550/600 등. 규칙 구조는 그대로입니다.
2. **전장의 안개 신설** — 예선에서는 결과 블록이 양쪽 이벤트를 병합해 와서
   **완전정보**였습니다. 본선은 그 병합(`_merge_results`)이 삭제되고
   `HOP_VISION = 2` 와 `WARRIOR`/`BUILDING` 스냅샷이 들어왔습니다.
   **이게 올해의 진짜 게임 변경점입니다.**

**주의:** 심판기가 같다고 예선 봇·튜닝 결과를 가져다 쓰라는 뜻이 아닙니다.
예선 자산은 참고용이고, 애매하면 새로 씁니다 — [REFERENCE.md](REFERENCE.md).
