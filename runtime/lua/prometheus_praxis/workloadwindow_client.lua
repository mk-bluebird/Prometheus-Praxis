-- Filename: runtime/lua/prometheus_praxis/workloadwindow_client.lua
-- License: MIT OR Apache-2.0
-- Pure, non-actuating Lua client for workload window functions.
-- Aligned with ecosafety.workload.window.list.input.v1 and
-- ecosafety.workload.window.summary.output.v1.

local M = {}

--- Get workload node windows.
-- @param input table shaped like ecosafety.workload.window.list.input.v1:
--   {
--     shardid = string?,
--     nodeid = string?,
--     assetid = string?,
--     window_start_utc_min = number?,
--     window_end_utc_max = number?,
--     json_path = string?  -- optional path to read from
--   }
-- @return table of rows shaped like ecosafety.workload.node.window.v1
function M.get_workload_node_windows(input)
  -- Stub implementation: returns empty list or reads from JSON file if provided.
  if input and input.json_path then
    -- In a real implementation, this would read and parse the JSON file.
    -- For now, return an empty list as a pure stub.
    return {}
  end
  
  -- Pure stub: return empty list without any side effects.
  return {}
end

--- Summarize workload node windows.
-- @param input table shaped like ecosafety.workload.window.summary.input.v1:
--   {
--     nodeid = string,
--     window_start_utc = number,
--     window_end_utc = number,
--     json_path = string?  -- optional path to read from
--   }
-- @return table shaped like ecosafety.workload.window.summary.output.v1
function M.summarize_workload_node_window(input)
  -- Stub implementation: returns zeroed summary or reads from JSON file if provided.
  if input and input.json_path then
    -- In a real implementation, this would read and parse the JSON file.
    -- For now, return a zeroed summary as a pure stub.
  end
  
  -- Pure stub: return zeroed summary without any side effects.
  return {
    nodeid = (input and input.nodeid) or "",
    window_count = 0,
    mean_energy_req_j = 0.0,
    mean_energy_surplus_j = 0.0,
    mean_accepted_fraction = 0.0,
    mean_rejected_fraction = 0.0,
    mean_rerouted_fraction = 0.0,
    mean_vt_before = 0.0,
    mean_vt_after = 0.0,
    mean_delta_vt = 0.0,
    hard_violation_count = 0,
    soft_violation_count = 0,
  }
end

return M
