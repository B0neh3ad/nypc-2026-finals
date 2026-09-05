"""score = f(parameters) 를 배우는 gradient boosting 대리모형.

sklearn 이 있으면 GradientBoostingRegressor 를 부트스트랩 배깅으로 여러 개 학습해
평균(mu)과 표준편차(sigma)를 낸다. sigma 가 있어야 UCB 로 탐색/활용을 조절할 수 있다.
sklearn 이 없는 환경(클러스터 노드 등)에서는 같은 인터페이스의 순수 파이썬 GBT 로
자동 대체된다 — 정확도는 떨어져도 파이프라인은 멈추지 않는다.
"""
import random

try:
    import numpy as np
    from sklearn.ensemble import GradientBoostingRegressor
    HAVE_SKLEARN = True
except Exception:                                   # pragma: no cover
    HAVE_SKLEARN = False


# ───────────────────────── sklearn 경로 ─────────────────────────
class SklearnGBT:
    def __init__(self, n_models=6, n_estimators=300, max_depth=3,
                 learning_rate=0.05, subsample=0.8, seed=0):
        self.cfg = dict(n_estimators=n_estimators, max_depth=max_depth,
                        learning_rate=learning_rate, subsample=subsample)
        self.n_models = n_models
        self.seed = seed
        self.models = []

    def fit(self, X, y):
        X = np.asarray(X, dtype=float)
        y = np.asarray(y, dtype=float)
        n = len(y)
        rng = np.random.default_rng(self.seed)
        self.models = []
        for b in range(self.n_models):
            idx = rng.integers(0, n, n) if n >= 8 else np.arange(n)
            m = GradientBoostingRegressor(random_state=self.seed + b, **self.cfg)
            m.fit(X[idx], y[idx])
            self.models.append(m)
        return self

    def predict(self, X):
        X = np.asarray(X, dtype=float)
        P = np.stack([m.predict(X) for m in self.models])       # (B, n)
        return P.mean(axis=0), P.std(axis=0)

    def importances(self, names):
        imp = np.mean([m.feature_importances_ for m in self.models], axis=0)
        return sorted(zip(names, imp.tolist()), key=lambda t: -t[1])


# ───────────────────────── 순수 파이썬 대체 ─────────────────────────
class _Tree:
    def __init__(self, depth, min_leaf):
        self.depth, self.min_leaf, self.node = depth, min_leaf, None

    def fit(self, X, y, idx):
        self.node = self._build(X, y, idx, self.depth)
        return self

    def _build(self, X, y, idx, depth):
        mean = sum(y[i] for i in idx) / len(idx)
        if depth == 0 or len(idx) < 2 * self.min_leaf:
            return ("leaf", mean)
        best = None
        base = sum((y[i] - mean) ** 2 for i in idx)
        for f in range(len(X[0])):
            order = sorted(idx, key=lambda i: X[i][f])
            sl = sr = 0.0
            nl, nr = 0, len(order)
            tot = sum(y[i] for i in order)
            sr = tot
            for k in range(len(order) - 1):
                i = order[k]
                sl += y[i]; sr -= y[i]; nl += 1; nr -= 1
                if nl < self.min_leaf or nr < self.min_leaf:
                    continue
                if X[order[k + 1]][f] == X[i][f]:
                    continue
                gain = base - (sum(y[j] ** 2 for j in order) - sl * sl / nl - sr * sr / nr)
                score = sl * sl / nl + sr * sr / nr
                if best is None or score > best[0]:
                    thr = 0.5 * (X[i][f] + X[order[k + 1]][f])
                    best = (score, f, thr)
        if best is None:
            return ("leaf", mean)
        _, f, thr = best
        left = [i for i in idx if X[i][f] <= thr]
        right = [i for i in idx if X[i][f] > thr]
        if not left or not right:
            return ("leaf", mean)
        return ("split", f, thr, self._build(X, y, left, depth - 1),
                self._build(X, y, right, depth - 1))

    def predict_one(self, x):
        n = self.node
        while n[0] == "split":
            n = n[3] if x[n[1]] <= n[2] else n[4]
        return n[1]


class PurePythonGBT:
    def __init__(self, n_models=4, n_estimators=60, max_depth=3,
                 learning_rate=0.1, subsample=0.8, seed=0, min_leaf=2):
        self.n_models, self.n_estimators = n_models, n_estimators
        self.max_depth, self.lr = max_depth, learning_rate
        self.subsample, self.seed, self.min_leaf = subsample, seed, min_leaf
        self.models = []

    def _fit_one(self, X, y, rng):
        base = sum(y) / len(y)
        resid = [v - base for v in y]
        trees = []
        for _ in range(self.n_estimators):
            idx = [i for i in range(len(y)) if rng.random() < self.subsample] or list(range(len(y)))
            t = _Tree(self.max_depth, self.min_leaf).fit(X, resid, idx)
            trees.append(t)
            for i in range(len(y)):
                resid[i] -= self.lr * t.predict_one(X[i])
        return base, trees

    def fit(self, X, y):
        self.models = []
        for b in range(self.n_models):
            rng = random.Random(self.seed + b)
            n = len(y)
            idx = [rng.randrange(n) for _ in range(n)] if n >= 8 else list(range(n))
            self.models.append(self._fit_one([X[i] for i in idx], [y[i] for i in idx], rng))
        return self

    def _predict_one(self, model, x):
        base, trees = model
        return base + self.lr * sum(t.predict_one(x) for t in trees)

    def predict(self, X):
        mu, sd = [], []
        for x in X:
            vals = [self._predict_one(m, x) for m in self.models]
            m = sum(vals) / len(vals)
            var = sum((v - m) ** 2 for v in vals) / max(1, len(vals) - 1)
            mu.append(m); sd.append(var ** 0.5)
        return mu, sd

    def importances(self, names):
        return [(n, 0.0) for n in names]


def make_surrogate(**kw):
    if HAVE_SKLEARN:
        return SklearnGBT(**{k: v for k, v in kw.items() if k in
                             ("n_models", "n_estimators", "max_depth", "learning_rate", "subsample", "seed")})
    return PurePythonGBT(**{k: v for k, v in kw.items() if k in
                            ("n_models", "n_estimators", "max_depth", "learning_rate", "subsample", "seed")})
