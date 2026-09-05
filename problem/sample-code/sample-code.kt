import kotlin.math.ceil
import kotlin.math.min
import kotlin.math.sqrt
import kotlin.system.exitProcess

const val MAX_TURN = 400          // maximum turn (days)
const val START_GOLD = 750        // initial gold
const val START_WARRIORS = 3      // initial warriors
const val MOVE_COST = 10          // move cost
const val TRAIN_COST = 120        // train cost
const val WORK_INCOME = 15        // income per warrior
const val UPKEEP_PER_WARRIOR = 2  // upkeep per warrior
const val HQ_MAX_LEVEL = 5        // HQ max level
const val BASE_MAX_LEVEL = 3      // base max level
const val HQ_HEAL_COST = 1000     // HQ fix cost
const val BASE_HEAL_COST = 500    // base fix cost
const val HOP_VISION = 2          // vision radius shared by all units

data class HqLevelEntry(
    val upgradeCost: Int,
    val warriorHp: Int,
    val hp: Int,
    val turret: Int,
    val trainCap: Int,
    val workCap: Int,
)

data class BaseLevelEntry(
    val cost: Int,
    val hp: Int,
    val turret: Int,
    val workCap: Int,
)

val HQ_LEVELS = arrayOf(
    HqLevelEntry(0,    0,  0,  0, 0, 0),
    HqLevelEntry(0,    4, 10,  1, 1, 1),
    HqLevelEntry(600,  5, 15,  2, 1, 2),
    HqLevelEntry(1000, 6, 20,  2, 2, 3),
    HqLevelEntry(2000, 7, 25,  3, 2, 4),
    HqLevelEntry(3000, 8, 30,  3, 3, 5),
)
val BASE_LEVELS = arrayOf(
    BaseLevelEntry(0,    0, 0, 0),
    BaseLevelEntry(500,  6, 1, 1),
    BaseLevelEntry(550, 12, 1, 2),
    BaseLevelEntry(600, 18, 2, 3),
)

enum class Side { LEFT, RIGHT }
enum class BType { HQ, BASE }
enum class WState { STATIONARY, MOVING }

fun Side.opposite(): Side = if (this == Side.LEFT) Side.RIGHT else Side.LEFT
fun Side.toChar(): Char = if (this == Side.LEFT) 'A' else 'B'
fun parseSideChar(c: Char): Side = if (c == 'A') Side.LEFT else Side.RIGHT
fun parseSideWord(w: String): Side = if (w == "LEFT") Side.LEFT else Side.RIGHT

data class WarriorId(val side: Side, val num: Int) {
    override fun toString(): String = "${side.toChar()}$num"
    companion object {
        fun parse(tok: String): WarriorId {
            require(tok.isNotEmpty() && (tok[0] == 'A' || tok[0] == 'B'))
            return WarriorId(parseSideChar(tok[0]), tok.substring(1).toInt())
        }
    }
}

data class Warrior(
    val id: WarriorId,
    var region: Int,
    var hp: Int,
    var state: WState = WState.STATIONARY,
    var target: Int = 0,
)

data class Building(
    val region: Int,
    val side: Side,
    val type: BType,
    var level: Int = 1,
    var hp: Int = 10,
) {
    fun currentHp(): Int = if (type == BType.HQ) HQ_LEVELS[level].hp else BASE_LEVELS[level].hp
    fun workCap(): Int = if (type == BType.HQ) HQ_LEVELS[level].workCap else BASE_LEVELS[level].workCap
    fun applyUpgrade() {
        level += 1
        hp = currentHp()
    }
    fun upgradeCost(): Int = if (type == BType.HQ) HQ_LEVELS[level + 1].upgradeCost else BASE_LEVELS[level + 1].cost
    fun maxLevel(): Int = if (type == BType.HQ) HQ_MAX_LEVEL else BASE_MAX_LEVEL
}

fun makeBase(region: Int, s: Side) = Building(region, s, BType.BASE, 1, BASE_LEVELS[1].hp)

