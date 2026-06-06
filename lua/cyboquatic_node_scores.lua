-- filename: lua/cyboquatic_node_scores.lua
-- destination: eco_restoration_shard/lua/cyboquatic_node_scores.lua
-- Purpose:
-- - Lua harness for non-actuating diagnostics over the Cyboquatic spine.
-- - Calls a Rust JSON helper (via cdylib) to list eco-restorative nodes.

local ffi = require("ffi")

ffi.cdef[[
    const char* econet_cybo_list_nodes(const char* db_path, double rplane_max);
    void econet_cybo_free_json(char* ptr);
]]

local lib = ffi.load("eco_restoration_shard_cybo") -- cdylib name to match build

local M = {}

local function read_json_ptr(ptr)
    if ptr == nil then
        return nil, "null pointer from econet_cybo_list_nodes"
    end
    local s = ffi.string(ptr)
    lib.econet_cybo_free_json(ptr)
    return s, nil
end

function M.list_ecorestorative_nodes(db_path, rplane_max)
    local cjson = lib.econet_cybo_list_nodes(db_path, rplane_max or 0.2)
    return read_json_ptr(cjson)
end

return M
