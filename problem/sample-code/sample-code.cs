using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;

record struct HqLevelEntry(int UpgradeCost, int WarriorHp, int Hp, int Turret, int TrainCap, int WorkCap);
record struct BaseLevelEntry(int Cost, int Hp, int Turret, int WorkCap);

enum Side { LEFT = 0, RIGHT = 1 }
enum BType { HQ, BASE }
enum WState { STATIONARY, MOVING }

record struct WarriorId(Side Side, int Num)
{
    public override string ToString() => $"{Program.SideChar(Side)}{Num}";

    public static WarriorId Parse(string tok)
    {
        if (tok.Length == 0 || (tok[0] != 'A' && tok[0] != 'B'))
            throw new ArgumentException($"Invalid warrior id: {tok}");
        return new WarriorId(Program.ParseSideChar(tok[0]), int.Parse(tok[1..]));
    }
}

class Warrior
{
    public WarriorId Id;
    public int Region;
    public int Hp;
    public WState State = WState.STATIONARY;
    public int Target;
}

class Building
{
    public int Region;
    public Side Side;
    public BType Type;
    public int Level = 1;
    public int Hp;

    public int CurrentHp =>
        Type == BType.HQ ? Program.HQ_LEVELS[Level].Hp : Program.BASE_LEVELS[Level].Hp;

    public int WorkCap =>
        Type == BType.HQ ? Program.HQ_LEVELS[Level].WorkCap : Program.BASE_LEVELS[Level].WorkCap;

    public void ApplyUpgrade()
    {
        Level += 1;
        Hp = CurrentHp;
    }

    public int UpgradeCost =>
        Type == BType.HQ
            ? Program.HQ_LEVELS[Level + 1].UpgradeCost
            : Program.BASE_LEVELS[Level + 1].Cost;
}

class GameMap
{
    public int N, K;
    public long[] X = Array.Empty<long>();
    public long[] Y = Array.Empty<long>();
    public List<int> Strongholds = new();
    public List<List<int>> Adj = new();
    public Side MySide;
    public int MyHq;
    public int OppHq;

    public int HqOf(Side s) => s == Side.LEFT ? 0 : N - 1;
}

class GameState
{
    public int Gold = Program.START_GOLD;
    public int MyCountdown = 5;
    public int OppCountdown = 5;
    public List<Warrior> Warriors = new();
    public List<Building> Buildings = new();
    public List<int> Visible = new();

    public Building? FindBuilding(int region) =>
        Buildings.FirstOrDefault(b => b.Region == region);

    public Warrior? FindWarrior(WarriorId id) =>
        Warriors.FirstOrDefault(w => w.Id == id);
}

class Actions
{
    public int TrainN = 0;
    public List<(WarriorId Id, int Target)> Moves = new();
    public List<int> Upgrades = new();
}

class Paths
{
    public readonly double[][] Dist;
    public readonly int[][] Nxt;
    public Paths(int n)
    {
        Dist = new double[n][];
        Nxt = new int[n][];
        for (int i = 0; i < n; i++)
        {
            Dist[i] = new double[n];
            Nxt[i] = new int[n];
        }
    }
}

static class Program
{
    public const int MAX_TURN = 400;            // maximum turn (days)
    public const int START_GOLD = 750;          // initial gold
    public const int START_WARRIORS = 3;        // initial warriors
    public const int MOVE_COST = 10;            // move cost
    public const int TRAIN_COST = 120;          // train cost
    public const int WORK_INCOME = 15;          // income per warrior
    public const int UPKEEP_PER_WARRIOR = 2;    // upkeep per warrior
    public const int HQ_MAX_LEVEL = 5;          // HQ max level
    public const int BASE_MAX_LEVEL = 3;        // base max level
    public const int HQ_HEAL_COST = 1000;       // HQ fix cost
    public const int BASE_HEAL_COST = 500;      // base fix cost
    public const int HOP_VISION = 2;            // vision radius shared by all units

