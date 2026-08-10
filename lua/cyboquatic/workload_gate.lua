-- File: lua/cyboquatic/workload_gate.lua
local ffi = require("ffi")

ffi.cdef[[
typedef struct WorkloadInput {
  double flow_m3_s;
  double lift_m;
  double efficiency;
  double runtime_s;
  double voltage_drop_v;
  double renewable_fraction;
  double embodied_carbon_g_per_j;
  double biodiversity_risk;
} WorkloadInput;

typedef struct WorkloadAssessment {
  double energyreq_j;
  double delta_vt;
  double knowledge_factor;
  double eco_impact_value;
  uint8_t accepted;
} WorkloadAssessment;

int32_t assess_workload(
  const WorkloadInput* input,
  WorkloadAssessment* output
);
]]

local M = {}
local OWNER_DID = "bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7"

local function finite_number(value)
  return type(value) == "number"
      and value == value
      and value ~= math.huge
      and value ~= -math.huge
end

local function source_directory()
  local source = debug.getinfo(1, "S").source
  if source:sub(1, 1) ~= "@" then
    return nil
  end
  return source:sub(2):match("^(.*[/\\])")
end

local function load_corridor_validator(directory)
  local ok, module_or_error = pcall(
    require, "cyboquatic.generated.workload_corridor_validator")
  if ok then
    return module_or_error
  end

  if directory then
    local loader, load_error = loadfile(
      directory .. "generated/workload_corridor_validator.lua")
    if loader then
      local module = loader()
      if type(module) == "table"
          and type(module.validate_workload_frame) == "function" then
        return module
      end
      return nil, "generated validator does not export validate_workload_frame"
    end
    return nil, tostring(load_error)
  end

  return nil, tostring(module_or_error)
end

local function load_core(directory)
  local candidates = {}
  local configured = os.getenv("CYBOQUATIC_CORE_LIBRARY")

  if configured and configured ~= "" then
    candidates[#candidates + 1] = configured
  end
  if directory then
    local root = directory .. "../../"
    candidates[#candidates + 1] =
      root .. "cyboquatic-core/target/release/libcyboquatic_core.so"
    candidates[#candidates + 1] =
      root .. "cyboquatic-core/target/release/libcyboquatic_core.dylib"
    candidates[#candidates + 1] =
      root .. "cyboquatic-core/target/release/cyboquatic_core.dll"
  end
  candidates[#candidates + 1] = "cyboquatic_core"

  local failures = {}
  for _, candidate in ipairs(candidates) do
    local ok, library_or_error = pcall(ffi.load, candidate)
    if ok then
      return library_or_error
    end
    failures[#failures + 1] = candidate .. ": " .. tostring(library_or_error)
  end
  return nil, table.concat(failures, " | ")
end

local function input_value(sample, snake_name, camel_name)
  local value = sample[snake_name]
  if value == nil then
    value = sample[camel_name]
  end
  return value
end

local directory = source_directory()
local corridor, corridor_load_error = load_corridor_validator(directory)
local core, core_load_error = load_core(directory)

function M.evaluate(sample)
  if not core then
    return {
      accepted = false,
      route = "human-review",
      reason = "cyboquatic-core library unavailable",
      detail = core_load_error
    }
  end
  if not corridor then
    return {
      accepted = false,
      route = "human-review",
      reason = "ALN workload validator unavailable",
      detail = corridor_load_error
    }
  end
  if type(sample) ~= "table" then
    return {
      accepted = false,
      route = "human-review",
      reason = "workload sample must be a table"
    }
  end

  local node_id = input_value(sample, "node_id", "nodeId")
  local canal_node = input_value(sample, "canal_node", "canalNode") or node_id
  local owner_did = input_value(sample, "owner_did", "ownerDid") or OWNER_DID
  if type(node_id) ~= "string" or node_id == "" then
    return {
      accepted = false,
      route = "human-review",
      reason = "node_id must be a nonempty string"
    }
  end

  local values = {
    flow_m3_s = input_value(sample, "flow_m3_s", "flowM3s"),
    lift_m = input_value(sample, "lift_m", "liftM"),
    efficiency = sample.efficiency,
    runtime_s = input_value(sample, "runtime_s", "runtimeS"),
    voltage_drop_v = input_value(sample, "voltage_drop_v", "voltageDropV"),
    renewable_fraction = input_value(sample, "renewable_fraction", "renewableFraction"),
    embodied_carbon_g_per_j = input_value(
      sample, "embodied_carbon_g_per_j", "embodiedCarbonGPerJ"),
    biodiversity_risk = input_value(sample, "biodiversity_risk", "biodiversityRisk"),
    K = sample.K,
    E = sample.E,
    R = sample.R,
    fog_confidence = input_value(sample, "fog_confidence", "fogConfidence")
  }

  for field, value in pairs(values) do
    if not finite_number(value) then
      return {
        accepted = false,
        route = "human-review",
        reason = "missing or non-finite field: " .. field
      }
    end
  end

  local input = ffi.new("WorkloadInput")
  input.flow_m3_s = values.flow_m3_s
  input.lift_m = values.lift_m
  input.efficiency = values.efficiency
  input.runtime_s = values.runtime_s
  input.voltage_drop_v = values.voltage_drop_v
  input.renewable_fraction = values.renewable_fraction
  input.embodied_carbon_g_per_j = values.embodied_carbon_g_per_j
  input.biodiversity_risk = values.biodiversity_risk

  local assessment = ffi.new("WorkloadAssessment")
  local status = core.assess_workload(input, assessment)
  if status ~= 0 then
    return {
      accepted = false,
      route = "human-review",
      reason = "cyboquatic-core rejected workload telemetry",
      status = tonumber(status)
    }
  end

  local frame = {
    owner_did = owner_did,
    canal_node = canal_node,
    energyreqJ = tonumber(assessment.energyreq_j),
    deltaVt = tonumber(assessment.delta_vt),
    K = values.K,
    E = values.E,
    R = values.R,
    fog_confidence = values.fog_confidence,
    eco_impact_value = tonumber(assessment.eco_impact_value)
  }

  local corridor_valid, reason = corridor.validate_workload_frame(frame)
  local accepted = assessment.accepted ~= 0 and corridor_valid

  return {
    accepted = accepted,
    route = accepted and "canal-restoration" or "human-review",
    reason = accepted and "" or reason or "cyboquatic-core workload corridor not satisfied",
    nodeId = node_id,
    energyreqJ = frame.energyreqJ,
    deltaVt = frame.deltaVt,
    knowledgeFactor = tonumber(assessment.knowledge_factor),
    ecoImpactValue = frame.eco_impact_value,
    fogConfidence = frame.fog_confidence,
    frame = frame
  }
end

return M
