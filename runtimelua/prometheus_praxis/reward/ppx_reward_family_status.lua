local ffi  = require("ffi")
local json = require("cjson")

ffi.cdef[[
  char* ecoindex_summarize_workload_window_json(
    const char* db_path,
    const char* node_id,
    const char* t_start_utc,
    const char* t_end_utc
  );
  void  ecoindex_free_cstring(char* ptr);
]]

local ecoindex = ffi.load("ecorestorationindex")

local M = {}

local function decode_json_cstring(ptr)
  if ptr == nil then
    return nil, "ffi_call_failed"
  end
  local s = ffi.string(ptr)
  ecoindex.ecoindex_free_cstring(ptr)
  local ok, decoded = pcall(json.decode, s)
  if not ok then
    return nil, "json_decode_error"
  end
  return decoded
end

local function load_reward_family(family_id)
  local path = "core_aln/prometheus_praxis/reward/families/" .. family_id .. ".json"
  local f = io.open(path, "r")
  if not f then
    return nil, "family_not_found"
  end
  local content = f:read("*a")
  f:close()
  local ok, decoded = pcall(json.decode, content)
  if not ok then
    return nil, "family_json_decode_error"
  end
  return decoded
end

function M.get_nanoswarm_reward_family_status(family_id, db_path)
  if type(family_id) ~= "string" or family_id == "" then
    return { error = "invalid_family_id" }
  end
  if type(db_path) ~= "string" or db_path == "" then
    return { error = "invalid_db_path", family_id = family_id }
  end

  local family, ferr = load_reward_family(family_id)
  if not family then
    return { error = ferr or "family_not_found", family_id = family_id }
  end

  local functions = family.functions or {}
  local specs     = family.specs or {}
  local assets    = family.assets or {}
  local linked    = family.linked_nodes or {}

  local function_count = #functions
  local spec_count     = #specs
  local asset_count    = #assets

  local dal_count = 0
  local ker_sum   = 0.0
  local eco_sum   = 0.0

  for _, a in ipairs(assets) do
    if a.data_as_labor_eligible then
      dal_count = dal_count + 1
    end
    if a.ker_score then
      ker_sum = ker_sum + a.ker_score
    end
    if a.eco_impact_score then
      eco_sum = eco_sum + a.eco_impact_score
    end
  end

  local mean_ker = asset_count > 0 and (ker_sum / asset_count) or 0.0
  local mean_eco = asset_count > 0 and (eco_sum / asset_count) or 0.0

  local windows_checked = 0
  local accept_sum      = 0.0
  local vt_delta_sum    = 0.0

  for _, node in ipairs(linked) do
    local node_id = node.id
    local tstart  = node.tstart
    local tend    = node.tend

    if type(node_id) == "string" and type(tstart) == "string" and type(tend) == "string" then
      local c_db    = ffi.new("const char[?]", #db_path + 1, db_path)
      local c_node  = ffi.new("const char[?]", #node_id + 1, node_id)
      local c_start = ffi.new("const char[?]", #tstart + 1, tstart)
      local c_end   = ffi.new("const char[?]", #tend + 1, tend)

      local ptr = ecoindex.ecoindex_summarize_workload_window_json(c_db, c_node, c_start, c_end)
      local summary, serr = decode_json_cstring(ptr)
      if summary and not summary.error then
        windows_checked = windows_checked + 1
        accept_sum   = accept_sum   + (summary.accept_fraction or 0.0)
        vt_delta_sum = vt_delta_sum + (summary.mean_delta_vt or 0.0)
      end
    end
  end

  local mean_accept   = windows_checked > 0 and (accept_sum / windows_checked) or 0.0
  local mean_vt_delta = windows_checked > 0 and (vt_delta_sum / windows_checked) or 0.0

  return {
    family_id                 = family_id,
    function_count            = function_count,
    spec_count                = spec_count,
    asset_count               = asset_count,
    data_as_labor_asset_count = dal_count,
    mean_ker_score            = mean_ker,
    mean_eco_impact_score     = mean_eco,
    recent_windows_checked    = windows_checked,
    mean_accept_fraction      = mean_accept,
    mean_vt_delta             = mean_vt_delta
  }
end

return M
