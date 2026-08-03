-- File: lua/mcp/mcp_ker_synapse_client.lua
-- Destination: mk-bluebird/Prometheus-Praxis/lua/mcp/mcp_ker_synapse_client.lua

local McpKerSynapseClient = {}
McpKerSynapseClient.__index = McpKerSynapseClient

-- Simple utility to run a command and capture its stdin/stdout
local function spawn_server(server_path, sqlite_db_path)
    -- For portability in Lua, we assume the server is launched externally
    -- and we communicate via temporary files or pipes.
    -- For this sketch, we use os.execute for synchronous calls via temp file.
    return {
        path = server_path,
        db = sqlite_db_path
    }
end

local function json_escape(s)
    s = s:gsub("\\", "\\\\")
    s = s:gsub("\"", "\\\"")
    s = s:gsub("\n", "\\n")
    s = s:gsub("\r", "\\r")
    s = s:gsub("\t", "\\t")
    return s
end

local function extract_rows_json(response)
    local rows = {}
    local rows_start = response:find("\"rows\"")
    if not rows_start then return rows end
    local array_start = response:find("%[", rows_start)
    local array_end = response:match(".*()%]")
    if not array_start or not array_end or array_end < array_start then
        return rows
    end
    local content = response:sub(array_start + 1, array_end - 1)
    local depth = 0
    local start = nil
    for i = 1, #content do
        local c = content:sub(i, i)
        if c == "{" then
            if depth == 0 then start = i end
            depth = depth + 1
        elseif c == "}" then
            depth = depth - 1
            if depth == 0 and start then
                table.insert(rows, content:sub(start, i))
                start = nil
            end
        end
    end
    return rows
end

local function get_field(obj_json, field)
    local key = "\"" .. field .. "\""
    local idx = obj_json:find(key)
    if not idx then return nil end
    local colon = obj_json:find(":", idx + #key)
    if not colon then return nil end
    local val = obj_json:sub(colon + 1):match("^%s*(.-)%s*$")
    if val:sub(-1) == "," then
        val = val:sub(1, -2):match("^%s*(.-)%s*$")
    end
    if val:sub(1, 1) == "\"" then
        val = val:sub(2)
        local end_quote = val:find("\"")
        if end_quote then
            return val:sub(1, end_quote - 1)
        end
    else
        local cleaned = val:match("^(.-)[,}]")
        if cleaned then
            return cleaned:match("^%s*(.-)%s*$")
        else
            return val
        end
    end
    return nil
end

function McpKerSynapseClient.new(server_path, sqlite_db_path)
    local self = setmetatable({}, McpKerSynapseClient)
    self.server = spawn_server(server_path, sqlite_db_path)
    return self
end

function McpKerSynapseClient:send_request(request_json)
    -- For the sketch, we write request_json to a temp file and invoke the server
    local req_file = "mcp_request.json"
    local res_file = "mcp_response.json"
    local f = assert(io.open(req_file, "w"))
    f:write(request_json)
    f:close()

    local cmd = string.format("%s %s < %s > %s", self.server.path, self.server.db, req_file, res_file)
    os.execute(cmd)

    local rf = io.open(res_file, "r")
    if not rf then
        return [[{ "ok": false, "error": "no response" }]]
    end
    local content = rf:read("*a")
    rf:close()
    return content
end

function McpKerSynapseClient:hex_stability_carbon(limit, primary_plane)
    limit = limit or 50
    primary_plane = primary_plane or "HYDRAULICS"
    local request_json = string.format(
        [[{ "tool": "hex_stability_carbon", "params": { "limit": %d, "primary_plane": "%s" } }]],
        limit,
        json_escape(primary_plane)
    )
    local response = self:send_request(request_json)
    local rows_json = extract_rows_json(response)
    local result = {}
    for _, obj in ipairs(rows_json) do
        local row = {
            hex_id = get_field(obj, "hex_id") or "",
            region_name = get_field(obj, "region_name") or "",
            topology_band = get_field(obj, "topology_band") or "",
            primary_plane = get_field(obj, "primary_plane") or "",
            workload_count = tonumber(get_field(obj, "workload_count") or "0") or 0,
            total_delta_v_t = tonumber(get_field(obj, "total_delta_v_t") or "0.0") or 0.0,
            avg_delta_v_t = tonumber(get_field(obj, "avg_delta_v_t") or "0.0") or 0.0,
            max_delta_v_t = tonumber(get_field(obj, "max_delta_v_t") or "0.0") or 0.0,
            avg_ker_k = tonumber(get_field(obj, "avg_ker_k") or "0.0") or 0.0,
            avg_ker_e = tonumber(get_field(obj, "avg_ker_e") or "0.0") or 0.0,
            avg_ker_r = tonumber(get_field(obj, "avg_ker_r") or "0.0") or 0.0,
            avg_ker_s = tonumber(get_field(obj, "avg_ker_s") or "0.0") or 0.0,
            avg_carbon_intensity_gco2_kwh = tonumber(get_field(obj, "avg_carbon_intensity_gco2_kwh") or "0.0") or 0.0,
            count_green_band = tonumber(get_field(obj, "count_green_band") or "0") or 0,
            count_neutral_band = tonumber(get_field(obj, "count_neutral_band") or "0") or 0,
            count_red_band = tonumber(get_field(obj, "count_red_band") or "0") or 0,
            violations_dvt_global = tonumber(get_field(obj, "violations_dvt_global") or "0") or 0,
            violations_ker_nonpositive = tonumber(get_field(obj, "violations_ker_nonpositive") or "0") or 0,
            violations_joint_ker_dvt = tonumber(get_field(obj, "violations_joint_ker_dvt") or "0") or 0,
            violations_red_band_ker = tonumber(get_field(obj, "violations_red_band_ker") or "0") or 0,
            violations_prod_red_band = tonumber(get_field(obj, "violations_prod_red_band") or "0") or 0
        }
        table.insert(result, row)
    end
    return result
end

function McpKerSynapseClient:hexes_needing_stability_attention(limit, primary_plane)
    local rows = self:hex_stability_carbon(limit or 50, primary_plane or "HYDRAULICS")
    local flagged = {}
    for _, h in ipairs(rows) do
        if h.violations_dvt_global > 0 or h.violations_joint_ker_dvt > 0 then
            table.insert(flagged, h)
        end
    end
    return flagged
end

function McpKerSynapseClient:hexes_needing_carbon_attention(limit, primary_plane)
    local rows = self:hex_stability_carbon(limit or 50, primary_plane or "HYDRAULICS")
    local flagged = {}
    for _, h in ipairs(rows) do
        if h.count_red_band > 0 or h.violations_red_band_ker > 0 or h.violations_prod_red_band > 0 then
            table.insert(flagged, h)
        end
    end
    return flagged
end

-- Example usage:
-- local client = McpKerSynapseClient.new("cpp/tools/mcp_ker_synapse_server_named", "eco_restoration_shard.db")
-- local unstable_hexes = client:hexes_needing_stability_attention(50, "HYDRAULICS")
-- for _, h in ipairs(unstable_hexes) do
--     print(h.hex_id, h.region_name, h.total_delta_v_t, h.violations_dvt_global)
-- end

return McpKerSynapseClient
