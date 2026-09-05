#!/usr/bin/env python3
import argparse
import csv
import os
import shlex
import subprocess
import sys
import time
from concurrent.futures import ThreadPoolExecutor, as_completed
from dataclasses import dataclass
from pathlib import Path
from typing import Dict, Iterable, List, Optional, Tuple


ROOT = Path(__file__).resolve().parents[1]
SUB_DIR = ROOT / "submissions"
BUILD_DIR = ROOT / ".build"
TOOL = ROOT / "nation-providing" / "testing-tool.py"


RAW_FIELDS = [
    "N",
    "game",
    "seed",
    "K",
    "left",
    "right",
    "outcome",
    "reason",
    "winner",
    "elapsed_ms",
]


@dataclass(frozen=True)
class Task:
    n: int
    game: int
    seed: int
    k: int
    left: str
    right: str
    left_cmd: str
    right_cmd: str
    timeout: float


def die(message: str) -> None:
    print(f"error: {message}", file=sys.stderr)
    raise SystemExit(1)


def resolve_source(arg: str) -> Path:
    p = Path(arg)
    if p.is_file():
        return p.resolve()
    p = SUB_DIR / arg
    if p.is_file():
        return p.resolve()
    for ext in ("cpp", "cc", "cxx", "c", "py"):
        p = SUB_DIR / f"{arg}.{ext}"
        if p.is_file():
            return p.resolve()
    hits = sorted(SUB_DIR.glob(f"{arg}.*"))
    if hits:
        return hits[0].resolve()
    die(f"source not found for {arg}")


def build_command(source: Path) -> str:
    ext = source.suffix.lower()
    if ext == ".py":
        return f"{shlex.quote(os.environ.get('PYTHON', 'python3'))} {shlex.quote(str(source))}"

    BUILD_DIR.mkdir(parents=True, exist_ok=True)
    output = BUILD_DIR / source.stem
    if ext == ".c":
        compiler = os.environ.get("CC", "gcc")
        flags = shlex.split(os.environ.get("CFLAGS", "-O2"))
    elif ext in (".cpp", ".cc", ".cxx"):
        compiler = os.environ.get("CXX", "g++")
        flags = shlex.split(os.environ.get("CXXFLAGS", "-O2 -std=c++17"))
    else:
        die(f"unsupported source type: {source}")

    if not output.exists() or not os.access(output, os.X_OK) or source.stat().st_mtime > output.stat().st_mtime:
        cmd = [compiler, *flags, "-o", str(output), str(source)]
        print(f">> compiling {source.name}", file=sys.stderr)
        subprocess.run(cmd, check=True)
    return shlex.quote(str(output.resolve()))


def allowed_k_values(n: int) -> List[int]:
    k_lo = (3 * n + 19) // 20
    if k_lo % 2 == 0:
        k_lo += 1
    k_hi = n // 5
    if k_hi % 2 == 0:
        k_hi -= 1
    return list(range(k_lo, k_hi + 1, 2))


def map_k_for_seed(n: int, seed: int) -> int:
    values = allowed_k_values(n)
    return values[seed % len(values)]


def parse_result(stdout: str) -> Tuple[str, str]:
    for line in reversed(stdout.splitlines()):
        if line.startswith("RESULT "):
            parts = line.split()
            outcome = parts[1] if len(parts) >= 2 else "ERR"
            reason = parts[2] if len(parts) >= 3 else ""
            return outcome, reason
    return "ERR", "NO_RESULT"


def run_one(task: Task) -> Dict[str, str]:
    started = time.perf_counter()
    np_value = (task.n - 1) // 2
    kp_value = (task.k - 1) // 2
    cmd = [
        sys.executable,
        str(TOOL),
        "-a",
        task.left_cmd,
        "-b",
        task.right_cmd,
        "--seed",
        str(task.seed),
        "--NP",
        str(np_value),
        "--KP",
        str(kp_value),
        "-l",
        "/dev/null",
    ]

    try:
        result = subprocess.run(
            cmd,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            timeout=task.timeout,
        )
    except subprocess.TimeoutExpired:
        outcome, reason = "ERR", "TIMEOUT"
    else:
        outcome, reason = parse_result(result.stdout)
        if outcome == "ERR" and result.stderr.strip():
            reason = result.stderr.strip().splitlines()[-1][:120]

    if outcome == "LEFT_WIN":
        winner = task.left
    elif outcome == "RIGHT_WIN":
        winner = task.right
    elif outcome == "DRAW":
        winner = "DRAW"
    else:
        winner = "ERR"

    elapsed_ms = int(round((time.perf_counter() - started) * 1000))
    return {
        "N": str(task.n),
        "game": str(task.game),
        "seed": str(task.seed),
        "K": str(task.k),
        "left": task.left,
        "right": task.right,
        "outcome": outcome,
        "reason": reason,
        "winner": winner,
        "elapsed_ms": str(elapsed_ms),
    }


