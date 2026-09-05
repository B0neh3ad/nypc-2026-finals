-- maximum turn (days)
local MAX_TURN = 400
-- initial gold
local START_GOLD = 750
-- initial warriors
local START_WARRIORS = 3
-- move cost
local MOVE_COST = 10
-- train cost
local TRAIN_COST = 120
-- income per warrior
local WORK_INCOME = 15
-- upkeep per warrior
local UPKEEP_PER_WARRIOR = 2
-- HQ max level
local HQ_MAX_LEVEL = 5
-- base max level
local BASE_MAX_LEVEL = 3
-- HQ fix cost
local HQ_HEAL_COST = 1000
-- base fix cost
local BASE_HEAL_COST = 500
-- vision radius shared by all units
local HOP_VISION = 2

local HQ_LEVELS = {
    [0] = {upgrade_cost=0,    warrior_hp=0, hp=0,  turret=0, train_cap=0, work_cap=0},
    [1] = {upgrade_cost=0,    warrior_hp=4, hp=10, turret=1, train_cap=1, work_cap=1},
    [2] = {upgrade_cost=600,  warrior_hp=5, hp=15, turret=2, train_cap=1, work_cap=2},
    [3] = {upgrade_cost=1000, warrior_hp=6, hp=20, turret=2, train_cap=2, work_cap=3},
    [4] = {upgrade_cost=2000, warrior_hp=7, hp=25, turret=3, train_cap=2, work_cap=4},
    [5] = {upgrade_cost=3000, warrior_hp=8, hp=30, turret=3, train_cap=3, work_cap=5},
}

local BASE_LEVELS = {
    [0] = {cost=0,    hp=0,  turret=0, work_cap=0},
    [1] = {cost=500,  hp=6,  turret=1, work_cap=1},
    [2] = {cost=550, hp=12, turret=1, work_cap=2},
    [3] = {cost=600, hp=18, turret=2, work_cap=3},
}

-- Side constants: "A" = LEFT, "B" = RIGHT
local LEFT  = "A"
local RIGHT = "B"

local HQ   = "HQ"
local BASE = "BASE"

local STATIONARY = 0
local MOVING     = 1

local function opposite(s)
    return s == LEFT and RIGHT or LEFT
end

local function side_from_word(w)
    return w == "LEFT" and LEFT or RIGHT
end

local function side_from_char(c)
    return c == "A" and LEFT or RIGHT
end

local function parse_warrior(tok)
    local side = side_from_char(tok:sub(1, 1))
    local num  = tonumber(tok:sub(2))
    return {side=side, num=num}
end

local function format_warrior(id)
    return id.side .. id.num
end

local function warrior_id_eq(a, b)
    return a.side == b.side and a.num == b.num
end

local function hq_of(N, s)
    return s == LEFT and 0 or N - 1
end

local function make_base(region, s)
    return {region=region, side=s, btype=BASE, level=1, hp=BASE_LEVELS[1].hp}
end

local function building_current_hp(b)
    if b.btype == HQ then
        return HQ_LEVELS[b.level].hp
    else
        return BASE_LEVELS[b.level].hp
    end
end

local function building_work_cap(b)
    if b.btype == HQ then
        return HQ_LEVELS[b.level].work_cap
    else
        return BASE_LEVELS[b.level].work_cap
    end
end

local function apply_upgrade(b)
    b.level = b.level + 1
    b.hp = building_current_hp(b)
end

local function upgrade_cost(b)
    if b.btype == HQ then
        return HQ_LEVELS[b.level + 1].upgrade_cost
    else
        return BASE_LEVELS[b.level + 1].cost
    end
end

local function max_level(b)
    return b.btype == HQ and HQ_MAX_LEVEL or BASE_MAX_LEVEL
end

local function readln()
    local line = io.read("*l")
    if line == nil then
        os.exit(0)
    end
    return line
end

