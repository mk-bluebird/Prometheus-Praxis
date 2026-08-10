-- File: crates/ker-composition/lua/ker_composition_validator.lua
local M = {}

local THETA = 0.30
local RULE_ID = "keroplusgeomminmaxv1"

local function finite_number(value)
  return type(value) == "number"
    and value == value
    and value ~= math.huge
    and value ~= -math.huge
end

local function unit_interval(value)
  return finite_number(value) and value >= 0.0 and value <= 1.0
end

local function canonical_lane(value)
  if value == "PILOT" then
    return "EXPPROD"
  end
  return value
end

local function valid_lane(value)
  value = canonical_lane(value)
  return value == "RESEARCH" or value == "EXPPROD" or value == "PROD"
end

local function field(table_value, modern_name, legacy_name)
  if table_value[modern_name] ~= nil then
    return table_value[modern_name]
  end
  return table_value[legacy_name]
end

local function normalize_payload(payload)
  if type(payload) ~= "table"
    or type(payload.left) ~= "table"
    or type(payload.right) ~= "table" then
    return nil
  end

  local source = payload.composition or payload.comp
  if type(source) ~= "table" then
    return nil
  end

  local left = {
    k = field(payload.left, "k", "K"),
    e = field(payload.left, "e", "E"),
    r = field(payload.left, "r", "R"),
    lane = canonical_lane(payload.left.lane),
  }
  local right = {
    k = field(payload.right, "k", "K"),
    e = field(payload.right, "e", "E"),
    r = field(payload.right, "r", "R"),
    lane = canonical_lane(payload.right.lane),
  }
  local composition = {
    k = field(source, "k_combined", "Kcombined"),
    e = field(source, "e_combined", "Ecombined"),
    r = field(source, "r_combined", "Rcombined"),
    members = source.members,
    rule_id = field(source, "rule_id", "ruleid"),
    lane = canonical_lane(source.lane),
    evidencehex = source.evidencehex,
  }

  return left, right, composition
end

local function valid_particle(particle)
  return unit_interval(particle.k)
    and unit_interval(particle.e)
    and unit_interval(particle.r)
    and valid_lane(particle.lane)
end

local function valid_composition(composition)
  return unit_interval(composition.k)
    and unit_interval(composition.e)
    and unit_interval(composition.r)
    and valid_lane(composition.lane)
    and type(composition.members) == "string"
    and composition.members ~= ""
    and composition.rule_id == RULE_ID
end

local function risk_cap(left, right, composition)
  if left.r <= THETA and right.r <= THETA then
    return composition.r <= THETA
  end
  return true
end

local function knowledge_and_impact_bounds(left, right, composition)
  local k_min = math.min(left.k, right.k)
  local k_max = math.max(left.k, right.k)
  return composition.k >= k_min
    and composition.k <= k_max
    and composition.e <= left.e
    and composition.e <= right.e
end

local function risk_monotone(left, right, composition)
  return composition.r >= left.r and composition.r >= right.r
end

local function lane_safe(left, right, composition)
  if composition.lane == "PROD" then
    return left.lane == "PROD" and right.lane == "PROD"
  end
  if composition.lane == "EXPPROD" then
    return left.lane ~= "RESEARCH" and right.lane ~= "RESEARCH"
  end
  return true
end

local function positive_ker_margin(composition)
  return composition.k * composition.e > composition.r
end

function M.validate(payload)
  local left, right, composition = normalize_payload(payload)
  if left == nil
    or not valid_particle(left)
    or not valid_particle(right)
    or not valid_composition(composition) then
    return false
  end

  return risk_cap(left, right, composition)
    and knowledge_and_impact_bounds(left, right, composition)
    and risk_monotone(left, right, composition)
    and lane_safe(left, right, composition)
    and positive_ker_margin(composition)
end

return M
