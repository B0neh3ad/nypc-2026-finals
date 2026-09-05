
# Opponent archetypes and the opponents that beat us (NEXT NATION, team 141, Pretest rounds 3-30)

Data: 560 parsed games (`analysis/parsed/games.csv`, `turns.csv`, per-game JSON). Score convention W=1/D=0.5/L=0; overall 349W/50D/161L = 0.668.
Scripts: `analysis/scripts/opponents_features.py` (feature vectors + archetypes + k-means), `opponents_report.py` (tables, profiles, behaviour flags),
`opponents_deepdive.py` (per-game mechanics from JSON series + `cmds_opp`), `opponents_extra.py` (ex-ante flags, squad-family fingerprint, deadlock, evolution), `opponents_write_report.py` (this file).
Intermediate tables: `analysis/parsed/opponents/*.csv|json|txt` (notably `features.csv` = one row per game with ~250 opponent/us features, `deepdive.txt` = readable per-game narratives for the 20 most dangerous opponents, `playbook.csv`).

**How to read "opp_*" timings**: `first_forward_t` = first turn an opponent warrior stood in OUR half (BFS-hops closer to our HQ than to theirs); `first_at_our_HQ_t` = first turn an opponent warrior stood in our HQ region; `first_5fwd_t` / `first_4stack_t` = first turn with >=5 / >=4 opponent warriors in our half; `mean_n_fwd` = average number of opponent warriors in our half over the game. All gold/income figures are replay estimates (never went negative; believed accurate).



## 0. Executive summary

1. **One archetype causes two thirds of our losses.** Rule-based archetypes (section 1) put 176/560 games (31%) in `aggro_macro` = opponents that build 6-10 bases, keep ~70% of warriors at their own buildings, train continuously (peak army median 28 vs our 17), and from turn ~60 keep a standing force of 3+ warriors in our half. Our score vs them is **0.341** (106 losses = 65.8% of all 161 losses; 71 of those by HQ destruction). Against every other archetype we score 0.56-0.92. The k-means cross-check finds the same cluster (cluster 5: n=102, score 0.25, peak army 34, income 25.9k, mean 4.5 warriors forward).
2. **How they beat us (deep dives, section 4):** not by an early rush but by *base sniping then siege*: a 4-5 warrior stack enters our half around t55-80, kills the single warrior guarding each of our L1 bases and then the base (we lose a median of 7 bases per game vs these opponents, 60.4% of aggro_macro losses had >=3 of our bases destroyed by t=100), our income and army stall (median income ratio opp/us in those losses 1.68, peak-army ratio 1.91), and the HQ is sieged only at the end (in the 71 HQ-destroyed losses the median first siege on our HQ is t=164 and the HQ dies at t=169) - our HQ never got past L2 in 84% of the 106 losses (L1 in 33%). When we win vs aggro_macro it is because we out-eco them (median income ratio 0.64).
3. **A cheap exploit family beats us with 10 warriors.** 28 games (19 different opponents, e.g. 395, 994, 184, 129, 1182) follow one fingerprint: TRAIN 1 every turn for the first 5 turns (6 warriors at t=20), at most 2 bases, never upgrade HQ, send a 4-5 stack into our half at t~30 and simply hunt our lone base guards. Median opponent peak army 10, income 4.9k - and our score vs them is **0.143** (3W/2D/23L, 20 HQ kills, median end turn 146); it did not improve with later bot versions (r3-9 0.0, r10-21 0.167, r22-30 0.188) and is side-independent (LEFT 0.115 n=13, RIGHT 0.167 n=15).
4. **Economic deadlock** (0 warriors and <120 gold for >=5 consecutive turns, i.e. no income and cannot train) occurred in 27 games - all 27 are losses (16.8% of our losses; 11 of them vs the squad family, 3 vs naive HQ rushers). The mirror state happened to opponents 31 times - all our wins. Worth a hard guard in the bot (never spend below 120+upkeep when army <= 2).
5. **Naive sample-bot HQ rush** (all starting warriors walk to our HQ, arrive t~12) = 39 games; we lost 10 of them (6 in round 7 alone, plus r10, r12, r21, r23) whenever the bot left 0-1 warriors home in the first 15 turns. Score improved 0.57 (r3-9) -> 0.80 -> 0.90, but 21_174014 (opp 1205, dead at t19) and 23_183808 (opp 712, deadlock, dead at t58) show the hole was not fully closed.
6. **Timing of the first 4-stack in our half is the strongest ex-ante predictor** of the result: <=t45 score 0.49 (n=119, 58 losses), t46-60 0.53, t61-100 0.62, >t150 0.77, never 0.955 (n=134, 1 loss). As RIGHT it is 0.22 / 0.30 / 0.32-0.41 / 0.69 / 0.87.
7. **Opponents we never beat (>=3 games):** 474 (0/8), 313 (0/7), 455 (0/5), 1170 (0/5), 522 (0/3), 1182 (0/3). 474 beat us 8/8 (r4-r30, always with us as RIGHT) with a modest army (peak 22) by sniping bases at t55-80 and sieging by t~100; 313 beat us 7/7 on both sides; 455 and 1170 5/5. The latest bot (r24-30, 140 games, 34 losses of which 18 vs aggro_macro) still lost to 474 (x3), 313 (x3), 1237 (x3), 455 (x2), 997 (x2), 1170 and 25 (x2).
8. **Population evolution:** aggro_macro share fell from 34-35% (r3-21) to 24% (r22-30) while plain `macro` rose 24% -> 47%; the share of opponents reaching HQ L3 by t150 fell 43% -> 27% -> 15%, and heavier compute (>=2 ms/turn) rose 4% -> 12%. Holding the r10-21 archetype mix fixed, our r22-30 score would have been 0.659 instead of 0.714: most of the late improvement is the easier/less aggressive population (and fewer RIGHT-side games), not better results within an archetype (aggro_macro: 0.33 -> 0.32 -> 0.38).



## 1. Opponent archetypes

Per-game opponent feature vector (all from `games.csv` + `turns.csv`): peak army and its turn, total trained, HQ L2/L3/L4/L5 turns, peak bases and max L3 bases, first base turn,
first-forward / first-at-our-HQ / first-5-forward turns, mean and peak number of warriors in our half, share of warriors forward, spend split build/train/move, total income, move-command count,
idle-turn fraction (turns with no command), avg/max ms, siege dealt to our HQ, their HQ level at first siege, and relative features vs us (income ratio, army ratio, HQ-level lead at t50/100/150).

**Rule-based archetypes (priority order; timing rules first because they are robust to game length):**
- `hq_rush` - an opponent warrior reaches our HQ region by t<=30, or >=2 starting warriors are ordered to our HQ on turn 1 (sample-bot opening). 39 games.
- `passive` - (<=8 trained per 100 turns and <=1 base) or >=85% idle turns or <=8 move commands per 100 turns. 41 games.
- `early_aggro` - first warrior in our half by t<=25 (but not an HQ rush). 26 games.
- `turtle` - first warrior in our half at t>=120 or never, while active. 67 games.
- `aggro_macro` - forward entry t26-119 and sustained presence: mean warriors-in-our-half >=3, or >=30% of their warriors forward on average, or a peak of >=10 forward. 176 games.
- `macro` - everything else (forward entry t26-119, low sustained presence). 210 games.
- `broken` - 0 turns (opponent TLE at t1). 1 game.
Compute is an overlay: `light` <2 ms avg (508), `medium` 2-20 ms (43), `heavy` >=20 ms (9).


### 1.1 Our score per archetype

| arch | n | score | W | D | L | L_hq | median_margin | median_nturns | L_tl | share_% |
|---|---|---|---|---|---|---|---|---|---|---|
| hq_rush | 39 | 0.744 | 29 | 0 | 10 | 10 | 15 | 182 | 0 | 7 |
| early_aggro | 26 | 0.558 | 13 | 3 | 10 | 7 | 2.500 | 200 | 3 | 4.600 |
| aggro_macro | 176 | 0.341 | 50 | 20 | 106 | 71 | -10 | 200 | 35 | 31.400 |
| macro | 210 | 0.819 | 163 | 18 | 29 | 21 | 10 | 200 | 8 | 37.500 |
| turtle | 67 | 0.896 | 57 | 6 | 4 | 1 | 10 | 200 | 3 | 12 |
| passive | 41 | 0.915 | 36 | 3 | 2 | 0 | 20 | 197 | 2 | 7.300 |
| broken | 1 | 1 | 1 | 0 | 0 | 0 | 0 | 0 | 0 | 0.200 |

L_hq / L_tl = losses by HQ destruction / at the turn limit. median_margin = our final HQ hp minus theirs.

### 1.2 Archetype x side (LEFT = we are team A at region 0; RIGHT games are against stronger opposition on every proxy)

| arch | mean_LEFT | mean_RIGHT | count_LEFT | count_RIGHT |
|---|---|---|---|---|
| hq_rush | 0.737 | 1 | 38 | 1 |
| early_aggro | 0.733 | 0.318 | 15 | 11 |
| aggro_macro | 0.476 | 0.146 | 104 | 72 |
| macro | 0.877 | 0.625 | 162 | 48 |
| turtle | 0.913 | 0.833 | 52 | 15 |
| passive | 0.914 | 0.917 | 35 | 6 |
| broken | 1 |  | 1 |  |

### 1.3 What each archetype looks like (medians per game)

| index | hq_rush | early_aggro | aggro_macro | macro | turtle | passive | broken | ALL |
|---|---|---|---|---|---|---|---|---|
| peak_army | 6 | 19 | 28 | 17 | 20 | 8 | 0 | 20 |
| peak_army_t | 3 | 120 | 157 | 90 | 112 | 70 |  | 108 |
| trained | 6 | 39 | 58 | 41 | 43 | 12 | 0 | 43 |
| peak_bases | 0 | 6 | 8 | 6 | 6 | 2 | 0 | 6 |
| max_L3_bases | 0 | 0 | 0 | 0 | 0 | 0 |  | 0 |
| HQ_L2_t | 111 | 88.50 | 68 | 67 | 58 | 44 |  | 65 |
| HQ_L3_t | 147 | 142 | 137.50 | 143 | 109 | 76 |  | 133 |
| HQ_L4_t | 160 | 172 | 170 | 181 | 173 | 139 |  | 173 |
| first_base_t | 41 | 2 | 2 | 2 | 2 | 2 |  | 2 |
| first_fwd_t | 8 | 17.50 | 60.50 | 54 | 172 | 37 |  | 54 |
| first_at_our_HQ_t | 13 | 137 | 159 | 96 | 186.50 | 118.50 |  | 126 |
| first_5fwd_t | 9 | 69 | 77 | 70 | 178 | 31 |  | 72 |
| mean_n_fwd | 0.29 | 1.68 | 3.08 | 0.41 | 0 | 0 |  | 0.75 |
| fwd_share | 0.20 | 0.19 | 0.21 | 0.04 | 0 | 0 |  | 0.09 |
| peak_fwd | 5 | 6.50 | 15 | 5 | 0 | 0 |  | 6 |
| spend_build% | 0 | 0.42 | 0.40 | 0.33 | 0.41 | 0.32 |  | 0.36 |
| spend_train% | 0.87 | 0.56 | 0.52 | 0.60 | 0.55 | 0.60 |  | 0.58 |
| spend_move% | 0.10 | 0.04 | 0.06 | 0.05 | 0.02 | 0.03 |  | 0.05 |
| income | 2010 | 14775 | 19627.50 | 12517.50 | 13575 | 4290 | 0 | 13642.50 |
| move_cmds | 9 | 93.50 | 179 | 105 | 97 | 10 |  | 111 |
| idle_turn_frac | 0.82 | 0.60 | 0.40 | 0.52 | 0.59 | 0.88 |  | 0.52 |
| avg_ms | 0 | 0.01 | 0.05 | 0.04 | 0.01 | 0.01 | 0 | 0.01 |
| siege_on_our_HQ | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| their_HQ_lvl_at_first_siege | 1 | 1 | 2 | 1 | 1 | 1 |  | 2 |
| income_ratio_opp/us | 0.19 | 0.99 | 1.37 | 0.59 | 0.49 | 0.22 |  | 0.68 |
| peak_army_ratio_opp/us | 0.50 | 1.28 | 1.62 | 0.77 | 0.69 | 0.29 |  | 0.88 |

131 opponents were met >=2 times; 42 of them kept the same archetype in every game, 89 switched at least once (bot updates between rounds, or the archetype depends on the matchup/map). Archetype is therefore a *per-game behaviour*, not a fixed team label; profiles below list each opponent's archetype string across games.

### 1.4 Damage they do to us (medians per game) and deadlock games

| arch | us_bases_lost | opp_bases_lost | us_total_died | opp_total_died | us_bases_lost_by100 | us_hunger_dmg | us_deadlock_run | deadlock_games |
|---|---|---|---|---|---|---|---|---|
| aggro_macro | 7 | 2 | 40 | 39.50 | 3 | 0 | 0 | 8 |
| broken | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| early_aggro | 3.50 | 4 | 34.50 | 29.50 | 2 | 0 | 0 | 4 |
| hq_rush | 0 | 0 | 5 | 8 | 0 | 0 | 0 | 3 |
| macro | 3 | 5 | 31.50 | 35 | 2 | 0 | 0 | 11 |
| passive | 0 | 2 | 8 | 12 | 0 | 0 | 0 | 1 |
| turtle | 1 | 5 | 34 | 34 | 0 | 0 | 0 | 0 |

### 1.5 k-means cross-check (numpy k-means++, k=6, 17 standardized features, best of 10 seeds)

| km | n | score | peak_army | trained | bases | HQ_L3_t | first_fwd | first_at_our_HQ | mean_fwd | income | moves | idle | avg_ms |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 0 | 101 | 0.767 | 21 | 40 | 6 | 108 | 62.500 | 120 | 0.305 | 16530 | 83 | 0.610 | 0 |
| 1 | 122 | 0.943 | 14 | 31 | 4 | 185 | 52 | 163 | 0.120 | 8902.500 | 70 | 0.667 | 0.010 |
| 2 | 7 | 0.571 | 16 | 43 | 6 | 158 | 63 | 116 | 1.280 | 12900 | 83 | 0.600 | 70.690 |
| 3 | 83 | 0.512 | 9 | 17 | 2 | 167 | 18 | 40.500 | 1.413 | 4005 | 35 | 0.735 | 0.010 |
| 4 | 145 | 0.755 | 21 | 57 | 7 | 178 | 59 | 148 | 1.085 | 15810 | 174 | 0.402 | 0.120 |
| 5 | 102 | 0.250 | 34 | 71 | 10 | 136 | 59.500 | 170 | 4.458 | 25852.500 | 206.500 | 0.373 | 0.055 |

Crosstab rule archetype (rows) x k-means cluster (columns):

| arch | 0 | 1 | 2 | 3 | 4 | 5 |
|---|---|---|---|---|---|---|
| hq_rush | 3 | 0 | 0 | 35 | 1 | 0 |
| early_aggro | 4 | 3 | 1 | 6 | 3 | 9 |
| aggro_macro | 15 | 6 | 2 | 10 | 53 | 90 |
| macro | 45 | 63 | 2 | 24 | 74 | 2 |
| turtle | 23 | 29 | 0 | 0 | 14 | 1 |
| passive | 11 | 20 | 2 | 8 | 0 | 0 |
| broken | 0 | 1 | 0 | 0 | 0 | 0 |

Cluster 5 (n=102, score 0.25: peak army 34, income 25.9k, 4.5 warriors forward on average, 207 move commands) is the big-army pusher; 90 of its 102 games are `aggro_macro` by the rules. Cluster 3 (n=83, score 0.51) is a "short game / small numbers" cluster that mixes naive HQ rushers (35) with the squad family and early losses - k-means on totals is confounded by game length, which is why the rule set uses timings and rates.

### 1.6 Two-dimensional view: army size x sustained aggression (score, then n)

| pw | fwd<=0.5 | 0.5-1.5 | 1.5-3 | >3 |
|---|---|---|---|---|
| <=10 | 0.98 | 0.47 | 0.19 | 0 |
| 11-20 | 0.94 | 0.80 | 0.57 | 0.18 |
| 21-30 | 0.91 | 0.84 | 0.17 | 0.29 |
| >30 | 0.79 | 0.73 | 0.33 | 0.19 |

| pw | fwd<=0.5 | 0.5-1.5 | 1.5-3 | >3 |
|---|---|---|---|---|
| <=10 | 58 | 16 | 21 | 1 |
| 11-20 | 120 | 49 | 36 | 11 |
| 21-30 | 58 | 32 | 26 | 36 |
| >30 | 12 | 15 | 18 | 50 |

Sustained presence in our half dominates army size: with <=0.5 warriors forward on average we score 0.79-0.98 whatever their army; with >3 forward we score 0.18-0.29. Note `mean_fwd` is partly an outcome (a winning opponent camps in our half), so the next table uses only turns 51-100:

### 1.7 Ex-ante version: forward presence during t51-100 x peak army (games >=100 turns; score then n)

| pw | mean_0 | mean_0-1 | mean_1-3 | mean_>3 | count_0 | count_0-1 | count_1-3 | count_>3 |
|---|---|---|---|---|---|---|---|---|
| <=10 | 0.97 | 0.90 | 0.62 | 0.25 | 39 | 21 | 17 | 4 |
| 11-20 | 0.92 | 0.93 | 0.77 | 0.22 | 50 | 78 | 66 | 16 |
| 21-30 | 0.88 | 0.77 | 0.44 | 0.40 | 29 | 49 | 50 | 24 |
| >30 | 0.65 | 0.50 | 0.21 | 0.25 | 13 | 33 | 31 | 18 |

### 1.8 Compute overlay

| compute | n | score | avg_ms | max_ms | peak_army | income |
|---|---|---|---|---|---|---|
| heavy | 9 | 0.556 | 67.350 | 77 | 16 | 13200 |
| light | 508 | 0.665 | 0.010 | 1 | 19 | 13237.500 |
| medium | 43 | 0.721 | 4.750 | 36 | 23 | 17535 |

| arch | mean_heavy | mean_light | mean_medium | count_heavy | count_light | count_medium |
|---|---|---|---|---|---|---|
| hq_rush |  | 0.763 | 0 |  | 38 | 1 |
| early_aggro | 0.500 | 0.560 |  | 1 | 25 |  |
| aggro_macro | 0 | 0.324 | 0.500 | 2 | 153 | 21 |
| macro | 0.500 | 0.810 | 0.972 | 3 | 189 | 18 |
| turtle | 1 | 0.889 | 1 | 1 | 63 | 3 |
| passive | 1 | 0.910 |  | 2 | 39 |  |
| broken |  | 1 |  |  | 1 |  |

The 9 heavy-compute games (avg >=20 ms):

| gid | opp | us_side | outcome | reason | hp_margin | opp_avg_ms | opp_max_ms | opp_tokens_end | arch | opp_peak_war | opp_total_income |
|---|---|---|---|---|---|---|---|---|---|---|---|
| 07_73217 | 1247 | LEFT | W | HQ_DESTROYED | 20 | 80.47 | 93 | 5 | passive | 16 | 5790 |
| 10_89109 | 519 | LEFT | D | TURN_LIMIT | 0 | 85.50 | 94 | 5 | macro | 29 | 28560 |
| 10_93461 | 474 | RIGHT | L | HQ_DESTROYED | -15 | 82.20 | 97 | 5 | aggro_macro | 21 | 13200 |
| 15_127532 | 474 | RIGHT | L | HQ_DESTROYED | -15 | 67.35 | 74 | 5 | aggro_macro | 34 | 12900 |
| 17_143451 | 1328 | LEFT | D | TURN_LIMIT | 0 | 55.05 | 75 | 5 | early_aggro | 12 | 7980 |
| 18_145887 | 1498 | LEFT | W | HQ_DESTROYED | 30 | 70.69 | 95 | 5 | passive | 3 | 2385 |
| 19_155908 | 469 | LEFT | W | TURN_LIMIT | 5 | 22.89 | 49 | 5 | turtle | 16 | 19200 |
| 21_168879 | 884 | LEFT | W | TURN_LIMIT | 5 | 62.09 | 77 | 5 | macro | 15 | 16170 |
| 29_240544 | 554 | LEFT | L | TURN_LIMIT | -5 | 24.98 | 68 | 5 | macro | 37 | 33465 |

Heavy compute is rare (1.6%) and not itself dangerous (0.56, n=9 - two of the losses are 474 in rounds 10/15 at 82/67 ms, the same team later used ~1 ms and still beat us). Medium compute (2-20 ms) scores 0.72, i.e. slightly *better* for us than light. Compute is not the threat; strategy is.


## 2. Opponent population by round

### 2.1 Archetype counts per round (20 games each) with our score and opponent medians

| round | hq_rush | early_aggro | aggro_macro | macro | turtle | passive | broken | score | opp_peak_army_med | opp_income_med | opp_HQL3_t_med | opp_reached_L3_% | opp_avg_ms_med | n_RIGHT |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 3 | 0 | 2 | 4 | 8 | 3 | 3 | 0 | 0.57 | 25.50 | 17737.50 | 103 | 60 | 0.01 | 20 |
| 4 | 5 | 1 | 7 | 1 | 3 | 3 | 0 | 0.68 | 24.50 | 14017.50 | 89 | 50 | 0.03 | 8 |
| 5 | 0 | 3 | 9 | 3 | 4 | 1 | 0 | 0.65 | 27 | 17767.50 | 96 | 65 | 0.21 | 1 |
| 6 | 1 | 1 | 8 | 7 | 3 | 0 | 0 | 0.68 | 20 | 13717.50 | 85.50 | 20 | 0.01 | 6 |
| 7 | 6 | 1 | 3 | 4 | 5 | 1 | 0 | 0.60 | 14.50 | 9097.50 | 129 | 15 | 0 | 2 |
| 8 | 2 | 1 | 9 | 3 | 2 | 3 | 0 | 0.62 | 22 | 16927.50 | 139.50 | 60 | 0.03 | 3 |
| 9 | 0 | 1 | 8 | 7 | 2 | 2 | 0 | 0.62 | 17.50 | 12480 | 102 | 35 | 0 | 2 |
| 10 | 2 | 1 | 7 | 9 | 1 | 0 | 0 | 0.65 | 19 | 12892.50 | 138 | 40 | 0.01 | 9 |
| 11 | 1 | 1 | 9 | 8 | 1 | 0 | 0 | 0.65 | 25.50 | 18990 | 152 | 40 | 0.07 | 8 |
| 12 | 2 | 1 | 9 | 8 | 0 | 0 | 0 | 0.68 | 20 | 16950 | 164 | 50 | 0.04 | 4 |
| 13 | 2 | 2 | 6 | 6 | 2 | 2 | 0 | 0.68 | 19.50 | 12202.50 | 116 | 45 | 0.01 | 0 |
| 14 | 2 | 2 | 6 | 7 | 1 | 2 | 0 | 0.65 | 19.50 | 13095 | 115 | 35 | 0 | 9 |
| 15 | 0 | 2 | 6 | 8 | 3 | 1 | 0 | 0.65 | 18.50 | 14017.50 | 156 | 60 | 0.01 | 8 |
| 16 | 2 | 0 | 5 | 11 | 0 | 1 | 1 | 0.68 | 17.50 | 15480 | 150 | 45 | 0 | 4 |
| 17 | 1 | 1 | 9 | 5 | 1 | 3 | 0 | 0.60 | 21 | 15060 | 137 | 65 | 0.11 | 0 |
| 18 | 0 | 0 | 9 | 5 | 5 | 1 | 0 | 0.62 | 21 | 15045 | 117.50 | 50 | 0.08 | 15 |
| 19 | 0 | 0 | 7 | 7 | 5 | 1 | 0 | 0.68 | 20.50 | 17280 | 145 | 55 | 0.01 | 8 |
| 20 | 2 | 1 | 6 | 8 | 3 | 0 | 0 | 0.68 | 19.50 | 14115 | 129 | 35 | 0.06 | 5 |
| 21 | 1 | 1 | 6 | 11 | 0 | 1 | 0 | 0.65 | 16.50 | 12592.50 | 154 | 45 | 0.06 | 6 |
| 22 | 1 | 0 | 6 | 10 | 1 | 2 | 0 | 0.72 | 18.50 | 13860 | 127 | 25 | 0.03 | 8 |
| 23 | 3 | 1 | 5 | 8 | 2 | 1 | 0 | 0.65 | 18.50 | 11130 | 99 | 30 | 0 | 0 |
| 24 | 1 | 0 | 6 | 10 | 2 | 1 | 0 | 0.70 | 18 | 11460 | 139 | 35 | 0.14 | 13 |
| 25 | 0 | 1 | 3 | 10 | 4 | 2 | 0 | 0.78 | 17.50 | 11887.50 | 161 | 30 | 0.20 | 7 |
| 26 | 0 | 1 | 6 | 8 | 4 | 1 | 0 | 0.70 | 23 | 16785 | 143 | 35 | 0.16 | 1 |
| 27 | 2 | 0 | 4 | 10 | 3 | 1 | 0 | 0.72 | 16 | 10087.50 | 161 | 15 | 0.07 | 1 |
| 28 | 0 | 0 | 3 | 11 | 1 | 5 | 0 | 0.70 | 19.50 | 13432.50 | 128 | 30 | 0.33 | 0 |
| 29 | 2 | 0 | 5 | 9 | 2 | 2 | 0 | 0.72 | 14.50 | 11460 | 104 | 20 | 0 | 1 |
| 30 | 1 | 1 | 5 | 8 | 4 | 1 | 0 | 0.72 | 17.50 | 12900 | 178.50 | 30 | 0 | 4 |

