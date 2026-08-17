local function number_argument(index, name)
    local value = tonumber(arg[index])
    assert(value, name .. " must be numeric")
    return value
end

local function nonnegative(value)
    return math.max(0.0, value)
end

local function derivative(total_particles, k_hyd, k_frag, alpha)
    if total_particles <= 0.0 then
        return 0.0
    end
    return k_hyd * total_particles - k_frag * total_particles ^ alpha
end

if #arg ~= 6 then
    io.stderr:write(
        "usage: lua polymer_fragmentation_stiffness.lua " ..
        "<k_hyd_per_day> <k_frag_per_day> <alpha> <initial_particle_count> <step_days> <steps>\n"
    )
    os.exit(64)
end

local ok, message = pcall(function()
    local k_hyd = number_argument(1, "k_hyd_per_day")
    local k_frag = number_argument(2, "k_frag_per_day")
    local alpha = number_argument(3, "alpha")
    local particles = number_argument(4, "initial_particle_count")
    local step_days = number_argument(5, "step_days")
    local steps = math.floor(number_argument(6, "steps"))

    assert(k_hyd >= 0.0 and k_frag >= 0.0 and alpha > 0.0 and particles >= 0.0 and step_days > 0.0 and steps > 0,
        "rates and state must be non-negative; alpha, step, and steps must be positive")

    local stiffness_proxy = (
        k_hyd + k_frag * alpha * math.max(1.0, particles) ^ math.max(0.0, alpha - 1.0)
    ) * step_days

    print("time_days,total_particles,dN_dt")
    for index = 0, steps do
        local time_days = index * step_days
        local dndt = derivative(particles, k_hyd, k_frag, alpha)
        print(string.format("%.8f,%.8f,%.8f", time_days, particles, dndt))

        if index < steps then
            local predicted = particles + step_days * dndt
            particles = nonnegative(predicted)
        end
    end

    print(string.format("stiffness_proxy=%.8f", stiffness_proxy))
    print("integration_guidance=" ..
        (stiffness_proxy > 0.20 and "REDUCE_STEP_OR_USE_IMPLICIT_SOLVER" or "EXPLICIT_EULER_SCREENING_ONLY"))
    print("distribution_limit=SCALAR_N_CANNOT_ESTABLISH_BIMODAL_SIZE_DISTRIBUTION")
end)

if not ok then
    io.stderr:write("error: " .. message .. "\n")
    os.exit(65)
end
