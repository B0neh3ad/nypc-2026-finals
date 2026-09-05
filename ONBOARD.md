# 시작하기

접속해서 5분 안에 일할 수 있는 상태가 되는 것이 목표입니다.

## 1. 접속

계정과 키는 이미 만들어져 있습니다. 통합자(jinsu)가 보낸 키를 받아 두세요.

```bash
mkdir -p ~/.ssh && chmod 700 ~/.ssh
install -m 600 <받은키> ~/.ssh/nypc_ec2

cat >> ~/.ssh/config <<'CFG'

Host nypc
  HostName <서버-호스트명>
  User <내-아이디>
  IdentityFile ~/.ssh/nypc_ec2
  IdentitiesOnly yes
CFG
chmod 600 ~/.ssh/config
```

아이디는 `jinsu` `jinyoung` `sehyeon` `inhyeok` 중 본인 것입니다.
**Tailscale 이 켜져 있어야 합니다** — 안 켜져 있으면 위 호스트 이름이 해석되지 않습니다.

```bash
ssh nypc
```

안 되면: Tailscale 연결 확인 → 키 권한(600) 확인 → 통합자에게 문의.

## 2. 환경 점검

접속하자마자 한 번:

```bash
~/shared/tools/doctor.sh
```

컴파일러가 채점기와 같은지, 공유 폴더에 쓸 수 있는지, 코어 풀이 비었는지까지 봅니다.
`전부 정상입니다` 가 나오면 준비 끝입니다. FAIL 이 있으면 통합자에게 보여주세요.

## 3. 어디에 뭘 두나

| | 경로 | |
|---|---|---|
| 개인 작업 | `~` (내 홈) | 팀원이 **읽을 수** 있고 고치지는 못합니다 |
| 팀 공유 | `~/shared` (= `/srv/nypc`) | 전원 읽기/쓰기 |

공유 폴더는 **2분마다 자동으로 커밋**됩니다. 커밋은 신경 쓰지 마세요. 누가 내 파일을
덮어써도 `git log` 로 되찾습니다.

큰 것(대국 로그, 바이너리)과 비밀(대회 사이트 로그인)은 **공유 폴더에 두지 마세요.**
자동 커밋이 이력에 영구히 남깁니다.

## 4. 읽을 것 하나

```bash
cat ~/shared/CLAUDE.md
```

대회 규정(서브에이전트 금지 등), 공유 서버에서 남을 방해하지 않는 법, 컴파일 방법이
들어 있습니다. 에이전트를 쓴다면 에이전트도 이걸 읽습니다.

예선에서 비싸게 배운 것과 협업 규약은 `~/shared/docs/COLLAB.md` (짧습니다).

## 5. 컴파일

`g++` 를 직접 치지 말고:

```bash
~/shared/tools/build.sh mybot.cpp        # 채점기와 같은 코드 생성
./mybot NYPC
```

채점기는 AMD, 우리 서버는 Intel 이라 `-march=native` 의 의미가 다릅니다.
`build.sh` 기본 모드가 채점기 쪽에 맞춰져 있습니다.

## 6. 남을 방해하지 않기 — 두 줄만

**병렬로 뭔가 돌릴 때는 감쌉니다.** 16코어를 넷이 씁니다. 그냥 돌리면 서로의 측정에
가짜 타임아웃을 만들고, 그러면 승률이 실력이 아니라 서버 부하를 재게 됩니다.

```bash
~/shared/tools/corepool.sh 6 -- <명령>
```

**시간을 잴 때는 코어 하나에 묶습니다.** 채점기는 프로그램에 코어를 하나만 줍니다.

```bash
~/shared/tools/corepool.sh 1 -- taskset -c 0 ./mybot NYPC
```

오래 걸리거나 코어를 많이 먹는 작업은 이 서버에서 돌리지 말고 jinsu 나 jinyoung 에게
말하세요.

## 7. 알아두면 좋은 것

- **봇을 올릴 때** — `~/shared/TEAM/` 에 파일 하나로. 이름만으로 대결시킬 수
  있습니다: `tools/match.sh 200 <내봇> sample -j 6`
- **노트북에서 코드를 올릴 때** — `~/shared/tools/laptop/` 의 `nypc-push`.
  `scp` + `ssh` 로 해도 같습니다.
- **모델에 붙여넣을 컨텍스트** — `ssh nypc '~/shared/tools/ctx.sh' | pbcopy`
- **파이썬 패키지** — conda, 환경은 각자 `~/.conda/envs`. 스크립트에서는
  `conda activate` 말고 `conda run -n <env> ...` (`~/shared/docs/TOOLCHAIN.md`)
- **문제와 대전 도구** — 문제 전문은 `~/shared/docs/problem-1-next-vision.md`,
  로컬에서 한 판 돌리는 법은 `~/shared/docs/PROBLEM-TOOLING.md`.
  주최측 배포본은 `~/shared/problem/` 에 그대로 있습니다.
- **예선 코드** — `~/shared/reference/`. **참고용입니다. 애매하면 새로 쓰세요.**
  본선은 예선과 같은 게임에 상수만 바뀌고 전장의 안개가 추가된 형태라 읽을 값어치는
  있지만, 포크 대상이 아닙니다. 이유는 `~/shared/docs/REFERENCE.md`.

## 8. 안 되는 것

- **`sudo` 없습니다.** 일부러 그렇습니다. 필요하면 통합자에게 말하고, 우회로를 찾지 마세요.
- 일상 작업에는 필요 없습니다 — 빌드, 공유 폴더 쓰기, conda 환경 생성 전부 됩니다.

## 문서 지도

| | |
|---|---|
| `~/shared/CLAUDE.md` | 규정 + 서버 사용법. **먼저 읽을 것** |
| `~/shared/TEAM/README.md` | **봇 올리는 곳** |
| `~/shared/docs/TOOLCHAIN.md` | 컴파일 상세 |
| `~/shared/docs/COLLAB.md` | 협업·권한·측정·제출. 예선에서 배운 것 포함 |
| `~/shared/docs/problem-1-next-vision.md` | **본선 문제 전문** |
| `~/shared/docs/PROBLEM-TOOLING.md` | 로컬 대전 도구 사용법 |
| `~/shared/docs/REFERENCE.md` | 예선 레포 — 참고용인 이유 |
| `~/shared/tools/README.md` | 도구 색인 |
