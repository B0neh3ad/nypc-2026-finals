# 노트북에서 실행하는 도구

서버가 아니라 **내 노트북**에서 돌립니다.

## nypc-setup — 접속 설정 (처음 한 번)

통합자가 보낸 키 파일을 받아서:

```bash
./nypc-setup <내-아이디> <받은-키-파일>
```

Tailscale 확인 → 키 설치(600) → `~/.ssh/config` 에 `Host nypc` 추가(백업) → 접속 확인
→ 서버에서 `doctor.sh` 까지 한 번에 합니다. **아무것도 덮어쓰지 않습니다** — 이미
있으면 알려주고 멈춥니다.

## nypc-push — 소스를 올려서 컴파일

```bash
cp nypc-push ~/bin/ && chmod +x ~/bin/nypc-push
nypc-push bot.cpp          # 파일에서
nypc-push -n myidea        # 클립보드에서
```

- 클립보드에 ` ```cpp 블록` 이 있으면 알아서 뽑아냅니다. 모델 답변을 설명 문장째
  복사해도 됩니다.
- `~/bots/<이름>-<시각>.cpp` 로 올라갑니다. **덮어쓰지 않습니다.**
- 바로 컴파일하고 오류를 이쪽 화면에 그대로 보여줍니다.

`--no-build` 로 올리기만, `-m native|static` 으로 빌드 모드 변경.
편의 도구일 뿐이라 `scp` + `ssh` 로 해도 결과는 같습니다.

## ctx.sh — 반대 방향

이 저장소를 못 보는 대화창에 붙여넣을 컨텍스트(실행 환경 제약, 확인된 규칙,
이미 버린 아이디어):

```bash
ssh nypc '~/shared/tools/ctx.sh' | pbcopy
```

현재 봇을 같이 넣으려면 `ctx.sh --bot ~/bots/xxx.cpp`, 문제 원문까지면 `--problem`.
