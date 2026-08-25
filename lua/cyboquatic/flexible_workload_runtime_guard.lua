local runtime_guard = {}

runtime_guard.ACTION = {
    HOLD_NONESSENTIAL_WORKLOADS = "hold_nonessential_workloads",
    REJECT_RUNTIME_ADJUSTMENT = "reject_runtime_adjustment",
    QUEUE_FOR_OPERATOR_APPROVAL = "queue_for_operator_approved_schedule"
}

runtime_guard.REASON = {
    INVALID_CONTEXT = "invalid_context",
    TELEMETRY_QUALITY_LOW = "telemetry_quality_low",
    BATTERY_HEALTH_NOT_APPROVED = "battery_health_not_within_reference_envelope",
    VOLTAGE_MARGIN_INSUFFICIENT = "predicted_voltage_below_safe_minimum",
    BATTERY_RESERVE_INSUFFICIENT = "battery_soc_below_critical_reserve",
    WATER_OPERATIONS_NOT_NORMAL = "water_operations_not_normal",
    EMERGENCY_ACTIVE = "emergency_status_not_normal",
    TASK_NOT_FLEXIBLE = "task_not_nonessential_deferrable",
    OPERATOR_APPROVAL_MISSING = "operator_approval_missing",
    TASK_ENERGY_INVALID = "task_energy_invalid",
    TASK_WINDOW_INVALID = "task_window_invalid",
    NO_SCHEDULING_CAPACITY = "no_safe_scheduling_capacity",
    SAFE_TO_QUEUE = "safe_to_queue_for_operator_approval"
}

local APPROVED_BATTERY_HEALTH = "within_reference_envelope"
local NORMAL_STATUS = "normal"
local APPROVED_STATUS = "approved"
local FLEXIBLE_TASK_CLASS = "nonessential_deferrable"

local function is_finite_number(value)
    return type(value) == "number"
        and value == value
        and value ~= math.huge
        and value ~= -math.huge
end

local function is_unit_interval(value)
    return is_finite_number(value) and value >= 0.0 and value <= 1.0
end

local function nonempty_string(value)
    return type(value) == "string" and value:match("%S") ~= nil
end

local function require_table(value, name)
    if type(value) ~= "table" then
        return false, name .. " must be a table"
    end
    return true
end

local function make_decision(action, reason, context)
    return {
        action = action,
        reason = reason,
        workload_id = context.workload_id,
        node_id = context.node_id,
        decision_timestamp_utc = context.decision_timestamp_utc,
        policy_version = context.policy_version,
        reversible = true,
        commands_physical_infrastructure = false,
        requires_human_operator_approval =
            action == runtime_guard.ACTION.QUEUE_FOR_OPERATOR_APPROVAL
    }
end

local function validate_context(context)
    local valid, message = require_table(context, "context")
    if not valid then
        return false, message
    end

    local required_strings = {
        "workload_id",
        "node_id",
        "decision_timestamp_utc",
        "policy_version",
        "battery_health_status",
        "water_operations_status",
        "emergency_status",
        "proposed_task_class",
        "operator_approval_status"
    }

    for _, field in ipairs(required_strings) do
        if not nonempty_string(context[field]) then
            return false, field .. " must be a non-empty string"
        end
    end

    local required_unit_intervals = {
        "telemetry_quality",
        "battery_soc",
        "critical_reserve_soc"
    }

    for _, field in ipairs(required_unit_intervals) do
        if not is_unit_interval(context[field]) then
            return false, field .. " must be a finite number within 0..1"
        end
    end

    local required_nonnegative_numbers = {
        "predicted_voltage_v",
        "voltage_safe_min_v",
        "requested_energy_j",
        "available_flexible_energy_j"
    }

    for _, field in ipairs(required_nonnegative_numbers) do
        if not is_finite_number(context[field]) or context[field] < 0.0 then
            return false, field .. " must be a finite non-negative number"
        end
    end

    if not is_finite_number(context.release_time_epoch_s)
        or not is_finite_number(context.deadline_time_epoch_s) then
        return false, "release_time_epoch_s and deadline_time_epoch_s must be finite numbers"
    end

    if context.deadline_time_epoch_s < context.release_time_epoch_s then
        return false, "deadline_time_epoch_s must not be earlier than release_time_epoch_s"
    end

    return true
