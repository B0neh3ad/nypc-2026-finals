#!/usr/bin/env python3
"""MoE 파라미터 탐색 CLI.

  extract-space  봇 소스의 #ifndef/#define 튜닝 블록 -> config/space.json 초안
  smoke          한 판만 돌려 심판기/봇/패널 배선 확인
  eval           파라미터 한 벌을 패널 전체와 붙여 score 확인
  search         버킷 하나에 대해 GBT 대리모형 탐색 (슬럼 태스크 한 개 = 이것)
  report         버킷별 결과 요약
  export         버킷별 best 파라미터 -> work/experts/params_moe.json
  assemble       5벌을 한 소스로 조립 (N 으로 expert 를 고르는 최종 제출본)
"""
import argparse
import json
import os
import random
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from moe import assemble as asm
from moe import buckets, evaluate, panel as panel_mod, referee, report, search, store, util
from moe.build import command_for, compile_bot
from moe.space import Space, build_space_config


def load_cfg(args):
    cfg = util.read_json(util.path("config", "search.json"), {}) or {}
    for k in ("bot_src", "games_per_opponent", "n_init", "batch", "max_iters",
              "max_seconds", "reeval_top", "seed_base"):
        v = getattr(args, k, None)
        if v is not None:
            cfg[k] = v
    if not cfg.get("bot_src"):
        raise SystemExit("config/search.json 의 bot_src 를 정하세요 (--bot-src 로도 가능)")
    cfg["bot_src"] = os.path.expanduser(cfg["bot_src"])
    return cfg


def cmd_extract_space(args):
    active = None
    if args.active_file and os.path.exists(args.active_file):
        active = {l.split("#")[0].strip() for l in open(args.active_file) if l.split("#")[0].strip()}
    cfg = build_space_config(os.path.expanduser(args.src), active)
    out = args.out or util.path("config", "space.json")
    if os.path.exists(out) and not args.force:
        raise SystemExit("%s 가 이미 있습니다 (--force 로 덮어쓰기)" % out)
    util.write_json(out, cfg)
    n_act = sum(1 for p in cfg["params"] if p["active"])
    print("파라미터 %d개 추출, 활성 %d개, 제외 %d개 -> %s"
          % (len(cfg["params"]), n_act, len(cfg["excluded"]), out))
    if not n_act:
        print("  ! active 가 0개입니다. config/active_core.txt 를 주거나 space.json 에서 active 를 켜세요.")


def cmd_smoke(args):
    cfg = load_cfg(args)
    space = Space.load()
    b = buckets.get(args.bucket)
    util.log("심판기: %s" % util.referee_path())
    util.log("봇 컴파일: %s" % cfg["bot_src"])
    cand = compile_bot(cfg["bot_src"], space.defines(space.defaults()))
    panel = panel_mod.prepare(cfg.get("opponents"))
    g = buckets.game_plan(b, 1, cfg.get("seed_base", 20260829))[0]
    util.log("한 판: N=%d K=%d seed=%d vs %s" % (2 * g["np"] + 1, 2 * g["kp"] + 1, g["seed"], panel[0]["name"]))
    r = referee.run_game(cand, panel[0]["cmd"], g["seed"], g["np"], g["kp"])
    print(json.dumps(r, ensure_ascii=False))
    print("배선 정상" if r["winner"] != "ERR" else "실패 — 위 stderr 확인")


def cmd_eval(args):
    cfg = load_cfg(args)
    space = Space.load()
    b = buckets.get(args.bucket)
    params = space.defaults()
    if args.params:
        params.update({k: int(v) for k, v in util.read_json(args.params).items() if k in space.by_name})
    panel = panel_mod.prepare(cfg.get("opponents"))
    res = search.evaluate_params(params, space, cfg["bot_src"], panel, b, cfg, args.jobs,
                                 cfg.get("seed_base", 20260829))
    print(json.dumps({"score": res["score"], "stderr": res["stderr"], "games": res["games"],
                      "err": res["err"],
                      "per_opponent": {k: round(v["winrate"], 3) for k, v in res["per_opponent"].items()}},
                     ensure_ascii=False, indent=2))
    if args.record:
        store.record(b["id"], params, res, {"stage": "manual"}, args.tag)


def cmd_search(args):
    cfg = load_cfg(args)
    best = search.run(args.bucket, cfg, jobs=args.jobs, tag=args.tag)
    print(json.dumps({"bucket": args.bucket, "score": best["score"]}, ensure_ascii=False))


