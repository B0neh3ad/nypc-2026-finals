"""버킷별 결과 요약과 파라미터 내보내기."""
from . import buckets, store, util
from .space import Space


def summarize(tag=None, space_path=None):
    space = Space.load(space_path)
    out = []
    for b in buckets.load_buckets():
        rows = store.load(b["id"], tag)
        if not rows:
            out.append({"bucket": b, "n_trials": 0})
            continue
        re_rows = [r for r in rows if r.get("stage") == "reeval"]
        best = store.best(re_rows) or store.best(rows)
        base = next((r for r in rows if r.get("stage") == "init" and r.get("iter") == 0), None)
        out.append({
            "bucket": b, "n_trials": len(rows),
            "best_score": best["score"], "best_params": best["params"],
            "baseline_score": base["score"] if base else None,
            "per_opponent": best.get("per_opponent", {}),
            "changed": {k: v for k, v in best["params"].items()
                        if int(v) != int(space.by_name[k]["default"])},
        })
    return out


def text_report(tag=None, space_path=None):
    rows = summarize(tag, space_path)
    lines = ["버킷별 결과 (tag=%s)" % (tag or "default"), ""]
    for r in rows:
        b = r["bucket"]
        if not r["n_trials"]:
            lines.append("  %-14s  시행 없음" % b["name"])
            continue
        base = r["baseline_score"]
        lines.append("  %-14s  시행 %4d  best %.4f%s"
                     % (b["name"], r["n_trials"], r["best_score"],
                        ("  (기본값 %.4f, %+.4f)" % (base, r["best_score"] - base)) if base is not None else ""))
        if r["per_opponent"]:
            lines.append("      상대별: " + "  ".join("%s %.2f" % (k, v) for k, v in sorted(r["per_opponent"].items())))
        if r["changed"]:
            lines.append("      바뀐 값 %d개: %s" % (len(r["changed"]),
                         ", ".join("%s=%s" % kv for kv in sorted(r["changed"].items())[:8])
                         + (" ..." if len(r["changed"]) > 8 else "")))
    return "\n".join(lines)


def export_params(tag=None, space_path=None, out=None):
    rows = summarize(tag, space_path)
    obj = {"buckets": [{"id": r["bucket"]["id"], "name": r["bucket"]["name"],
                        "n_lo": r["bucket"]["n_lo"], "n_hi": r["bucket"]["n_hi"],
                        "score": r.get("best_score"),
                        "params": r.get("best_params", {})} for r in rows]}
    out = out or util.work("experts", "params_moe.json")
    util.write_json(out, obj)
    return out, obj
