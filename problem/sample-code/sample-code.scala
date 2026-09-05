import scala.collection.mutable
import scala.io.StdIn

val MAX_TURN           = 400  // maximum turn (days)
val START_GOLD         = 750  // initial gold
val START_WARRIORS     = 3    // initial warriors
val MOVE_COST          = 10   // move cost
val TRAIN_COST         = 120  // train cost
val WORK_INCOME        = 15   // income per warrior
val UPKEEP_PER_WARRIOR = 2    // upkeep per warrior
val HQ_MAX_LEVEL       = 5    // HQ max level
val BASE_MAX_LEVEL     = 3    // base max level
val HQ_HEAL_COST       = 1000 // HQ fix cost
val BASE_HEAL_COST     = 500  // base fix cost
val HOP_VISION         = 2    // vision radius shared by all units

case class HqLevelEntry(upgrade_cost: Int, warrior_hp: Int, hp: Int, turret: Int, train_cap: Int, work_cap: Int)
case class BaseLevelEntry(cost: Int, hp: Int, turret: Int, work_cap: Int)

val HQ_LEVELS: Array[HqLevelEntry] = Array(
  HqLevelEntry(0,    0, 0,  0, 0, 0),
  HqLevelEntry(0,    4, 10, 1, 1, 1),
  HqLevelEntry(600,  5, 15, 2, 1, 2),
  HqLevelEntry(1000, 6, 20, 2, 2, 3),
  HqLevelEntry(2000, 7, 25, 3, 2, 4),
  HqLevelEntry(3000, 8, 30, 3, 3, 5),
)
val BASE_LEVELS: Array[BaseLevelEntry] = Array(
  BaseLevelEntry(0,    0,  0, 0),
  BaseLevelEntry(500,  6,  1, 1),
  BaseLevelEntry(550, 12, 1, 2),
  BaseLevelEntry(600, 18, 2, 3),
)

enum Side:
  case LEFT, RIGHT

  def opposite: Side = if this == LEFT then RIGHT else LEFT
  def char: Char     = if this == LEFT then 'A' else 'B'

object Side:
  def fromWord(w: String): Side = if w == "LEFT" then LEFT else RIGHT
  def fromChar(c: Char): Side   = if c == 'A' then LEFT else RIGHT

enum BType:
  case HQ, BASE

enum WState:
  case STATIONARY, MOVING

case class WarriorId(side: Side, num: Int):
  override def toString: String = s"${side.char}$num"

object WarriorId:
  def parse(tok: String): WarriorId =
    require(tok.nonEmpty && (tok(0) == 'A' || tok(0) == 'B'))
    WarriorId(Side.fromChar(tok(0)), tok.substring(1).toInt)

class Warrior(
  val id: WarriorId,
  var region: Int,
  var hp: Int,
  var state: WState = WState.STATIONARY,
  var target: Int = 0
)

class Building(
  val region: Int,
  val side: Side,
  val btype: BType,
  var level: Int = 1,
  var hp: Int = 10
):
  def currentHp: Int    = if btype == BType.HQ then HQ_LEVELS(level).hp else BASE_LEVELS(level).hp
  def workCap: Int      = if btype == BType.HQ then HQ_LEVELS(level).work_cap else BASE_LEVELS(level).work_cap
  def maxLevel: Int     = if btype == BType.HQ then HQ_MAX_LEVEL else BASE_MAX_LEVEL
  def upgradeCost: Int  =
    if btype == BType.HQ then HQ_LEVELS(level + 1).upgrade_cost
    else BASE_LEVELS(level + 1).cost
  def applyUpgrade(): Unit =
    level += 1
    hp = currentHp

class GameMap:
  var N: Int = 0
  var K: Int = 0
  var x: Array[Long] = Array.empty
  var y: Array[Long] = Array.empty
  var strongholds: Array[Int] = Array.empty
  var adj: Array[Array[Int]] = Array.empty
  var mySide: Side = Side.LEFT
  var myHq: Int = 0
  var oppHq: Int = 0

  def hqOf(s: Side): Int = if s == Side.LEFT then 0 else N - 1

