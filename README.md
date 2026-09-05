# NYPC 2026 Master Track Qualification Round (Team CPG)

**Passed the NYPC 2026 Master Track Qualification Round and advanced to the Finals**

---

## 1. Qualification Round Overview

| | Qualification Round | Finals |
|---|---|---|
| Dates | **Jun 29 - Jul 8 (10 days)** | Aug 29 (6 hours) |
| Format | Online, NYPC Arena | On-site, Seoul |
| Game | **NEXT NATION** | NEXT VISION |
| Scale | **1,603 teams entered** | 23 teams |
| Result | **Advanced to the Finals** | Bronze |

The Qualification Round let you keep improving for ten days with **no limit on the
number of submissions**. Rounds opened regularly, played a league against every
entrant, and refreshed the standings. Unlike the Finals (a single six-hour sprint),
this was **an environment where long-running automated optimization was possible**,
which is why we chose the genetic-algorithm approach described below.

### The game: NEXT NATION

A two-player turn-based strategy game on a randomly generated graph (51-109 zones).
You train warriors at your HQ, capture neutral strongholds and build bases on them to
grow income, and win by destroying the enemy HQ. If nothing is decided within 200
turns, the winner is determined by **remaining HQ health**.

NEXT VISION in the Finals is the successor to this game: constants rebalanced, plus
**fog of war** added. (96.2% identical at the engine-code level. See the Finals branch
README.)

---

## 2. Core Approach: Evolve the Parameters, Branch on Map Size

**The strategic logic was written by hand**, and only **the numbers inside it were
searched by a genetic algorithm**. No machine-learning model was trained.

### 1. Expose the strategy as parameters

`src/species2.cpp` (about 10,000 lines) exposes **123 values** as `#define`s: attack
timing, defensive judgment radius, opening expansion thresholds, minimum troop balance,
and so on. The GA never touches the code structure, only **the values**.

### 2. A different expert per map size (Mixture of Experts)

The key finding that emerged during the search: **the optimal parameters differ by map
size.** A tight 51-zone map and a sprawling 109-zone map wanted nearly opposite
settings for reinforcement distance, opening tempo, and how hard to contest the center.

So instead of one global parameter set, we evolved **four experts**, each evaluated
only on maps in its own size band.

| Expert | Map sizes | Final generation | Mean score vs opponent panel |
|---|---|---|---|
| 0 (small) | 51-63 | 4 | **0.844** |
| 1 (small-mid) | 65-79 | 7 | 0.821 |
| 2 (mid-large) | 81-93 | 26 | 0.803 |
| 3 (large) | 95-109 | 25 | 0.830 |

(The per-opponent scores for each expert are still there in the header comment of the
final submission, `submission/107691.cpp`.)

For submission, all four sets are **baked into a single program** that branches to its
own expert the moment it reads the map.

```c
int e = (N <= 63) ? 0 : (N <= 79) ? 1 : (N <= 93) ? 2 : 3;
```

**19** of the 123 values appear in preprocessor `#if` directives and therefore change
the shape of the code itself, so all four experts must agree on them to share one
executable. Those 19 were frozen and only the remaining 104 evolved per expert.

### 3. Fitness: mean win rate against a fixed opponent panel

Each candidate was scored on its mean result against **7 fixed opponents** (win 1.0 /
draw 0.5 / loss 0.0), alternating sides and evaluated on fresh maps from its own band
each generation. The panel deliberately mixed types: five standard bots plus a
continuously pressuring rush (`anchor_rush2`) and a center-breaking attacker
(`center2_attack`), so that a candidate beating only one type could not climb to the
top.

Two things were excluded on purpose.

- **Past champions were never recycled as opponents.** If the opponents change each
  generation, the meaning of the score changes with them and generations become
  incomparable. The target has to stay fixed for improvement to be measurable.
- **No self-play.** Playing yourself is symmetric, so the win rate pins to 0.5 and
  produces no gradient.

### 4. Correcting for the winner's curse

Pick the maximum out of hundreds of candidates and that value is structurally
inflated: what gets selected is a lucky sample, not skill. A 50-game screen has a
standard error of about 0.07, so a 0.05 "improvement" is indistinguishable from noise.

So we filtered in three stages.

