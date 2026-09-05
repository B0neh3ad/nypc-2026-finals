import java.io.BufferedReader;
import java.io.InputStreamReader;
import java.io.PrintWriter;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashSet;
import java.util.List;
import java.util.OptionalInt;
import java.util.Set;

public class Main {

    // maximum turn (days)
    static final int MAX_TURN = 400;
    // initial gold
    static final int START_GOLD = 750;
    // initial warriors
    static final int START_WARRIORS = 3;
    // move cost
    static final int MOVE_COST = 10;
    // train cost
    static final int TRAIN_COST = 120;
    // income per warrior
    static final int WORK_INCOME = 15;
    // upkeep per warrior
    static final int UPKEEP_PER_WARRIOR = 2;
    // HQ max level
    static final int HQ_MAX_LEVEL = 5;
    // base max level
    static final int BASE_MAX_LEVEL = 3;
    // HQ fix cost
    static final int HQ_HEAL_COST = 1000;
    // base fix cost
    static final int BASE_HEAL_COST = 500;
    // vision radius shared by all units
    static final int HOP_VISION = 2;

    record HqLevelEntry(int upgradeCost, int warriorHp, int hp, int turret, int trainCap, int workCap) {}
    record BaseLevelEntry(int cost, int hp, int turret, int workCap) {}

    static final HqLevelEntry[] HQ_LEVELS = {
        new HqLevelEntry(0,    0,  0,  0, 0, 0),
        new HqLevelEntry(0,    4, 10,  1, 1, 1),
        new HqLevelEntry(600,  5, 15,  2, 1, 2),
        new HqLevelEntry(1000, 6, 20,  2, 2, 3),
        new HqLevelEntry(2000, 7, 25,  3, 2, 4),
        new HqLevelEntry(3000, 8, 30,  3, 3, 5),
    };
    static final BaseLevelEntry[] BASE_LEVELS = {
        new BaseLevelEntry(0,    0,  0, 0),
        new BaseLevelEntry(500,  6,  1, 1),
        new BaseLevelEntry(550, 12, 1, 2),
        new BaseLevelEntry(600, 18, 2, 3),
    };

    enum Side {
        LEFT, RIGHT;

        Side opposite() { return this == LEFT ? RIGHT : LEFT; }
        char toChar()   { return this == LEFT ? 'A' : 'B'; }

        static Side fromWord(String w) { return w.equals("LEFT") ? LEFT : RIGHT; }
        static Side fromChar(char c)   { return c == 'A' ? LEFT : RIGHT; }
    }

    enum BType { HQ, BASE }
    enum WState { STATIONARY, MOVING }

    record WarriorId(Side side, int num) {
        @Override public String toString() { return "" + side.toChar() + num; }

        static WarriorId parse(String tok) {
            assert !tok.isEmpty() && (tok.charAt(0) == 'A' || tok.charAt(0) == 'B');
            return new WarriorId(Side.fromChar(tok.charAt(0)), Integer.parseInt(tok.substring(1)));
        }
    }

    record Move(WarriorId warrior, int target) {}

    record Damage(WarriorId warrior, int amount) {}

    static class Warrior {
        WarriorId id;
        int region;
        int hp;
        WState state = WState.STATIONARY;
        int target = 0;

        Warrior(WarriorId id, int region, int hp) {
            this.id = id; this.region = region; this.hp = hp;
        }
    }

    static class Building {
        int region;
        Side side;
        BType type;
        int level;
        int hp;

        Building(int region, Side side, BType type, int level, int hp) {
            this.region = region; this.side = side;
            this.type = type; this.level = level; this.hp = hp;
        }

        int currentHp() {
            return type == BType.HQ ? HQ_LEVELS[level].hp() : BASE_LEVELS[level].hp();
        }
        int workCap() {
            return type == BType.HQ ? HQ_LEVELS[level].workCap() : BASE_LEVELS[level].workCap();
        }
    }

