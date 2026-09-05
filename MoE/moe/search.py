"""버킷 하나에 대한 GBT 대리모형 탐색 (ask-tell).

  1) 초기 설계: 기본 파라미터 + 라틴하이퍼큐브 무작위 n_init 개를 실제 대전으로 평가
  2) 반복:
       - 지금까지의 (파라미터, score) 로 GBT 를 학습
       - 후보를 대량 생성 (무작위 + 상위 시행 주변 섭동)
       - UCB = mu + kappa*sigma 상위 batch 개만 실제 대전으로 평가
       - 기록 -> 다시 학습
  3) 마무리: 상위 후보를 새 시드로 재평가 (winner's curse 보정)

대전 자체가 노이즈이므로, 모든 후보는 같은 시드 집합에서 평가한다 (buckets.game_plan).
재평가 단계에서만 시드를 바꾼다.
"""
import random
import time

from . import buckets, evaluate, panel as panel_mod, store, surrogate, util
from .build import compile_bot
from .space import Space


def _lhs(space, n, rng):
    """라틴하이퍼큐브 — 차원마다 n 구간을 한 번씩 쓰게 섞는다."""
    d = space.dim
    cols = []
    for _ in range(d):
        col = [(i + rng.random()) / n for i in range(n)]
        rng.shuffle(col)
        cols.append(col)
    return [space.decode([cols[j][i] for j in range(d)]) for i in range(n)]