def cmd_verify(args):
    """조립된 MoE 봇 vs 기준(기본 파라미터) 봇을 버킷마다 새 시드로 붙여 본다.
    탐색이 시드에 과적합했는지 여기서 드러난다."""
    cfg = load_cfg(args)
    space = Space.load()
    moe_src = args.moe_src or util.work("experts", "moe_bot" + os.path.splitext(cfg["bot_src"])[1])
    if not os.path.exists(moe_src):
        raise SystemExit("조립본이 없습니다: %s (먼저 assemble)" % moe_src)
    panel = panel_mod.prepare(cfg.get("opponents"))
    moe_cmd = command_for(moe_src)
    base_cmd = compile_bot(cfg["bot_src"], space.defines(space.defaults()))
    seed = cfg.get("seed_base", 20260829) + args.seed_shift
    print("%-14s %10s %10s %10s" % ("버킷", "기준", "MoE", "차이"))
    tot_b = tot_m = 0.0
    for b in buckets.load_buckets():
        rb = evaluate.evaluate(base_cmd, panel, b, args.games_per_opponent, seed,
                               jobs=args.jobs, tiebreak_weight=0.0)
        rm = evaluate.evaluate(moe_cmd, panel, b, args.games_per_opponent, seed,
                               jobs=args.jobs, tiebreak_weight=0.0)
        tot_b += rb["winrate"]; tot_m += rm["winrate"]
        print("%-14s %10.4f %10.4f %+10.4f" % (b["name"], rb["winrate"], rm["winrate"],
                                               rm["winrate"] - rb["winrate"]))
    nb = len(buckets.load_buckets())
    print("%-14s %10.4f %10.4f %+10.4f" % ("전체", tot_b / nb, tot_m / nb, (tot_m - tot_b) / nb))
    print("\n(승률만 비교. 판당 표준오차를 넘는 차이인지 꼭 확인할 것)")


def cmd_report(args):
    print(report.text_report(args.tag))


def cmd_export(args):
    out, obj = report.export_params(args.tag, out=args.out)
    print("내보냄: %s" % out)
    for b in obj["buckets"]:
        print("  %-14s score=%s  파라미터 %d개" % (b["name"], b["score"], len(b["params"])))


def cmd_assemble(args):
    cfg = load_cfg(args)
    src_params = util.read_json(args.params or util.work("experts", "params_moe.json"))
    if not src_params:
        raise SystemExit("먼저 export 를 도세요 (work/experts/params_moe.json 없음)")
    per_bucket = [b["params"] for b in src_params["buckets"]]
    if any(not p for p in per_bucket):
        raise SystemExit("파라미터가 빈 버킷이 있습니다 — 그 버킷 탐색을 먼저 도세요")
    out = args.out or util.work("experts", "moe_bot" + os.path.splitext(cfg["bot_src"])[1])
    r = asm.assemble(cfg["bot_src"], per_bucket, out,
                     default_bucket=args.default_bucket, hook_regex=cfg.get("moe_hook_regex"))
    print(json.dumps({k: v for k, v in r.items() if k != "error"}, ensure_ascii=False, indent=2)[:2000])
    if not r["ok"]:
        print("\n컴파일 실패:\n" + r.get("error", "")[-1500:], file=sys.stderr)
        sys.exit(1)
    if not r["hooked"]:
        print("\n! moe_set_map(N) 호출을 자동으로 못 넣었습니다. N 을 읽은 직후에 손으로 넣으세요.\n"
              "  config/search.json 의 moe_hook_regex 로 위치를 지정할 수도 있습니다.", file=sys.stderr)
    print("\n조립됨: %s  (버킷마다 다른 파라미터 %d개, 고정 %d개)"
          % (r["out"], len(r["varying"]), len(r["pinned"])))


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--tag", default=os.environ.get("MOE_TAG", "default"), help="실행 묶음 이름 (work/trials/<tag>)")
    sub = ap.add_subparsers(dest="cmd", required=True)

    p = sub.add_parser("extract-space")
    p.add_argument("--src", required=True)
    p.add_argument("--out")
    p.add_argument("--active-file", default=util.path("config", "active_core.txt"))
    p.add_argument("--force", action="store_true")
    p.set_defaults(func=cmd_extract_space)

    for name, fn in (("smoke", cmd_smoke), ("eval", cmd_eval), ("search", cmd_search)):
        p = sub.add_parser(name)
        p.add_argument("--bucket", type=int, default=0)
        p.add_argument("--jobs", type=int, default=int(os.environ.get("SLURM_CPUS_PER_TASK", "1")))
        p.add_argument("--bot-src", dest="bot_src")
        p.add_argument("--games-per-opponent", dest="games_per_opponent", type=int)
        p.add_argument("--n-init", dest="n_init", type=int)
        p.add_argument("--batch", type=int)
        p.add_argument("--max-iters", dest="max_iters", type=int)
        p.add_argument("--max-seconds", dest="max_seconds", type=int)
        p.add_argument("--reeval-top", dest="reeval_top", type=int)
        p.add_argument("--seed-base", dest="seed_base", type=int)
        if name == "eval":
            p.add_argument("--params", help="파라미터 json")
            p.add_argument("--record", action="store_true")
        p.set_defaults(func=fn)

    p = sub.add_parser("verify")
    p.add_argument("--jobs", type=int, default=int(os.environ.get("SLURM_CPUS_PER_TASK", "1")))
    p.add_argument("--games-per-opponent", dest="games_per_opponent", type=int, default=20)
    p.add_argument("--seed-shift", type=int, default=31_337, help="탐색에 쓰지 않은 시드로 옮긴다")
    p.add_argument("--moe-src"); p.add_argument("--bot-src", dest="bot_src")
    p.set_defaults(func=cmd_verify)

    p = sub.add_parser("report"); p.set_defaults(func=cmd_report)
    p = sub.add_parser("export"); p.add_argument("--out"); p.set_defaults(func=cmd_export)
    p = sub.add_parser("assemble")
    p.add_argument("--params"); p.add_argument("--out")
    p.add_argument("--bot-src", dest="bot_src")
    p.add_argument("--default-bucket", type=int, default=2)
    p.set_defaults(func=cmd_assemble)

    args = ap.parse_args()
    args.func(args)


if __name__ == "__main__":
    main()
