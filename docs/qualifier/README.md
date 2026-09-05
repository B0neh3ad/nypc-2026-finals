# qualifier/ — carried over from NEXT NATION (team 141)

Background from the qualifier, kept because the final round is *the same game
with new constants plus fog of war*. Read these as **evidence, not doctrine** —
several qualifier conclusions are explicitly dead under the new rules.

| file | what it is | still true? |
|---|---|---|
| [GA-sibling-selfplay-trap.md](GA-sibling-selfplay-trap.md) | Why self-play against your own lineage returns zero signal | **Yes** — mechanism still present in the ported builds |
| [v3-changelog.md](v3-changelog.md) | Evidence trail for the v3 line: what each patch was worth, and the `83616` regression we introduced and fixed | Measurements are qualifier-era; the **lessons** transfer |
| [opponents.md](opponents.md) | Playbooks of the teams that beat us (474, 313, 455, 1170) | **Yes** — those teams are in this final too |

## What is dead under the new rules

- **Winning on HQ-HP margin.** 267 of our 349 qualifier wins were turn-limit wins
  on HQ level. 400 days plus cheaper HQ upgrades (6600 vs 7800) means both good
  bots reach L5/30 HP and those games become **draws**.
- **Spamming cheap L1 bases.** Gold per labour slot went from 300 -> 450 -> 633
  (width wins) to **500 -> 525 -> 550** (flat). We used to finish on 11-15 bases;
  that is now the wrong shape.

Measured on 200 generated final-round maps: N median 217, K median 17, **HQ-to-HQ
15 hops** (was 9), and **you see 5.9% of the map on turn 1**.
