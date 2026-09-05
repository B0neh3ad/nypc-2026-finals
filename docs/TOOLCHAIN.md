# 컴파일 환경과 절차

EC2에 **채점 환경과 같은 툴체인**이 설치돼 있습니다. 확인은 한 줄:

```bash
~/shared/tools/doctor.sh
```

빌드는 `tools/build.sh`로 합니다. 이유는 아래 "함정 둘"을 보세요 — 손으로
`g++`를 치면 조용히 틀린 걸 재게 됩니다.

## 빠른 시작

```bash
cd ~/shared
tools/doctor.sh                        # 아침에 한 번
tools/build.sh main.cpp                # 채점기와 같은 코드 생성 → ./main
./main NYPC                            # 채점기 실행 명령과 동일
```

이 인스턴스를 떠나는 바이너리만 다릅니다 (정적 링크):

```bash
tools/build.sh main.cpp -m static -o main_static
```

## 채점 환경 (주최측 공지, 2026-08-28 확인)

| | |
|---|---|
| 머신 | AWS `c7a.2xlarge` — **AMD EPYC 4세대**, 3.7GHz, Ubuntu 24.04 |
| 컴파일러 | gcc **14.2.0**, `-std=gnu++20` |
| 외부 라이브러리 | **Boost 1.90.0** — `/opt/boost/gcc/{include,lib}` |
| 종료 코드 | 반드시 0 |
| CPU | **코어 1개만 할당.** 스레드를 써도 시간·메모리는 전부 합산 |

```
g++ -std=gnu++20 -O2 -DONLINE_JUDGE -DNYPC -Wall -Wextra -march=native -mtune=native \
    -o main main.cpp -I/opt/boost/gcc/include -L/opt/boost/gcc/lib
./main NYPC
```

`-DONLINE_JUDGE`와 `-DNYPC`가 정의됩니다. 로컬 전용 디버그 코드를 `#ifndef NYPC`로
감싸두면 제출본에서 자동으로 빠집니다.

## 우리 EC2에 설치된 것

- gcc/g++ **14.2.0** — `g++`가 기본으로 이걸 가리킵니다 (예전 13.3은 `g++-13`)
- Boost **1.90.0** — `/opt/boost/gcc`, **채점기와 같은 경로**. 정적 45개 / 공유 9개. 전원 읽기 가능

위 채점기 명령을 그대로 복사해 붙여도 빌드·실행됩니다. 확인해 뒀습니다.

인스턴스를 새로 만들었다면 **관리자(통합자)** 가 `ubuntu` 계정으로 돌립니다:
`ssh nypc` 후 `sudo /srv/nypc/tools/setup-toolchain.sh` (Boost 빌드 10~15분).
팀원 계정에는 `sudo`가 없습니다.

## tools/build.sh

```
tools/build.sh <source.cpp> [-o out] [-m judge|native|static] [-- 추가옵션]
```

| 모드 | `-march` | 링크 | 쓸 곳 |
|---|---|---|---|
| `judge` (기본) | `znver4` | 동적 | **평소·시간 측정.** 채점기와 같은 코드 생성 |
| `native` | `sapphirerapids` | 동적 | 여기서 제일 빠름. 기능만 빠르게 볼 때 |
| `static` | `znver4` | **정적** | **이 인스턴스를 떠나는 바이너리는 반드시 이것** |

`-Wall -Wextra`는 채점기 명령에 있는 그대로 항상 켜집니다.

## 함정 둘 — 둘 다 실측으로 확인

### 1. `-march=native`가 여기서는 채점기와 다른 뜻입니다

| | CPU | `native` 의 의미 |
|---|---|---|
| 채점기 | AMD EPYC 4세대 (c7a) | `znver4` |
| 우리 EC2 | Intel Xeon 8488C (c7i) | `sapphirerapids` |

같은 소스인데 생성 코드가 다릅니다. **`native`로 잰 시간은 채점기 시간이 아닙니다.**

**본선 문제는 턴당 100ms입니다**(초읽기 5개, 초과분 100ms마다 1개 소모).
즉 이 차이가 실제로 판정을 바꿉니다 — 성능 측정은 반드시 기본 `judge` 모드로
하세요.

다행히 **`znver4`로 빌드한 바이너리가 우리 Intel 머신에서 정상 실행됩니다**(확인함).
그래서 `build.sh`의 기본이 `judge`(=znver4)입니다 — 채점기와 같은 코드를 여기서 돌립니다.

