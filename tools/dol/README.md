# tools/dol — 심판기 로그 정밀 분석 (jinyoung, 2026-08-29)

`replay.py` 가 공식 `testing-tool.py` 엔진으로 로그를 **재실행**해서 숨겨진 상태(양쪽 골드,
전사 위치/체력, 건물)를 매 턴 복원합니다. 로그의 결과 블록과 1:1 대조하므로 정확합니다
(불일치가 있으면 `MISMATCH` 를 찍음). 대회 사이트 로그(`runs/contest/round_*/battle_*.log`)에도 그대로 씁니다.

    python3 profile.py <log> LEFT|RIGHT [every]     # N턴마다 골드/전사/기지 + 건설·업그레이드 이벤트(직전 골드)
    python3 moves.py   <log> LEFT|RIGHT t0 t1       # 턴별 MOVE 명령 분류 (아군건물/중립거점/적기지/적본부/평지)
    python3 rules2.py  <log>...                      # 거점 선택 순서(홉·경로길이 순위), HQ 업글, 병력 시작, 집결지, 정찰
    python3 fights.py  <log> t0 t1                   # 턴별 교전 구역·피해
    python3 geo.py     <log> LEFT|RIGHT t...         # 특정 턴의 기지 배치(홉), 적 기지 시야/수비, 빈 거점, 스택
    python3 endgame.py <log> LEFT|RIGHT t0 t1        # 본부 근처 적/아군 분포 (방어 붕괴 추적)

돌 팀 분석 결과: `docs/dol-strategy.md`.
