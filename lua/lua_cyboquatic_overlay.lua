-- filename: lua_cyboquatic_overlay.lua
-- destination: eco_restoration_shard/lua/lua_cyboquatic_overlay.lua
-- Purpose:
--   - LuaJIT FFI overlay for Cyboquatic eco spine cdylib.
--   - Read-only diagnostics and visualization; no hardware drivers.

local ffi = require("ffi")

ffi.cdef[[
    char* cybo_get_ker_for_node(const char* db_path, const char* node_id);
    char* cybo_get_blastradius_for_node(const char* db_path, const char* node_id);
    char* cybo_get_workload_windows_for_node(const char* db_path, const char* node_id);
    char* cybo_get_substrate_summary_for_node(const char* db_path, const char* node_id);
    void  cybo_free_json(char* ptr);
]]

-- Adjust the library name/path as needed in your build system.
local lib = ffi.load("cyboquatic_spine")

local M = {}

local function read_json_ptr(ptr)
    if ptr == nil then
        return nil, "null pointer"
    end
    local s = ffi.string(ptr)
    lib.cybo_free_json(ptr)
    return s, nil
end

function M.get_ker_for_node(db_path, node_id)
    local c = lib.cybo_get_ker_for_node(db_path, node_id)
    return read_json_ptr(c)
end

function M.get_blastradius_for_node(db_path, node_id)
    local c = lib.cybo_get_blastradius_for_node(db_path, node_id)
    return read_json_ptr(c)
end

function M.get_workload_windows_for_node(db_path, node_id)
    local c = lib.cybo_get_workload_windows_for_node(db_path, node_id)
    return read_json_ptr(c)
end

function M.get_substrate_summary_for_node(db_path, node_id)
    local c = lib.cybo_get_substrate_summary_for_node(db_path, node_id)
    return read_json_ptr(c)
end

return M
