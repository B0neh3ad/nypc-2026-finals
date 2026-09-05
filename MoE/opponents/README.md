# opponents — 점수를 재는 상대 패널 (비어 있음: 직접 채우세요)

여기 있는 봇들에 대한 **평균 승률**이 후보 파라미터의 score 다. 파일 하나 = 봇 하나,
파일명(확장자 제외)이 봇 이름이 된다. `.c/.cc/.cpp/.cxx` 는 채점기와 같은 플래그로 자동
컴파일되고, `.py` 는 `python3` 로 실행된다. 그냥 복사해 넣으면 끝이다.

```bash
cp ~/shared/TEAM/round_15_1430/inhyuk_v6.cpp  opponents/
cp ~/shared/problem/nation-fr-providing/sample-code.py opponents/sample.py
cp <튜닝할 봇 자신>                            opponents/self_baseline.cpp
```

## 고를 때 (여기가 결과를 제일 크게 좌우한다)

- **비용은 상대 수에 비례한다.** 상대 하나당 매 평가에 `games_per_opponent` 판이 더 붙는다.
- **승률이 0.00 이나 1.00 으로 박히는 상대는 정보가 0이다.** 파라미터를 어떻게 흔들어도
  같은 값을 돌려주므로 비용만 든다. `eval` 출력의 `per_opponent` 를 보고 걸러라.
- **같은 봇의 여러 버전은 한 스타일을 여러 번 세는 것**이다. 과적합을 부른다.
- **`self_baseline`(튜닝할 봇 자신의 복사본)은 항상 넣어라.** 기본 파라미터로 컴파일되므로
  정확히 거울 대국이고 score 0.5 에 고정된다. 0.5 를 넘으면 그게 진짜 개선이다.
  **절대 포화되지 않는 유일한 상대**다.
- 실력 차가 큰 상대는 `config/search.json` 의 `opponent_weights` 로 낮추거나 (0.0 이면
  점수에서 빠지고 WA 감시용으로만 남는다) 빼라. 안 적으면 전부 1.0.

## 바꾼 뒤에는

패널을 바꾸면 score 의 정의가 바뀐다. **`--tag` 를 새로 주고 새로 시작해라** —
옛 패널로 매긴 시행이 섞이면 GBT 가 엉뚱한 함수를 배운다.