### 2.2 By bot-version era (r3-9 / r10-21 / r22-30; archetype share in %, then medians)

| arch | r3-9 | r10-21 | r22-30 |
|---|---|---|---|
| hq_rush | 10 | 6.20 | 5.60 |
| early_aggro | 7.10 | 5 | 2.20 |
| aggro_macro | 34.30 | 35.40 | 23.90 |
| macro | 23.60 | 38.80 | 46.70 |
| turtle | 15.70 | 9.20 | 12.80 |
| passive | 9.30 | 5 | 8.90 |
| broken | 0 | 0.40 | 0 |
| n | 140 | 240 | 180 |
| score | 0.63 | 0.65 | 0.71 |
| peak_army_med | 21 | 20 | 18 |
| income_med | 14257.50 | 14235 | 12180 |
| trained_med | 45 | 42 | 43 |
| bases_med | 6.50 | 6 | 5 |
| HQ_L2_t_med | 63 | 69 | 60 |
| HQ_L3_t_med | 102 | 144 | 145.50 |
| first_fwd_t_med | 55.50 | 51 | 54 |
| mean_fwd_med | 0.53 | 1.26 | 0.48 |
| avg_ms_med | 0.01 | 0.01 | 0.08 |
| inc_ratio_med | 0.65 | 0.76 | 0.63 |
| L3_reached_% | 43.60 | 47.10 | 27.80 |
| L4_reached_% | 17.10 | 19.60 | 11.10 |

### 2.3 Cleaner evolution proxies (long games only where noted)

| index | r3-9 | r10-21 | r22-30 |
|---|---|---|---|
| n_games | 140 | 240 | 180 |
| n_games_>=150t | 115 | 213 | 165 |
| opp_HQ_L2_by_t100_%(>=150t) | 70.40 | 56.80 | 50.30 |
| opp_HQ_L3_by_t150_%(>=150t) | 43.50 | 26.80 | 15.20 |
| opp_army_t100_med(>=150t) | 15 | 15 | 13 |
| opp_army_t150_med(>=150t) | 16 | 13 | 11 |
| opp_cuminc_t100_med(>=150t) | 8190 | 7725 | 7125 |
| us_cuminc_t100_med(>=150t) | 8460 | 8715 | 8850 |
| opp_bases_t100_med(>=150t) | 5 | 5 | 4 |
| opp_first_fwd4_med | 73 | 64 | 65 |
| opp_4stack_by_t60_% | 22.90 | 37.50 | 31.70 |
| opp_avg_ms>=2_% | 4.30 | 10 | 12.20 |
| opp_idle_turn_frac_med | 0.54 | 0.51 | 0.54 |
| opp_moves_per100_med | 51.38 | 67 | 59.75 |
| squad_family_% | 3.60 | 6.20 | 4.40 |
| hq_rush_% | 10 | 6.20 | 5.60 |
| our_score | 0.63 | 0.65 | 0.71 |
| our_score_LEFT | 0.72 | 0.79 | 0.77 |
| our_score_RIGHT | 0.42 | 0.37 | 0.50 |
| n_RIGHT | 42 | 76 | 35 |

### 2.4 Our score per archetype per era

| arch | mean_r3-9 | mean_r10-21 | mean_r22-30 | count_r3-9 | count_r10-21 | count_r22-30 |
|---|---|---|---|---|---|---|
| hq_rush | 0.571 | 0.800 | 0.900 | 14 | 15 | 10 |
| early_aggro | 0.700 | 0.375 | 0.750 | 10 | 12 | 4 |
| aggro_macro | 0.333 | 0.324 | 0.384 | 48 | 85 | 43 |
| macro | 0.788 | 0.844 | 0.804 | 33 | 93 | 84 |
| turtle | 0.886 | 0.977 | 0.826 | 22 | 22 | 23 |
| passive | 0.923 | 1 | 0.844 | 13 | 12 | 16 |
| broken |  | 1 |  |  | 1 |  |

Reading: (a) the population got *less* eco-heavy over time - among games lasting >=150 turns the share of opponents with HQ L3 by t150 fell 43.5% -> 26.8% -> 15.2%, opponent army at t150 fell 16 -> 13 -> 11, while early 4-stacks (by t60) rose 23% -> 38% -> 32% and >=2 ms compute rose 4% -> 10% -> 12%. Part of this is the growing, newer participant pool (~270 teams in r3, ~860 in r30), part is our own stronger bot suppressing their economy (these are opponent stats *against us*, so they are not a clean census).
(b) Our score by era is 0.63 / 0.65 / 0.71, but within `aggro_macro` it is 0.33 / 0.32 / 0.38 and within `macro` 0.79 / 0.84 / 0.80. Re-weighting the r22-30 per-archetype scores with the r10-21 archetype mix gives 0.659 (actual 0.714); the r3-9 figure re-weighted is 0.621 (actual 0.632). So the late improvement is mostly mix (and fewer RIGHT-side games: 42/140, 76/240, 35/180), not a better answer to the pushers.
(c) hq_rush share fell 10% -> 6% -> 6% and our score vs it rose 0.57 -> 0.80 -> 0.90; early_aggro fell 7% -> 5% -> 2%.

Per-round extra proxies:

| index | 3 | 4 | 5 | 6 | 7 | 8 | 9 | 10 | 11 | 12 | 13 | 14 | 15 | 16 | 17 | 18 | 19 | 20 | 21 | 22 | 23 | 24 | 25 | 26 | 27 | 28 | 29 | 30 |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 4stack_by_t60_% | 5 | 35 | 5 | 30 | 45 | 20 | 20 | 25 | 35 | 45 | 20 | 45 | 30 | 40 | 40 | 35 | 25 | 50 | 60 | 50 | 35 | 30 | 25 | 25 | 45 | 20 | 30 | 25 |
| squad_% | 0 | 5 | 0 | 10 | 10 | 0 | 0 | 0 | 5 | 5 | 5 | 5 | 10 | 15 | 5 | 5 | 5 | 10 | 5 | 10 | 5 | 5 | 0 | 10 | 0 | 10 | 0 | 0 |
| opp_L2_by_t100_% | 75 | 65 | 80 | 55 | 40 | 60 | 55 | 60 | 70 | 40 | 50 | 45 | 70 | 50 | 55 | 60 | 65 | 40 | 55 | 45 | 60 | 45 | 50 | 55 | 35 | 50 | 55 | 50 |
| score | 0.57 | 0.68 | 0.65 | 0.68 | 0.60 | 0.62 | 0.62 | 0.65 | 0.65 | 0.68 | 0.68 | 0.65 | 0.65 | 0.68 | 0.60 | 0.62 | 0.68 | 0.68 | 0.65 | 0.72 | 0.65 | 0.70 | 0.78 | 0.70 | 0.72 | 0.70 | 0.72 | 0.72 |


## 3. Profiles of every opponent met >= 4 times (36 opponents)

Sorted by our score (worst first). Columns are medians over that opponent's games unless stated; `archs` lists all archetypes the opponent showed; `t1` = their most common turn-1 command string.

| opp | n | score | W | D | L | rounds | n_LEFT | arch | archs | med_margin | med_nturns | first_base_t | HQ_L2_t | HQ_L3_t | HQ_L4_t | peak_army | peak_army_t | trained | peak_bases | first_fwd_t | first_at_our_HQ_t | first_5fwd_t | mean_fwd | income | inc_ratio | army_ratio | siege_on_us | avg_ms | max_ms | build_frac | our_bases_lost | t1 |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 455 | 5 | 0 | 0 | 0 | 5 | 8,9,11,26,30 | 3 | aggro_macro | aggro_macro/macro | -10 | 195 | 3 | 67 | 170.50 | 182 | 29 | 173 | 66 | 8 | 77 | 159 | 90 | 2.70 | 19440 | 1.61 | 2.07 | 10 | 0.01 | 1 | 0.43 | 6 | MOVE A1 1;MOVE A2 6 |
| 1170 | 5 | 0 | 0 | 0 | 5 | 6,11,12,20,24 | 1 | aggro_macro | aggro_macro | -15 | 182 | 2 | 118 | 129.50 |  | 30 | 163 | 50 | 7 | 40 | 181 | 40 | 3.77 | 17640 | 2.19 | 3.33 | 10 | 0.15 | 40 | 0.37 | 8 | MOVE A2 16;MOVE A3 5 |
| 313 | 7 | 0 | 0 | 0 | 7 | 6,14,19,21,24,26,28 | 5 | aggro_macro | aggro_macro/macro | -20 | 162 | 2 | 47 | 127.50 | 163 | 26 | 119 | 62 | 7 | 78 | 129 | 78 | 2.49 | 16470 | 1.44 | 1.40 | 15 | 1.13 | 44 | 0.27 | 8 | MOVE A2 5;MOVE A3 6 |
| 474 | 8 | 0 | 0 | 0 | 8 | 4,10,15,21,22,25,27,30 | 0 | aggro_macro | aggro_macro/macro | -15 | 107.50 | 2 | 48 |  |  | 22.50 | 97.50 | 31 | 6 | 57 | 94.50 | 68 | 3.20 | 9592.50 | 1.45 | 1.71 | 15 | 1.06 | 97 | 0.30 | 7 | MOVE A2 1;MOVE A3 1 |
| 138 | 6 | 0.17 | 1 | 0 | 5 | 5,10,11,20,23,30 | 3 | aggro_macro | aggro_macro/early_aggro/macro | -20 | 169.50 | 2 | 94 | 101 |  | 42 | 161 | 82.50 | 11 | 61.50 | 164 | 72.50 | 4.12 | 23377.50 | 1.55 | 2.11 | 15 | 0.01 | 26 | 0.35 | 7 | MOVE A1 8 |
| 97 | 5 | 0.20 | 1 | 0 | 4 | 3,6,19,22,25 | 1 | aggro_macro | aggro_macro/early_aggro/turtle | -15 | 187 | 3 | 74.50 |  |  | 32 | 163 | 79 | 10 | 71 | 183 | 138 | 3.43 | 21210 | 1.31 | 1.71 | 15 | 9.46 | 57 | 0.30 | 8 | MOVE A2 29;MOVE A3 26;TRAIN 1 |
| 395 | 5 | 0.20 | 1 | 0 | 4 | 6,7,14,21,22 | 0 | macro | aggro_macro/macro | -10 | 107 | 2 |  |  |  | 10 | 52 | 19 | 2 | 30 | 84 | 30 | 2.33 | 4680 | 1.04 | 1.67 | 10 | 0 | 1 | 0.18 | 7 | MOVE A2 10;MOVE A3 25 |
| 994 | 5 | 0.20 | 1 | 0 | 4 | 5,16,18,19,20 | 1 | macro | aggro_macro/macro | -10 | 104 | 3 | 115 |  |  | 10 | 67 | 21 | 2 | 30 | 65 | 30 | 1.41 | 4575 | 1.54 | 1.67 | 10 | 0.38 | 48 | 0.20 | 5 | MOVE A2 2;MOVE A3 47 |
| 129 | 4 | 0.25 | 1 | 0 | 3 | 4,10,12,14 | 1 | aggro_macro | aggro_macro/early_aggro/macro | -12.50 | 195.50 | 2.50 | 82.50 | 118 | 170 | 14 | 86.50 | 16 | 3.50 | 28.50 | 120 | 29 | 4.06 | 9367.50 | 1.80 | 1.67 | 5 | 0 | 1 | 0.57 | 4 | MOVE A3 2;MOVE A2 40 |
| 1167 | 4 | 0.25 | 1 | 0 | 3 | 5,13,16,23 | 4 | aggro_macro | aggro_macro | -7.50 | 200 | 2 | 79 | 117 | 160 | 33 | 121.50 | 53 | 10.50 | 69.50 | 183.50 | 81 | 5.63 | 25185 | 1.71 | 1.52 | 0 | 0.40 | 50 | 0.54 | 12 | MOVE B1 103;MOVE B2 90 |
| 1237 | 6 | 0.25 | 1 | 1 | 4 | 7,16,22,24,25,26 | 1 | aggro_macro | aggro_macro/early_aggro/macro | -12.50 | 200 | 2 | 129 | 150 | 171 | 25.50 | 164.50 | 66.50 | 11.50 | 52 | 181.50 | 65.50 | 3.21 | 24517.50 | 1.73 | 1.34 | 0 | 0.09 | 16 | 0.49 | 8 | MOVE A2 7 |
| 184 | 5 | 0.30 | 1 | 1 | 3 | 5,6,9,15,20 | 1 | macro | aggro_macro/macro | -5 | 200 | 2 | 80 | 128 | 130 | 12 | 57 | 48 | 5 | 48 | 98 | 67.50 | 1.80 | 12525 | 1.25 | 1.33 | 0 | 0.11 | 47 | 0.31 | 7 | MOVE A2 10;MOVE A3 18;TRAIN 1 |
| 997 | 5 | 0.30 | 1 | 1 | 3 | 12,22,25,26,27 | 3 | aggro_macro | aggro_macro/macro | -10 | 197 | 3 | 170 | 179 |  | 19 | 156 | 40 | 6 | 33 | 194 | 65 | 1.75 | 12525 | 1.81 | 2.38 | 10 | 0.13 | 50 | 0.32 | 4 | MOVE A1 5;MOVE A3 38 |
| 865 | 4 | 0.38 | 1 | 1 | 2 | 4,12,24,27 | 1 | aggro_macro | aggro_macro/turtle | -7.50 | 190.50 | 2 | 70 | 139 | 186 | 45.50 | 171 | 81.50 | 10 | 35 | 137.50 | 114 | 3.89 | 23722.50 | 1.52 | 2.51 | 7.50 | 0.01 | 9 | 0.38 | 8.50 | MOVE A2 2;MOVE A3 3 |
| 618 | 4 | 0.50 | 2 | 0 | 2 | 3,9,11,18 | 2 | aggro_macro | aggro_macro/macro | -5 | 188 | 2 | 59.50 | 127 | 191 | 41.50 | 142 | 70.50 | 10.50 | 65.50 | 170 | 125 | 1.62 | 22455 | 1.13 | 1.73 | 7.50 | 0.01 | 33 | 0.40 | 2.50 | MOVE A2 17;MOVE A3 19 |
| 642 | 4 | 0.50 | 2 | 0 | 2 | 11,15,20,26 | 2 | macro | aggro_macro/macro | 2.50 | 199.50 | 2 | 58 |  |  | 19 | 95 | 49.50 | 7 | 57.50 | 120 | 59 | 1.42 | 12382.50 | 1.00 | 1.19 | 5 | 0 | 4 | 0.22 | 8.50 | MOVE A2 12;MOVE A3 25 |
| 696 | 4 | 0.50 | 2 | 0 | 2 | 9,11,26,28 | 4 | aggro_macro | aggro_macro/macro/turtle | -2.50 | 200 | 2.50 | 49 | 156 | 197 | 23.50 | 144 | 51.50 | 5.50 | 66 | 187 | 69 | 1.06 | 14482.50 | 1.13 | 1.09 | 0 | 0.10 | 28 | 0.36 | 6 | MOVE B2 52;MOVE B3 61 |
| 762 | 4 | 0.50 | 2 | 0 | 2 | 9,11,12,19 | 1 | aggro_macro | aggro_macro/macro | 0 | 200 | 2 | 73 | 106.50 | 138 | 21.50 | 148 | 66.50 | 7.50 | 53 | 158.50 | 63.50 | 1.77 | 18952.50 | 0.99 | 1.10 | 0 | 0.01 | 15 | 0.36 | 13 | MOVE A2 22 |
| 518 | 5 | 0.50 | 2 | 1 | 2 | 16,19,21,23,27 | 4 | macro | macro | 0 | 200 | 2 |  |  |  | 14 | 75 | 28 | 3 | 40 | 52 | 40 | 1.80 | 6885 | 1.18 | 1.40 | 0 | 0 | 1 | 0.18 | 6 | MOVE A1 3;MOVE A2 42 |
| 773 | 5 | 0.50 | 2 | 1 | 2 | 5,7,14,16,29 | 4 | aggro_macro | aggro_macro/macro | 0 | 200 | 2 | 78 | 134 | 172.50 | 32 | 190 | 64 | 6 | 31 | 191 | 32 | 1.38 | 16485 | 1.56 | 2 | 0 | 0 | 1 | 0.51 | 7 | MOVE A3 4;MOVE A2 16 |
| 916 | 4 | 0.62 | 1 | 3 | 0 | 3,22,24,26 | 1 | macro | aggro_macro/macro | 0 | 200 | 2.50 | 67.50 | 174.50 | 162 | 18.50 | 138 | 57.50 | 6 | 65 |  | 71 | 0.27 | 17385 | 0.77 | 0.87 | 0 | 1.32 | 29 | 0.39 | 3.50 | MOVE A2 10;MOVE A3 8 |
| 1227 | 4 | 0.62 | 2 | 1 | 1 | 8,15,16,25 | 2 | macro | aggro_macro/macro | 5 | 200 | 2.50 | 96 | 138 | 172 | 19 | 120.50 | 62 | 8.50 | 63.50 | 96 | 85 | 0.85 | 21082.50 | 1.07 | 0.95 | 0 | 9.15 | 86 | 0.52 | 6 | MOVE A2 13;MOVE A3 3 |
| 256 | 6 | 0.67 | 3 | 2 | 1 | 12,15,21,22,28,30 | 4 | aggro_macro | aggro_macro/early_aggro/macro | 2.50 | 200 | 2 | 69 | 189 | 185 | 29 | 174.50 | 60.50 | 8.50 | 51 | 192 | 77.50 | 1.74 | 21112.50 | 0.99 | 1.31 | 0 | 0.01 | 17 | 0.43 | 10 | MOVE A3 3;MOVE A2 43 |
| 25 | 8 | 0.69 | 5 | 1 | 2 | 7,16,18,19,22,27,28,30 | 5 | turtle | macro/turtle | 5 | 200 | 2 | 69.50 | 151 | 197 | 19 | 147 | 55 | 6 | 73.50 | 176 | 137.50 | 0.15 | 16702.50 | 0.72 | 0.89 | 0 | 0 | 1 | 0.39 | 1 | MOVE A2 3 |
| 453 | 4 | 0.75 | 3 | 0 | 1 | 3,9,21,29 | 2 | turtle | aggro_macro/macro/turtle | 12.50 | 200 | 2.50 | 72.50 | 125 | 170 | 12.50 | 126.50 | 31.50 | 4 | 49.50 | 197 | 64 | 1.37 | 8550 | 0.53 | 0.65 | 0 | 0 | 0 | 0.34 | 2 | MOVE A1 2;MOVE A2 29;TRAIN 1 |
| 657 | 4 | 0.75 | 3 | 0 | 1 | 6,14,22,26 | 1 | turtle | aggro_macro/macro/turtle | 7.50 | 200 | 2.50 | 94 | 82 |  | 20.50 | 111 | 60.50 | 5 | 106 | 164 | 62 | 1.66 | 14835 | 0.77 | 1.22 | 0 | 0.07 | 9 | 0.23 | 6.50 | MOVE A2 28;MOVE A3 40 |
| 978 | 4 | 0.75 | 3 | 0 | 1 | 4,8,13,20 | 2 | hq_rush | aggro_macro/hq_rush/macro | 10 | 200 | 29 | 150 | 156.50 | 175 | 19 | 100.50 | 32 | 6.50 | 23.50 | 17 | 42.50 | 0.24 | 12315 | 0.55 | 0.93 | 0 | 0.01 | 6 | 0.46 | 0.50 | nan |
| 1154 | 6 | 0.75 | 4 | 1 | 1 | 12,15,17,18,21,22 | 4 | aggro_macro | aggro_macro/early_aggro/macro/turtle | 5 | 200 | 2.50 | 97.50 | 165 | 150.50 | 25 | 118.50 | 64.50 | 8 | 77.50 | 113.50 | 91 | 1.46 | 19560 | 0.82 | 1.10 | 0 | 2.46 | 33 | 0.39 | 7 | MOVE A2 44;TRAIN 1 |
| 36 | 5 | 0.80 | 3 | 2 | 0 | 9,10,20,25,29 | 5 | aggro_macro | aggro_macro/macro/passive | 5 | 200 | 3 | 47 | 93 | 139 | 12 | 85 | 25 | 6 | 58 | 192 | 61 | 0.66 | 11070 | 0.62 | 0.90 | 0 | 0 | 17 | 0.36 | 4 | MOVE B1 105;MOVE B2 91 |
| 290 | 5 | 0.80 | 4 | 0 | 1 | 6,12,20,23,25 | 5 | macro | macro/turtle | 20 | 200 | 2 | 95.50 |  |  | 9 | 30 | 31 | 2 | 31 | 97.50 | 31 | 0.28 | 8085 | 0.30 | 0.46 | 0 | 0 | 11 | 0.24 | 3 | MOVE B2 102;MOVE B3 81 |
| 894 | 5 | 0.80 | 3 | 2 | 0 | 3,6,12,19,23 | 4 | aggro_macro | aggro_macro/macro | 5 | 200 | 2 | 53 | 171 | 193 | 24 | 149 | 67 | 7 | 44 | 154 | 114.50 | 1.42 | 18015 | 0.87 | 1.12 | 0 | 0 | 0 | 0.26 | 4 | MOVE A2 4;MOVE A3 6 |
| 1010 | 5 | 0.80 | 4 | 0 | 1 | 14,17,19,21,26 | 4 | aggro_macro | aggro_macro/macro/turtle | 10 | 200 | 2 | 56 | 150 | 172 | 25 | 107 | 43 | 8 | 71 |  | 94.50 | 0.85 | 15000 | 0.74 | 0.94 | 0 | 0 | 0 | 0.57 | 7 | MOVE A2 3;MOVE A3 37 |
| 345 | 4 | 0.88 | 3 | 1 | 0 | 3,4,5,23 | 2 | macro | aggro_macro/macro/passive | 22.50 | 198.50 | 2.50 | 61 | 77 | 122 | 20.50 | 132.50 | 36 | 8.50 | 59.50 | 112 | 107 | 0.29 | 18997.50 | 0.45 | 0.27 | 0 | 0.01 | 88 | 0.61 | 0 | MOVE A1 1;MOVE A2 16 |
| 967 | 5 | 0.90 | 4 | 1 | 0 | 19,21,24,26,30 | 2 | macro | macro | 5 | 200 | 2 | 74 | 143 | 187.50 | 19 | 196 | 64 | 6 | 43 |  |  | 0.29 | 17700 | 0.67 | 0.86 | 0 | 0.03 | 12 | 0.33 | 1 | MOVE A2 1;MOVE A3 3 |
| 187 | 4 | 1 | 4 | 0 | 0 | 5,11,14,24 | 2 | macro | aggro_macro/macro/turtle | 15 | 200 | 2.50 | 60.50 |  |  | 22.50 | 91 | 51 | 7 | 66 |  | 78 | 0.32 | 13132.50 | 0.53 | 0.93 | 0 | 6.57 | 36 | 0.40 | 8 | MOVE A1 12;MOVE A2 10 |
| 1059 | 4 | 1 | 4 | 0 | 0 | 10,21,26,27 | 4 | macro | early_aggro/macro | 15 | 200 | 2 | 42 |  |  | 12 | 55 | 36 | 3.50 | 44.50 | 71 | 53 | 0.58 | 9442.50 | 0.44 | 0.71 | 0 | 0.01 | 11 | 0.28 | 3 | MOVE B2 59;MOVE B3 13 |