def read_existing(raw_path: Path) -> Tuple[set[Tuple[int, int]], Dict[Tuple[int, int], int], List[Dict[str, str]]]:
    if not raw_path.exists():
        return set(), {}, []
    rows: List[Dict[str, str]] = []
    done: set[Tuple[int, int]] = set()
    attempts: Dict[Tuple[int, int], int] = {}
    with raw_path.open(newline="") as f:
        for row in csv.DictReader(f):
            if not row:
                continue
            rows.append(row)
            key = (int(row["N"]), int(row["game"]))
            attempts[key] = attempts.get(key, 0) + 1
            if row.get("winner") != "ERR":
                done.add(key)
    return done, attempts, rows


def append_raw(raw_path: Path, row: Dict[str, str]) -> None:
    exists = raw_path.exists()
    with raw_path.open("a", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=RAW_FIELDS)
        if not exists:
            writer.writeheader()
        writer.writerow(row)


def summarize(rows: Iterable[Dict[str, str]], player_a: str, player_b: str) -> List[Dict[str, object]]:
    by_n: Dict[int, Dict[str, int]] = {}
    for row in rows:
        n = int(row["N"])
        bucket = by_n.setdefault(n, {player_a: 0, player_b: 0, "DRAW": 0, "ERR": 0, "total": 0})
        winner = row["winner"]
        if winner in (player_a, player_b, "DRAW"):
            bucket[winner] += 1
            bucket["total"] += 1
        else:
            bucket["ERR"] += 1

    out: List[Dict[str, object]] = []
    for n in sorted(by_n):
        bucket = by_n[n]
        total = bucket["total"]
        a_wins = bucket[player_a]
        b_wins = bucket[player_b]
        draws = bucket["DRAW"]
        out.append(
            {
                "N": n,
                "games": total,
                f"{player_a}_wins": a_wins,
                "draws": draws,
                f"{player_b}_wins": b_wins,
                "errors": bucket["ERR"],
                f"{player_a}_win_pct": 100.0 * a_wins / total if total else 0.0,
                "draw_pct": 100.0 * draws / total if total else 0.0,
                f"{player_b}_win_pct": 100.0 * b_wins / total if total else 0.0,
            }
        )
    return out


def write_summary_csv(path: Path, rows: List[Dict[str, object]], player_a: str, player_b: str) -> None:
    fields = [
        "N",
        "games",
        f"{player_a}_wins",
        "draws",
        f"{player_b}_wins",
        f"{player_a}_win_pct",
        "draw_pct",
        f"{player_b}_win_pct",
        "map_gen_retries",
    ]
    with path.open("w", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=fields)
        writer.writeheader()
        for row in rows:
            clean = dict(row)
            for key in (f"{player_a}_win_pct", "draw_pct", f"{player_b}_win_pct"):
                clean[key] = f"{float(clean[key]):.1f}"
            clean["map_gen_retries"] = clean.pop("errors")
            writer.writerow(clean)


def write_markdown(path: Path, rows: List[Dict[str, object]], player_a: str, player_b: str) -> None:
    lines = [
        f"# {player_a} vs {player_b} by map size",
        "",
        f"| N | games | {player_a} W | Draw | {player_b} W | {player_a} W% | Draw% | {player_b} W% | map-gen retries |",
        "|---:|---:|---:|---:|---:|---:|---:|---:|---:|",
    ]
    for row in rows:
        lines.append(
            "| {N} | {games} | {aw} | {draws} | {bw} | {ap:.1f} | {dp:.1f} | {bp:.1f} | {errors} |".format(
                N=row["N"],
                games=row["games"],
                aw=row[f"{player_a}_wins"],
                draws=row["draws"],
                bw=row[f"{player_b}_wins"],
                ap=row[f"{player_a}_win_pct"],
                dp=row["draw_pct"],
                bp=row[f"{player_b}_win_pct"],
                errors=row["errors"],
            )
        )
    lines.append("")
    path.write_text("\n".join(lines))


