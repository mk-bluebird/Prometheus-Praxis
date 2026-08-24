local M = {}

local VALID_SERVICE_STATUS = {
    MEETS_BASELINE = true,
    BELOW_BASELINE = true,
    UNKNOWN = true,
    FALLBACK_ACTIVE = true,
}

local VALID_INTERVENTION_TARGETS = {
    CITY_INFRASTRUCTURE = true,
    PUBLIC_ENVIRONMENT = true,
    PERSON_AFFECTING = true,
}

local VALID_CONSENT_STATUS = {
    ACTIVE = true,
    REVOKED = true,
    EXPIRED = true,
    COMPLETED = true,
}

local VALID_DECISIONS = {
    ALLOW = true,
    BLOCK = true,
    REQUIRE_HUMAN_REVIEW = true,
}

local function is_string(value)
    return type(value) == "string" and #value > 0
end

local function is_integer(value)
    return type(value) == "number" and value % 1 == 0
end

local function is_positive_integer(value)
    return is_integer(value) and value > 0
end

local function clamp(value, lower, upper)
    if value < lower then
        return lower
    end
    if value > upper then
        return upper
    end
    return value
end

local function copy_array(values)
    local output = {}
    for index, value in ipairs(values) do
        output[index] = value
    end
    return output
end

local function reject(reason, harm_risk)
    return {
        decision = "BLOCK",
        allowed = false,
        reason = reason,
        knowledge_factor = 0,
        eco_impact_value = 0,
        harm_risk = harm_risk or 1,
    }
end

local function has_forbidden_identity_field(record)
    local forbidden = {
        "person_id",
        "resident_id",
        "account_id",
        "did",
        "biometric",
        "diagnosis",
        "medical_record",
        "identity_claim",
        "identity_declaration",
        "augmentation_status",
        "device_serial",
        "name",
        "email",
        "phone_number",
        "address",
    }

    if type(record) ~= "table" then
        return nil
    end

    for _, field in ipairs(forbidden) do
        if record[field] ~= nil then
            return field
        end
    end

    return nil
end

local function validate_service_observation(observation)
    if type(observation) ~= "table" then
        return nil, "service_observation_must_be_table"
    end

    local forbidden = has_forbidden_identity_field(observation)
    if forbidden then
        return nil, "identity_field_forbidden:" .. forbidden
    end

    if not is_string(observation.service_code) then
        return nil, "service_code_required"
    end

    if not is_string(observation.zone_code) then
        return nil, "zone_code_required"
    end

    if not is_positive_integer(observation.observed_at) then
        return nil, "observed_at_required"
    end

    if type(observation.baseline_units) ~= "number" or observation.baseline_units < 0 then
        return nil, "baseline_units_invalid"
    end

    if type(observation.delivered_units) ~= "number" or observation.delivered_units < 0 then
        return nil, "delivered_units_invalid"
    end

    if not VALID_SERVICE_STATUS[observation.status] then
        return nil, "invalid_service_status"
    end

    return {
        service_code = observation.service_code,
        zone_code = observation.zone_code,
        observed_at = observation.observed_at,
        baseline_units = observation.baseline_units,
        delivered_units = observation.delivered_units,
        status = observation.status,
    }
end

