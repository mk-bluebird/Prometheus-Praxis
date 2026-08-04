-- File: lua/h3_to_canal_crosswalk.lua
-- Lua script that, given an H3 index, returns the nearest canal_segment and capacity
-- by querying a spatial index (R*Tree) in SQLite. This enables the FOG router to
-- consider canal capacity when routing runoff from a hex-cell.

local Crosswalk = {}

-- Spatial schema assumptions:
-- Table canal_segments_geo:
--   canal_segment TEXT PRIMARY KEY,
--   center_lat REAL,
--   center_lon REAL,
--   canal_capacity_m3_s REAL
--
-- R*Tree index canal_segments_rtree:
--   canal_segment TEXT,
--   min_lat REAL, max_lat REAL,
--   min_lon REAL, max_lon REAL
--
-- H3 hex registry table hex_cell_catalog:
--   h3_index TEXT PRIMARY KEY,
--   center_lat REAL,
--   center_lon REAL,
--   basin_id TEXT

-- Helper: run sqlite3 query and parse nearest canal segment.
local function nearest_canal_segment(db_path, h3_index)
    -- First, get hex center coordinates.
    local cmd_hex = string.format(
        "sqlite3 %s \"SELECT center_lat, center_lon FROM hex_cell_catalog " ..
        "WHERE h3_index='%s';\"",
        db_path, h3_index
    )
    local handle_hex = io.popen(cmd_hex)
    if not handle_hex then
        return nil
    end
    local lat, lon
    for line in handle_hex:lines() do
        lat, lon = line:match("([^|]+)|([^|]+)")
        if lat and lon then
            lat = tonumber(lat)
            lon = tonumber(lon)
            break
        }
    end
    handle_hex:close()
    if not lat or not lon then
        return nil
    end

    -- Query R*Tree for nearby canal segments, then choose nearest by distance.
    local cmd_canal = string.format(
        "sqlite3 %s \"SELECT g.canal_segment, g.center_lat, g.center_lon, g.canal_capacity_m3_s " ..
        "FROM canal_segments_rtree r " ..
        "JOIN canal_segments_geo g ON g.canal_segment = r.canal_segment " ..
        "WHERE r.min_lat <= %f AND r.max_lat >= %f " ..
        "  AND r.min_lon <= %f AND r.max_lon >= %f;\"",
        db_path, lat, lat, lon, lon
    )
    local handle_canal = io.popen(cmd_canal)
    if not handle_canal then
        return nil
    end

    local best = nil
    local bestDist2 = math.huge
    for line in handle_canal:lines() do
        local seg, clat, clon, cap = line:match("([^|]+)|([^|]+)|([^|]+)|([^|]+)")
        if seg and clat and clon and cap then
            clat = tonumber(clat)
            clon = tonumber(clon)
            cap  = tonumber(cap)
            local dlat = clat - lat
            local dlon = clon - lon
            local dist2 = dlat * dlat + dlon * dlon
            if dist2 < bestDist2 then
                bestDist2 = dist2
                best = {
                    canal_segment = seg,
                    canal_capacity_m3_s = cap,
                    center_lat = clat,
                    center_lon = clon
                }
            end
        end
    end
    handle_canal:close()

    return best
end

-- Public API: given H3 index, get nearest canal_segment and capacity.
function Crosswalk.get_canal_for_hex(db_path, h3_index)
    return nearest_canal_segment(db_path, h3_index)
end

-- Example usage:
-- local info = Crosswalk.get_canal_for_hex("./data/cyboquatic_workload.db", "8a2a1072bffffff")
-- if info then
--     print("Nearest canal_segment:", info.canal_segment,
--           "capacity_m3_s:", info.canal_capacity_m3_s)
-- end

return Crosswalk
