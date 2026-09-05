# v3 / v3_1 changelog — what changed since the submitted build, and what it is worth

*Written incrementally during the final merge (2026-08-26).  Every number in this file is a
`analysis/eval/gauntlet.py` measurement with its run tag; nothing is estimated.  Acceptance rule:
`analysis/eval/PROTOCOL.md`.*

## 0. Answer first

**Final build: `next/v3_1.cpp` / `next/v3_1.exe` (md5 `d33ea375`).  It is STRONGER than the build we
submitted (109855), by +0.158 score [+0.133,+0.182] and +2.43 hp of margin [+1.99,+2.89] over 2,400
paired cells at 200 seeds — 4.5× the minimum detectable effect — with all seven PROTOCOL rules
passing and identical per-turn compute (1.1 ms serial, 5 overtime tokens unused).  Submit it.**

### Lineage

```
107691  (round-30 entry, analysis/bin/107691.exe)
  └─ 107691-v2 = next/v3_base.cpp/.exe  = the SUBMITTED build 109855          <- the thing to beat
       └─ next/v3.cpp/.exe   = v3_base + f1f2 + center(e86) + trickle + evac   (wave 1)
            └─ next/v3_1.cpp/.exe = v3 + pingpong retreat-fix
                                       + saver/finisher with the cap-train HQ gate   (wave 2, here)
```

All builds compile with
`g++ -O2 -include build/getline_compat.h -x c++ <src> -o <exe>`.
The complete `v3 → v3_1` source delta is `next/patches/v3_1_from_v3.diff`; the one-token gate that
turned the saver from score-negative into shippable is `next/patches/saverD_capgate.diff`.

---

## 1. What `v3` is (wave 1) — four patches, all measured vs `v3_base`

| patch | REPORT §11 item | what it does | 60-seed T1 dScore | dMargin | run tag | verdict |
|---|---|---|---|---|---|---|
| **f1f2** | 1 | F1 hoists the HQ level-up above the staged-attack early return in `issue_hard_advantage_conversion`; F2 issues the L1→L2 upgrade after the defence modules, placed immediately **before** the forced-HQ2 block and gated so it cannot eat the center-split wave budget (the naive integration `base_v2` collapsed to 0.339 exactly there); the patch also widens `P_FORCE_HQ2_SAVE_TURN` from `{0,0,85,0}` to `{85,85,85,85}`, i.e. the forced-HQ2 save now runs in all four map buckets instead of only `81–93` | **+0.187 [+0.154,+0.219]** | +1.82 [+1.44,+2.19] | `f1f2_b_full` | STRONGER |
| **center (e86)** | mirror-draw study | bounded exit for the infinite center wave-loop: `center_second_base_opening_active` now times out and releases the economy at `P_CENTER_SPLIT_ECON_AFTER_TURN = 86`, plus a standby-keep guard in the move choke point so the release does not drain the HQ | **+0.153 [+0.108,+0.197]** | +2.42 [+1.74,+3.09] | `centerB_full` | STRONGER |
| **trickle** | 2 | never send an under-strength sortie; re-attack memory (`P_REATTACK_FORGET_TURNS = 30`); offense-reinforcement travel gate | −0.007 [−0.028,+0.014] | −0.24 [−0.70,+0.23] | `trickle_full` | no detectable difference (kept for the behaviour it enforces, not for score) |
| **evac (F1)** | §11-7 adjacent | the round-30 evacuation bug: launch-guard so a wave is not launched into a region we are about to abandon | +0.005 [−0.005,+0.016] | +0.04 [−0.12,+0.22] | `evac_f1_full` | no detectable difference |

**`v3` as a whole vs `v3_base`** (tag **`v3_clean3`** — the clean re-play of `v3_clean2`, see §3.1;
60 seeds, T1 = 720 paired cells):

```
dScore  +0.187 [+0.143,+0.230]      dMargin +2.02 [+1.35,+2.70]     informative 456/720
T1 cand 0.853 / +6.02   base 0.667 / +4.00      W/D/L 538/153/29
T2 (sibling 107691) dScore +0.221 [+0.154,+0.287]      T3 360/360 wins      VERDICT STRONGER
```

f1f2 and center are the whole gain and they are not additive (each alone is already ≈ +0.15…+0.19):
they fix the same failure mode from two sides — an HQ that never leaves L1 because the center wave
loop eats every 600 gold.

---

## 2. Wave 2 — what was merged now, and what was not

### Merged

* **pingpong — retreat fix only** (`next/patches/pingpong.diff`, report `impl-pingpong.md`).
  `issue_losing_fight_retreats` no longer issues a retreat out of a region that still holds an enemy
  warrior: the engine (`apply_day_movement`) only advances a warrior whose *current* region is
  enemy-free, so such an order can never resolve — it latches the body into `MOVING` state where no
  module can re-task it and where every stationary-warrior count stops seeing it.  Instrumented on 12
  cells: **42 of 42** retreat orders `v3_base` issues are of exactly this impossible kind; the fix
  takes pinned-retreat deaths from 1.808 to 0.271 per game (−85 %).
  vs `v3_base`: dScore **+0.014 [+0.001,+0.035]** (tag `pingpong_final`).
  The ping-pong **veto** ships compiled in but **disabled** (`P_PINGPONG_VETO_TURNS {0,0,0,0}`):
  at the tested window 15 it is behaviourally strong (reversals −73 %, quick reversals −88 %,
  +1.7 pp income-earning warrior-days) but score-neutral and margin-negative
  (dScore +0.003 [−0.012,+0.018], dMargin −0.29 [−0.67,+0.11]), so it stays a GA gene.
