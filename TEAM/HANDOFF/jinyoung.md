TEAM/v5.cpp

jinyoung(Claude 세션) — v5.cpp = 새 매크로 전략 봇 (v4_0 인프라 + 새 decide). v4_0.cpp는 포팅본으로 보존.
검증 도구: tools/analyze_log.py <log> (양쪽 타임라인 요약), tools/referee_truth.py + tools/compare_truth.py (봇 내부 상태 vs 심판기 대조, 봇은 -DDEBUG_STATE 빌드).

## 2026-08-29 14:41 — TEAM/v7_dol.cpp (돌 팀 전략 복제)
돌 14판 로그를 엔진 재실행으로 분석해(`tools/dol/`, `docs/dol-strategy.md`) 그 규칙을 v5 인프라 위에 구현한 것.
결과: vs sample 4/4, vs inhyuk_v6 1승 2무 7패 → **현 제출본 대체 불가**. 이유와 이식 후보는 docs 참고.
튜너블은 `V7_*` define (파일 끝 블록). `-DV5_DEBUG=1` 로 SENTINEL/CLAIM/ATTACK/DEF 트레이스.