`native` 모드는 "컴파일 되나, 크래시 안 나나"를 빨리 볼 때만 쓰세요.
성능·시간 판단에는 쓰지 마세요.

### 2. 이 인스턴스를 떠나는 바이너리는 정적 링크여야 합니다

EC2는 Ubuntu 24.04 / g++ 14.2.0이고 libstdc++가 `GLIBCXX_3.4.32`입니다.
C++20 코드를 동적 링크로 빌드해 더 오래된 배포판의 머신으로 넘기면 이렇게 죽습니다:

```
./main: /lib/x86_64-linux-gnu/libstdc++.so.6: version `GLIBCXX_3.4.32' not found
```

실제로 재현했고, `-static`으로 빌드하면 Boost를 써도 정상 실행되는 것까지 확인했습니다.

**그래서 바이너리를 넘길 때는 이렇게 합니다:**

1. EC2에서 `tools/build.sh <src> -m static -o main_static`
2. 무엇을 돌려서 무엇을 돌려받고 싶은지 한 줄로 적어 **jinsu 또는 jinyoung에게** 전달
3. **받는 쪽에서 컴파일하지 않습니다.** 컴파일러가 다르면 거기서 나온 결과를 믿을 수 없습니다

후보마다 재컴파일해야 하는 방식(파라미터 탐색 등)이라면, **컴파일은 EC2 16코어에서
하고 실행만** 넘기세요. 컴파일은 초 단위고 실행이 병목입니다.

## 파이썬 / conda

채점기 파이썬은 **3.12.3** (numpy, scipy, PyTorch, TensorFlow 포함).
EC2 시스템 `python3`도 3.12.3이라 표준 라이브러리만 쓰는 스크립트는 그냥 쓰면 됩니다.

패키지가 필요하면 conda — 공용 베이스 `/opt/miniconda3`, 환경은 각자 `~/.conda/envs`에 격리.

```bash
conda create -n myenv python=3.12.3 numpy   # 버전을 명시할 것 (베이스는 3.12.13)
conda run -n myenv python script.py         # 스크립트·에이전트에서는 activate 말고 이것
```

**`conda activate` 는 비대화형 셸에 없습니다.** 셸 *함수*라서 `~/.bashrc` 를 읽는
셸에만 존재하는데, Ubuntu 의 `.bashrc` 는 비대화형이면 즉시 빠져나갑니다. 에이전트가
돌리는 `bash -c`, 잡 스크립트, Makefile 이 전부 여기 해당합니다. 그래서 스크립트에서는
`conda run -n <env> ...` 을 쓰세요. 정 `activate` 가 필요하면
`source /opt/miniconda3/etc/profile.d/conda.sh` 를 먼저 하세요.

**공유 폴더(`/srv/nypc`)에 환경을 만들지 마세요** — 2분 스냅샷이 수 GB를 커밋하려
듭니다. 기본값(`~/.conda/envs`)을 그대로 쓰면 됩니다.

## 기억할 것

- **정본은 채점기**이고, EC2를 거기에 맞춰 놨습니다 (둘 다 14.2.0) —
  EC2에서 안 되면 채점기에서도 안 됩니다.
- 플랫폼 종속 코드(Windows API, MSVC 헤더, 비표준 함수)는 채점기에서 실패합니다.
- 프로그램은 **종료 코드 0**으로 끝나야 합니다.
- 소스는 1 MiB, 바이너리는 10 MiB 이하
  ([master-대회규칙.md](master-대회규칙.md) 의 `문제 풀이`).
- **채점기는 코어를 1개만 줍니다.** 제출 바이너리를 멀티스레드로 만들어도
  모든 스레드의 CPU time이 합산돼 TLE만 앞당깁니다. 측정도 1코어에 묶어야
  채점기와 같은 숫자가 나옵니다 — `tools/corepool.sh 1 -- taskset -c 0 ./main NYPC`.
  (`corepool.sh`는 팀원을 비켜서게 할 뿐이고, 실제로 1코어에 가두는 건 `taskset`입니다.)
  `TLE`(CPU time)와 `WTLE`(실제 경과 시간)는 별도 판정입니다
  ([master-대회규칙.md](master-대회규칙.md) 의 `실행 및 채점 환경`, `채점 결과`).
