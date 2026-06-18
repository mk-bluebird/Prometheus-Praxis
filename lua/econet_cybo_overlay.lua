-- filename: lua/econet_cybo_overlay.lua
-- destination: mk-bluebird/eco_restoration_shard/ecorestorationshard/lua/econet_cybo_overlay.lua
-- target-repo: github.com/mk-bluebird/eco_restoration_shard
-- Purpose:
-- Minimal LuaJIT FFI harness for the EcoNet Cyboquatic read-only cdylib.
-- Visual-only JSON access for KER targets, blast-radius overlays,
-- workload trends, and Cyboquatic eco-metrics. No actuation.

local ffi = require("ffi")

ffi.cdef[[
char* econet_get_ker_targets(const char* dbpath, const char* reponame);
char* econet_get_blast_radius_for_node(const char* dbpath, const char* nodeid);
char* econet_get_workload_trends_for_node(const char* dbpath, const char* nodeid);
char* econet_get_cybo_node_eco_metrics(const char* dbpath, const char* nodeid);
void  econet_free_json(char* ptr);
]]

-- The shared library name must match the Rust cdylib artifact name.
-- Built from the eco_restoration_shard repo as libeco_restoration_shard.*.
local lib = ffi.load("eco_restoration_shard")

local M = {}

local function read_json_ptr(ptr)
  if ptr == nil then
    return nil, "null pointer"
  end
  local s = ffi.string(ptr)
  lib.econet_free_json(ptr)
  return s, nil
end

function M.get_ker_targets(dbpath, reponame)
  if dbpath == nil or reponame == nil then
    return nil, "dbpath and reponame are required"
  end
  local c = lib.econet_get_ker_targets(dbpath, reponame)
  return read_json_ptr(c)
end

function M.get_blast_radius(dbpath, nodeid)
  if dbpath == nil or nodeid == nil then
    return nil, "dbpath and nodeid are required"
  end
  local c = lib.econet_get_blast_radius_for_node(dbpath, nodeid)
  return read_json_ptr(c)
end

function M.get_workload_trends(dbpath, nodeid)
  if dbpath == nil or nodeid == nil then
    return nil, "dbpath and nodeid are required"
  end
  local c = lib.econet_get_workload_trends_for_node(dbpath, nodeid)
  return read_json_ptr(c)
end

function M.get_cybo_node_eco_metrics(dbpath, nodeid)
  if dbpath == nil or nodeid == nil then
    return nil, "dbpath and nodeid are required"
  end
  local c = lib.econet_get_cybo_node_eco_metrics(dbpath, nodeid)
  return read_json_ptr(c)
end

return M