### 3.1 Per-game results for each of these opponents

**Opponent 455** - n=5, score 0.00 (0W/0D/5L), rounds 8,9,11,26,30, archetypes aggro_macro/macro

| round | us_side | outcome | reason | nturns | hp_margin | arch | opp_peak_war | us_peak_war | opp_total_income | us_total_income | opp_hq_l2_turn | opp_hq_l3_turn | opp_hq_l4_turn | us_hq_l3_turn | opp_first_forward_t | opp_first_at_enemy_hq_t | opp_first_fwd5_t | us_hq_siege_taken | opp_hq_siege_taken | us_bases_lost | opp_avg_ms | opp_turn1_cmds |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 8 | LEFT | L | TURN_LIMIT | 200 | -10 | aggro_macro | 34 | 12 | 27225 | 10740 | 67 | 167 | 182 |  | 114 |  | 131 | 0 | 0 | 6 | 0.0 | MOVE B1 45;MOVE B2 36 |
| 9 | LEFT | L | HQ_DESTROYED | 195 | -20 | aggro_macro | 29 | 14 | 19440 | 13185 | 105 | 191 |  |  | 62 | 189 | 90 | 10 | 0 | 5 | 0 | MOVE B1 50;MOVE B2 33 |
| 11 | RIGHT | L | HQ_DESTROYED | 173 | -20 | aggro_macro | 49 | 20 | 31455 | 15165 | 51 | 166 |  |  | 77 | 159 | 77 | 15 | 0 | 9 | 0.0 | MOVE A1 1;MOVE A2 6 |
| 26 | LEFT | L | HQ_DESTROYED | 165 | -10 | aggro_macro | 26 | 19 | 18045 | 11235 |  |  |  |  | 61 | 139 | 77 | 10 | 0 | 9 | 0.0 | MOVE B1 73;MOVE B2 56 |
| 30 | RIGHT | L | TURN_LIMIT | 200 | -5 | macro | 16 | 17 | 14685 | 17415 | 67 | 174 |  |  | 90 |  | 155 | 0 | 0 | 1 | 0 | MOVE A1 30;MOVE A2 26 |

**Opponent 1170** - n=5, score 0.00 (0W/0D/5L), rounds 6,11,12,20,24, archetypes aggro_macro

| round | us_side | outcome | reason | nturns | hp_margin | arch | opp_peak_war | us_peak_war | opp_total_income | us_total_income | opp_hq_l2_turn | opp_hq_l3_turn | opp_hq_l4_turn | us_hq_l3_turn | opp_first_forward_t | opp_first_at_enemy_hq_t | opp_first_fwd5_t | us_hq_siege_taken | opp_hq_siege_taken | us_bases_lost | opp_avg_ms | opp_turn1_cmds |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 6 | RIGHT | L | HQ_DESTROYED | 182 | -15 | aggro_macro | 25 | 13 | 17640 | 12510 | 143 |  |  |  | 60 | 181 | 60 | 15 | 0 | 17 | 0.1 | MOVE A2 7;MOVE A3 10 |
| 11 | RIGHT | L | HQ_DESTROYED | 170 | -20 | aggro_macro | 58 | 11 | 19770 | 7005 | 117 | 128 |  |  | 39 | 169 | 39 | 10 | 0 | 7 | 0.2 | MOVE A2 16;MOVE A3 5 |
| 12 | RIGHT | L | HQ_DESTROYED | 173 | -20 | aggro_macro | 52 | 12 | 19020 | 7440 | 119 | 131 |  |  | 39 | 172 | 39 | 10 | 0 | 8 | 0.1 | MOVE A2 1;MOVE A3 12 |
| 20 | LEFT | L | HQ_DESTROYED | 185 | -15 | aggro_macro | 30 | 9 | 13695 | 6255 | 70 |  |  |  | 40 | 184 | 40 | 10 | 0 | 8 | 0.1 | MOVE B2 53;MOVE B3 54 |
| 24 | RIGHT | L | HQ_DESTROYED | 188 | -10 | aggro_macro | 20 | 14 | 12480 | 10545 |  |  |  |  | 76 | 186 | 76 | 10 | 0 | 10 | 0.2 | MOVE A2 1;MOVE A3 5 |

**Opponent 313** - n=7, score 0.00 (0W/0D/7L), rounds 6,14,19,21,24,26,28, archetypes aggro_macro/macro

| round | us_side | outcome | reason | nturns | hp_margin | arch | opp_peak_war | us_peak_war | opp_total_income | us_total_income | opp_hq_l2_turn | opp_hq_l3_turn | opp_hq_l4_turn | us_hq_l3_turn | opp_first_forward_t | opp_first_at_enemy_hq_t | opp_first_fwd5_t | us_hq_siege_taken | opp_hq_siege_taken | us_bases_lost | opp_avg_ms | opp_turn1_cmds |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 6 | LEFT | L | HQ_DESTROYED | 124 | -10 | aggro_macro | 27 | 13 | 13155 | 9105 |  |  |  |  | 61 | 119 | 61 | 15 | 0 | 15 | 0.7 | MOVE B2 89;MOVE B3 77 |
| 14 | RIGHT | L | HQ_DESTROYED | 175 | -10 | macro | 20 | 12 | 16470 | 8310 |  |  |  |  | 54 | 89 | 54 | 15 | 0 | 8 | 0.5 | MOVE A2 5;MOVE A3 6 |
| 19 | LEFT | L | HQ_DESTROYED | 162 | -20 | aggro_macro | 40 | 21 | 21855 | 16185 | 56 | 145 |  |  | 103 | 159 | 103 | 15 | 0 | 13 | 1.9 | MOVE B2 75 |
| 21 | LEFT | L | HQ_DESTROYED | 144 | -15 | aggro_macro | 21 | 15 | 13245 | 7740 | 42 |  |  |  | 59 | 120 | 59 | 15 | 0 | 6 | 1.1 | MOVE B2 76 |
| 24 | RIGHT | L | TURN_LIMIT | 200 | -20 | macro | 18 | 16 | 19980 | 10920 | 42 | 139 | 163 |  | 86 | 129 | 90 | 0 | 0 | 6 | 1.0 | MOVE A2 7;MOVE A3 22 |
| 26 | LEFT | L | HQ_DESTROYED | 191 | -20 | aggro_macro | 29 | 23 | 23445 | 21480 | 51 | 116 |  |  | 81 | 181 | 81 | 15 | 0 | 11 | 2.2 | MOVE B2 99 |
| 28 | LEFT | L | HQ_DESTROYED | 147 | -20 | macro | 26 | 22 | 15690 | 11595 | 47 | 105 |  |  | 78 | 143 | 78 | 10 | 0 | 8 | 1.4 | MOVE B2 90 |

**Opponent 474** - n=8, score 0.00 (0W/0D/8L), rounds 4,10,15,21,22,25,27,30, archetypes aggro_macro/macro

| round | us_side | outcome | reason | nturns | hp_margin | arch | opp_peak_war | us_peak_war | opp_total_income | us_total_income | opp_hq_l2_turn | opp_hq_l3_turn | opp_hq_l4_turn | us_hq_l3_turn | opp_first_forward_t | opp_first_at_enemy_hq_t | opp_first_fwd5_t | us_hq_siege_taken | opp_hq_siege_taken | us_bases_lost | opp_avg_ms | opp_turn1_cmds |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 4 | RIGHT | L | HQ_DESTROYED | 109 | -15 | aggro_macro | 24 | 10 | 10290 | 5295 | 48 |  |  |  | 56 | 102 | 68 | 15 | 0 | 5 | 1.0 | MOVE A2 17;MOVE A3 17 |
| 10 | RIGHT | L | HQ_DESTROYED | 144 | -15 | aggro_macro | 21 | 12 | 13200 | 9135 | 58 |  |  |  | 54 | 91 | 81 | 15 | 0 | 10 | 82.2 | MOVE A2 1;MOVE A3 1 |
| 15 | RIGHT | L | HQ_DESTROYED | 122 | -15 | aggro_macro | 34 | 17 | 12900 | 10035 | 48 |  |  |  | 72 | 116 | 73 | 15 | 0 | 9 | 67.3 | MOVE A2 1;MOVE A3 1 |
| 21 | RIGHT | L | HQ_DESTROYED | 119 | -15 | aggro_macro | 25 | 15 | 9615 | 8265 | 43 |  |  |  | 58 | 115 | 58 | 15 | 0 | 9 | 0.8 | MOVE A2 5;MOVE A3 5 |
| 22 | RIGHT | L | HQ_DESTROYED | 92 | -10 | aggro_macro | 20 | 14 | 7695 | 7695 |  |  |  |  | 55 | 90 | 55 | 10 | 0 | 10 | 1.1 | MOVE A2 2;MOVE A3 2 |
| 25 | RIGHT | L | HQ_DESTROYED | 91 | -10 | macro | 14 | 8 | 5415 | 3300 |  |  |  |  | 68 | 74 | 68 | 10 | 0 | 2 | 1.0 | MOVE A2 2;MOVE A3 2 |
| 27 | RIGHT | L | HQ_DESTROYED | 106 | -10 | macro | 12 | 8 | 4860 | 3165 |  |  |  |  | 32 | 77 | 35 | 10 | 0 | 3 | 1.2 | MOVE A2 6;MOVE A3 6 |
| 30 | RIGHT | L | HQ_DESTROYED | 105 | -15 | aggro_macro | 31 | 21 | 9570 | 6540 | 59 |  |  |  | 83 | 98 | 83 | 15 | 0 | 3 | 1.1 | MOVE A2 1;MOVE A3 22 |

**Opponent 138** - n=6, score 0.17 (1W/0D/5L), rounds 5,10,11,20,23,30, archetypes aggro_macro/early_aggro/macro

| round | us_side | outcome | reason | nturns | hp_margin | arch | opp_peak_war | us_peak_war | opp_total_income | us_total_income | opp_hq_l2_turn | opp_hq_l3_turn | opp_hq_l4_turn | us_hq_l3_turn | opp_first_forward_t | opp_first_at_enemy_hq_t | opp_first_fwd5_t | us_hq_siege_taken | opp_hq_siege_taken | us_bases_lost | opp_avg_ms | opp_turn1_cmds |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 5 | LEFT | L | HQ_DESTROYED | 164 | -20 | aggro_macro | 76 | 36 | 32670 | 18690 | 95 | 101 |  |  | 58 | 164 | 103 | 15 | 0 | 5 | 0 | MOVE B1 90;MOVE B2 65;TRAIN 1 |
| 10 | RIGHT | L | HQ_DESTROYED | 160 | -20 | aggro_macro | 57 | 18 | 28095 | 14070 | 94 | 101 |  |  | 72 | 155 | 72 | 15 | 0 | 9 | 0.0 | MOVE A2 9;MOVE A3 19 |
| 11 | RIGHT | L | HQ_DESTROYED | 175 | -15 | early_aggro | 38 | 18 | 19500 | 14520 | 97 |  |  |  | 18 | 170 | 70 | 15 | 0 | 12 | 0.1 | MOVE A1 8 |
| 20 | RIGHT | L | HQ_DESTROYED | 187 | -20 | aggro_macro | 46 | 25 | 27255 | 22440 | 91 | 103 |  | 159 | 68 | 179 | 73 | 20 | 0 | 12 | 0.0 | MOVE A2 2;MOVE A3 24 |
| 23 | LEFT | L | HQ_DESTROYED | 143 | -20 | aggro_macro | 30 | 14 | 16110 | 8385 | 81 | 97 |  |  | 65 | 140 | 65 | 10 | 0 | 5 | 0 | MOVE B2 27;MOVE B3 45 |
| 30 | LEFT | W | TURN_LIMIT | 200 | 20 | macro | 17 | 29 | 11175 | 29520 |  |  |  | 142 | 45 |  | 79 | 0 | 0 | 3 | 0 | MOVE B2 67;MOVE B3 34 |

**Opponent 97** - n=5, score 0.20 (1W/0D/4L), rounds 3,6,19,22,25, archetypes aggro_macro/early_aggro/turtle

| round | us_side | outcome | reason | nturns | hp_margin | arch | opp_peak_war | us_peak_war | opp_total_income | us_total_income | opp_hq_l2_turn | opp_hq_l3_turn | opp_hq_l4_turn | us_hq_l3_turn | opp_first_forward_t | opp_first_at_enemy_hq_t | opp_first_fwd5_t | us_hq_siege_taken | opp_hq_siege_taken | us_bases_lost | opp_avg_ms | opp_turn1_cmds |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 3 | RIGHT | L | HQ_DESTROYED | 181 | -10 | early_aggro | 38 | 12 | 17460 | 8490 |  |  |  |  | 4 | 150 | 65 | 15 | 0 | 8 | 0.6 | MOVE A2 29;MOVE A3 26;TRAIN 1 |
| 6 | LEFT | L | HQ_DESTROYED | 164 | -15 | aggro_macro | 45 | 26 | 27030 | 18450 | 70 |  |  |  | 71 | 160 | 107 | 15 | 0 | 9 | 2.9 | MOVE B2 54;MOVE B3 101 |
| 19 | RIGHT | L | HQ_DESTROYED | 187 | -15 | aggro_macro | 32 | 22 | 22920 | 20040 | 79 |  |  | 166 | 73 | 183 | 160 | 20 | 0 | 10 | 9.8 | MOVE A2 52;MOVE A3 12 |
| 22 | RIGHT | L | HQ_DESTROYED | 194 | -15 | aggro_macro | 29 | 17 | 21210 | 16215 | 80 |  |  |  | 69 | 189 | 138 | 15 | 0 | 7 | 9.5 | MOVE A2 37;MOVE A3 7 |
| 25 | RIGHT | W | TURN_LIMIT | 200 | 5 | turtle | 20 | 23 | 16485 | 24270 | 58 |  |  | 196 | 183 | 189 | 183 | 0 | 0 | 0 | 10.7 | MOVE A2 38;MOVE A3 4 |

**Opponent 395** - n=5, score 0.20 (1W/0D/4L), rounds 6,7,14,21,22, archetypes aggro_macro/macro

| round | us_side | outcome | reason | nturns | hp_margin | arch | opp_peak_war | us_peak_war | opp_total_income | us_total_income | opp_hq_l2_turn | opp_hq_l3_turn | opp_hq_l4_turn | us_hq_l3_turn | opp_first_forward_t | opp_first_at_enemy_hq_t | opp_first_fwd5_t | us_hq_siege_taken | opp_hq_siege_taken | us_bases_lost | opp_avg_ms | opp_turn1_cmds |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 6 | RIGHT | L | HQ_DESTROYED | 110 | -10 | aggro_macro | 12 | 7 | 4800 | 3975 |  |  |  |  | 33 | 84 | 33 | 10 | 0 | 7 | 0.0 | MOVE A2 20;MOVE A3 40 |
| 7 | RIGHT | L | HQ_DESTROYED | 87 | -10 | aggro_macro | 10 | 6 | 3810 | 3660 |  |  |  |  | 29 | 85 | 29 | 10 | 0 | 8 | 0 | MOVE A2 9 |
| 14 | RIGHT | L | HQ_DESTROYED | 107 | -10 | macro | 10 | 7 | 4680 | 2985 |  |  |  |  | 33 | 61 | 33 | 10 | 0 | 4 | 0 | MOVE A2 10;MOVE A3 25 |
| 21 | RIGHT | W | TURN_LIMIT | 200 | 5 | macro | 9 | 9 | 8895 | 10830 |  |  |  |  | 30 | 61 | 30 | 0 | 0 | 10 | 0 | MOVE A2 2;MOVE A3 34 |
| 22 | RIGHT | L | HQ_DESTROYED | 96 | -10 | macro | 11 | 6 | 4215 | 4095 |  |  |  |  | 30 | 93 | 30 | 10 | 0 | 7 | 0 | MOVE A2 6;MOVE A3 29 |

**Opponent 994** - n=5, score 0.20 (1W/0D/4L), rounds 5,16,18,19,20, archetypes aggro_macro/macro

| round | us_side | outcome | reason | nturns | hp_margin | arch | opp_peak_war | us_peak_war | opp_total_income | us_total_income | opp_hq_l2_turn | opp_hq_l3_turn | opp_hq_l4_turn | us_hq_l3_turn | opp_first_forward_t | opp_first_at_enemy_hq_t | opp_first_fwd5_t | us_hq_siege_taken | opp_hq_siege_taken | us_bases_lost | opp_avg_ms | opp_turn1_cmds |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 5 | RIGHT | L | HQ_DESTROYED | 143 | -15 | aggro_macro | 43 | 15 | 17475 | 8610 | 60 |  |  |  | 76 | 140 | 76 | 15 | 0 | 5 | 0.4 | MOVE A2 3 |
| 16 | LEFT | L | HQ_DESTROYED | 104 | -10 | macro | 10 | 6 | 4575 | 2970 |  |  |  |  | 30 | 61 | 30 | 10 | 0 | 5 | 0.3 | MOVE B2 48;MOVE B3 35 |
| 18 | RIGHT | L | HQ_DESTROYED | 92 | -10 | macro | 10 | 6 | 4005 | 2595 |  |  |  |  | 31 | 65 | 31 | 10 | 0 | 4 | 0.8 | MOVE A2 2;MOVE A3 47 |
| 19 | RIGHT | L | HQ_DESTROYED | 84 | -10 | macro | 9 | 6 | 3645 | 2700 |  |  |  |  | 30 | 61 | 30 | 10 | 0 | 4 | 0.4 | MOVE A2 3;MOVE A3 10 |
| 20 | RIGHT | W | TURN_LIMIT | 200 | 5 | macro | 10 | 10 | 8895 | 10575 | 170 |  |  | 189 | 30 | 138 | 30 | 0 | 0 | 10 | 0.2 | MOVE A2 4;MOVE A3 30 |

**Opponent 129** - n=4, score 0.25 (1W/0D/3L), rounds 4,10,12,14, archetypes aggro_macro/early_aggro/macro

| round | us_side | outcome | reason | nturns | hp_margin | arch | opp_peak_war | us_peak_war | opp_total_income | us_total_income | opp_hq_l2_turn | opp_hq_l3_turn | opp_hq_l4_turn | us_hq_l3_turn | opp_first_forward_t | opp_first_at_enemy_hq_t | opp_first_fwd5_t | us_hq_siege_taken | opp_hq_siege_taken | us_bases_lost | opp_avg_ms | opp_turn1_cmds |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 4 | LEFT | W | HQ_DESTROYED | 195 | 30 | macro | 26 | 57 | 12570 | 47130 | 43 | 76 |  | 104 | 77 |  |  | 0 | 20 | 0 | 0 | MOVE B1 89;MOVE B2 81;TRAIN 1 |
| 10 | RIGHT | L | TURN_LIMIT | 200 | -10 | early_aggro | 11 | 6 | 11130 | 4170 | 85 | 118 | 170 |  | 22 | 120 | 41 | 0 | 0 | 4 | 0 | MOVE A3 2;MOVE A2 40 |
| 12 | RIGHT | L | HQ_DESTROYED | 196 | -20 | aggro_macro | 12 | 8 | 7605 | 4560 | 129 | 181 |  |  | 29 | 189 | 29 | 10 | 0 | 4 | 0.0 | MOVE A3 5;MOVE A2 25 |
| 14 | RIGHT | L | HQ_DESTROYED | 117 | -15 | aggro_macro | 16 | 6 | 5970 | 3090 | 80 |  |  |  | 28 | 62 | 28 | 10 | 0 | 5 | 0 | MOVE A3 7;MOVE A2 49 |

**Opponent 1167** - n=4, score 0.25 (1W/0D/3L), rounds 5,13,16,23, archetypes aggro_macro

| round | us_side | outcome | reason | nturns | hp_margin | arch | opp_peak_war | us_peak_war | opp_total_income | us_total_income | opp_hq_l2_turn | opp_hq_l3_turn | opp_hq_l4_turn | us_hq_l3_turn | opp_first_forward_t | opp_first_at_enemy_hq_t | opp_first_fwd5_t | us_hq_siege_taken | opp_hq_siege_taken | us_bases_lost | opp_avg_ms | opp_turn1_cmds |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 5 | LEFT | L | TURN_LIMIT | 200 | -10 | aggro_macro | 38 | 19 | 41310 | 16455 | 77 | 117 | 135 | 171 | 85 | 183 | 85 | 0 | 0 | 10 | 0.4 | MOVE B1 103;MOVE B2 90 |
| 13 | LEFT | L | HQ_DESTROYED | 185 | -20 | aggro_macro | 44 | 11 | 21735 | 4620 | 153 | 169 |  |  | 58 | 184 | 85 | 10 | 0 | 2 | 0.1 | MOVE B1 73;MOVE B2 56 |
| 16 | LEFT | W | TURN_LIMIT | 200 | 10 | aggro_macro | 28 | 27 | 22170 | 24165 | 81 |  |  | 166 | 62 |  | 74 | 0 | 0 | 15 | 0.4 | MOVE B2 92;MOVE B3 87 |
| 23 | LEFT | L | TURN_LIMIT | 200 | -5 | aggro_macro | 28 | 30 | 28200 | 32910 | 53 | 76 | 185 | 195 | 77 |  | 77 | 0 | 0 | 14 | 1.6 | MOVE B2 99;MOVE B3 85 |

**Opponent 1237** - n=6, score 0.25 (1W/1D/4L), rounds 7,16,22,24,25,26, archetypes aggro_macro/early_aggro/macro

| round | us_side | outcome | reason | nturns | hp_margin | arch | opp_peak_war | us_peak_war | opp_total_income | us_total_income | opp_hq_l2_turn | opp_hq_l3_turn | opp_hq_l4_turn | us_hq_l3_turn | opp_first_forward_t | opp_first_at_enemy_hq_t | opp_first_fwd5_t | us_hq_siege_taken | opp_hq_siege_taken | us_bases_lost | opp_avg_ms | opp_turn1_cmds |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 7 | LEFT | W | TURN_LIMIT | 200 | 15 | aggro_macro | 25 | 38 | 13635 | 25275 |  |  |  | 180 | 63 |  | 63 | 0 | 0 | 7 | 0 | MOVE B2 70 |
| 16 | RIGHT | L | TURN_LIMIT | 200 | -10 | aggro_macro | 25 | 14 | 27585 | 15390 | 136 | 152 | 170 | 176 | 46 | 196 | 63 | 0 | 0 | 9 | 0 | MOVE A2 1 |
| 22 | RIGHT | D | TURN_LIMIT | 200 | 0 | macro | 21 | 20 | 18210 | 20610 | 189 |  |  |  | 53 |  | 70 | 0 | 0 | 11 | 0.2 | MOVE A2 4 |
| 24 | RIGHT | L | HQ_DESTROYED | 183 | -25 | aggro_macro | 26 | 13 | 22800 | 9675 | 129 | 148 | 172 |  | 53 | 180 | 93 | 15 | 0 | 8 | 0 | MOVE A2 10 |
| 25 | RIGHT | L | TURN_LIMIT | 200 | -15 | aggro_macro | 26 | 23 | 26235 | 15660 | 122 | 174 | 189 |  | 51 | 177 | 65 | 0 | 0 | 7 | 0.7 | MOVE A2 7 |
| 26 | RIGHT | L | HQ_DESTROYED | 186 | -25 | early_aggro | 34 | 22 | 30345 | 13545 | 107 | 143 | 163 |  | 7 | 183 | 66 | 15 | 0 | 8 | 0.2 | MOVE A2 7 |