fun computeVisible(S: GameState, M: GameMap): List<Int> {
    val visible = mutableSetOf<Int>()
    fun addHops(start: Int, radius: Int) {
        val seen = mutableSetOf(start)
        var frontier = listOf(start)
        repeat(radius) {
            val next = mutableListOf<Int>()
            for (region in frontier) {
                for (neighbor in M.adj[region]) {
                    if (seen.add(neighbor)) next.add(neighbor)
                }
            }
            frontier = next
        }
        visible.addAll(seen)
    }
    for (w in S.warriors)
        if (w.id.side == M.mySide) addHops(w.region, HOP_VISION)
    for (b in S.buildings)
        if (b.side == M.mySide) addHops(b.region, HOP_VISION)
    return visible.sorted()
}

class GameMap(
    val n: Int,
    val k: Int,
    val x: LongArray,
    val y: LongArray,
    val strongholds: List<Int>,
    val adj: List<List<Int>>,
    val mySide: Side,
) {
    fun hqOf(s: Side): Int = if (s == Side.LEFT) 0 else n - 1
    val myHq: Int = hqOf(mySide)
    val oppHq: Int = hqOf(mySide.opposite())
}

class GameState(
    var gold: Int = START_GOLD,
    var myCountdown: Int = 5,
    var oppCountdown: Int = 5,
    val warriors: MutableList<Warrior> = mutableListOf(),
    val buildings: MutableList<Building> = mutableListOf(),
    var visible: List<Int> = emptyList(),
) {
    fun findBuilding(region: Int): Building? = buildings.firstOrNull { it.region == region }
    fun findWarrior(id: WarriorId): Warrior? = warriors.firstOrNull { it.id == id }
}

data class Actions(
    var trainN: Int = 0,
    val moves: MutableList<Pair<WarriorId, Int>> = mutableListOf(),
    val upgrades: MutableList<Int> = mutableListOf(),
)

class Paths(
    val dist: Array<DoubleArray>,
    val nxt: Array<IntArray>,
)

private val reader = System.`in`.bufferedReader()
private val writer = System.out.bufferedWriter()

private fun readln(): String = reader.readLine() ?: exitProcess(0)

private fun readTokens(): List<String> = readln().trim().split(" ").filter { it.isNotEmpty() }

private fun writeln(s: String) = writer.write("$s\n")
private fun flush() = writer.flush()

fun parseInit(): Pair<GameMap, GameState> {
    var t = readTokens()
    require(t.size >= 2 && t[0] == "READY")
    val mySide = parseSideWord(t[1])

    t = readTokens()
    val n = t[0].toInt()
    val k = t[1].toInt()

    val x = readTokens().map { it.toLong() }.toLongArray() // x_0 x_1 ... x_{N-1}
    val y = readTokens().map { it.toLong() }.toLongArray() // y_0 y_1 ... y_{N-1}

    val strongholds = readTokens().map { it.toInt() }.sorted() // K strongholds

    val adj = (0 until n).map {
        val rt = readTokens() // deg n_1 n_2 ...
        val deg = rt[0].toInt()
        (1..deg).map { i -> rt[i].toInt() }.sorted()
    }

    val M = GameMap(n, k, x, y, strongholds, adj, mySide)

    val opp = mySide.opposite()
    val S = GameState()
    for (sfx in 1..START_WARRIORS) {
        S.warriors.add(Warrior(WarriorId(mySide, sfx), M.myHq, HQ_LEVELS[1].warriorHp))
        S.warriors.add(Warrior(WarriorId(opp, sfx), M.oppHq, HQ_LEVELS[1].warriorHp))
    }
    S.buildings.add(Building(0, Side.LEFT, BType.HQ, 1, HQ_LEVELS[1].hp))
    S.buildings.add(Building(n - 1, Side.RIGHT, BType.HQ, 1, HQ_LEVELS[1].hp))

    writeln("OK")
    flush()
    return M to S
}

fun readTurnStart(): Int? {
    val line = readln()
    if (line == "FINISH") return null
    val t = line.trim().split(" ")
    require(t.isNotEmpty() && t[0] == "START")
    return t[2].toInt()
}

