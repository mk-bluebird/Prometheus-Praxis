-- LuaJIT FFI harness for non-actuating EcoNet/Eco-Restoration cdylib.
-- Provides read-only JSON access to KER targets, blast-radius, workload
-- trends, and Cyboquatic energy/restoration windows.

local ffi = require("ffi")

ffi.cdef[[
    char* econet_get_kertargets(const char* dbpath, const char* reponame);
    char* econet_get_blastradius_for_node(const char* dbpath, const char* nodeid);
    char* econet_get_workloadtrends_for_node(const char* dbpath, const char* nodeid);
    char* econet_get_cybo_ecoplot_for_node(const char* dbpath, const char* nodeid);
    char* econet_get_cybo_restoration_for_node(const char* dbpath, const char* nodeid);
    void  econet_free_json(char* ptr);
]]

local M = {}

local function call_and_copy(fn, dbpath, arg)
    local c_db = ffi.new("char[?]", #dbpath + 1)
    ffi.copy(c_db, dbpath)
    local c_arg = ffi.new("char[?]", #arg + 1)
    ffi.copy(c_arg, arg)

    local raw = fn(c_db, c_arg)
    if raw == nil then
        return nil, "cdylib returned NULL"
    end

    local s = ffi.string(raw)
    ffi.C.econet_free_json(raw)
    return s, nil
end

function M.get_kertargets(dbpath, reponame)
    if type(dbpath) ~= "string" or type(reponame) ~= "string" then
        return nil, "dbpath and reponame must be strings"
    end
    return call_and_copy(ffi.C.econet_get_kertargets, dbpath, reponame)
end

function M.get_blastradius(dbpath, nodeid)
    if type(dbpath) ~= "string" or type(nodeid) ~= "string" then
        return nil, "dbpath and nodeid must be strings"
    end
    return call_and_copy(ffi.C.econet_get_blastradius_for_node, dbpath, nodeid)
end

function M.get_workloadtrends(dbpath, nodeid)
    if type(dbpath) ~= "string" or type(nodeid) ~= "string" then
        return nil, "dbpath and nodeid must be strings"
    end
    return call_and_copy(ffi.C.econet_get_workloadtrends_for_node, dbpath, nodeid)
end

function M.get_cybo_ecoplot(dbpath, nodeid)
    if type(dbpath) ~= "string" or type(nodeid) ~= "string" then
        return nil, "dbpath and nodeid must be strings"
    end
    return call_and_copy(ffi.C.econet_get_cybo_ecoplot_for_node, dbpath, nodeid)
end

function M.get_cybo_restoration(dbpath, nodeid)
    if type(dbpath) ~= "string" or type(nodeid) ~= "string" then
        return nil, "dbpath and nodeid must be strings"
    end
    return call_and_copy(ffi.C.econet_get_cybo_restoration_for_node, dbpath, nodeid)
end

return M
