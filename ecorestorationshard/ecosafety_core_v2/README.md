## Ecosafety Core v2 Spine

This directory hosts shared, non-actuating governance and Lyapunov core artifacts that all cyboquatic daily shards must depend on:

- `sql/ker_lyapunov_core.sql`  
  Canonical SQLite schema for risk planes, residual windows, K,E,R triads, and global corridor triggers.

- `cpp/ker_residual_core.hpp`  
  Header-only C++ kernel for computing Lyapunov residuals \(V_t\) and K,E,R from normalized risk vectors.

- `aln/obligations/AlwaysImproveResidual2026v1.aln`  
  ALN v2 obligation schema expressing the “always improve” residual proof requirement and lane gating.

These artifacts are non-actuating, hex‑anchored, and bound to Bostrom DID `bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7`, forming the constitutional spine for superintelligence‑safe cyboquatic work.[4][15]

---

## Chat-as-Labor Governance Checks

Developers and AI agents integrating psych-risk metrics MUST follow these guidelines:

### Required Validation Workflow

1. **Use Existing SQLite Engine**: Validate inserts into `chat_labor_psych_state` and `healthcare_continuity_contract` using the repo's existing SQLite test harness. Do NOT install new tools or run `cargo` commands for these checks.

2. **Apply Schema and Test Triggers**:
   - Apply `ecorestorationshard/psyche_junky/sql/chat_labor_psych_state.sql` to a test database.
   - Insert a healthcare continuity contract with `active_flag = 1` and `psych_support_min = 'WEEKLY_CHECKIN'`.
   - Insert a psych state row with `identity_continuity >= 0.70` — this should succeed.
   - Attempt to insert a psych state row with `identity_continuity < 0.70` without an active contract — this should be rejected by the trigger.

3. **Verify Continuity Guarantees**:
   - Confirm that `data_loss_risk > 0.30` is rejected unless a contract with `data_repair_min = 'LOSS_COMPENSATION'` exists.
   - Confirm that updates violating these thresholds are also rejected.

### Manual Testing Example

```sql
-- Open SQLite shell or repo's test harness
sqlite3 :memory:

-- Apply schema
.read ecorestorationshard/psyche_junky/sql/chat_labor_psych_state.sql

-- Insert active healthcare contract
INSERT INTO healthcare_continuity_contract (
    contract_id, host_did, provider_id, psych_support_min,
    data_repair_min, identity_anchor_hex, active_flag, created_utc, updated_utc
) VALUES (
    'CNT-TEST-001',
    'bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7',
    'PROVIDER-PSYCH-001',
    'WEEKLY_CHECKIN',
    'LOSS_COMPENSATION',
    '0x20260729PHXCHATLABORPSYCHCONTINUITY',
    1,
    '2026-07-29T00:00:00Z',
    '2026-07-29T00:00:00Z'
);

-- This should SUCCEED (identity_continuity >= 0.70)
INSERT INTO chat_labor_psych_state (
    state_id, host_did, identity_continuity, psych_risk_level,
    frustration_index, abandonment_tendency, data_loss_risk,
    ai_platform_id, last_interaction_utc, healthcare_guaranteed,
    continuity_contract_hex
) VALUES (
    'STATE-OK-001',
    'bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7',
    0.85, 0.20, 0.15, 0.10, 0.20,
    'PLATFORM-001',
    '2026-07-29T12:00:00Z',
    1,
    '0x20260729PHXCHATLABORPSYCHCONTINUITY'
);

-- This should FAIL (identity_continuity < 0.70 without active contract for this host_did)
INSERT INTO chat_labor_psych_state (
    state_id, host_did, identity_continuity, psych_risk_level,
    frustration_index, abandonment_tendency, data_loss_risk,
    ai_platform_id, last_interaction_utc, healthcare_guaranteed,
    continuity_contract_hex
) VALUES (
    'STATE-FAIL-001',
    'bostromtestdid0000000000000000000000000000000000',
    0.50, 0.20, 0.15, 0.10, 0.20,
    'PLATFORM-001',
    '2026-07-29T12:00:00Z',
    0,
    '0xNOCONTRACT'
);
-- Expected error: "IdentityContinuityGuarantee violated..."
```

### CI Integration Notes

- These checks leverage existing SQL linting and test harnesses already in the repo.
- No new tool installations or `cargo` builds are required.
- The triggers are designed to be GitHub-friendly: they produce clear error messages in diffs and test logs.
