package main

import (
	"bufio"
	"fmt"
	"math"
	"os"
	"sort"
	"strconv"
	"strings"
)

const (
	maxTurn          = 400  // maximum turn (days)
	startGold        = 750  // initial gold
	startWarriors    = 3    // initial warriors
	moveCost         = 10   // move cost
	trainCost        = 120  // train cost
	workIncome       = 15   // income per warrior
	upkeepPerWarrior = 2    // upkeep per warrior
	hqMaxLevel       = 5    // HQ max level
	baseMaxLevel     = 3    // base max level
	hqHealCost       = 1000 // HQ fix cost
	baseHealCost     = 500  // base fix cost
	hopVision        = 2    // vision radius shared by all units
)

type hqLevelEntry struct {
	upgradeCost int
	warriorHp   int
	hp          int
	turret      int
	trainCap    int
	workCap     int
}

type baseLevelEntry struct {
	cost    int
	hp      int
	turret  int
	workCap int
}

var hqLevels = [hqMaxLevel + 1]hqLevelEntry{
	{0, 0, 0, 0, 0, 0},
	{0, 4, 10, 1, 1, 1},
	{600, 5, 15, 2, 1, 2},
	{1000, 6, 20, 2, 2, 3},
	{2000, 7, 25, 3, 2, 4},
	{3000, 8, 30, 3, 3, 5},
}

var baseLevels = [baseMaxLevel + 1]baseLevelEntry{
	{0, 0, 0, 0},
	{500, 6, 1, 1},
	{550, 12, 1, 2},
	{600, 18, 2, 3},
}

type side int

const (
	sideLeft  side = 0
	sideRight side = 1
)

func oppSide(s side) side {
	if s == sideLeft {
		return sideRight
	}
	return sideLeft
}

func sideChar(s side) byte {
	if s == sideLeft {
		return 'A'
	}
	return 'B'
}

func parseSideChar(c byte) side {
	if c == 'A' {
		return sideLeft
	}
	return sideRight
}

type btype int

const (
	btypeHQ   btype = 0
	btypeBase btype = 1
)

type wstate int

const (
	wstateStationary wstate = 0
	wstateMoving     wstate = 1
)

type warriorID struct {
	side side
	num  int
}

func formatWarrior(id warriorID) string {
	return fmt.Sprintf("%c%d", sideChar(id.side), id.num)
}

func parseWarrior(tok string) warriorID {
	return warriorID{
		side: parseSideChar(tok[0]),
		num:  mustAtoi(tok[1:]),
	}
}

type warrior struct {
	id     warriorID
	region int
	hp     int
	state  wstate
	target int
}

type building struct {
	region int
	side   side
	btype  btype
	level  int
	hp     int
}

func (b *building) currentHp() int {
	if b.btype == btypeHQ {
		return hqLevels[b.level].hp
	}
	return baseLevels[b.level].hp
}

func (b *building) workCap() int {
	if b.btype == btypeHQ {
		return hqLevels[b.level].workCap
	}
	return baseLevels[b.level].workCap
}

func (b *building) maxLevel() int {
	if b.btype == btypeHQ {
		return hqMaxLevel
	}
	return baseMaxLevel
}

func (b *building) upgradeCost() int {
	if b.btype == btypeHQ {
		return hqLevels[b.level+1].upgradeCost
	}
	return baseLevels[b.level+1].cost
}

func (b *building) applyUpgrade() {
	b.level++
	b.hp = b.currentHp()
}

type gameMap struct {
	N           int
	K           int
	x           []int64
	y           []int64
	strongholds []int
	adj         [][]int
	mySide      side
	myHQ        int
	oppHQ       int
}

func hqOf(N int, s side) int {
	if s == sideLeft {
		return 0
	}
	return N - 1
}

type gameState struct {
	gold         int
	myCountdown  int
	oppCountdown int
	warriors     []*warrior
	buildings    []*building
	visible      []int
}

type move struct {
	id     warriorID
	target int
}

type actions struct {
	trainN   int
	moves    []move
	upgrades []int
}

var stdin *bufio.Reader
var stdout *bufio.Writer

func readln() string {
	line, err := stdin.ReadString('\n')
	if err != nil {
		if len(line) == 0 {
			os.Exit(0)
		}
	}
	return strings.TrimRight(line, "\r\n")
}

func readTokens() []string {
	return strings.Fields(readln())
}

func mustAtoi(s string) int {
	n, err := strconv.Atoi(s)
	if err != nil {
		panic(err)
	}
	return n
}

func mustAtoi64(s string) int64 {
	n, err := strconv.ParseInt(s, 10, 64)
	if err != nil {
		panic(err)
	}
	return n
}

func makeBase(region int, s side) *building {
	return &building{region: region, side: s, btype: btypeBase, level: 1, hp: baseLevels[1].hp}
}

