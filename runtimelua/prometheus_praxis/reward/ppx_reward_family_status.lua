local ffi = require("ffi")
local json = require("cjson")      -- or any pure Lua JSON parser

-- Load the ecorestorationindex FFI (non‑actuating)
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

local function decode_json_cstring(ptr)
  if ptr == nil then return nil, "ffi_call_failed" end
  local s = ffi.string(ptr)
  ecoindex.ecoindex_free_cstring(ptr)
  local ok, decoded = pcall(json.decode, s)
  if not ok then return nil, "json_decode_error" end
  return decoded
end

-- Placeholder: load reward family shard from ALN/JSON store.
-- In a real runtime this would parse the ALN or a pre‑computed JSON export.
local function load_reward_family(family_id)
  -- Example structure corresponding to ppx.nanoswarm.reward.family.v1
  return {
    family_id = family_id,
    functions = {
      { function_label = "ecoreward.nanoswarm.data_as_labor.window.v1" },
      { function_label = "ecoreward.nanoswarm.data_as_labor.window.ffi.v1" }
    },
    specs = {
      { spec_id = "PPX-NS-HEALTH-DAL-2026-V1" }
    },
    assets = {
      {
        asset_id = "asset-ko-001",
        owner_did = "did:ppx:host123",
        ker_score = 0.87,
        eco_impact_score = 0.93,
        data_as_labor_eligible = true
      }
    }
  }
end

-- Main diagnostic entrypoint
function get_nanoswarm_reward_family_status(params)
  local family_id = params.family_id
  local db_path = params.db_path

  local family = load_reward_family(family_id)
  if not family then
    return { error = "family_not_found" }
  end

  -- 1. Count functions, specs, assets
  local function_count = #family.functions
  local spec_count     = #family.specs
  local asset_count    = #family.assets
  local dal_count      = 0
  local ker_sum        = 0.0
  local eco_sum        = 0.0

  for _, asset in ipairs(family.assets) do
    if asset.data_as_labor_eligible then
      dal_count = dal_count + 1
    end
    ker_sum = ker_sum + asset.ker_score
    eco_sum = eco_sum + asset.eco_impact_score
  end

  local mean_ker = asset_count > 0 and (ker_sum / asset_count) or 0.0
  local mean_eco = asset_count > 0 and (eco_sum / asset_count) or 0.0

  -- 2. Query a few recent workload windows (fixed nodes for demonstration)
  --    In practice, nodes would be derived from family function metadata or asset provenance.
  local nodes = { "ns-node-phoenix-01" }   -- example node tied to this reward family
  local t_end_utc   = "2026-06-27T01:00:00Z"
  local t_start_utc = "2026-06-27T00:00:00Z"
  local windows_checked = 0
  local total_accept = 0.0
  local total_vt_delta = 0.0

  for _, node_id in ipairs(nodes) do
    local c_db   = ffi.new("const char[?]", #db_path + 1, db_path)
    local c_node = ffi.new("const char[?]", #node_id + 1, node_id)
    local c_start = ffi.new("const char[?]", #t_start_utc + 1, t_start_utc)
    local c_end   = ffi.new("const char[?]", #t_end_utc + 1, t_end_utc)

    local ptr = ecoindex.ecoindex_summarize_workload_window_json(
      c_db, c_node, c_start, c_end
    )
    local decoded, err = decode_json_cstring(ptr)
    if decoded and not decoded.error then
      windows_checked = windows_checked + 1
      total_accept    = total_accept + (decoded.accept_fraction or 0.0)
      total_vt_delta  = total_vt_delta + (decoded.mean_delta_vt or 0.0)
    end
  end

  local mean_accept = windows_checked > 0 and (total_accept / windows_checked) or 0.0
  local mean_vt     = windows_checked > 0 and (total_vt_delta / windows_checked) or 0.0

  return {
    family_id                  = family_id,
    function_count             = function_count,
    spec_count                 = spec_count,
    asset_count                = asset_count,
    data_as_labor_asset_count  = dal_count,
    mean_ker_score             = mean_ker,
    mean_eco_impact_score      = mean_eco,
    recent_windows_checked     = windows_checked,
    mean_accept_fraction       = mean_accept,
    mean_vt_delta              = mean_vt
  }
end

return { get_nanoswarm_reward_family_status = get_nanoswarm_reward_family_status }
