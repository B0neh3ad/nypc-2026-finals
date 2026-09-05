"""시행 기록. 버킷별 jsonl 한 개 — 이어 돌리기(resume)와 슬럼 재시작의 근거."""
import os
import time

from . import util


def trials_path(bucket_id, tag=None):
    tag = tag or os.environ.get("MOE_TAG", "default")
    return util.work("trials", tag, "bucket%d.jsonl" % bucket_id)


def record(bucket_id, params, result, meta=None, tag=None):
    row = {
        "t": time.time(),
        "bucket": bucket_id,
        "params": params,
        "score": result["score"],
        "winrate": result.get("winrate"),
        "margin": result.get("margin"),
        "stderr": result.get("stderr"),
        "games": result.get("games"),
        "err": result.get("err"),
        "per_opponent": {k: round(v["winrate"], 4) for k, v in result.get("per_opponent", {}).items()},
    }
    if meta:
        row.update(meta)
    util.append_jsonl(trials_path(bucket_id, tag), row)
    return row


def load(bucket_id, tag=None):
    return util.read_jsonl(trials_path(bucket_id, tag))


def seen_keys(rows, space):
    return {space.key(r["params"]) for r in rows}


def best(rows, min_games=0):
    rows = [r for r in rows if (r.get("games") or 0) >= min_games]
    if not rows:
        return None
    return max(rows, key=lambda r: (r["score"], r.get("games") or 0))


def xy(rows, space):
    X = [space.encode(r["params"]) for r in rows]
    y = [r["score"] for r in rows]
    return X, y