end

function runtime_guard.evaluate(context)
    local valid = validate_context(context)
    if not valid then
        return make_decision(
            runtime_guard.ACTION.REJECT_RUNTIME_ADJUSTMENT,
            runtime_guard.REASON.INVALID_CONTEXT,
            type(context) == "table" and context or {}
        )
    end

    if context.telemetry_quality < 0.95 then
        return make_decision(
            runtime_guard.ACTION.HOLD_NONESSENTIAL_WORKLOADS,
            runtime_guard.REASON.TELEMETRY_QUALITY_LOW,
            context
        )
    end

    if context.battery_health_status ~= APPROVED_BATTERY_HEALTH then
        return make_decision(
            runtime_guard.ACTION.HOLD_NONESSENTIAL_WORKLOADS,
            runtime_guard.REASON.BATTERY_HEALTH_NOT_APPROVED,
            context
        )
    end

    if context.predicted_voltage_v < context.voltage_safe_min_v then
        return make_decision(
            runtime_guard.ACTION.HOLD_NONESSENTIAL_WORKLOADS,
            runtime_guard.REASON.VOLTAGE_MARGIN_INSUFFICIENT,
            context
        )
    end

    if context.battery_soc < context.critical_reserve_soc then
        return make_decision(
            runtime_guard.ACTION.HOLD_NONESSENTIAL_WORKLOADS,
            runtime_guard.REASON.BATTERY_RESERVE_INSUFFICIENT,
            context
        )
    end

    if context.water_operations_status ~= NORMAL_STATUS then
        return make_decision(
            runtime_guard.ACTION.HOLD_NONESSENTIAL_WORKLOADS,
            runtime_guard.REASON.WATER_OPERATIONS_NOT_NORMAL,
            context
        )
    end

    if context.emergency_status ~= NORMAL_STATUS then
        return make_decision(
            runtime_guard.ACTION.HOLD_NONESSENTIAL_WORKLOADS,
            runtime_guard.REASON.EMERGENCY_ACTIVE,
            context
        )
    end

    if context.proposed_task_class ~= FLEXIBLE_TASK_CLASS then
        return make_decision(
            runtime_guard.ACTION.REJECT_RUNTIME_ADJUSTMENT,
            runtime_guard.REASON.TASK_NOT_FLEXIBLE,
            context
        )
    end

    if context.operator_approval_status ~= APPROVED_STATUS then
        return make_decision(
            runtime_guard.ACTION.REJECT_RUNTIME_ADJUSTMENT,
            runtime_guard.REASON.OPERATOR_APPROVAL_MISSING,
            context
        )
    end

    if context.requested_energy_j <= 0.0 then
        return make_decision(
            runtime_guard.ACTION.REJECT_RUNTIME_ADJUSTMENT,
            runtime_guard.REASON.TASK_ENERGY_INVALID,
            context
        )
    end

    if context.deadline_time_epoch_s == context.release_time_epoch_s then
        return make_decision(
            runtime_guard.ACTION.REJECT_RUNTIME_ADJUSTMENT,
            runtime_guard.REASON.TASK_WINDOW_INVALID,
            context
        )
    end

    if context.requested_energy_j > context.available_flexible_energy_j then
        return make_decision(
            runtime_guard.ACTION.HOLD_NONESSENTIAL_WORKLOADS,
            runtime_guard.REASON.NO_SCHEDULING_CAPACITY,
            context
        )
    end

    return make_decision(
        runtime_guard.ACTION.QUEUE_FOR_OPERATOR_APPROVAL,
        runtime_guard.REASON.SAFE_TO_QUEUE,
        context
    )
end

function runtime_guard.is_physical_command(decision)
    return decision.commands_physical_infrastructure == true
end

return runtime_guard
