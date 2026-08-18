local function integer_argument(index, name)
    local value = tonumber(arg[index])
    assert(value and value == math.floor(value), name .. " must be an integer")
    return value
end

local function floor_mod(value, modulus)
    local result = value % modulus
    if result < 0 then
        result = result + modulus
    end
    return result
end

local function encode(q, r, coordinate_offset, radix)
    assert(radix > 0, "radix must be positive")
    assert(q >= -coordinate_offset and q <= coordinate_offset, "q is outside declared reversible range")
    assert(r >= -coordinate_offset and r <= coordinate_offset, "r is outside declared reversible range")

    local q_shifted = q + coordinate_offset
    local r_shifted = r + coordinate_offset
    assert(q_shifted >= 0 and q_shifted < radix, "shifted q must be in [0, radix)")
    assert(r_shifted >= 0 and r_shifted < radix, "shifted r must be in [0, radix)")

    return q_shifted + radix * r_shifted
end

local function decode(identifier, coordinate_offset, radix)
    assert(radix > 0, "radix must be positive")
    assert(identifier >= 0, "identifier must be non-negative")

    local q_shifted = floor_mod(identifier, radix)
    local r_shifted = (identifier - q_shifted) / radix
    assert(r_shifted >= 0 and r_shifted < radix, "identifier is outside declared reversible range")

    local q = q_shifted - coordinate_offset
    local r = r_shifted - coordinate_offset
    local s = -q - r
    return q, r, s
end

if #arg < 4 then
    io.stderr:write(
        "usage:\n" ..
        "  lua hex_anchor_linear_id.lua encode <q> <r> <coordinate_offset> <radix>\n" ..
        "  lua hex_anchor_linear_id.lua decode <id> <coordinate_offset> <radix>\n"
    )
    os.exit(64)
end

local mode = arg[1]
local ok, a, b, c = pcall(function()
    local first = integer_argument(2, mode == "encode" and "q" or "id")
    local second = mode == "encode" and integer_argument(3, "r") or integer_argument(3, "coordinate_offset")
    local third = mode == "encode" and integer_argument(4, "coordinate_offset") or integer_argument(4, "radix")
    local fourth = mode == "encode" and integer_argument(5, "radix") or nil

    if mode == "encode" then
        assert(fourth, "encode requires radix")
        local id = encode(first, second, third, fourth)
        return id
    end

    if mode == "decode" then
        return decode(first, second, third)
    end

    error("mode must be encode or decode")
end)

if not ok then
    io.stderr:write("error: " .. a .. "\n")
    os.exit(65)
end

if mode == "encode" then
    print("id=" .. a)
else
    print("q=" .. a)
    print("r=" .. b)
    print("s=" .. c)
end
