# The GA trap: never rate a build against its own lineage

*Qualifier finding, carried over because `reference/src/` still ships
`ga_moe.py`, `ga_search.py` and `ga_species2.py`. If someone restarts that loop
against sibling builds, it will burn days and return nothing.*

## What happened

We rated candidates by self-play against their own parent. The fitness signal
was **exactly zero**, and it took a long time to notice because the runs looked
healthy — games completed, scores came back, generations advanced.

Measured:

- **320 of 320 sibling games were exact-mirror draws** — byte-identical replays.
- **84 mutated parameters changed nothing across 480 games.**
- Of a 40-game panel, roughly **2.4 games carried any information at all.**

## Why

The build had a "centre-split" opening whose gate returns from `decide()` before
every economy, upgrade and attack module. Two identical bots both contest the
centre, feed one warrior at a time into a duel that always trades evenly, the
empty centre re-arms both state machines, and the loop runs all 200 turns. Since
both sides are the same program on a point-symmetric map, every decision is
mirrored and the game is deterministic — so mutating a parameter that the gate
never reaches cannot change the outcome.

A genetic algorithm optimising that fitness is **optimising a constant.**

## The rule that replaced it

Rate candidates against a **fixed, diverse pool that does not share the
candidate's assumptions** — and never against the candidate's own parent.
Sibling builds are still useful, but only as a *sensitivity detector*: because
they draw by default, any behavioural change at all breaks the draw, which makes
them good at answering "did this change anything?" and useless at answering
"is this better?".

See [../EVAL-PROTOCOL.md](../EVAL-PROTOCOL.md) for the acceptance rule.

## Does this still apply under the new rules?

**The mechanism is not automatically gone.** Fog of war and the 400-day limit
change the dynamics, but the centre-split gate still exists in the ported builds
and the map is still point-symmetric. Before trusting any self-play number,
check the cheap tell: run a build against itself on a few seeds and look at the
result lines. If they are all `DRAW` with identical logs, you have no gradient.