    public static readonly HqLevelEntry[] HQ_LEVELS = {
        new(0,    0,  0,  0, 0, 0),
        new(0,    4, 10,  1, 1, 1),
        new(600,  5, 15,  2, 1, 2),
        new(1000, 6, 20,  2, 2, 3),
        new(2000, 7, 25,  3, 2, 4),
        new(3000, 8, 30,  3, 3, 5),
    };
    public static readonly BaseLevelEntry[] BASE_LEVELS = {
        new(0,    0,  0, 0),
        new(500,  6,  1, 1),
        new(550, 12, 1, 2),
        new(600, 18, 2, 3),
    };

    public static char SideChar(Side s) => s == Side.LEFT ? 'A' : 'B';
    public static Side ParseSideChar(char c) => c == 'A' ? Side.LEFT : Side.RIGHT;
    static Side Opposite(Side s) => s == Side.LEFT ? Side.RIGHT : Side.LEFT;
    static int MaxLevel(Building b) => b.Type == BType.HQ ? HQ_MAX_LEVEL : BASE_MAX_LEVEL;

    static List<int> ComputeVisible(GameState S, GameMap M)
    {
        var visible = new HashSet<int>();
        void AddHops(int start, int radius)
        {
            var seen = new HashSet<int> { start };
            var frontier = new List<int> { start };
            for (int hop = 0; hop < radius; hop++)
            {
                var next = new List<int>();
                foreach (int region in frontier)
                {
                    foreach (int neighbor in M.Adj[region])
                    {
                        if (seen.Add(neighbor)) next.Add(neighbor);
                    }
                }
                frontier = next;
            }
            visible.UnionWith(seen);
        }
        foreach (var w in S.Warriors)
            if (w.Id.Side == M.MySide) AddHops(w.Region, HOP_VISION);
        foreach (var b in S.Buildings)
            if (b.Side == M.MySide) AddHops(b.Region, HOP_VISION);
        return visible.OrderBy(region => region).ToList();
    }

    static double EuclidCeil(GameMap M, int u, int v)
    {
        double dx = M.X[u] - M.X[v];
        double dy = M.Y[u] - M.Y[v];
        return Math.Ceiling(Math.Sqrt(dx * dx + dy * dy));
    }

    static string Readln()
    {
        string? line = Console.ReadLine();
        if (line is null) Environment.Exit(0);
        return line!;
    }

    static string[] Tokens(string s) => s.Split(' ', StringSplitOptions.RemoveEmptyEntries);

    static Building MakeBase(int region, Side s) =>
        new Building { Region = region, Side = s, Type = BType.BASE, Level = 1, Hp = BASE_LEVELS[1].Hp };

