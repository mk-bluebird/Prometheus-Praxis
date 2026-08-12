-- File: lua/eco_restoration/run_reanalysis_validation.lua
local water_model, heat_model, validator, input, water_csv, heat_csv =
    assert(arg[1]), assert(arg[2]), assert(arg[3]), assert(arg[4]), assert(arg[5]), assert(arg[6])

local function run(command)
    local ok, _, status = os.execute(command)
    assert(ok and status == 0, "command failed: " .. command)
end

run(string.format('%q %q %q', water_model, input, water_csv))
run(string.format('%q %q %q', heat_model, input, heat_csv))
run(string.format('%q %q', validator, water_csv))
run(string.format('%q %q', validator, heat_csv))