def svg_text(x: float, y: float, text: object, cls: str = "", anchor: str = "start") -> str:
    cls_attr = f' class="{cls}"' if cls else ""
    return f'<text{cls_attr} x="{x:.1f}" y="{y:.1f}" text-anchor="{anchor}">{text}</text>'


def write_svg(path: Path, rows: List[Dict[str, object]], player_a: str, player_b: str) -> None:
    width, height = 1280, 760
    left, right, top, bottom = 76, 34, 78, 92
    plot_w = width - left - right
    plot_h = height - top - bottom
    base_y = height - bottom

    bar_gap = 5
    slot = plot_w / len(rows)
    bar_w = max(8, slot - bar_gap)

    parts = [
        '<svg xmlns="http://www.w3.org/2000/svg" width="1280" height="760" viewBox="0 0 1280 760">',
        "<style>",
        "text{font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',sans-serif;fill:#1f2933;font-size:12px}",
        ".title{font-size:22px;font-weight:700}.sub{fill:#52606d}.axis{font-size:13px;font-weight:600}",
        ".tick{fill:#62717f;font-size:11px}.legend{font-size:12px}.grid{stroke:#d9e2ec;stroke-width:1}",
        ".a{fill:#2f9e44}.d{fill:#9aa5b1}.b{fill:#2b6cb0}",
        "</style>",
        '<rect width="1280" height="760" fill="#ffffff"/>',
        svg_text(left, 34, f"{player_a} vs {player_b}: outcome distribution by map size", "title"),
        svg_text(left, 56, "1000 games per N, random map generated for every game, sides alternated", "sub"),
    ]

    legend_x = width - 365
    legend_y = 32
    for idx, (cls, label) in enumerate((("a", f"{player_a} win"), ("d", "draw"), ("b", f"{player_b} win"))):
        x = legend_x + idx * 122
        parts.append(f'<rect class="{cls}" x="{x}" y="{legend_y - 10}" width="13" height="13" rx="2"/>')
        parts.append(svg_text(x + 19, legend_y + 1, label, "legend"))

    for pct in range(0, 101, 20):
        y = base_y - plot_h * pct / 100
        parts.append(f'<line class="grid" x1="{left}" y1="{y:.1f}" x2="{width - right}" y2="{y:.1f}"/>')
        parts.append(svg_text(left - 10, y + 4, f"{pct}%", "tick", "end"))

    for idx, row in enumerate(rows):
        x = left + idx * slot + (slot - bar_w) / 2
        total = float(row["games"] or 1)
        a_pct = float(row[f"{player_a}_wins"]) / total
        d_pct = float(row["draws"]) / total
        b_pct = float(row[f"{player_b}_wins"]) / total

        b_h = plot_h * b_pct
        d_h = plot_h * d_pct
        a_h = plot_h * a_pct
        y = base_y
        if b_h > 0:
            parts.append(f'<rect class="b"><title>N={row["N"]}: {player_b} win {row[f"{player_b}_wins"]}/{int(total)} ({b_pct * 100:.1f}%)</title></rect>'.replace("<rect class=\"b\">", f'<rect class="b" x="{x:.1f}" y="{y - b_h:.1f}" width="{bar_w:.1f}" height="{b_h:.1f}">'))
            y -= b_h
        if d_h > 0:
            parts.append(f'<rect class="d"><title>N={row["N"]}: draw {row["draws"]}/{int(total)} ({d_pct * 100:.1f}%)</title></rect>'.replace("<rect class=\"d\">", f'<rect class="d" x="{x:.1f}" y="{y - d_h:.1f}" width="{bar_w:.1f}" height="{d_h:.1f}">'))
            y -= d_h
        if a_h > 0:
            parts.append(f'<rect class="a"><title>N={row["N"]}: {player_a} win {row[f"{player_a}_wins"]}/{int(total)} ({a_pct * 100:.1f}%)</title></rect>'.replace("<rect class=\"a\">", f'<rect class="a" x="{x:.1f}" y="{y - a_h:.1f}" width="{bar_w:.1f}" height="{a_h:.1f}">'))
        parts.append(svg_text(x + bar_w / 2, base_y + 18, row["N"], "tick", "middle"))

    parts.append(f'<line x1="{left}" y1="{base_y}" x2="{width - right}" y2="{base_y}" stroke="#334e68" stroke-width="1.2"/>')
    parts.append(f'<line x1="{left}" y1="{top}" x2="{left}" y2="{base_y}" stroke="#334e68" stroke-width="1.2"/>')
    parts.append(svg_text(width / 2, height - 31, "map size N", "axis", "middle"))
    parts.append(svg_text(18, top + plot_h / 2, "outcome share", "axis", "middle"))
    parts.append("</svg>")
    path.write_text("\n".join(parts))


