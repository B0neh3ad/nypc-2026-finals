"""맵 크기 버킷 (MoE 의 expert 단위).

규정: N = 2*NP+1, NP in [90,124]  ->  N in [181,249] 홀수 35종
     K = 2*KP+1, ceil(sqrt(N)-1) <= K <= floor(sqrt(N)+4), K 홀수

35개의 N 을 5개 버킷에 7개씩 나눈다. 버킷 하나가 expert 하나.
"""
import math

from . import util

NP_LO, NP_HI = 90, 124
N_BUCKETS = 5


def default_buckets(n_buckets=N_BUCKETS):
    total = NP_HI - NP_LO + 1                      # 35
    per = total // n_buckets                       # 7
    out = []
    for b in range(n_buckets):
        lo = NP_LO + b * per
        hi = NP_LO + (b + 1) * per - 1 if b < n_buckets - 1 else NP_HI
        out.append({
            "id": b,
            "name": "b%d_N%d-%d" % (b, 2 * lo + 1, 2 * hi + 1),
            "np_lo": lo,
            "np_hi": hi,
            "n_lo": 2 * lo + 1,
            "n_hi": 2 * hi + 1,
        })
    return out


def load_buckets():
    cfg = util.read_json(util.path("config", "buckets.json"))
    if not cfg:
        return default_buckets()
    return cfg["buckets"]


def get(bucket_id):
    for b in load_buckets():
        if b["id"] == int(bucket_id):
            return b
    raise SystemExit("bucket %s 없음" % bucket_id)


def k_range(n):
    """N 에서 허용되는 홀수 K 의 [lo, hi]. match.sh 와 같은 계산."""
    r = math.sqrt(n)
    lo = math.ceil(r - 1)
    hi = math.floor(r + 4)
    if lo % 2 == 0:
        lo += 1
    if hi % 2 == 0:
        hi -= 1
    return lo, hi


def map_for(bucket, index, seed):
    """버킷 안에서 index 번째 맵 (NP,KP). index 는 버킷의 N 들을 순환하고,
    K 는 시드로 고른다 — match.sh --map-sweep 과 같은 규칙."""
    nps = list(range(bucket["np_lo"], bucket["np_hi"] + 1))
    np_ = nps[index % len(nps)]
    n = 2 * np_ + 1
    klo, khi = k_range(n)
    cnt = (khi - klo) // 2 + 1
    k = klo + 2 * (seed % cnt)
    return np_, (k - 1) // 2


def game_plan(bucket, n_games, seed_base):
    """평가용 게임 목록. 모든 후보가 똑같은 맵/시드를 쓰도록 결정적으로 생성한다
    (짝지어진 비교 -> 분산 감소). 한 쌍은 같은 맵을 좌/우 바꿔 두 번 두므로
    진영 편향도 지워진다."""
    plan = []
    for i in range(n_games):
        pair = i // 2
        seed = seed_base + pair * 7919 + bucket["id"] * 1_000_003
        np_, kp = map_for(bucket, pair, seed)
        plan.append({"seed": seed, "np": np_, "kp": kp, "cand_is_left": (i % 2 == 0)})
    return plan
