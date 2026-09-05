# EVAL-PROTOCOL — how to tell whether a build actually got better

*Carried over from the qualifier (team 141 / inhyeok). The body below is the
acceptance rule we used to measure the v3 line; it is what let us claim
"+0.185 score, 95% CI [+0.163, +0.206]" instead of "looks better".*

## The one change worth making to `tools/match.sh` right now

`match.sh` already alternates sides — good, and necessary: in the qualifier we
scored **0.764 as LEFT vs 0.412 as RIGHT** (n=560), so an unalternated
comparison measures side, not skill.

But each game currently draws a **different seed**, so A-as-LEFT and B-as-LEFT
are played on **different maps**. The comparison therefore carries the full
map-to-map variance, which is far larger than the effect sizes we care about.
`build_jobs()` today:

```bash
if [ -n "${SEED_BASE:-}" ]; then seed=$((SEED_BASE + i - 1)); else seed=$RANDOM$RANDOM; fi
if (( i % 2 == 1 )); then left="$A"; right="$B"; ll="A"; rl="B"
else                     left="$B"; right="$A"; ll="B"; rl="A"; fi
```

Pair the seeds so each map is played **both ways** — games 2k-1 and 2k share a seed:

```bash
if (( i % 2 == 1 )); then
  if [ -n "${SEED_BASE:-}" ]; then seed=$(( SEED_BASE + (i-1)/2 )); else seed=$RANDOM$RANDOM; fi
  PAIR_SEED=$seed
  left="$A"; right="$B"; ll="A"; rl="B"
else
  seed=$PAIR_SEED
  left="$B"; right="$A"; ll="B"; rl="A"
fi
```

Then score the **pair** (A's result as LEFT + A's result as RIGHT on the same map),
not the individual games, and bootstrap over pairs. Map variance cancels inside the
pair. This is the whole reason our minimum detectable effect was 0.032 score at 120
games; `match.sh`'s current 100-game run reported se 4.9pp (~±9.6pp at 95%).

**Caveat carried from the qualifier:** never rate a candidate against its own parent.
See [qualifier/GA-sibling-selfplay-trap.md](qualifier/GA-sibling-selfplay-trap.md).

---

# PROTOCOL — when is a new build accepted as stronger?

This is the acceptance rule for replacing the current base build (`analysis/bin/107691.exe`, the round-30 entry).
The harness is `analysis/eval/gauntlet.py` (see README.md); the numbers backing every rule are in
`analysis/findings/next/gauntlet-baseline.md` and `analysis/eval/runs/*/summary.json`.

## Why not "candidate vs previous version"

Games are **deterministic** (same map, same builds -> byte-identical commands and result; only the measured-ms
lines jitter).  Same-lineage builds are so similar that self-play collapses to draws:

* 109855 mirror: 100/100 draws, exact point-reflections; vs sibling 107318: 87/100 draws (prior study).
* An actual parameter-sibling pair, 107509 vs 107691 (different source, different binary): **identical play on
  all 840 gauntlet cells** — a "beat your parent" fitness has zero gradient there, and 0 informative cells is
  also the correct verdict (the builds are the same policy in practice).
* The whole center-split lineage draws itself: base 107691 vs 97281 / 102712 / 104939 / 107318 = 120/120 draws
  against EACH (480/480 cells), and vs itself (T2) 120/120 — center wave-fights with both HQs stuck at L1.
* Cross-era anchors are decisive: base vs 38743 = 0.992, vs 83616 = 0.829 here (and 109855 vs 83616 = 0.815 in
  the prior 200-game study).

So a candidate is measured against the FIXED T1 pool on FIXED seeds, both sides, and compared with the base
**cell by cell**.  The pool's two halves are complementary: the cross-era anchors pin the absolute level, and the
all-draw lineage opponents are the sensitive detector — for both candidates tested, every behavioural difference
surfaced as broken draws against the lineage (57/720 informative cells for the real improvement 107691-v2,
468/720 for the broken patch base_v2) while the anchor games stayed byte-identical.

## The gauntlet run

```
python analysis/eval/gauntlet.py --cand <new build .exe|.cpp>            # defaults: base 107691, seeds 1..60,
                                                                         # tiles T1+T2+T3+T4, 8 workers
```

* T1 (headline): 38743, 83616, 97281, 102712, 104939, 107318 x seeds 1..60 x both sides = 720 paired cells.
* T2 (sibling 107691): 120 cells, reported separately — draws expected, a significant loss here is a warning.
* T3 (provided sample/rusher/boomer): 360 cells — regression guard only.
* T4 (140 recorded r24-30 games, opponent commands fixed): replay-delta guard, run before submitting.
* T5 (scripted archetypes `hunter`/`reinforcer`/`latemass`/`scaler`, reproducing the styles that beat us in
  the practice rounds - see `analysis/findings/next/opponent-pool-t5.md`): **regression guard only, like T3.**
  It is saturated (our builds win 100 %), so it is NOT a score axis and NOT GA fitness; a candidate that
  stops winning it has lost the ability to handle stack-raid aggression.  Its margin readout is a useful
  secondary signal (~30 % of cells differ between builds).
* `--quick` (20 seeds, T1 only, ~3 min with cache) for GA-loop screening; the full run for acceptance.

## Acceptance rule

Let dScore and dMargin be the candidate-minus-base means over the 720 T1 cells, with 95 % CIs from the paired
cluster bootstrap by seed (10 000 resamples).  **Accept the candidate iff all of the following hold:**

