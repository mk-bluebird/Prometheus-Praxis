local function clamp01(value)
    if value < 0.0 then return 0.0 end
    if value > 1.0 then return 1.0 end
    return value
end

local function number_argument(index, name)
    local value = tonumber(arg[index])
    assert(value, name .. " must be numeric")
    return value
end

local function rank_2x2(a, b, c, d)
    local s11 = a * a + c * c
    local s12 = a * b + c * d
    local s22 = b * b + d * d
    local trace = s11 + s22
    local determinant = s11 * s22 - s12 * s12
    local discriminant = math.max(0.0, trace * trace - 4.0 * determinant)
    local lambda_max = 0.5 * (trace + math.sqrt(discriminant))
    local lambda_min = 0.5 * (trace - math.sqrt(discriminant))
    local tolerance = math.max(1.0e-12, lambda_max * 1.0e-10)

    if lambda_max <= tolerance then return 0 end
    if lambda_min > tolerance then return 2 end
    return 1
end

local function jacobian_rank(observations, bod_u, k)
    local normal11, normal12, normal22 = 0.0, 0.0, 0.0

    for _, point in ipairs(observations) do
        local exp_term = math.exp(-k * point.time_days)
        local d_bodu = 1.0 - exp_term
        local d_k = bod_u * point.time_days * exp_term
        normal11 = normal11 + d_bodu * d_bodu
        normal12 = normal12 + d_bodu * d_k
        normal22 = normal22 + d_k * d_k
    end

    return rank_2x2(normal11, normal12, normal12, normal22)
end

if #arg < 4 or ((#arg - 2) % 2 ~= 0) then
    io.stderr:write(
        "usage: lua bod_identifiability.lua <BODu_mg_L> <candidate_k_per_day> " ..
        "<time_days> <BOD_mg_L> [<time_days> <BOD_mg_L> ...]\n"
    )
    os.exit(64)
end

local ok, message = pcall(function()
    local bod_u = number_argument(1, "BODu")
    local k = number_argument(2, "candidate_k")
    assert(bod_u > 0.0 and k > 0.0, "BODu and candidate k must be positive")

    local observations = {}
    for index = 3, #arg, 2 do
        local time_days = number_argument(index, "time_days")
        local bod = number_argument(index + 1, "BOD")
        assert(time_days >= 0.0 and bod >= 0.0, "time and BOD must be non-negative")
        table.insert(observations, { time_days = time_days, bod = bod })
    end

    local rank = jacobian_rank(observations, bod_u, k)
    local first = observations[1]
    local k_estimate = nil
    if first.time_days > 0.0 and first.bod > 0.0 and first.bod < bod_u then
        k_estimate = -math.log(1.0 - first.bod / bod_u) / first.time_days
    end

    local positive_time_count = 0
    for _, observation in ipairs(observations) do
        if observation.time_days > 0.0 then positive_time_count = positive_time_count + 1 end
    end

    local knowledge_factor = clamp01(
        0.25 + 0.35 * math.min(1.0, #observations / 4.0) +
        0.25 * positive_time_count / #observations + 0.15 * (rank == 2 and 1.0 or 0.0)
    )
    local harm_risk = clamp01(
        0.20 + 0.45 * (rank < 2 and 1.0 or 0.0) +
        0.20 * (#observations < 3 and 1.0 or 0.0)
    )

    print("jacobian_rank_for_[BODu,k]=" .. rank)
    print("local_identifiability_for_[BODu,k]=" .. (rank == 2 and "POSSIBLE" or "NOT_ESTABLISHED"))
    print("k_estimate_if_BODu_known_per_day=" .. (k_estimate and string.format("%.8f", k_estimate) or "UNAVAILABLE"))
    print("knowledge_factor=" .. string.format("%.8f", knowledge_factor))
    print("eco_impact_value=" .. string.format("%.8f", clamp01(knowledge_factor * (1.0 - harm_risk))))
    print("harm_risk=" .. string.format("%.8f", harm_risk))
    print("decision=" .. (rank == 2 and #observations >= 3 and "USE_AS_SCREENING_ESTIMATE" or "COLLECT_MORE_DATA"))
end)

if not ok then
    io.stderr:write("error: " .. message .. "\n")
    os.exit(65)
end
