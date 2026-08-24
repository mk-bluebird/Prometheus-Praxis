local M = {}

local MAX_TTL_SECONDS = 300
local MAX_NONCE_LENGTH = 128
local MAX_REQUIREMENT_LENGTH = 96

local VALID_REQUIREMENTS = {
    ACCESSIBLE_DOOR_HOLD_OPEN = true,
    ACCESSIBLE_SIGNAL_EXTEND = true,
    LOW_EMI_ROUTE_NOTICE = true,
    HUMAN_ASSISTANCE_REQUEST = true,
    NON_DIGITAL_COMMUNICATION = true,
}

local VALID_SCOPES = {
    ACCESSIBILITY_INTERFACE = true,
    CIVIC_ASSISTANCE_PANEL = true,
    TRANSIT_ACCESS_INTERFACE = true,
}

local VALID_INFRASTRUCTURE_TARGETS = {
    DOOR = true,
    PEDESTRIAN_SIGNAL = true,
    ASSISTANCE_PANEL = true,
    TRANSIT_GATE = true,
}

local function is_string(value)
    return type(value) == "string" and #value > 0
end

local function is_integer(value)
    return type(value) == "number" and value % 1 == 0
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

local function copy_table(value)
    local out = {}
    for key, item in pairs(value) do
        out[key] = item
    end
    return out
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

local function validate_need_packet(packet, now)
    if type(packet) ~= "table" then
        return nil, "packet_must_be_table"
    end

    local forbidden = {
        "did",
        "biometric",
        "identity",
        "identity_claim",
        "medical_record",
        "diagnosis",
        "device_serial",
        "augmentation_status",
        "certificate",
        "person_id",
        "account_id",
    }

    for _, field in ipairs(forbidden) do
        if packet[field] ~= nil then
            return nil, "identity_or_medical_field_forbidden:" .. field
        end
    end

    if not is_string(packet.functional_requirement)
        or #packet.functional_requirement > MAX_REQUIREMENT_LENGTH
        or not VALID_REQUIREMENTS[packet.functional_requirement] then
        return nil, "invalid_functional_requirement"
    end

    if not is_integer(packet.expires_at) then
        return nil, "invalid_expiry"
    end

    if packet.expires_at <= now then
        return nil, "packet_expired"
    end

    if packet.expires_at - now > MAX_TTL_SECONDS then
        return nil, "packet_ttl_exceeds_limit"
    end

    if not is_string(packet.nonce) or #packet.nonce > MAX_NONCE_LENGTH then
        return nil, "invalid_nonce"
    end

    return {
        functional_requirement = packet.functional_requirement,
        expires_at = packet.expires_at,
        nonce = packet.nonce,
    }
end

function M.validate_need_packet(packet, now)
    if not is_integer(now) or now < 0 then
        return reject("invalid_current_time")
    end

    local normalized, error_message = validate_need_packet(packet, now)
    if not normalized then
        return reject(error_message)
    end

    return {
        allowed = true,
        packet = normalized,
        knowledge_factor = 1,
        eco_impact_value = 0.55,
        harm_risk = 0.03,
    }
end

function M.authorize_ephemeral_action(packet, auth, target, now, nonce_seen)
    local packet_result = M.validate_need_packet(packet, now)
    if not packet_result.allowed then
        return packet_result
    end

    if type(auth) ~= "table" then
        return reject("authentication_context_required")
    end

    if not is_string(auth.ephemeral_public_key) then
        return reject("ephemeral_key_required")
    end

    if not is_string(auth.action_scope) or not VALID_SCOPES[auth.action_scope] then
        return reject("invalid_action_scope")
    end

    if type(target) ~= "table"
        or not is_string(target.infrastructure_class)
        or not VALID_INFRASTRUCTURE_TARGETS[target.infrastructure_class]
        or not is_string(target.local_zone) then
        return reject("invalid_infrastructure_target")
    end

    if type(nonce_seen) == "function" and nonce_seen(packet_result.packet.nonce) then
        return reject("replayed_nonce")
    end

    local requirement = packet_result.packet.functional_requirement
    local permitted = {
        ACCESSIBLE_DOOR_HOLD_OPEN = target.infrastructure_class == "DOOR",
        ACCESSIBLE_SIGNAL_EXTEND = target.infrastructure_class == "PEDESTRIAN_SIGNAL",
        LOW_EMI_ROUTE_NOTICE = target.infrastructure_class == "ASSISTANCE_PANEL",
        HUMAN_ASSISTANCE_REQUEST = target.infrastructure_class == "ASSISTANCE_PANEL"
            or target.infrastructure_class == "TRANSIT_GATE",
        NON_DIGITAL_COMMUNICATION = target.infrastructure_class == "ASSISTANCE_PANEL",
    }

    if not permitted[requirement] then
        return reject("requirement_not_supported_by_target")
    end

    return {
        allowed = true,
        action = {
            requirement = requirement,
            expires_at = packet_result.packet.expires_at,
            target_class = target.infrastructure_class,
            local_zone = target.local_zone,
            action_scope = auth.action_scope,
        },
        knowledge_factor = 0.9,
        eco_impact_value = 0.58,
        harm_risk = 0.06,
    }
