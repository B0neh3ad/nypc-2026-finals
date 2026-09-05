"""파라미터 공간 — 봇 소스의 튜닝 블록에서 추출하고, 정규화 벡터로 오간다."""
import hashlib
import re

from . import util

# 튜닝 대상에서 반드시 빼야 하는 것들 (자료구조 크기, 비트 플래그, 상수 별칭)
EXCLUDE_RE = re.compile(
    r"(MAX_TRACKED|MAX_COMBAT_SIM|RETREAT_MAX_UNITS|_FLAG_|^MAX_.*_UNITS$)")

BLOCK_START = "TUNING PARAMETERS"
BLOCK_END = "END TUNING PARAMETERS"


# ─────────────────────────── 추출 ───────────────────────────
def extract_defines(src_path):
    """`#ifndef X / #define X v / #endif` 쌍을 순서대로 뽑는다.
    튜닝 블록 마커가 있으면 그 안만, 없으면 파일 전체를 본다."""
    text = open(src_path, errors="replace").read().splitlines()
    lo, hi = 0, len(text)
    for i, line in enumerate(text):
        if BLOCK_END in line:
            hi = i
            break
    for i, line in enumerate(text):
        if BLOCK_START in line and BLOCK_END not in line:
            lo = i
            break
    out, pending = [], None
    for line in text[lo:hi]:
        m = re.match(r"\s*#ifndef\s+(\w+)", line)
        if m:
            pending = m.group(1)
            continue
        m = re.match(r"\s*#define\s+(\w+)\s+(\S+)", line)
        if m and pending == m.group(1):
            out.append((m.group(1), m.group(2)))
            pending = None
    return out


def guess_range(name, value):
    """이름과 기본값으로 탐색 범위를 추정한다. config/space.json 을 손으로 고쳐 쓰라고
    있는 초안일 뿐이다."""
    v = value
    if name.startswith("ENABLE_") or v in (0, 1) and re.search(
            r"(ENABLE|FORCE|SKIP|ONLY|KEEP_SOLVENT|FIRST|LOCK|STICKY|WAIT_FOR|STREAM|RELAX|CAP_TRAIN)", name):
        return 0, 1, "bool"
    if re.search(r"TURN$|_TURNS$|_UNTIL|_ETA$|COOLDOWN|DELAY|LOOKAHEAD|_DAYS$", name):
        lo, hi = max(0, v - 40), min(400, v + 40)
        if v == 0:
            lo, hi = 0, 40
        return lo, max(hi, lo + 1), "int"
    if re.search(r"PERCENT", name):
        return max(0, v - 25), min(100, v + 25), "int"
    if re.search(r"GOLD|RESERVE|COST|INCOME", name):
        span = max(50, abs(v) // 2)
        return max(0, v - span), v + span, "int"
    span = max(2, abs(v) // 2)
    lo, hi = max(-5 if v < 0 else 0, v - span), v + span
    # 분모로 쓰이는 값에 0 이 들어가면 0 나누기로 죽는다. 하한을 1 로 올린다.
    if name.endswith("_DEN") or "_DEN_" in name or name.endswith("_DIV"):
        lo = max(1, lo)
    return lo, max(hi, lo + 1), "int"


def build_space_config(src_path, active_names=None):
    params, skipped = [], []
    for name, raw in extract_defines(src_path):
        try:
            value = int(raw, 0)
        except ValueError:
            skipped.append({"name": name, "default": raw, "why": "정수가 아님(심볼 참조)"})
            continue
        if EXCLUDE_RE.search(name):
            skipped.append({"name": name, "default": value, "why": "튜닝 금지(구조/플래그)"})
            continue
        lo, hi, kind = guess_range(name, value)
        params.append({
            "name": name, "default": value, "low": lo, "high": hi,
            "kind": kind, "step": 1,
            "active": bool(active_names and name in active_names),
        })
    return {"source": src_path, "params": params, "excluded": skipped}


# ─────────────────────────── 공간 객체 ───────────────────────────
class Space:
    def __init__(self, cfg):
        self.all = cfg["params"]
        self.by_name = {p["name"]: p for p in self.all}
        self.active = [p for p in self.all if p.get("active")]
        for p in self.all:
            if p["low"] > p["high"]:
                raise SystemExit("범위가 뒤집힘: %s" % p["name"])

    @classmethod
    def load(cls, path=None):
        path = path or util.path("config", "space.json")
        cfg = util.read_json(path)
        if cfg is None:
            raise SystemExit("space.json 이 없습니다. run_search.py extract-space 를 먼저 도세요: %s" % path)
        return cls(cfg)

    @property
    def dim(self):
        return len(self.active)

    def defaults(self):
        return {p["name"]: p["default"] for p in self.active}

    # 정규화 [0,1] 벡터 <-> 파라미터 dict
    def encode(self, params):
        v = []
        for p in self.active:
            lo, hi = p["low"], p["high"]
            x = params.get(p["name"], p["default"])
            v.append(0.5 if hi == lo else (x - lo) / (hi - lo))
        return v

    def decode(self, vec):
        out = {}
        for p, u in zip(self.active, vec):
            lo, hi = p["low"], p["high"]
            u = min(1.0, max(0.0, u))
            x = lo + u * (hi - lo)
            step = p.get("step", 1) or 1
            x = lo + round((x - lo) / step) * step
            out[p["name"]] = int(min(hi, max(lo, round(x))))
        return out

    def sample(self, rng):
        return self.decode([rng.random() for _ in self.active])

    def perturb(self, params, rng, sigma=0.15):
        vec = self.encode(params)
        return self.decode([u + rng.gauss(0, sigma) for u in vec])

    def defines(self, params):
        """-DNAME=value 목록. 기본값과 같은 항목은 빼서 빌드 캐시 적중률을 올린다."""
        out = []
        for name, val in sorted(params.items()):
            p = self.by_name.get(name)
            if p is None or int(val) == int(p["default"]):
                continue
            out.append("-D%s=%d" % (name, int(val)))
        return out

    def key(self, params):
        """빌드 캐시/중복 평가 판별용 해시."""
        blob = ";".join("%s=%d" % (n, int(v)) for n, v in sorted(params.items()))
        return hashlib.md5(blob.encode()).hexdigest()[:12]
