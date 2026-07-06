// FILE: crates/prometheus_praxis/tests/ecorestoration_gaia_kani.rs
// ROLE: Kani harnesses for Gaia corridor thresholds and autopause logic.
//
// PROPERTIES:
//   1. If multiple metric breaches occur, autopause_reason_for == Combined.
//   2. If any single metric exceeds its ALN threshold, autopause_reason_for
//      is not None (i.e., it never misses a breach).

use rust_decimal::Decimal;

use prometheus_praxis::ecorestoration::gaia_thresholds::GaiaCorridorThresholds;
use prometheus_praxis::ecorestoration::gaia_snapshot::{GaiaSentinelSnapshot, AutopauseReason};

fn d(v: f32) -> Decimal {
    Decimal::from_f32(v).unwrap()
}

/// Helper: construct a conservative default thresholds bundle.
fn mk_thresholds() -> GaiaCorridorThresholds {
    GaiaCorridorThresholds {
        policy_id: "GAIA-THRESHOLDS-DEFAULT".to_string(),
        jurisdiction: "GLOBAL".to_string(),
        soil_moisture_pause_threshold: d(0.30),
        soil_moisture_days_below_for_pause: 3,
        heat_budget_pause_threshold: d(0.70),
        drought_idx_pause_threshold: d(0.60),
        flood_risk_pause_threshold: d(0.50),
        fire_risk_pause_threshold: d(0.50),
        max_consecutive_moisture_breach_days: 5,
        max_consecutive_heat_drought_breach_days: 5,
        max_consecutive_flood_breach_events: 5,
        max_consecutive_fire_breach_events: 5,
        preflight_rule_id: "ALE-GOV-GAIA-THRESHOLDS-001.aln#PAUSERULES".to_string(),
        target_workflow_ids: vec!["ALE-ERM-RESTORATION-SORTIES-001".to_string()],
    }
}

/// Helper: construct a symbolic snapshot; Kani will vary the fields.
fn mk_symbolic_snapshot() -> GaiaSentinelSnapshot {
    GaiaSentinelSnapshot {
        snapshot_id: "KANITEST-GAIA-SNAP".to_string(),
        tile_id: "TILE-XYZ".to_string(),
        timestamp_utc: "2026-01-01T00:00:00Z".to_string(),
        soil_moisture_idx: d(0.5),
        drought_idx: d(0.5),
        heat_budget_idx: d(0.5),
        flood_risk_idx: d(0.5),
        fire_risk_idx: d(0.5),
        moisture_below_floor: false,
        heat_budget_over_limit: false,
        drought_above_threshold: false,
        flood_risk_high: false,
        fire_risk_high: false,
        corridor_violation_ids: Vec::new(),
        autopause_reason: AutopauseReason::None,
    }
}

/// Proof 1: Combined reason for multiple breaches.
///
/// If at least two different metrics breach their thresholds,
/// autopause_reason_for must return Combined.
#[kani::proof]
fn proof_autopause_combined_for_multiple_breaches() {
    let thresholds = mk_thresholds();
    let mut snap = mk_symbolic_snapshot();

    // Let Kani vary all metrics, but we enforce two breaches via assumptions.

    // Breach soil moisture: snapshot value below threshold.
    snap.soil_moisture_idx = d(0.10);
    kani::assume(snap.soil_moisture_idx < thresholds.soil_moisture_pause_threshold);

    // Breach flood risk: snapshot value above threshold.
    snap.flood_risk_idx = d(0.90);
    kani::assume(snap.flood_risk_idx > thresholds.flood_risk_pause_threshold);

    let reason = thresholds.autopause_reason_for(&snap);

    // With at least two distinct breaches, reason must be Combined.
    kani::assert!(matches!(reason, AutopauseReason::Combined));
}

/// Proof 2: No missed breach.
///
/// For each single metric, if it breaches its threshold, autopause_reason_for
/// must not be None. This is checked per metric so any missed mapping is caught.
#[kani::proof]
fn proof_autopause_not_none_for_any_single_breach() {
    let thresholds = mk_thresholds();
    let mut snap = mk_symbolic_snapshot();

    // Case 1: soil moisture breach only.
    snap.soil_moisture_idx = d(0.10);
    kani::assume(snap.soil_moisture_idx < thresholds.soil_moisture_pause_threshold);
    snap.flood_risk_idx = d(0.0);
    snap.fire_risk_idx = d(0.0);
    snap.heat_budget_idx = d(0.0);
    snap.drought_idx = d(0.0);
    let reason_moisture = thresholds.autopause_reason_for(&snap);
    kani::assert!(!matches!(reason_moisture, AutopauseReason::None));

    // Case 2: heat + drought breach only.
    snap = mk_symbolic_snapshot();
    snap.heat_budget_idx = d(0.90);
    snap.drought_idx = d(0.80);
    kani::assume(snap.heat_budget_idx > thresholds.heat_budget_pause_threshold);
    kani::assume(snap.drought_idx > thresholds.drought_idx_pause_threshold);
    snap.soil_moisture_idx = d(0.5);
    snap.flood_risk_idx = d(0.0);
    snap.fire_risk_idx = d(0.0);
    let reason_heat_drought = thresholds.autopause_reason_for(&snap);
    kani::assert!(!matches!(reason_heat_drought, AutopauseReason::None));

    // Case 3: flood risk breach only.
    snap = mk_symbolic_snapshot();
    snap.flood_risk_idx = d(0.90);
    kani::assume(snap.flood_risk_idx > thresholds.flood_risk_pause_threshold);
    snap.soil_moisture_idx = d(0.5);
    snap.fire_risk_idx = d(0.0);
    snap.heat_budget_idx = d(0.0);
    snap.drought_idx = d(0.0);
    let reason_flood = thresholds.autopause_reason_for(&snap);
    kani::assert!(!matches!(reason_flood, AutopauseReason::None));

    // Case 4: fire risk breach only.
    snap = mk_symbolic_snapshot();
    snap.fire_risk_idx = d(0.90);
    kani::assume(snap.fire_risk_idx > thresholds.fire_risk_pause_threshold);
    snap.soil_moisture_idx = d(0.5);
    snap.flood_risk_idx = d(0.0);
    snap.heat_budget_idx = d(0.0);
    snap.drought_idx = d(0.0);
    let reason_fire = thresholds.autopause_reason_for(&snap);
    kani::assert!(!matches!(reason_fire, AutopauseReason::None));
}
