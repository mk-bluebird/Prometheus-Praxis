-- File: lua/cyboquatic_workload_20260807/fog_workload_gate.lua
local gate = {}

function gate.evaluate(frame)
    local net_energy = math.max(0.0, frame.energyreq_j - frame.recovered_energy_j)
    local accepted = frame.fog_state == "CLEAR"
        and frame.delta_vt <= 0.0
        and frame.renewable_fraction >= 0.70
        and frame.renewable_fraction <= 1.0
        and frame.water_quality_gain >= 0.20
        and frame.water_quality_gain <= 1.0
        and frame.recovered_energy_j >= 0.0
        and frame.recovered_energy_j <= frame.energyreq_j
        and net_energy <= 50000.0
    return { decision = accepted and "ACCEPT" or "REJECT", net_energy_j = net_energy }
end

return gate
