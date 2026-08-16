local foundation = {}

local function finite_number(value)
    return type(value) == "number" and value == value
        and value ~= math.huge and value ~= -math.huge
end

function foundation.clamp(value, minimum, maximum)
    assert(finite_number(value), "value must be a finite number")
    assert(finite_number(minimum), "minimum must be a finite number")
    assert(finite_number(maximum), "maximum must be a finite number")
    assert(minimum <= maximum, "minimum must not exceed maximum")

    if value < minimum then
        return minimum
    end
    if value > maximum then
        return maximum
    end
    return value
end

function foundation.round(value, decimals)
    assert(finite_number(value), "value must be a finite number")
    decimals = decimals or 0
    assert(type(decimals) == "number" and decimals >= 0 and decimals % 1 == 0,
        "decimals must be a non-negative integer")

    local scale = 10 ^ decimals
    if value >= 0 then
        return math.floor(value * scale + 0.5) / scale
    end
    return math.ceil(value * scale - 0.5) / scale
end

function foundation.require_number(record, key, minimum, maximum)
    assert(type(record) == "table", "record must be a table")
    assert(type(key) == "string" and key ~= "", "key must be a non-empty string")

    local value = record[key]
    if not finite_number(value) then
        return nil, string.format("%s must be a finite number", key)
    end

    if minimum ~= nil and value < minimum then
        return nil, string.format("%s must be at least %s", key, tostring(minimum))
    end

    if maximum ~= nil and value > maximum then
        return nil, string.format("%s must be at most %s", key, tostring(maximum))
    end

    return value
end

function foundation.require_string(record, key, minimum_length, maximum_length)
    assert(type(record) == "table", "record must be a table")
    assert(type(key) == "string" and key ~= "", "key must be a non-empty string")

    local value = record[key]
    if type(value) ~= "string" then
        return nil, string.format("%s must be a string", key)
    end

    local length = #value
    if minimum_length and length < minimum_length then
        return nil, string.format("%s is shorter than %d characters", key, minimum_length)
    end

    if maximum_length and length > maximum_length then
        return nil, string.format("%s exceeds %d characters", key, maximum_length)
    end

    return value
end

function foundation.copy_array(values)
    assert(type(values) == "table", "values must be a table")

    local copy = {}
    for index = 1, #values do
        copy[index] = values[index]
    end
    return copy
end

function foundation.sum(values)
    assert(type(values) == "table", "values must be a table")

    local total = 0.0
    for index = 1, #values do
        assert(finite_number(values[index]), "all array values must be finite numbers")
        total = total + values[index]
    end
    return total
end

function foundation.mean(values)
    assert(type(values) == "table" and #values > 0, "values must be a non-empty table")
    return foundation.sum(values) / #values
end

function foundation.weighted_mean(values, weights)
    assert(type(values) == "table", "values must be a table")
    assert(type(weights) == "table", "weights must be a table")
    assert(#values > 0 and #values == #weights, "values and weights must have equal non-zero length")

    local weighted_total = 0.0
    local weight_total = 0.0

    for index = 1, #values do
        assert(finite_number(values[index]), "all values must be finite numbers")
        assert(finite_number(weights[index]) and weights[index] >= 0,
            "all weights must be finite non-negative numbers")

        weighted_total = weighted_total + values[index] * weights[index]
        weight_total = weight_total + weights[index]
    end

    assert(weight_total > 0, "weight total must be greater than zero")
    return weighted_total / weight_total
end

function foundation.normalize_weights(weights)
    assert(type(weights) == "table" and #weights > 0, "weights must be a non-empty table")

    local total = 0.0
    for index = 1, #weights do
        assert(finite_number(weights[index]) and weights[index] >= 0,
            "weights must be finite non-negative numbers")
        total = total + weights[index]
    end

    assert(total > 0, "weight total must be greater than zero")

    local normalized = {}
    for index = 1, #weights do
        normalized[index] = weights[index] / total
    end

    return normalized
end

function foundation.score_completeness(record, required_keys)
    assert(type(record) == "table", "record must be a table")
    assert(type(required_keys) == "table" and #required_keys > 0,
        "required_keys must be a non-empty table")

    local present = 0
    local missing = {}

    for index = 1, #required_keys do
        local key = required_keys[index]
        assert(type(key) == "string" and key ~= "", "required key names must be non-empty strings")

        local value = record[key]
        if value ~= nil and value ~= "" then
            present = present + 1
        else
            missing[#missing + 1] = key
        end
    end

    return {
        completeness = present / #required_keys,
        present = present,
        required = #required_keys,
        missing = missing,
    }
end

function foundation.project_metrics(input)
    assert(type(input) == "table", "input must be a table")

    local completeness = foundation.clamp(input.completeness or 0.0, 0.0, 1.0)
    local evidence_quality = foundation.clamp(input.evidence_quality or 0.0, 0.0, 1.0)
    local ecological_benefit = foundation.clamp(input.ecological_benefit or 0.0, 0.0, 1.0)
    local toxicity_risk = foundation.clamp(input.toxicity_risk or 0.0, 0.0, 1.0)
    local habitat_risk = foundation.clamp(input.habitat_risk or 0.0, 0.0, 1.0)
    local water_risk = foundation.clamp(input.water_risk or 0.0, 0.0, 1.0)
    local uncertainty = foundation.clamp(input.uncertainty or 1.0, 0.0, 1.0)

    local knowledge_factor = foundation.weighted_mean(
        { completeness, evidence_quality, 1.0 - uncertainty },
        { 0.40, 0.40, 0.20 }
    )

    local eco_impact_value = foundation.weighted_mean(
        { ecological_benefit, knowledge_factor },
        { 0.70, 0.30 }
    )

    local harm_risk = foundation.weighted_mean(
        { toxicity_risk, habitat_risk, water_risk, uncertainty },
        { 0.35, 0.30, 0.20, 0.15 }
    )

    return {
        knowledge_factor = foundation.round(knowledge_factor, 6),
        eco_impact_value = foundation.round(eco_impact_value, 6),
        harm_risk = foundation.round(harm_risk, 6),
    }
end

function foundation.decomposition_remaining(initial_mass_kg, rate_per_day, elapsed_days)
    assert(finite_number(initial_mass_kg) and initial_mass_kg >= 0,
        "initial_mass_kg must be a finite non-negative number")
    assert(finite_number(rate_per_day) and rate_per_day >= 0,
        "rate_per_day must be a finite non-negative number")
    assert(finite_number(elapsed_days) and elapsed_days >= 0,
        "elapsed_days must be a finite non-negative number")

    return initial_mass_kg * math.exp(-rate_per_day * elapsed_days)
end

function foundation.decomposition_fraction(initial_mass_kg, rate_per_day, elapsed_days)
    assert(finite_number(initial_mass_kg) and initial_mass_kg > 0,
        "initial_mass_kg must be a finite positive number")

    local remaining = foundation.decomposition_remaining(
        initial_mass_kg,
        rate_per_day,
        elapsed_days
    )

    return foundation.clamp(1.0 - (remaining / initial_mass_kg), 0.0, 1.0)
end

function foundation.waste_diversion_rate(diverted_mass_kg, total_mass_kg)
    assert(finite_number(diverted_mass_kg) and diverted_mass_kg >= 0,
        "diverted_mass_kg must be a finite non-negative number")
    assert(finite_number(total_mass_kg) and total_mass_kg > 0,
        "total_mass_kg must be a finite positive number")
    assert(diverted_mass_kg <= total_mass_kg,
        "diverted_mass_kg must not exceed total_mass_kg")

    return diverted_mass_kg / total_mass_kg
end

return foundation