**Opponent 184** - n=5, score 0.30 (1W/1D/3L), rounds 5,6,9,15,20, archetypes aggro_macro/macro

| round | us_side | outcome | reason | nturns | hp_margin | arch | opp_peak_war | us_peak_war | opp_total_income | us_total_income | opp_hq_l2_turn | opp_hq_l3_turn | opp_hq_l4_turn | us_hq_l3_turn | opp_first_forward_t | opp_first_at_enemy_hq_t | opp_first_fwd5_t | us_hq_siege_taken | opp_hq_siege_taken | us_bases_lost | opp_avg_ms | opp_turn1_cmds |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 5 | LEFT | D | TURN_LIMIT | 200 | 0 | macro | 13 | 22 | 22395 | 22095 | 64 | 77 | 130 | 161 | 29 |  | 107 | 0 | 0 | 7 | 0.1 | MOVE B2 67;MOVE B3 69;TRAIN 1 |
| 6 | RIGHT | W | TURN_LIMIT | 200 | 15 | macro | 12 | 36 | 12525 | 30060 | 65 |  |  | 159 | 107 |  |  | 0 | 0 | 3 | 0.1 | MOVE A2 10;MOVE A3 18;TRAIN 1 |
| 9 | RIGHT | L | HQ_DESTROYED | 123 | -15 | macro | 12 | 9 | 9510 | 5535 | 95 |  |  |  | 54 | 98 | 87 | 10 | 0 | 7 | 0.1 | MOVE A2 4;MOVE A3 6;TRAIN 1 |
| 15 | RIGHT | L | HQ_DESTROYED | 121 | -10 | macro | 10 | 7 | 5340 | 3660 |  |  |  |  | 32 | 81 | 32 | 10 | 0 | 5 | 0.4 | MOVE A2 1;MOVE A3 10 |
| 20 | RIGHT | L | TURN_LIMIT | 200 | -5 | aggro_macro | 20 | 14 | 16530 | 13245 | 134 | 179 |  |  | 48 | 147 | 48 | 0 | 0 | 14 | 0.3 | MOVE A2 3;MOVE A3 13 |

**Opponent 997** - n=5, score 0.30 (1W/1D/3L), rounds 12,22,25,26,27, archetypes aggro_macro/macro

| round | us_side | outcome | reason | nturns | hp_margin | arch | opp_peak_war | us_peak_war | opp_total_income | us_total_income | opp_hq_l2_turn | opp_hq_l3_turn | opp_hq_l4_turn | us_hq_l3_turn | opp_first_forward_t | opp_first_at_enemy_hq_t | opp_first_fwd5_t | us_hq_siege_taken | opp_hq_siege_taken | us_bases_lost | opp_avg_ms | opp_turn1_cmds |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 12 | RIGHT | L | HQ_DESTROYED | 197 | -15 | aggro_macro | 19 | 8 | 8985 | 4620 | 170 |  |  |  | 30 | 194 | 30 | 10 | 0 | 4 | 0.1 | MOVE A1 6;MOVE A3 28 |
| 22 | LEFT | W | TURN_LIMIT | 200 | 15 | macro | 18 | 21 | 12525 | 24525 |  |  |  | 151 | 29 | 194 | 56 | 0 | 0 | 8 | 0.6 | MOVE B1 106;MOVE B3 54 |
| 25 | RIGHT | L | HQ_DESTROYED | 194 | -20 | aggro_macro | 23 | 8 | 14130 | 3435 | 167 | 179 |  |  | 45 | 194 | 74 | 10 | 0 | 2 | 0.1 | MOVE A1 5;MOVE A3 38 |
| 26 | LEFT | D | TURN_LIMIT | 200 | 0 | macro | 16 | 16 | 10275 | 15945 | 196 |  |  |  | 46 |  |  | 0 | 0 | 4 | 0.2 | MOVE B1 74;MOVE B3 44 |
| 27 | LEFT | L | HQ_DESTROYED | 194 | -10 | aggro_macro | 30 | 12 | 14880 | 8235 |  |  |  |  | 33 | 194 | 74 | 10 | 0 | 5 | 0.1 | MOVE B1 62;MOVE B3 36 |

**Opponent 865** - n=4, score 0.38 (1W/1D/2L), rounds 4,12,24,27, archetypes aggro_macro/turtle

| round | us_side | outcome | reason | nturns | hp_margin | arch | opp_peak_war | us_peak_war | opp_total_income | us_total_income | opp_hq_l2_turn | opp_hq_l3_turn | opp_hq_l4_turn | us_hq_l3_turn | opp_first_forward_t | opp_first_at_enemy_hq_t | opp_first_fwd5_t | us_hq_siege_taken | opp_hq_siege_taken | us_bases_lost | opp_avg_ms | opp_turn1_cmds |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 4 | RIGHT | L | HQ_DESTROYED | 181 | -15 | aggro_macro | 48 | 16 | 23520 | 11760 | 68 |  |  |  | 71 | 116 | 92 | 15 | 0 | 12 | 0.1 | MOVE A2 2;MOVE A3 3 |
| 12 | RIGHT | L | HQ_DESTROYED | 170 | -20 | aggro_macro | 43 | 15 | 23925 | 13080 | 47 | 144 |  |  | 26 | 159 | 128 | 15 | 0 | 8 | 0 | MOVE A2 6;MOVE A3 4 |
| 24 | RIGHT | D | TURN_LIMIT | 200 | 0 | aggro_macro | 56 | 26 | 30870 | 25560 | 75 | 95 |  | 181 | 35 |  | 114 | 0 | 0 | 9 | 0.0 | MOVE A2 6;MOVE A3 34 |
| 27 | LEFT | W | TURN_LIMIT | 200 | 5 | turtle | 22 | 26 | 20235 | 29640 | 72 | 139 | 186 | 145 |  |  |  | 0 | 0 | 2 | 0 | MOVE B2 68;MOVE B3 43 |

**Opponent 618** - n=4, score 0.50 (2W/0D/2L), rounds 3,9,11,18, archetypes aggro_macro/macro

| round | us_side | outcome | reason | nturns | hp_margin | arch | opp_peak_war | us_peak_war | opp_total_income | us_total_income | opp_hq_l2_turn | opp_hq_l3_turn | opp_hq_l4_turn | us_hq_l3_turn | opp_first_forward_t | opp_first_at_enemy_hq_t | opp_first_fwd5_t | us_hq_siege_taken | opp_hq_siege_taken | us_bases_lost | opp_avg_ms | opp_turn1_cmds |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 3 | RIGHT | L | HQ_DESTROYED | 136 | -15 | aggro_macro | 44 | 20 | 15735 | 9240 | 90 |  |  |  | 74 | 129 | 125 | 15 | 0 | 3 | 0.6 | MOVE A2 17;MOVE A3 19 |
| 9 | LEFT | W | TURN_LIMIT | 200 | 5 | aggro_macro | 39 | 31 | 25845 | 33615 | 58 | 127 |  | 165 | 66 | 184 | 180 | 0 | 0 | 1 | 0 | MOVE B2 42;MOVE B3 83 |
| 11 | LEFT | W | TURN_LIMIT | 200 | 5 | macro | 17 | 29 | 19065 | 35985 | 61 | 160 | 191 | 141 | 62 |  |  | 0 | 0 | 2 | 0.0 | MOVE B2 51;MOVE B3 101 |
| 18 | RIGHT | L | HQ_DESTROYED | 176 | -20 | aggro_macro | 48 | 20 | 29445 | 19755 | 51 | 103 |  | 87 | 65 | 170 | 87 | 20 | 0 | 16 | 0 | MOVE A2 3;MOVE A3 6 |

**Opponent 642** - n=4, score 0.50 (2W/0D/2L), rounds 11,15,20,26, archetypes aggro_macro/macro

| round | us_side | outcome | reason | nturns | hp_margin | arch | opp_peak_war | us_peak_war | opp_total_income | us_total_income | opp_hq_l2_turn | opp_hq_l3_turn | opp_hq_l4_turn | us_hq_l3_turn | opp_first_forward_t | opp_first_at_enemy_hq_t | opp_first_fwd5_t | us_hq_siege_taken | opp_hq_siege_taken | us_bases_lost | opp_avg_ms | opp_turn1_cmds |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 11 | RIGHT | L | HQ_DESTROYED | 199 | -15 | aggro_macro | 36 | 19 | 27450 | 14550 | 58 |  |  |  | 75 | 144 | 84 | 15 | 0 | 11 | 0.1 | MOVE A2 3;MOVE A3 52 |
| 15 | RIGHT | L | HQ_DESTROYED | 104 | -10 | macro | 16 | 10 | 7605 | 5115 |  |  |  |  | 40 | 96 | 59 | 10 | 0 | 6 | 0 | MOVE A2 12;MOVE A3 25 |
| 20 | LEFT | W | TURN_LIMIT | 200 | 15 | macro | 11 | 14 | 8895 | 20430 |  |  |  | 148 | 33 |  | 33 | 0 | 0 | 11 | 0 | MOVE B2 93;MOVE B3 82 |
| 26 | LEFT | W | TURN_LIMIT | 200 | 20 | macro | 22 | 29 | 15870 | 31515 |  |  |  | 140 | 80 |  |  | 0 | 0 | 2 | 0 | MOVE B2 104;MOVE B3 53 |

**Opponent 696** - n=4, score 0.50 (2W/0D/2L), rounds 9,11,26,28, archetypes aggro_macro/macro/turtle

| round | us_side | outcome | reason | nturns | hp_margin | arch | opp_peak_war | us_peak_war | opp_total_income | us_total_income | opp_hq_l2_turn | opp_hq_l3_turn | opp_hq_l4_turn | us_hq_l3_turn | opp_first_forward_t | opp_first_at_enemy_hq_t | opp_first_fwd5_t | us_hq_siege_taken | opp_hq_siege_taken | us_bases_lost | opp_avg_ms | opp_turn1_cmds |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 9 | LEFT | W | TURN_LIMIT | 200 | 20 | turtle | 20 | 39 | 12405 | 31200 |  |  |  | 167 |  |  |  | 0 | 0 | 2 | 0 | MOVE B2 76;MOVE B3 71 |
| 11 | LEFT | W | TURN_LIMIT | 200 | 10 | aggro_macro | 28 | 22 | 16560 | 21270 | 194 |  |  | 146 | 66 | 187 | 72 | 0 | 0 | 7 | 0.1 | MOVE B2 53;MOVE B3 57 |
| 26 | LEFT | L | TURN_LIMIT | 200 | -15 | aggro_macro | 27 | 24 | 27765 | 17490 | 49 | 156 | 197 |  | 69 | 199 | 69 | 0 | 0 | 12 | 0.1 | MOVE B2 96;MOVE B3 97 |
| 28 | LEFT | L | HQ_DESTROYED | 138 | -15 | macro | 18 | 17 | 11640 | 7830 | 46 |  |  |  | 65 | 95 | 67 | 15 | 0 | 5 | 0.3 | MOVE B2 52;MOVE B3 61 |

**Opponent 762** - n=4, score 0.50 (2W/0D/2L), rounds 9,11,12,19, archetypes aggro_macro/macro

| round | us_side | outcome | reason | nturns | hp_margin | arch | opp_peak_war | us_peak_war | opp_total_income | us_total_income | opp_hq_l2_turn | opp_hq_l3_turn | opp_hq_l4_turn | us_hq_l3_turn | opp_first_forward_t | opp_first_at_enemy_hq_t | opp_first_fwd5_t | us_hq_siege_taken | opp_hq_siege_taken | us_bases_lost | opp_avg_ms | opp_turn1_cmds |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 9 | RIGHT | L | HQ_DESTROYED | 148 | -25 | aggro_macro | 31 | 13 | 19410 | 7995 | 71 | 102 | 138 |  | 44 | 140 | 55 | 15 | 0 | 8 | 0 | MOVE A2 22 |
| 11 | RIGHT | W | TURN_LIMIT | 200 | 5 | macro | 20 | 26 | 18495 | 27210 | 76 | 108 |  | 147 | 60 |  | 160 | 0 | 0 | 11 | 0.4 | MOVE A2 3 |
| 12 | LEFT | W | TURN_LIMIT | 200 | 5 | macro | 21 | 23 | 18450 | 24285 | 69 | 187 |  | 146 | 54 |  | 60 | 0 | 0 | 17 | 0 | MOVE B2 91 |
| 19 | RIGHT | L | TURN_LIMIT | 200 | -5 | aggro_macro | 22 | 17 | 19680 | 16035 | 75 | 105 |  |  | 52 | 177 | 67 | 0 | 0 | 15 | 0.0 | MOVE A2 7 |

**Opponent 518** - n=5, score 0.50 (2W/1D/2L), rounds 16,19,21,23,27, archetypes macro

| round | us_side | outcome | reason | nturns | hp_margin | arch | opp_peak_war | us_peak_war | opp_total_income | us_total_income | opp_hq_l2_turn | opp_hq_l3_turn | opp_hq_l4_turn | us_hq_l3_turn | opp_first_forward_t | opp_first_at_enemy_hq_t | opp_first_fwd5_t | us_hq_siege_taken | opp_hq_siege_taken | us_bases_lost | opp_avg_ms | opp_turn1_cmds |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 16 | LEFT | L | HQ_DESTROYED | 83 | -10 | macro | 9 | 6 | 3600 | 2190 |  |  |  |  | 31 | 48 | 31 | 10 | 0 | 3 | 0 | MOVE B1 50;MOVE B2 28 |
| 19 | RIGHT | W | TURN_LIMIT | 200 | 10 | macro | 15 | 9 | 11625 | 9885 |  |  |  | 197 | 40 | 52 | 40 | 0 | 0 | 9 | 0 | MOVE A1 3;MOVE A2 42 |
| 21 | LEFT | W | TURN_LIMIT | 200 | 20 | macro | 14 | 23 | 8910 | 26580 |  |  |  | 146 | 40 |  | 40 | 0 | 0 | 6 | 0 | MOVE B1 100;MOVE B2 52 |
| 23 | LEFT | L | HQ_DESTROYED | 122 | -10 | macro | 14 | 10 | 6885 | 5850 |  |  |  |  | 40 | 87 | 40 | 10 | 0 | 9 | 0 | MOVE B1 78;MOVE B2 40 |
| 27 | LEFT | D | TURN_LIMIT | 200 | 0 | macro | 12 | 9 | 5820 | 5220 |  |  |  |  | 45 |  | 45 | 0 | 0 | 4 | 0.0 | MOVE B1 99;MOVE B2 50 |

**Opponent 773** - n=5, score 0.50 (2W/1D/2L), rounds 5,7,14,16,29, archetypes aggro_macro/macro

| round | us_side | outcome | reason | nturns | hp_margin | arch | opp_peak_war | us_peak_war | opp_total_income | us_total_income | opp_hq_l2_turn | opp_hq_l3_turn | opp_hq_l4_turn | us_hq_l3_turn | opp_first_forward_t | opp_first_at_enemy_hq_t | opp_first_fwd5_t | us_hq_siege_taken | opp_hq_siege_taken | us_bases_lost | opp_avg_ms | opp_turn1_cmds |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 5 | LEFT | L | HQ_DESTROYED | 197 | -25 | aggro_macro | 27 | 6 | 13995 | 4995 | 60 | 127 | 166 |  | 26 | 191 | 188 | 10 | 0 | 6 | 0 | MOVE B3 55;MOVE B2 42 |
| 7 | LEFT | W | TURN_LIMIT | 200 | 5 | macro | 15 | 49 | 15165 | 30270 | 77 | 134 |  | 160 | 32 |  | 32 | 0 | 0 | 5 | 0 | MOVE B3 70;MOVE B2 67 |
| 14 | RIGHT | D | TURN_LIMIT | 200 | 0 | aggro_macro | 68 | 18 | 37140 | 12300 | 89 |  |  |  | 31 | 197 | 31 | 0 | 0 | 13 | 0.0 | MOVE A3 4;MOVE A2 16 |
| 16 | LEFT | W | TURN_LIMIT | 200 | 5 | macro | 40 | 26 | 24450 | 25185 | 78 | 153 | 179 | 141 | 31 |  | 31 | 0 | 0 | 11 | 0 | MOVE B3 85;MOVE B2 69 |
| 29 | LEFT | L | HQ_DESTROYED | 194 | -15 | aggro_macro | 32 | 16 | 16485 | 10560 | 132 |  |  |  | 84 | 191 | 84 | 10 | 0 | 7 | 0 | MOVE B2 33;MOVE B3 62 |

**Opponent 916** - n=4, score 0.62 (1W/3D/0L), rounds 3,22,24,26, archetypes aggro_macro/macro

| round | us_side | outcome | reason | nturns | hp_margin | arch | opp_peak_war | us_peak_war | opp_total_income | us_total_income | opp_hq_l2_turn | opp_hq_l3_turn | opp_hq_l4_turn | us_hq_l3_turn | opp_first_forward_t | opp_first_at_enemy_hq_t | opp_first_fwd5_t | us_hq_siege_taken | opp_hq_siege_taken | us_bases_lost | opp_avg_ms | opp_turn1_cmds |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 3 | RIGHT | D | TURN_LIMIT | 200 | 0 | macro | 20 | 34 | 28005 | 29010 | 71 | 85 | 162 | 125 | 72 |  |  | 0 | 0 | 0 | 0.3 | MOVE A2 1;MOVE A3 8 |
| 22 | RIGHT | W | TURN_LIMIT | 200 | 5 | macro | 17 | 20 | 15195 | 22725 | 61 | 189 |  | 164 | 58 |  |  | 0 | 0 | 4 | 1.3 | MOVE A2 6;MOVE A3 8 |
| 24 | RIGHT | D | TURN_LIMIT | 200 | 0 | macro | 15 | 17 | 13950 | 18825 | 67 | 184 |  | 176 | 79 |  | 79 | 0 | 0 | 3 | 1.3 | MOVE A2 10;MOVE A3 8 |
| 26 | LEFT | D | TURN_LIMIT | 200 | 0 | aggro_macro | 24 | 23 | 19575 | 24375 | 68 | 165 |  | 190 | 58 |  | 63 | 0 | 0 | 11 | 2.3 | MOVE B2 81;MOVE B3 77 |

**Opponent 1227** - n=4, score 0.62 (2W/1D/1L), rounds 8,15,16,25, archetypes aggro_macro/macro

| round | us_side | outcome | reason | nturns | hp_margin | arch | opp_peak_war | us_peak_war | opp_total_income | us_total_income | opp_hq_l2_turn | opp_hq_l3_turn | opp_hq_l4_turn | us_hq_l3_turn | opp_first_forward_t | opp_first_at_enemy_hq_t | opp_first_fwd5_t | us_hq_siege_taken | opp_hq_siege_taken | us_bases_lost | opp_avg_ms | opp_turn1_cmds |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 8 | LEFT | D | TURN_LIMIT | 200 | 0 | macro | 17 | 17 | 26265 | 20190 | 86 | 135 | 182 | 172 | 51 |  |  | 0 | 0 | 7 | 4.7 | MOVE B2 71;MOVE B3 64 |
| 15 | RIGHT | L | TURN_LIMIT | 200 | -23 | aggro_macro | 18 | 8 | 17955 | 3435 | 131 | 141 | 162 |  | 39 | 96 | 39 | 3 | 0 | 5 | 0 | MOVE A2 5;MOVE A3 9 |
| 16 | RIGHT | W | TURN_LIMIT | 200 | 10 | macro | 20 | 22 | 21690 | 25815 | 61 | 150 |  | 150 | 84 |  | 85 | 0 | 0 | 11 | 13.6 | MOVE A2 13;MOVE A3 3 |
| 25 | LEFT | W | TURN_LIMIT | 200 | 10 | macro | 23 | 37 | 20475 | 31755 | 106 | 122 |  | 144 | 76 |  | 86 | 0 | 0 | 3 | 15.6 | MOVE B2 92;MOVE B3 81 |

**Opponent 256** - n=6, score 0.67 (3W/2D/1L), rounds 12,15,21,22,28,30, archetypes aggro_macro/early_aggro/macro

| round | us_side | outcome | reason | nturns | hp_margin | arch | opp_peak_war | us_peak_war | opp_total_income | us_total_income | opp_hq_l2_turn | opp_hq_l3_turn | opp_hq_l4_turn | us_hq_l3_turn | opp_first_forward_t | opp_first_at_enemy_hq_t | opp_first_fwd5_t | us_hq_siege_taken | opp_hq_siege_taken | us_bases_lost | opp_avg_ms | opp_turn1_cmds |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 12 | LEFT | L | HQ_DESTROYED | 190 | -15 | early_aggro | 35 | 13 | 20220 | 11085 | 167 |  |  |  | 15 | 190 | 68 | 10 | 0 | 12 | 0 | MOVE B3 67;MOVE B2 34 |
| 15 | RIGHT | W | TURN_LIMIT | 200 | 5 | early_aggro | 39 | 28 | 29475 | 29715 | 74 | 157 | 185 | 156 | 14 | 192 | 91 | 0 | 0 | 11 | 0 | MOVE A3 4;MOVE A2 53 |
| 21 | LEFT | D | TURN_LIMIT | 200 | 0 | aggro_macro | 26 | 21 | 18690 | 18975 | 56 | 189 |  | 170 | 77 | 192 | 77 | 0 | 0 | 7 | 0 | MOVE B3 66;MOVE B2 34 |
| 22 | RIGHT | W | TURN_LIMIT | 200 | 5 | aggro_macro | 24 | 25 | 20670 | 24030 | 62 |  |  | 198 | 70 |  | 78 | 0 | 0 | 14 | 0.0 | MOVE A3 3;MOVE A2 43 |
| 28 | LEFT | W | TURN_LIMIT | 200 | 5 | macro | 22 | 27 | 22455 | 31245 | 74 | 192 |  | 178 | 68 |  | 87 | 0 | 0 | 9 | 0.4 | MOVE B3 103;MOVE B2 54 |
| 30 | LEFT | D | TURN_LIMIT | 200 | 0 | aggro_macro | 32 | 23 | 21555 | 18900 | 64 |  |  |  | 34 |  | 75 | 0 | 0 | 7 | 0.2 | MOVE B3 81;MOVE B2 47 |

**Opponent 25** - n=8, score 0.69 (5W/1D/2L), rounds 7,16,18,19,22,27,28,30, archetypes macro/turtle