class GameState:
  var gold: Int = START_GOLD
  var myCountdown: Int = 5
  var oppCountdown: Int = 5
  val warriors: mutable.ArrayBuffer[Warrior] = mutable.ArrayBuffer.empty
  val buildings: mutable.ArrayBuffer[Building] = mutable.ArrayBuffer.empty
  var visible: Array[Int] = Array.empty

  def findBuilding(region: Int): Option[Building] = buildings.find(_.region == region)
  def findWarrior(id: WarriorId): Option[Warrior]  = warriors.find(_.id == id)

class Actions:
  var trainN: Int = 0
  val moves: mutable.ArrayBuffer[(WarriorId, Int)] = mutable.ArrayBuffer.empty
  val upgrades: mutable.ArrayBuffer[Int] = mutable.ArrayBuffer.empty

def makeBase(region: Int, s: Side): Building =
  Building(region, s, BType.BASE, 1, BASE_LEVELS(1).hp)

def computeVisible(S: GameState, M: GameMap): Array[Int] =
  val visible = mutable.Set.empty[Int]
  def addHops(start: Int, radius: Int): Unit =
    val seen = mutable.Set(start)
    var frontier = Seq(start)
    for _ <- 0 until radius do
      val next = mutable.ArrayBuffer.empty[Int]
      for region <- frontier; neighbor <- M.adj(region) do
        if seen.add(neighbor) then next += neighbor
      frontier = next.toSeq
    visible ++= seen

  for w <- S.warriors if w.id.side == M.mySide do
    addHops(w.region, HOP_VISION)
  for b <- S.buildings if b.side == M.mySide do
    addHops(b.region, HOP_VISION)
  visible.toArray.sorted

def readln(): String =
  val line = StdIn.readLine()
  if line == null then sys.exit(0)
  line

def readTokens(): Array[String] = readln().trim.split("\\s+")

def euclidCeil(M: GameMap, u: Int, v: Int): Double =
  val dx = (M.x(u) - M.x(v)).toDouble
  val dy = (M.y(u) - M.y(v)).toDouble
  math.ceil(math.sqrt(dx * dx + dy * dy))

case class Paths(dist: Array[Array[Double]], nxt: Array[Array[Int]])

def calculatePaths(M: GameMap): Paths =
  val n = M.N
  val INF = Double.PositiveInfinity
  val dist = Array.fill(n, n)(INF)
  val nxt  = Array.fill(n, n)(-1)

  for i <- 0 until n do
    dist(i)(i) = 0.0
    nxt(i)(i) = i

  for u <- 0 until n do
    for v <- M.adj(u) do
      val w = euclidCeil(M, u, v)
      if w < dist(u)(v) then dist(u)(v) = w

  for k <- 0 until n do
    for u <- 0 until n do
      if dist(u)(k) != INF then
        for v <- 0 until n do
          val cand = dist(u)(k) + dist(k)(v)
          if cand < dist(u)(v) then dist(u)(v) = cand

  for u <- 0 until n do
    for v <- 0 until n do
      if u != v && dist(u)(v) != INF then
        M.adj(u)
          .filter(nb => dist(nb)(v) != INF)
          .minByOption(nb => euclidCeil(M, u, nb) + dist(nb)(v))
          .foreach(nb => nxt(u)(v) = nb)

  Paths(dist, nxt)

def nextStep(P: Paths, u: Int, v: Int): Int = P.nxt(u)(v)

def path(P: Paths, u: Int, v: Int): List[Int] =
  if P.nxt(u)(v) == -1 then List.empty
  else
    val out = mutable.ArrayBuffer(u)
    var cur = u
    while cur != v do
      cur = P.nxt(cur)(v)
      out += cur
    out.toList

