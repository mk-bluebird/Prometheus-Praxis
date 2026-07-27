-- filename: lua/fogrouter_sandbox.lua

local base_env = {
    tonumber = tonumber,
    math     = math,
    pairs    = pairs,
    ipairs   = ipairs,
    type     = type,
}

-- Load predicate module in restricted environment.
local function load_predicate()
    local chunk = assert(loadfile("lua/fogrouter_drainage_predicate.lua", "t", base_env))
    local mod = chunk()
    return mod
end

local predicate = load_predicate()

return {
    evaluate = predicate.evaluate
}
