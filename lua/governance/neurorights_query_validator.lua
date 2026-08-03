-- File: lua/governance/neurorights_query_validator.lua
-- Lua Script to Validate Neurorights Query Stats
-- Queries v_neurorights_query_stats and sends an alert via log (or MQTT if available)
-- when neuro-flagged queries exceed configured thresholds.

local sqlite3 = require("lsqlite3")

local DB_PATH = "prometheus_praxis.db"

-- Thresholds for neurorights monitoring.
local THRESHOLDS = {
    max_neuro_queries_per_hour = 100,   -- maximum acceptable neuro-flagged queries per hour
    max_neuro_ratio            = 0.30   -- max fraction of neuro-flagged to total queries
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

-- Placeholder alert sender. In production, replace with MQTT publish.
local function send_alert(message)
    -- Example MQTT hook:
    -- local mqtt = require("mqtt")
    -- mqtt.publish("eco/neurorights/alerts", message)
    -- For now, just log to stdout.
    print("[NEURORIGHTS_ALERT] " .. message)
end

-- Validate neurorights query stats against thresholds.
local function validate_neurorights_stats()
    local db = open_db()

    -- v_neurorights_query_stats is assumed to expose per-hour aggregated stats:
    -- (ts_hour, total_queries, neuro_flagged_queries)
    local sql = [[
        SELECT ts_hour, total_queries, neuro_flagged_queries
        FROM v_neurorights_query_stats
        ORDER BY ts_hour DESC
        LIMIT 24
    ]]

    for row in db:nrows(sql) do
        local ts_hour = row.ts_hour
        local total = row.total_queries or 0
        local neuro = row.neuro_flagged_queries or 0
        local ratio = 0.0
        if total > 0 then
            ratio = neuro / total
        end

        if neuro > THRESHOLDS.max_neuro_queries_per_hour then
            send_alert(string.format(
                "Hour %s: neuro_flagged_queries=%d exceeds max_neuro_queries_per_hour=%d",
                ts_hour, neuro, THRESHOLDS.max_neuro_queries_per_hour))
        end

        if ratio > THRESHOLDS.max_neuro_ratio then
            send_alert(string.format(
                "Hour %s: neuro_ratio=%.3f exceeds max_neuro_ratio=%.3f (neuro=%d total=%d)",
                ts_hour, ratio, THRESHOLDS.max_neuro_ratio, neuro, total))
        end
    end

    close_db(db)
end

-- Entry point (for cron / daemon)
validate_neurorights_stats()