| round | us_side | outcome | reason | nturns | hp_margin | arch | opp_peak_war | us_peak_war | opp_total_income | us_total_income | opp_hq_l2_turn | opp_hq_l3_turn | opp_hq_l4_turn | us_hq_l3_turn | opp_first_forward_t | opp_first_at_enemy_hq_t | opp_first_fwd5_t | us_hq_siege_taken | opp_hq_siege_taken | us_bases_lost | opp_avg_ms | opp_turn1_cmds |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 7 | LEFT | W | TURN_LIMIT | 200 | 5 | turtle | 28 | 80 | 17595 | 38040 | 58 | 87 |  | 130 |  |  |  | 0 | 0 | 0 | 0 | MOVE B2 82 |
| 16 | LEFT | W | TURN_LIMIT | 200 | 10 | macro | 19 | 20 | 15810 | 22290 | 47 |  |  | 166 | 36 | 192 | 194 | 0 | 0 | 4 | 0 | MOVE B2 67 |
| 18 | RIGHT | W | TURN_LIMIT | 200 | 15 | turtle | 16 | 20 | 10860 | 25290 | 38 |  |  | 130 |  |  |  | 0 | 0 | 1 | 0 | MOVE A2 3 |
| 19 | LEFT | W | TURN_LIMIT | 200 | 10 | macro | 23 | 43 | 17760 | 37260 | 51 | 67 |  | 98 | 100 |  | 103 | 0 | 0 | 2 | 0.0 | MOVE B2 101 |
| 22 | RIGHT | W | TURN_LIMIT | 200 | 5 | macro | 19 | 23 | 20400 | 27600 | 81 | 162 |  | 168 | 47 | 172 | 48 | 0 | 0 | 8 | 0 | MOVE A2 4;MOVE A3 45 |
| 27 | LEFT | D | TURN_LIMIT | 200 | 0 | turtle | 17 | 15 | 11175 | 13590 | 116 |  |  |  |  |  |  | 0 | 0 | 0 | 0 | MOVE B2 69;MOVE B3 36 |
| 28 | LEFT | L | TURN_LIMIT | 200 | -10 | turtle | 21 | 16 | 17910 | 14235 | 103 | 151 | 199 |  | 172 | 176 | 172 | 0 | 0 | 1 | 0 | MOVE B2 72;MOVE B3 37 |
| 30 | RIGHT | L | TURN_LIMIT | 200 | -15 | turtle | 17 | 13 | 14910 | 10125 | 97 | 151 | 195 |  |  |  |  | 0 | 0 | 0 | 0 | MOVE A2 9;MOVE A3 45 |

**Opponent 453** - n=4, score 0.75 (3W/0D/1L), rounds 3,9,21,29, archetypes aggro_macro/macro/turtle

| round | us_side | outcome | reason | nturns | hp_margin | arch | opp_peak_war | us_peak_war | opp_total_income | us_total_income | opp_hq_l2_turn | opp_hq_l3_turn | opp_hq_l4_turn | us_hq_l3_turn | opp_first_forward_t | opp_first_at_enemy_hq_t | opp_first_fwd5_t | us_hq_siege_taken | opp_hq_siege_taken | us_bases_lost | opp_avg_ms | opp_turn1_cmds |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 3 | RIGHT | W | TURN_LIMIT | 200 | 20 | turtle | 11 | 23 | 6015 | 20775 |  |  |  | 173 |  |  |  | 0 | 5 | 0 | 0 | MOVE A1 3;MOVE A2 14;TRAIN 1 |
| 9 | LEFT | W | HQ_DESTROYED | 196 | 20 | turtle | 7 | 59 | 3255 | 24330 |  |  |  | 180 |  |  |  | 0 | 10 | 1 | 0 | MOVE B1 42;MOVE B2 43 |
| 21 | RIGHT | L | TURN_LIMIT | 200 | -10 | aggro_macro | 22 | 21 | 20895 | 12795 | 63 | 125 | 170 |  | 37 |  | 49 | 0 | 0 | 3 | 0 | MOVE A1 2;MOVE A2 29;TRAIN 1 |
| 29 | LEFT | W | TURN_LIMIT | 200 | 5 | macro | 14 | 17 | 11085 | 14370 | 82 |  |  | 180 | 62 | 197 | 79 | 0 | 0 | 3 | 0 | MOVE B1 64;MOVE B2 35;TRAIN 1 |

**Opponent 657** - n=4, score 0.75 (3W/0D/1L), rounds 6,14,22,26, archetypes aggro_macro/macro/turtle

| round | us_side | outcome | reason | nturns | hp_margin | arch | opp_peak_war | us_peak_war | opp_total_income | us_total_income | opp_hq_l2_turn | opp_hq_l3_turn | opp_hq_l4_turn | us_hq_l3_turn | opp_first_forward_t | opp_first_at_enemy_hq_t | opp_first_fwd5_t | us_hq_siege_taken | opp_hq_siege_taken | us_bases_lost | opp_avg_ms | opp_turn1_cmds |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 6 | RIGHT | L | HQ_DESTROYED | 124 | -10 | aggro_macro | 11 | 6 | 5475 | 4275 |  |  |  |  | 30 | 79 | 30 | 10 | 0 | 7 | 0.0 | MOVE A2 4;MOVE A3 1 |
| 14 | RIGHT | W | TURN_LIMIT | 200 | 10 | turtle | 61 | 54 | 30990 | 39420 | 65 | 82 |  | 129 | 178 | 184 | 178 | 0 | 0 | 6 | 0.1 | MOVE A2 6;MOVE A3 5 |
| 22 | RIGHT | W | TURN_LIMIT | 200 | 10 | macro | 25 | 19 | 18270 | 24420 | 195 |  |  | 176 | 52 | 165 | 62 | 0 | 0 | 9 | 0.1 | MOVE A2 28;MOVE A3 40 |
| 26 | LEFT | W | TURN_LIMIT | 200 | 5 | turtle | 16 | 20 | 11400 | 16665 | 94 |  |  | 173 | 160 | 163 |  | 0 | 0 | 1 | 0.0 | MOVE B2 48;MOVE B3 29 |

**Opponent 978** - n=4, score 0.75 (3W/0D/1L), rounds 4,8,13,20, archetypes aggro_macro/hq_rush/macro

| round | us_side | outcome | reason | nturns | hp_margin | arch | opp_peak_war | us_peak_war | opp_total_income | us_total_income | opp_hq_l2_turn | opp_hq_l3_turn | opp_hq_l4_turn | us_hq_l3_turn | opp_first_forward_t | opp_first_at_enemy_hq_t | opp_first_fwd5_t | us_hq_siege_taken | opp_hq_siege_taken | us_bases_lost | opp_avg_ms | opp_turn1_cmds |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 4 | RIGHT | W | HQ_DESTROYED | 144 | 20 | hq_rush | 7 | 32 | 2835 | 15105 |  |  |  | 133 | 12 | 16 | 12 | 0 | 10 | 0 | 0.1 | nan |
| 8 | RIGHT | L | TURN_LIMIT | 200 | -5 | aggro_macro | 31 | 18 | 29625 | 20685 | 147 | 153 | 168 | 87 | 83 |  | 92 | 0 | 0 | 7 | 0 | nan |
| 13 | LEFT | W | TURN_LIMIT | 200 | 15 | macro | 15 | 16 | 9690 | 20700 | 188 |  |  | 147 | 33 |  | 71 | 0 | 0 | 0 | 0.0 | MOVE B1 26 |
| 20 | LEFT | W | TURN_LIMIT | 200 | 5 | hq_rush | 23 | 25 | 14940 | 23790 | 150 | 160 | 182 | 152 | 14 | 18 | 14 | 0 | 0 | 1 | 0 | nan |

**Opponent 1154** - n=6, score 0.75 (4W/1D/1L), rounds 12,15,17,18,21,22, archetypes aggro_macro/early_aggro/macro/turtle

| round | us_side | outcome | reason | nturns | hp_margin | arch | opp_peak_war | us_peak_war | opp_total_income | us_total_income | opp_hq_l2_turn | opp_hq_l3_turn | opp_hq_l4_turn | us_hq_l3_turn | opp_first_forward_t | opp_first_at_enemy_hq_t | opp_first_fwd5_t | us_hq_siege_taken | opp_hq_siege_taken | us_bases_lost | opp_avg_ms | opp_turn1_cmds |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 12 | LEFT | W | TURN_LIMIT | 200 | 20 | aggro_macro | 29 | 36 | 22515 | 30675 |  |  |  | 141 | 58 |  | 113 | 0 | 0 | 14 | 2.3 | MOVE B2 85;MOVE B3 92 |
| 15 | RIGHT | L | HQ_DESTROYED | 163 | -10 | early_aggro | 9 | 7 | 4110 | 4545 |  |  |  |  | 19 | 118 | 45 | 10 | 0 | 7 | 0.3 | MOVE A2 44;TRAIN 1 |
| 17 | LEFT | D | TURN_LIMIT | 200 | 0 | aggro_macro | 27 | 20 | 23715 | 18810 | 102 | 165 | 200 | 104 | 64 | 109 | 78 | 0 | 0 | 7 | 2.8 | MOVE B2 87 |
| 18 | RIGHT | W | TURN_LIMIT | 200 | 5 | turtle | 21 | 39 | 17355 | 43200 | 46 | 67 | 101 | 93 | 179 |  |  | 0 | 0 | 4 | 2.6 | MOVE A2 8 |
| 21 | LEFT | W | TURN_LIMIT | 200 | 5 | macro | 28 | 22 | 21765 | 21510 | 136 | 178 |  | 157 | 91 |  | 91 | 0 | 0 | 12 | 3.0 | MOVE B2 88 |
| 22 | LEFT | W | TURN_LIMIT | 200 | 10 | macro | 23 | 25 | 16905 | 26115 | 93 |  |  | 176 | 103 |  | 199 | 0 | 0 | 3 | 2.0 | MOVE B2 72 |

**Opponent 36** - n=5, score 0.80 (3W/2D/0L), rounds 9,10,20,25,29, archetypes aggro_macro/macro/passive

| round | us_side | outcome | reason | nturns | hp_margin | arch | opp_peak_war | us_peak_war | opp_total_income | us_total_income | opp_hq_l2_turn | opp_hq_l3_turn | opp_hq_l4_turn | us_hq_l3_turn | opp_first_forward_t | opp_first_at_enemy_hq_t | opp_first_fwd5_t | us_hq_siege_taken | opp_hq_siege_taken | us_bases_lost | opp_avg_ms | opp_turn1_cmds |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 9 | LEFT | W | HQ_DESTROYED | 196 | 30 | macro | 12 | 57 | 11070 | 37215 | 47 |  |  | 162 | 58 |  |  | 0 | 15 | 3 | 0 | MOVE B1 78;MOVE B2 58 |
| 10 | LEFT | D | TURN_LIMIT | 200 | 0 | aggro_macro | 45 | 88 | 30270 | 48570 | 47 | 93 | 139 | 96 | 87 | 192 | 90 | 0 | 0 | 4 | 0 | MOVE B1 105;MOVE B2 91 |
| 20 | LEFT | W | TURN_LIMIT | 200 | 10 | aggro_macro | 48 | 24 | 32310 | 27870 | 52 |  |  | 99 | 79 |  | 95 | 0 | 0 | 11 | 0.1 | MOVE B1 95;MOVE B2 91 |
| 25 | LEFT | D | TURN_LIMIT | 200 | 0 | macro | 9 | 10 | 6660 | 6330 |  |  |  |  | 32 |  | 32 | 0 | 0 | 5 | 0 | MOVE B1 59;MOVE B2 51 |
| 29 | LEFT | W | TURN_LIMIT | 200 | 5 | passive | 10 | 9 | 3135 | 5760 |  |  |  |  | 32 |  | 32 | 0 | 0 | 3 | 0 | MOVE B1 79;MOVE B2 72 |

**Opponent 290** - n=5, score 0.80 (4W/0D/1L), rounds 6,12,20,23,25, archetypes macro/turtle

| round | us_side | outcome | reason | nturns | hp_margin | arch | opp_peak_war | us_peak_war | opp_total_income | us_total_income | opp_hq_l2_turn | opp_hq_l3_turn | opp_hq_l4_turn | us_hq_l3_turn | opp_first_forward_t | opp_first_at_enemy_hq_t | opp_first_fwd5_t | us_hq_siege_taken | opp_hq_siege_taken | us_bases_lost | opp_avg_ms | opp_turn1_cmds |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 6 | LEFT | W | HQ_DESTROYED | 175 | 30 | turtle | 24 | 52 | 12060 | 39645 | 99 |  |  | 99 |  |  |  | 0 | 15 | 3 | 9.9 | MOVE B2 102;MOVE B3 81 |
| 12 | LEFT | W | TURN_LIMIT | 200 | 20 | macro | 13 | 20 | 9390 | 25800 |  |  |  | 143 | 59 | 65 |  | 0 | 0 | 2 | 0 | MOVE B2 54;MOVE B3 53;TRAIN 1 |
| 20 | LEFT | L | HQ_DESTROYED | 182 | -10 | macro | 9 | 8 | 8085 | 5790 |  |  |  |  | 30 | 130 | 30 | 10 | 0 | 6 | 0 | MOVE B2 72;MOVE B3 50 |
| 23 | LEFT | W | TURN_LIMIT | 200 | 20 | macro | 9 | 25 | 6480 | 30930 |  |  |  | 142 | 31 |  | 32 | 0 | 0 | 4 | 0 | MOVE B2 69;MOVE B3 62 |
| 25 | LEFT | W | TURN_LIMIT | 200 | 17 | turtle | 8 | 24 | 6300 | 26370 | 92 |  |  | 126 |  |  |  | 0 | 2 | 0 | 0 | MOVE B2 65;MOVE B3 59 |

**Opponent 894** - n=5, score 0.80 (3W/2D/0L), rounds 3,6,12,19,23, archetypes aggro_macro/macro

| round | us_side | outcome | reason | nturns | hp_margin | arch | opp_peak_war | us_peak_war | opp_total_income | us_total_income | opp_hq_l2_turn | opp_hq_l3_turn | opp_hq_l4_turn | us_hq_l3_turn | opp_first_forward_t | opp_first_at_enemy_hq_t | opp_first_fwd5_t | us_hq_siege_taken | opp_hq_siege_taken | us_bases_lost | opp_avg_ms | opp_turn1_cmds |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 3 | RIGHT | D | TURN_LIMIT | 200 | 0 | aggro_macro | 32 | 19 | 18015 | 13380 | 26 |  |  |  | 90 | 126 | 108 | 0 | 0 | 1 | 0 | MOVE A2 4;MOVE A3 6 |
| 6 | LEFT | W | TURN_LIMIT | 200 | 10 | macro | 20 | 26 | 22260 | 25635 | 75 |  |  | 158 | 44 |  | 199 | 0 | 0 | 12 | 0 | MOVE B2 77;MOVE B3 66 |
| 12 | LEFT | W | HQ_DESTROYED | 144 | 15 | macro | 8 | 18 | 7875 | 12855 |  |  |  |  | 39 |  |  | 0 | 10 | 3 | 0 | MOVE B2 55;MOVE B3 56 |
| 19 | LEFT | D | TURN_LIMIT | 200 | 0 | aggro_macro | 28 | 25 | 23205 | 21120 | 59 | 171 | 193 | 163 | 92 |  | 121 | 0 | 0 | 4 | 0 | MOVE B2 67 |
| 23 | LEFT | W | TURN_LIMIT | 200 | 5 | aggro_macro | 24 | 19 | 16560 | 21420 | 47 |  |  | 195 | 41 | 182 | 108 | 0 | 0 | 5 | 0 | MOVE B2 64;MOVE B3 65 |

**Opponent 1010** - n=5, score 0.80 (4W/0D/1L), rounds 14,17,19,21,26, archetypes aggro_macro/macro/turtle

| round | us_side | outcome | reason | nturns | hp_margin | arch | opp_peak_war | us_peak_war | opp_total_income | us_total_income | opp_hq_l2_turn | opp_hq_l3_turn | opp_hq_l4_turn | us_hq_l3_turn | opp_first_forward_t | opp_first_at_enemy_hq_t | opp_first_fwd5_t | us_hq_siege_taken | opp_hq_siege_taken | us_bases_lost | opp_avg_ms | opp_turn1_cmds |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 14 | LEFT | W | TURN_LIMIT | 200 | 5 | macro | 9 | 19 | 13335 | 26625 | 47 | 146 | 186 | 127 | 27 |  |  | 0 | 0 | 1 | 0 | MOVE B2 49;MOVE B3 25 |
| 17 | LEFT | L | TURN_LIMIT | 200 | -15 | aggro_macro | 58 | 28 | 36645 | 15120 | 52 | 146 | 158 |  | 105 |  | 118 | 0 | 0 | 1 | 0 | MOVE B2 65;MOVE B3 33 |
| 19 | RIGHT | W | TURN_LIMIT | 200 | 10 | turtle | 25 | 34 | 21810 | 29070 | 58 | 182 |  | 150 | 161 |  | 161 | 0 | 0 | 12 | 0 | MOVE A2 3;MOVE A3 37 |
| 21 | LEFT | W | TURN_LIMIT | 200 | 10 | macro | 16 | 17 | 14745 | 19950 | 69 | 154 |  | 138 | 32 |  | 32 | 0 | 0 | 7 | 0 | MOVE B2 49;MOVE B3 26 |
| 26 | LEFT | W | TURN_LIMIT | 200 | 15 | aggro_macro | 32 | 34 | 15000 | 39060 | 56 |  |  | 140 | 71 |  | 71 | 0 | 0 | 7 | 0 | MOVE B2 81 |

**Opponent 345** - n=4, score 0.88 (3W/1D/0L), rounds 3,4,5,23, archetypes aggro_macro/macro/passive

| round | us_side | outcome | reason | nturns | hp_margin | arch | opp_peak_war | us_peak_war | opp_total_income | us_total_income | opp_hq_l2_turn | opp_hq_l3_turn | opp_hq_l4_turn | us_hq_l3_turn | opp_first_forward_t | opp_first_at_enemy_hq_t | opp_first_fwd5_t | us_hq_siege_taken | opp_hq_siege_taken | us_bases_lost | opp_avg_ms | opp_turn1_cmds |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 3 | RIGHT | D | TURN_LIMIT | 200 | 0 | macro | 28 | 29 | 22185 | 24555 | 89 | 104 | 123 | 151 | 85 |  |  | 0 | 0 | 3 | 0.0 | MOVE A1 9;MOVE A2 12 |
| 4 | RIGHT | W | HQ_DESTROYED | 197 | 30 | passive | 13 | 50 | 12150 | 31380 | 57 | 77 |  | 154 | 37 |  |  | 0 | 20 | 0 | 0 | MOVE A1 1;MOVE A2 16 |
| 5 | LEFT | W | HQ_DESTROYED | 197 | 30 | macro | 13 | 68 | 17505 | 34680 | 64 | 75 | 121 | 109 | 63 |  |  | 0 | 25 | 0 | 0 | MOVE B1 88;MOVE B2 87 |
| 23 | LEFT | W | TURN_LIMIT | 200 | 15 | aggro_macro | 32 | 118 | 20490 | 53580 | 58 |  |  | 119 | 56 | 112 | 107 | 0 | 0 | 0 | 16.1 | MOVE B1 48;MOVE B3 94 |

**Opponent 967** - n=5, score 0.90 (4W/1D/0L), rounds 19,21,24,26,30, archetypes macro

| round | us_side | outcome | reason | nturns | hp_margin | arch | opp_peak_war | us_peak_war | opp_total_income | us_total_income | opp_hq_l2_turn | opp_hq_l3_turn | opp_hq_l4_turn | us_hq_l3_turn | opp_first_forward_t | opp_first_at_enemy_hq_t | opp_first_fwd5_t | us_hq_siege_taken | opp_hq_siege_taken | us_bases_lost | opp_avg_ms | opp_turn1_cmds |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 19 | RIGHT | W | TURN_LIMIT | 200 | 10 | macro | 17 | 26 | 12735 | 21090 | 75 |  |  | 148 | 41 |  |  | 0 | 0 | 1 | 0.0 | MOVE A2 1;MOVE A3 3 |
| 21 | RIGHT | D | TURN_LIMIT | 200 | 0 | macro | 25 | 28 | 21495 | 28800 | 74 | 143 | 185 | 162 | 55 |  |  | 0 | 0 | 0 | 0.1 | MOVE A2 7;MOVE A3 14 |
| 24 | RIGHT | W | TURN_LIMIT | 200 | 5 | macro | 21 | 28 | 18705 | 26685 | 54 | 114 | 190 | 151 | 51 |  |  | 0 | 0 | 2 | 0.1 | MOVE A2 3;MOVE A3 8 |
| 26 | LEFT | W | TURN_LIMIT | 200 | 5 | macro | 19 | 22 | 17700 | 26520 | 54 | 197 |  | 170 | 43 |  |  | 0 | 0 | 1 | 0.0 | MOVE B2 65;MOVE B3 62 |
| 30 | LEFT | W | TURN_LIMIT | 200 | 10 | macro | 19 | 21 | 8790 | 18135 | 93 |  |  | 139 | 26 |  |  | 0 | 0 | 2 | 0 | MOVE B1 73;MOVE B2 74 |

**Opponent 187** - n=4, score 1.00 (4W/0D/0L), rounds 5,11,14,24, archetypes aggro_macro/macro/turtle

| round | us_side | outcome | reason | nturns | hp_margin | arch | opp_peak_war | us_peak_war | opp_total_income | us_total_income | opp_hq_l2_turn | opp_hq_l3_turn | opp_hq_l4_turn | us_hq_l3_turn | opp_first_forward_t | opp_first_at_enemy_hq_t | opp_first_fwd5_t | us_hq_siege_taken | opp_hq_siege_taken | us_bases_lost | opp_avg_ms | opp_turn1_cmds |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 5 | LEFT | W | HQ_DESTROYED | 153 | 15 | turtle | 27 | 25 | 14145 | 15780 |  |  |  |  |  |  |  | 0 | 10 | 3 | 0.4 | MOVE B1 64;MOVE B2 57;TRAIN 1 |
| 11 | LEFT | W | TURN_LIMIT | 200 | 15 | macro | 18 | 24 | 11835 | 26865 | 65 |  |  | 142 | 61 |  | 78 | 0 | 0 | 9 | 6.3 | MOVE B1 69;MOVE B2 65 |
| 14 | RIGHT | W | TURN_LIMIT | 200 | 15 | aggro_macro | 27 | 25 | 18510 | 29910 | 56 |  |  | 145 | 70 |  | 112 | 0 | 0 | 13 | 7.5 | MOVE A1 12;MOVE A2 10 |
| 24 | RIGHT | W | TURN_LIMIT | 200 | 20 | macro | 18 | 23 | 12120 | 29925 |  |  |  | 144 | 66 |  | 76 | 0 | 0 | 7 | 6.9 | MOVE A1 1;MOVE A2 16 |

**Opponent 1059** - n=4, score 1.00 (4W/0D/0L), rounds 10,21,26,27, archetypes early_aggro/macro

| round | us_side | outcome | reason | nturns | hp_margin | arch | opp_peak_war | us_peak_war | opp_total_income | us_total_income | opp_hq_l2_turn | opp_hq_l3_turn | opp_hq_l4_turn | us_hq_l3_turn | opp_first_forward_t | opp_first_at_enemy_hq_t | opp_first_fwd5_t | us_hq_siege_taken | opp_hq_siege_taken | us_bases_lost | opp_avg_ms | opp_turn1_cmds |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 10 | LEFT | W | TURN_LIMIT | 200 | 15 | macro | 16 | 20 | 12030 | 27945 | 42 |  |  | 128 | 86 | 94 | 89 | 0 | 0 | 0 | 0.8 | MOVE B2 61;MOVE B3 46 |
| 21 | LEFT | W | TURN_LIMIT | 200 | 20 | early_aggro | 10 | 25 | 8130 | 23340 |  |  |  | 145 | 5 |  | 26 | 0 | 0 | 2 | 0.0 | MOVE B2 59;MOVE B3 13 |
| 26 | LEFT | W | TURN_LIMIT | 200 | 15 | macro | 10 | 16 | 8055 | 18135 |  |  |  | 129 | 41 | 71 | 58 | 0 | 0 | 4 | 0 | MOVE B2 59;MOVE B3 51 |
| 27 | LEFT | W | TURN_LIMIT | 200 | 10 | macro | 14 | 17 | 10755 | 14895 |  |  |  | 187 | 48 | 71 | 48 | 8 | 0 | 6 | 0.0 | MOVE B2 91;MOVE B3 52 |

### 3.2 All opponents met >= 3 times (66), compact

