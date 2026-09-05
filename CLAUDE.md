# NYPC 2026 Finals — Working Rules

Four of us share **one folder directly** here (`/srv/nypc`). Do personal work in
your own home (`~`); put only what the team needs in here.

New here? [ONBOARD.md](ONBOARD.md) gets you working in five minutes.

## Never do these — contest regulations

- **No subagents.** Do not call the Agent/Task tools.
- **Forbidden commands**: `/batch`, `/code-review`, `/fork`, `/simplify`, `/ultraplan`, `/workflow`, `/effort ultracode`, `ultrareview`.
- **Allowed**: `/effort max`, `/goal`, `/loop`. **Conditional**: `/plan` (must not spawn subagents), `/branch` (not alongside the original session), `/schedule` (not concurrently).
- **No parallel test-time computing**: best-of-N, Deep Think, Research/Deep Research, cloud background agents.
- **Do not discuss the problem or solutions with anyone outside the team.**
- **Do not submit code written by anyone outside the team after the contest starts.** (LLM-generated code does not count.)

Full rules, verbatim from the organizers:
[docs/master-대회규칙.md](docs/master-대회규칙.md) (rules) and
[docs/master-개발도구사용안내.md](docs/master-개발도구사용안내.md) (dev tools).
A violation is **the contestant's responsibility even if nobody asked for it**.
When unsure, stop and ask a human.

## What the rules do *not* forbid

What is banned is **LLM parallelism**, not CPU parallelism. The organizers stated
explicitly that parallel tool calls are fine. So running 16 cores in parallel and
CPU-based parameter search are **all allowed**. That is ordinary computation, not
an agent.

## Working in a shared folder

**If two people edit the same file at once, whoever saves last silently wins.**
That is the price of a shared folder, and there is no way around it. Instead:

- A snapshot commit happens automatically every 2 minutes. Even if you get
  overwritten, `git log` gets it back.
  **Do not think about commits. They happen on their own.**
- Say something before making large changes to someone else's file.

To roll back:
```bash
cd /srv/nypc
git log --oneline -- <file>      # when did it change
git diff HEAD~5 -- <file>        # what changed
git checkout HEAD~5 -- <file>    # roll it back
```

## Where things go

During a contest nobody has time to tidy up — files just pile up. The qualifier working directory grew to 1,300 files in ten days, with analysis output
dumped loose at the top level and the site login sitting in a plain `.env`.
Deciding up front saves you from losing things later.

| What | Where | Why |
|---|---|---|
| Contest-site logins, tokens, keys | **Your home. Never the shared folder** | A snapshot commit would put it in git history permanently, for all four of you |
| Match results | `runs/` — **`match.sh` writes them there for you** | Appends one row per run to `runs/matches.tsv`; gitignored, so share conclusions in the channel |
| Replays, raw CSVs | `runs/` or your home | 60 KB per game; thousands of games is hundreds of MB |
| Compiled binaries | anywhere (git-ignored) | 1–3 MB every build |
| Any bot you want matched | `TEAM/` | One file, one bot; `match.sh` finds it by name - [TEAM/README.md](TEAM/README.md) |
| The organizers' distribution | `problem/` | As received, unmodified |

`.gitignore` already blocks logs, binaries, and credential-shaped filenames, but
a file with a different name gets committed anyway. **Default to keeping anything
big or secret outside the shared folder.**

## This account has no sudo

That is deliberate. Changing the system is not something an agent does in passing.

- Installing packages, changing system settings, changing permissions on someone
  else's files — **none of it is possible.**
- If you need it, tell the integrator. Administration happens by hand from a
  separate account (`ubuntu`).
- Do not go looking for a way around a blocked `sudo`. Blocked is the intended state.

Day-to-day work needs no `sudo` at all — building, writing to the shared folder,
corepool, creating jobs, and creating conda environments all work.

## EC2 is 16 cores shared by four people

Oversubscribing the CPU produces **fake timeouts** in anything with a time limit,
and then your measurements record server load instead of how good the code is.
It fails quietly, which makes it the most dangerous failure here.