func findBuilding(S *gameState, region int) *building {
	for _, b := range S.buildings {
		if b.region == region {
			return b
		}
	}
	return nil
}

func findWarrior(S *gameState, id warriorID) *warrior {
	for _, w := range S.warriors {
		if w.id == id {
			return w
		}
	}
	return nil
}

func computeVisible(S *gameState, M *gameMap) []int {
	visible := make([]bool, M.N)
	addHops := func(start, radius int) {
		seen := make([]bool, M.N)
		frontier := []int{start}
		seen[start] = true
		visible[start] = true
		for hop := 0; hop < radius; hop++ {
			next := make([]int, 0)
			for _, region := range frontier {
				for _, neighbor := range M.adj[region] {
					if !seen[neighbor] {
						seen[neighbor] = true
						visible[neighbor] = true
						next = append(next, neighbor)
					}
				}
			}
			frontier = next
		}
	}
	for _, w := range S.warriors {
		if w.id.side == M.mySide {
			addHops(w.region, hopVision)
		}
	}
	for _, b := range S.buildings {
		if b.side == M.mySide {
			addHops(b.region, hopVision)
		}
	}
	result := make([]int, 0)
	for region, isVisible := range visible {
		if isVisible {
			result = append(result, region)
		}
	}
	return result
}

func parseInit() (gameMap, gameState) {
	var M gameMap
	var S gameState

	{
		t := readTokens()
		if t[1] == "LEFT" {
			M.mySide = sideLeft
		} else {
			M.mySide = sideRight
		}
	}
	{
		t := readTokens()
		M.N = mustAtoi(t[0])
		M.K = mustAtoi(t[1])
	}
	M.x = make([]int64, M.N)
	M.y = make([]int64, M.N)
	{
		t := readTokens() // x_0 x_1 ... x_{N-1}
		for i := 0; i < M.N; i++ {
			M.x[i] = mustAtoi64(t[i])
		}
	}
	{
		t := readTokens() // y_0 y_1 ... y_{N-1}
		for i := 0; i < M.N; i++ {
			M.y[i] = mustAtoi64(t[i])
		}
	}
	{
		t := readTokens() // K strongholds
		M.strongholds = make([]int, len(t))
		for i, s := range t {
			M.strongholds[i] = mustAtoi(s)
		}
		sort.Ints(M.strongholds)
	}
	M.adj = make([][]int, M.N)
	for r := 0; r < M.N; r++ {
		t := readTokens() // deg n_1 n_2 ...
		deg := mustAtoi(t[0])
		nb := make([]int, deg)
		for j := 0; j < deg; j++ {
			nb[j] = mustAtoi(t[1+j])
		}
		sort.Ints(nb)
		M.adj[r] = nb
	}

	M.myHQ = hqOf(M.N, M.mySide)
	M.oppHQ = hqOf(M.N, oppSide(M.mySide))

	S.gold = startGold
	S.myCountdown = 5
	S.oppCountdown = 5
	opp := oppSide(M.mySide)
	for sfx := 1; sfx <= startWarriors; sfx++ {
		S.warriors = append(S.warriors, &warrior{id: warriorID{M.mySide, sfx}, region: M.myHQ, hp: hqLevels[1].warriorHp})
		S.warriors = append(S.warriors, &warrior{id: warriorID{opp, sfx}, region: M.oppHQ, hp: hqLevels[1].warriorHp})
	}
	S.buildings = append(S.buildings, &building{region: hqOf(M.N, sideLeft), side: sideLeft, btype: btypeHQ, level: 1, hp: hqLevels[1].hp})
	S.buildings = append(S.buildings, &building{region: hqOf(M.N, sideRight), side: sideRight, btype: btypeHQ, level: 1, hp: hqLevels[1].hp})

	fmt.Fprintln(stdout, "OK")
	stdout.Flush()
	return M, S
}

func readTurnStart() (int, bool) {
	line := readln()
	if line == "FINISH" {
		return 0, false
	}
	t := strings.Fields(line)
	return mustAtoi(t[2]), true
}

