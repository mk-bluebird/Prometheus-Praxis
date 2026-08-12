-- File: lua/eco_restoration/lane_ffi_benchmark.lua

local ffi = require("ffi")
local library_path = assert(arg[1], "usage: luajit lane_ffi_benchmark.lua lane_c_abi_library")

ffi.cdef[[
typedef unsigned long long uint64_t;
typedef long long int64_t;
typedef unsigned int uint32_t;
typedef unsigned char uint8_t;
typedef struct {
  uint64_t hex_anchor; int64_t observed_unix_s;
  double knowledge_factor, eco_impact_value, risk_of_harm, energy_kwh, carbon_g;
  double heat_risk, water_risk, delta_v;
  uint32_t decision_code, sample_count;
  char frame_id[64], owner_did[64], source_id[32];
  uint8_t reserved[16];
} TelemetryC;
int evaluate_telemetry_batch(const TelemetryC*, size_t, uint8_t*);
]]

local lane = ffi.load(library_path)
local count = 100000
local frames = ffi.new("TelemetryC[?]", count)
local actions = ffi.new("uint8_t[?]", count)

for i = 0, count - 1 do
  frames[i].knowledge_factor = 0.5 + (i % 50) / 100
  frames[i].eco_impact_value = 0.4 + (i % 60) / 100
  frames[i].risk_of_harm = (i % 80) / 100
end

local started = os.clock()
assert(lane.evaluate_telemetry_batch(frames, count, actions) == 0)
local ffi_seconds = os.clock() - started

started = os.clock()
local pure_lua = {}
for i = 0, count - 1 do
  local frame = frames[i]
  pure_lua[i] = frame.knowledge_factor < 0.60 or frame.eco_impact_value < 0.55 or frame.risk_of_harm > 0.70
      and 2 or (frame.risk_of_harm > 0.35 or frame.eco_impact_value < 0.63) and 1 or 0
end
local lua_seconds = os.clock() - started

print(string.format(
  '{"frames":%d,"ffi_seconds":%.6f,"lua_seconds":%.6f,"speedup":%.3f}',
  count, ffi_seconds, lua_seconds, lua_seconds / math.max(ffi_seconds, 1e-12)
))