    static void ParseInit(GameMap M, GameState S)
    {
        var tReady = Tokens(Readln());
        M.MySide = tReady[1] == "LEFT" ? Side.LEFT : Side.RIGHT;

        var tNK = Tokens(Readln());
        M.N = int.Parse(tNK[0]);
        M.K = int.Parse(tNK[1]);

        M.X = new long[M.N];
        M.Y = new long[M.N];
        var tX = Tokens(Readln()); // x_0 x_1 ... x_{N-1}
        for (int i = 0; i < M.N; i++) M.X[i] = long.Parse(tX[i]);
        var tY = Tokens(Readln()); // y_0 y_1 ... y_{N-1}
        for (int i = 0; i < M.N; i++) M.Y[i] = long.Parse(tY[i]);

        var tSH = Tokens(Readln()); // K strongholds
        M.Strongholds = tSH.Select(int.Parse).OrderBy(x => x).ToList();

        M.Adj = new List<List<int>>(M.N);
        for (int r = 0; r < M.N; r++)
        {
            var tAdj = Tokens(Readln()); // deg n_1 n_2 ...
            int deg = int.Parse(tAdj[0]);
            var nb = new List<int>(deg);
            for (int j = 0; j < deg; j++) nb.Add(int.Parse(tAdj[1 + j]));
            nb.Sort();
            M.Adj.Add(nb);
        }

        M.MyHq = M.HqOf(M.MySide);
        M.OppHq = M.HqOf(Opposite(M.MySide));

        S.Gold = START_GOLD;
        var opp = Opposite(M.MySide);
        for (int sfx = 1; sfx <= START_WARRIORS; sfx++)
        {
            S.Warriors.Add(new Warrior { Id = new WarriorId(M.MySide, sfx), Region = M.MyHq, Hp = HQ_LEVELS[1].WarriorHp });
            S.Warriors.Add(new Warrior { Id = new WarriorId(opp, sfx), Region = M.OppHq, Hp = HQ_LEVELS[1].WarriorHp });
        }
        S.Buildings.Add(new Building { Region = M.HqOf(Side.LEFT),  Side = Side.LEFT,  Type = BType.HQ, Level = 1, Hp = HQ_LEVELS[1].Hp });
        S.Buildings.Add(new Building { Region = M.HqOf(Side.RIGHT), Side = Side.RIGHT, Type = BType.HQ, Level = 1, Hp = HQ_LEVELS[1].Hp });

        Console.WriteLine("OK");
        Console.Out.Flush();
    }

    static bool ReadTurnStart(out int turnIndex)
    {
        string line = Readln();
        if (line == "FINISH") { turnIndex = 0; return false; }
        var t = Tokens(line);
        turnIndex = int.Parse(t[2]);
        return true;
    }

