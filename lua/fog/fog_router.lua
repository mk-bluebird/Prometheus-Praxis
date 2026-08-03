-- File: lua/fog/fog_router.lua
-- Lua Predictive FOG Router with Schedule Cache
-- Calls a C++ CLI predictor, populates schedule_cache, and uses it
-- for corridor-safe workload dispatch.

local sqlite3 = require("lsqlite3")

local DB_PATH = "prometheus_praxis.db"
local CPP_PREDICTOR = "./fog_schedule_predictor"  -- C++ CLI wiring binary

local function open_db()
    local db = sqlite3.open(DB_PATH)
    if not db then
        error("Failed to open SQLite database: " .. DB_PATH)
    end
    return db
end

local function close_db(db)
    if db then db:close() end
end

-- Call the C++ predictor CLI and capture JSON-like output.
local function call_cpp_predictor()
    local cmd = CPP_PREDICTOR .. " 2>/dev/null"
    local handle = io.popen(cmd)
    if not handle then
        error("Failed to spawn C++ predictor: " .. CPP_PREDICTOR)
    end
    local output = handle:read("*a")
    handle:close()
    return output
end

-- Parse a simple JSON-like schedule from the C++ predictor output.
-- Expected format:
-- { "schedule": [ { "hexId": "...", "hour": 0, "priority": 0.8 }, ... ] }
local function parse_schedule(json)
    local schedule = {}
    for line in json:gmatch("[^\r\n]+") do
        if line:match("\"hexId\"") then
            local hex_id = line:match("\"hexId\"%s*:%s*\"([^\"]+)\"")
            local hour = line:match("\"hour\"%s*:%s*(%d+)")
            local priority = line:match("\"priority\"%s*:%s*([%d%.]+)")
            if hex_id and hour and priority then
                table.insert(schedule, {
                    hex_id = hex_id,
                    hour = tonumber(hour),
                    priority = tonumber(priority)
                })
            end
        end
    end
    return schedule
end

-- Populate schedule_cache table from parsed schedule.
local function populate_schedule_cache(db, schedule)
    db:exec("DELETE FROM schedule_cache;")

    local stmt = db:prepare([[
        INSERT INTO schedule_cache
        (hex_id, hour, priority, ts)
        VALUES (?, ?, ?, CURRENT_TIMESTAMP)
    ]])

    for _, entry in ipairs(schedule) do
        stmt:bind_values(entry.hex_id, entry.hour, entry.priority)
        stmt:step()
        stmt:reset()
    end

    stmt:finalize()
end

-- Corridor-safe workload dispatch using schedule_cache.
local function dispatch_workloads(db)
    local sql = [[
        SELECT sc.hex_id, sc.hour, sc.priority,
               h.carbon_band, h.v_residual
        FROM schedule_cache sc
        JOIN hex_stability_carbon h ON h.hex_id = sc.hex_id
        ORDER BY sc.hour ASC, sc.priority DESC
    ]]

    for row in db:nrows(sql) do
        local hex_id = row.hex_id
        local hour = row.hour
        local priority = row.priority
        local carbon_band = row.carbon_band
        local v_residual = row.v_residual

        -- Simple corridor-safe dispatch predicate:
        -- skip RED_BAND hexes with high residual.
        local safe =
            (carbon_band ~= "RED_BAND" or v_residual < 0.8)

        if safe then
            print(string.format(
                "Dispatch workload to %s at hour %d (priority=%.3f, band=%s, V=%.3f)",
                hex_id, hour, priority, carbon_band, v_residual))
            -- In a real FOG router, this would enqueue workloads to a message bus.
        else
            print(string.format(
                "Skip hex %s at hour %d (band=%s, V=%.3f) due to corridor constraints",
                hex_id, hour, carbon_band, v_residual))
        end
    end
end

local function run_fog_router()
    local db = open_db()
    local json = call_cpp_predictor()
    local schedule = parse_schedule(json)
    populate_schedule_cache(db, schedule)
    dispatch_workloads(db)
    close_db(db)
end

-- Entry point
run_fog_router()
