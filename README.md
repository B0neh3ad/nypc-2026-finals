# NYPC 2026 Master Track Finals

Team CPG (Jinsu Kim, Inhyuk Park, Sehyeon Park, Jinyoung Park)

**Bronze Award (Nexon Foundation Chairman's Award), NYPC 2026 Master Track**

---

## 1. Contest Overview

> [NYPC](https://new.nypc.co.kr/ko/) (Nexon Youth Programming Challenge) is a programming contest hosted by [Nexon](https://www.nexon.com/Home/Game).
> The **Master Track** (university students and ages 19-24) was added as a regular
> track in 2026. The task is not algorithmic problem solving but a **game AI strategy
> competition**, entered by teams of up to four.

| Stage | Dates | Format |
|---|---|---|
| Registration | Jun 4 - Jul 8 | Online |
| **Qualification Round** | Jun 29 - Jul 8 (10 days) | Online, NYPC Arena |
| Finalist announcement | Mid-July | - |
| **Final Round** | **Sat Aug 29, 10:00 - 16:00 (6 hours, no break)** | On-site, Seoul |

**Scale and result**

- Qualification Round entries: **1,603 teams**
- Advanced to the Finals: **23 teams**
- Master Track awards, 10 teams total: 1 Winner, 1 Gold, 3 Silver, 5 Bronze
- Result: **Bronze (Nexon Foundation Chairman's Award)**

> Note: this document covers the Finals. The Qualification Round (NEXT NATION,
> genetic-algorithm parameter search) is written up separately on the
> [`qualification-round` branch](../../tree/qualification-round).

### The Finals task: NEXT VISION

A **two-player turn-based strategy game** played on a point-symmetric graph. You
build structures to grow an economy and train warriors to **destroy the enemy HQ**.

- Map: 181 - 249 zones (odd), 13 - 19 strongholds, randomly generated each game
- Victory: within 400 days (turns); otherwise decided by remaining HQ health
- Thinking time: 100 ms per turn, 5 countdown extensions
- **Fog of war**: 2-hop vision, only the area around your own units is visible

Over the six hours, an interim evaluation ran every 15 minutes: a round robin against
all 23 teams, with the standings refreshed each time. In effect the whole contest was
a 15-minute loop of "submit, get scored, check the standings, improve".

---

## 2. Team and Roles

Four of us worked simultaneously in a single shared folder on EC2 (`/srv/nypc`).
There were no branches and no pull requests; conflicts were handled by an automatic
snapshot commit every two minutes.

### Jinsu: infrastructure, measurement, opponent analysis

Rather than writing a bot, I owned the basis on which the whole team could judge
**what had actually improved**.

- **Collaboration environment**: shared EC2 server, account and permission design for
  four members, a toolchain identical to the judge's (gcc 14.2.0 / Boost 1.90.0), and
  a core semaphore that prevented fake timeouts when four people ran jobs at once
- **Match automation**: `match.sh` (n-game aggregation, standard error, side
  alternation, even map-size distribution), `versus.sh`, `roundrobin.sh` (league play
  across a whole folder). A 105-game match finished in 20 seconds, so several
  candidates could be validated inside one 15-minute cycle
- **Automated round collection**: a poller that downloaded and filed round logs,
  results, and submitted code on its own. It is the reason all 17 rounds are recorded
  in `contest/` in this repository
- **Opponent analysis**: reconstructed opponents' command sequences from logs to pin
  down why we lost
- **Sparring partners**: three bots reproducing opposing teams' rush strategies
  (`TEAM/vs/`)
- **Parameter search operations**: ran map-size-specific parameter search on the h200
  cluster (Slurm)

### Inhyuk: main bot development (final submission)

Advanced the primary bot across eight generations, `inhyuk_v1` through `v8`, and
completed the **final submission `inhyuk_v8.cpp`**. He parameterized defensive
judgment, attack timing, and resource allocation into more than 100 constants, so
strategies could be swapped and tested through compile flags alone, without touching
the source.

### Sehyeon: porting qualifier assets, parallel bot line

Early in the contest he produced `new_rule_baseline` by porting the qualifier bot to
the Finals rules (400 days, fog of war), which **gave the team a starting point**. He
then developed the `sehyeon_v1` through `v7` line independently, competing against
Inhyuk's line; in the middle rounds his line was the strongest we had.

### Jinyoung: opponent reproduction, search framework

Built benchmark bots reproducing strong teams' strategies (`babsang`, `v7_dol`), which
let us **reproduce the opponents we could not beat** in self-play. He also built the
map-size parameter search framework (`MoE/`) and developed the `v4_0` and `jyp_v2`
through `v4` bot lines.

---

## 3. Core Ideas in the Final Submission (`inhyuk_v8.cpp`)

**1. Win by expanding, then win by protecting the expansion**

The single metric that decided games was **stronghold count**. Across 22 analyzed
interim-round games, we went **14-0** when ahead on strongholds and 3-3 when behind.
The bot grabs as many strongholds as possible early and staffs each one with labor so
that gold income compounds.

**2. Stop expanding the moment a threat appears**

Expand without limit and your defense empties out and you die. Once the enemy's first
attack is observed, the bot sharply restricts new expansion and switches to troop
production. Releasing that brake in an experiment produced **105 losses in 105 games**,
which says expansion speed was never the bottleneck: the brake was what kept us alive.

**3. Decide HQ defense by simulation**

The bot precomputes "if N enemies arrive in M turns, does the current garrison hold?"
and recalls only as many troops as the gap requires. By never pulling back more than
needed, it keeps offense and economy in balance.

**4. Separate every decision threshold into a constant**

Defense radius, attack-launch troop count, expansion targets, and roughly 100 other
values are exposed as `#define`s, so variants can be produced through compile flags
without editing the source. This design is what made it possible to validate dozens of
variants by automated matches within six hours.

---

## 4. Problems Solved During the Contest

### 1. Reading the Finals rules: "the same engine as the qualifier"

Right after the contest opened, we diffed the Finals referee against the qualifier
referee and confirmed they were **96.2% identical** (1599 vs 1641 lines, 124 changed).
Only two things had changed.

- **Rebalanced constants**: starting gold 500 to 750, 200 days to 400, maps of
  51 - 109 zones to 181 - 249
- **Fog of war introduced**: the qualifier was a perfect-information game in which all
  opponent actions were visible; the Finals imposed a 2-hop vision limit

This let us **start from a port** rather than rewriting the qualifier bot from scratch,
while also making clear exactly where qualifier strategy could no longer be trusted
(scouting, estimating opponent state).

### 2. Pinning down why we lost: the early rush

We were sitting at 15th place in the early rounds. Analyzing 74 games of logs
identified the cause numerically.

| | Games | Win rate |
|---|---|---|
| Rushed | 12 | **33%** |
| Not rushed | 62 | 65% |

Three opposing teams used the identical opening every round: **train warriors on turns
1-5, then send all seven straight at our HQ on turn 6**. The seven spent 19 turns
walking, and because of the 2-hop vision limit they were invisible until just before
arrival. We expanded the whole time and lost our HQ on turn 25.

### 3. Reproducing opponent strategy: turning it into a measurable target

The observation "we are weak to rushes" gives you no way to measure improvement. We
built a **sparring bot that reproduced the opponent's command sequence exactly**,
which turned defensive improvement into something we could judge in minutes.

Reproduction was accurate down to reinforcement training timings (actual T29, 40, 53,
71, 94 / reproduced T27, 38, 51, 68, 92). Not because the timings were hardcoded, but
because **the economy forces them**.

### 4. Catching the rush evolve: the staging tactic

Later in the contest the same teams changed their opening. The destination was no
longer our HQ but a **stronghold near it**; they massed there and charged again from
that point. Because the destination was not the HQ, it slipped past our existing
detection.

Reproducing this variant and measuring defense rate showed a stepwise decline.

| Rush form | Defense rate (105 games) |
|---|---|
| Old, direct | 86.7% ± 3.3%p |
| **New, staged** | **74.8% ± 4.3%p** |
| Staged plus reinforcement | 66.7% ± 4.6%p |

Adding the staging point alone cost 11.9%p. In the actual round where we first met this
tactic, we fell from 3rd to 10th.

### 5. A strong team's winning formula: accounting, not combat

We were on an eight-game losing streak against one of the top teams. Digging into the
logs, the **combat exchange ratio was an even 1:1** (damage dealt 346:350). The
difference was where the money went.

- Them: once fighting started, gold went **100% into troops** and construction spending
  effectively stopped
- Us: we kept pouring repair costs into a base that their wave was actively hitting
  (3,400 gold over 50 turns)

**While we spent 500 gold on a building their next wave would burn down, they trained
four warriors.** It was not a tactical problem but a resource-allocation one.

### 6. The measurement trap

Win-rate comparisons are noisy. Several were overturned during the contest itself.

- A bot that looked "undefeated" over 105 games turned out to lose 2.9% over 210
- The same binary swung between 40% and 54% against the same rush
- Parameter search reported a baseline of 0.536 and a best of 0.889, but
  **re-evaluating on fresh seeds gave 0.623** (the structural bias of picking the
  maximum out of 99 candidates)

So we made it a rule to **report a standard error and confidence interval with every
verdict**, and never to call something an improvement when the intervals overlapped.
Variants that looked good in self-play turned out to be regressions more than once, and
this rule is what **stopped those submissions**.

---

## 5. Interim Round Progression

Seventeen interim evaluations ran over the six hours. Each round is a round robin
against all 23 teams.

| Round | Time | Score | Rank | Record |
|---|---|---|---|---|
| #1 | 11:00 | 1030 | 15th | 2W 2D 11L |
| #2 | 11:15 | 1820 | **2nd** | 13W 5D 1L |
| #3 | 11:30 | 1450 | 12th | 8W 2D 10L |
| #4 | 11:45 | 1890 | 3rd | 14W 2D 4L |
| #7 | 12:30 | 2060 | **2nd** | 18W 1D 3L |
| #8 | 12:45 | 2060 | 3rd | 18W 0D 4L |
| #12 | 13:45 | 1950 | **2nd** | 18W 1D 3L |
| #14 | 14:15 | 2110 | 3rd | 18W 2D 2L |
| #15 | 14:30 | 1990 | 10th | 13W 0D 9L |
| #17 | 15:00 | 1880 | 9th | 13W 2D 7L |

The peak was **2110 points (3rd) at #14**. The collapse at #15 is the round where we
first ran into the **staged rush** described above.

---

## 6. Repository Layout

| Path | Contents |
|---|---|
| `docs/` | Full problem statement, tool usage, collaboration and compilation conventions |
| `problem/` | Organizers' distribution (referee, sample bots in 12 languages) |
| `simulator/` | Static mirror of the replay viewer |
| `tools/` | Match and build tooling, interim-round collection poller |
| `TEAM/` | Bot sources by round, plus the `vs/` sparring panel |
| `contest/` | Logs, results, and per-round submitted code for all 17 interim rounds |
| `MoE/` | Map-size parameter search framework and trial records |

### Reproducing a match

```bash
cd tools
BOT_PATH=../TEAM/round_17_1500:../TEAM/vs ./match.sh 105 inhyuk_v8 sr_reinf --map-sweep -j 6
```

`--map-sweep` distributes games evenly across the 35 map sizes the rules allow and
alternates sides every game, so side advantage does not leak into the win rate. Use a
multiple of 35 for the game count (105, 210, 315).

To watch a replay, serve `simulator/` over HTTP and paste in a log.