end

local function validate_public_actuator(actuator)
    if type(actuator) ~= "table" then
        return nil, "actuator_must_be_table"
    end

    if actuator.private_device == true
        or actuator.medical_support == true
        or actuator.person_affecting_target == true
        or actuator.identity_record == true then
        return nil, "private_or_person_affecting_target_forbidden"
    end

    if not is_string(actuator.infrastructure_class)
        or not VALID_INFRASTRUCTURE_TARGETS[actuator.infrastructure_class] then
        return nil, "invalid_public_infrastructure_class"
    end

    return {
        infrastructure_class = actuator.infrastructure_class,
        local_zone = actuator.local_zone,
        sensor_healthy = actuator.sensor_healthy == true,
        public_asset = actuator.public_asset == true,
    }
end

function M.evaluate_public_actuation(actuator, command, safety)
    local safe_actuator, error_message = validate_public_actuator(actuator)
    if not safe_actuator then
        return reject(error_message)
    end

    if not safe_actuator.public_asset then
        return reject("municipal_authority_limited_to_public_assets")
    end

    if type(command) ~= "table" or not is_string(command.kind) then
        return reject("invalid_command")
    end

    if type(safety) ~= "table" then
        return reject("missing_safety_state")
    end

    if command.kind == "CLOSE_DOOR" then
        if safe_actuator.infrastructure_class ~= "DOOR" then
            return reject("door_command_requires_door_target")
        end

        if not safe_actuator.sensor_healthy
            or safety.presence_zone_clear ~= true
            or safety.calibration_fresh ~= true
            or safety.uncertainty_bounded ~= true
            or safety.safe_clearance_m == nil
            or safety.safe_clearance_m <= 0 then
            return {
                allowed = false,
                command = "HOLD_OPEN",
                reason = "door_closure_guard_failed",
                knowledge_factor = 0.75,
                eco_impact_value = 0.42,
                harm_risk = 0.02,
            }
        end
    end

    return {
        allowed = true,
        command = command.kind,
        target = copy_table(safe_actuator),
        knowledge_factor = 0.9,
        eco_impact_value = 0.5,
        harm_risk = 0.08,
    }
end

local function lookup_force_limit(lut, pain_index, fear_index)
    if type(lut) ~= "table" then
        return nil
    end

    local pain_bucket = clamp(math.floor(pain_index * 10 + 0.5), 0, 10)
    local fear_bucket = clamp(math.floor(fear_index * 10 + 0.5), 0, 10)
    local row = lut[pain_bucket]

    if type(row) ~= "table" then
        return nil
    end

    local limit = row[fear_bucket]
    if type(limit) ~= "number" or limit < 0 then
        return nil
    end

    return limit
end

function M.enforce_force_clamp(request)
    if type(request) ~= "table" then
        return reject("force_request_must_be_table")
    end

    if request.person_targeted == true
        or request.private_device == true
        or request.medical_support == true
        or request.identity_effect == true then
        return reject("personal_or_private_intervention_forbidden")
    end

    if not is_string(request.actuator_class)
        or not VALID_INFRASTRUCTURE_TARGETS[request.actuator_class] then
        return reject("invalid_public_actuator_class")
    end

    if type(request.requested_force) ~= "number" or request.requested_force < 0 then
        return reject("invalid_requested_force")
    end

    if type(request.pain_index) ~= "number"
        or type(request.fear_index) ~= "number"
        or request.pain_index < 0
        or request.pain_index > 1
        or request.fear_index < 0
        or request.fear_index > 1 then
        return reject("invalid_safety_indices")
    end

    if request.sensor_healthy ~= true
        or request.calibration_fresh ~= true
        or request.public_asset ~= true then
        return {
            allowed = false,
            hard_stop = true,
            reason = "public_actuator_safety_evidence_insufficient",
            permitted_force = 0,
            knowledge_factor = 0.35,
            eco_impact_value = 0.2,
            harm_risk = 0.04,
        }
    end

    local permitted_force = lookup_force_limit(
        request.force_lut,
        request.pain_index,
        request.fear_index
    )

    if not permitted_force then
        return {
            allowed = false,
            hard_stop = true,
            reason = "force_lut_unavailable",
            permitted_force = 0,
            knowledge_factor = 0.2,
            eco_impact_value = 0.1,
            harm_risk = 0.03,
        }
    end

    if request.requested_force > permitted_force then
        return {
            allowed = false,
            hard_stop = true,
            reason = "requested_force_exceeds_conservative_limit",
            permitted_force = permitted_force,
            knowledge_factor = 0.85,
            eco_impact_value = 0.4,
            harm_risk = 0.02,
        }
    end

    return {
        allowed = true,
        hard_stop = false,
        permitted_force = permitted_force,
        knowledge_factor = 0.9,
        eco_impact_value = 0.45,
        harm_risk = 0.06,
    }
end

return M
