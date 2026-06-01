-- filename: lua/econet_cybo_overlay.lua
-- destination: eco_restoration_shard/lua/econet_cybo_overlay.lua
-- Purpose:
-- - Edge Lua harness sketch for calling the cdylib JSON APIs.
-- - Visual-only: prints or forwards JSON for KER maps and blast-radius overlays.

local ffi = require("ffi")

ffi.cdef[[
    char *econet_get_ker_targets(const char *db_path, const char *repo_name);
    char *econet_get_blastradius_for_node(const char *db_path, const char *node_id);
    char *econet_get_workload_trends_for_node(const char *db_path, const char *node_id);
    void  econet_free_json(char *ptr);
]]

local lib = ffi.load("libeco_restoration_shard")  -- adjust to actual .so/.dll name

local M = {}

local function read_json(ptr)
    if ptr == nil then
        return nil, "null pointer"
    end
    local s = ffi.string(ptr)
    lib.econet_free_json(ptr)
    return s, nil
end

function M.get_ker_targets(db_path, repo_name)
    local c = lib.econet_get_ker_targets(db_path, repo_name)
    return read_json(c)
end

function M.get_blastradius(db_path, node_id)
    local c = lib.econet_get_blastradius_for_node(db_path, node_id)
    return read_json(c)
end

function M.get_workload_trends(db_path, node_id)
    local c = lib.econet_get_workload_trends_for_node(db_path, node_id)
    return read_json(c)
end

return M
