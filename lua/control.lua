-- control.lua
local function safe_actuate(actuator_id, cmd)
    local ok = request_actuation(actuator_id, cmd)
    if not ok then
        -- Non-actuating-only violation or other fuse-box denial
        -- Raise a Lua error or log to ALN/SQLite via bindings.
        error("Actuation denied by fuse-box for actuator " .. actuator_id)
    end
end

-- Example: some control loop
function control_step()
    -- This call will trip the relay if non_actuating_only = true
    safe_actuate("valve:drainage_outlet", "OPEN")
end