def make_tasks(
    player_a: str,
    player_b: str,
    cmd_a: str,
    cmd_b: str,
    games: int,
    seed_base: int,
    timeout: float,
    done: set[Tuple[int, int]],
    attempts: Dict[Tuple[int, int], int],
) -> List[Task]:
    tasks: List[Task] = []
    for n in range(51, 110, 2):
        for game in range(1, games + 1):
            if (n, game) in done:
                continue
            prior_attempts = attempts.get((n, game), 0)
            seed = seed_base + n * 1_000_000 + game - 1 + prior_attempts * 1_000_000_000
            k = map_k_for_seed(n, seed)
            if game % 2 == 1:
                left, right = player_a, player_b
                left_cmd, right_cmd = cmd_a, cmd_b
            else:
                left, right = player_b, player_a
                left_cmd, right_cmd = cmd_b, cmd_a
            tasks.append(Task(n, game, seed, k, left, right, left_cmd, right_cmd, timeout))
    return tasks


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--a", default="72865")
    parser.add_argument("--b", default="83616")
    parser.add_argument("--games", type=int, default=1000)
    parser.add_argument("--seed-base", type=int, default=20260706)
    parser.add_argument("--workers", type=int, default=max(1, min(8, (os.cpu_count() or 2) // 2)))
    parser.add_argument("--timeout", type=float, default=30.0)
    parser.add_argument("--prefix", default=None)
    args = parser.parse_args()

    source_a = resolve_source(args.a)
    source_b = resolve_source(args.b)
    cmd_a = build_command(source_a)
    cmd_b = build_command(source_b)

    prefix = args.prefix or f"match_{args.a}_{args.b}_by_N_{args.games}"
    raw_path = ROOT / f"{prefix}_raw.csv"
    summary_csv_path = ROOT / f"{prefix}.csv"
    md_path = ROOT / f"{prefix}.md"
    svg_path = ROOT / f"{prefix}.svg"

    done, attempts, existing_rows = read_existing(raw_path)
    tasks = make_tasks(args.a, args.b, cmd_a, cmd_b, args.games, args.seed_base, args.timeout, done, attempts)
    total_needed = 30 * args.games
    print(f"existing={len(done)} pending={len(tasks)} total={total_needed} workers={args.workers}", flush=True)

    rows = list(existing_rows)
    completed = len(done)
    last_report = time.time()
    started = time.time()

    if tasks:
        with ThreadPoolExecutor(max_workers=args.workers) as executor:
            futures = [executor.submit(run_one, task) for task in tasks]
            for future in as_completed(futures):
                row = future.result()
                append_raw(raw_path, row)
                rows.append(row)
                completed += 1
                now = time.time()
                if completed % 200 == 0 or now - last_report >= 30:
                    elapsed = now - started
                    rate = (completed - len(done)) / elapsed if elapsed > 0 else 0.0
                    print(f"progress={completed}/{total_needed} rate={rate:.1f}/s last_N={row['N']} last={row['winner']}", flush=True)
                    last_report = now

    summary = summarize(rows, args.a, args.b)
    write_summary_csv(summary_csv_path, summary, args.a, args.b)
    write_markdown(md_path, summary, args.a, args.b)
    write_svg(svg_path, summary, args.a, args.b)

    print(f"raw={raw_path}")
    print(f"summary_csv={summary_csv_path}")
    print(f"markdown={md_path}")
    print(f"svg={svg_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
