local M = {}

local VALID_CHANNELS = {
    WEB = true,
    PHONE = true,
    IN_PERSON = true,
    POSTAL_MAIL = true,
    ASSISTANCE_PANEL = true,
}

local VALID_DECISIONS = {
    DENY = true,
    RESTRICT = true,
    DEFER = true,
    FALLBACK_REQUIRED = true,
}

local VALID_APPEAL_STATUS = {
    RECEIVED = true,
    UNDER_REVIEW = true,
    REMEDY_OFFERED = true,
    CLOSED = true,
}

local VALID_LOG_STATUS = {
    RECORDED = true,
    APPEALED = true,
    REMEDIED = true,
    EXPIRED = true,
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

local function copy_table(value)
    local output = {}
    for key, item in pairs(value) do
        output[key] = item
    end
    return output
end

local function reject(reason)
    return {
        allowed = false,
        reason = reason,
        knowledge_factor = 0,
        eco_impact_value = 0,
        harm_risk = 1,
    }
end

local function safe_text(value, maximum_length)
    return is_string(value) and #value <= maximum_length
end

local function contains_forbidden_identity_field(record)
    local forbidden = {
        "person_id",
        "resident_id",
        "account_id",
        "did",
        "biometric",
        "diagnosis",
        "medical_record",
        "augmentation_status",
        "identity_claim",
        "identity_declaration",
        "device_serial",
        "phone_number",
        "email",
        "address",
        "name",
    }

    for _, field in ipairs(forbidden) do
        if record[field] ~= nil then
            return field
        end
    end

    return nil
end

local function validate_psych_continuity_input(input)
    if type(input) ~= "table" then
        return nil, "interaction_must_be_table"
    end

    local forbidden = contains_forbidden_identity_field(input)
    if forbidden then
        return nil, "identity_or_medical_field_forbidden:" .. forbidden
    end

    if not is_string(input.interaction_mode) then
        return nil, "interaction_mode_required"
    end

    if type(input.cognitive_load) ~= "number"
        or input.cognitive_load < 0
        or input.cognitive_load > 1 then
        return nil, "cognitive_load_must_be_in_zero_one"
    end

    if type(input.psych_continuity_floor) ~= "number"
        or input.psych_continuity_floor < 0
        or input.psych_continuity_floor > 1 then
        return nil, "psych_continuity_floor_must_be_in_zero_one"
    end

    if type(input.multimodal_fallback_available) ~= "boolean" then
        return nil, "multimodal_fallback_flag_required"
    end

    return {
        interaction_mode = input.interaction_mode,
        cognitive_load = input.cognitive_load,
        psych_continuity_floor = input.psych_continuity_floor,
        multimodal_fallback_available = input.multimodal_fallback_available,
    }
end

function M.evaluate_psych_continuity(input)
    local interaction, error_message = validate_psych_continuity_input(input)
    if not interaction then
        return reject(error_message)
    end

    local permitted_load = 1 - interaction.psych_continuity_floor
    local within_envelope = interaction.cognitive_load <= permitted_load

    if within_envelope then
        return {
            allowed = true,
            required_mode = interaction.interaction_mode,
            permitted_cognitive_load = permitted_load,
            knowledge_factor = 0.85,
            eco_impact_value = 0.55,
            harm_risk = 0.05,
        }
    end

    if interaction.multimodal_fallback_available then
        return {
            allowed = false,
            fallback_required = true,
            reason = "cognitive_load_exceeds_continuity_envelope",
            required_mode = "MULTIMODAL_OR_HUMAN_ASSISTANCE",
            permitted_cognitive_load = permitted_load,
            knowledge_factor = 0.8,
            eco_impact_value = 0.58,
            harm_risk = 0.08,
        }
    end

    return {
        allowed = false,
        fallback_required = true,
        reason = "cognitive_load_exceeds_envelope_without_fallback",
        required_mode = "HUMAN_ASSISTANCE_REQUIRED",
        permitted_cognitive_load = permitted_load,
        knowledge_factor = 0.65,
        eco_impact_value = 0.45,
        harm_risk = 0.16,
    }
end

local function validate_adverse_decision(decision)
    if type(decision) ~= "table" then
        return nil, "decision_must_be_table"
    end

    local forbidden = contains_forbidden_identity_field(decision)
    if forbidden then
        return nil, "identity_or_medical_field_forbidden:" .. forbidden
    end

    if not is_string(decision.decision_id) or #decision.decision_id > 128 then
        return nil, "invalid_decision_id"
    end

    if not VALID_DECISIONS[decision.outcome] then
        return nil, "invalid_adverse_outcome"
    end

    if not safe_text(decision.policy_id, 128) then
        return nil, "policy_id_required"
    end

    if not safe_text(decision.reason_code, 128) then
        return nil, "reason_code_required"
    end

    if not safe_text(decision.accountable_unit, 128) then
        return nil, "accountable_unit_required"
    end

    if not is_positive_integer(decision.recorded_at) then
        return nil, "recorded_at_required"
    end

    if not is_positive_integer(decision.remedy_due_at)
        or decision.remedy_due_at < decision.recorded_at then
        return nil, "invalid_remedy_timeline"
    end

    if type(decision.available_channels) ~= "table" then
        return nil, "appeal_channels_required"
    end

    local valid_channel_count = 0
    local has_non_digital_channel = false

    for _, channel in ipairs(decision.available_channels) do
        if not VALID_CHANNELS[channel] then
            return nil, "invalid_appeal_channel"
        end

        valid_channel_count = valid_channel_count + 1

        if channel == "PHONE" or channel == "IN_PERSON" or channel == "POSTAL_MAIL" then
            has_non_digital_channel = true
        end
    end

    if valid_channel_count == 0 then
        return nil, "at_least_one_appeal_channel_required"
    end

    if not has_non_digital_channel then
        return nil, "non_digital_appeal_channel_required"
    end

    return {
        decision_id = decision.decision_id,
        outcome = decision.outcome,
        policy_id = decision.policy_id,
        reason_code = decision.reason_code,
        accountable_unit = decision.accountable_unit,
        recorded_at = decision.recorded_at,
        remedy_due_at = decision.remedy_due_at,
        available_channels = copy_table(decision.available_channels),
    }
end

function M.create_adverse_decision_log(decision)
    local validated, error_message = validate_adverse_decision(decision)
    if not validated then
        return reject(error_message)
    end

    return {
        allowed = true,
        log_record = {
            decision_id = validated.decision_id,
            outcome = validated.outcome,
            policy_id = validated.policy_id,
            reason_code = validated.reason_code,
            accountable_unit = validated.accountable_unit,
            recorded_at = validated.recorded_at,
            remedy_due_at = validated.remedy_due_at,
            appeal_channels = validated.available_channels,
            status = "RECORDED",
        },
        knowledge_factor = 0.95,
        eco_impact_value = 0.62,
        harm_risk = 0.04,
    }
end

function M.open_identity_neutral_appeal(log_record, appeal)
    if type(log_record) ~= "table" then
        return reject("log_record_required")
    end

    if type(appeal) ~= "table" then
        return reject("appeal_must_be_table")
    end

    local forbidden = contains_forbidden_identity_field(appeal)
    if forbidden then
        return reject("identity_or_medical_field_forbidden:" .. forbidden)
    end

    if not safe_text(appeal.decision_id, 128)
        or appeal.decision_id ~= log_record.decision_id then
        return reject("appeal_decision_id_mismatch")
    end

    if not VALID_CHANNELS[appeal.channel] then
        return reject("invalid_appeal_channel")
    end

    local channel_offered = false
    for _, offered in ipairs(log_record.appeal_channels or {}) do
        if offered == appeal.channel then
            channel_offered = true
            break
        end
    end

    if not channel_offered then
        return reject("appeal_channel_not_offered")
    end

    if not is_positive_integer(appeal.received_at) then
        return reject("appeal_received_at_required")
    end

    if not is_positive_integer(appeal.review_due_at)
        or appeal.review_due_at < appeal.received_at then
        return reject("invalid_review_timeline")
    end

    if appeal.status ~= nil and not VALID_APPEAL_STATUS[appeal.status] then
        return reject("invalid_appeal_status")
    end

    return {
        allowed = true,
        appeal_record = {
            appeal_handle = appeal.appeal_handle or "LOCAL_UNLINKED_APPEAL",
            decision_id = appeal.decision_id,
            channel = appeal.channel,
            received_at = appeal.received_at,
            review_due_at = appeal.review_due_at,
            status = appeal.status or "RECEIVED",
        },
        knowledge_factor = 0.9,
        eco_impact_value = 0.65,
        harm_risk = 0.05,
    }
end

function M.transition_log_status(log_record, next_status, now)
    if type(log_record) ~= "table" then
        return reject("log_record_required")
    end

    if not VALID_LOG_STATUS[next_status] then
        return reject("invalid_log_status")
    end

    if not is_positive_integer(now) then
        return reject("transition_time_required")
    end

    if not is_positive_integer(log_record.recorded_at)
        or now < log_record.recorded_at then
        return reject("invalid_transition_time")
    end

    local permitted = {
        RECORDED = {
            APPEALED = true,
            REMEDIED = true,
            EXPIRED = true,
        },
        APPEALED = {
            REMEDIED = true,
            EXPIRED = true,
        },
        REMEDIED = {},
        EXPIRED = {},
    }

    local current = log_record.status or "RECORDED"
    if not permitted[current] or not permitted[current][next_status] then
        return reject("invalid_status_transition")
    end

    local updated = copy_table(log_record)
    updated.status = next_status
    updated.status_changed_at = now

    return {
        allowed = true,
        log_record = updated,
        knowledge_factor = 0.95,
        eco_impact_value = 0.6,
        harm_risk = 0.04,
    }
end

function M.audit_log_record(log_record)
    if type(log_record) ~= "table" then
        return reject("log_record_required")
    end

    local forbidden = contains_forbidden_identity_field(log_record)
    if forbidden then
        return reject("identity_or_medical_field_present:" .. forbidden)
    end

    local required = {
        "decision_id",
        "outcome",
        "policy_id",
        "reason_code",
        "accountable_unit",
        "recorded_at",
        "remedy_due_at",
        "appeal_channels",
        "status",
    }

    for _, field in ipairs(required) do
        if log_record[field] == nil then
            return reject("missing_audit_field:" .. field)
        end
    end

    if not VALID_DECISIONS[log_record.outcome] then
        return reject("invalid_logged_outcome")
    end

    if not VALID_LOG_STATUS[log_record.status] then
        return reject("invalid_logged_status")
    end

    if type(log_record.appeal_channels) ~= "table" then
        return reject("invalid_logged_appeal_channels")
    end

    local non_digital = false
    for _, channel in ipairs(log_record.appeal_channels) do
        if channel == "PHONE" or channel == "IN_PERSON" or channel == "POSTAL_MAIL" then
            non_digital = true
            break
        end
    end

    if not non_digital then
        return reject("logged_non_digital_appeal_channel_missing")
    end

    return {
        allowed = true,
        audit_result = {
            decision_id = log_record.decision_id,
            audit_status = "PASS",
            identity_fields_present = false,
            non_digital_appeal_available = true,
            accountable_unit = log_record.accountable_unit,
            policy_id = log_record.policy_id,
        },
        knowledge_factor = 0.98,
        eco_impact_value = 0.62,
        harm_risk = 0.03,
    }
end

return M