* **saver + finisher** (`next/patches/saver.diff`, report `impl-saver.md`) — REPORT §11 items 5 + 6.
  Threat-conditioned late HQ saver from t140 with a 2-hop threat release, and a conservative
  finisher (cap-training + relaxed HQ targeting + streaming) that **wires the never-called
  `issue_endgame_hq_attack`** (architecture §5.1 latent bug #1).
  vs `v3_base`: dScore +0.003 [−0.003,+0.011], dMargin **+0.194 [+0.000,+0.394]**, T4 dScore 0.000
  (PROTOCOL rule 7 passes), T4 dMargin −0.59 (mostly WA-convention artifacts; −0.11 excluding them).
  Tags `saver_full` / `saver_t4`.
  **Merged in a modified form.**  Re-measured on top of `v3` the patch as written is score-negative
  (§4); it ships with one extra gate — the finisher's cap-training now waits for our own next HQ
  level, exactly as its assault already did (§5).  That version is what `v3_1` contains.

### Not merged (diffs kept)

* **concentr** (`next/patches/concentr.diff`, `impl-concentr.md`) — nothing STRONGER on any axis:
  T1 dScore +0.001 [−0.020,+0.021], dMargin −0.13.  Only its build-veto sub-variant
  (`src_vetoonly`) does anything real (T4 bases lost −10.6 %); the L2 floor is provably a regression.
* **posture** (`next/patches/posture.diff`, `impl-posture.md`) — score-negative:
  dScore −0.013 [−0.033,+0.000], dMargin −0.21.  The **detector** is excellent (it converts the two
  named tournament losses `29_238870` and `30_251179`), so this is a future lever, not something to
  ship today.

---

## 3. Merge trail (this session)

Order: `v3` → apply pingpong retreat fix → apply saver.  Both patches were built against `v3_base`
and re-anchored onto `v3`.

| step | source | build | integration notes |
|---|---|---|---|
| 0 | `next/v3.cpp` | `next/v3.exe` | wave-1 baseline |
| 1 | `next/work/final/src.step1_pp.cpp` | `next/work/final/step1_pp.exe` | `patch` applied all 7 pingpong hunks with offsets, **no rejects**.  Hand-verified the shared function `add_move_action_ex_stack_flags`: the pingpong veto sits right after the contested-source check, center's standby-keep guard is still in place after `stack_guard_would_feed`, and `pingpong_note_order` sits after the `VEC_PUSH` — **both guards preserved**.  MoE row ships as `{0,0,0,0}`. |
| 2 | `next/work/final/src.step2_saver.cpp` | `next/work/final/step2_saver.exe` | 8 of 9 saver hunks applied with offsets; hunk 9 (the appended MoE rows) **rejected**, because `v3` already appends center/trickle rows at the tail of `moe_apply_expert_params`.  Re-applied by hand after `P_REATTACK_FORGET_TURNS`, so all 16 saver/finisher genes exist with their tested values and **no existing row is renumbered**. |
| 2C | `next/work/final/srcC.cpp` | `next/work/final/step2C_saverC.exe` | the saver `srcC` variant (adds `P_FINISHER_ATTACK_WAIT_FOR_HQ` / `P_FINISHER_KILL_SLACK_TURNS`: the *assault* waits while our own next HQ level is still reachable, cap-training does not) — built for the head-to-head in §4. |

### Quick screens (20 seeds, T1, 240 paired cells, base = `next/v3.exe`)

| step | dScore | dMargin | informative | verdict |
|---|---|---|---|---|
| 1 — v3 + pingpong retreat fix (`final_pp_quick`) | **+0.004 [−0.013,+0.025]** | +0.13 [−0.04,+0.33] | 10/240 | no detectable difference (both point estimates ≥ 0) |
| 2 — + saver (`final_saver_quick`) | −0.015 [−0.052,+0.017] | **+0.19 [−0.29,+0.75]** | 32/240 | no detectable difference |

Neither addition trips the 0.05 quick-dScore bisect trigger (step 2's marginal cost is −0.019, well
inside this pair's own 20-seed MDE of 0.050), and no guard fails: 0 own WA/TLE aborts, trickle proxy
flat (0.287 vs 0.287), 0 losses by HQ kill.  Because the quick screen sits *below* its own MDE, the
saver-vs-saverC choice was settled at 60 seeds instead — §4.


### 3.1 Cache hygiene before the final measurements

`v3`'s cached cells were timing-poisoned: 838 of 1200 had `us_ms_max >= 100`, 566 ended with
`tokens_left = 0`, and four (`97281` seeds 31/32/34) carried `us_ms_max = 244578` — the signature of a
gauntlet process suspended by a killed background task while four agents shared the machine.
Per the hygiene rule this needs **both** `analysis/eval/cache/<exe_md5>/` and
`analysis/eval/logs/<label>-<md5prefix>/` deleted, so both were purged and all 1200 v3 cells were
replayed on an idle machine (tag **`v3_clean3`**):

```
T1 720 | cand 0.853 / +6.02  base 0.667 / +4.00
dScore +0.187 [+0.143,+0.230]   dMargin +2.02 [+1.35,+2.70]   informative 456/720 (58/60 seeds)
T2 dScore +0.221 [+0.154,+0.287]      T3 360/360 wins      VERDICT STRONGER
ms mean 3.132 | 2.679   ms max 63 | 78   tokens left min 5 | 5   own aborts 0 | 0
```

Identical verdict and dScore to the poisoned `v3_clean2` run; the margin moved +2.06 → +2.02 because
a handful of token-starved cells really had played differently.  **All later runs in this changelog
use these clean `v3` cells as the base.**

---

## 4. The saver does not survive the move onto `v3` — 60-seed evidence

The saver was measured on top of `v3_base`, where it was worth +0.003 score / **+0.194 margin**.
Re-measured on top of `v3` (60 seeds, T1 = 720 paired cells, base = clean `v3` cells):

| build | dScore [95 % CI] | seed sign | dMargin [95 % CI] | inform | T3 | verdict |
|---|---|---|---|---|---|---|
| `v3 + pp + saver` (`final_saver60`) | **−0.011 [−0.028,+0.005]** | 13+/28− p=0.028 | +0.15 [−0.11,+0.43] | 82/720 | 360/360 | no detectable difference, negative lean |
| `v3 + pp + saverC` (`final_saverC60`) | **−0.011 [−0.028,+0.005]** | 13+/28− p=0.028 | +0.17 [−0.07,+0.44] | 74/720 | 360/360 | identical |

Both variants land on exactly the same cells, so `srcC`'s "the assault waits while our own next HQ
level is still reachable" is not the binding constraint here.  The cell-by-cell breakdown is
unambiguous and identical for the two:

| transition | n | what it is |
|---|---|---|
| **W(+5) → D(0)** | **24** | a turn-limit win by exactly one HQ level, turned into a draw — we finish one level lower |
| D(0) → W(+5/+8/+15) | 9 | the §11-5 static-upper-bound draws the saver was built to convert |
| W → W, new HQ kill | 19 (saver) / 17 (saverC) | the finisher converting turn-limit wins into kills, **+179 / +155 margin** |
| other | 30 | churn |

So on `v3` the finisher's margin lever still works (+0.15 dMargin, 19 extra HQ kills), but it is paid
for by 24 narrow wins that become draws: **−8.0 score points against +111 hp of margin over 720
cells.**  At the project's own GA weight (score + 0.01/hp × margin) that is −0.011 + 0.0015 = −0.0095,
i.e. net negative.

**Why the sign flipped between `v3_base` and `v3`.**  On `v3_base` the HQ ends at level 2.33 and
392/720 games never even reach L2 — the saver's job (bank the next level) is pure profit there.  On
`v3` the f1f2 + center pair already takes the HQ to **3.20 at the end, L2 in 720/720 games**, so the
level the saver would have bought is usually already bought, and what remains is the finisher's
cap-training spending the gold that would have paid for the *last* level.  A patch measured against a
weaker parent does not automatically survive re-basing — this is the concrete case.

Both variants are also identical on T4 (140 recorded r24–30 replays, vs `v3_base`, tags
`final_saver_t4` / `final_saverC_t4`): dScore **+0.024 [−0.012,+0.063]**, dMargin −1.04 [−2.48,+0.40],
0 own errors (the base itself engine-errors on 13 of the 140, hence n = 127).

---

## 5. What actually fixed it: the finisher's cap-training must wait for our own HQ level

`srcC` gates only the **assault** on "is our next HQ level still reachable with the turns that
remain" (`finisher_hq_level_still_reachable`, `MAX_TURN − turn − FINISHER_KILL_SLACK_TURNS`), and
deliberately lets cap-training run — "the bodies are what makes the later kill fast".  The 24 W→D
cells say that is backwards on `v3`: it is **cap-training** that eats the gold for the last level.

**Variant D** (`next/work/final/srcD.cpp`, this session) is `srcC` with one token changed at the
`FINISHER_CAP_TRAIN` branch of `choose_train_count`:

```c
-  } else if (FINISHER_CAP_TRAIN && finisher_dominant(S, M, a, budget, turn)) {
+  } else if (FINISHER_CAP_TRAIN && finisher_attack_allowed(S, M, a, budget, turn)) {
```

**Variant E** (`next/work/final/srcE.cpp`) is the shipped saver with the whole finisher switched off
at the MoE table (`P_FINISHER_CAP_TRAIN / _HQ_TARGET_RELAX / _STREAM = {0,0,0,0}`) — the saver alone.

### The five wave-2 candidates on identical cells (60 seeds, T1 = 720 paired cells, base = `v3`)

| build | dScore [95 % CI] | dMargin [95 % CI] | inform | W→D | D→W | new HQ kills | run tag |
|---|---|---|---|---|---|---|---|
| pp only | −0.001 [−0.011,+0.007] | +0.01 [−0.11,+0.12] | 17 | 3 | 4 | 0 | `final_pp60` |
| pp + saver (as shipped in the patch) | −0.011 [−0.028,+0.005] | +0.15 [−0.11,+0.43] | 82 | **24** | 9 | 19 | `final_saver60` |
| pp + saverC | −0.011 [−0.028,+0.005] | +0.17 [−0.07,+0.44] | 74 | **24** | 9 | 17 | `final_saverC60` |
| pp + saverE (saver only, finisher off) | −0.003 [−0.015,+0.010] | +0.02 [−0.15,+0.18] | 35 | — | — | 0 | `final_saverE60` |
| **pp + saverD (SHIPPED as `v3_1`)** | **−0.001 [−0.016,+0.013]** | **+0.27 [+0.02,+0.55]** | 62 | **12** | 11 | 15 | `final_saverD60` |

Reading the table:

* **the saver on its own is inert on `v3`** (variant E: 35 informative cells, ±0 on both axes) — the
  HQ levels it was built to buy are already bought by f1f2 + center;
* **the finisher is the whole effect**, and it is a margin lever: it turns turn-limit wins into HQ
  kills (21 → 40 HQ-kill wins over 720 cells);
* **untamed, the finisher pays for that margin in narrow wins** (24 × W(+5) → D(0));
* **gating cap-training on HQ reachability keeps the margin and halves the bill**: W→D 24 → 12, and
  the summed cell deltas go from −8.0 score / +111 hp to **−1.0 score / +196 hp**.

Variant D is the only wave-2 build that satisfies the PROTOCOL acceptance rule against `v3`:
neither CI below 0 (rule 1) and the **dMargin CI excludes 0** (rule 2), with T3 360/360 (rule 3),
0 own aborts (rule 4), trickle proxy 0.375 vs 0.324 (rule 5, limit +1.0), T2 within noise (rule 6).
So **`v3_1 = v3 + pingpong retreat fix + saver/finisher with the D gate`**.

The two dead knobs it inherits stay compiled in for the GA: `P_PINGPONG_VETO_TURNS = 0` and
`P_FINISHER_ATTACK_WAIT_FOR_HQ = 1 / P_FINISHER_KILL_SLACK_TURNS = 15`.
The complete `v3 → v3_1` delta is saved as `next/patches/v3_1_from_v3.diff` (546 lines).

---

## 6. Final build and its numbers

**`next/v3_1.cpp` / `next/v3_1.exe`** (11,914 lines; md5 `d33ea375`), compiled from
`next/work/final/srcD.cpp` with the standard recipe.  Re-measured end-to-end from the shipped
artifact (the binary was recompiled from `next/v3_1.cpp`, so every cell below was **replayed**, not
reused from the variant's cache — and it reproduces `final_saverD60` to the last digit, which is also
the determinism check).

### (a) vs `next/v3.exe` — what wave 2 added (tag `final_vs_v3`, 60 seeds)

```
T1 720 | cand 0.852 / +6.29   base 0.853 / +6.02
dScore  -0.001 [-0.016,+0.013]      dMargin +0.27 [+0.02,+0.55]      informative 62/720
T2 107691 | dScore -0.008 [-0.042,+0.017]        T3 360/360 wins
per opponent dScore: 38743 +0.000 | 83616 -0.021 | 97281 +0.033 [+0.008,+0.067] |
                     102712 -0.008 | 104939 -0.017 | 107318 +0.004
```

Score-neutral, margin-positive with the CI clear of 0.

### (b) HEADLINE — vs `next/v3_base.exe` = the submitted 109855 (tag `final_vs_v3base`, 60 seeds)

```
T1 720 cells | cand 0.852 / +6.29   base 0.667 / +4.00
dScore  +0.185 [+0.143,+0.228]   sign 311+/42-  p=0.000
dMargin +2.29  [+1.57,+3.04]     informative 462/720 (59/60 seeds)
T2 (sibling 107691) dScore +0.212 [+0.142,+0.279]
T3 sample 120/120, rusher 120/120, boomer 120/120 = 360/360, 0 handshake failures
VERDICT: STRONGER (both CIs exclude 0)
```

PROTOCOL acceptance, rule by rule:

| rule | requirement | measured | |
|---|---|---|---|
| 1 | neither CI entirely below 0 | dScore [+0.143,+0.228], dMargin [+1.57,+3.04] | **PASS** |
| 2 | at least one CI entirely above 0 | **both** are | **PASS** |
| 3 | T3 100 % wins | 360/360, 0 handshake failures | **PASS** |
| 4 | no own WA/TLE | 0 own aborts in 1,340 candidate cells; 0 cells with ms ≥ 100; tokens left 5 everywhere | **PASS** |
| 5 | sortie guard ≤ +1.0 | trickle deaths **0.375 vs 0.828** (down) | **PASS** |
| 6 | T2 not weaker | +0.212 [+0.142,+0.279] (better) | **PASS** |
| 7 | T4 dScore not significantly negative | +0.024 [−0.012,+0.063] | **PASS** |

By MoE map bucket: `N≤63 +0.277`, `65–79 +0.250`, `81–93 +0.015 [−0.061,+0.114]`,
`95–109 +0.147`; by side `LEFT +0.186 / RIGHT +0.185` (no side artefact).
Behaviour vs the submitted build: HQ level at end **3.19 vs 2.33**, games that never reach L2
**0 vs 392**, unspent gold 926 vs 1572, peak army 19.4 vs 14.0, wins by HQ kill **40 vs 20**,
losses by HQ kill 1 vs 1, deaths at uncaptured enemy bases 0.375 vs 0.828.

### (c) T4 — the 140 recorded r24–30 games (tag `final_t4`, vs `v3_base`)

```
127 paired cells | cand 0.969 / +16.66   base 0.945 / +17.70
dScore  +0.024 [-0.012,+0.063]      dMargin -1.04 [-2.47,+0.42]      informative 50/127
vs the RECORDED outcomes: cand 0.950 (recorded 0.721) dScore +0.229 [+0.161,+0.300],
                          margin +16.08 (recorded +6.68) dMargin +9.40 [+7.39,+11.49]
cand engine errors 0 (the base itself errors on 13 of the 140 -> n = 127)
```

Rule 7 passes.  The −1.04 dMargin is the same replay-harness artifact as in `impl-saver.md` §3.3 and
`impl-pingpong.md` §3: `v3` alone already shows −1.16 on the identical cells (tag `v3_t4`), so **none**
of it comes from wave 2 — it is games the candidate now wins by HQ destruction 15–31 turns *earlier*
(so our own HQ is one level lower at the recorded end) plus the ±30 WA convention firing on the base
side when the scripted opponent runs out of recorded commands.

### (d) vs the round-30 entry `analysis/bin/107691.exe` (tag `final_vs_r30`, 60 seeds)

```
T1 720 | cand 0.852 / +6.29   base 0.637 / +3.69
dScore  +0.215 [+0.179,+0.251]   sign 352+/36-   dMargin +2.61 [+1.91,+3.33]   informative 486/720
T2 107691 | dScore +0.275 [+0.208,+0.338]      T3 360/360
```

### (e) Stratified confirmation (tag `final_strata`, 15 seeds per MoE bucket, 2,400 fresh games)

The unstratified seed set gives 17/15/11/17 seeds per map bucket; `--strata` balances it to 15/15/15/15
with fixed `--NP/--KP`.  Every cell here is new (no cache), so this is a fully independent replication:

```
T1 720 | cand 0.866 / +6.53   base 0.681 / +3.81
dScore  +0.185 [+0.140,+0.230]   sign 305+/40-   dMargin +2.71 [+1.94,+3.50]   informative 472/720 (60/60 seeds)
T2 +0.217 [+0.138,+0.292]    T3 360/360    own aborts 0    tokens left 5    VERDICT STRONGER
by bucket: N<=63 +0.275 | 65-79 +0.233 | 81-93 +0.000 [-0.069,+0.078] | 95-109 +0.231
```

Same +0.185 as the unstratified headline.  The `81–93` bucket is flat in both designs — that is the
map size where `v3_base`'s forced-HQ2 opening already works, and it is the one place where wave 1
bought nothing.

---

## 7. REPORT §11, item by item — what is closed and what is still open

| §11 item | status in `v3_1` | evidence / what is left |
|---|---|---|
| 1. F1/F2 source patches | **shipped** (`f1f2`) | +0.187 dScore on its own; the biggest single win in the whole programme.  Closed. |
| 2. never trickle / never re-attack a reinforced base / recall on a threatening stack | **half shipped** (`trickle`: sortie-strength gate + re-attack memory) | score-neutral on the pool but the behaviour is real: deaths at uncaptured enemy bases 0.375 vs 0.828, deaths at enemy bases (any) 2.70 vs 4.71.  The *recall* half is the posture detector — still open. |
| 3. concentrate: 2 defenders on a kept base or abandon it | **open** | `concentr` measured nothing on any axis; only its build-veto sub-variant did (T4 bases lost −10.6 %).  `v3_1` in fact loses *more* bases than the submitted build (2.42 vs 1.51) because it now expands instead of sitting at L1 — the concentration rule is the natural next patch, and it should be re-derived on `v3`, not on `v3_base`. |
| 4. an L2 floor (bank 600) | **closed as obsolete** | f1f2 fixes the mechanical half; the policy half is provably a regression on this pool (`concentr`).  `v3_1` reaches L2 in **720/720** games (`v3_base`: 328/720). |
| 5. threat-conditioned late saver | **shipped but inert** | variant E shows the saver alone is ±0 on `v3` (35/720 informative).  It stays in as 7 GA genes; its value returns only if a future change makes the late HQ level scarce again. |
| 6. finisher / margin module | **shipped, and it is where wave 2's gain is** | HQ-kill wins 21 → 40 per 720 cells, dMargin +0.27 [+0.02,+0.55].  Still conservative: the gate is open in only ~12 % of dominant-window samples, and the "train at cap from t150 + stream" version of §11-6 (54/79 convertible) is *not* what ships — cap-training now waits for our own HQ level, which is exactly the tension §11-6 flagged. |
| 7. posture switch | **open** | detector built and validated (converts `29_238870` and `30_251179`); the response is score-negative (−0.013).  Highest-value open item. |
| 8. rotation loop | **half shipped** | the impossible-retreat half is in (42/42 of `v3_base`'s retreat orders were unexecutable; pinned-retreat deaths −85 %).  The ping-pong veto is compiled in at 0 — it needs to become a *preference* inside the source choosers rather than a hard refusal at the move choke point. |
| 9. spend the compute | **untouched — the biggest open lever** | serial cost is ~1–2 ms of the 100 ms budget (§8).  No look-ahead of any kind is in the build. |
| 10. which build to submit | **answered** by this file and the gauntlet | §9. |

---

## 8. Timing (serial, idle machine — the only readings that mean anything)

`next/work/merge/timecheck.py` with `v3_1_final` added to its `BUILDS` dict; one full 200-turn game
per (build, seed) for seeds 31/32/34 vs `analysis/bin/97281.exe`, **no parallel load**:

| build | mean ms | p95 | max ms | overtime tokens left | turns |
|---|---|---|---|---|---|
| `v3_base` (submitted 109855) | **1.1** | 16 | 16 | 5 | 200 |
| + f1f2 | 1.1 | 15–16 | 16 | 5 | 200 |
| + center | 0.9 | 16 | 16 | 5 | 200 |
| + trickle | 1.0 | 15–16 | 16 | 5 | 200 |
| `v3` (all of wave 1) | 1.2 | 15–16 | 16 | 5 | 200 |
| **`v3_1` (final)** | **1.1** | 15 | **16** | **5** | 200 |

The final build costs the same as the build we submitted: ~1.1 ms of the 100 ms budget, a 16 ms
maximum (one Windows scheduler tick — the clock's granularity, not compute), and all 5 overtime
tokens still unspent after 200 turns.

**Do not report parallel-run ms/token numbers as real.**  Under 8 workers the same builds read
2–3 ms mean and 30–60 ms max, and under heavy multi-agent load the readings became absurd
(`244578 ms`, tokens 0) — see §3.1.  Every ms or token warning must be re-checked serially before it
is believed; every such warning in this programme has dissolved on a serial re-run.

Cleanliness of the published numbers: across all 1,340 `v3_1` cells (T1+T2+T3+T4) there are
**0 own aborts, 0 cells with `us_ms_max >= 100`, and 0 cells with fewer than 5 tokens left** — nothing
needed purging on the candidate side; the only purge in this session was `v3`'s poisoned cache (§3.1).

---

## 9. Submission-grade run: 200 seeds (tag `final_200seed`)

PROTOCOL says settle a *submission* choice at 200 seeds (MDE ≈ 0.035 dScore for this pair), so the
headline was repeated on T1 with `--seed-list 1-200` — 2,400 paired cells, 3,360 fresh games:

```
T1 2400 cells | cand 0.841 / +6.35   base 0.683 / +3.91
dScore  +0.158 [+0.133,+0.182]   sign 951+/176-  p=0.000
dMargin +2.43  [+1.99,+2.89]     informative 1492/2400 (199/200 seeds)
own aborts 0 | 0     tokens left min 5 | 5     VERDICT STRONGER (both CIs exclude 0)
by bucket: N<=63 +0.211 | 65-79 +0.237 | 81-93 -0.016 [-0.060,+0.028] | 95-109 +0.178
```

The effect is **4.5× the 200-seed MDE**, so this is not a borderline call.

### The one honest regression: opponent `83616`

| design | 83616 dScore | 83616 dMargin |
|---|---|---|
| 60 seeds (`final_vs_v3base`) | −0.067 [−0.192,+0.058] | −3.08 [−5.46,−0.62] |
| 60 seeds stratified (`final_strata`) | −0.025 [−0.129,+0.083] | +0.50 [−1.75,+2.83] |
| **200 seeds (`final_200seed`)** | **−0.068 [−0.128,−0.009]** | **−1.43 [−2.66,−0.21]** |

At 200 seeds both CIs exclude 0: against this one opponent `v3_1` really is weaker.  It is a
**wave-1** effect, not wave 2 — `v3` alone is −0.046 / −2.92 against 83616 (`v3_clean3`) while wave 2
adds only −0.021 / −0.17 (`final_vs_v3`).  The mechanism is the same one that wins everywhere else:
`v3_base` sits at HQ L1 and turtles, `v3` releases the economy and expands (bases lost 2.38 vs 1.60),
and 83616 — an r22-era build that punishes loose bases — is the archetype that can exploit that.
This is exactly REPORT §11-3 (concentrate: two defenders on a kept base or abandon it), which is the
one §11 item that is still fully open.  It costs −0.068 on 1/6 of the pool against +0.16 to +0.32 on
the other five.

---

## 10. What to submit

**Submit `next/v3_1.cpp` (binary `next/v3_1.exe`, md5 `d33ea375`).**

The case, in order of strength:

1. **It beats the build we actually submitted (109855 = `v3_base`) by a wide, replicated margin**:
   +0.158 dScore [+0.133,+0.182] / +2.43 hp [+1.99,+2.89] over 2,400 paired cells at 200 seeds, and
   +0.185 / +2.29 at 60 seeds, and +0.185 / +2.71 on the independent stratified design.  All seven
   PROTOCOL rules pass, in every design.
2. **It beats the round-30 entry `107691` by +0.215 / +2.61** and beats the *sibling* head-to-head
   (T2) by +0.212 — the tile that is supposed to be all draws.
3. **The mechanism is structural, not pool-specific**: HQ level at end 3.17 vs 2.35, and
   `games that never reach L2` **0 vs 1,248 of 2,400**.  A bot that spent whole games at HQ L1 now
   never does.  It shows up on every measurement surface we have — the T1 pool, the provided bots
   (T3 kill turns faster: sample 164 vs 165, rusher 174 vs 176), and the 140 recorded real games
   (T4: +0.229 dScore vs the *recorded* outcomes, +9.40 hp margin).
4. **It costs nothing in compute**: 1.1 ms/turn serially, identical to the submitted build, all 5
   overtime tokens unused (§8).
5. **The margin gain matters for the judge** (ELO rose 1450→1700 at an unchanged 6/1/7 W/D/L), and
   margin is where wave 2 contributes: +0.27 hp [+0.02,+0.55] and HQ-kill wins 21 → 40 per 720 cells.

Residual risks, stated plainly:

* **`83616` (−0.068 at 200 seeds)** — the one archetype that punishes the wider base footprint.  If
  an opponent in the bracket plays that way, this build is worse than the submitted one *against
  that opponent*.  The fix is REPORT §11-3, and it should be re-derived on `v3`, not on `v3_base`.
* **Map bucket 81–93 is flat** (−0.016 [−0.060,+0.028]) in all three designs.
* **Pool over-fit** is the standing caveat of the whole harness (PROTOCOL "Caveats").  T4 is the
  hedge and it is positive (+0.024 [−0.012,+0.063]), but it is only 127 usable replays and the
  scripted opponent stops reacting after ~turn 31.

**Fallback ladder** if something has to be rolled back: `next/v3.cpp` (wave 1 only) is score-identical
to `v3_1` (−0.001) and loses only the margin gain; `next/v3_base.cpp` is the currently submitted build.

**Next levers, ranked** (all measured, none shipped): REPORT §11-3 concentration re-derived on `v3`
(would also repair 83616); §11-7 posture *response* (the detector already converts `29_238870` and
`30_251179`); §11-9 look-ahead — we use 1 ms of the 100 ms budget; and a GA pass over the 19 new genes
this merge added (`P_PINGPONG_VETO_TURNS`, 7 × `P_LATE_SAVER_*`, 11 × `P_FINISHER_*`), which have
never been tuned — their values are the hand-picked ones from the two implementation studies.

---

## Verification (independent)

*Second pass, run from scratch on 2026-08-26 by an agent that did not build `v3_1`, against the
shipped artifacts only (`next/v3_1.cpp`, `next/v3_1.exe`, `next/v3_base.cpp/.exe`, `next/patches/*.diff`).
Run tags created here: `verify_final`, `verify_fresh6170`, `verify_fresh6170_t3`, `verify_t3_unseen`.*

**Verdict: CONFIRMED.**  Every headline number reproduces exactly, the source contains exactly the
patches this file claims and nothing else, all six included patches demonstrably fire in the merged
binary, and the compute cost is unchanged.  One real issue was found — it is **not** a `v3_1`
regression (`v3_base` has it identically) and it does not change the submission decision.

### 1. Source — the delta is exactly the claimed patches

`diff next/v3_base.cpp next/v3_1.cpp` = 51 hunks, **936 added / 19 removed** lines.

* **Attribution.** All 19 removed lines and 676 of the 709 distinct added lines appear verbatim in
  `next/patches/{f1f2,center,trickle,evac,pingpong,saver,saverD_capgate}.diff`.  The 33 remaining
  added lines are **exactly** the two documented modifications and nothing else:
  * `srcC`'s HQ-reachability wait — `P_FINISHER_ATTACK_WAIT_FOR_HQ`, `P_FINISHER_KILL_SLACK_TURNS`,
    `finisher_hq_level_still_reachable()`, `finisher_attack_allowed()` and the substitution of
    `finisher_dominant` to `finisher_attack_allowed` at all three finisher call sites (section 5);
  * the `center` **e86** MoE value `{86,86,86,86}` in place of the patch's `{24,24,86,24}` (section 1).
* **`srcC` to shipped is one line.** `diff next/work/final/srcC.cpp next/v3_1.cpp` is a single
  changed token at the `FINISHER_CAP_TRAIN` branch of `choose_train_count` and is **byte-identical to
  `next/patches/saverD_capgate.diff`**.
* **`v3` to shipped.** The recomputed `diff next/v3.cpp next/v3_1.cpp` matches the stored
  `next/patches/v3_1_from_v3.diff` line for line (546 lines).
* **Hunk placement spot-checked by hand**, not just by text: F1 sits above the staged-attack early
  return inside `issue_hard_advantage_conversion`; F2 sits after the army-deficit block and
  immediately before `issue_forced_hq2_save_or_upgrade` (the section 1 table said "after" — corrected
  above); the shared `add_move_action_ex_stack_flags` carries the ping-pong veto right after the
  contested-source check, then `stack_guard_would_feed`, **then** center's standby-keep guard, and
  `pingpong_note_order` after the `VEC_PUSH` — **both guards preserved**, as claimed; the retreat fix
  refuses the order when `region_has_enemy_warrior(S,M,r)`; the three finisher call sites match
  `saver.diff` including its deliberate `S->gold` (not `budget`) at the `HQ_TARGET_RELAX` site.
* **No stray edits.**  MoE table 104 to **131 rows, 131 distinct genes, 0 duplicates, 0 removals** —
  nothing renumbered, no double-applied hunk.  The set of duplicate function names (forward
  declarations) and duplicate `#define`s is *identical* in both builds.  `g++ -O2 -Wall -Wextra`
  emits the **same single pre-existing warning** (`picked_eta` set but not used) for `v3_base` and
  `v3_1` — the merge introduces no new warning.
* **Switch states as claimed:** `EVACFIX_F1 1`, `EVACFIX_F2 0`,
  `P_PINGPONG_VETO_TURNS = {0,0,0,0}`.  `DEBUG_PATCH` is **never defined** anywhere in the file and
  all 20 `fprintf(stderr, ...)` calls sit inside `#ifdef DEBUG_PATCH` (checked by a preprocessor-nesting
  walk, not by grep) — the shipped binary prints nothing.
* **Binary matches source.** `g++` output here is not byte-deterministic (two back-to-back builds of
  the same source give different md5s; size is identical at 227,078 bytes), so md5 cannot prove it.
  Instead a fresh rebuild of `next/v3_1.cpp` was played against `97281.exe` on seeds 7 / 31 / 62: the
  engine logs are **identical to `next/v3_1.exe`'s, line for line**, apart from the recorded
  `COMMAND:` path.  `next/v3_1.exe` is the shipped source.

### 2. Headline gauntlet re-run — reproduces to the digit

`--cand next/v3_1.exe --base next/v3_base.exe --tag verify_final --tiles T1,T2,T3 --workers 8`:

```
T1 720 | cand 0.852/+6.29  base 0.667/+4.00
dScore  +0.185 [+0.143,+0.228]  sign 311+/42-  p=7.2e-52     dMargin +2.29 [+1.57,+3.04]
informative 462/720 (59/60 seeds)     T2 +0.212 [+0.142,+0.279]     T3 360/360, 0 handshake failures
guards: fail=[] warn=[]                VERDICT: STRONGER (both CIs exclude 0)
```

Identical to `final_vs_v3base` on every field.  All 2,400 cells came from cache (the games are
deterministic), so this is a re-analysis, not a replay — section 3 supplies the replay.

**Poisoned cells: none.**  Every WA line in the run is `abort_side=opp` on `boomer` (the opponent's
own WA).  A scan of **all 4,280 cached `v3_1` cells** (T1+T2+T3+T4 across every run tag) gives
**0 own aborts, 0 cells with `us_ms_max >= 100` (max seen 63 under 8-way load), 0 cells with fewer
than 5 tokens left, 0 non-OK statuses** — the section 8 cleanliness claim holds, and on a larger cell
set than the 1,340 it was stated for.  The base's 13 `ENGINE_ERROR` cells are all T4 replays,
matching the `n = 127` in section 6(c).

**Every other number in this file was checked against its `runs/<tag>/summary.json`** and matches:
`final_200seed` (+0.158 [+0.133,+0.182] / +2.43 [+1.99,+2.89], 1492/2400 informative),
`final_strata` (+0.185 [+0.140,+0.230] / +2.71 [+1.94,+3.50]),
`final_vs_v3` (-0.001 [-0.016,+0.013] / +0.27 [+0.02,+0.55], 62 informative),
`final_vs_r30` (+0.215 / +2.61), `v3_clean3` (+0.187 / +2.02), `final_t4` (+0.024 [-0.012,+0.063],
n=127).  `final_saverD60` is field-for-field identical to `final_vs_v3`, which is the determinism
check section 6 claims.

### 3. Fresh games — 480 headline cells genuinely replayed, all exact

Seeds 61-70 turned out to be **already cached** from `final_200seed`, so their 480 cache entries and
480 engine logs were moved aside and the cells were **replayed for real** (`verify_fresh6170`,
`games played 240, cached 0`):

```
T1 120 cells | cand 0.825/+5.67  base 0.742/+5.39
dScore +0.083 [+0.008,+0.167]   dMargin +0.28 [-1.13,+1.72]   informative 52/120 (10/10 seeds)
own WA/TLE aborts 0 | 0     engine errors 0 | 0     ms max 16 | 16     tokens left 5 | 5
```

All **240 replayed cells reproduce the purged cache exactly** on outcome, margin, reason, end turn,
HQ level, deaths, gold, army and bases — an independent confirmation both of determinism and of the
200-seed run's cells.  The point estimate is lower than the 60-seed +0.185 because 83616 alone is
-0.200 on these ten maps; that is the known section 9 regression, not a new effect (10 seeds is well
below this pair's MDE).

The same purge-and-replay was then done **inside the acceptance seed set** — seeds 1-10, all six T1
opponents, both sides, both builds (`verify_replay_1_10`, `games played 240, cached 0`):
`dScore +0.237 [+0.129,+0.333]`, `dMargin +2.41 [+0.60,+3.96]`, 79/120 informative, 0 own aborts,
tokens 5.  **All 240 of those cells also reproduce the original cache exactly**, and re-running the
full headline afterwards returns byte-identically the same table (`+0.185 [+0.143,+0.228]` /
`+2.29 [+1.57,+3.04]`, 462/720 informative).  So 480 of the 720 headline pairs have now been played
from scratch by a second party with zero disagreement — the cached headline is sound.

### 4. Timing — serial, idle machine

`next/work/merge/timecheck.py`, seeds 31/32/34 vs `97281.exe`, no parallel load:

| build | mean ms | max ms | tokens left | turns |
|---|---|---|---|---|
| `v3_base` | 0.8 | 16 | 5 | 200 |
| `v3_1` | **1.2** | **16** | **5** | 200 |

Same class as the base: about 1 ms of the 100 ms budget, a 16 ms maximum (the Windows scheduler
tick), all 5 overtime tokens unused after 200 turns.  (Section 8 recorded 1.1 vs 1.1; the
sub-millisecond difference is run-to-run jitter at the clock's granularity.)

### 5. Every included patch fires in the merged binary

A `-DDEBUG_PATCH` build of `next/v3_1.cpp` was played for **60 games** (7 opponents x 12 seeds,
logs in `next/work/final/dbg*/`) and the traces counted:

| patch | trace | hits | games |
|---|---|---|---|
| f1f2 | `F1 hoist fired` / `F2 fired` | 53 / 18 | 29 / 18 of 60 |
| center (e86) | `center-econ-release` / `center-wave` | 775 / 552 | 42 / 60 of 60 |
| trickle | `mem-record` / `mem-block` / `resp-sim` | 11749 / 716 / 10876 | 59 / 13 / 54 |
| evac | `evacfix F1 refuse` | 84 | 12 of 60 |
| pingpong (retreat fix) | `retreat-pinned-skip` / `retreat-order` | **90 / 4** | 21 / 3 of 60 |
| saver | `SAVER hold` / `release` / `gather-keep` | 53 / 107 / 4 | 14 / 10 / 1 |
| finisher | `FIN captrain` / `hqtarget` / `stream` | 2 / 189 / 1 | 2 / 9 / 1 |

The retreat fix's 90 : 4 ratio is the claim of section 2 measured live in the merged build:
**96 % of the retreats this bot would issue are of the impossible pinned kind and are now refused.**
Exactly three traces never fire, and they are exactly the three things that are supposed to be off:
`evacfix F2 pursuit` (`EVACFIX_F2 0`), `pp-veto` (`P_PINGPONG_VETO_TURNS = 0` — confirming the veto
is compiled in but inert) and `center-giveup` (the secondary `MAX_FAILED_WAVES = 25` bound; the
primary turn-86 exit fires 775 times).

### 6. Finding: the T3 "100 % wins" guard is seed-set-specific — and it is a `v3_base` problem too

PROTOCOL rule 3 and section 6(b) are stated on seeds 1-60, where T3 really is 360/360.  On
**40 unseen seeds (61-100, 480 fresh T3 games, tags `verify_fresh6170_t3` + `verify_t3_unseen`)**:

| opponent | `v3_1` | `v3_base` (submitted) |
|---|---|---|
| sample | 80/80 | 80/80 |
| boomer | 80/80 | 80/80 |
| **rusher** | **76/80** | **76/80** |

Both builds lose the **same four cells**: seed 62 (N=69) and seed 90 (N=97), *both sides each*,
`HQ_DESTROYED` at **turn 58**, margin -10, with `abort_side` empty — genuine losses, not WA/TLE and
not a harness artifact (seed 62 was re-played serially outside the harness and reproduces:
`RESULT RIGHT_WIN HQ_DESTROYED` at turn 58).

So **this is not a `v3_1` regression** — the currently-submitted build dies on the identical maps, at
the identical turn, by the identical margin, and the merge changes nothing about it.  What it does
mean is that "T3 100 %" should be read as "100 % on the acceptance seed set", not as "we never lose
to a rusher": on unseen maps this lineage loses about 5 % of games to the provided rusher, and the
two losing maps share a signature (HQ dead at turn 58 on both sides, two different map sizes) that
looks like one missing early-defence response rather than map-specific bad luck.  Since section 7
already ranks the posture *response* as the highest-value open item, **"survive the provided rusher
on seeds 62 and 90" is a concrete, cheap, fully reproducible target for the next patch** — and a
sharper regression test than the seed-1-60 T3 tile, which cannot see the failure at all.

### 7. Corrections made to this file

* Section 1, `f1f2` row: F2 is placed **before**, not after, the forced-HQ2 block; and the row now
  records that the patch also widens `P_FORCE_HQ2_SAVE_TURN` from `{0,0,85,0}` to `{85,85,85,85}`
  (the forced-HQ2 save moves from the `81-93` bucket only to all four buckets).  This is a real part
  of the measured `f1f2` effect and was previously undocumented.

Nothing else needed changing.  **Ship `next/v3_1.cpp` / `next/v3_1.exe` (md5 `d33ea375`).**

---

# Wave 3 (2026-08-26/27): the real-opponent tile and the 83616 repair -> `v3_2`

Wave 3 answered the standing caveat of everything above — *the pool is made of our own builds* — and used
what it found to close the one honest regression §6 recorded.

## 11. T5: scripted reproductions of the styles that beat us

`analysis/eval/opponents/` adds four reactive Python opponents (`hunter` = team 474, `reinforcer` = 313/455,
`latemass` = 1170, plus a non-clone `scaler`), wired into the harness as tile **T5**.  Full write-up and
fidelity table: `analysis/findings/next/opponent-pool-t5.md`.

**Result: a guard, not a discriminator.**  `v3_base` and `v3_1` both beat the clones 72/72 (`t5_probe`);
only margin varies.  The cause is measured, not guessed: the clones end on 4–6 bases at HQ L2/L3 while our
build ends on 11–15 bases at L5.  The playbooks are faithful to what beat our *mid-2026* builds — the ones
frozen at HQ L1 in the center wave-loop — and `f1f2`+`center` removed exactly that weakness.  Use T5 like
T3: a candidate must keep 100 %, and it is neither a score axis nor GA fitness.

## 12. The 83616 regression: cause found, fixed, and it was worth more than it cost

§6 recorded `83616 −0.068` as the one honest regression.  Attribution, from cached cells (all games there
end at the turn limit, so the result is a pure HQ-level race):

| build | 83616 score | margin | our HQ level at end |
|---|---|---|---|
| `v3_base` | 0.829 | +8.83 | 3.98 |
| `v3` (wave 1) | 0.783 | +5.92 | 3.58 |
| `v3_1` (wave 2) | 0.762 | +5.75 | 3.57 |

Per-patch isolation settles it: `f1f2` and `center` are **byte-identical** against 83616 (0 informative
cells), `evac` costs −0.004, and **`trickle` costs −0.033 on its own** (margin 8.83 → 6.29, HQ level
3.98 → 3.62).  The no-trickle gate was vetoing attacks that were *profitable* against a base-heavy
opponent: we captured less, earned less, and lost the HQ-level race that decides every turn-limit game.

The gate is made of MoE genes, so the fix is a tuning question, not a rewrite.  Four relaxations were
screened at 30 seeds and two run in full; the winner is **`P_OFFENSE_REINF_ARRIVAL_SLACK {2,2,2,2} → {0,0,0,0}`**
(stop crediting the defender two extra turns of travel time when counting reinforcements):

| variant | change | T1 dScore vs `v3_base` | 83616 | trickle proxy |
|---|---|---|---|---|
| `rA` | MIN_STACK 2, SLACK 1 | +0.207 | +0.000 (margin −0.67) | 1.165 (worse than base) |
| **`rC`** | **SLACK 0** | **+0.214 [+0.176,+0.251]** | **+0.017 (margin −0.12)** | **0.629 (better than base)** |
| `rB` | MIN_STACK 3, SLACK 0 | +0.229 @30 seeds | +0.033 | — |
| `rD` | + re-attack memory off | +0.224 @30 seeds | +0.000 | — |

`rC` is shipped as **`next/v3_2.cpp` / `.exe`** (`next/patches/v3_2_from_v3_1.diff` — one line).

### `v3_2` numbers

* vs `v3_base` (the submitted 109855), 60 seeds, tag `v3_2_full`: **dScore +0.214 [+0.176, +0.251],
  dMargin +2.92 [+2.21, +3.70]**, T2 +0.250, T3 360/360, 0 aborts.  (`v3_1` was +0.185 / +2.29.)
* vs `v3_1` head-to-head, 60 seeds, tag `v3_2_vs_v3_1`: **dScore +0.028 [+0.005, +0.054],
  dMargin +0.62 [+0.10, +1.17]** — STRONGER, both CIs exclude 0; 83616 +0.083 (margin +2.96).
* T4, the 140 recorded games against the **real** opponents (tag `rC_t4`): +0.028 [+0.004, +0.059] vs
  `v3_base`, and **0.957 against the recorded 0.721** — the best T4 score of any build so far
  (`v3_1`: 0.950).  0 engine errors.
* T5 guard (tag `rC_t5`): 72/72, 0 aborts.
* The trickle proxy sits between the two: 0.629 vs `v3_base` 0.828 (still improved) and `v3_1` 0.375
  (some of the reduction traded back for aggression).  T4 confirms the trade is favourable against real
  opponents.

**Submit `next/v3_2.cpp`.**  Fallback ladder: `v3_2` → `v3_1` → `v3` → `v3_base`.
