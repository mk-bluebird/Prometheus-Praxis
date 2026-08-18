local function finite_nonnegative(value, name)
    assert(value and value == value and value ~= math.huge and value ~= -math.huge, name .. " must be finite numeric")
    assert(value >= 0.0, name .. " must be non-negative")
    return value
end

local function zone_for_distance(distance_m, conservative_radius_m)
    if distance_m > conservative_radius_m then
        return "SAFE", "OPERATE_ONLY_AFTER_SITE_REVIEW"
    end

    if distance_m > conservative_radius_m / 2.0 then
        return "CAUTION", "HOLD_FOR_FIELD_INSPECTION"
    end

    return "EXCLUDE", "NO_ENTRY_OR_OPERATION"
end

if #arg ~= 4 then
    io.stderr:write(
        "usage: lua hex_exposure_zone.lua <canal_node_id> <phoenix_hex_anchor> " ..
        "<distance_m> <conservative_radius_m>\n"
    )
    os.exit(64)
end

local ok, result_or_error = pcall(function()
    local node_id = assert(arg[1] and #arg[1] > 0, "canal_node_id must be non-empty")
    local hex_anchor = assert(arg[2] and #arg[2] > 0, "phoenix_hex_anchor must be non-empty")
    local distance_m = finite_nonnegative(tonumber(arg[3]), "distance_m")
    local conservative_radius_m = finite_nonnegative(tonumber(arg[4]), "conservative_radius_m")
    assert(conservative_radius_m > 0.0, "conservative_radius_m must be positive")

    local zone, action = zone_for_distance(distance_m, conservative_radius_m)
    return {
        node_id = node_id,
        hex_anchor = hex_anchor,
        distance_m = distance_m,
        conservative_radius_m = conservative_radius_m,
        zone = zone,
        action = action
    }
end)

if not ok then
    io.stderr:write("error: " .. result_or_error .. "\n")
    os.exit(65)
end

local result = result_or_error
print("canal_node_id=" .. result.node_id)
print("phoenix_hex_anchor=" .. result.hex_anchor)
print(string.format("distance_m=%.8f", result.distance_m))
print(string.format("conservative_radius_m=%.8f", result.conservative_radius_m))
print("zone=" .. result.zone)
print("action=" .. result.action)
