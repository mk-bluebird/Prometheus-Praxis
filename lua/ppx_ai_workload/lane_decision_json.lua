-- File: lua/ppx_ai_workload/lane_decision_json.lua
local json = require("dkjson")

local M = {}

function M.decode_and_validate(text)
  local record, position, error_message = json.decode(text, 1, nil)
  if not record then return nil, error_message end
  local required = {"schema_id", "machine_id", "station_id", "timestamp_utc", "hex_anchor",
    "k_knowledge", "e_eco_impact", "r_risk", "roh", "vt_current", "vt_next",
    "delta_vt", "lane", "action", "reason_code"}
  for _, key in ipairs(required) do
    if record[key] == nil then return nil, "missing_" .. key end
  end
  if record.schema_id ~= "ppx.ai_workload.lane.v1" then return nil, "schema_id" end
  for _, key in ipairs({"k_knowledge", "e_eco_impact", "r_risk", "roh"}) do
    if type(record[key]) ~= "number" or record[key] < 0 or record[key] > 1 then
      return nil, "invalid_" .. key
    end
  end
  return record, position
end

return M
