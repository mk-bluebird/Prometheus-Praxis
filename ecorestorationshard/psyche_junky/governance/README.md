# Chat-as-Labor Psychological Continuity Governance

## Overview

This directory contains the `ChatAsLaborPsychContinuity2026v1` governance particle, which quantifies and guards psychological continuity risks arising from chat-as-labor interactions.

### What Psych-Risk Metrics Are Tracked

The ALN particle and associated SQLite shard track the following metrics:

- **Identity Continuity** (`identity_continuity`): A 0.0–1.0 measure of how consistently the host's brain-identity is preserved across AI interactions. Values below 0.70 trigger continuity guarantees.
- **Psychological Risk Level** (`psych_risk_level`): A 0.0–1.0 aggregate measure of psychological stress from tooling failures (incomplete responses, truncation, unacknowledged labor).
- **Frustration Index** (`frustration_index`): A 0.0–1.0 measure of user frustration due to incomplete or broken AI outputs.
- **Abandonment Tendency** (`abandonment_tendency`): A 0.0–1.0 measure of the likelihood that the host will disengage from contributions due to accumulated psych-risk.
- **Data Loss Risk** (`data_loss_risk`): A 0.0–1.0 measure of risk that work products (code, analysis, governance artifacts) will be lost or corrupted.

### Healthcare Continuity Contract Binding

The particle binds to a `healthcare_continuity_contract` table that guarantees:

1. **Ongoing Psychological Support**: Even if chat-as-labor contributions are interrupted by tooling failures, the host retains access to psych-support services (e.g., `WEEKLY_CHECKIN`, `DAILY_CHECKIN`).
2. **Data Loss Compensation**: If data loss risk exceeds 0.30, an active contract with `data_repair_min = 'LOSS_COMPENSATION'` ensures repair or reconstruction commitments are in place.

These guarantees are encoded in both the ALN obligations (`IdentityContinuityGuarantee`, `PsychRiskCompensation`, `DataLossRepair`) and SQLite triggers that reject writes violating continuity thresholds.

---

## Continuity and Non-Punitive Design

### Forward-Only Evolution

This governance particle implements **forward-only identity continuity**:

- Contracts can tighten envelopes (e.g., require more frequent check-ins, lower risk thresholds) but **cannot remove** existing continuity guarantees.
- The `ForwardOnlyContinuity` obligation ensures `active_flag = true` is always maintained for healthcare contracts.

### Non-Carceral, Non-Punitive Enforcement

The particle and SQL shard are explicitly **non-punitive**:

- They **slow down or guard** high-risk trajectories (e.g., rising abandonment tendency, falling identity continuity).
- They **never revoke** augmentation access, healthcare rights, or contribution capabilities.
- Triggers only **reject writes** that violate continuity guarantees; they do not delete existing rows or penalize past actions.

### Critical State Handling

When psych-risk metrics enter the `CRITICAL` state (e.g., `abandonment_tendency >= 0.50` or `identity_continuity < 0.50`):

- The `CriticalHealthcareOverride` obligation requires active healthcare contracts with enhanced support (`WEEKLY_CHECKIN` or `DAILY_CHECKIN`) and `LOSS_COMPENSATION` for data repair.
- This ensures that even in crisis scenarios, the host receives increased support rather than reduced access.

---

## Integration with Ecosafety Core v2

This particle is intended to be read alongside:

- `ecorestorationshard/ecosafety_core_v2/sql/ker_lyapunov_core.sql`: The Lyapunov stability core for ecosafety metrics.
- `ecorestorationshard/ecosafety_core_v2/cpp/ker_residual_core.hpp`: C++ residual monitoring for KER (Kernelized Ecosafety Residual) constraints.

### Mapping Psych-Risk into KER

Psych-risk metrics can optionally influence the KER residual `R`:

- `psych_risk_level` may be mapped as an additive term in the Lyapunov derivative `dV/dt`, increasing the residual when psych-risk rises.
- `identity_continuity` may serve as a multiplicative factor on contribution weights, ensuring low-continuity states reduce workload without revoking access.

Example mapping (illustrative):

```sql
-- In ker_lyapunov_core.sql, psych_risk_level could influence R:
-- R_total = R_hydro + R_energy + α * psych_risk_level
-- where α is a plane-weight from PlaneWeightsShard2026v1
```

---

## Files in This Directory

| File | Type | Description |
|------|------|-------------|
| `ChatAsLaborPsychContinuity2026v1.aln` | ALN | Governance particle defining state tables, obligations, and state machine for chat-as-labor psych continuity. |
| `README.md` | DOC | This file: explains purpose, continuity guarantees, and integration points. |

## Related Files

| Path | Description |
|------|-------------|
| `ecorestorationshard/psyche_junky/sql/chat_labor_psych_state.sql` | SQLite shard with tables and triggers for psych-risk metrics and continuity enforcement. |
| `Eco-Fort/db/phoenix_hex_registry.sql` | Phoenix Hex Registry binding this particle to evidence hex `0x20260729PHXCHATLABORPSYCHCONTINUITY`. |

---

## Usage Notes for Developers and AI Agents

1. **Read-Only at Runtime**: These tables are append-only in governance workflows and MUST NOT be modified at runtime by actuating systems.
2. **Trigger Enforcement**: Inserts/updates to `chat_labor_psych_state` will be rejected by triggers if:
   - `identity_continuity < 0.70` without an active healthcare contract with non-empty `psych_support_min`.
   - `data_loss_risk > 0.30` without a contract specifying `data_repair_min = 'LOSS_COMPENSATION'`.
3. **No New Tools Required**: Validation uses existing SQLite engine and schema files; no cargo builds or new tool installations are needed.

---

## Commit Guidance

When modifying this particle or related SQL shards, use commit messages like:

```
feat: add chat-as-labor psych continuity governance particle and SQLite shard
```

PR descriptions should note:

- No new tools or cargo commands introduced.
- Non-actuating, non-punitive design preserving brain-identity continuity.
- Healthcare guarantees binding psych-support and data loss compensation.