func readTurnResult(S *gameState, M *gameMap, submitted *actions) {
	for _, region := range submitted.upgrades {
		b := findBuilding(S, region)
		if b == nil {
			S.gold -= baseLevels[1].cost
		} else {
			if b.level >= b.maxLevel() {
				if b.btype == btypeHQ {
					S.gold -= hqHealCost
				} else {
					S.gold -= baseHealCost
				}
				b.hp = b.currentHp()
			} else {
				S.gold -= b.upgradeCost()
				b.applyUpgrade()
			}
		}
	}

	built := make(map[int]bool, len(submitted.upgrades))
	for _, region := range submitted.upgrades {
		built[region] = true
	}

	for _, mv := range submitted.moves {
		b := findBuilding(S, mv.target)
		cost := moveCost
		if built[mv.target] || (b != nil && b.side == M.mySide) {
			cost = 0
		}
		S.gold -= cost
		if w := findWarrior(S, mv.id); w != nil {
			w.state = wstateMoving
			w.target = mv.target
		}
	}

	S.gold -= trainCost * submitted.trainN

	{
		line := readln()
		if line == "FINISH" {
			stdout.Flush()
			os.Exit(0)
		}
	}
	{
		t := readTokens()
		S.myCountdown = mustAtoi(t[2])
		S.oppCountdown = mustAtoi(t[4])
	}
	// UPGRADE
	{
		t := readTokens() // "UPGRADE N"
		n := mustAtoi(t[1])
		for i := 0; i < n; i++ {
			r := readTokens() // "<A|B> <region>"
			region := mustAtoi(r[1])
			b := findBuilding(S, region)
			if b == nil {
				S.buildings = append(S.buildings, makeBase(region, M.mySide))
			} else {
				if b.level >= b.maxLevel() {
					b.hp = b.currentHp()
				} else {
					b.applyUpgrade()
				}
			}
		}
	}
	// TRAIN
	{
		t := readTokens() // "TRAIN N"
		n := mustAtoi(t[1])
		if n > 0 {
			ids := readTokens()
			hqB := findBuilding(S, M.myHQ)
			hqLevel := 1
			if hqB != nil {
				hqLevel = hqB.level
			}
			for i := 0; i < n; i++ {
				id := parseWarrior(ids[i])
				S.warriors = append(S.warriors, &warrior{id: id, region: M.myHQ, hp: hqLevels[hqLevel].warriorHp})
			}
		}
	}
	// MOVE
	{
		t := readTokens() // "MOVE N"
		n := mustAtoi(t[1])
		for i := 0; i < n; i++ {
			r := readTokens()
			id := parseWarrior(r[0])
			region := mustAtoi(r[1])
			if w := findWarrior(S, id); w != nil {
				w.region = region
				if w.state == wstateMoving && w.region == w.target {
					w.state = wstateStationary
				}
			}
		}
	}
	// DAMAGE
	type starvedRow struct {
		id     warriorID
		damage int
	}
	var starved []starvedRow
	{
		t := readTokens() // "DAMAGE N"
		n := mustAtoi(t[1])
		for i := 0; i < n; i++ {
			r := readTokens() // "<cause> <id> <damage>"
			id := parseWarrior(r[1])
			damage := mustAtoi(r[2])
			if r[0] == "HUNGER" {
				starved = append(starved, starvedRow{id, damage})
				continue
			}
			if w := findWarrior(S, id); w != nil {
				w.hp -= damage
			}
		}
		alive := S.warriors[:0]
		for _, w := range S.warriors {
			if w.hp > 0 {
				alive = append(alive, w)
			}
		}
		S.warriors = alive
	}
	// SIEGE
	{
		t := readTokens() // "SIEGE N"
		n := mustAtoi(t[1])
		for i := 0; i < n; i++ {
			r := readTokens()
			region := mustAtoi(r[1])
			damage := mustAtoi(r[2])
			if b := findBuilding(S, region); b != nil {
				b.hp -= damage
			}
		}
		standing := S.buildings[:0]
		for _, b := range S.buildings {
			if b.hp > 0 {
				standing = append(standing, b)
			}
		}
		S.buildings = standing
	}
	S.visible = computeVisible(S, M)
	// WARRIOR
	{
		t := readTokens() // "WARRIOR W"
		n := mustAtoi(t[1])
		own := S.warriors[:0]
		for _, w := range S.warriors {
			if w.id.side == M.mySide {
				own = append(own, w)
			}
		}
		S.warriors = own
		for i := 0; i < n; i++ {
			r := readTokens() // "<id> <region> <hp>"
			id := parseWarrior(r[0])
			if id.side == M.mySide {
				continue
			}
			S.warriors = append(S.warriors, &warrior{id: id, region: mustAtoi(r[1]), hp: mustAtoi(r[2])})
		}
	}
	// BUILDING
	{
		t := readTokens() // "BUILDING B"
		n := mustAtoi(t[1])
		own := S.buildings[:0]
		for _, b := range S.buildings {
			if b.side == M.mySide {
				own = append(own, b)
			}
		}
		S.buildings = own
		for i := 0; i < n; i++ {
			r := readTokens() // "<side> <region> <kind> <level> <hp>"
			s := parseSideChar(r[0][0])
			if s == M.mySide {
				continue
			}
			bt := btypeBase
			if r[2] == "HQ" {
				bt = btypeHQ
			}
			S.buildings = append(S.buildings, &building{region: mustAtoi(r[1]), side: s, btype: bt, level: mustAtoi(r[3]), hp: mustAtoi(r[4])})
		}
	}
	readln() // "END"

	income := 0
	for _, b := range S.buildings {
		if b.side != M.mySide {
			continue
		}
		count := 0
		for _, w := range S.warriors {
			if w.id.side == M.mySide && w.region == b.region {
				count++
			}
		}
		wc := b.workCap()
		if count < wc {
			wc = count
		}
		income += workIncome * wc
	}
	S.gold += income

	aliveCount := 0
	for _, w := range S.warriors {
		if w.id.side == M.mySide {
			aliveCount++
		}
	}
	fed := S.gold / upkeepPerWarrior
	if fed > aliveCount {
		fed = aliveCount
	}
	S.gold -= upkeepPerWarrior * fed

	for _, row := range starved {
		if w := findWarrior(S, row.id); w != nil {
			w.hp -= row.damage
		}
	}
	kept := S.warriors[:0]
	for _, w := range S.warriors {
		if w.hp > 0 {
			kept = append(kept, w)
		}
	}
	S.warriors = kept
}

