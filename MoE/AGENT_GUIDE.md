# MoE Parameter Search — Operating Guide

> For an agent (or person) who has never seen this directory. Read it top to bottom once;
> after that, section 3 is the only part you need open. Everything here was verified on the
> NYPC EC2 box on 2026-08-29. Korean overview: [README.md](README.md).

## 1. What this does

The bot for NEXT VISION (NYPC 2026 finals) is one C source with **209 integer tunables**
declared as `#ifndef NAME / #define NAME v / #endif`. Good values are not the same on a
181-zone map as on a 249-zone map, so this framework:

1. splits the legal map sizes (`N` odd, 181–249) into **5 buckets**, one *expert* each;
2. for each bucket, searches parameter values that maximize the **average win rate against
   a panel of our own earlier bots** (`opponents/`);
3. models `score = f(parameters)` with **gradient boosting**, and picks the next candidates
   by UCB (`mu + kappa*sigma`) instead of random search;
4. finally **assembles the 5 parameter sets into one source file** that selects its expert at
   runtime from the `N` it reads on the first line of input — because the judge takes one binary.

```
candidate params ──▶ compile with -DNAME=v ──▶ play the whole panel ──▶ score
        ▲                                                                │
        └───── fit GBT on all past (params, score), rank by UCB ◀────────┘
                    (5 buckets = 5 independent searches)
```

Parameters are injected as `-DNAME=v` at compile time. **The bot source is never edited**;
the `#ifndef` guards make the injection work as-is.

## 2. Environment invariants

| Thing | Value |
|---|---|
| Python with numpy/scikit-learn | `~/.conda/envs/moe/bin/python` (3.12.3, numpy 2.5.2, sklearn 1.9.0) |
| System `python3` | has **no** numpy/sklearn — the framework falls back to a pure-Python GBT, which works but is weaker |
| Compiler flags | same as the judge: `-std=gnu++20 -O2 -DONLINE_JUDGE -DNYPC -march=znver4` |
| Referee | `/srv/nypc/problem/nation-fr-providing/testing-tool.py` |
| Cost of one candidate | ~5 s compile + ~5 s for 48 games at `--jobs 6` |

**Rules you must not break** (contest regulations, see `/srv/nypc/CLAUDE.md`):

- **Never call subagent/Task tools.** Never use `/batch`, `/code-review`, `/fork`, `/workflow`,
  `/simplify`, `/ultraplan`, `ultrareview`. CPU parallelism is fine; LLM parallelism is not.
- **Always wrap local runs in `corepool.sh`.** 16 cores are shared by four people. Without it
  you oversubscribe the CPU, bots miss the 100 ms/turn limit, and your win rates measure
  server load instead of bot quality. This failure is silent.
  ```bash
  ~/shared/tools/corepool.sh 6 -- <command>
  ```
- Long sweeps do not belong on that box at all — submit them to slurm (section 7).
- No sudo. Do not try to work around it.

## 3. Commands

Set `PY=~/.conda/envs/moe/bin/python` and run everything from `~/MoE`.
Every command takes `--tag NAME` to keep separate runs apart (default `default`).

| Command | Use it when | Notes |
|---|---|---|
| `$PY run_search.py extract-space --src <bot.cpp> --force` | the bot source changed, or you switch to a different bot | rewrites `config/space.json` from the source's tunables; `--force` overwrites |
| `$PY run_search.py smoke --bucket 2` | first run on a new machine | plays exactly one game; prints `배선 정상` if referee + compiler + panel all work |
| `$PY run_search.py eval --bucket 2 --jobs 6` | you want the score of the **default** parameters (the baseline every result is compared against) | add `--params file.json` to score a specific set, `--record` to store it |
| `$PY run_search.py search --bucket B --jobs 6` | the actual search | one bucket per invocation; resumable |
| `$PY run_search.py report` | see per-bucket best vs baseline | reads the trial store, runs no games |
| `$PY run_search.py export` | write the 5 winning sets to `work/experts/params_moe.json` | needed before `assemble` |
| `$PY run_search.py assemble` | build the single submittable source | writes `work/experts/moe_bot.cpp` |
| `$PY run_search.py verify --jobs 6` | **always, before trusting anything** | replays assembled vs baseline on seeds the search never saw |

Typical full pass:

