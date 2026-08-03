-- File: cpp/simulation/lua/fog_router_cyboquatic.lua
-- Destination: mk-bluebird/Prometheus-Praxis/cpp/simulation/lua/fog_router_cyboquatic.lua

local FOGRouter = {}
FOGRouter.__index = FOGRouter

local function clamp01(v)
    if v < 0.0 then
        return 0.0
    elseif v > 1.0 then
        return 1.0
    else
        return v
    end
end

function FOGRouter.new(alpha, beta, max_delta_v)
    local self = setmetatable({}, FOGRouter)
    self.alpha = alpha or 0.5
    self.beta = beta or 1e-6
    self.max_delta_v = max_delta_v or 0.05
    self.last_timestamp = nil
    return self
end

function FOGRouter:compute_dt(current_timestamp)
    if self.last_timestamp == nil then
        return 1.0
    end
    local dt = current_timestamp - self.last_timestamp
    if dt <= 0.0 then
        return 1.0
    end
    return dt
end

function FOGRouter:step(telemetry)
    if telemetry.flow_rate_m3s < 0.0 then
        error("flow_rate_m3s must be non-negative")
    end
    if telemetry.lift_height_m < 0.0 then
        error("lift_height_m must be non-negative")
    end
    if telemetry.pump_power_kw < 0.0 then
        error("pump_power_kw must be non-negative")
    end
    if telemetry.water_density_kgm3 <= 0.0 then
        error("water_density_kgm3 must be positive")
    end
    if telemetry.gravity_ms2 <= 0.0 then
        error("gravity_ms2 must be positive")
    end
    if telemetry.eco_efficiency < 0.0 or telemetry.eco_efficiency > 1.0 then
        error("eco_efficiency must be in [0,1]")
    end

    local dt = self:compute_dt(telemetry.timestamp_seconds)

    local hydraulic_energy_j = telemetry.water_density_kgm3 *
        telemetry.gravity_ms2 *
        telemetry.flow_rate_m3s *
        telemetry.lift_height_m *
        dt

    local electrical_energy_j = telemetry.pump_power_kw * 1000.0 * dt

    local energy_req_j = hydraulic_energy_j + electrical_energy_j

    local eco_factor = 1.0 + self.alpha * (1.0 - clamp01(telemetry.eco_efficiency))
    local eco_weighted_energy_j = energy_req_j * eco_factor

    local delta_v_t = self.beta * eco_weighted_energy_j
    if delta_v_t > self.max_delta_v then
        delta_v_t = self.max_delta_v
    end

    self.last_timestamp = telemetry.timestamp_seconds

    return {
        energy_req_j = energy_req_j,
        eco_energy_j = eco_weighted_energy_j,
        delta_v_t = delta_v_t
    }
end

local function demo()
    local router = FOGRouter.new(0.5, 1e-6, 0.05)
    local now = os.time()
    local t1 = {
        node_id = "node-A",
        flow_rate_m3s = 0.4,
        head_loss_m = 0.2,
        pump_power_kw = 1.2,
        lift_height_m = 2.0,
        water_density_kgm3 = 1000.0,
        gravity_ms2 = 9.81,
        eco_efficiency = 0.9,
        timestamp_seconds = now
    }

    local r1 = router:step(t1)
    print(string.format(
        "node=%s energy_req_j=%.3f eco_energy_j=%.3f delta_v_t=%.6f",
        t1.node_id, r1.energy_req_j, r1.eco_energy_j, r1.delta_v_t
    ))
end

if ... == nil then
    demo()
end

return FOGRouter
