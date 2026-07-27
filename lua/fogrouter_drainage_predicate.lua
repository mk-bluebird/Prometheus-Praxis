-- filename: lua/fogrouter_drainage_predicate.lua

local M = {}

-- Pure predicate, no IO or global state.
function M.evaluate(frame)
    local cec = tonumber(frame.CEC) or 0.0
    local tss = tonumber(frame.TSS) or 0.0
    local bod = tonumber(frame.BOD) or 0.0

    -- Complex predicate: CEC > 0.7 AND (TSS > 0.8 OR BOD < 0.2)
    if cec > 0.7 and (tss > 0.8 or bod < 0.2) then
        return true
    else
        return false
    end
end

return M
