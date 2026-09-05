#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Panel match wrapper for ga_tuner.py.

The GA compiles a candidate bot binary and calls this script as its --match-cmd.
Instead of always fighting one fixed baseline, we rotate over a panel of strong
opponents (chosen by seed) so the tuned parameters generalize rather than
overfitting to a single opponent's style.

The candidate is placed on the side the GA asked for ({side}); ga_tuner's
parse_match_result()/run_one_match() then converts LEFT_WIN/RIGHT_WIN into a
candidate-relative score.

Usage (as ga_tuner --match-cmd):
  python3 panel_match.py --cand {bot} --side {side} --seed {seed}
"""

import argparse
import os
import subprocess
import sys

ROOT = os.path.dirname(os.path.abspath(__file__))
OPP_DIR = os.path.join(ROOT, "ga_work", "opponents")
TOOL = os.path.join(ROOT, "nation-providing", "testing-tool.py")

# Pre-compiled opponent binaries living in OPP_DIR (see build step).
# Panel for tuning 46175: six opponents spanning the gradient zone where the
# 46175 baseline is NOT saturated (measured baseline win-rate in parens), so
# every matchup has headroom to improve:
#   38525 (0.21)  38743 (0.25)  39623 (0.33)
#   44909-2 (0.50, requested sparring partner)  44909 (0.54)  37085 (0.67)
# Excludes the ones the baseline already wins handily (28235/31963/36551/39937)
# to keep the gradient concentrated on winnable-but-hard matchups.
OPPONENTS = ["38525", "38743", "39623", "44909-2", "44909", "37085"]


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--cand", required=True, help="candidate bot binary path")
    ap.add_argument("--side", required=True, choices=["left", "right"])
    ap.add_argument("--seed", type=int, required=True)
    args = ap.parse_args()

    opp = os.path.join(OPP_DIR, OPPONENTS[args.seed % len(OPPONENTS)])

    if args.side == "left":
        left, right = args.cand, opp
    else:
        left, right = opp, args.cand

    # -l /dev/null keeps the verbose per-turn log off stdout; only the final
    # "RESULT ..." line is printed for parse_match_result().
    cmd = ["python3", TOOL, "-a", left, "-b", right,
           "--seed", str(args.seed), "-l", "/dev/null"]
    result = subprocess.run(
        cmd,
        stdout=subprocess.PIPE,
        stderr=subprocess.DEVNULL,
        text=True,
    )
    # Forward the simulator's "RESULT ..." line for parse_match_result().
    sys.stdout.write(result.stdout)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
