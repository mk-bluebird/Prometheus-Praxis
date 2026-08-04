-- File: lua/fog_router_trend.lua
-- Lua FOG router extended with recent history from SQL and trend-based routing.

local FOGRouterTrend = {}

-- Helper: run sqlite3 command and capture output lines.
local function fetch_recent_samples(db_path, node_id, canal_id, limit)
    local cmd = string.format(
        "sqlite3 %s \"SELECT timestamp_s, dissolved_o2_mg_l, pfas_ug_l " ..
        "FROM fog_media_ext WHERE node_id='%s' AND canal_id='%s' " ..
        "ORDER BY timestamp_s DESC LIMIT %d;\"",
        db_path, node_id, canal_id, limit
    )
    local handle = io.popen(cmd)
    if not handle then
        return {}
    end
    local samples = {}
    for line in handle:lines() do
        -- Expect pipe-separated or space-separated output; sqlite default is |.
        local ts, do2, pfas = line:match("([^|]+)|([^|]+)|([^|]+)")
        if ts and do2 and pfas then
            table.insert(samples, {
                timestamp_s = tonumber(ts),
                dissolved_o2_mg_l = tonumber(do2),
                pfas_ug_l = tonumber(pfas)
            })
        }
    end
    handle:close()
    return samples
end

-- Simple linear regression: y = a + b t over recent samples.
local function linear_regression(samples, field)
    local n = #samples
    if n < 2 then
        return 0.0 -- insufficient data, no trend
    end
    local sum_t, sum_y, sum_t2, sum_ty = 0.0, 0.0, 0.0, 0.0
    for i, s in ipairs(samples) do
        local t = s.timestamp_s
        local y = s[field]
        sum_t = sum_t + t
        sum_y = sum_y + y
        sum_t2 = sum_t2 + t * t
        sum_ty = sum_ty + t * y
    end
    local denom = n * sum_t2 - sum_t * sum_t
    if denom == 0.0 then
        return 0.0
    end
    local b = (n * sum_ty - sum_t * sum_y) / denom
    return b
end

-- Trend-based FOG routing decision.
-- Returns "FOG:TREND_IMPROVING" if DO increasing and PFAS decreasing,
-- "FOG:TREND_DEGRADING" otherwise.
function FOGRouterTrend.route_with_trend(db_path, node_id, canal_id)
    local samples = fetch_recent_samples(db_path, node_id, canal_id, 6)
    if #samples < 2 then
        return "FOG:TREND_UNKNOWN"
    end

    local b_do = linear_regression(samples, "dissolved_o2_mg_l")
    local b_pfas = linear_regression(samples, "pfas_ug_l")

    local improving = (b_do > 0.0) and (b_pfas < 0.0)
    if improving then
        return "FOG:TREND_IMPROVING"
    else
        return "FOG:TREND_DEGRADING"
    end
end

-- Example usage:
-- local decision = FOGRouterTrend.route_with_trend("./data/cyboquatic_workload.db", "node-01", "canal-01")
-- print("FOG trend-based decision:", decision)

return FOGRouterTrend