    static void ReadTurnResult(GameState S, GameMap M, Actions submitted)
    {
        foreach (int region in submitted.Upgrades)
        {
            Building? b = S.FindBuilding(region);
            if (b == null)
            {
                S.Gold -= BASE_LEVELS[1].Cost;
            }
            else
            {
                if (b.Level >= MaxLevel(b))
                {
                    int cost = b.Type == BType.HQ ? HQ_HEAL_COST : BASE_HEAL_COST;
                    S.Gold -= cost;
                    b.Hp = b.CurrentHp;
                }
                else
                {
                    S.Gold -= b.UpgradeCost;
                    b.ApplyUpgrade();
                }
            }
        }

        var built = new HashSet<int>(submitted.Upgrades);

        foreach (var (id, target) in submitted.Moves)
        {
            Building? b = S.FindBuilding(target);
            bool own = built.Contains(target) || (b != null && b.Side == M.MySide);
            int cost = own ? 0 : MOVE_COST;
            S.Gold -= cost;
            Warrior? w = S.FindWarrior(id);
            if (w != null)
            {
                w.State = WState.MOVING;
                w.Target = target;
            }
        }

        S.Gold -= TRAIN_COST * submitted.TrainN;

        string turnLine = Readln();
        if (turnLine == "FINISH") Environment.Exit(0);

        var tCountdown = Tokens(Readln());
        S.MyCountdown = int.Parse(tCountdown[2]);
        S.OppCountdown = int.Parse(tCountdown[4]);

        // UPGRADE
        var tUpgrade = Tokens(Readln()); // "UPGRADE N"
        int upgradeN = int.Parse(tUpgrade[1]);
        for (int i = 0; i < upgradeN; i++)
        {
            var r = Tokens(Readln()); // "<A|B> <region>"
            int region = int.Parse(r[1]);
            var b = S.FindBuilding(region);
            if (b == null)
            {
                S.Buildings.Add(MakeBase(region, M.MySide));
            }
            else
            {
                if (b.Level >= MaxLevel(b))
                    b.Hp = b.CurrentHp;
                else
                    b.ApplyUpgrade();
            }
        }

        // TRAIN
        var tTrain = Tokens(Readln()); // "TRAIN N"
        int trainN = int.Parse(tTrain[1]);
        if (trainN > 0)
        {
            var ids = Tokens(Readln());
            var hqB = S.FindBuilding(M.MyHq);
            int hqLevel = hqB?.Level ?? 1;
            for (int i = 0; i < trainN; i++)
            {
                var wid = WarriorId.Parse(ids[i]);
                S.Warriors.Add(new Warrior { Id = wid, Region = M.MyHq, Hp = HQ_LEVELS[hqLevel].WarriorHp });
            }
        }

        // MOVE
        var tMove = Tokens(Readln()); // "MOVE N"
        int moveN = int.Parse(tMove[1]);
        for (int i = 0; i < moveN; i++)
        {
            var r = Tokens(Readln());
            var wid = WarriorId.Parse(r[0]);
            int region = int.Parse(r[1]);
            var w = S.FindWarrior(wid);
            if (w != null)
            {
                w.Region = region;
                if (w.State == WState.MOVING && w.Region == w.Target)
                    w.State = WState.STATIONARY;
            }
        }

        // DAMAGE
        var starved = new List<(WarriorId Id, int Damage)>();
        var tDamage = Tokens(Readln()); // "DAMAGE N"
        int damageN = int.Parse(tDamage[1]);
        for (int i = 0; i < damageN; i++)
        {
            var r = Tokens(Readln()); // "<cause> <id> <damage>"
            var wid = WarriorId.Parse(r[1]);
            int damage = int.Parse(r[2]);
            if (r[0] == "HUNGER")
            {
                starved.Add((wid, damage));
                continue;
            }
            var w = S.FindWarrior(wid);
            if (w != null) w.Hp -= damage;
        }
        S.Warriors.RemoveAll(w => w.Hp <= 0);

        // SIEGE
        var tSiege = Tokens(Readln()); // "SIEGE N"
        int siegeN = int.Parse(tSiege[1]);
        for (int i = 0; i < siegeN; i++)
        {
            var r = Tokens(Readln());
            int region = int.Parse(r[1]);
            int damage = int.Parse(r[2]);
            var b = S.FindBuilding(region);
            if (b != null) b.Hp -= damage;
        }
        S.Buildings.RemoveAll(b => b.Hp <= 0);

        S.Visible = ComputeVisible(S, M);

        // WARRIOR
        var tWarrior = Tokens(Readln()); // "WARRIOR W"
        int warriorW = int.Parse(tWarrior[1]);
        var seenWarriors = new List<Warrior>();
        for (int i = 0; i < warriorW; i++)
        {
            var r = Tokens(Readln()); // "<id> <region> <hp>"
            var wid = WarriorId.Parse(r[0]);
            if (wid.Side == M.MySide) continue;
            seenWarriors.Add(new Warrior { Id = wid, Region = int.Parse(r[1]), Hp = int.Parse(r[2]) });
        }
        S.Warriors = S.Warriors.Where(w => w.Id.Side == M.MySide).Concat(seenWarriors).ToList();

        // BUILDING
        var tBuilding = Tokens(Readln()); // "BUILDING B"
        int buildingB = int.Parse(tBuilding[1]);
        var seenBuildings = new List<Building>();
        for (int i = 0; i < buildingB; i++)
        {
            var r = Tokens(Readln()); // "<side> <region> <kind> <level> <hp>"
            var side = ParseSideChar(r[0][0]);
            if (side == M.MySide) continue;
            var btype = r[2] == "HQ" ? BType.HQ : BType.BASE;
            seenBuildings.Add(new Building { Region = int.Parse(r[1]), Side = side, Type = btype, Level = int.Parse(r[3]), Hp = int.Parse(r[4]) });
        }
        S.Buildings = S.Buildings.Where(b => b.Side == M.MySide).Concat(seenBuildings).ToList();

        Readln(); // "END"

        int income = 0;
        foreach (Building b in S.Buildings)
        {
            if (b.Side != M.MySide) continue;
            int count = S.Warriors.Count(w => w.Id.Side == M.MySide && w.Region == b.Region);
            income += WORK_INCOME * Math.Min(count, b.WorkCap);
        }
        S.Gold += income;

        int alive = S.Warriors.Count(w => w.Id.Side == M.MySide);
        S.Gold -= UPKEEP_PER_WARRIOR * Math.Min(alive, S.Gold / UPKEEP_PER_WARRIOR);

        foreach (var (wid, damage) in starved)
        {
            var w = S.FindWarrior(wid);
            if (w != null) w.Hp -= damage;
        }
        S.Warriors.RemoveAll(w => w.Hp <= 0);
    }

