local function parse_finite(index, label)
    local value = tonumber(arg[index])
    assert(value and value == value and value ~= math.huge and value ~= -math.huge, label .. " must be finite")
    return value
end

local function round_half_away_from_zero(value)
    if value >= 0.0 then
        return math.floor(value + 0.5)
    end
    return math.ceil(value - 0.5)
end

local function classify(q, t, sensitivity, distance, coefficient)
    assert(q > 0.0 and t > 0.0 and coefficient > 0.0, "Q, T, and c must be positive")
    assert(sensitivity >= 0.0 and sensitivity <= 1.0 and distance >= 0.0,
        "S_b must be in [0, 1] and distance must be non-negative")

    local radius = coefficient * math.sqrt(q * t) * (1.0 + 1.5 * sensitivity)
    assert(radius == radius and radius ~= math.huge and radius ~= -math.huge, "conservative radius must be finite")

    local zone
    if distance > radius then
        zone = "SAFE"
    elseif distance > radius / 2.0 then
        zone = "CAUTION"
    else
        zone = "EXCLUDE"
    end

    return zone, round_half_away_from_zero(radius * 1000000.0)
end

if #arg ~= 5 then
    io.stderr:write("usage: lua blast_radius_conformance.lua <Q_m3_s> <T_s> <S_b_0_to_1> <distance_m> <c>\n")
    os.exit(64)
end

local ok, zone, scaled_or_error = pcall(
    classify,
    parse_finite(1, "Q"),
    parse_finite(2, "T"),
    parse_finite(3, "S_b"),
    parse_finite(4, "distance"),
    parse_finite(5, "c")
)

if not ok then
    io.stderr:write("error: " .. zone .. "\n")
    os.exit(65)
end

print(zone .. "|" .. scaled_or_error)
