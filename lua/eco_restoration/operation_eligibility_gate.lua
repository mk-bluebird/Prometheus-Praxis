local OperationEligibilityGate = {}

local function finite(value, name)
    assert(type(value) == "number" and value == value and value ~= math.huge and value ~= -math.huge,
        name .. " must be finite")
    return value
end

local function bounded(value, name)
    finite(value, name)
    assert(value >= 0.0 and value <= 1.0, name .. " must be within 0..1")
    return value
end

local function nonempty(value, name)
    assert(type(value) == "string" and value:match("%S"),
        name .. " must be a non-empty string")
    return value
end

function OperationEligibilityGate.evaluate(input)
    assert(type(input) == "table", "input must be a table")

    nonempty(input.operation_id, "operation_id")
    nonempty(input.hex_anchor_id, "hex_anchor_id")
    nonempty(input.steward_did, "steward_did")

    bounded(input.knowledge_factor, "knowledge_factor")
    bounded(input.eco_impact_value, "eco_impact_value")
    bounded(input.harm_risk, "harm_risk")
    bounded(input.minimum_knowledge_factor, "minimum_knowledge_factor")
    bounded(input.minimum_eco_impact_value, "minimum_eco_impact_value")
    bounded(input.maximum_harm_risk, "maximum_harm_risk")

    assert(type(input.corridor_safe) == "boolean", "corridor_safe must be boolean")
    assert(type(input.hex_consistent) == "boolean", "hex_consistent must be boolean")
    assert(type(input.sovereign_veto) == "boolean", "sovereign_veto must be boolean")
    assert(type(input.human_approval) == "boolean", "human_approval must be boolean")

    local blocked_reasons = {}

    if not input.corridor_safe then
        blocked_reasons[#blocked_reasons + 1] = "ecological_corridor_breached"
    end
    if not input.hex_consistent then
        blocked_reasons[#blocked_reasons + 1] = "hex_anchor_inconsistent"
    end
    if input.knowledge_factor < input.minimum_knowledge_factor then
        blocked_reasons[#blocked_reasons + 1] = "knowledge_factor_below_threshold"
    end
    if input.eco_impact_value < input.minimum_eco_impact_value then
        blocked_reasons[#blocked_reasons + 1] = "eco_impact_value_below_threshold"
    end
    if input.harm_risk > input.maximum_harm_risk then
        blocked_reasons[#blocked_reasons + 1] = "harm_risk_above_threshold"
    end
    if input.sovereign_veto then
        blocked_reasons[#blocked_reasons + 1] = "sovereign_veto_active"
    end
    if not input.human_approval then
        blocked_reasons[#blocked_reasons + 1] = "human_approval_absent"
    end

    return {
        operation_id = input.operation_id,
        hex_anchor_id = input.hex_anchor_id,
        steward_did = input.steward_did,
        eligible = #blocked_reasons == 0,
        verdict = #blocked_reasons == 0 and "eligible" or "blocked",
        blocked_reasons = blocked_reasons,
        physical_operation_authorized = false
    }
end

return OperationEligibilityGate