1. **Not weaker:** neither the dScore CI nor the dMargin CI lies entirely below 0.
2. **Better on at least one axis:** the dScore CI lies entirely above 0, or the dMargin CI does
   (margin counts because the judge's rating is margin-sensitive: ELO rose 1450->1700 at an unchanged 6/1/7 W/D/L).
3. **T3 + T5 guard:** 100 % wins vs sample, rusher and boomer, and vs the four T5 archetypes (opponent-WA wins count; handshake failures are excluded
   and surfaced; any loss or draw = REJECT).
4. **No own WA/TLE:** zero candidate aborts across all tiles.  The ms / token readouts are WALL-CLOCK under
   8-way parallel load: spikes cluster on consecutive seeds and hit both sides at once (e.g. 609 ms recorded,
   16 ms when the same cell is re-run alone).  On a >=100 ms warning, re-run the hot cells serially before
   treating it as real compute (our bot's true per-turn cost is < 20 ms).
5. **Sortie guard:** mean deaths at never-captured enemy bases (the trickle proxy, REPORT.md §11.2) not up by
   more than +1.0 vs the base on T1 (warning at the report level; investigate before shipping).
6. **T2 sanity:** candidate not significantly weaker than the base head-to-head (warning-level).
7. **T4 sanity (before submission):** dScore vs base on the 140 replays not significantly negative — the replayed
   opponents are the only real tournament opponents we have.

The harness prints the verdict (STRONGER / NO DETECTABLE DIFFERENCE / WEAKER / MIXED / REJECT(guard)) implementing
1+2 and the guards automatically.

## What the gauntlet can and cannot detect (validation, seeds 1..60)

* **Known-weaker build flagged:** 83616 as candidate -> `WEAKER`; dScore -0.258 [-0.306, -0.206], dMargin
  -6.58 [-7.84, -5.26]; both CIs exclude 0; 598/720 informative cells.  PASS.
* **Known-equivalent build:** 107509 (round-30 sibling) -> `NO DETECTABLE DIFFERENCE`; 0/840 informative cells
  (identical play).  PASS — no false positive is even possible for this pair.
* **Real candidates:** 107691-v2 (= submitted 109855) -> `STRONGER (both CIs exclude 0)`: dScore +0.030
  [+0.010, +0.053], dMargin +0.31 [+0.09, +0.56], the whole gain in map bucket 81-93 — exactly the bucket its
  forced-HQ2-save change targets.  base_v2 (v2 + patches F1/F2 as built in `next/base_v2.cpp`) -> `WEAKER`:
  dScore -0.298 [-0.317, -0.276], dMargin -4.85 [-5.37, -4.33], trickle deaths 4.0 vs 0.8 -> **do not ship
  base_v2; the F1/F2 integration is broken as-built.**
* **MDE** (80 % power, alpha .05, paired; per-seed sd of the delta):
  from the real-improvement pair (v2): dScore 0.055 / 0.032 / 0.017 and dMargin 0.60 / 0.35 / 0.19 hp at
  20 / 60 / 200 seeds; from the broken-patch pair: dScore 0.051 / 0.029 / 0.016, dMargin 1.31 / 0.76 / 0.42 hp.
  The 107509 pair gives literally 0 (the builds never differ) — with deterministic games the MDE is a property
  of the candidate's delta distribution, not of engine noise.  v2's true effect (+0.030) sits AT the 60-seed MDE:
  20 seeds would have missed it; use 60+ seeds for acceptance and `--seed-list 1-200` for a submission decision.

## Recommended GA fitness (instead of sibling self-play)

* **Fitness = mean score + w x mean margin vs the FIXED T1 pool, on COMMON seeds across the whole generation
  and across generations** (`--quick` = seeds 1..20, T1 only = 240 games per individual, ~1.5 min at 8 workers
  and fully incremental via the cache; w ~ 0.01 per hp keeps margin a tie-breaker at equal W/D/L, matching the
  judge's margin sensitivity — ELO rose 1450->1700 at an unchanged 6/1/7 W/D/L).
* Why not "beat your parent / sibling" self-play fitness — the numbers:
  1. **Draw saturation = fitness plateau.** Sibling matches are 87-100 % draws (109855 mirror 100/100;
     vs 107318 87/100; 107509 vs 107691 120/120 T2 draws with 0 informative cells anywhere).  Most mutations
     leave head-to-head fitness at exactly 0.5 and the GA random-walks.
  2. **No absolute scale.** Self-play fitness is relative to a moving opponent, so it is incomparable across
     generations.  The fixed pool + common seeds is one yardstick for the whole run: 107691-v2 0.667 / base
     0.637 / 83616 0.379 / broken patch 0.339, all on the same 720 cells.
  3. **A single sibling is one detector; the pool is six.** Both tested candidates happened to surface only
     against the lineage opponents, but a macro/economy change would move the anchor scores instead; the pool
     covers both, plus per-bucket and per-side splits for free.
* Screening at 20 seeds (MDE ~ 0.055 dScore) catches base_v2-sized breakage instantly but misses v2-sized gains
  (+0.030); promote GA champions to the full 60-seed gauntlet, and settle submission choices at 200 seeds
  (MDE ~ 0.017).
* Every accepted champion must pass the full acceptance rule above before it replaces the base, and then joins
  the T1 pool (add, never remove).

## Caveats

* The pool is our own lineage + provided bots; a candidate over-fitted to the pool can pass while regressing vs
  unseen archetypes.  T4 (real recorded opponents) is the partial hedge; refresh the pool with any new strong
  build that passes (add, never remove, so old runs stay comparable via the cache).
* Margin is HQ-hp margin at game end; WA games use +-30 by convention (surfaced separately, never silent).
* The bootstrap clusters by seed (map), the strongest dependence; opponents share cells within a seed.
