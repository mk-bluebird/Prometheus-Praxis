// FILE: crates/prometheus_praxis/tests/ecorestoration_gaia_kani.rs
// ROLE: Kani harnesses for Gaia corridor thresholds, autopause logic,
//       and consecutive-breach preflight blocking.

use rust_decimal::Decimal;

use prometheus_praxis::ecorestoration::gaia_thresholds::GaiaCorridorThresholds;
use prometheus_praxis::ecorestoration::gaia_snapshot::{GaiaSentinelSnapshot, AutopauseReason};

fn d(v: f32) -> Decimal {
    Decimal::from_f32(v).unwrap()
}

/// Simple breach-counter model for a tile.
/// This stays in tests to avoid polluting core types.
#[derive(Clone, Copy)]
struct GaiaBreachCounters {
    pub moisture_breach_days: i32,
    pub heat_drought_breach_days: i32,
    pub flood_breach_events: i32,
    pub fire_breach_events: i32,
}

/// Preflight guard: should sorties be blocked for this tile,
/// given thresholds and current breach counters?
///
/// Blocking rule:
/// - If any counter > corresponding max_consecutive_* field, block (return true).
/// - Otherwise, allow (return false).
fn preflight_block_for_counters(
    thresholds: &GaiaCorridorThresholds,
    counters: &GaiaBreachCounters,
) -> bool {
    counters.moisture_breach_days > thresholds.max_consecutive_moisture_breach_days
        || counters.heat_drought_breach_days > thresholds.max_consecutive_heat_drought_breach_days
        || counters.flood_breach_events > thresholds.max_consecutive_flood_breach_events
        || counters.fire_breach_events > thresholds.max_consecutive_fire_breach_events
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

    snap.soil_moisture_idx = d(0.10);
    kani::assume(snap.soil_moisture_idx < thresholds.soil_moisture_pause_threshold);

    snap.flood_risk_idx = d(0.90);
    kani::assume(snap.flood_risk_idx > thresholds.flood_risk_pause_threshold);

    let reason = thresholds.autopause_reason_for(&snap);
    kani::assert!(matches!(reason, AutopauseReason::Combined));
}

/// Proof 2: No missed breach.
///
/// For each single metric, if it breaches its threshold, autopause_reason_for
/// must not be None.
#[kani::proof]
fn proof_autopause_not_none_for_any_single_breach() {
    let thresholds = mk_thresholds();
    let mut snap = mk_symbolic_snapshot();

    // Soil moisture breach only.
    snap.soil_moisture_idx = d(0.10);
    kani::assume(snap.soil_moisture_idx < thresholds.soil_moisture_pause_threshold);
    snap.flood_risk_idx = d(0.0);
    snap.fire_risk_idx = d(0.0);
    snap.heat_budget_idx = d(0.0);
    snap.drought_idx = d(0.0);
    let reason_moisture = thresholds.autopause_reason_for(&snap);
    kani::assert!(!matches!(reason_moisture, AutopauseReason::None));

    // Heat + drought breach only.
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

    // Flood breach only.
    snap = mk_symbolic_snapshot();
    snap.flood_risk_idx = d(0.90);
    kani::assume(snap.flood_risk_idx > thresholds.flood_risk_pause_threshold);
    snap.soil_moisture_idx = d(0.5);
    snap.fire_risk_idx = d(0.0);
    snap.heat_budget_idx = d(0.0);
    snap.drought_idx = d(0.0);
    let reason_flood = thresholds.autopause_reason_for(&snap);
    kani::assert!(!matches!(reason_flood, AutopauseReason::None));

    // Fire breach only.
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

/// Proof 3: Once counters exceed limits, preflight must block.
///
/// If any counter is strictly greater than its configured maximum,
/// preflight_block_for_counters must return true (block sorties).
#[kani::proof]
fn proof_preflight_block_when_counters_exceed_limits() {
    let thresholds = mk_thresholds();

    // Symbolic counters; Kani will vary them.
    let counters = GaiaBreachCounters {
        moisture_breach_days: kani::any(),
        heat_drought_breach_days: kani::any(),
        flood_breach_events: kani::any(),
        fire_breach_events: kani::any(),
    };

    // Assume at least one counter exceeds its corresponding maximum.
    let over_moisture =
        counters.moisture_breach_days > thresholds.max_consecutive_moisture_breach_days;
    let over_heat_drought =
        counters.heat_drought_breach_days > thresholds.max_consecutive_heat_drought_breach_days;
    let over_flood =
        counters.flood_breach_events > thresholds.max_consecutive_flood_breach_events;
    let over_fire =
        counters.fire_breach_events > thresholds.max_consecutive_fire_breach_events;

    kani::assume(over_moisture || over_heat_drought || over_flood || over_fire);

    let block = preflight_block_for_counters(&thresholds, &counters);
    kani::assert!(block);
}

/// Proof 4: When all counters are within limits, preflight must allow.
///
/// If all counters are <= their configured maximums,
/// preflight_block_for_counters must return false (no block).
#[kani::proof]
fn proof_preflight_allow_when_counters_within_limits() {
    let thresholds = mk_thresholds();

    let counters = GaiaBreachCounters {
        moisture_breach_days: kani::any(),
        heat_drought_breach_days: kani::any(),
        flood_breach_events: kani::any(),
        fire_breach_events: kani::any(),
    };

    kani::assume(counters.moisture_breach_days <= thresholds.max_consecutive_moisture_breach_days);
    kani::assume(
        counters.heat_drought_breach_days
            <= thresholds.max_consecutive_heat_drought_breach_days,
    );
    kani::assume(counters.flood_breach_events <= thresholds.max_consecutive_flood_breach_events);
    kani::assume(counters.fire_breach_events <= thresholds.max_consecutive_fire_breach_events);

    let block = preflight_block_for_counters(&thresholds, &counters);
    kani::assert!(!block);
}
