-- File: lua/eco_restoration/lane_threshold_scheduler.lua

local sqlite3 = require("lsqlite3")

local function season_for_month(month)
    return (month >= 7 and month <= 9) and "monsoon" or "dry"
end

local function load_schedule(database_path, hour, month, canal_flow)
    local database = assert(sqlite3.open(database_path))
    local season = season_for_month(month)
    local statement = assert(database:prepare(
        "SELECT * FROM lane_threshold_schedule WHERE season=? " ..
        "AND ((hour_start<=hour_end AND ? BETWEEN hour_start AND hour_end) " ..
        "OR (hour_start>hour_end AND (? >= hour_start OR ? <= hour_end))) " ..
        "AND ? BETWEEN flow_min AND flow_max LIMIT 1;"
    ))
    statement:bind_values(season, hour, hour, hour, canal_flow)
    local row = statement:nrows()()
    statement:finalize()
    database:close()
    if not row then error("no matching threshold schedule") end
    return row
end

local function clamp01(value)
    return math.max(0.0, math.min(1.0, value))
end

local function schedule(database_path, hour, month, canal_flow, solar_fraction)
    local row = load_schedule(database_path, hour, month, canal_flow)
    solar_fraction = clamp01(solar_fraction)
    local flow_fraction = clamp01((canal_flow - row.flow_min) / math.max(1e-12, row.flow_max - row.flow_min))

    return {
        k_min = clamp01(row.k_min + row.solar_k_gain * solar_fraction + row.flow_k_gain * flow_fraction),
        e_min = clamp01(row.e_min + row.solar_e_gain * solar_fraction + row.flow_e_gain * flow_fraction),
        r_max = clamp01(row.r_max + row.solar_r_gain * solar_fraction + row.flow_r_gain * flow_fraction)
    }
end

return { schedule = schedule }
