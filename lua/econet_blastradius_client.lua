-- filename: lua/econet_blastradius_client.lua
-- destination: EcoNet/lua/econet_blastradius_client.lua
-- purpose:
--   Lua edge-script helper to read JSON from the Rust cdylib and
--   route only advisory information into cyboquatic FOG routers.

local ffi = require("ffi")

ffi.cdef[[
    char* econet_blastradius_spine_init_json(const char* root_path_utf8,
                                             const char* region_utf8,
                                             double min_restoration_score);
    char* econet_blastradius_spine_improvement_json(const char* root_path_utf8,
                                                    const char* lane_utf8);
    void  econet_blastradius_spine_free_string(char* ptr);
]]

local M = {}

local function to_lua_string(c_ptr)
    if c_ptr == nil then
        return nil
    end
    local s = ffi.string(c_ptr)
    ffi.C.econet_blastradius_spine_free_string(c_ptr)
    return s
end

function M.list_shards_for_region(lib_path, repo_root, region, min_restoration_score)
    local lib = ffi.load(lib_path)
    local json_c = lib.econet_blastradius_spine_init_json(repo_root, region, min_restoration_score)
    local json_str = to_lua_string(json_c)
    return json_str
end

function M.list_improvement_ok(lib_path, repo_root, lane)
    local lib = ffi.load(lib_path)
    local json_c = lib.econet_blastradius_spine_improvement_json(repo_root, lane)
    local json_str = to_lua_string(json_c)
    return json_str
end

return M