    static class GameMap {
        int N, K;
        long[] x, y;
        int[] strongholds;
        int[][] adj;
        Side mySide;
        int myHq, oppHq;

        int hqOf(Side s) { return s == Side.LEFT ? 0 : N - 1; }
    }

    static class GameState {
        int gold = START_GOLD;
        int myCountdown = 5;
        int oppCountdown = 5;
        List<Warrior>  warriors  = new ArrayList<>();
        List<Building> buildings = new ArrayList<>();
        List<Integer>  visible   = new ArrayList<>();

        Building findBuilding(int region) {
            for (var b : buildings) if (b.region == region) return b;
            return null;
        }
        Warrior findWarrior(WarriorId id) {
            for (var w : warriors) if (w.id.equals(id)) return w;
            return null;
        }
    }

    static class Actions {
        int trainN = 0;
        List<Move>    moves    = new ArrayList<>();
        List<Integer> upgrades = new ArrayList<>();
    }

    record Paths(double[][] dist, int[][] nxt) {}

    static BufferedReader in  = new BufferedReader(new InputStreamReader(System.in));
    static PrintWriter    out = new PrintWriter(System.out);

    static String readln() {
        try {
            var s = in.readLine();
            if (s == null) System.exit(0);
            return s;
        } catch (Exception e) {
            System.exit(0);
            return null; // unreachable
        }
    }

    static String[] tokens(String s) { return s.trim().split("\\s+"); }

    static Building makeBase(int region, Side s) {
        return new Building(region, s, BType.BASE, 1, BASE_LEVELS[1].hp());
    }

    static void applyUpgrade(Building b) {
        b.level += 1;
        b.hp = b.currentHp();
    }

    static int upgradeCost(Building b) {
        return b.type == BType.HQ
            ? HQ_LEVELS[b.level + 1].upgradeCost()
            : BASE_LEVELS[b.level + 1].cost();
    }

    static int maxLevel(Building b) {
        return b.type == BType.HQ ? HQ_MAX_LEVEL : BASE_MAX_LEVEL;
    }

    static void addHops(GameMap M, int start, int radius, Set<Integer> visible) {
        var seen = new HashSet<Integer>();
        seen.add(start);
        var frontier = new ArrayList<Integer>();
        frontier.add(start);
        for (int d = 0; d < radius; d++) {
            var next = new ArrayList<Integer>();
            for (int region : frontier) {
                for (int neighbor : M.adj[region]) {
                    if (seen.add(neighbor)) next.add(neighbor);
                }
            }
            frontier = next;
        }
        visible.addAll(seen);
    }

    static List<Integer> computeVisible(GameState S, GameMap M) {
        var visible = new HashSet<Integer>();
        for (var w : S.warriors) {
            if (w.id.side() == M.mySide) addHops(M, w.region, HOP_VISION, visible);
        }
        for (var b : S.buildings) {
            if (b.side == M.mySide) addHops(M, b.region, HOP_VISION, visible);
        }
        var out = new ArrayList<>(visible);
        out.sort(null);
        return out;
    }

