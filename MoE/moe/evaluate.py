"""후보 파라미터 하나를 패널 전체와 붙여 score 를 낸다.

score = 상대별 승률(승 1.0 / 무 0.5 / 패 0.0)의 가중 평균.
WA·TLE 로 진 판은 그냥 패배로 세되, 별도로 세어서 보고한다 (규칙 위반 봇이
운으로 살아남는 것을 막기 위해 err_rate 가 임계치를 넘으면 score 를 0으로 만든다).
"""
import concurrent.futures as cf

from . import buckets, panel as panel_mod, referee, util

ERR_KILL_RATE = 0.02          # 이 비율 이상 ERR 이면 후보를 실격 처리
MAX_TURN = 400


def margin_of(winner, reason, end_turn, cand_is_left):
    """승패만으로는 기울기가 안 생기는 상대(항상 지거나 항상 이기는 상대)에서
    쓰는 미세 신호. 0~1, 클수록 좋다.

      HQ 파괴로 짐  -> 오래 버틸수록 좋다        end_turn/400
      HQ 파괴로 이김 -> 빨리 끝낼수록 좋다        1 - end_turn/400 을 0.5 위로 얹음
      턴 제한       -> 본부 체력으로 이미 갈렸으니 중립 0.5
    """
    if winner == "ERR":
        return 0.0
    t = max(0, min(MAX_TURN, end_turn or MAX_TURN)) / float(MAX_TURN)
    if reason != "HQ_DESTROYED":
        return 0.5
    won = (winner == "LEFT") == cand_is_left
    return 0.5 + 0.5 * (1.0 - t) if won else 0.5 * t


def _one(job):
    r = referee.run_game(job["left"], job["right"], job["seed"], job["np"], job["kp"],
                         timeout=job.get("timeout", 180))
    r.update({"opponent": job["opponent"], "cand_is_left": job["cand_is_left"]})
    return r


def score_of(winner, cand_is_left):
    if winner == "DRAW":
        return 0.5
    if winner == "ERR":
        return 0.0
    return 1.0 if (winner == "LEFT") == cand_is_left else 0.0


def evaluate(cand_cmd, panel, bucket, games_per_opponent, seed_base, jobs=1, timeout=180,
             tiebreak_weight=0.05):
    plan = buckets.game_plan(bucket, games_per_opponent, seed_base)
    jobs_list = []
    for o in panel:
        for g in plan:
            jobs_list.append({
                "opponent": o["name"],
                "left": cand_cmd if g["cand_is_left"] else o["cmd"],
                "right": o["cmd"] if g["cand_is_left"] else cand_cmd,
                "seed": g["seed"], "np": g["np"], "kp": g["kp"],
                "cand_is_left": g["cand_is_left"], "timeout": timeout,
            })
    results = []
    if jobs <= 1:
        results = [_one(j) for j in jobs_list]
    else:
        with cf.ThreadPoolExecutor(max_workers=jobs) as ex:
            results = list(ex.map(_one, jobs_list))

    per = {}
    for o in panel:
        per[o["name"]] = {"games": 0, "points": 0.0, "margin": 0.0,
                          "win": 0, "draw": 0, "loss": 0, "err": 0}
    for r in results:
        s = per[r["opponent"]]
        s["games"] += 1
        pts = score_of(r["winner"], r["cand_is_left"])
        s["points"] += pts
        s["margin"] += margin_of(r["winner"], r.get("reason"), r.get("end_turn"), r["cand_is_left"])
        if r["winner"] == "ERR":
            s["err"] += 1
            s["loss"] += 1
        elif pts == 1.0:
            s["win"] += 1
        elif pts == 0.5:
            s["draw"] += 1
        else:
            s["loss"] += 1
    for k, s in per.items():
        s["winrate"] = s["points"] / s["games"] if s["games"] else 0.0
        s["margin"] = s["margin"] / s["games"] if s["games"] else 0.0

    ws = panel_mod.weights(panel)
    tot_w = sum(ws) or 1.0
    winrate = sum(w * per[o["name"]]["winrate"] for o, w in zip(panel, ws)) / tot_w
    margin = sum(w * per[o["name"]]["margin"] for o, w in zip(panel, ws)) / tot_w
    # score = 평균 승률 + (작은 가중치) x 미세 신호.
    # 가중치는 승패 하나를 절대 못 뒤집을 만큼 작아야 한다 (기본 0.05 << 1/판수).
    score = winrate + tiebreak_weight * margin
    n_games = sum(s["games"] for s in per.values())
    n_err = sum(s["err"] for s in per.values())
    if n_games and n_err / n_games >= ERR_KILL_RATE:
        util.log("  실격: ERR %d/%d" % (n_err, n_games))
        score = winrate = 0.0
    # 상대별 승률의 표준오차를 평균낸 값 — 노이즈 감각용
    se = 0.0
    for s in per.values():
        p = s["winrate"]
        se += (p * (1 - p) / s["games"]) ** 0.5 if s["games"] else 0.0
    se = se / max(1, len(per))
    return {"score": score, "winrate": winrate, "margin": margin, "stderr": se,
            "games": n_games, "err": n_err, "per_opponent": per}