def parseInit(): (GameMap, GameState) =
  val M = new GameMap()

  val t0 = readTokens()
  require(t0.length >= 2 && t0(0) == "READY")
  M.mySide = Side.fromWord(t0(1))

  val t1 = readTokens()
  M.N = t1(0).toInt
  M.K = t1(1).toInt

  M.x = readTokens().map(_.toLong) // x_0 x_1 ... x_{N-1}
  M.y = readTokens().map(_.toLong) // y_0 y_1 ... y_{N-1}

  M.strongholds = readTokens().map(_.toInt).sorted // K strongholds

  M.adj = Array.tabulate(M.N) { _ =>
    val t = readTokens() // deg n_1 n_2 ...
    val deg = t(0).toInt
    t.slice(1, 1 + deg).map(_.toInt).sorted
  }

  M.myHq  = M.hqOf(M.mySide)
  M.oppHq = M.hqOf(M.mySide.opposite)

  val S = new GameState()
  val opp = M.mySide.opposite
  for sfx <- 1 to START_WARRIORS do
    S.warriors += Warrior(WarriorId(M.mySide, sfx), M.myHq,  HQ_LEVELS(1).warrior_hp)
    S.warriors += Warrior(WarriorId(opp,       sfx), M.oppHq, HQ_LEVELS(1).warrior_hp)
  S.buildings += Building(0,       Side.LEFT,  BType.HQ, 1, HQ_LEVELS(1).hp)
  S.buildings += Building(M.N - 1, Side.RIGHT, BType.HQ, 1, HQ_LEVELS(1).hp)

  println("OK")
  Console.out.flush()
  (M, S)

def readTurnStart(): Option[Int] =
  val line = readln()
  if line == "FINISH" then None
  else
    val t = line.trim.split("\\s+")
    require(t.nonEmpty && t(0) == "START")
    Some(t(2).toInt)