def propose(space, rows, cfg, rng):
    """GBT + UCB 로 다음 batch 를 고른다."""
    X, y = store.xy(rows, space)
    model = surrogate.make_surrogate(
        n_models=cfg.get("gbt_models", 6),
        n_estimators=cfg.get("gbt_trees", 300),
        max_depth=cfg.get("gbt_depth", 3),
        learning_rate=cfg.get("gbt_lr", 0.05),
        subsample=cfg.get("gbt_subsample", 0.8),
        seed=rng.randrange(10 ** 6),
    ).fit(X, y)

    top = sorted(rows, key=lambda r: -r["score"])[:max(3, len(rows) // 5)]
    n_cand = cfg.get("n_candidates", 4000)
    cands = []
    for i in range(n_cand):
        if i % 3 == 0 or not top:
            cands.append(space.sample(rng))                       # 순수 탐색
        else:
            base = top[rng.randrange(len(top))]["params"]
            sigma = cfg.get("perturb_sigma", 0.12) * (2.0 if i % 3 == 2 else 1.0)
            cands.append(space.perturb(base, rng, sigma))         # 국소 개선
    seen = store.seen_keys(rows, space)
    uniq, keys = [], set()
    for c in cands:
        k = space.key(c)
        if k in seen or k in keys:
            continue
        keys.add(k)
        uniq.append(c)
    if not uniq:
        return [space.sample(rng) for _ in range(cfg.get("batch", 8))], model

    mu, sd = model.predict([space.encode(c) for c in uniq])
    kappa = cfg.get("ucb_kappa", 1.5)
    ranked = sorted(range(len(uniq)), key=lambda i: -(float(mu[i]) + kappa * float(sd[i])))
    return [uniq[i] for i in ranked[: cfg.get("batch", 8)]], model


def evaluate_params(params, space, src, panel, bucket, cfg, jobs, seed_base):
    binpath = compile_bot(src, space.defines(params))
    return evaluate.evaluate(binpath, panel, bucket, cfg.get("games_per_opponent", 12),
                             seed_base, jobs=jobs, timeout=cfg.get("game_timeout", 180),
                             tiebreak_weight=cfg.get("tiebreak_weight", 0.05))


def _precompile(space, params_list, src, jobs):
    """배치를 평가 전에 병렬 컴파일해 둔다 (evaluate 쪽은 캐시에 맞는다).

    평가 루프가 순차라, 컴파일을 후보마다 직렬로 하면 그게 벽시계를 지배한다.
    """
    import concurrent.futures as _cf
    from . import build as _build
    uniq = {}
    for p in params_list:
        uniq.setdefault(tuple(space.defines(p)), p)
    items = list(uniq.keys())
    if len(items) <= 1:
        return
    t0 = time.time()
    with _cf.ThreadPoolExecutor(max_workers=min(jobs, len(items))) as ex:
        list(ex.map(lambda d: _build.compile_bot(src, list(d)), items))
    util.log("선컴파일 %d개 %.1fs" % (len(items), time.time() - t0))


def run(bucket_id, cfg, jobs=1, tag=None):
    space = Space.load(cfg.get("space"))
    bucket = buckets.get(bucket_id)
    src = cfg["bot_src"]
    panel = panel_mod.prepare(cfg.get("opponents"))
    seed_base = cfg.get("seed_base", 20260829)
    rng = random.Random(cfg.get("rng_seed", 7) * 1000 + bucket_id)
    deadline = time.time() + cfg["max_seconds"] if cfg.get("max_seconds") else None

    util.log("=== bucket %d (%s) | 활성 파라미터 %d개 | 패널 %d개 | jobs=%d ==="
             % (bucket_id, bucket["name"], space.dim, len(panel), jobs))

    rows = store.load(bucket_id, tag)
    util.log("이전 시행 %d개 이어받음" % len(rows))

    # ── 1) 초기 설계 ──────────────────────────────────────────
    n_init = cfg.get("n_init", 24)
    if len(rows) < n_init:
        todo = [space.defaults()] if not rows else []
        todo += _lhs(space, n_init - len(rows) - len(todo), rng)
        _precompile(space, todo, src, jobs)
        for i, p in enumerate(todo):
            if deadline and time.time() > deadline:
                break
            res = evaluate_params(p, space, src, panel, bucket, cfg, jobs, seed_base)
            row = store.record(bucket_id, p, res, {"stage": "init", "iter": i}, tag)
            rows.append(row)
            util.log("init %2d/%d  score=%.4f (승률 %.3f ±%.3f)"
                     % (i + 1, len(todo), res["score"], res["winrate"], res["stderr"]))

    # ── 2) GBT 안내 탐색 ──────────────────────────────────────
    it = 0
    while True:
        it += 1
        if cfg.get("max_iters") is not None and it > cfg["max_iters"]:
            break
        if deadline and time.time() > deadline:
            util.log("시간 예산 종료")
            break
        batch, model = propose(space, rows, cfg, rng)
        _precompile(space, batch, src, jobs)
        util.log("iter %d: 후보 %d개 평가 (지금까지 %d 시행, best=%.4f)"
                 % (it, len(batch), len(rows), store.best(rows)["score"]))
        for p in batch:
            if deadline and time.time() > deadline:
                break
            res = evaluate_params(p, space, src, panel, bucket, cfg, jobs, seed_base)
            row = store.record(bucket_id, p, res, {"stage": "gbt", "iter": it}, tag)
            rows.append(row)
            util.log("  score=%.4f (승률 %.3f ±%.3f)  best=%.4f"
                     % (res["score"], res["winrate"], res["stderr"], store.best(rows)["score"]))
        if hasattr(model, "importances") and it % 5 == 0:
            imp = model.importances([p["name"] for p in space.active])[:8]
            util.log("  중요도 상위: " + ", ".join("%s %.3f" % (n, v) for n, v in imp))

    # ── 3) 상위 재평가 (winner's curse 보정) ───────────────────
    k = cfg.get("reeval_top", 5)
    if k and rows:
        util.log("상위 %d개를 새 시드로 재평가" % k)
        for r in sorted(rows, key=lambda r: -r["score"])[:k]:
            res = evaluate_params(r["params"], space, src, panel, bucket, cfg, jobs,
                                  seed_base + cfg.get("reeval_seed_shift", 777_777))
            store.record(bucket_id, r["params"], res,
                         {"stage": "reeval", "orig_score": r["score"]}, tag)
            util.log("  재평가 %.4f -> %.4f" % (r["score"], res["score"]))

    final = store.best([r for r in store.load(bucket_id, tag) if r.get("stage") == "reeval"]) \
        or store.best(store.load(bucket_id, tag))
    util.log("bucket %d 최종 best score=%.4f" % (bucket_id, final["score"]))
    return final
