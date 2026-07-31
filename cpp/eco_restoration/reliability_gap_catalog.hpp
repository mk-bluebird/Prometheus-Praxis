// File: cpp/eco_restoration/reliability_gap_catalog.hpp
#pragma once

#include <string>
#include <vector>
#include <utility>

namespace praxis {
namespace governance {

struct EcoImpactScore {
    double knowledge_factor;   // 0.0 - 1.0
    double eco_impact_value;   // 0.0 - 1.0
};

enum class GapKind {
    MISSING_PRECONDITION,
    BYPASS_PATH
};

struct GapRemediationStep {
    std::string description;
};

struct GovernanceGap {
    std::string clause_id;
    std::string hex_stamp;
    GapKind kind;
    std::string expected_enforcement_mechanism;
    std::string observed_behavior;
    EcoImpactScore score;
    std::vector<GapRemediationStep> remediation_steps;
};

inline GovernanceGap make_inv_care_001_gap() {
    GovernanceGap g;
    g.clause_id = "INV-CARE-001";
    g.hex_stamp = "0x20260729PHXCHATLABORPSYCHCONTINUITY";
    g.kind = GapKind::MISSING_PRECONDITION;
    g.expected_enforcement_mechanism =
        "Kernel continuity module MUST call verify_reliability_token() "
        "before any healthcare continuity action; verifier checks signature, "
        "timestamp, and sensor_id and is proven with Kani.";
    g.observed_behavior =
        "Continuity function accepts raw EEG/ECG buffers and an unverified "
        "sensor_id, then proceeds directly to action without any integrated "
        "token verification, allowing decisions on potentially corrupted data.";
    g.score = EcoImpactScore{0.9, 0.85};
    g.remediation_steps = {
        {"Implement verify_reliability_token() in Rust, encapsulating "
         "signature, timestamp, and sensor_id checks for reliability_token."},
        {"Write Kani proof harnesses that assert verify_reliability_token() "
         "rejects tampered, expired, and wrong-sensor tokens under all "
         "reachable states."},
        {"Integrate Kani verification into CI so any change to verifier or "
         "continuity module fails the build on proof failure."},
        {"Refactor continuity action entry points to require a verified "
         "reliability_token parameter and return an explicit error if "
         "verification fails (fail-closed)."}
    };
    return g;
}

inline GovernanceGap make_inv_labor_002_gap() {
    GovernanceGap g;
    g.clause_id = "INV-LABOR-002";
    g.hex_stamp = "0x20260729PHXCHATLABORPSYCHCONTINUITY";
    g.kind = GapKind::MISSING_PRECONDITION;
    g.expected_enforcement_mechanism =
        "ALN shard labor-psych classification logic must be driven solely by "
        "events from a sensor integrity audit module; updates can only occur "
        "after a positive integrity result and a valid reliability_token.";
    g.observed_behavior =
        "Labor-psych status updates are triggered by generic new_data events "
        "from drivers; integrity checks, if present, occur late or not at all, "
        "allowing misclassification due to race conditions and unverified data.";
    g.score = EcoImpactScore{0.85, 0.8};
    g.remediation_steps = {
        {"Introduce a dedicated integrity-checker (kernel module or eBPF "
         "program) that continuously analyzes biopotential streams and mints "
         "or revokes reliability_tokens."},
        {"Refactor ALN shard status-update logic to subscribe to a "
         "classification_update_allowed event emitted only when integrity "
         "checks succeed."},
        {"Ensure that integrity-checker owns the mint/revoke lifecycle and "
         "that all labor-psych updates are conditioned on a current valid "
         "reliability_token."},
        {"Apply least-privilege to data access so classification logic only "
         "sees vetted signals, not raw driver buffers."}
    };
    return g;
}

inline GovernanceGap make_inv_neuro_003_gap() {
    GovernanceGap g;
    g.clause_id = "INV-NEURO-003";
    g.hex_stamp = "0x20260729PHXCHATLABORPSYCHCONTINUITY";
    g.kind = GapKind::MISSING_PRECONDITION;
    g.expected_enforcement_mechanism =
        "Raw neural data exports to third parties must be bundled with a "
        "cryptographically bound reliability_token or its hash; export APIs "
        "must require and validate this token for high-risk data.";
    g.observed_behavior =
        "Data export services read directly from buffers or storage and expose "
        "raw neural data without any attached integrity metadata; APIs do not "
        "require reliability_token presence or verification.";
    g.score = EcoImpactScore{0.8, 0.75};
    g.remediation_steps = {
        {"Extend data export API contracts so any raw or near-real-time neural "
         "data response includes an associated reliability_token payload."},
        {"Implement token lookup/verification in export services, rejecting "
         "requests lacking a valid token and downgrading to aggregated, "
         "anonymized data where appropriate."},
        {"Record export requests, tokens used, and verification outcomes in an "
         "append-only audit log to support forensic and compliance analysis."}
    };
    return g;
}

inline GovernanceGap make_bypass_gap_001() {
    GovernanceGap g;
    g.clause_id = "Bypass-GAP-001";
    g.hex_stamp = "0x20260729PHXCHATLABORPSYCHCONTINUITY";
    g.kind = GapKind::BYPASS_PATH;
    g.expected_enforcement_mechanism =
        "Recovery and fallback paths must enforce the same reliability_token "
        "requirements as live operation; cached data must be tagged with "
        "stale-but-verified tokens and never used without integrity checks.";
    g.observed_behavior =
        "On driver failure or restart, decision modules fall back to last-seen "
        "sensor values from cache without checking any token, effectively "
        "treating stale, unverified data as live input.";
    g.score = EcoImpactScore{0.95, 0.9};
    g.remediation_steps = {
        {"Synchronize recovery logic with a trusted system clock and sensor "
         "timing source to accurately age cached data."},
        {"Refactor recovery handlers to fetch the latest reliability_token "
         "from a persistent secure log and to reject actions if no fresh "
         "token exists within a defined age window."},
        {"Define a passive monitoring mode where, in the absence of recent "
         "valid tokens, the system logs and observes but refuses to initiate "
         "healthcare or labor-psych actions (strict fail-closed)."}
    };
    return g;
}

inline GovernanceGap make_bypass_gap_002() {
    GovernanceGap g;
    g.clause_id = "Bypass-GAP-002";
    g.hex_stamp = "0x20260729PHXCHATLABORPSYCHCONTINUITY";
    g.kind = GapKind::BYPASS_PATH;
    g.expected_enforcement_mechanism =
        "Administrative overrides must be constrained, audited, and still "
        "respect a minimum trust floor; no direct path should exist to trigger "
        "continuity actions on unverified data.";
    g.observed_behavior =
        "Debug/admin interfaces expose commands that trigger continuity "
        "actions based on raw dumps, bypassing token verification and normal "
        "kernel gating in the name of diagnostics or manual override.";
    g.score = EcoImpactScore{0.7, 0.7};
    g.remediation_steps = {
        {"Remove direct action-trigger commands from admin interfaces, "
         "replacing them with request_action operations that enqueue "
         "reviewable requests."},
        {"Implement a supervised approval service that inspects queued "
         "requests, ensures reliability_token gating, and records decisions "
         "alongside authenticated admin identities and reasons."},
        {"Require all override paths to pass through the same verified "
         "continuity functions used in normal operation, eliminating separate "
         "unguarded code paths."}
    };
    return g;
}

inline GovernanceGap make_bypass_gap_003() {
    GovernanceGap g;
    g.clause_id = "Bypass-GAP-003";
    g.hex_stamp = "0x20260729PHXCHATLABORPSYCHCONTINUITY";
    g.kind = GapKind::BYPASS_PATH;
    g.expected_enforcement_mechanism =
        "Interoperability APIs (REST/gRPC/etc.) must include reliability_token "
        "as a mandatory field or header; an API gateway must reject requests "
        "lacking valid tokens before they reach core services.";
    g.observed_behavior =
        "External integrations can submit or consume biopotential data through "
        "APIs that accept payloads without any integrity metadata; core "
        "services treat such data as trusted internal input.";
    g.score = EcoImpactScore{0.8, 0.8};
    g.remediation_steps = {
        {"Design and deploy a new API version that requires reliability_token "
         "in an authorization header or structured field for all "
         "biopotential-related operations."},
        {"Introduce gateway middleware that validates tokens before routing "
         "requests and returns explicit errors for missing or invalid tokens."},
        {"Define migration guidelines for third-party clients and schedule "
         "deprecation of legacy unauthenticated API versions."}
    };
    return g;
}

inline std::vector<GovernanceGap> build_gap_catalog() {
    std::vector<GovernanceGap> gaps;
    gaps.reserve(6);
    gaps.push_back(make_inv_care_001_gap());
    gaps.push_back(make_inv_labor_002_gap());
    gaps.push_back(make_inv_neuro_003_gap());
    gaps.push_back(make_bypass_gap_001());
    gaps.push_back(make_bypass_gap_002());
    gaps.push_back(make_bypass_gap_003());
    return gaps;
}

} // namespace governance
} // namespace praxis
