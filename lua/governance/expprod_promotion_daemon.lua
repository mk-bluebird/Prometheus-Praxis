-- File: lua/governance/expprod_promotion_daemon.lua
-- Lua Governance Daemon for Automatic EXPPROD Promotion
-- Scans module_ker_profile, checks observation windows, and writes
-- promotion recommendations when KER thresholds are met.
--
-- Assumes Lua has access to a SQLite-compatible binding, exposed here
-- as a simple `db` object with :exec and :query methods. In practice,
-- this could be luasql, lsqlite3, or a custom binding.

local sqlite3 = require("lsqlite3")

local DB_PATH = "prometheus_praxis.db"

local PROMOTION_THRESHOLDS = {
    s_min = 0.20,   -- KER scalar minimum for EXPPROD
    k_min = 0.70,   -- knowledge
    e_min = 0.70,   -- eco-efficiency
    r_max = 0.50    -- risk-of-harm
}

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

-- Check if module meets EXPPROD KER thresholds over its recent observation window.
local function module_meets_expprod_ker(db, module_id)
    -- Example: compute window averages from a telemetry view.
    local stmt = db:prepare([[
        SELECT AVG(ker_k) AS k_avg,
               AVG(ker_e) AS e_avg,
               AVG(ker_r) AS r_avg,
               AVG(ker_s) AS s_avg
        FROM module_ker_profile_window
        WHERE module_id = ?
    ]])
    stmt:bind_values(module_id)
    local k_avg, e_avg, r_avg, s_avg = nil, nil, nil, nil

    for row in stmt:nrows() do
        k_avg = row.k_avg
        e_avg = row.e_avg
        r_avg = row.r_avg
        s_avg = row.s_avg
    end
    stmt:finalize()

    if not k_avg or not e_avg or not r_avg or not s_avg then
        return false
    end

    if s_avg < PROMOTION_THRESHOLDS.s_min then return false end
    if k_avg < PROMOTION_THRESHOLDS.k_min then return false end
    if e_avg < PROMOTION_THRESHOLDS.e_min then return false end
    if r_avg > PROMOTION_THRESHOLDS.r_max then return false end

    return true
end

-- Write a promotion recommendation into module_lane_history (no auto-execution).
local function write_promotion_recommendation(db, module_id, from_lane, to_lane, reason)
    local stmt = db:prepare([[
        INSERT INTO module_lane_history
        (advisory_run_id, module_id, from_lane, to_lane, reason, ts)
        VALUES (?, ?, ?, ?, ?, CURRENT_TIMESTAMP)
    ]])
    local run_id = "lua_expprod_daemon_" .. os.date("%Y%m%d_%H%M%S")
    stmt:bind_values(run_id, module_id, from_lane, to_lane, reason)
    stmt:step()
    stmt:finalize()
end

-- Main daemon loop: scan RESEARCH/EXPPROD modules and emit promotion recommendations.
local function run_expprod_promotion_daemon()
    local db = open_db()

    local sql = [[
        SELECT module_id, lane
        FROM module_ker_profile
        WHERE lane IN ('RESEARCH', 'EXPPROD')
    ]]

    for row in db:nrows(sql) do
        local module_id = row.module_id
        local lane = row.lane

        if lane == "RESEARCH" or lane == "EXPPROD" then
            local ok = module_meets_expprod_ker(db, module_id)
            if ok then
                local from_lane = lane
                local to_lane = (lane == "RESEARCH") and "EXPPROD" or "PROD"
                local reason = "Lua FOG daemon: KER observation window meets EXPPROD thresholds."

                print(string.format(
                    "Promotion candidate: %s %s -> %s (reason=%s)",
                    module_id, from_lane, to_lane, reason))

                write_promotion_recommendation(db, module_id, from_lane, to_lane, reason)
            end
        end
    end

    close_db(db)
end

-- Entry point (for cron/FOG daemon)
run_expprod_promotion_daemon()