function M.compute_aggregate_disparity(observations)
    if type(observations) ~= "table" or #observations == 0 then
        return reject("at_least_one_aggregate_observation_required")
    end

    local normalized = {}
    local delivered_sum = 0
    local baseline_sum = 0

    for index, observation in ipairs(observations) do
        local valid, error_message = validate_service_observation(observation)
        if not valid then
            return reject("invalid_observation_" .. tostring(index) .. ":" .. error_message)
        end

        normalized[#normalized + 1] = valid
        delivered_sum = delivered_sum + valid.delivered_units
        baseline_sum = baseline_sum + valid.baseline_units
    end

    local city_average = delivered_sum / #normalized
    local min_ratio = math.huge
    local deficits = {}
    local rows = {}

    for _, observation in ipairs(normalized) do
        local ratio
        if observation.baseline_units == 0 then
            ratio = 1
        else
            ratio = observation.delivered_units / observation.baseline_units
        end

        ratio = clamp(ratio, 0, 1)
        min_ratio = math.min(min_ratio, ratio)

        local disparity = observation.delivered_units - city_average
        local shortfall = math.max(0, observation.baseline_units - observation.delivered_units)

        rows[#rows + 1] = {
            service_code = observation.service_code,
            zone_code = observation.zone_code,
            observed_at = observation.observed_at,
            baseline_units = observation.baseline_units,
            delivered_units = observation.delivered_units,
            service_ratio = ratio,
            disparity_from_city_average = disparity,
            shortfall_units = shortfall,
            status = observation.status,
        }

        if shortfall > 0 or observation.status == "UNKNOWN" then
            deficits[#deficits + 1] = {
                service_code = observation.service_code,
                zone_code = observation.zone_code,
                reason = observation.status == "UNKNOWN"
                    and "service_status_unknown_requires_verification"
                    or "baseline_service_shortfall",
                shortfall_units = shortfall,
            }
        end
    end

    local knowledge_factor = #deficits == 0 and 0.95 or 0.8
    local eco_impact_value = #deficits == 0 and 0.55 or 0.7
    local harm_risk = #deficits == 0 and 0.04 or 0.2

    return {
        decision = #deficits == 0 and "ALLOW" or "REQUIRE_HUMAN_REVIEW",
        allowed = #deficits == 0,
        city_average_delivered_units = city_average,
        aggregate_baseline_units = baseline_sum,
        aggregate_delivered_units = delivered_sum,
        fairness_floor = min_ratio,
        rows = rows,
        remediation_required = deficits,
        knowledge_factor = knowledge_factor,
        eco_impact_value = eco_impact_value,
        harm_risk = harm_risk,
    }
end

function M.evaluate_rights_floor(proposal)
    if type(proposal) ~= "table" then
        return reject("proposal_must_be_table")
    end

    local forbidden = has_forbidden_identity_field(proposal)
    if forbidden then
        return reject("identity_feature_forbidden:" .. forbidden)
    end

    if proposal.baseline_access_preserved ~= true then
        return reject("rights_floor_baseline_access_not_preserved", 0.9)
    end

    if proposal.accessible_fallback_available ~= true then
        return reject("rights_floor_accessible_fallback_missing", 0.85)
    end

    if proposal.non_discrimination_passed ~= true then
        return reject("rights_floor_non_discrimination_failed", 0.95)
    end

    if proposal.privacy_minimization_passed ~= true then
        return reject("rights_floor_privacy_minimization_failed", 0.8)
    end

    if proposal.person_affecting_action == true then
        return {
            decision = "REQUIRE_HUMAN_REVIEW",
            allowed = false,
            reason = "person_affecting_action_requires_valid_consent_artifact",
            knowledge_factor = 0.8,
            eco_impact_value = 0.45,
            harm_risk = 0.4,
        }
    end

    if proposal.safety_envelope_passed ~= true then
        return reject("safety_envelope_failed", 0.75)
    end

    if proposal.ecological_guard_passed ~= true then
        return reject("ecological_guard_failed", 0.7)
    end

    return {
        decision = "ALLOW",
        allowed = true,
        precedence = {
            "RIGHTS_FLOOR",
            "SAFETY",
            "ECOLOGICAL_GUARD",
            "EFFICIENCY",
        },
        knowledge_factor = 0.9,
        eco_impact_value = 0.72,
        harm_risk = 0.06,
    }
end

local function validate_consent_artifact(artifact, now)
    if type(artifact) ~= "table" then
        return nil, "consent_artifact_must_be_table"
    end

    local forbidden = has_forbidden_identity_field(artifact)
    if forbidden then
        return nil, "identity_field_forbidden:" .. forbidden
    end

    if not is_string(artifact.consent_handle) then
        return nil, "consent_handle_required"
    end

    if not is_string(artifact.action_code) then
        return nil, "action_code_required"
    end

    if not is_string(artifact.scope_code) then
        return nil, "scope_code_required"
    end

    if not is_string(artifact.target_class)
        or not VALID_INTERVENTION_TARGETS[artifact.target_class] then
        return nil, "invalid_target_class"
    end

    if artifact.revocable ~= true then
        return nil, "revocable_consent_required"
    end

    if artifact.informed ~= true then
        return nil, "informed_consent_required"
    end

    if artifact.voluntary ~= true then
        return nil, "voluntary_consent_required"
    end

    if not VALID_CONSENT_STATUS[artifact.status] then
        return nil, "invalid_consent_status"
    end

    if not is_positive_integer(artifact.issued_at)
        or not is_positive_integer(artifact.expires_at)
        or artifact.expires_at < artifact.issued_at then
        return nil, "invalid_consent_time_bounds"
    end

    if artifact.status ~= "ACTIVE" then
        return nil, "consent_not_active"
    end

    if now < artifact.issued_at or now > artifact.expires_at then
        return nil, "consent_outside_valid_time_window"
    end

    return {
        consent_handle = artifact.consent_handle,
        action_code = artifact.action_code,
        scope_code = artifact.scope_code,
        target_class = artifact.target_class,
        revocable = true,
        informed = true,
        voluntary = true,
        status = artifact.status,
        issued_at = artifact.issued_at,
        expires_at = artifact.expires_at,
    }
end

function M.authorize_consent_gated_intervention(intervention, artifact, now)
    if type(intervention) ~= "table" then
        return reject("intervention_must_be_table")
    end

    local forbidden = has_forbidden_identity_field(intervention)
    if forbidden then
        return reject("identity_field_forbidden:" .. forbidden)
    end

    if not is_string(intervention.action_code)
        or not is_string(intervention.scope_code)
        or not is_string(intervention.target_class)
        or not VALID_INTERVENTION_TARGETS[intervention.target_class] then
        return reject("invalid_intervention_descriptor")
    end

    if intervention.target_class ~= "PERSON_AFFECTING" then
        return reject("consent_artifact_only_authorizes_person_affecting_actions", 0.45)
    end

    if not is_positive_integer(now) then
        return reject("current_time_required")
    end

    local valid_artifact, error_message = validate_consent_artifact(artifact, now)
    if not valid_artifact then
        return reject(error_message, 0.9)
    end

    if valid_artifact.target_class ~= "PERSON_AFFECTING"
        or valid_artifact.action_code ~= intervention.action_code
        or valid_artifact.scope_code ~= intervention.scope_code then
        return reject("consent_scope_mismatch", 0.95)
    end

    if intervention.least_intrusive ~= true then
        return reject("least_intrusive_method_required", 0.7)
    end

    if intervention.independent_review_passed ~= true then
        return {
            decision = "REQUIRE_HUMAN_REVIEW",
            allowed = false,
            reason = "independent_review_required",
            knowledge_factor = 0.75,
            eco_impact_value = 0.35,
            harm_risk = 0.5,
        }
    end

    return {
        decision = "ALLOW",
        allowed = true,
        authorization = {
            consent_handle = valid_artifact.consent_handle,
            action_code = valid_artifact.action_code,
            scope_code = valid_artifact.scope_code,
            expires_at = valid_artifact.expires_at,
            revocable = true,
        },
        knowledge_factor = 0.95,
        eco_impact_value = 0.35,
        harm_risk = 0.12,
    }
end

function M.revoke_consent_artifact(artifact, now)
    if type(artifact) ~= "table" then
        return reject("consent_artifact_must_be_table")
    end

    if not is_positive_integer(now) then
        return reject("current_time_required")
    end

    if artifact.revocable ~= true then
        return reject("artifact_not_revocable", 0.85)
    end

    if artifact.status ~= "ACTIVE" then
        return reject("only_active_artifact_may_be_revoked")
    end

    local revoked = {}
    for key, value in pairs(artifact) do
        revoked[key] = value
    end

    revoked.status = "REVOKED"
    revoked.revoked_at = now

    return {
        decision = "ALLOW",
        allowed = true,
        consent_artifact = revoked,
        knowledge_factor = 0.98,
        eco_impact_value = 0.6,
        harm_risk = 0.02,
    }
end

return M