1. Screen all candidates over 50 games (cheap and rough)
2. Give only the top candidates **200 more games on fresh maps**, then re-rank on the
   pooled 250
3. Verify the noisiest bands out to **1,000 games**

Why stage 3 was necessary: in the rush-defense band on mid-size maps, in-run scores
inflated by 0.05 to 0.15, and **what looked like four consecutive generations of
improvement evaporated entirely under verification.** Without this procedure that
genome would have gone into the submission.

The same lesson carried into the Finals. See "The measurement trap" in the Finals
README.

**Full methodology: [`docs/STRATEGY.md`](docs/STRATEGY.md)**

---

## 3. Repository Layout

| Path | Contents |
|---|---|
| `src/species2.cpp` | The bot itself (10,731 lines, 123 parameters) |
| `src/ga_*.py` | Genetic algorithm (search, MoE evolution, parameter space definition) |
| `src/bake_moe_submit.py` | Bakes the four experts into a single submission |
| `src/run_moe.sbatch` | Cluster (Slurm) run script |
| `champions/expert*.jsonl` | Final genomes per band, plus per-generation records |
| `submission/107691.cpp` | **Final submission** |
| `opponents/` | The fixed opponent panel used for fitness |
| `engine/nation-providing/` | Organizers' referee and local match tooling |
| `simulator/` | Replay viewer (log files excluded for size) |
| `docs/RULES_ko.md` | Game rules |
| `docs/STRATEGY.md` | Full strategy and GA/MoE methodology |
| `tools/` | Match automation and contest-record collection scripts |
| `rounds/` | Per-round evaluation results and submission mapping |

### `tools/`: automation

Managing 270 submissions over ten days was not something to do by hand.

| Script | What it does |
|---|---|
| `match.sh` / `versus.sh` | Play two bots for n games and aggregate results (side alternation, fixed map size option) |
| `panel_match.py` | Play the whole fixed opponent panel and compute fitness |
| `download_all_logs.py` | Collect per-round match logs automatically |
| `download_all_submissions.py` | Bulk-collect submitted code |
| `export_round_evaluations.py` | Export round evaluation detail (result, time, memory, side) as JSON |
| `build_round_mapping.py` | Build the round-to-submission mapping |
| `run_all.py` | Run the three collection scripts above in order |
| `map_size_match_report.py` | Win-rate report by map-size band. **The evidence behind the MoE split points** |

Credentials are read only from environment variables (`NYPC_ID` / `NYPC_PW`) and are
not included in this repository.

---

## 4. What Carried Into the Finals

Of everything built during the qualifier, some was actually used in the Finals and
some was dropped.

**Carried over: the methodology**

- Scoring against a fixed opponent panel
- Winner's-curse correction (re-evaluating top candidates)
- The idea of branching parameters by map size, reimplemented as the Finals `MoE/`
  framework
- The design of the match automation and record collection tooling, rewritten in the
  Finals as `match.sh` and the poller

**Dropped: the bot code itself**

Once **fog of war** was added in the Finals, the qualifier bot's premise that all
opponent actions are visible collapsed. Code depending on opponent information was
scattered across 10,000 lines with no way to be sure we had found all of it, and the
constants had moved far enough that the qualifier's balance point no longer held.

So in the Finals we treated the qualifier code as **reference only**, taking the rules
understanding and the methodology while rewriting the bot for the new rules.

**A weakness we saw coming: rush defense**

We finished the qualifier with one unresolved problem: *on mid-size maps, we cannot
stop a continuous, back-to-back rush.* Hand edits did not fix it, and neither did
dozens of GA generations; that band stayed at a 0.45-0.55 win rate. The conclusion at
the time was that it needed **a new defensive mechanism, not more parameter tuning**.

And that is exactly what knocked us down in the Finals. A 33% win rate against the
early rush, and against the staged rush that appeared later (massing at a stronghold,
then charging again) our defense rate slid from 86.7% to 74.8% to 66.7%. The hole we
papered over with parameters in the qualifier showed through unchanged in a six-hour
final.

---

## 5. Original Repository

The qualifier output is also organized in a separate public repository:
**[ishlove77/NYPC2026](https://github.com/ishlove77/NYPC2026)**

This branch adds to that the **automation tooling and round records that only ever
existed in the working directory** (`tools/`, `rounds/`).