fun readTurnResult(S: GameState, M: GameMap, submitted: Actions) {
    for (region in submitted.upgrades) {
        val b = S.findBuilding(region)
        if (b == null) {
            S.gold -= BASE_LEVELS[1].cost
        } else {
            if (b.level >= b.maxLevel()) {
                val cost = if (b.type == BType.HQ) HQ_HEAL_COST else BASE_HEAL_COST
                S.gold -= cost
                b.hp = b.currentHp()
            } else {
                S.gold -= b.upgradeCost()
                b.applyUpgrade()
            }
        }
    }

    val built = submitted.upgrades.toSet()

    for ((id, target) in submitted.moves) {
        val b = S.findBuilding(target)
        val own = target in built || (b != null && b.side == M.mySide)
        val cost = if (own) 0 else MOVE_COST
        S.gold -= cost
        S.findWarrior(id)?.let { w ->
            w.state = WState.MOVING
            w.target = target
        }
    }

    S.gold -= TRAIN_COST * submitted.trainN

    val line = readln()
    if (line == "FINISH") exitProcess(0)
    val tTurn = line.trim().split(" ")
    require(tTurn.isNotEmpty() && tTurn[0] == "TURN")

    val tTime = readTokens()
    S.myCountdown = tTime[2].toInt()
    S.oppCountdown = tTime[4].toInt()

    // UPGRADE
    val tUpgrade = readTokens() // "UPGRADE N"
    val upgradeN = tUpgrade[1].toInt()
    for (i in 0 until upgradeN) {
        val r = readTokens() // "<A|B> <region>"
        val region = r[1].toInt()
        val b = S.findBuilding(region)
        if (b == null) {
            S.buildings.add(makeBase(region, M.mySide))
        } else if (b.level >= b.maxLevel()) {
            b.hp = b.currentHp()
        } else {
            b.applyUpgrade()
        }
    }

    // TRAIN
    val tTrain = readTokens() // "TRAIN N"
    val trainN = tTrain[1].toInt()
    if (trainN > 0) {
        val ids = readTokens()
        for (i in 0 until trainN) {
            val wid = WarriorId.parse(ids[i])
            val hqB = S.findBuilding(M.myHq)
            val hqLevel = hqB?.level ?: 1
            S.warriors.add(Warrior(wid, M.myHq, HQ_LEVELS[hqLevel].warriorHp))
        }
    }

    // MOVE
    val tMove = readTokens() // "MOVE N"
    val moveN = tMove[1].toInt()
    for (i in 0 until moveN) {
        val r = readTokens()
        val wid = WarriorId.parse(r[0])
        val region = r[1].toInt()
        S.findWarrior(wid)?.let { w ->
            w.region = region
            if (w.state == WState.MOVING && w.region == w.target) {
                w.state = WState.STATIONARY
            }
        }
    }

    // DAMAGE
    val starved = mutableListOf<Pair<WarriorId, Int>>()
    val tDamage = readTokens() // "DAMAGE N"
    val damageN = tDamage[1].toInt()
    for (i in 0 until damageN) {
        val r = readTokens() // "<cause> <id> <damage>"
        val wid = WarriorId.parse(r[1])
        val damage = r[2].toInt()
        if (r[0] == "HUNGER") {
            starved.add(wid to damage)
            continue
        }
        S.findWarrior(wid)?.let { it.hp -= damage }
    }
    S.warriors.removeAll { it.hp <= 0 }

    // SIEGE
    val tSiege = readTokens() // "SIEGE N"
    val siegeN = tSiege[1].toInt()
    for (i in 0 until siegeN) {
        val r = readTokens()
        val region = r[1].toInt()
        val damage = r[2].toInt()
        S.findBuilding(region)?.let { it.hp -= damage }
    }
    S.buildings.removeAll { it.hp <= 0 }

    S.visible = computeVisible(S, M)

    // WARRIOR
    val tWarrior = readTokens() // "WARRIOR W"
    val warriorW = tWarrior[1].toInt()
    val seenWarriors = mutableListOf<Warrior>()
    for (i in 0 until warriorW) {
        val r = readTokens() // "<id> <region> <hp>"
        val wid = WarriorId.parse(r[0])
        if (wid.side == M.mySide) continue
        seenWarriors.add(Warrior(wid, r[1].toInt(), r[2].toInt()))
    }
    S.warriors.removeAll { it.id.side != M.mySide }
    S.warriors.addAll(seenWarriors)

    // BUILDING
    val tBuilding = readTokens() // "BUILDING B"
    val buildingB = tBuilding[1].toInt()
    val seenBuildings = mutableListOf<Building>()
    for (i in 0 until buildingB) {
        val r = readTokens() // "<side> <region> <kind> <level> <hp>"
        val side = parseSideChar(r[0][0])
        if (side == M.mySide) continue
        val btype = if (r[2] == "HQ") BType.HQ else BType.BASE
        seenBuildings.add(Building(r[1].toInt(), side, btype, r[3].toInt(), r[4].toInt()))
    }
    S.buildings.removeAll { it.side != M.mySide }
    S.buildings.addAll(seenBuildings)

    readln() // "END"

    S.gold += S.buildings
        .filter { it.side == M.mySide }
        .sumOf { b ->
            val count = S.warriors.count { it.id.side == M.mySide && it.region == b.region }
            WORK_INCOME * min(count, b.workCap())
        }

    val alive = S.warriors.count { it.id.side == M.mySide }
    S.gold -= UPKEEP_PER_WARRIOR * min(alive, S.gold / UPKEEP_PER_WARRIOR)

    for ((wid, damage) in starved) {
        S.findWarrior(wid)?.let { it.hp -= damage }
    }
    S.warriors.removeAll { it.hp <= 0 }
}

