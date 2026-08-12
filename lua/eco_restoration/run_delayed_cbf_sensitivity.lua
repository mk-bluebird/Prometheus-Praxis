-- File: lua/eco_restoration/run_delayed_cbf_sensitivity.lua

local executable, database = arg[1], arg[2]
if not executable or not database then
    error("usage: lua run_delayed_cbf_sensitivity.lua executable indices.sqlite")
end

local command = string.format('"%s" "%s"', executable, database)
local success, reason, code = os.execute(command)
if not success then
    error(string.format("sensitivity execution failed: %s (%s)", tostring(reason), tostring(code)))
end
