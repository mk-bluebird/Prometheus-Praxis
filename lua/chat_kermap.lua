-- filename: lua/chat_kermap.lua
-- destination: ecorestorationshard/lua/chat_kermap.lua
-- purpose :
--   Thin Lua helper for AI-chat agents to call the kerresidual cdylib JSON APIs
--   and return compact K/E/R summaries for a node or shard, minimizing tokens.

local ffi = require("ffi")

ffi.cdef[[
char* econetgetworkloadtrendsfornode(const char* dbpath, const char* nodeid);
char* econetgetkertargets(const char* dbpath, const char* reponame);
void  econetfreejson(char* ptr);
]]

local lib = ffi.load("ecorestorationshard")

local M = {}

local function call_and_decode(cfunc, a, b)
  local ptr = cfunc(a, b)
  if ptr == nil then
    return nil, "null from cdylib"
  end
  local json = ffi.string(ptr)
  lib.econetfreejson(ptr)
  return json, nil
end

function M.get_kertargets(dbpath, reponame)
  return call_and_decode(lib.econetgetkertargets, dbpath, reponame)
end

function M.get_workload_trends(dbpath, nodeid)
  return call_and_decode(lib.econetgetworkloadtrendsfornode, dbpath, nodeid)
end

return M
