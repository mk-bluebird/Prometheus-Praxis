-- filename: lua/chat_kermap.lua
-- destination: ecorestorationshard/lua/chat_kermap.lua
-- purpose:
--   Thin Lua helper for AI-chat agents to call the kerresidual cdylib JSON APIs
--   and return compact K/E/R summaries for a node or shard, minimizing tokens.

local ffi = require("ffi")

ffi.cdef[[
char* econetgetworkloadtrendsfornode(const char* dbpath, const char* nodeid);
char* econetgetkertargets(const char* dbpath, const char* reponame);
void  econetfreejson(char* ptr);
]]

-- The shared library name is kept stable and minimal for production wiring.
local lib = ffi.load("ecorestorationshard")

local M = {}

local function call_and_decode(cfunc, a, b)
    if a == nil or b == nil then
        return nil, "invalid arguments"
    end

    local ptr = cfunc(a, b)
    if ptr == nil then
        return nil, "null from cdylib"
    end

    local ok, json = pcall(ffi.string, ptr)
    lib.econetfreejson(ptr)

    if not ok then
        return nil, "failed to decode JSON from cdylib"
    end

    return json, nil
end

function M.get_kertargets(dbpath, reponame)
    return call_and_decode(lib.econetgetkertargets, dbpath, reponame)
end

function M.get_workload_trends(dbpath, nodeid)
    return call_and_decode(lib.econetgetworkloadtrendsfornode, dbpath, nodeid)
end

return M
