local function numeric(index, name)
    local value = tonumber(arg[index])
    assert(value and value == value and value ~= math.huge and value ~= -math.huge, name .. " must be finite numeric")
    return value
end

local function round_half_away_from_zero(value)
    if value >= 0.0 then
        return math.floor(value + 0.5)
    end
    return math.ceil(value - 0.5)
end

local function cube_round(q_float, r_float, s_float)
    local q = round_half_away_from_zero(q_float)
    local r = round_half_away_from_zero(r_float)
    local s = round_half_away_from_zero(s_float)

    local q_error = math.abs(q - q_float)
    local r_error = math.abs(r - r_float)
    local s_error = math.abs(s - s_float)

    if q_error > r_error and q_error > s_error then
        q = -r - s
    elseif r_error > s_error then
        r = -q - s
    else
        s = -q - r
    end

    return q, r, s
end

local function encode(
    latitude_deg,
    longitude_deg,
    origin_latitude_deg,
    origin_longitude_deg,
    meters_per_degree_latitude,
    meters_per_degree_longitude,
    side_length_m
)
    assert(meters_per_degree_latitude > 0.0, "meters_per_degree_latitude must be positive")
    assert(meters_per_degree_longitude > 0.0, "meters_per_degree_longitude must be positive")
    assert(side_length_m > 0.0, "side_length_m must be positive")

    local x_m = (longitude_deg - origin_longitude_deg) * meters_per_degree_longitude
    local y_m = (latitude_deg - origin_latitude_deg) * meters_per_degree_latitude
    local q_float = (math.sqrt(3.0) / 3.0 * x_m - y_m / 3.0) / side_length_m
    local r_float = (2.0 / 3.0 * y_m) / side_length_m
    local q, r, s = cube_round(q_float, r_float, -q_float - r_float)

    local center_x_m = side_length_m * math.sqrt(3.0) * (q + r / 2.0)
    local center_y_m = side_length_m * 1.5 * r
    local center_latitude_deg = origin_latitude_deg + center_y_m / meters_per_degree_latitude
    local center_longitude_deg = origin_longitude_deg + center_x_m / meters_per_degree_longitude

    return {
        x_m = x_m,
        y_m = y_m,
        q = q,
        r = r,
        s = s,
        center_x_m = center_x_m,
        center_y_m = center_y_m,
        center_latitude_deg = center_latitude_deg,
        center_longitude_deg = center_longitude_deg
    }
end

if #arg ~= 7 then
    io.stderr:write(
        "usage: lua phoenix_hex_transform.lua <lat_deg> <lon_deg> <origin_lat_deg> <origin_lon_deg> " ..
        "<meters_per_degree_lat> <meters_per_degree_lon> <hex_side_m>\n"
    )
    os.exit(64)
end

local ok, result_or_error = pcall(
    encode,
    numeric(1, "latitude_deg"),
    numeric(2, "longitude_deg"),
    numeric(3, "origin_latitude_deg"),
    numeric(4, "origin_longitude_deg"),
    numeric(5, "meters_per_degree_latitude"),
    numeric(6, "meters_per_degree_longitude"),
    numeric(7, "side_length_m")
)

if not ok then
    io.stderr:write("error: " .. result_or_error .. "\n")
    os.exit(65)
end

local result = result_or_error
print(string.format("x_m=%.10f", result.x_m))
print(string.format("y_m=%.10f", result.y_m))
print("q=" .. result.q)
print("r=" .. result.r)
print("s=" .. result.s)
print(string.format("hex_center_x_m=%.10f", result.center_x_m))
print(string.format("hex_center_y_m=%.10f", result.center_y_m))
print(string.format("hex_center_lat_deg=%.10f", result.center_latitude_deg))
print(string.format("hex_center_lon_deg=%.10f", result.center_longitude_deg))
