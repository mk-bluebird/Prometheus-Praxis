-- File: lua/cyboquatic/workload_assess.lua
local gate = require("cyboquatic.workload_gate")

local names = {
  "flow_m3_s", "lift_m", "efficiency", "runtime_s", "voltage_drop_v",
  "renewable_fraction", "embodied_carbon_g_per_j", "biodiversity_risk"
}
local sample = {
  node_id = "phoenix-canal-pump-01",
  owner_did = "bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7",
  canal_node = "phoenix-canal-pump-01",
  K = 0.94, E = 0.91, R = 0.12, fog_confidence = 0.90
}

for index, name in ipairs(names) do
  local value = tonumber(arg[index])
  assert(value, "expected eight numeric telemetry values")
  sample[name] = value
end

local result = gate.evaluate(sample)
assert(result.frame, result.reason or "assessment failed")
print("energyreqJ=" .. result.frame.energyreqJ)
print("deltaVt=" .. result.frame.deltaVt)
print("knowledge_factor=" .. result.frame.knowledgeFactor)
print("eco_impact_value=" .. result.frame.ecoImpactValue)
print("accepted=" .. (result.accepted and "1" or "0"))