    static void parseInit(GameMap M, GameState S) {
        {
            var t = tokens(readln());
            assert t.length >= 2 && t[0].equals("READY");
            M.mySide = Side.fromWord(t[1]);
        }
        {
            var t = tokens(readln());
            M.N = Integer.parseInt(t[0]);
            M.K = Integer.parseInt(t[1]);
        }
        M.x = new long[M.N];
        M.y = new long[M.N];
        {
            var t = tokens(readln()); // x_0 x_1 ... x_{N-1}
            for (int i = 0; i < M.N; i++) M.x[i] = Long.parseLong(t[i]);
        }
        {
            var t = tokens(readln()); // y_0 y_1 ... y_{N-1}
            for (int i = 0; i < M.N; i++) M.y[i] = Long.parseLong(t[i]);
        }
        {
            var t = tokens(readln()); // K strongholds
            M.strongholds = new int[t.length];
            for (int i = 0; i < t.length; i++) M.strongholds[i] = Integer.parseInt(t[i]);
            Arrays.sort(M.strongholds);
        }
        M.adj = new int[M.N][];
        for (int r = 0; r < M.N; r++) {
            var t = tokens(readln()); // deg n_1 n_2 ...
            int deg = Integer.parseInt(t[0]);
            M.adj[r] = new int[deg];
            for (int j = 0; j < deg; j++) M.adj[r][j] = Integer.parseInt(t[1 + j]);
            Arrays.sort(M.adj[r]);
        }

        M.myHq  = M.hqOf(M.mySide);
        M.oppHq = M.hqOf(M.mySide.opposite());

        S.gold = START_GOLD;
        var opp = M.mySide.opposite();
        for (int sfx = 1; sfx <= START_WARRIORS; sfx++) {
            S.warriors.add(new Warrior(new WarriorId(M.mySide, sfx), M.myHq,  HQ_LEVELS[1].warriorHp()));
            S.warriors.add(new Warrior(new WarriorId(opp,      sfx), M.oppHq, HQ_LEVELS[1].warriorHp()));
        }
        S.buildings.add(new Building(M.hqOf(Side.LEFT),  Side.LEFT,  BType.HQ, 1, HQ_LEVELS[1].hp()));
        S.buildings.add(new Building(M.hqOf(Side.RIGHT), Side.RIGHT, BType.HQ, 1, HQ_LEVELS[1].hp()));

        out.println("OK");
        out.flush();
    }

    static OptionalInt readTurnStart() {
        var line = readln();
        if (line.equals("FINISH")) return OptionalInt.empty();
        var t = tokens(line);
        assert t.length > 0 && t[0].equals("START");
        return OptionalInt.of(Integer.parseInt(t[2]));
    }

