local guard = require("lua.cyboquatic.flexible_workload_runtime_guard")

local passed = 0
local failed = 0

local function assert_equal(actual, expected, message)
    if actual ~= expected then
        error(message .. ": expected " .. tostring(expected) .. ", got " .. tostring(actual))
    end
end

local function test(name, fn)
    local ok, error_message = pcall(fn)
    if ok then
        passed = passed + 1
        io.write("PASS ", name, "\n")
    else
        failed = failed + 1
        io.write("FAIL ", name, ": ", error_message, "\n")
    end
end

local function valid_context()
    return {
        workload_id = "inference-batch-001",
        node_id = "canal-node-phx-07",
        decision_timestamp_utc = "2026-08-25T19:40:00Z",
        policy_version = "1.0.0",
        telemetry_quality = 0.98,
        battery_health_status = "within_reference_envelope",
        predicted_voltage_v = 49.2,
        voltage_safe_min_v = 47.5,
        battery_soc = 0.82,
        critical_reserve_soc = 0.45,
        water_operations_status = "normal",
        emergency_status = "normal",
        proposed_task_class = "nonessential_deferrable",
        operator_approval_status = "approved",
        requested_energy_j = 120000.0,
        available_flexible_energy_j = 250000.0,
        release_time_epoch_s = 1787686800,
        deadline_time_epoch_s = 1787690400
    }
end

test("queues an approved safe flexible workload", function()
    local result = guard.evaluate(valid_context())
    assert_equal(
        result.action,
        guard.ACTION.QUEUE_FOR_OPERATOR_APPROVAL,
        "safe workload action"
    )
    assert_equal(result.reason, guard.REASON.SAFE_TO_QUEUE, "safe workload reason")
    assert_equal(result.reversible, true, "queue decision must be reversible")
    assert_equal(
        guard.is_physical_command(result),
        false,
        "runtime guard must not issue a physical command"
    )
end)

test("holds on low-quality telemetry", function()
    local context = valid_context()
    context.telemetry_quality = 0.94
    local result = guard.evaluate(context)
    assert_equal(
        result.action,
        guard.ACTION.HOLD_NONESSENTIAL_WORKLOADS,
        "low telemetry action"
    )
    assert_equal(result.reason, guard.REASON.TELEMETRY_QUALITY_LOW, "low telemetry reason")
end)

test("holds when battery health is not approved", function()
    local context = valid_context()
    context.battery_health_status = "maintenance_inspection_required"
    local result = guard.evaluate(context)
    assert_equal(
        result.action,
        guard.ACTION.HOLD_NONESSENTIAL_WORKLOADS,
        "battery health action"
    )
    assert_equal(
        result.reason,
        guard.REASON.BATTERY_HEALTH_NOT_APPROVED,
        "battery health reason"
    )
end)

test("holds when voltage margin is insufficient", function()
    local context = valid_context()
    context.predicted_voltage_v = 47.4
    local result = guard.evaluate(context)
    assert_equal(
        result.action,
        guard.ACTION.HOLD_NONESSENTIAL_WORKLOADS,
        "voltage action"
    )
    assert_equal(
        result.reason,
        guard.REASON.VOLTAGE_MARGIN_INSUFFICIENT,
        "voltage reason"
    )
end)

test("holds when state of charge is below critical reserve", function()
    local context = valid_context()
    context.battery_soc = 0.44
    local result = guard.evaluate(context)
    assert_equal(
        result.action,
        guard.ACTION.HOLD_NONESSENTIAL_WORKLOADS,
        "battery reserve action"
    )
    assert_equal(
        result.reason,
        guard.REASON.BATTERY_RESERVE_INSUFFICIENT,
        "battery reserve reason"
    )
end)

test("holds while water operations are not normal", function()
    local context = valid_context()
    context.water_operations_status = "flood_response"
    local result = guard.evaluate(context)
    assert_equal(
        result.action,
        guard.ACTION.HOLD_NONESSENTIAL_WORKLOADS,
        "water operation action"
    )
    assert_equal(
        result.reason,
        guard.REASON.WATER_OPERATIONS_NOT_NORMAL,
        "water operation reason"
    )
end)

test("holds during an emergency", function()
    local context = valid_context()
    context.emergency_status = "active"
    local result = guard.evaluate(context)
    assert_equal(
        result.action,
        guard.ACTION.HOLD_NONESSENTIAL_WORKLOADS,
        "emergency action"
    )
    assert_equal(result.reason, guard.REASON.EMERGENCY_ACTIVE, "emergency reason")
end)

test("rejects an essential task", function()
    local context = valid_context()
    context.proposed_task_class = "essential_aeration"
    local result = guard.evaluate(context)
    assert_equal(
        result.action,
        guard.ACTION.REJECT_RUNTIME_ADJUSTMENT,
        "essential workload action"
    )
    assert_equal(
        result.reason,
        guard.REASON.TASK_NOT_FLEXIBLE,
        "essential workload reason"
    )
end)

test("rejects when operator approval is missing", function()
    local context = valid_context()
    context.operator_approval_status = "pending"
    local result = guard.evaluate(context)
    assert_equal(
        result.action,
        guard.ACTION.REJECT_RUNTIME_ADJUSTMENT,
        "missing approval action"
    )
    assert_equal(
        result.reason,
        guard.REASON.OPERATOR_APPROVAL_MISSING,
        "missing approval reason"
    )
end)

test("holds when requested energy exceeds safe flexible capacity", function()
    local context = valid_context()
    context.requested_energy_j = 300000.0
    local result = guard.evaluate(context)
    assert_equal(
        result.action,
        guard.ACTION.HOLD_NONESSENTIAL_WORKLOADS,
        "capacity action"
    )
    assert_equal(
        result.reason,
        guard.REASON.NO_SCHEDULING_CAPACITY,
        "capacity reason"
    )
end)

test("rejects malformed input", function()
    local result = guard.evaluate({
        workload_id = "missing-critical-fields"
    })
    assert_equal(
        result.action,
        guard.ACTION.REJECT_RUNTIME_ADJUSTMENT,
        "malformed input action"
    )
    assert_equal(result.reason, guard.REASON.INVALID_CONTEXT, "malformed input reason")
end)

io.write(string.format("Tests complete: %d passed, %d failed\n", passed, failed))

if failed > 0 then
    os.exit(1)
end