| opp | n | score | W | D | L | rounds | n_LEFT | arch | archs | med_margin | peak_army | income | HQ_L3_t | first_fwd_t | first_at_our_HQ_t | mean_fwd | avg_ms |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 522 | 3 | 0 | 0 | 0 | 3 | 8,11,17 | 2 | aggro_macro | aggro_macro | -15 | 28 | 28425 | 137 | 55 | 196 | 6.71 | 0.79 |
| 1182 | 3 | 0 | 0 | 0 | 3 | 4,7,13 | 1 | early_aggro | early_aggro | -10 | 9 | 5025 |  | 19 | 59 | 1.63 | 0 |
| 455 | 5 | 0 | 0 | 0 | 5 | 8,9,11,26,30 | 3 | aggro_macro | aggro_macro/macro | -10 | 29 | 19440 | 170.50 | 77 | 159 | 2.70 | 0.01 |
| 1170 | 5 | 0 | 0 | 0 | 5 | 6,11,12,20,24 | 1 | aggro_macro | aggro_macro | -15 | 30 | 17640 | 129.50 | 40 | 181 | 3.77 | 0.15 |
| 313 | 7 | 0 | 0 | 0 | 7 | 6,14,19,21,24,26,28 | 5 | aggro_macro | aggro_macro/macro | -20 | 26 | 16470 | 127.50 | 78 | 129 | 2.49 | 1.13 |
| 474 | 8 | 0 | 0 | 0 | 8 | 4,10,15,21,22,25,27,30 | 0 | aggro_macro | aggro_macro/macro | -15 | 22.50 | 9592.50 |  | 57 | 94.50 | 3.20 | 1.06 |
| 138 | 6 | 0.17 | 1 | 0 | 5 | 5,10,11,20,23,30 | 3 | aggro_macro | aggro_macro/early_aggro/macro | -20 | 42 | 23377.50 | 101 | 61.50 | 164 | 4.12 | 0.01 |
| 97 | 5 | 0.20 | 1 | 0 | 4 | 3,6,19,22,25 | 1 | aggro_macro | aggro_macro/early_aggro/turtle | -15 | 32 | 21210 |  | 71 | 183 | 3.43 | 9.46 |
| 395 | 5 | 0.20 | 1 | 0 | 4 | 6,7,14,21,22 | 0 | macro | aggro_macro/macro | -10 | 10 | 4680 |  | 30 | 84 | 2.33 | 0 |
| 994 | 5 | 0.20 | 1 | 0 | 4 | 5,16,18,19,20 | 1 | macro | aggro_macro/macro | -10 | 10 | 4575 |  | 30 | 65 | 1.41 | 0.38 |
| 129 | 4 | 0.25 | 1 | 0 | 3 | 4,10,12,14 | 1 | aggro_macro | aggro_macro/early_aggro/macro | -12.50 | 14 | 9367.50 | 118 | 28.50 | 120 | 4.06 | 0 |
| 1167 | 4 | 0.25 | 1 | 0 | 3 | 5,13,16,23 | 4 | aggro_macro | aggro_macro | -7.50 | 33 | 25185 | 117 | 69.50 | 183.50 | 5.63 | 0.40 |
| 1237 | 6 | 0.25 | 1 | 1 | 4 | 7,16,22,24,25,26 | 1 | aggro_macro | aggro_macro/early_aggro/macro | -12.50 | 25.50 | 24517.50 | 150 | 52 | 181.50 | 3.21 | 0.09 |
| 184 | 5 | 0.30 | 1 | 1 | 3 | 5,6,9,15,20 | 1 | macro | aggro_macro/macro | -5 | 12 | 12525 | 128 | 48 | 98 | 1.80 | 0.11 |
| 997 | 5 | 0.30 | 1 | 1 | 3 | 12,22,25,26,27 | 3 | aggro_macro | aggro_macro/macro | -10 | 19 | 12525 | 179 | 33 | 194 | 1.75 | 0.13 |
| 580 | 3 | 0.33 | 1 | 0 | 2 | 10,18,20 | 1 | aggro_macro | aggro_macro/hq_rush/macro | -5 | 23 | 17700 | 171 | 44 | 99 | 1.42 | 0 |
| 809 | 3 | 0.33 | 1 | 0 | 2 | 13,17,25 | 3 | aggro_macro | aggro_macro/early_aggro | -10 | 19 | 13875 | 151 | 38 | 120 | 4.85 | 0.32 |
| 822 | 3 | 0.33 | 1 | 0 | 2 | 8,9,26 | 2 | aggro_macro | aggro_macro | -15 | 35 | 22530 | 98 | 55 | 127 | 2.53 | 0.06 |
| 884 | 3 | 0.33 | 1 | 0 | 2 | 14,21,23 | 2 | macro | aggro_macro/macro | -10 | 20 | 16170 | 154 | 68 | 149 | 1.51 | 0 |
| 1148 | 3 | 0.33 | 1 | 0 | 2 | 9,10,11 | 2 | aggro_macro | aggro_macro | -10 | 30 | 18915 | 169 | 46 | 136 | 3.93 | 0 |
| 865 | 4 | 0.38 | 1 | 1 | 2 | 4,12,24,27 | 1 | aggro_macro | aggro_macro/turtle | -7.50 | 45.50 | 23722.50 | 139 | 35 | 137.50 | 3.89 | 0.01 |
| 618 | 4 | 0.50 | 2 | 0 | 2 | 3,9,11,18 | 2 | aggro_macro | aggro_macro/macro | -5 | 41.50 | 22455 | 127 | 65.50 | 170 | 1.62 | 0.01 |
| 642 | 4 | 0.50 | 2 | 0 | 2 | 11,15,20,26 | 2 | macro | aggro_macro/macro | 2.50 | 19 | 12382.50 |  | 57.50 | 120 | 1.42 | 0 |
| 696 | 4 | 0.50 | 2 | 0 | 2 | 9,11,26,28 | 4 | aggro_macro | aggro_macro/macro/turtle | -2.50 | 23.50 | 14482.50 | 156 | 66 | 187 | 1.06 | 0.10 |
| 762 | 4 | 0.50 | 2 | 0 | 2 | 9,11,12,19 | 1 | aggro_macro | aggro_macro/macro | 0 | 21.50 | 18952.50 | 106.50 | 53 | 158.50 | 1.77 | 0.01 |
| 518 | 5 | 0.50 | 2 | 1 | 2 | 16,19,21,23,27 | 4 | macro | macro | 0 | 14 | 6885 |  | 40 | 52 | 1.80 | 0 |
| 773 | 5 | 0.50 | 2 | 1 | 2 | 5,7,14,16,29 | 4 | aggro_macro | aggro_macro/macro | 0 | 32 | 16485 | 134 | 31 | 191 | 1.38 | 0 |
| 916 | 4 | 0.62 | 1 | 3 | 0 | 3,22,24,26 | 1 | macro | aggro_macro/macro | 0 | 18.50 | 17385 | 174.50 | 65 |  | 0.27 | 1.32 |
| 1227 | 4 | 0.62 | 2 | 1 | 1 | 8,15,16,25 | 2 | macro | aggro_macro/macro | 5 | 19 | 21082.50 | 138 | 63.50 | 96 | 0.85 | 9.15 |
| 388 | 3 | 0.67 | 2 | 0 | 1 | 16,24,30 | 0 | aggro_macro | aggro_macro | 10 | 21 | 14625 | 144 | 76 | 121 | 0.67 | 4.51 |
| 489 | 3 | 0.67 | 2 | 0 | 1 | 12,21,27 | 2 | macro | aggro_macro/macro | 15 | 38 | 23475 |  | 44 | 185.50 | 0.72 | 0.02 |
| 829 | 3 | 0.67 | 2 | 0 | 1 | 10,11,24 | 1 | macro | aggro_macro/macro | 10 | 18 | 12585 | 180 | 51 | 165 | 2.06 | 0 |
| 902 | 3 | 0.67 | 2 | 0 | 1 | 10,11,30 | 3 | aggro_macro | aggro_macro/macro | 5 | 23 | 21570 | 183 | 48 | 188.50 | 2.48 | 8.28 |
| 1060 | 3 | 0.67 | 2 | 0 | 1 | 4,7,17 | 3 | hq_rush | hq_rush | 15 | 6 | 1695 | 141 | 9 | 13 | 0.26 | 0 |
| 1092 | 3 | 0.67 | 2 | 0 | 1 | 4,7,27 | 3 | aggro_macro | aggro_macro/passive/turtle | 5 | 23 | 14385 | 80 | 58 | 101 | 0.06 | 0 |
| 1382 | 3 | 0.67 | 2 | 0 | 1 | 8,9,12 | 3 | macro | aggro_macro/macro | 10 | 17 | 14235 | 89 | 49 | 113.50 | 1.41 | 0 |
| 256 | 6 | 0.67 | 3 | 2 | 1 | 12,15,21,22,28,30 | 4 | aggro_macro | aggro_macro/early_aggro/macro | 2.50 | 29 | 21112.50 | 189 | 51 | 192 | 1.74 | 0.01 |
| 25 | 8 | 0.69 | 5 | 1 | 2 | 7,16,18,19,22,27,28,30 | 5 | turtle | macro/turtle | 5 | 19 | 16702.50 | 151 | 73.50 | 176 | 0.15 | 0 |
| 453 | 4 | 0.75 | 3 | 0 | 1 | 3,9,21,29 | 2 | turtle | aggro_macro/macro/turtle | 12.50 | 12.50 | 8550 | 125 | 49.50 | 197 | 1.37 | 0 |
| 657 | 4 | 0.75 | 3 | 0 | 1 | 6,14,22,26 | 1 | turtle | aggro_macro/macro/turtle | 7.50 | 20.50 | 14835 | 82 | 106 | 164 | 1.66 | 0.07 |
| 978 | 4 | 0.75 | 3 | 0 | 1 | 4,8,13,20 | 2 | hq_rush | aggro_macro/hq_rush/macro | 10 | 19 | 12315 | 156.50 | 23.50 | 17 | 0.24 | 0.01 |
| 1154 | 6 | 0.75 | 4 | 1 | 1 | 12,15,17,18,21,22 | 4 | aggro_macro | aggro_macro/early_aggro/macro/turtle | 5 | 25 | 19560 | 165 | 77.50 | 113.50 | 1.46 | 2.46 |
| 36 | 5 | 0.80 | 3 | 2 | 0 | 9,10,20,25,29 | 5 | aggro_macro | aggro_macro/macro/passive | 5 | 12 | 11070 | 93 | 58 | 192 | 0.66 | 0 |
| 290 | 5 | 0.80 | 4 | 0 | 1 | 6,12,20,23,25 | 5 | macro | macro/turtle | 20 | 9 | 8085 |  | 31 | 97.50 | 0.28 | 0 |
| 894 | 5 | 0.80 | 3 | 2 | 0 | 3,6,12,19,23 | 4 | aggro_macro | aggro_macro/macro | 5 | 24 | 18015 | 171 | 44 | 154 | 1.42 | 0 |
| 1010 | 5 | 0.80 | 4 | 0 | 1 | 14,17,19,21,26 | 4 | aggro_macro | aggro_macro/macro/turtle | 10 | 25 | 15000 | 150 | 71 |  | 0.85 | 0 |
| 107 | 3 | 0.83 | 2 | 1 | 0 | 22,27,30 | 3 | macro | macro/passive | 5 | 13 | 9420 | 197 | 39 | 192.50 | 1.33 | 0.11 |
| 146 | 3 | 0.83 | 2 | 1 | 0 | 11,14,23 | 2 | macro | early_aggro/macro | 15 | 15 | 10905 | 141 | 39 | 91 | 0.95 | 0 |
| 819 | 3 | 0.83 | 2 | 1 | 0 | 9,15,30 | 3 | macro | aggro_macro/macro | 5 | 14 | 14940 | 100.50 | 43 |  | 2.17 | 0.04 |
| 1050 | 3 | 0.83 | 2 | 1 | 0 | 4,13,28 | 3 | macro | hq_rush/macro | 10 | 20 | 15795 | 167 | 63 | 29 | 0.52 | 0 |
| 1620 | 3 | 0.83 | 2 | 1 | 0 | 24,25,27 | 1 | turtle | macro/turtle | 5 | 22 | 19740 | 190.50 | 72 |  | 0 | 0 |
| 345 | 4 | 0.88 | 3 | 1 | 0 | 3,4,5,23 | 2 | macro | aggro_macro/macro/passive | 22.50 | 20.50 | 18997.50 | 77 | 59.50 | 112 | 0.29 | 0.01 |
| 967 | 5 | 0.90 | 4 | 1 | 0 | 19,21,24,26,30 | 2 | macro | macro | 5 | 19 | 17700 | 143 | 43 |  | 0.29 | 0.03 |
| 75 | 3 | 1 | 3 | 0 | 0 | 11,18,22 | 2 | aggro_macro | aggro_macro/macro | 10 | 27 | 17940 | 132 | 38 | 176 | 3.91 | 0.01 |
| 115 | 3 | 1 | 3 | 0 | 0 | 12,20,25 | 2 | macro | macro | 10 | 12 | 8865 | 175 | 55 | 81 | 0.39 | 0 |
| 173 | 3 | 1 | 3 | 0 | 0 | 9,12,21 | 3 | aggro_macro | aggro_macro/macro | 10 | 20 | 12585 | 178 | 97 | 102 | 2.71 | 1.41 |
| 280 | 3 | 1 | 3 | 0 | 0 | 10,15,27 | 2 | macro | aggro_macro/macro | 5 | 16 | 12510 | 161 | 65 | 98 | 0.41 | 0.04 |
| 418 | 3 | 1 | 3 | 0 | 0 | 16,20,30 | 3 | turtle | macro/turtle | 20 | 16 | 10290 |  | 80 |  | 0 | 0 |
| 621 | 3 | 1 | 3 | 0 | 0 | 5,26,30 | 3 | turtle | turtle | 25 | 22 | 12090 | 77.50 | 168 |  | 0 | 0.24 |
| 725 | 3 | 1 | 3 | 0 | 0 | 3,20,29 | 2 | macro | macro | 10 | 19 | 12780 | 87 | 74 | 186 | 0.34 | 1.39 |
| 770 | 3 | 1 | 3 | 0 | 0 | 25,28,29 | 3 | macro | macro | 15 | 27 | 16875 |  | 67 |  | 0.01 | 0.52 |
| 999 | 3 | 1 | 3 | 0 | 0 | 10,13,18 | 1 | macro | macro | 15 | 22 | 21480 | 116 | 62 |  | 0.50 | 0.33 |
| 1036 | 3 | 1 | 3 | 0 | 0 | 3,5,6 | 2 | early_aggro | early_aggro | 15 | 27 | 18030 | 199 | 7 |  | 1.32 | 0 |
| 1070 | 3 | 1 | 3 | 0 | 0 | 4,10,27 | 2 | hq_rush | hq_rush/macro | 15 | 7 | 3000 |  | 8 | 12 | 0.23 | 0.12 |
| 187 | 4 | 1 | 4 | 0 | 0 | 5,11,14,24 | 2 | macro | aggro_macro/macro/turtle | 15 | 22.50 | 13132.50 |  | 66 |  | 0.32 | 6.57 |
| 1059 | 4 | 1 | 4 | 0 | 0 | 10,21,26,27 | 4 | macro | early_aggro/macro | 15 | 12 | 9442.50 |  | 44.50 | 71 | 0.58 | 0.01 |


## 4. Deep dive: the opponents that beat us most (>=3 games, score <= 0.34)

