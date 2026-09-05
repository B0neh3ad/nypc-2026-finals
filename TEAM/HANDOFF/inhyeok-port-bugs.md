# Two defects in the current ports (inhyeok, 2026-08-29)

Not editing anyone's file — reporting so the owners can fix. Both ports are
derived from the qualifier's `v3_2`, confirmed by
`P_OFFENSE_REINF_ARRIVAL_SLACK = {0,0,0,0}` plus the saver/finisher block.

## 1. `v4_0.cpp` — `PREDICT_MAX_ID` is still 512 (WRONG-DECISION, one line)

```
v4_0.cpp:              #define PREDICT_MAX_ID 512
new_rule_baseline.cpp: #define PREDICT_MAX_ID 4096   <- already fixed
```

Enemy warrior IDs are bounds-checked and **silently skipped** above the limit
(`if (w->id.num < 0 || w->id.num >= PREDICT_MAX_ID) return -1;`). It does not
crash — the enemy-movement prediction module just stops seeing those units.

At HQ L5 you train 3/day. Over 400 days IDs run past 512 somewhere around the
midpoint, so **`v4_0` plays the entire back half of every game with enemy
prediction disabled**. Copy sehyeon's 4096.

## 2. Both ports — the MoE routes every final-round map to one expert (TUNING)

```
int e = (N <= 63) ? 0 : (N <= 79) ? 1 : (N <= 93) ? 2 : 3;
```

Final-round N is **181..249**, so every map takes the `else` branch: **expert 3,
always.** Three of the four gene sets are now dead code, and the surviving one
was evolved on N = 94..109 — less than half the size, with 200 days and full
information.

This is the largest tuning gap in both builds. Two options, cheapest first:

1. **Re-cut the buckets** for the real range (measured over 200 generated maps:
   N median 217, quartiles 195 / 217 / 233). Seed all four experts from the
   current expert 3, then let them diverge.
2. Accept one expert for now and re-tune it against a fixed pool — but see
   [../../docs/qualifier/GA-sibling-selfplay-trap.md](../../docs/qualifier/GA-sibling-selfplay-trap.md)
   before pointing any GA at self-play.

Also worth knowing: every turn-valued gene was calibrated for a 200-day game.
`v4_0` handles this with `LATE_TURN_SHIFT (+200)` on endgame-anchored thresholds,
but *opening* genes are unshifted by design — e.g.
`P_OPENING_NEUTRAL_FIRST_MAX_TURN = 123`, so the opening now ends at turn 123 of
400 and the bot plays ~280 turns of midgame it was never tuned for.
