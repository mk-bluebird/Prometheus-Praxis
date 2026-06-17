-- filename: lua_cyboquatic_spine.lua
-- destination: ecorestorationshard/lua/lua_cyboquatic_spine.lua

local ffi = require("ffi")

ffi.cdef[[
char* cybo_get_node_blastradius(const char* dbpath, const char* nodeid);
char* cybo_get_workload_window(const char* dbpath, const char* nodeid);
void  cybo_free_json(char* ptr);
]]

-- Adjust name to actual built artifact on each platform (e.g. libcyboquatic_spine.so)
local lib = ffi.load("cyboquatic_spine")

local M = {}

local function read_json(ptr)
    if ptr == nil then
        return nil, "null pointer"
    end
    local s = ffi.string(ptr)
    lib.cybo_free_json(ptr)
    return s, nil
end

function M.get_node_blastradius(dbpath, nodeid)
    local c = lib.cybo_get_node_blastradius(dbpath, nodeid)
    return read_json(c)
end

function M.get_workload_window(dbpath, nodeid)
    local c = lib.cybo_get_workload_window(dbpath, nodeid)
    return read_json(c)
end

return M