def readTurnResult(S: GameState, M: GameMap, submitted: Actions): Unit =
  for region <- submitted.upgrades do
    S.findBuilding(region) match
      case None =>
        S.gold -= BASE_LEVELS(1).cost
      case Some(b) =>
        if b.level >= b.maxLevel then
          val cost = if b.btype == BType.HQ then HQ_HEAL_COST else BASE_HEAL_COST
          S.gold -= cost
          b.hp = b.currentHp
        else
          S.gold -= b.upgradeCost
          b.applyUpgrade()

  val built = submitted.upgrades.toSet

  for (wid, tgt) <- submitted.moves do
    val own = built.contains(tgt) || S.findBuilding(tgt).exists(_.side == M.mySide)
    val cost = if own then 0 else MOVE_COST
    S.gold -= cost
    S.findWarrior(wid).foreach { w =>
      w.state = WState.MOVING
      w.target = tgt
    }

  S.gold -= TRAIN_COST * submitted.trainN

  val firstLine = readln()
  if firstLine == "FINISH" then sys.exit(0)
  val t0 = firstLine.trim.split("\\s+")
  require(t0.nonEmpty && t0(0) == "TURN")

  val t1 = readTokens()
  S.myCountdown  = t1(2).toInt
  S.oppCountdown = t1(4).toInt

  // UPGRADE
  val tUpg = readTokens() // "UPGRADE N"
  val upgN = tUpg(1).toInt
  for _ <- 0 until upgN do
    val r = readTokens() // "<A|B> <region>"
    val region = r(1).toInt
    S.findBuilding(region) match
      case None    => S.buildings += makeBase(region, M.mySide)
      case Some(b) =>
        if b.level >= b.maxLevel then b.hp = b.currentHp
        else b.applyUpgrade()

  // TRAIN
  val tTrain = readTokens() // "TRAIN N"
  val trainN = tTrain(1).toInt
  if trainN > 0 then
    val ids = readTokens()
    for i <- 0 until trainN do
      val wid     = WarriorId.parse(ids(i))
      val hqLevel = S.findBuilding(M.myHq).map(_.level).getOrElse(1)
      S.warriors += Warrior(wid, M.myHq, HQ_LEVELS(hqLevel).warrior_hp)

  // MOVE
  val tMov = readTokens() // "MOVE N"
  val movN = tMov(1).toInt
  for _ <- 0 until movN do
    val r      = readTokens()
    val wid    = WarriorId.parse(r(0))
    val region = r(1).toInt
    S.findWarrior(wid).foreach { w =>
      w.region = region
      if w.state == WState.MOVING && w.region == w.target then
        w.state = WState.STATIONARY
    }

  // DAMAGE
  val starved = mutable.ArrayBuffer.empty[(WarriorId, Int)]
  val tDmg = readTokens() // "DAMAGE N"
  val dmgN = tDmg(1).toInt
  for _ <- 0 until dmgN do
    val r      = readTokens() // "<cause> <id> <damage>"
    val wid    = WarriorId.parse(r(1))
    val damage = r(2).toInt
    if r(0) == "HUNGER" then starved += ((wid, damage))
    else S.findWarrior(wid).foreach(w => w.hp -= damage)
  S.warriors.filterInPlace(_.hp > 0)

  // SIEGE
  val tSiege = readTokens() // "SIEGE N"
  val siegeN = tSiege(1).toInt
  for _ <- 0 until siegeN do
    val r      = readTokens()
    val region = r(1).toInt
    val damage = r(2).toInt
    S.findBuilding(region).foreach(b => b.hp -= damage)
  S.buildings.filterInPlace(_.hp > 0)

  S.visible = computeVisible(S, M)

  // WARRIOR
  val tWar = readTokens() // "WARRIOR W"
  val warW = tWar(1).toInt
  val seenWarriors = mutable.ArrayBuffer.empty[Warrior]
  for _ <- 0 until warW do
    val r   = readTokens() // "<id> <region> <hp>"
    val wid = WarriorId.parse(r(0))
    if wid.side != M.mySide then
      seenWarriors += Warrior(wid, r(1).toInt, r(2).toInt)
  S.warriors.filterInPlace(_.id.side == M.mySide)
  S.warriors ++= seenWarriors

  // BUILDING
  val tBld = readTokens() // "BUILDING B"
  val bldB = tBld(1).toInt
  val seenBuildings = mutable.ArrayBuffer.empty[Building]
  for _ <- 0 until bldB do
    val r    = readTokens() // "<side> <region> <kind> <level> <hp>"
    val side = Side.fromChar(r(0)(0))
    if side != M.mySide then
      val btype = if r(2) == "HQ" then BType.HQ else BType.BASE
      seenBuildings += Building(r(1).toInt, side, btype, r(3).toInt, r(4).toInt)
  S.buildings.filterInPlace(_.side == M.mySide)
  S.buildings ++= seenBuildings

  readln() // "END"

  val income = S.buildings
    .filter(_.side == M.mySide)
    .map(b => WORK_INCOME * math.min(S.warriors.count(w => w.id.side == M.mySide && w.region == b.region), b.workCap))
    .sum
  S.gold += income

  val alive = S.warriors.count(_.id.side == M.mySide)
  S.gold -= UPKEEP_PER_WARRIOR * math.min(alive, S.gold / UPKEEP_PER_WARRIOR)

  for (wid, damage) <- starved do
    S.findWarrior(wid).foreach(w => w.hp -= damage)
  S.warriors.filterInPlace(_.hp > 0)

//////////////////////////////////
//// WRITE YOUR STRATEGY HERE ////
//////////////////////////////////
def decide(S: GameState, M: GameMap, P: Paths, turn: Int): Actions =
  val a = Actions()
  if turn == 1 then
    for w <- S.warriors if w.id.side == M.mySide do
      a.moves += ((w.id, M.oppHq))
  a

def emitCommand(): Unit = print("COMMAND\n")

def emitActions(a: Actions): Unit =
  for (wid, tgt) <- a.moves do
    print(s"MOVE $wid $tgt\n")
  for r <- a.upgrades do
    print(s"UPGRADE $r\n")
  if a.trainN > 0 then
    print(s"TRAIN ${a.trainN}\n")

def emitEnd(): Unit =
  print("END\n")
  Console.out.flush()

@main def main(): Unit =
  val (gmap, state) = parseInit()
  val paths = calculatePaths(gmap)

  Iterator.continually(readTurnStart())
    .takeWhile(_.isDefined)
    .flatten
    .foreach { turn =>
      val a = decide(state, gmap, paths, turn)
      emitCommand()
      emitActions(a)
      emitEnd()
      readTurnResult(state, gmap, a)
    }