- Always wrap parallel work on EC2: `tools/corepool.sh 6 -- <command>`
- Anything long-running or core-hungry does not run here at all — see below.

## The judge gives your program ONE core

The 16 cores above are **ours, for development**. They are not the judge's.

> 여러 쓰레드를 활용하여 문제를 해결하는 것도 가능합니다. 단, 프로그램에는 **한 개의
> CPU 코어만 할당되며**, 시간 및 메모리 제한을 측정할 때 **모든 쓰레드의 시간 및
> 메모리가 합산되어** 측정됩니다.
> — [docs/master-대회규칙.md](docs/master-대회규칙.md), `실행 및 채점 환경`

So threading the submitted binary to search wider **buys nothing and costs you
the time limit**: every thread's CPU time is summed against the same budget.
Spend the effort on a better single-threaded search instead.

Two separate limits are enforced, and they fail differently:

- **TLE** — CPU time (summed across threads) over the problem's limit.
- **WTLE** — wall-clock time over the limit. You can hit this while under the
  CPU limit — blocking, sleeping, or waiting on I/O does it.

To get a timing number that means anything on the judge, **pin to one core and
reserve it**:

```bash
tools/corepool.sh 1 -- taskset -c 0 ./main NYPC
```

Both halves are needed and they do different jobs. `corepool.sh` keeps teammates
off the core; `taskset` is what actually confines your program to one — under
`corepool.sh` alone a threaded binary still spreads across all 16 and reports a
time the judge will never reproduce.

## Compute-heavy work does not run on this instance

Large evaluation sweeps, parameter search, long self-play — none of it belongs on
this box. Running it anyway corrupts everyone else's timing measurements, and it
will not finish quickly enough to be worth it.

**Ask jinsu or jinyoung.** They have access to compute resources and will take the
job from there. Do not hunt for that compute yourself, and do not try to reach it
from this instance.

Build the binary here (`tools/build.sh <src> -m static`) and hand it over with a
one-line description of what you want run and what output you need back.

## Compiling

The same toolchain as the judge is installed — gcc **14.2.0**, `-std=gnu++20`,
Boost **1.90.0** (`/opt/boost/gcc`, the same path the judge uses).

```bash
tools/build.sh main.cpp                 # same codegen as the judge (default)
tools/build.sh main.cpp -m static       # binary leaving this instance (must be static)
```

Remember two things. **(1)** The judge runs AMD, so `-march=native` means something
different there than it does on our Intel box — `build.sh`'s default mode targets
the judge's side (`znver4`). **(2)** Any binary that leaves this instance must be
built with `-m static`; a dynamically linked one usually fails to start elsewhere.
Details in [docs/TOOLCHAIN.md](docs/TOOLCHAIN.md).

## Python

The judge's Python is 3.12.3, and so is the system `python3`. If you need packages,
use conda — shared base at `/opt/miniconda3`, with environments isolated per person
under `~/.conda/envs`.

```bash
conda create -n myenv python=3.12.3 numpy   # pin the version
conda run -n myenv python script.py         # use this in scripts and agents, not activate
```

`conda activate` does not exist in a non-interactive shell (it is a shell function —
this is where agents get stuck). Use `conda run` instead.

## The qualifier repo — reference only

The July qualifier code (NEXT NATION) is in `~/shared/reference` (= /srv/NYPC2026).

**Team policy: it is reference material. When in doubt, write from scratch.**
Read it to understand the rules and to see what the qualifier team decided and why.
Do not fork it. If some specific piece is clearly worth lifting, note why in
the channel first.

That policy stands even though the finals are the same game. We diffed the two
referees on 2026-08-29: `testing-tool.py` is **96.2% identical** (1599 vs 1641
lines, 124 changed) and the CLI is byte-for-byte the same. Useful to know — but
"the code runs" is not "the code is right for this problem":

- The qualifier bot evolved under **full information**. The finals have fog of war.
  Every place it assumes knowledge of the opponent is a bug now, scattered through
  10,731 lines, and you cannot be sure you found them all. Tuning on top of an
  unfound one is the same quiet failure as measuring server load.