```bash
cd ~/MoE && PY=~/.conda/envs/moe/bin/python
$PY run_search.py extract-space --src ~/shared/TEAM/round_13_1400/jyp_v4.cpp --force
$PY run_search.py smoke --bucket 2
~/shared/tools/corepool.sh 6 -- $PY run_search.py eval --bucket 2 --jobs 6     # baseline
for b in 0 1 2 3 4; do scripts/local_search.sh $b 6; done                       # or: sbatch
$PY run_search.py report
$PY run_search.py export && $PY run_search.py assemble
~/shared/tools/corepool.sh 6 -- $PY run_search.py verify --jobs 6
```

Useful `search` flags (they override `config/search.json`):
`--games-per-opponent N` `--n-init N` `--batch N` `--max-iters N` `--max-seconds S` `--reeval-top K`

## 4. Files: what to edit, what to leave alone

**Edit these.** They are the knobs; the code reads them and nothing else defines behaviour.

- `config/search.json` — `bot_src` (which bot you are tuning), games per opponent, budget,
  GBT/UCB settings, `opponent_weights`, `tiebreak_weight`.
- `config/active_core.txt` — the list of parameter names the search is allowed to move.
  Currently 20. **Fewer names, better results per game played.**
- `config/space.json` — generated by `extract-space`, but the ranges are only heuristic guesses.
  Hand-tighten `low`/`high` when you know a parameter's sensible span; flip `active`.
- `opponents/` — the panel. See section 5.
- `config/buckets.json` — only if you change the number of buckets (then re-run everything).

**Do not edit** `moe/*.py` to change tuning behaviour — the configs cover it. Do not edit the
bot source to add parameters; add `#ifndef` guards in the bot itself and re-run `extract-space`.

`work/` is entirely regenerable *except* `work/trials/<tag>/bucketN.jsonl`, which is the record
of every game ever played. Deleting those throws away real compute.

## 5. Choosing the panel — this is the most consequential decision

The panel **defines** the score. Do not just dump every bot the team has written in there.

- **Cost is linear.** Each opponent adds `games_per_opponent` games to every single evaluation.
- **Saturated opponents contribute nothing.** If the candidate wins 100 % or loses 100 % against
  a bot, that bot returns the same number for every parameter set — it is pure cost, no gradient.
  Check the `per_opponent` block in `eval` output; anything pinned at `0.00` or `1.00` is dead weight.
- **Near-duplicates over-count one style.** `inhyuk_v4/v5/v6` are versions of one bot; including
  all three triples the weight of that opponent's habits and invites overfitting to them.
- **Always include `self_baseline`** — a copy of the candidate's own source. Compiled with default
  parameters it is an exact mirror, so it scores 0.5 by construction and *any* real improvement
  shows up as a number above 0.5. It is the one opponent that can never saturate.

`opponents/` **ships empty — the team fills it.** Copy files in; the filename minus its extension
becomes the bot name. Set per-opponent weights in `config/search.json` under `opponent_weights`
(absent = 1.0; `0.0` keeps the bot as a crash/WA tripwire without letting it move the score).

A worked example, measured on 2026-08-29 with `jyp_v4` as the candidate and a six-bot panel:

| Opponent | Candidate's win rate | Verdict |
|---|---|---|
| `self_baseline` | 0.50 | the anchor — exactly what you want |
| `inhyuk_v6`, `inhyuk_v4`, `sehyeon_v3` | 0.00 | all three saturated: no gradient at all |
| `jinsu_rush` | 0.75 | usable |
| `sample` | 1.00 | saturated; keep at weight 0 as a tripwire |

Three of six opponents were dead weight because `jyp_v4` is far weaker than the team's current
bots — the search would have been optimizing almost entirely against `self_baseline`. Check this
with one `eval` run **before** spending a search budget.

Whenever you change the panel, **start a new `--tag`**: old trials were scored under the old panel
and will teach the GBT the wrong function if they are mixed in.

## 6. Reading results, and the four ways this goes wrong

**Noise floor.** With 4 effective opponents × 12 games, the standard error is roughly ±0.07.
A `+0.02` improvement is nothing. `report` prints the baseline next to the best so you can see
the size of the claim; `stderr` is in every trial record.

