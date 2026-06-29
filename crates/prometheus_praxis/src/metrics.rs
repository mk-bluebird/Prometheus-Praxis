//! Prometheus metrics for monitoring Prometheus-Praxis execution and safety.
//!
//! This module centralizes all Prometheus metric definitions for the
//! Prometheus-Praxis and Perkunos-Nexus ecosafety stack. Metrics are exposed
//! conditionally under the `metrics` feature to keep non-metric builds lean
//! and to avoid duplicated static registrations.

#![forbid(unsafe_code)]

#[cfg(feature = "metrics")]
use once_cell::sync::Lazy;
#[cfg(feature = "metrics")]
use prometheus::{Gauge, IntCounter};

/// Decoder accuracy for neuromorphic decoders, normalized to [0.0, 1.0].
///
/// Intended wiring:
/// - Updated by BCI / decoder subsystems when model accuracy changes.
/// - Used by ecosafety guards and KER kernels as an observability coordinate
///   for cognitive load and decoder reliability.
#[cfg(feature = "metrics")]
pub static DECODER_ACCURACY: Lazy<Gauge> = Lazy::new(|| {
    Gauge::new(
        "prometheus_praxis_decoder_accuracy",
        "Neuromorphic decoder accuracy 0-1",
    )
    .expect("prometheus_praxis_decoder_accuracy gauge must register")
});

/// Eco-impact score, normalized to [0.0, 1.0].
///
/// Intended wiring:
/// - Aggregates CEIM-style load reduction kernels and corridor-aligned
///   eco-impact metrics (PFAS, PFBS, E. coli, heat, recharge).
/// - Serves as the scalar EcoImpactScore used in KER triads and Lyapunov
///   residuals for city-scale Prometheus-Praxis controllers.
#[cfg(feature = "metrics")]
pub static ECO_IMPACT_SCORE: Lazy<Gauge> = Lazy::new(|| {
    Gauge::new(
        "prometheus_praxis_eco_impact_score",
        "Normalized EcoImpactScore 0-1",
    )
    .expect("prometheus_praxis_eco_impact_score gauge must register")
});

/// Average daily device hours reduced, normalized to [0.0, 1.0].
///
/// Intended wiring:
/// - Tracks reduction in device/implant usage time per host due to
///   ecosafety or neuromorphic optimizations.
/// - Acts as an eco-health and mental-privacy indicator for Prometheus-Praxis
///   deployments, feeding into host-level KER scoring.
#[cfg(feature = "metrics")]
pub static AVG_DAILY_DEVICE_HOURS_REDUCED: Lazy<Gauge> = Lazy::new(|| {
    Gauge::new(
        "prometheus_praxis_avg_daily_device_hours_reduced",
        "Normalized AvgDailyDeviceHoursReduced 0-1",
    )
    .expect("prometheus_praxis_avg_daily_device_hours_reduced gauge must register")
});

/// Host fatigue index, normalized to [0.0, 1.0].
///
/// Intended wiring:
/// - Computed from vitals, engagement, and rehabilitation load.
/// - Used by in-body Perkunos-Nexus controllers to derate nanoswarm and
///   neuromodulation actuation when host fatigue approaches corridor ceilings.
#[cfg(feature = "metrics")]
pub static FATIGUE_INDEX: Lazy<Gauge> = Lazy::new(|| {
    Gauge::new(
        "prometheus_praxis_fatigue_index",
        "Host fatigue index 0-1",
    )
    .expect("prometheus_praxis_fatigue_index gauge must register")
});

/// Count of safety invariant violations detected by guards.
///
/// Intended wiring:
/// - Incremented by Lyapunov, corridor, and RoH guards whenever an action
///   proposal violates non-offsettable ecosafety or neurorights invariants.
/// - Surfaces into Prometheus-Praxis and Perkunos-Nexus telemetry for
///   CI, governance, and Veritas-Chain audit trails.
#[cfg(feature = "metrics")]
pub static SAFETY_INVARIANT_VIOLATION_COUNT: Lazy<IntCounter> = Lazy::new(|| {
    IntCounter::new(
        "prometheus_praxis_safety_invariant_violation_count",
        "Count of safety invariant violations detected by guards",
    )
    .expect("prometheus_praxis_safety_invariant_violation_count counter must register")
});

/// Count of equality / non-exclusion violations.
///
/// Intended wiring:
/// - Incremented when any controller or policy breaches equality,
///   non-exclusion, or representation-floor corridors (e.g., bias against
///   certain hosts, regions, or corridor groups).
/// - Feeds fairness and ecosafety planes, ensuring that augmented citizens
///   and regions are not excluded from basic services or protections.
#[cfg(feature = "metrics")]
pub static EQUALITY_VIOLATION_COUNT: Lazy<IntCounter> = Lazy::new(|| {
    IntCounter::new(
        "prometheus_praxis_equality_violation_count",
        "Count of equality/non-exclusion violations",
    )
    .expect("prometheus_praxis_equality_violation_count counter must register")
});