Danger list (20): [522, 1182, 455, 1170, 313, 474, 138, 97, 395, 994, 129, 1167, 1237, 184, 997, 580, 809, 822, 884, 1148]. Full per-game narratives (timelines every 25 turns, first-siege snapshot, base losses before siege, their command habits, move targets) are in `analysis/parsed/opponents/deepdive.txt`; the table below condenses them (medians over that opponent's games).

### 4.1 Playbook table

| opp | n | score | W_D_L | our_side_RIGHT | end_turn_med | HQ_kills | opp_first_fwd5_med | push_t_med | opp_HQ_at_push | opp_war_at_push | us_war_at_push | army_lead5_t_med | first_siege_med | our_HQ_lvl_at_siege | our_war_at_siege | our_bases_lost_med | our_bases_lost_before_siege_med | our_died_med | opp_peak_war_med | us_peak_war_med | inc_ratio_med | opp_HQ_L3_reached | train_turn_frac | own_bld_share | moves_to_our_half | avg_ms_med | hunger_games |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 474 | 8 | 0 | 0/0/8 | 8 | 107.5 | 8 | 68 | 69.5 | 2 | 18 | 11 | 62.5 | 102 | 2 | 0.5 | 7 | 7 | 19.5 | 22.5 | 13 | 1.4 | 0 | 0.3 | 0.7 | 0.6 | 1.1 | 1 |
| 313 | 7 | 0 | 0/0/7 | 2 | 162 | 6 | 78 | 78 | 2 | 19 | 14 | 89 | 153.5 | 2 | 0 | 8 | 8 | 49 | 26 | 16 | 1.4 | 4 | 0.4 | 0.7 | 0.5 | 1.1 | 0 |
| 455 | 5 | 0 | 0/0/5 | 2 | 195 | 3 | 90 | 131 | 2 | 25 | 13 | 86 | 171 | 1 | 1 | 6 | 6 | 62 | 29 | 17 | 1.6 | 4 | 0.3 | 0.7 | 0.3 | 0.0 | 0 |
| 1170 | 5 | 0 | 0/0/5 | 4 | 182 | 5 | 40 | 146 | 2 | 30 | 11 | 95 | 181 | 1 | 1 | 8 | 8 | 24 | 30 | 12 | 2.2 | 2 | 0.3 | 0.7 | 0.5 | 0.1 | 0 |
| 522 | 3 | 0 | 0/0/3 | 1 | 200 | 1 | 57 | 116 | 1 | 25 | 11 | 57 | 188 | 2 | 0 | 8 | 8 | 32 | 28 | 13 | 2.3 | 3 | 0.3 | 0.7 | 0.8 | 0.8 | 0 |
| 1182 | 3 | 0 | 0/0/3 | 2 | 168 | 2 | 53 | 19 | 1 | 6 | 5 | 64 | 144 | 1 | 0 | 2 | 2 | 16 | 9 | 5 | 1.9 | 0 | 0.1 | 0.6 | 0.6 | 0 | 0 |
| 138 | 6 | 0.2 | 1/0/5 | 3 | 169.5 | 5 | 72.5 | 135 | 3 | 33.5 | 14 | 64 | 164 | 1 | 0 | 7 | 7 | 48.5 | 42 | 21.5 | 1.6 | 4 | 0.4 | 0.8 | 0.4 | 0.0 | 1 |
| 97 | 5 | 0.2 | 1/0/4 | 4 | 187 | 4 | 138 | 162 | 2 | 27 | 15 | 62.5 | 173.5 | 2 | 0 | 8 | 7 | 79 | 32 | 22 | 1.3 | 0 | 0.4 | 0.7 | 0.4 | 9.5 | 0 |
| 395 | 5 | 0.2 | 1/0/4 | 5 | 107 | 4 | 30 | 30 | 1 | 9 | 6 | 35 | 98.5 | 1 | 0 | 7 | 7 | 14 | 10 | 7 | 1.0 | 0 | 0.2 | 0.6 | 0.6 | 0 | 0 |
| 994 | 5 | 0.2 | 1/0/4 | 4 | 104 | 4 | 30 | 30 | 1 | 9 | 5 | 61.5 | 87.5 | 1 | 0 | 5 | 5 | 13 | 10 | 6 | 1.5 | 0 | 0.2 | 0.7 | 0.6 | 0.4 | 0 |
| 1237 | 6 | 0.2 | 1/1/4 | 5 | 200 | 2 | 65.5 | 78 | 1 | 18.5 | 14 | 71.5 | 183 | 2 | 0 | 8 | 8 | 52 | 25.5 | 21 | 1.7 | 4 | 0.3 | 0.8 | 0.4 | 0.1 | 0 |
| 129 | 4 | 0.2 | 1/0/3 | 3 | 195.5 | 2 | 29 | 39.5 | 1 | 9.5 | 5.5 | 48 | 154 | 1 | 0 | 4 | 4 | 11 | 14 | 7 | 1.8 | 3 | 0.1 | 0.7 | 0.8 | 0 | 1 |
| 1167 | 4 | 0.2 | 1/0/3 | 0 | 200 | 1 | 81 | 86.5 | 2.5 | 24.5 | 17.5 | 73 | 184 | 1 | 0 | 12 | 12 | 41 | 33 | 23 | 1.7 | 3 | 0.2 | 0.8 | 0.5 | 0.4 | 1 |
| 184 | 5 | 0.3 | 1/1/3 | 4 | 200 | 2 | 67.5 | 62 | 1 | 12 | 7 | 59.5 | 118 | 1 | 0 | 7 | 7 | 34 | 12 | 14 | 1.2 | 2 | 0.2 | 0.7 | 0.5 | 0.1 | 0 |
| 997 | 5 | 0.3 | 1/1/3 | 2 | 197 | 3 | 65 | 56 | 1 | 12 | 5 | 81 | 194 | 0 | 0 | 4 | 4 | 41 | 19 | 12 | 1.8 | 1 | 0.2 | 0.7 | 0.4 | 0.1 | 2 |
| 580 | 3 | 0.3 | 1/0/2 | 2 | 200 | 0 | 51 | 44 | 1 | 11 | 6 | 65 |  |  |  | 10 | 10 | 28 | 23 | 16 | 1.4 | 2 | 0.2 | 0.7 | 0.4 | 0 | 0 |
| 809 | 3 | 0.3 | 1/0/2 | 0 | 200 | 1 | 46 | 94 | 2 | 16 | 11 | 116.5 | 147 | 1 | 3 | 5 | 5 | 27 | 19 | 20 | 1.4 | 1 | 0.3 | 0.8 | 0.8 | 0.3 | 0 |
| 822 | 3 | 0.3 | 1/0/2 | 1 | 170 | 2 | 81 | 100 | 1 | 25 | 11 | 51.5 | 138 | 1.5 | 4.5 | 7 | 7 | 41 | 35 | 15 | 1.8 | 1 | 0.3 | 0.8 | 0.3 | 0.1 | 0 |
| 884 | 3 | 0.3 | 1/0/2 | 1 | 200 | 1 | 79 | 98 | 1 | 18 | 13 | 111 | 152 | 2 | 2 | 8 | 8 | 40 | 20 | 15 | 1.3 | 1 | 0.3 | 0.8 | 0.3 | 0 | 0 |
| 1148 | 3 | 0.3 | 1/0/2 | 1 | 183 | 2 | 53 | 146 | 1 | 21 | 8 | 65 | 181.5 | 1.5 | 0 | 9 | 9 | 40 | 30 | 12 | 1.7 | 1 | 0.3 | 0.7 | 0.5 | 0 | 1 |

Column notes: `push_t` = first turn their forward count reached max(3, half of their peak forward count); `opp_HQ_at_push` = their HQ level then; `army_lead5_t` = first turn their army exceeded ours by >=5;
`our_HQ_lvl_at_siege` = our HQ level at the first siege hit; `own_bld_share` = average share of their warriors standing on their own buildings (working); `moves_to_our_half` = share of their MOVE commands targeting our half/our HQ; `hunger_games` = games in which we took hunger damage.

### 4.2 How each of them beats us (from the per-game series and command logs)

**474 (0/8; r4,10,15,21,22,25,27,30; we were RIGHT every time).** Opening `MOVE A2 x; MOVE A3 x` (two warriors to the nearest stronghold), 2-7 bases, HQ L2 at t43-59 in 5/8 games (never L3). Army 7-13 at t50, typically 16-27 by t75-100, trained on 25-35% of turns. A 5-10 stack crosses at t35-83 (`first_5fwd` median 68), hunts our bases (we lose 2-10 bases *before* any siege; median 7), its army is ahead of ours by 5 from t50-91, and the HQ is hit at t79-142 with 2-13 warriors in our HQ region while 0-1 of ours stand there (in 22_181147 and 30_251179 we still had 10-19 warriors - elsewhere); game over t91-144 (median 107.5), margin -10/-15. Income ratio only 1.4 - it wins on tempo, not on eco. In r10/r15 it ran at 82/67 ms per turn (a search), in other games ~1 ms - same result. Files: `games/04_64183.json`, `10_93461`, `15_127532`, `21_173261`, `22_181147`, `25_203407`, `27_228417`, `30_251179`.
**313 (0/7; r6-28; 5x LEFT, 2x RIGHT).** Same skeleton but bigger: 5-9 bases, HQ L2 at t42-56 (5/7), L3 at t105-145 (4/7), army 15-22 at t75, 5-stack at t54-103 (median 78), 6-15 of our bases destroyed before the siege, our army collapses from ~15 to <5 by t125-150, HQ killed at t124-191 (6 HQ kills, 1 turn-limit loss by -20). 1-2 ms/turn. In 26_214804 (r26) we matched it in income until t150 (21.5k vs 23.4k) and still lost: 101 of our warriors died vs 70 - its L2/L3 bases with turrets shred our counter-attacks.
**455 (0/5; r8,9,11,26,30).** Worker-heavy (own-bld share 0.7-0.8, 89% of moves to its own strongholds in r30), 1 base more than us, HQ L2 at t51-105 (4/5); it waits: 5-stack at t77-155, army lead of 5 at t49-99 (never in r30), main push at t131-174 with 25-32 warriors in 3 games (t77/t108 with 13-19 in the others) when its HQ is L2/L3; 3 HQ kills at t165-195 and 2 turn-limit losses (-10, -5). In 30_255070 (r30, our latest bot) we out-ecoed it (17.4k vs 14.7k) and still lost -5: our HQ L2 at t86 vs their L2 at t67 and L3 at t174.
**1170 (0/5; r6-24).** Army-first with late HQ upgrade: L2 at t70-143 (4/5), L3 in 2 games (t128-131); army 13-19 at t100 but peak 20-58 at t157-177 (it doubles after t125 in 3 games), 5-stack at t39-76, 7-17 of our bases destroyed, then a 13-35-warrior wave into our HQ at t169-187 (5/5 HQ kills, margins -10..-20). Our peak army was 9-14.
**522 (0/3; r8,11,17).** HQ L1 until t115-143 (!), 8-13 bases, army 16-20 at t100, out-incomes us 2.2-3x (21-28k vs 9-13k), 5-stack at t54-70 and 7-17 warriors parked in our half from t100 onwards, then L2->L5 in the last 60 turns (HQ L5 at t161-199) to win the margin game (-15, -15) or kill us at t188.
**1182 (0/3; r4,7,13).** Tiny: peak army 8-9, 1 base, never upgrades; 4-stack at t18-21, kills our 1-2 bases, walks to our HQ; we deadlocked (0 warriors, <120 gold) in 2 of 3 games and lost the third at the turn limit by -2 with 0 warriors left.
**138 (1/6; r5-30).** The biggest armies we faced: peak 30-76 in the 5 losses, 7-18 bases, income 16-33k (1.55x ours), HQ L3 at t97-103 in 4/5 losses, 5-stack at t65-103, mass push of 19-43 warriors at t124-174 after our base count is gone; 5 HQ kills at t143-187. The single win (30_254642, r30, LEFT) is a game where it never got going (peak 17, L1 all game) and we reached HQ L5.
**97 (1/4+1W; r3-25).** Early scout in r3 only (first forward t4; t69-73 otherwise), 9-16 bases, income 1.1-2.1x ours, HQ L2 at t70-80 and never L3, 5-stack at t65-160 but then 19-25 warriors sit in our half; HQ kills at t164-194 after sieges starting t155-193. 3-11 ms/turn in r6-25.
**395 / 994 / 184(r15) / 129(r10-14) / 1182 / 997(r12) - the squad family (see 4.3).** 395: 0/4+1W, all with us RIGHT; 994: 0/4+1W.
**1167 (1/3+1W; all LEFT).** 28-44 peak army, 9-19 bases, 21.7-41.3k income (2.5x ours in r5), HQ L3 at t76-169 (3/4) and L4 in 2; wins at the turn limit (-10, -5) or by a 29-warrior wave into our HQ at t184 (13_111574, where we also starved: hunger 12).
**1237 (1/4, 1D; r7-26, 5x RIGHT).** HQ L2 only at t107-189 (L1 at push in every game), then a late L3/L4 (L4 at t163-189 in 4 games), 7-16 bases, income 1.7-2.4x ours in the losses, 5-stack at t63-93 and a permanent 8-19 presence in our half; 2 HQ kills at t183-186 with HQ L4, 2 turn-limit losses (-10, -15), a draw (r22) and a win (r7).
**997 (1/3, 1D).** 2-9 bases, L1 HQ most of the game (L2 at t167-196 or never), income 1.8-4.1x ours in the losses; 2 of 3 losses involved our hunger damage (13-14) and deadlock (0 warriors for the last 20-46 turns): it kills bases, we keep rebuilding them and starve.
**184 (1/3, 1D).** r15 squad-family game; r9/r20 base sniping (7 and 14 bases lost) with its HQ still L1 at the push.

### 4.3 The squad family (cheap early harassment) - all matching games

Fingerprint (from turn series): >=6 warriors at t=20 (TRAIN 1 on each of turns 1-5 from an L1 HQ), <=3 bases at t=25, a >=4 stack in our half by t<=45, no HQ L2 before t=100, peak army <=16, and not a naive HQ rush. 28 games / 19 opponents ([129, 184, 290, 295, 395, 518, 528, 657, 709, 994, 1054, 1059, 1093, 1154, 1182, 1328, 1409, 1414, 1566]); our score 0.143 (3W/2D/23L), 20 HQ kills, median end turn 146, median opponent peak army 10 and income 4912 vs our peak army 7 and 4 bases lost. By side: LEFT [0.115, 13], RIGHT [0.167, 15]; by era: {'r3-9': [0.0, 5], 'r10-21': [0.167, 15], 'r22-30': [0.188, 8]}.

| gid | opp | round | us_side | outcome | reason | nturns | hp_margin | opp_war_t20 | opp_peak_war | opp_total_trained | opp_peak_bases | opp_first_fwd4_t | opp_first_at_enemy_hq_t | us_bases_lost | us_bases_lost_by100 | us_peak_war | us_total_trained | us_total_income | opp_total_income | us_hq_l2_turn | opp_turn1_cmds |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 04_64885 | 1182 | 4 | RIGHT | L | HQ_DESTROYED | 168 | -10 | 6 | 8 | 24 | 1 | 19 | 56 | 2 | 2 | 5 | 13 | 2640 | 5025 |  | MOVE A2 7;TRAIN 1 |
| 06_70674 | 395 | 6 | RIGHT | L | HQ_DESTROYED | 110 | -10 | 6 | 12 | 19 | 2 | 33 | 84 | 7 | 7 | 7 | 11 | 3975 | 4800 |  | MOVE A2 20;MOVE A3 40 |
| 06_71097 | 657 | 6 | RIGHT | L | HQ_DESTROYED | 124 | -10 | 6 | 11 | 23 | 2 | 30 | 79 | 7 | 7 | 6 | 14 | 4275 | 5475 |  | MOVE A2 4;MOVE A3 1 |
| 07_75771 | 395 | 7 | RIGHT | L | HQ_DESTROYED | 87 | -10 | 6 | 10 | 14 | 2 | 29 | 85 | 8 |  | 6 | 8 | 3660 | 3810 |  | MOVE A2 9 |
| 07_76240 | 1182 | 7 | RIGHT | L | TURN_LIMIT | 200 | -2 | 6 | 9 | 22 | 1 | 18 | 137 | 9 | 8 | 7 | 18 | 6045 | 5970 |  | MOVE A2 12;TRAIN 1 |
| 11_97911 | 1093 | 11 | LEFT | L | HQ_DESTROYED | 199 | -10 | 6 | 11 | 38 | 2 | 34 | 126 | 9 | 7 | 9 | 27 | 7710 | 8760 |  | MOVE B2 84 |
| 12_106501 | 129 | 12 | RIGHT | L | HQ_DESTROYED | 196 | -20 | 6 | 12 | 10 | 3 | 29 | 189 | 4 | 4 | 8 | 9 | 4560 | 7605 |  | MOVE A3 5;MOVE A2 25 |
| 13_115543 | 1182 | 13 | LEFT | L | HQ_DESTROYED | 67 | -10 | 6 | 9 | 9 | 1 | 21 | 59 | 1 |  | 5 | 5 | 1050 | 1965 |  | MOVE B2 80;TRAIN 1 |
| 14_121514 | 395 | 14 | RIGHT | L | HQ_DESTROYED | 107 | -10 | 6 | 10 | 21 | 2 | 33 | 61 | 4 | 4 | 7 | 11 | 2985 | 4680 |  | MOVE A2 10;MOVE A3 25 |
| 15_128840 | 184 | 15 | RIGHT | L | HQ_DESTROYED | 121 | -10 | 6 | 10 | 24 | 2 | 32 | 81 | 5 | 5 | 7 | 12 | 3660 | 5340 |  | MOVE A2 1;MOVE A3 10 |
| 15_130446 | 1154 | 15 | RIGHT | L | HQ_DESTROYED | 163 | -10 | 6 | 9 | 16 | 1 | 19 | 118 | 7 | 7 | 7 | 12 | 4545 | 4110 |  | MOVE A2 44;TRAIN 1 |
| 16_135710 | 994 | 16 | LEFT | L | HQ_DESTROYED | 104 | -10 | 6 | 10 | 21 | 2 | 30 | 61 | 5 | 5 | 6 | 9 | 2970 | 4575 |  | MOVE B2 48;MOVE B3 35 |
| 16_137362 | 518 | 16 | LEFT | L | HQ_DESTROYED | 83 | -10 | 6 | 9 | 17 | 2 | 31 | 48 | 3 |  | 6 | 9 | 2190 | 3600 |  | MOVE B1 50;MOVE B2 28 |
| 16_138370 | 709 | 16 | LEFT | L | HQ_DESTROYED | 106 | -10 | 6 | 10 | 22 | 2 | 31 | 65 | 4 | 4 | 7 | 11 | 3000 | 4665 |  | MOVE B2 66;MOVE B3 62 |
| 17_143451 | 1328 | 17 | LEFT | D | TURN_LIMIT | 200 | 0 | 6 | 12 | 36 | 2 | 19 | 164 | 5 | 3 | 12 | 26 | 7125 | 7980 |  | MOVE B2 59;TRAIN 1 |
| 18_147159 | 994 | 18 | RIGHT | L | HQ_DESTROYED | 92 | -10 | 6 | 10 | 18 | 2 | 31 | 65 | 4 |  | 6 | 9 | 2595 | 4005 |  | MOVE A2 2;MOVE A3 47 |
| 19_156248 | 994 | 19 | RIGHT | L | HQ_DESTROYED | 84 | -10 | 6 | 9 | 17 | 2 | 30 | 61 | 4 |  | 6 | 10 | 2700 | 3645 |  | MOVE A2 3;MOVE A3 10 |
| 20_165591 | 994 | 20 | RIGHT | W | TURN_LIMIT | 200 | 5 | 6 | 10 | 30 | 2 | 30 | 138 | 10 | 5 | 10 | 22 | 10575 | 8895 | 105 | MOVE A2 4;MOVE A3 30 |
| 20_166314 | 290 | 20 | LEFT | L | HQ_DESTROYED | 182 | -10 | 6 | 9 | 39 | 2 | 30 | 130 | 6 | 4 | 8 | 23 | 5790 | 8085 |  | MOVE B2 72;MOVE B3 50 |
| 21_172540 | 395 | 21 | RIGHT | W | TURN_LIMIT | 200 | 5 | 6 | 9 | 41 | 2 | 30 | 61 | 10 | 4 | 9 | 33 | 10830 | 8895 | 153 | MOVE A2 2;MOVE A3 34 |
| 22_177010 | 395 | 22 | RIGHT | L | HQ_DESTROYED | 96 | -10 | 6 | 11 | 17 | 2 | 30 | 93 | 7 |  | 6 | 12 | 4095 | 4215 |  | MOVE A2 6;MOVE A3 29 |
| 22_177410 | 1414 | 22 | LEFT | L | HQ_DESTROYED | 169 | -10 | 6 | 11 | 30 | 2 | 34 | 150 | 7 | 4 | 8 | 25 | 6255 | 7230 |  | MOVE B1 51;MOVE B2 27 |
| 23_187618 | 528 | 23 | LEFT | L | TURN_LIMIT | 200 | -7 | 6 | 8 | 10 | 3 | 30 | 33 | 2 | 2 | 8 | 6 | 1530 | 4080 |  | MOVE B1 56;MOVE B2 58 |
| 24_200289 | 1409 | 24 | RIGHT | D | TURN_LIMIT | 200 | 0 | 6 | 8 | 10 | 2 | 42 | 189 | 2 | 2 | 8 | 8 | 3945 | 4065 |  | MOVE A1 2;MOVE A2 6 |
| 26_217992 | 295 | 26 | LEFT | L | HQ_DESTROYED | 128 | -10 | 6 | 9 | 21 | 2 | 31 | 124 | 3 | 3 | 9 | 15 | 3825 | 5205 |  | MOVE B1 54;MOVE B2 38 |
| 26_218396 | 1059 | 26 | LEFT | W | TURN_LIMIT | 200 | 15 | 6 | 10 | 34 | 3 | 41 | 71 | 4 | 2 | 16 | 33 | 18135 | 8055 | 113 | MOVE B2 59;MOVE B3 51 |
| 28_230911 | 1566 | 28 | LEFT | L | TURN_LIMIT | 200 | -5 | 6 | 6 | 3 | 0 | 8 | 193 | 0 | 0 | 7 | 6 | 3000 | 3000 |  | TRAIN 1 |
| 28_235924 | 1054 | 28 | LEFT | L | HQ_DESTROYED | 98 | -10 | 6 | 12 | 16 | 5 | 32 | 87 | 3 |  | 8 | 6 | 2370 | 5055 |  | MOVE B1 89;MOVE B2 88 |

Mechanism (e.g. `games/06_70674.json`, `14_121514`, `16_135710`, `19_156248`): their 5-stack arrives at one of our L1 bases around t30-35 where we keep exactly one worker; 5v1 combat kills the worker in one day and the 6-hp base in two more; they move to the next base; our bot responds by re-sending single warriors / rebuilding, losing 11-14 warriors over the game while training only 8-12 (income 3-4k). By t60-90 we have 2-5 warriors left, they walk 2-6 warriors into our L1 HQ (10 hp, 1 turret shot) and finish it in 3-7 turns. They never need more than 10 warriors. The 3 wins/2 draws came when we kept our army together (us_peak_war 9-16) or they bugged out (28_230911: 6 warriors, 0 bases, -5 turn-limit loss anyway).

### 4.4 Economic deadlock games (0 warriors and <120 gold for >=5 consecutive turns)

| gid | opp | round | us_side | outcome | reason | nturns | hp_margin | arch | us_deadlock_run | us_peak_war | opp_peak_war | us_bases_lost |
|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 03_60679 | 97 | 3 | RIGHT | L | HQ_DESTROYED | 181 | -10 | early_aggro | 27 | 12 | 38 | 8 |
| 04_64885 | 1182 | 4 | RIGHT | L | HQ_DESTROYED | 168 | -10 | early_aggro | 25 | 5 | 8 | 2 |
| 06_70674 | 395 | 6 | RIGHT | L | HQ_DESTROYED | 110 | -10 | aggro_macro | 7 | 7 | 12 | 7 |
| 07_76716 | 386 | 7 | LEFT | L | HQ_DESTROYED | 20 | -10 | hq_rush | 6 | 3 | 7 | 0 |
| 07_77185 | 1245 | 7 | LEFT | L | HQ_DESTROYED | 24 | -10 | hq_rush | 7 | 3 | 6 | 0 |
| 09_83870 | 822 | 9 | LEFT | L | HQ_DESTROYED | 144 | -15 | aggro_macro | 5 | 11 | 21 | 7 |
| 09_87765 | 1382 | 9 | LEFT | L | HQ_DESTROYED | 195 | -15 | macro | 5 | 14 | 17 | 1 |
| 11_99904 | 642 | 11 | RIGHT | L | HQ_DESTROYED | 199 | -15 | aggro_macro | 7 | 19 | 36 | 11 |
| 13_115543 | 1182 | 13 | LEFT | L | HQ_DESTROYED | 67 | -10 | early_aggro | 5 | 5 | 9 | 1 |
| 14_120263 | 313 | 14 | RIGHT | L | HQ_DESTROYED | 175 | -10 | macro | 8 | 12 | 20 | 8 |
| 14_123109 | 1187 | 14 | LEFT | L | HQ_DESTROYED | 154 | -10 | macro | 13 | 8 | 12 | 6 |
| 15_128840 | 184 | 15 | RIGHT | L | HQ_DESTROYED | 121 | -10 | macro | 7 | 7 | 10 | 5 |
| 15_129487 | 1227 | 15 | RIGHT | L | TURN_LIMIT | 200 | -23 | aggro_macro | 100 | 8 | 18 | 5 |
| 15_130446 | 1154 | 15 | RIGHT | L | HQ_DESTROYED | 163 | -10 | early_aggro | 39 | 7 | 9 | 7 |
| 16_135710 | 994 | 16 | LEFT | L | HQ_DESTROYED | 104 | -10 | macro | 20 | 6 | 10 | 5 |
| 16_137362 | 518 | 16 | LEFT | L | HQ_DESTROYED | 83 | -10 | macro | 7 | 6 | 9 | 3 |
| 16_138370 | 709 | 16 | LEFT | L | HQ_DESTROYED | 106 | -10 | macro | 19 | 7 | 10 | 4 |
| 20_166314 | 290 | 20 | LEFT | L | HQ_DESTROYED | 182 | -10 | macro | 8 | 8 | 9 | 6 |
| 21_169604 | 313 | 21 | LEFT | L | HQ_DESTROYED | 144 | -15 | aggro_macro | 9 | 15 | 21 | 6 |
| 23_183808 | 712 | 23 | LEFT | L | HQ_DESTROYED | 58 | -10 | hq_rush | 43 | 3 | 6 | 1 |
| 23_187618 | 528 | 23 | LEFT | L | TURN_LIMIT | 200 | -7 | passive | 161 | 8 | 8 | 2 |
| 23_189903 | 138 | 23 | LEFT | L | HQ_DESTROYED | 143 | -20 | aggro_macro | 5 | 14 | 30 | 5 |
| 25_203407 | 474 | 25 | RIGHT | L | HQ_DESTROYED | 91 | -10 | macro | 13 | 8 | 14 | 2 |
| 25_204202 | 997 | 25 | RIGHT | L | HQ_DESTROYED | 194 | -20 | aggro_macro | 46 | 8 | 23 | 2 |
| 27_222725 | 997 | 27 | LEFT | L | HQ_DESTROYED | 194 | -10 | aggro_macro | 20 | 12 | 30 | 5 |
| 28_232577 | 696 | 28 | LEFT | L | HQ_DESTROYED | 138 | -15 | macro | 11 | 17 | 18 | 5 |
| 28_235924 | 1054 | 28 | LEFT | L | HQ_DESTROYED | 98 | -10 | macro | 8 | 8 | 12 | 3 |

All 27 are losses (16.8% of our losses): once the last warrior dies with <120 gold there is no income (income needs a warrior on a building) and no training; the opponent can finish the HQ with 1-2 warriors at leisure (23_183808: opp 712 needed 40 turns with 1-2 warriors). The opponent fell into the same state in 31 games (all our wins). Era counts: {'r3-9': 7, 'r10-21': 12, 'r22-30': 8}.


## 5. Dangerous opponent behaviours (per-game flags, counts and our score)

Flags are computed per game; `score_when` vs `score_when_not`; `losses` = our losses in games with the flag; `share_of_all_losses` = that count / 161. Flags B08-B13, B15, B17-B22, B25 are partly *outcomes* of the game (a winning opponent sieges, out-ecos, destroys bases), flags B01-B06, B14, B16, B23-B24, B27-B30 and E01-E04, E07-E12, E14 are ex-ante behaviours.

### 5.1 Outcome-linked and general flags

| behaviour | n | pct | score_when | score_when_not | losses | draws | share_of_all_losses | score_LEFT | score_RIGHT | n_r22_30 | score_r22_30 |
|---|---|---|---|---|---|---|---|---|---|---|---|
| B01 naive HQ rush: warriors at our HQ by t<=30 | 39 | 7 | 0.744 | 0.662 | 10 | 0 | 6.200 | 0.737 | 1 | 10 | 0.900 |
| B02 early forward presence (first enemy-half entry t<=25, not HQ rush) | 31 | 5.500 | 0.597 | 0.672 | 11 | 3 | 6.800 | 0.737 | 0.375 | 7 | 0.714 |
| B03 forward pressure in t51-100 (mean n_forward>=1) | 241 | 43 | 0.465 | 0.821 | 117 | 24 | 72.700 | 0.605 | 0.260 | 74 | 0.561 |
| B04 5+ warriors forward before t=100 | 312 | 55.700 | 0.532 | 0.839 | 134 | 24 | 83.200 | 0.659 | 0.267 | 106 | 0.608 |
| B05 5+ warriors forward at t=100-150 (mid push) | 46 | 8.200 | 0.533 | 0.680 | 17 | 9 | 10.600 | 0.591 | 0.385 | 9 | 0.611 |
| B06 5+ warriors forward only after t=150 (late push) | 33 | 5.900 | 0.727 | 0.664 | 7 | 4 | 4.300 | 0.761 | 0.650 | 6 | 0.500 |
| B07 3+ warriors at our HQ at some point | 217 | 38.800 | 0.350 | 0.869 | 134 | 14 | 83.200 | 0.468 | 0.187 | 55 | 0.400 |
| B08 sustained HQ siege (>=10 siege dmg on our HQ) | 111 | 19.800 | 0 | 0.833 | 111 | 0 | 68.900 | 0 | 0 | 26 | 0 |
| B09 big army (peak >=30) | 105 | 18.800 | 0.367 | 0.737 | 59 | 15 | 36.600 | 0.509 | 0.191 | 19 | 0.421 |
| B10 out-trained us (>=1.5x our total, games>=100t) | 69 | 12.300 | 0.116 | 0.745 | 60 | 2 | 37.300 | 0.172 | 0.068 | 9 | 0.111 |
| B11 out-ecoed us (total income > ours, games>=100t) | 178 | 31.800 | 0.169 | 0.901 | 134 | 28 | 83.200 | 0.237 | 0.090 | 39 | 0.064 |
| B12 out-ecoed us by t=100 (cum income > ours) | 202 | 36.100 | 0.337 | 0.855 | 119 | 30 | 73.900 | 0.478 | 0.149 | 38 | 0.237 |
| B13 army lead at t=100 (>= ours+5) | 110 | 19.600 | 0.236 | 0.773 | 80 | 8 | 49.700 | 0.347 | 0.148 | 18 | 0.111 |
| B14 HQ L2 before ours | 140 | 25 | 0.611 | 0.687 | 46 | 17 | 28.600 | 0.684 | 0.382 | 63 | 0.667 |
| B15 HQ L3 before ours | 157 | 28 | 0.427 | 0.762 | 77 | 26 | 47.800 | 0.537 | 0.258 | 35 | 0.357 |
| B16 HQ L3 by t<=100 | 63 | 11.200 | 0.667 | 0.668 | 14 | 14 | 8.700 | 0.733 | 0.500 | 11 | 0.682 |
| B17 reached HQ L4 | 91 | 16.200 | 0.407 | 0.719 | 43 | 22 | 26.700 | 0.491 | 0.271 | 20 | 0.325 |
| B18 reached HQ L5 / repaired HQ | 24 | 4.300 | 0.146 | 0.691 | 17 | 7 | 10.600 | 0.143 | 0.150 | 2 | 0 |
| B19 many bases (peak >=10) | 84 | 15 | 0.381 | 0.718 | 46 | 12 | 28.600 | 0.522 | 0.211 | 17 | 0.206 |
| B20 >=3 L3 bases | 12 | 2.100 | 0.458 | 0.672 | 5 | 3 | 3.100 | 0.583 | 0.333 | 2 | 0.500 |
| B21 destroyed >=2 of our bases | 361 | 64.500 | 0.560 | 0.864 | 139 | 40 | 86.300 | 0.679 | 0.337 | 114 | 0.614 |
| B22 destroyed >=5 of our bases | 233 | 41.600 | 0.479 | 0.803 | 109 | 25 | 67.700 | 0.606 | 0.283 | 55 | 0.445 |
| B23 heavy compute (avg >=20ms) | 9 | 1.600 | 0.556 | 0.670 | 3 | 2 | 1.900 | 0.714 | 0 | 1 | 0 |
| B24 medium compute (2-20ms) | 43 | 7.700 | 0.721 | 0.663 | 10 | 4 | 6.200 | 0.727 | 0.700 | 21 | 0.738 |
| B25 killed >=20 of our warriors | 405 | 72.300 | 0.674 | 0.652 | 109 | 46 | 67.700 | 0.769 | 0.455 | 123 | 0.699 |
| B26 we took hunger damage | 17 | 3 | 0.118 | 0.685 | 14 | 2 | 8.700 | 0.150 | 0.071 | 8 | 0.188 |
| B27 high activity (>=150 move cmds) | 197 | 35.200 | 0.551 | 0.731 | 78 | 21 | 48.400 | 0.679 | 0.338 | 69 | 0.580 |
| B28 never forward | 73 | 13 | 0.952 | 0.625 | 1 | 5 | 0.600 | 0.975 | 0.846 | 22 | 0.909 |
| B29 turn-1 all warriors to our HQ (sample-bot opening) | 11 | 2 | 0.909 | 0.663 | 1 | 0 | 0.600 | 0.909 |  | 5 | 0.800 |
| B30 first base by t<=3 | 475 | 84.800 | 0.656 | 0.735 | 139 | 49 | 86.300 | 0.767 | 0.399 | 160 | 0.709 |

### 5.2 Ex-ante flags from the turn series

| behaviour | n | pct | score_when | score_when_not | losses | draws | share_of_all_losses | score_LEFT | score_RIGHT | n_r22_30 | score_r22_30 |
|---|---|---|---|---|---|---|---|---|---|---|---|
| E01 4+ warriors in our half by t<=45 (early stack) | 119 | 21.200 | 0.492 | 0.715 | 58 | 5 | 36 | 0.608 | 0.222 | 34 | 0.603 |
| E02 4+ warriors in our half by t<=60 | 179 | 32 | 0.506 | 0.744 | 84 | 9 | 52.200 | 0.636 | 0.254 | 57 | 0.684 |
| E03 4+ warriors in our half at t 61-100 | 180 | 32.100 | 0.622 | 0.689 | 58 | 20 | 36 | 0.729 | 0.353 | 67 | 0.604 |
| E04 4+ warriors in our half only after t=100 or never | 201 | 35.900 | 0.853 | 0.564 | 19 | 21 | 11.800 | 0.888 | 0.720 | 56 | 0.875 |
| E05 we lost >=3 bases by t=100 | 169 | 30.200 | 0.447 | 0.763 | 88 | 11 | 54.700 | 0.591 | 0.270 | 45 | 0.556 |
| E06 we lost >=1 base by t=60 | 164 | 29.300 | 0.473 | 0.749 | 80 | 13 | 49.700 | 0.622 | 0.250 | 56 | 0.616 |
| E07 squad-family fingerprint | 28 | 5 | 0.143 | 0.695 | 23 | 2 | 14.300 | 0.115 | 0.167 | 8 | 0.188 |
| E08 opp out-ecoed us by t=50 (cum income) | 204 | 36.400 | 0.515 | 0.756 | 86 | 26 | 53.400 | 0.631 | 0.293 | 33 | 0.364 |
| E09 opp army >= ours+3 at t=50 | 87 | 15.500 | 0.408 | 0.716 | 50 | 3 | 31.100 | 0.640 | 0.182 | 13 | 0.385 |
| E10 opp HQ L1 still at t=100 but army >= 15 (army-first, no HQ upgrade) | 74 | 13.200 | 0.561 | 0.684 | 31 | 3 | 19.300 | 0.724 | 0.240 | 17 | 0.559 |
| E11 opp HQ L2+ by t=60 (eco-first) | 159 | 28.400 | 0.695 | 0.657 | 40 | 17 | 24.800 | 0.769 | 0.488 | 55 | 0.727 |
| E12 opp keeps >=70% of warriors at own buildings (worker-heavy) | 375 | 67 | 0.740 | 0.522 | 79 | 37 | 49.100 | 0.816 | 0.505 | 119 | 0.752 |
| E13 we hit economic deadlock (0 warriors & <120 gold >=5 turns) | 27 | 4.800 | 0 | 0.702 | 27 | 0 | 16.800 | 0 | 0 | 8 | 0 |
| E14 opp trained every turn t1-5 (6+ warriors at t=20) | 87 | 15.500 | 0.552 | 0.689 | 34 | 10 | 21.100 | 0.684 | 0.300 | 35 | 0.714 |

### 5.3 Timing of the first 4-stack in our half

| f4b | mean_LEFT | mean_RIGHT | count_LEFT | count_RIGHT | all_mean | all_n | losses | med_opp_peak_war |
|---|---|---|---|---|---|---|---|---|
| <=45 | 0.608 | 0.222 | 83 | 36 | 0.492 | 119 | 58 | 10 |
| 46-60 | 0.700 | 0.300 | 35 | 25 | 0.533 | 60 | 26 | 20 |
| 61-80 | 0.750 | 0.324 | 76 | 34 | 0.618 | 110 | 35 | 22.500 |
| 81-100 | 0.698 | 0.412 | 53 | 17 | 0.629 | 70 | 23 | 23.500 |
| 101-150 | 0.611 | 0.400 | 27 | 10 | 0.554 | 37 | 13 | 29 |
| 151-200 | 0.795 | 0.688 | 22 | 8 | 0.767 | 30 | 5 | 23 |
| never | 0.973 | 0.870 | 111 | 23 | 0.955 | 134 | 1 | 17 |

### 5.4 Our bases destroyed by t=100

| bl100 | n | score | losses |
|---|---|---|---|
| game<100t | 22 | 0.091 | 20 |
| 0 | 197 | 0.855 | 20 |
| 1-2 | 172 | 0.744 | 33 |
| 3-4 | 97 | 0.541 | 40 |
| 5+ | 72 | 0.319 | 48 |

### 5.5 Top 10 most dangerous opponent behaviours (ranked by losses they account for, discounting pure outcome flags)
| # | behaviour | n games | our score | losses (share of 161) | note |
|---|---|---|---|---|---|
| 1 | Sustained forward presence in t51-100 (mean >=1 warrior in our half) | 241 | 0.465 | 117 (72.7%) | ex-ante aggression; RIGHT 0.26 |
| 2 | 5+ warriors forward before t=100 | 312 | 0.532 | 134 (83.2%) | the push itself; 0.27 as RIGHT |
| 3 | A 4-stack in our half by t<=60 | 179 | 0.506 | 84 (52.2%) | <=t45: 0.49 (n=119); never: 0.955 |
| 4 | Base sniping: >=3 of our bases destroyed by t=100 | 169 | 0.447 | 88 (54.7%) | 5+ by t100: 0.32 (n=72) |
| 5 | Out-eco by t=50 (cum income > ours) | 204 | 0.515 | 86 (53.4%) | usually 1 base more + 2 workers per base |
| 6 | Army lead >= ours+3 at t=50 | 87 | 0.408 | 50 (31.1%) | they train every turn early |
| 7 | Big army (peak >=30) | 105 | 0.367 | 59 (36.6%) | aggro_macro subset; >40: 0.28 |
| 8 | Squad family (6 warriors by t20, 2 bases, 5-stack at t30, no HQ upgrade) | 28 | 0.143 | 23 (14.3%) | cheapest exploit; unchanged across versions |
| 9 | Naive sample-bot HQ rush (at our HQ by t<=30) | 39 | 0.744 | 10 (6.2%) | all 10 losses = HQ left with <=1 defender |
| 10 | Army-first with no HQ upgrade (L1 at t100 but >=15 warriors) | 74 | 0.561 | 31 (19.3%) | RIGHT 0.24; they convert gold into warriors, we into HQ levels |
Also notable: HQ L3 before ours (157 games, 0.43, 77 losses), reaching HQ L5/repairing (24, 0.15), many bases (>=10: 84, 0.38), our hunger damage (17, 0.12), economic deadlock (27, 0.0). Behaviours that are *not* dangerous: heavy compute (9, 0.56), turn-1 sample opening (11, 0.91), worker-heavy opponents (375, 0.74), never-forward opponents (73, 0.95), HQ L2 by t60 eco-first (159, 0.70).


## 6. Caveats and data limitations

- Opponent features are measured *against our bot*, so they are not an unbiased census of the opponent's style: a winning opponent shows more forward presence, more bases destroyed and higher income partly because it is winning. Section 1.7 and 5.2 use ex-ante windows (t<=45/60, t51-100) to limit this; archetype assignment uses timings and rates rather than totals.
- Side confounding: RIGHT games (n=153, score 0.41) are against stronger opponents on every proxy; every archetype scores worse when we are RIGHT. Archetype x side tables are given; the danger list is not purely a RIGHT artefact (313: 5 LEFT games, 1167: 4 LEFT, 1237: 1 LEFT + 5 RIGHT, squad family LEFT 0.115).
- Many opponents changed bots between rounds (89/131 repeat opponents switched archetype at least once); per-opponent "playbooks" are medians over possibly different versions (e.g. 994 was a 43-army pusher in r5 and a 10-warrior squad bot in r16-20).
- Gold/income are replay estimates. k-means without sklearn is a plain numpy implementation (k=6, best of 10 seeds) used only as a cross-check.
- n is small for many cells (e.g. hq_rush RIGHT n=1, heavy compute n=9); treat scores on <15 games as indicative.


## Verification (independent)

Re-derived from `parsed/games.csv`, `parsed/turns.csv` and `parsed/games/*.json` with an independent script,
`analysis/scripts/verify-opponents.py` (own feature code, own archetype implementation from the textual rules in section 1, own k-means; full
printout in `parsed/opponents/verify_out.txt`). Verdicts: CONFIRMED = numbers reproduce and interpretation sound; PARTIAL = numbers right but
interpretation overclaims or minor errors; REFUTED = wrong, corrected numbers given.

| id | verdict | what was checked / corrections |
|---|---|---|
| O1 | PARTIAL | All numbers reproduce exactly: 176 games (31.4%), 50W/20D/106L = 0.341, 106/161 = 65.8% of losses (71 HQ_DESTROYED, 35 TURN_LIMIT), other archetypes 0.744/0.558/0.819/0.896/0.915, medians opp peak 28, income ratio 1.37, army ratio 1.62, 8 bases; own k-means (k=6, 20 seeds) finds the same cluster (n=107, score 0.257, peak army 33, income 25.4k, mean fwd 4.3; 95/107 aggro_macro). Two caveats: (a) "our 17" is not our median peak army in these games (that is 19; 15 in the losses, 25.5 in the wins; 17 = 28/1.62); (b) the label uses whole-game forward presence (mean_fwd, fwd_per_war, peak_fwd), which is partly an outcome: re-assigning with only t<=100 data keeps just 59 of the 176 as aggro_macro (ex-ante aggro_macro = 60 games, 14W/5D/41L = 0.275, 25.5% of losses), the other 117 are games whose standing force appeared after t100. Only 51% of aggro_macro games have whole-game mean_fwd >= 3 and 29.5% have >= 3 forward on average in t51-100, so "sustained 3+ warriors from t~60" describes the median, not the class. The descriptive statement is right; "accounts for two thirds of losses" mixes cause and consequence. |
| O2 | CONFIRMED | 60.4% (64/106; 62.1% among the 103 games >= 100 turns) of aggro_macro losses had >= 3 of our bases destroyed by t100; median bases lost 7 / 3 / 1 (aggro_macro / macro / turtle); losses median income ratio 1.68, peak-army ratio 1.91 (wins 0.64); 71 HQ-destroyed losses: median first siege t164, end t169; our max HQ level in the 106 losses: L1 33.0%, L2 50.9% (L3 14%, L4 2%); at the first siege our HQ was L1 in 26 and L2 in 35 of the 64 games where the siege turn is not the death turn. Bases-lost-by-t100 table reproduces (0: 0.855 n=197; 1-2: 0.744; 3-4: 0.541; 5+: 0.319 n=72, 48 L). Deep dives: 474 lost us 2-10 bases before the first siege, 313 6-15, 1170 7-17 (all 8/7/5 games checked). "Single worker guarding each base" is not verifiable from the snapshots (no per-region data), but in the cited games we had 5-6 warriors spread over 3-4 bases + HQ when the stack arrived. |
| O3 | REFUTED | The fingerprint as coded in `opponents_extra.py` contains `opp_first_at_enemy_hq_t > 30`, which is False for NaN, i.e. it silently DROPS every fingerprint game in which the opponent never reached our HQ. Those 10 dropped games are 9W/1D (17_141440, 20_160552, 20_161611, 21_169982, 23_186097, 25_204997, 25_208139, 27_223951, 28_230497, 29_243083), so the 0.143 score is selected on outcome. With the fingerprint applied ex-ante (same thresholds, "not an HQ rush" = not at our HQ by t30): **38 games / 26 opponents, 12W/3D/23L = 0.355**; by era 0.0 (n=5) / 0.342 (19) / 0.500 (14) - it DID improve with later versions; LEFT 0.429 (21) / RIGHT 0.265 (17). Medians: opp peak army 10, income 5,152, our peak army 8, 4 bases lost, 20 HQ kills (same 23 losses). The opening description is also wrong: "TRAIN 1 every turn for the first 5 turns" never happens - 0 of the 87 games with 6 opponent warriors at t20 trained on all of t1-5, 65 of 87 trained zero times in t1-5 (they build 1-2 bases first, then TRAIN 1 whenever gold reaches 120, roughly every 4 turns: e.g. 06_70674 trains at t11,15,18,22,26,30); flag E14's label has the same error. What survives: 395 (1/5) and 994 (1/5) are real; the cited games 06_70674 / 14_121514 / 16_135710 / 19_156248 do show a 4-5 stack entering at t30-33 with us at 5-6 warriors over 3-4 bases, first death at t31-36, first base lost t32-37, and we lost 12-14 warriors while training 9-11. So it is a real weakness (23 losses), but a smaller and shrinking one than claimed, and not side-independent. |
| O4 | CONFIRMED | 27 games with a run >= 5 of (0 warriors & gold < 120): 27/27 losses (16.8% of losses), eras 7/12/8, 11 overlap with the (corrected or original) squad set, 3 with hq_rush; opponent deadlock 31 games, 31/31 wins; 23_183808 run 43 (HQ hit from t22 to t58 by 0-1 warriors at a time, our gold frozen at 108, 0 warriors from t16), 15_129487 run 100, 23_187618 run 161. Note the state is absorbing by the rules (no warrior -> no income; < 120 -> no training): none of the 27 ever recovered, so "every one is a loss" is near-tautological - the useful content is the 27 count and the guard. |
| O5 | PARTIAL | 39 games, 29W/10L = 0.744; era 0.571 (14) / 0.800 (15) / 0.900 (10); losses in r7 x6 (all 6 r7 hq_rush games), r10, r12, r21, r23; turn-1 >= 2 moves to our HQ: 11 games, 0.909 (all 3: 6 games, 1.0); 21_174014 and 23_183808 timelines reproduce exactly (5 warriors staged at 18, moved at t13, we emptied the HQ at t14-15, dead t19; 712: bases at t2/t7, last warrior dead t16, gold 108, HQ dead t58). Overclaim: "whenever the bot left <= 1 defender home" - min warriors at our HQ over t1-15 was 0 in 8 losses / 0 wins, 1 in 2 losses / 21 wins, 2 in 0 losses / 8 wins; at the turn the rush arrived: 0 defenders -> 6L/0W, 1 -> 3L/9W, 2 -> 1L/8W, 3+ -> 0L/12W. So an empty HQ is the killer and one defender is a coin flip; 21 wins also had <= 1 defender at some point. |
| O6 | CONFIRMED | Buckets reproduce exactly (<=45: 0.492 n=119 58L; 46-60 0.533; 61-80 0.618; 81-100 0.629; 101-150 0.554; 151-200 0.767; never 0.955 n=134 1L) including the LEFT/RIGHT split and the t51-100 presence flag (241 games, 0.465, 117 losses = 72.7%). "Strongest predictor" is not formally tested (no model), but the spread is the largest among the ex-ante flags checked (army lead >= 3 at t50: 0.408 vs 0.716; out-eco at t50: 0.515 vs 0.756). Restricting to games >= 100 turns flattens the <=45 bucket to 0.574 (17 short rush losses sit in it), the gradient remains. |
| O7 | PARTIAL | Never-beaten list reproduces (474 0/8, 313 0/7, 455 0/5, 1170 0/5, 522 0/3, 1182 0/3; no other opponent with >= 3 games and 0 wins). 474: HQ L2 at t43-59 in 5/8, never L3; 5-stack t35-83; army lead >= 5 first at t50-91; first siege t79-142 with 0 of our warriors at the HQ in all 8 games (10 and 19 elsewhere in 22_181147/30_251179); end t91-144, median 107.5; income ratio median 1.45; 82/67 ms in r10/r15 vs ~1 ms otherwise. 313: 5-stack t54-103, 6-15 bases, 6 HQ kills at t124-191, 5 LEFT. 1170: army 13-19 at t100, peak 20-58, 13-36 at our HQ from t169-186, 5/5 HQ kills. Errors: r24-30 has **32** losses in 140 games (94W/14D/32L = 0.721), not 34 (18 vs aggro_macro is right; 1237/313/474 x3, 455/997/25/696/295/554 x2, 1170 x1); in 26_214804 "21.5k vs 23.4k by t150" are full-game totals (t191) - by t150 we were actually ahead (18.4k vs 17.1k), which strengthens the point. |
| O8 | PARTIAL | All numbers reproduce: shares 34.3/35.4/23.9 (aggro_macro), 23.6/38.8/46.7 (macro), 10/6.2/5.6 (hq_rush), 15.7/9.2/12.8 (turtle); HQ L3 by t150 among >= 150-turn games 43.5/26.8/15.2%; opp army at t150 16/13/11; 4-stack by t60 22.9/37.5/31.7%; >= 2 ms 4.3/10/12.2%; score 0.632/0.654/0.714, within aggro_macro 0.333/0.324/0.384, macro 0.788/0.844/0.804; r22-30 re-weighted with the r10-21 archetype mix 0.659; RIGHT 42/76/35; LEFT 0.724/0.787/0.766, RIGHT 0.417/0.368/0.500. Two problems: (a) "more early-aggressive" is not supported - first forward by t25 fell 17.9 -> 11.7 -> 9.4%, hq_rush and early_aggro shares fell, a 4-stack by t45 went 15.7 -> 26.2 -> 18.9% (up then down), only "6 warriors at t20" rose monotonically (9.3 -> 16.2 -> 19.4%); "less eco-heavy" is supported. (b) The archetype is outcome-contaminated (see O1), so a bot that suppresses pushes converts aggro_macro games into macro games and the improvement shows up as "mix". With a t<=100-only archetype the mix explains about half of the rise (re-weighted 0.682 vs actual 0.714 vs 0.654 base) and the within-class score vs ex-ante pushers rose 0.286 -> 0.450 (n=35 -> 10, small); archetype x side re-weighting explains essentially all of it (0.651). The bottom line (the rise is mostly mix + fewer RIGHT games) stands; "no better results against pushers" is uncertain. Also within aggro_macro by side: LEFT 0.467/0.534/0.400 (n 30/44/30), RIGHT 0.111/0.098/0.346 (18/41/13). |
| O9 | CONFIRMED | heavy 9 games 0.556 (3 losses: 474 r10, 474 r15, 554 r29), medium 43 games 0.721, light 508 games 0.665; aggro_macro light 0.324 (153) vs medium 0.500 (21); macro 0.810 vs 0.972; >= 2 ms share 4.3 -> 10 -> 12.2%. Small n for heavy; the direction of the claim (compute is not the discriminator) holds. |
| O10 | CONFIRMED | Both pivots reproduce cell by cell (mean forward <= 0.5: 0.79-0.98 across army buckets, n = 58+120+58+12 = 248 [the claim's n=279 does not match the table's cells]; 1.5-3: 0.17-0.57; > 3: 0.00-0.29; t51-100 ex-ante table identical); worker-heavy 375 games 0.740 vs 0.522; never-forward 73 games 0.952 (1 loss); >= 3 bases by t100 169 games 0.447 (88 L); >= 5 bases destroyed 233 games 0.479 (109 L). |
| O11 | PARTIAL | Archetype x side pivot reproduces exactly (aggro_macro 0.476/0.146, macro 0.877/0.625, early_aggro 0.733/0.318, turtle 0.913/0.833); 313 beat us in all 5 LEFT games, 1167 in 3 of 4 LEFT games, 474 8/8 with us RIGHT. Errors: "1237 once LEFT" - our only LEFT game vs 1237 (r7) was a WIN (the 4 losses + draw were all RIGHT); "squad family 0.115 when LEFT" comes from the outcome-selected set - corrected LEFT 0.429 (n=21) vs RIGHT 0.265 (17), i.e. the squad family is not side-independent either. |
| O12 | PARTIAL | 131 opponents met >= 2 times, 42 constant, 89 switched; 994 (43-army aggro_macro in r5, 9-10-warrior macro in r16-20), 978 (hq_rush -> aggro_macro -> macro -> hq_rush), 25 (macro/turtle, 8 games, 0.688) reproduce. But 52 of the 89 "switches" are only macro <-> aggro_macro flips, which hinge on a whole-game forward-presence threshold that depends on how the game went, and merging macro/aggro_macro/turtle leaves 30 of 131 genuine switchers. The recommendation (detect in-game) is fine; "most repeat opponents switched" is overstated. |

### Extra findings from the verification

1. **Squad-family filter bug** (`opponents_extra.py`, `(X.opp_first_at_enemy_hq_t > 30)`): drops NaN = opponent never at our HQ, so the set is selected on outcome. Corrected: 38 games, 0.355, improving by era (0.0 / 0.342 / 0.500), LEFT 0.429 / RIGHT 0.265. Flag E07 (and E14's label "trained every turn t1-5") inherit the error; the squad-family rows in sections 0, 4.3, 5.2, 5.5 and the O3 implication ("unchanged across versions") should be read with the corrected numbers. The 23 losses themselves are real.
2. **Ex-ante core of aggro_macro is much smaller than 176.** Re-assigning aggro_macro vs macro with t<=100 data only: 60 games (14W/5D/41L, 0.275, 25.5% of losses) keep the label, 117 move to macro. Share by era 10.7 / 14.6 / 5.6%, score within it 0.133 / 0.286 / 0.450. For sparring/validation, the 60-game ex-ante set (and 474/313/1170 specifically) is the sharper target.
3. **HQ-rush defence threshold is 0, not <= 1 defender:** at the turn the rush arrived, 0 defenders -> 6L/0W, 1 -> 3L/9W, 2 -> 1L/8W, 3+ -> 0L/12W (39 games).
4. **Latest bot by side vs pushers:** aggro_macro LEFT 0.467 -> 0.534 -> 0.400 (n 30/44/30) got worse, RIGHT 0.111 -> 0.098 -> 0.346 (18/41/13) got better; the LEFT/RIGHT asymmetry within the archetype narrowed in r22-30.
5. **Max HQ level reached, all games:** losses L1 74 / L2 66 / L3 19 / L4 2; wins L1 4 / L2 16 / L3 39 / L4 79 / L5 211 (outcome-linked, but a clean summary of the "we never get past L2 when we lose" pattern).
6. Minor: r24-30 = 94W/14D/32L (0.721), not 34 losses; "our 17" median peak army in aggro_macro is 19 (15 in losses).