| Symptom | What it means | Fix |
|---|---|---|
| Every candidate scores the same to 4 decimals | panel saturated (section 5) | rebuild the panel; `self_baseline` guarantees at least one live signal |
| `search` best is far above what `verify` reproduces | overfit to the fixed seed set | that is exactly what `verify` is for — trust `verify`, raise `games_per_opponent`, raise `reeval_top` |
| A trial scores exactly `0.0` with `err > 0` | the build crashed, timed out, or played an illegal move; ≥2 % ERR disqualifies a candidate | check the parameter combination by hand — usually a range in `space.json` is nonsense |
| `assemble` reports `pinned` names | those names are used in `#if` preprocessor directives, so they cannot vary per bucket; they were frozen at the middle bucket's value | accept it, or restructure that code in the bot |
| `assemble` says `hooked: false` | it could not find where the bot reads `N`, so no expert is ever selected | set `moe_hook_regex` in `config/search.json`, or insert `moe_set_map(N);` by hand right after `N` is parsed |

**Budget math.** 10 s per candidate. 300 candidates per bucket ≈ 50 min. Five buckets in parallel
on slurm ≈ under an hour for a real search. Compilation is the expensive half, and `work/bin/`
caches by `md5(source + defines)`, so re-running an identical candidate is free.

**Scoring detail.** `score = weighted mean win rate + tiebreak_weight × margin`, where `margin ∈ [0,1]`
rewards losing later and winning sooner (derived from the turn the game ended). Default weight
`0.05`, far below one game's worth of win rate, so it only breaks ties. Set it to `0` in
`config/search.json` for pure win rate.

## 7. slurm

One bucket per array task; tasks share no files, so they cannot corrupt each other.

```bash
cd ~/MoE && mkdir -p work/slurm
sbatch scripts/slurm_smoke.sbatch                                  # verify the node first
sbatch scripts/slurm_search.sbatch                                 # array 0-4
sbatch -p <partition> -A <account> scripts/slurm_search.sbatch     # named queue
sbatch --export=ALL,MOE_TAG=panel2 scripts/slurm_search.sbatch     # separate run
```

- `--cpus-per-task` becomes `--jobs`, i.e. concurrent games. 8 is a reasonable default.
- **Resume is automatic**: re-submitting the same command reads `work/trials/<tag>/bucketN.jsonl`
  and continues from the trials already there. A killed or preempted job loses at most one candidate.
- If the cluster has no `/srv/nypc`, run `scripts/pack_for_cluster.sh` — it copies the referee, the
  bot source and the panel into `bundle/` and tars the whole directory. Overridable paths:
  `MOE_ROOT`, `MOE_REFEREE`, `NYPC_SHARED`, `MOE_PYTHON`, `MOE_WORK`, `MOE_OPPONENTS`, `MOE_TAG`.
- A node without scikit-learn still runs (pure-Python GBT fallback), just less accurately.

## 8. Shipping the result

`assemble` produces `work/experts/moe_bot.cpp`: the original bot plus a `MoeParams` table with one
row per bucket, `#define NAME (g_moe->NAME)` for every parameter that differs between buckets, and
`moe_set_map(N)` inserted where the bot parses `N`. Parameters that are identical across buckets are
written back as plain constants, so there is no indirection cost for them.

Before submitting: run `verify`, then compile it the normal way and put it in the team folder.

```bash
~/shared/tools/build.sh work/experts/moe_bot.cpp -o /tmp/moe_bot        # judge flags
~/shared/tools/corepool.sh 1 -- taskset -c 0 /tmp/moe_bot NYPC          # if you need a timing number
cp work/experts/moe_bot.cpp ~/shared/TEAM/round_NN_HHMM/<yourname>_moe.cpp
```

Timing matters: the judge gives **one core**, sums CPU across threads, and allows 100 ms/turn with
five 100 ms extensions. Measure with both `corepool.sh 1` and `taskset -c 0`; either alone gives a
number the judge will not reproduce.

## 9. State as of 2026-08-29

Every command in section 3 has been run end to end at least once. `assemble` succeeded with 20
varying parameters, 0 pinned, and the runtime hook correctly placed; the assembled binary played
valid games at both `N=181` and `N=249`. **No search has been run at a real budget yet** — the
framework is standing, the results are not there.

`opponents/` is intentionally empty; the team assembles the panel. Fill it first (section 5),
run one `eval` to see the per-opponent breakdown, then start searching (section 3).
