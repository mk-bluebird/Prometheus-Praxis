-- filename: lua/cyboquatic_eco_overlay.lua
-- destination: eco_restoration_shard/lua/cyboquatic_eco_overlay.lua
--
-- Purpose:
-- - Thin LuaJIT FFI wrapper over cyboquatic_eco_spine cdylib.
-- - For visual-only dashboards and AI-chat-driven diagnostics.

local ffi = require("ffi")

ffi.cdef[[
char* cybo_get_machines_for_region(const char* dbpath, const char* region);
char* cybo_get_machine_windows(const char* dbpath, long long machine_id);
char* cybo_get_ecoperjoule_for_node(const char* dbpath, const char* nodeid);
char* cybo_get_ecorank_for_region(const char* dbpath, const char* region);
char* cybo_get_blastradius_for_machine(const char* dbpath, long long machine_id);
void  cybo_free_json(char* ptr);
]]

local lib = ffi.load("cyboquatic_eco_spine") -- adjust to actual .so/.dll name at deploy

local M = {}

local function read_json_ptr(ptr)
    if ptr == nil then
        return nil, "null pointer"
    end
    local s = ffi.string(ptr)
    lib.cybo_free_json(ptr)
    return s, nil
end

function M.get_machines_for_region(dbpath, region)
    local c = lib.cybo_get_machines_for_region(dbpath, region)
    return read_json_ptr(c)
end

function M.get_machine_windows(dbpath, machine_id)
    local c = lib.cybo_get_machine_windows(dbpath, machine_id)
    return read_json_ptr(c)
end

function M.get_ecoperjoule_for_node(dbpath, nodeid)
    local c = lib.cybo_get_ecoperjoule_for_node(dbpath, nodeid)
    return read_json_ptr(c)
end

function M.get_ecorank_for_region(dbpath, region)
    local c = lib.cybo_get_ecorank_for_region(dbpath, region)
    return read_json_ptr(c)
end

function M.get_blastradius_for_machine(dbpath, machine_id)
    local c = lib.cybo_get_blastradius_for_machine(dbpath, machine_id)
    return read_json_ptr(c)
end

return M