local function split_tokens(line)
    local t = {}
    for tok in line:gmatch("%S+") do
        t[#t + 1] = tok
    end
    return t
end

local function read_tokens()
    return split_tokens(readln())
end

local function find_building(S, region)
    for _, b in ipairs(S.buildings) do
        if b.region == region then
            return b
        end
    end
    return nil
end

local function find_warrior(S, id)
    for _, w in ipairs(S.warriors) do
        if warrior_id_eq(w.id, id) then
            return w
        end
    end
    return nil
end

local function compute_visible(S, M)
    local visible = {}
    local function add_hops(start, radius)
        local seen = {[start]=true}
        local frontier = {start}
        for _ = 1, radius do
            local next_frontier = {}
            for _, region in ipairs(frontier) do
                for _, neighbor in ipairs(M.adj[region]) do
                    if not seen[neighbor] then
                        seen[neighbor] = true
                        next_frontier[#next_frontier + 1] = neighbor
                    end
                end
            end
            frontier = next_frontier
        end
        for region in pairs(seen) do
            visible[region] = true
        end
    end
    for _, w in ipairs(S.warriors) do
        if w.id.side == M.my_side then
            add_hops(w.region, HOP_VISION)
        end
    end
    for _, b in ipairs(S.buildings) do
        if b.side == M.my_side then
            add_hops(b.region, HOP_VISION)
        end
    end
    local result = {}
    for region = 0, M.N - 1 do
        if visible[region] then
            result[#result + 1] = region
        end
    end
    return result
end

local function parse_init()
    local M = {}
    local t

    t = read_tokens()
    assert(#t >= 2 and t[1] == "READY")
    M.my_side = side_from_word(t[2])

    t = read_tokens()
    M.N = tonumber(t[1])
    M.K = tonumber(t[2])

    -- x_0 x_1 ... x_{N-1}
    t = read_tokens()
    M.x = {}
    for i = 0, M.N - 1 do
        M.x[i] = tonumber(t[i + 1])
    end

    -- y_0 y_1 ... y_{N-1}
    t = read_tokens()
    M.y = {}
    for i = 0, M.N - 1 do
        M.y[i] = tonumber(t[i + 1])
    end

    -- K strongholds
    t = read_tokens()
    M.strongholds = {}
    for _, v in ipairs(t) do
        M.strongholds[#M.strongholds + 1] = tonumber(v)
    end
    table.sort(M.strongholds)

    M.adj = {}
    for r = 0, M.N - 1 do
        t = read_tokens()  -- deg n_1 n_2 ...
        local deg = tonumber(t[1])
        local nb = {}
        for j = 1, deg do
            nb[j] = tonumber(t[1 + j])
        end
        table.sort(nb)
        M.adj[r] = nb
    end

    M.my_hq  = hq_of(M.N, M.my_side)
    M.opp_hq = hq_of(M.N, opposite(M.my_side))

    local S = {
        gold         = START_GOLD,
        my_countdown  = 5,
        opp_countdown = 5,
        warriors  = {},
        buildings = {},
        visible   = {},
    }

    local opp = opposite(M.my_side)
    for sfx = 1, START_WARRIORS do
        S.warriors[#S.warriors + 1] = {
            id={side=M.my_side, num=sfx},
            region=M.my_hq,
            hp=HQ_LEVELS[1].warrior_hp,
            state=STATIONARY, target=0,
        }
        S.warriors[#S.warriors + 1] = {
            id={side=opp, num=sfx},
            region=M.opp_hq,
            hp=HQ_LEVELS[1].warrior_hp,
            state=STATIONARY, target=0,
        }
    end
    S.buildings[#S.buildings + 1] = {
        region=hq_of(M.N, LEFT), side=LEFT, btype=HQ, level=1, hp=HQ_LEVELS[1].hp
    }
    S.buildings[#S.buildings + 1] = {
        region=hq_of(M.N, RIGHT), side=RIGHT, btype=HQ, level=1, hp=HQ_LEVELS[1].hp
    }

    io.write("OK\n")
    io.flush()

    return M, S
end

local function read_turn_start()
    local line = readln()
    if line == "FINISH" then
        return nil
    end
    local t = split_tokens(line)
    assert(#t > 0 and t[1] == "START")
    return tonumber(t[3])
end

local function read_turn_result(S, M, submitted)
    for _, region in ipairs(submitted.upgrades) do
        local b = find_building(S, region)
        if b == nil then
            S.gold = S.gold - BASE_LEVELS[1].cost
        else
            if b.level >= max_level(b) then
                local cost = (b.btype == HQ) and HQ_HEAL_COST or BASE_HEAL_COST
                S.gold = S.gold - cost
                b.hp = building_current_hp(b)
            else
                S.gold = S.gold - upgrade_cost(b)
                apply_upgrade(b)
            end
        end
    end

    local built = {}
    for _, region in ipairs(submitted.upgrades) do
        built[region] = true
    end

    for _, mv in ipairs(submitted.moves) do
        local wid, target = mv[1], mv[2]
        local b = find_building(S, target)
        local own = built[target] or (b ~= nil and b.side == M.my_side)
        local cost = own and 0 or MOVE_COST
        S.gold = S.gold - cost
        local w = find_warrior(S, wid)
        if w ~= nil then
            w.state  = MOVING
            w.target = target
        end
    end

    S.gold = S.gold - TRAIN_COST * submitted.train_n

    local line = readln()
    if line == "FINISH" then
        os.exit(0)
    end
    local t = split_tokens(line)
    assert(#t > 0 and t[1] == "TURN")

    t = read_tokens()
    S.my_countdown  = tonumber(t[3])
    S.opp_countdown = tonumber(t[5])

    -- UPGRADE
    t = read_tokens()  -- "UPGRADE N"
    local n = tonumber(t[2])
    for _ = 1, n do
        local r  = read_tokens()  -- "<A|B> <region>"
        local region = tonumber(r[2])
        local b  = find_building(S, region)
        if b == nil then
            S.buildings[#S.buildings + 1] = make_base(region, M.my_side)
        else
            if b.level >= max_level(b) then
                b.hp = building_current_hp(b)
            else
                apply_upgrade(b)
            end
        end
    end

    -- TRAIN
    t = read_tokens()  -- "TRAIN N"
    n = tonumber(t[2])
    if n > 0 then
        local ids = read_tokens()
        for i = 1, n do
            local wid      = parse_warrior(ids[i])
            local hq_b     = find_building(S, M.my_hq)
            local hq_level = (hq_b ~= nil) and hq_b.level or 1
            S.warriors[#S.warriors + 1] = {
                id=wid, region=M.my_hq,
                hp=HQ_LEVELS[hq_level].warrior_hp,
                state=STATIONARY, target=0,
            }
        end
    end

    -- MOVE
    t = read_tokens()  -- "MOVE N"
    n = tonumber(t[2])
    for _ = 1, n do
        local r      = read_tokens()
        local wid    = parse_warrior(r[1])
        local region = tonumber(r[2])
        local w      = find_warrior(S, wid)
        if w ~= nil then
            w.region = region
            if w.state == MOVING and w.region == w.target then
                w.state = STATIONARY
            end
        end
    end

    -- DAMAGE
    local starved = {}
    t = read_tokens()  -- "DAMAGE N"
    n = tonumber(t[2])
    for _ = 1, n do
        local r      = read_tokens()  -- "<cause> <id> <damage>"
        local wid    = parse_warrior(r[2])
        local damage = tonumber(r[3])
        if r[1] == "HUNGER" then
            starved[#starved + 1] = { wid, damage }
        else
            local w = find_warrior(S, wid)
            if w ~= nil then
                w.hp = w.hp - damage
            end
        end
    end
    local alive_warriors = {}
    for _, w in ipairs(S.warriors) do
        if w.hp > 0 then
            alive_warriors[#alive_warriors + 1] = w
        end
    end
    S.warriors = alive_warriors

    -- SIEGE
    t = read_tokens()  -- "SIEGE N"
    n = tonumber(t[2])
    for _ = 1, n do
        local r      = read_tokens()
        local region = tonumber(r[2])
        local damage = tonumber(r[3])
        local b      = find_building(S, region)
        if b ~= nil then
            b.hp = b.hp - damage
        end
    end
    local alive_buildings = {}
    for _, b in ipairs(S.buildings) do
        if b.hp > 0 then
            alive_buildings[#alive_buildings + 1] = b
        end
    end
    S.buildings = alive_buildings

    S.visible = compute_visible(S, M)

    -- WARRIOR
    t = read_tokens()  -- "WARRIOR W"
    local w_count = tonumber(t[2])
    local seen_warriors = {}
    for _ = 1, w_count do
        local r   = read_tokens()  -- "<id> <region> <hp>"
        local wid = parse_warrior(r[1])
        if wid.side ~= M.my_side then
            seen_warriors[#seen_warriors + 1] = {
                id=wid, region=tonumber(r[2]), hp=tonumber(r[3]),
                state=STATIONARY, target=0,
            }
        end
    end
    local kept_warriors = {}
    for _, w in ipairs(S.warriors) do
        if w.id.side == M.my_side then
            kept_warriors[#kept_warriors + 1] = w
        end
    end
    for _, w in ipairs(seen_warriors) do
        kept_warriors[#kept_warriors + 1] = w
    end
    S.warriors = kept_warriors

    -- BUILDING
    t = read_tokens()  -- "BUILDING B"
    local b_count = tonumber(t[2])
    local seen_buildings = {}
    for _ = 1, b_count do
        local r    = read_tokens()  -- "<side> <region> <kind> <level> <hp>"
        local side = side_from_char(r[1])
        if side ~= M.my_side then
            local btype = (r[3] == "HQ") and HQ or BASE
            seen_buildings[#seen_buildings + 1] = {
                region=tonumber(r[2]), side=side, btype=btype,
                level=tonumber(r[4]), hp=tonumber(r[5]),
            }
        end
    end
    local kept_buildings = {}
    for _, b in ipairs(S.buildings) do
        if b.side == M.my_side then
            kept_buildings[#kept_buildings + 1] = b
        end
    end
    for _, b in ipairs(seen_buildings) do
        kept_buildings[#kept_buildings + 1] = b
    end
    S.buildings = kept_buildings

    readln()  -- "END"

    local income = 0
    for _, b in ipairs(S.buildings) do
        if b.side == M.my_side then
            local count = 0
            for _, w in ipairs(S.warriors) do
                if w.id.side == M.my_side and w.region == b.region then
                    count = count + 1
                end
            end
            local cap = building_work_cap(b)
            income = income + WORK_INCOME * math.min(count, cap)
        end
    end
    S.gold = S.gold + income

    local alive = 0
    for _, w in ipairs(S.warriors) do
        if w.id.side == M.my_side then
            alive = alive + 1
        end
    end
    local fed = math.min(alive, math.floor(S.gold / UPKEEP_PER_WARRIOR))
    S.gold = S.gold - UPKEEP_PER_WARRIOR * fed

    for _, row in ipairs(starved) do
        local w = find_warrior(S, row[1])
        if w ~= nil then
            w.hp = w.hp - row[2]
        end
    end
    local survivors = {}
    for _, w in ipairs(S.warriors) do
        if w.hp > 0 then
            survivors[#survivors + 1] = w
        end
    end
    S.warriors = survivors
end

local function euclid_ceil(M, u, v)
    local dx = M.x[u] - M.x[v]
    local dy = M.y[u] - M.y[v]
    return math.ceil(math.sqrt(dx * dx + dy * dy))
end

local function calculate_paths(M)
    return {M = M, dist_to = {}, nxt_to = {}}
end

local function paths_toward(P, v)
    if P.dist_to[v] == nil then
        local M   = P.M
        local N   = M.N
        local INF = math.huge

        local dist = {}
        local done = {}
        for i = 0, N - 1 do
            dist[i] = INF
        end
        dist[v] = 0.0
        for _ = 1, N do
            local u, best = -1, INF
            for i = 0, N - 1 do
                if not done[i] and dist[i] < best then
                    u, best = i, dist[i]
                end
            end
            if u < 0 then
                break
            end
            done[u] = true
            for _, nb in ipairs(M.adj[u]) do
                local cand = best + euclid_ceil(M, u, nb)
                if cand < dist[nb] then
                    dist[nb] = cand
                end
            end
        end

        local nxt = {}
        for u = 0, N - 1 do
            nxt[u] = -1
        end
        nxt[v] = v
        for u = 0, N - 1 do
            if u ~= v and dist[u] ~= INF then
                local best_score = INF
                for _, nb in ipairs(M.adj[u]) do
                    if dist[nb] ~= INF then
                        local score = euclid_ceil(M, u, nb) + dist[nb]
                        if score < best_score then
                            best_score = score
                            nxt[u] = nb
                        end
                    end
                end
            end
        end

        P.dist_to[v] = dist
        P.nxt_to[v] = nxt
    end
    return P.dist_to[v], P.nxt_to[v]
end

local function next_step(P, u, v)
    local _, nxt = paths_toward(P, v)
    return nxt[u]
end

local function path(P, u, v)
    if next_step(P, u, v) == -1 then
        return {}
    end
    local out = {u}
    while u ~= v do
        u = next_step(P, u, v)
        out[#out + 1] = u
    end
    return out
end

local function emit(a)
    local out = {"COMMAND"}
    for _, mv in ipairs(a.moves) do
        out[#out + 1] = "MOVE " .. format_warrior(mv[1]) .. " " .. mv[2]
    end
    for _, r in ipairs(a.upgrades) do
        out[#out + 1] = "UPGRADE " .. r
    end
    if a.train_n > 0 then
        out[#out + 1] = "TRAIN " .. a.train_n
    end
    out[#out + 1] = "END"
    io.write(table.concat(out, "\n") .. "\n")
    io.flush()
end

----------------------------------
---- WRITE YOUR STRATEGY HERE ----
----------------------------------
local function decide(S, M, P, turn)
    local a = {train_n=0, moves={}, upgrades={}}
    if turn == 1 then
        for _, w in ipairs(S.warriors) do
            if w.id.side == M.my_side then
                a.moves[#a.moves + 1] = {w.id, M.opp_hq}
            end
        end
    end
    return a
end

local function main()
    local M, S = parse_init()
    local P    = calculate_paths(M)

    local turn = read_turn_start()
    while turn ~= nil do
        local a = decide(S, M, P, turn)
        emit(a)
        read_turn_result(S, M, a)
        turn = read_turn_start()
    end
end

main()
