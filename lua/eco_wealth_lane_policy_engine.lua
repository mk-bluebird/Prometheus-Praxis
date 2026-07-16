-- eco_wealth_lane_policy_engine.lua
--
-- Lua policy engine for eco-wealth lane decisions.
-- Inputs:
--   steward: table with fields:
--     id            (string)
--     eco_wealth    (number, in [0,1])
--     lane          (string: "RESEARCH", "PILOT", "PROD")
--     region        (string, optional)
--     scope         (string, optional)
--
-- Output:
--   policy_eval: table with fields:
--     steward_id
--     current_lane
--     eco_wealth
--     decision       ("PROMOTE", "DOWNGRADE", "STAY")
--     target_lane    (or nil)
--     applied_rules  (array of rule ids)
--     timestamp_utc
--
--   This table is serialized to JSON and handed to Rust, which computes
--   policy_eval_hex and signinghex.

local json = require("dkjson")

local function now_timestamp_utc()
  -- Simple UTC timestamp; actual implementation can be replaced by Rust.
  -- Format: YYYY-MM-DDTHH:MM:SSZ
  return os.date("!%Y-%m-%dT%H:%M:%SZ")
end

local function decide_lane(eco_wealth, lane)
  -- Example policy aligned with EcoWealthLanePolicy2026v1 semantics.
  -- This should be updated when the ALN spec is finalized.

  local rules = {}
  local decision = "STAY"
  local target_lane = nil

  if lane == "RESEARCH" then
    if eco_wealth >= 0.6 then
      decision = "PROMOTE"
      target_lane = "PILOT"
      table.insert(rules, "RULE_PROMOTE_RESEARCH_TO_PILOT")
    else
      table.insert(rules, "RULE_STAY_RESEARCH")
    end
  elseif lane == "PILOT" then
    if eco_wealth >= 0.85 then
      decision = "PROMOTE"
      target_lane = "PROD"
      table.insert(rules, "RULE_PROMOTE_PILOT_TO_PROD")
    elseif eco_wealth < 0.4 then
      decision = "DOWNGRADE"
      target_lane = "RESEARCH"
      table.insert(rules, "RULE_DOWNGRADE_PILOT_TO_RESEARCH")
    else
      table.insert(rules, "RULE_STAY_PILOT")
    end
  elseif lane == "PROD" then
    if eco_wealth < 0.7 then
      decision = "DOWNGRADE"
      target_lane = "PILOT"
      table.insert(rules, "RULE_DOWNGRADE_PROD_TO_PILOT")
    else
      table.insert(rules, "RULE_STAY_PROD")
    end
  else
    decision = "STAY"
    table.insert(rules, "RULE_UNKNOWN_LANE")
  end

  return decision, target_lane, rules
end

local function build_policy_eval(steward)
  local eco_wealth = steward.eco_wealth or 0.0
  local lane = steward.lane or "RESEARCH"

  local decision, target_lane, rules = decide_lane(eco_wealth, lane)

  local eval = {
    steward_id = steward.id,
    current_lane = lane,
    eco_wealth = eco_wealth,
    decision = decision,
    target_lane = target_lane,
    applied_rules = rules,
    region = steward.region,
    scope = steward.scope,
    timestamp_utc = now_timestamp_utc()
  }

  return eval
end

local function eval_to_json(eval)
  local encoded, _, err = json.encode(eval, { indent = false })
  if err then
    error("JSON encode error: " .. err)
  end
  return encoded
end

-- Public API:
--   eco_wealth_lane_policy_eval(steward_table) -> eval_table, eval_json
local M = {}

function M.eco_wealth_lane_policy_eval(steward)
  local eval = build_policy_eval(steward)
  local json_str = eval_to_json(eval)
  return eval, json_str
end

return M