    static void readTurnResult(GameState S, GameMap M, Actions submitted) {
        for (int region : submitted.upgrades) {
            var b = S.findBuilding(region);
            if (b == null) {
                S.gold -= BASE_LEVELS[1].cost();
            } else {
                if (b.level >= maxLevel(b)) {
                    int cost = (b.type == BType.HQ) ? HQ_HEAL_COST : BASE_HEAL_COST;
                    S.gold -= cost;
                    b.hp = b.currentHp();
                } else {
                    S.gold -= upgradeCost(b);
                    applyUpgrade(b);
                }
            }
        }

        var built = new HashSet<>(submitted.upgrades);

        for (var mv : submitted.moves) {
            var b     = S.findBuilding(mv.target());
            boolean own = built.contains(mv.target()) || (b != null && b.side == M.mySide);
            int cost  = own ? 0 : MOVE_COST;
            S.gold -= cost;
            var w = S.findWarrior(mv.warrior());
            if (w != null) {
                w.state  = WState.MOVING;
                w.target = mv.target();
            }
        }

        S.gold -= TRAIN_COST * submitted.trainN;

        {
            var line = readln();
            if (line.equals("FINISH")) System.exit(0);
            var t = tokens(line);
            assert t.length > 0 && t[0].equals("TURN");
        }
        {
            var t = tokens(readln());
            S.myCountdown  = Integer.parseInt(t[2]);
            S.oppCountdown = Integer.parseInt(t[4]);
        }
        // UPGRADE
        {
            var t = tokens(readln()); // "UPGRADE N"
            int n = Integer.parseInt(t[1]);
            for (int i = 0; i < n; i++) {
                var r      = tokens(readln()); // "<A|B> <region>"
                int region = Integer.parseInt(r[1]);
                var b      = S.findBuilding(region);
                if (b == null) {
                    S.buildings.add(makeBase(region, M.mySide));
                } else if (b.level >= maxLevel(b)) {
                    b.hp = b.currentHp();
                } else {
                    applyUpgrade(b);
                }
            }
        }
        // TRAIN
        {
            var t = tokens(readln()); // "TRAIN N"
            int n = Integer.parseInt(t[1]);
            if (n > 0) {
                var ids = tokens(readln());
                for (int i = 0; i < n; i++) {
                    var wid     = WarriorId.parse(ids[i]);
                    var hqB     = S.findBuilding(M.myHq);
                    int hqLevel = (hqB != null) ? hqB.level : 1;
                    S.warriors.add(new Warrior(wid, M.myHq, HQ_LEVELS[hqLevel].warriorHp()));
                }
            }
        }
        // MOVE
        {
            var t = tokens(readln()); // "MOVE N"
            int n = Integer.parseInt(t[1]);
            for (int i = 0; i < n; i++) {
                var r  = tokens(readln());
                var id = WarriorId.parse(r[0]);
                int region = Integer.parseInt(r[1]);
                var w = S.findWarrior(id);
                if (w != null) {
                    w.region = region;
                    if (w.state == WState.MOVING && w.region == w.target) {
                        w.state = WState.STATIONARY;
                    }
                }
            }
        }
        // DAMAGE
        var starved = new ArrayList<Damage>();
        {
            var t = tokens(readln()); // "DAMAGE N"
            int n = Integer.parseInt(t[1]);
            for (int i = 0; i < n; i++) {
                var r  = tokens(readln()); // "<cause> <id> <damage>"
                var id = WarriorId.parse(r[1]);
                int damage = Integer.parseInt(r[2]);
                if (r[0].equals("HUNGER")) {
                    starved.add(new Damage(id, damage));
                    continue;
                }
                var w = S.findWarrior(id);
                if (w != null) w.hp -= damage;
            }
            S.warriors.removeIf(w -> w.hp <= 0);
        }
        // SIEGE
        {
            var t = tokens(readln()); // "SIEGE N"
            int n = Integer.parseInt(t[1]);
            for (int i = 0; i < n; i++) {
                var r      = tokens(readln());
                int region = Integer.parseInt(r[1]);
                int damage = Integer.parseInt(r[2]);
                var b = S.findBuilding(region);
                if (b != null) b.hp -= damage;
            }
            S.buildings.removeIf(b -> b.hp <= 0);
        }

        S.visible = computeVisible(S, M);

        // WARRIOR
        {
            var t = tokens(readln()); // "WARRIOR W"
            int n = Integer.parseInt(t[1]);
            var seen = new ArrayList<Warrior>();
            for (int i = 0; i < n; i++) {
                var r  = tokens(readln()); // "<id> <region> <hp>"
                var id = WarriorId.parse(r[0]);
                if (id.side() == M.mySide) continue;
                seen.add(new Warrior(id, Integer.parseInt(r[1]), Integer.parseInt(r[2])));
            }
            S.warriors.removeIf(w -> w.id.side() != M.mySide);
            S.warriors.addAll(seen);
        }

        // BUILDING
        {
            var t = tokens(readln()); // "BUILDING B"
            int n = Integer.parseInt(t[1]);
            var seen = new ArrayList<Building>();
            for (int i = 0; i < n; i++) {
                var r    = tokens(readln()); // "<side> <region> <kind> <level> <hp>"
                var side = Side.fromChar(r[0].charAt(0));
                if (side == M.mySide) continue;
                var btype = r[2].equals("HQ") ? BType.HQ : BType.BASE;
                seen.add(new Building(Integer.parseInt(r[1]), side, btype,
                                      Integer.parseInt(r[3]), Integer.parseInt(r[4])));
            }
            S.buildings.removeIf(b -> b.side != M.mySide);
            S.buildings.addAll(seen);
        }

        readln(); // "END"

        int income = 0;
        for (var b : S.buildings) {
            if (b.side != M.mySide) continue;
            int count = 0;
            for (var w : S.warriors) {
                if (w.id.side() == M.mySide && w.region == b.region) count++;
            }
            income += WORK_INCOME * Math.min(count, b.workCap());
        }
        S.gold += income;

        int alive = 0;
        for (var w : S.warriors) if (w.id.side() == M.mySide) alive++;
        S.gold -= UPKEEP_PER_WARRIOR * Math.min(alive, S.gold / UPKEEP_PER_WARRIOR);

        for (var row : starved) {
            var w = S.findWarrior(row.warrior());
            if (w != null) w.hp -= row.amount();
        }
        S.warriors.removeIf(w -> w.hp <= 0);
    }