    static Paths CalculatePaths(GameMap M)
    {
        int N = M.N;
        var P = new Paths(N);
        for (int i = 0; i < N; i++)
            for (int j = 0; j < N; j++)
            {
                P.Dist[i][j] = double.PositiveInfinity;
                P.Nxt[i][j] = -1;
            }

        for (int i = 0; i < N; i++)
        {
            P.Dist[i][i] = 0.0;
            P.Nxt[i][i] = i;
        }
        for (int u = 0; u < N; u++)
        {
            foreach (int v in M.Adj[u])
            {
                double w = EuclidCeil(M, u, v);
                if (w < P.Dist[u][v]) P.Dist[u][v] = w;
            }
        }

        for (int k = 0; k < N; k++)
        {
            for (int u = 0; u < N; u++)
            {
                if (double.IsPositiveInfinity(P.Dist[u][k])) continue;
                for (int v = 0; v < N; v++)
                {
                    double cand = P.Dist[u][k] + P.Dist[k][v];
                    if (cand < P.Dist[u][v]) P.Dist[u][v] = cand;
                }
            }
        }

        for (int u = 0; u < N; u++)
        {
            for (int v = 0; v < N; v++)
            {
                if (u == v || double.IsPositiveInfinity(P.Dist[u][v])) continue;
                double bestScore = double.PositiveInfinity;
                foreach (int nb in M.Adj[u])
                {
                    if (double.IsPositiveInfinity(P.Dist[nb][v])) continue;
                    double score = EuclidCeil(M, u, nb) + P.Dist[nb][v];
                    if (score < bestScore)
                    {
                        bestScore = score;
                        P.Nxt[u][v] = nb;
                    }
                }
            }
        }
        return P;
    }

    static int NextStep(Paths P, int u, int v) => P.Nxt[u][v];

    static List<int> Path(Paths P, int u, int v)
    {
        if (P.Nxt[u][v] == -1) return [];
        var result = new List<int> { u };
        while (u != v)
        {
            u = P.Nxt[u][v];
            result.Add(u);
        }
        return result;
    }

    static void Emit(Actions a)
    {
        var sb = new StringBuilder();
        sb.AppendLine("COMMAND");
        foreach (var (id, target) in a.Moves)
            sb.AppendLine($"MOVE {id} {target}");
        foreach (int r in a.Upgrades)
            sb.AppendLine($"UPGRADE {r}");
        if (a.TrainN > 0)
            sb.AppendLine($"TRAIN {a.TrainN}");
        sb.AppendLine("END");
        Console.Write(sb);
        Console.Out.Flush();
    }

    //////////////////////////////////
    //// WRITE YOUR STRATEGY HERE ////
    //////////////////////////////////
    static Actions Decide(GameState S, GameMap M, Paths P, int turn)
    {
        var a = new Actions();
        if (turn == 1)
        {
            foreach (Warrior w in S.Warriors)
            {
                if (w.Id.Side != M.MySide) continue;
                a.Moves.Add((w.Id, M.OppHq));
            }
        }
        return a;
    }

    static void Main()
    {
        var M = new GameMap();
        var S = new GameState();
        ParseInit(M, S);
        var P = CalculatePaths(M);

        int turn;
        while (ReadTurnStart(out turn))
        {
            var a = Decide(S, M, P, turn);
            Emit(a);
            ReadTurnResult(S, M, a);
        }
    }
}