type paths struct {
	dist [][]float64
	nxt  [][]int
}

func euclidCeil(M *gameMap, u, v int) float64 {
	dx := float64(M.x[u] - M.x[v])
	dy := float64(M.y[u] - M.y[v])
	return math.Ceil(math.Sqrt(dx*dx + dy*dy))
}

func calculatePaths(M *gameMap) paths {
	N := M.N
	dist := make([][]float64, N)
	nxt := make([][]int, N)
	for i := 0; i < N; i++ {
		dist[i] = make([]float64, N)
		nxt[i] = make([]int, N)
		for j := 0; j < N; j++ {
			dist[i][j] = math.Inf(1)
			nxt[i][j] = -1
		}
		dist[i][i] = 0.0
		nxt[i][i] = i
	}
	for u := 0; u < N; u++ {
		for _, v := range M.adj[u] {
			w := euclidCeil(M, u, v)
			if w < dist[u][v] {
				dist[u][v] = w
			}
		}
	}

	for k := 0; k < N; k++ {
		for u := 0; u < N; u++ {
			if math.IsInf(dist[u][k], 1) {
				continue
			}
			for v := 0; v < N; v++ {
				cand := dist[u][k] + dist[k][v]
				if cand < dist[u][v] {
					dist[u][v] = cand
				}
			}
		}
	}

	for u := 0; u < N; u++ {
		for v := 0; v < N; v++ {
			if u == v || math.IsInf(dist[u][v], 1) {
				continue
			}
			bestScore := math.Inf(1)
			for _, nb := range M.adj[u] {
				if math.IsInf(dist[nb][v], 1) {
					continue
				}
				score := euclidCeil(M, u, nb) + dist[nb][v]
				if score < bestScore {
					bestScore = score
					nxt[u][v] = nb
				}
			}
		}
	}
	return paths{dist, nxt}
}

func nextStep(P *paths, u, v int) int { return P.nxt[u][v] }

func pathFrom(P *paths, u, v int) []int {
	if P.nxt[u][v] == -1 {
		return nil
	}
	out := []int{u}
	for u != v {
		u = P.nxt[u][v]
		out = append(out, u)
	}
	return out
}

func emitActions(a *actions) {
	fmt.Fprintln(stdout, "COMMAND")
	for _, mv := range a.moves {
		fmt.Fprintf(stdout, "MOVE %s %d\n", formatWarrior(mv.id), mv.target)
	}
	for _, r := range a.upgrades {
		fmt.Fprintf(stdout, "UPGRADE %d\n", r)
	}
	if a.trainN > 0 {
		fmt.Fprintf(stdout, "TRAIN %d\n", a.trainN)
	}
	fmt.Fprintln(stdout, "END")
	stdout.Flush()
}

//////////////////////////////////
//// WRITE YOUR STRATEGY HERE ////
//////////////////////////////////
func decide(S *gameState, M *gameMap, P *paths, turn int) actions {
	a := actions{}
	if turn == 1 {
		for _, w := range S.warriors {
			if w.id.side != M.mySide {
				continue
			}
			a.moves = append(a.moves, move{w.id, M.oppHQ})
		}
	}
	return a
}

func main() {
	stdin = bufio.NewReaderSize(os.Stdin, 1<<20)
	stdout = bufio.NewWriterSize(os.Stdout, 1<<20)

	M, S := parseInit()
	P := calculatePaths(&M)

	for {
		turn, ok := readTurnStart()
		if !ok {
			break
		}
		a := decide(&S, &M, &P, turn)
		emitActions(&a)
		readTurnResult(&S, &M, &a)
	}
}