- Constants moved far enough that the qualifier's balance point is not ours:
  750 starting gold (was 500), 400 days (was 200), maps of 181-249 zones (was
  51-109), base upgrades nearly flat at 500/550/600 (was 300/600/1000).
  **The tuned genomes in `champions/` are dead numbers.**
- Nobody has time mid-contest to audit someone else's 10,000 lines. 500 lines we
  wrote move faster than 10,000 we inherited.

What genuinely carries over is **method, not code**: fixed opponent panels, and
re-evaluating top candidates on fresh games to beat the winner's curse.
Both are in `reference/docs/STRATEGY.md`.

Full comparison and a per-file "what is this worth" table:
[docs/REFERENCE.md](docs/REFERENCE.md).

## The problem is out: NEXT VISION

A two-player turn-based strategy game. Build buildings, train warriors, destroy the
enemy HQ. 400 days, 181-249 zones on a point-symmetric graph, fog of war.
**Full statement: [docs/problem-1-next-vision.md](docs/problem-1-next-vision.md).**
**It is the qualifier game with new constants plus fog of war.** That makes the
qualifier repo useful background, not a starting point — see "The qualifier
repo" below.
How to run matches: [docs/PROBLEM-TOOLING.md](docs/PROBLEM-TOOLING.md).

The organizers' distribution is in `problem/`, unmodified:

| What | Where |
|---|---|
| Referee (interactor) | `problem/nation-fr-providing/testing-tool.py` |
| Organizers' README | `problem/nation-fr-providing/README.md` |
| Run config | `problem/nation-fr-providing/config.ini` |
| Sample bots, 12 languages | `problem/sample-code/` |
| Original zip | `problem/nation-fr-providing.zip` |

Run matches with the wrappers in `tools/`, not the referee directly - they
compile the way the judge does and record every result for you:

```bash
cd ~/shared/tools
./versus.sh mybot sample --seed 42 -l /tmp/log.txt   # one game + replay
./match.sh 200 mybot sample -j 6                     # 200 games, scored
```

Replays go in the viewer: `simulator/` (local mirror) or
https://d1thb30t7rs13h.cloudfront.net/.

### What we measured, so you do not measure it again

- **A full 400-turn match takes 0.36 s** with the Python samples on both sides.
  Experiments are cheap. Thousands of matches is minutes, not hours — but that is
  exactly the workload that oversubscribes the CPU, so keep using `corepool.sh`.
- A 12,000-line C++ bot builds through `tools/build.sh` in about 4 s.
- **The samples are not empty.** State tracking, visibility, and shortest paths are
  already written (`Paths`, `next_step`, `compute_visible`). Only `decide()` is a
  stub. Do not rewrite this from scratch — start from the sample for your language.
- `unzip` is not installed and this account cannot install it. Use
  `python3 -c "import zipfile; zipfile.ZipFile(f).extractall()"`.

### Two different distances — the easiest rule to get wrong

- **Movement** follows the path minimizing the sum of `ceil(euclidean)` between
  consecutive zone centers. Ties break toward the smaller zone number.
- **Vision** is 2 **hops** along the adjacency list. Not Euclidean.

The organizers' README restates the rules in English. **It does not contradict the
statement**, and almost everything in it is also in the statement. The only game
rule it adds is the row ordering inside each result section, which does not matter
unless you write a parser that depends on order. It also documents the log format:
stderr from your bot appears as `# Debug <LEFT/RIGHT>: <msg>`, and the last line is
`RESULT <LEFT_WIN|RIGHT_WIN|DRAW> <HQ_DESTROYED|TURN_LIMIT|WA>` — grep that line to
score a batch of matches.

## What we have not built yet

Submit tooling. That is it — build (`tools/build.sh`) and match
(`tools/versus.sh`, `tools/match.sh`) are done and in use.

Do not build a scoring or comparison layer on top of `match.sh` before you have a
strategy worth comparing. It already alternates sides, fixes map size, reports a
standard error, and appends every run to `runs/matches.tsv`.
