-- File: lua/governance/eco_wealth_lane_policy_engine.lua
-- Lua Overlay for Eco-Wealth Lane Policy Engine
-- Extends existing policy engine to incorporate eco_corridor_score from
-- a combined heat/carbon/KER view, influencing lane recommendations.

local sqlite3 = require("lsqlite3")

local DB_PATH = "prometheus_praxis.db"

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

-- Fetch eco_corridor_score and current lane for all modules.
local function fetch_module_corridor_scores(db)
    local modules = {}
    local sql = [[
        SELECT m.module_id,
               m.lane,
               v.eco_corridor_score
        FROM module_ker_profile m
        JOIN v_combined_heat_carbon_ker v
          ON v.module_id = m.module_id
    ]]
    for row in db:nrows(sql) do
        table.insert(modules, {
            module_id = row.module_id,
            lane = row.lane,
            eco_corridor_score = row.eco_corridor_score or 0.0
        })
    end
    return modules
end

-- Decide lane policy based on eco_corridor_score and existing KER lane.
local function recommend_lane(module)
    local lane = module.lane
    local score = module.eco_corridor_score

    -- Example overlay policy:
    --  - High score (>0.8) in RESEARCH or EXPPROD suggests promotion.
    --  - Low score (<0.4) in PROD suggests demotion or stricter review.
    local recommended_lane = lane
    local reason = "No change."

    if lane == "RESEARCH" and score > 0.8 then
        recommended_lane = "EXPPROD"
        reason = "Eco corridor score high; recommend promotion to EXPPROD."
    elseif lane == "EXPPROD" and score > 0.85 then
        recommended_lane = "PROD"
        reason = "Eco corridor score very high; recommend promotion to PROD."
    elseif lane == "PROD" and score < 0.4 then
        recommended_lane = "EXPPROD"
        reason = "Eco corridor score low; recommend demotion to EXPPROD or additional review."
    end

    return recommended_lane, reason
end

-- Write overlay recommendations into eco_wealth_lane_history (no auto execution).
local function write_lane_overlay(db, module_id, from_lane, to_lane, eco_corridor_score, reason)
    local stmt = db:prepare([[
        INSERT INTO eco_wealth_lane_history
        (run_id, module_id, from_lane, to_lane, eco_corridor_score, reason, ts)
        VALUES (?, ?, ?, ?, ?, ?, CURRENT_TIMESTAMP)
    ]])
    local run_id = "eco_wealth_overlay_" .. os.date("%Y%m%d_%H%M%S")
    stmt:bind_values(run_id, module_id, from_lane, to_lane, eco_corridor_score, reason)
    stmt:step()
    stmt:finalize()
end

local function run_eco_wealth_lane_overlay()
    local db = open_db()
    local modules = fetch_module_corridor_scores(db)

    for _, m in ipairs(modules) do
        local to_lane, reason = recommend_lane(m)
        if to_lane ~= m.lane then
            print(string.format(
                "Eco-wealth overlay: module %s %s -> %s (score=%.3f, reason=%s)",
                m.module_id, m.lane, to_lane, m.eco_corridor_score, reason))
            write_lane_overlay(db, m.module_id, m.lane, to_lane, m.eco_corridor_score, reason)
        else
            print(string.format(
                "Eco-wealth overlay: module %s remains in %s (score=%.3f)",
                m.module_id, m.lane, m.eco_corridor_score))
        end
    end

    close_db(db)
end

-- Entry point
run_eco_wealth_lane_overlay()
