# Flexible Workload Runtime Guard

## Purpose

This Lua 5.4 module performs a narrow runtime safety check for a proposed
nonessential, deferrable compute workload at a canal-node microgrid.

It may only return one of three reversible, non-actuating decisions:

- `hold_nonessential_workloads`
- `reject_runtime_adjustment`
- `queue_for_operator_approved_schedule`

The module does not operate pumps, gates, aerators, water-treatment equipment,
safety interlocks, public heat-relief equipment, lighting, sanitation,
communications, or other public-service loads.

## Safety conditions

A workload is eligible only when all of the following are true:

1. Telemetry quality is at least 0.95.
2. Battery health status is `within_reference_envelope`.
3. Predicted voltage is at or above the approved safe minimum.
4. Battery state of charge is at or above the critical reserve.
5. Water operations status is `normal`.
6. Emergency status is `normal`.
7. The task is exactly `nonessential_deferrable`.
8. A qualified operator has marked the task `approved`.
9. Requested energy is positive and within available flexible energy.
10. The proposed release/deadline window is valid.

Any failed safety condition returns hold or reject. The caller must treat these
results as advisory scheduling controls, not physical commands.

## Example

```lua
local guard = require("lua.cyboquatic.flexible_workload_runtime_guard")

local decision = guard.evaluate({
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
})

print(decision.action)
print(decision.reason)
```

## Run tests

From the repository root:

```text
lua tests/lua/cyboquatic/test_flexible_workload_runtime_guard.lua
```

The runtime environment must make the repository root available through
`package.path`, for example:

```text
LUA_PATH="./?.lua;./?/init.lua;;" lua tests/lua/cyboquatic/test_flexible_workload_runtime_guard.lua
```

## Integration boundary

An integration adapter may serialize an accepted queue request to an approved
operator workflow. It must not interpret a queue decision as authority to
switch physical infrastructure. Physical actions remain subject to independent
operator authorization, equipment interlocks, water-operations procedures, and
emergency controls.