    static double euclidCeil(GameMap M, int u, int v) {
        double dx = (double)(M.x[u] - M.x[v]);
        double dy = (double)(M.y[u] - M.y[v]);
        return Math.ceil(Math.sqrt(dx * dx + dy * dy));
    }

    static Paths calculatePaths(GameMap M) {
        final double INF = Double.POSITIVE_INFINITY;
        int N = M.N;
        var dist = new double[N][N];
        var nxt  = new int[N][N];

        for (var row : dist) Arrays.fill(row, INF);
        for (var row : nxt)  Arrays.fill(row, -1);

        for (int i = 0; i < N; i++) {
            dist[i][i] = 0.0;
            nxt[i][i]  = i;
        }
        for (int u = 0; u < N; u++) {
            for (int v : M.adj[u]) {
                double w = euclidCeil(M, u, v);
                if (w < dist[u][v]) dist[u][v] = w;
            }
        }

        for (int k = 0; k < N; k++) {
            for (int u = 0; u < N; u++) {
                if (dist[u][k] == INF) continue;
                for (int v = 0; v < N; v++) {
                    double cand = dist[u][k] + dist[k][v];
                    if (cand < dist[u][v]) dist[u][v] = cand;
                }
            }
        }

        for (int u = 0; u < N; u++) {
            for (int v = 0; v < N; v++) {
                if (u == v || dist[u][v] == INF) continue;
                double bestScore = INF;
                for (int nb : M.adj[u]) {
                    if (dist[nb][v] == INF) continue;
                    double score = euclidCeil(M, u, nb) + dist[nb][v];
                    if (score < bestScore) {
                        bestScore  = score;
                        nxt[u][v]  = nb;
                    }
                }
            }
        }

        return new Paths(dist, nxt);
    }

    static int nextStep(Paths P, int u, int v) { return P.nxt()[u][v]; }

    static List<Integer> path(Paths P, int u, int v) {
        List<Integer> steps = new ArrayList<>();
        if (P.nxt()[u][v] == -1) return steps;
        steps.add(u);
        while (u != v) {
            u = P.nxt()[u][v];
            steps.add(u);
        }
        return steps;
    }

    static void emitCommand() { out.println("COMMAND"); }

    static void emitActions(Actions a) {
        for (var mv : a.moves) {
            out.println("MOVE " + mv.warrior() + " " + mv.target());
        }
        for (int r : a.upgrades) {
            out.println("UPGRADE " + r);
        }
        if (a.trainN > 0) {
            out.println("TRAIN " + a.trainN);
        }
    }

    static void emitEnd() { out.println("END"); out.flush(); }

    // -------------------------------------------------------------------------
    //// WRITE YOUR STRATEGY HERE ////
    // -------------------------------------------------------------------------
    static Actions decide(GameState S, GameMap M, Paths P, int turn) {
        var a = new Actions();
        if (turn == 1) {
            for (var w : S.warriors) {
                if (w.id.side() != M.mySide) continue;
                a.moves.add(new Move(w.id, M.oppHq));
            }
        }
        return a;
    }

    public static void main(String[] args) {
        var M = new GameMap();
        var S = new GameState();
        parseInit(M, S);
        var P = calculatePaths(M);

        OptionalInt turnOpt;
        while ((turnOpt = readTurnStart()).isPresent()) {
            int turn = turnOpt.getAsInt();
            var a = decide(S, M, P, turn);
            emitCommand();
            emitActions(a);
            emitEnd();
            readTurnResult(S, M, a);
        }
    }
}