fun euclidCeil(M: GameMap, u: Int, v: Int): Double {
    val dx = (M.x[u] - M.x[v]).toDouble()
    val dy = (M.y[u] - M.y[v]).toDouble()
    return ceil(sqrt(dx * dx + dy * dy))
}

fun calculatePaths(M: GameMap): Paths {
    val n = M.n
    val dist = Array(n) { DoubleArray(n) { Double.POSITIVE_INFINITY } }
    val nxt = Array(n) { IntArray(n) { -1 } }

    for (i in 0 until n) {
        dist[i][i] = 0.0
        nxt[i][i] = i
    }
    for (u in 0 until n) {
        for (v in M.adj[u]) {
            val w = euclidCeil(M, u, v)
            if (w < dist[u][v]) dist[u][v] = w
        }
    }

    for (k in 0 until n) {
        for (u in 0 until n) {
            if (dist[u][k] == Double.POSITIVE_INFINITY) continue
            for (v in 0 until n) {
                val cand = dist[u][k] + dist[k][v]
                if (cand < dist[u][v]) dist[u][v] = cand
            }
        }
    }

    for (u in 0 until n) {
        for (v in 0 until n) {
            if (u == v || dist[u][v] == Double.POSITIVE_INFINITY) continue
            var bestScore = Double.POSITIVE_INFINITY
            for (nb in M.adj[u]) {
                if (dist[nb][v] == Double.POSITIVE_INFINITY) continue
                val score = euclidCeil(M, u, nb) + dist[nb][v]
                if (score < bestScore) {
                    bestScore = score
                    nxt[u][v] = nb
                }
            }
        }
    }
    return Paths(dist, nxt)
}

fun nextStep(P: Paths, u: Int, v: Int): Int = P.nxt[u][v]

fun path(P: Paths, u: Int, v: Int): List<Int> {
    if (P.nxt[u][v] == -1) return emptyList()
    val out = mutableListOf(u)
    var cur = u
    while (cur != v) {
        cur = P.nxt[cur][v]
        out.add(cur)
    }
    return out
}

fun emitActions(a: Actions) {
    writeln("COMMAND")
    for ((id, target) in a.moves) writeln("MOVE $id $target")
    for (r in a.upgrades) writeln("UPGRADE $r")
    if (a.trainN > 0) writeln("TRAIN ${a.trainN}")
    writeln("END")
    flush()
}

//////////////////////////////////
//// WRITE YOUR STRATEGY HERE ////
//////////////////////////////////
fun decide(S: GameState, M: GameMap, P: Paths, turn: Int): Actions {
    val a = Actions()
    if (turn == 1) {
        for (w in S.warriors) {
            if (w.id.side != M.mySide) continue
            a.moves.add(w.id to M.oppHq)
        }
    }
    return a
}

fun main() {
    val (M, S) = parseInit()
    val P = calculatePaths(M)

    while (true) {
        val turn = readTurnStart() ?: break
        val a = decide(S, M, P, turn)
        emitActions(a)
        readTurnResult(S, M, a)
    }
}
