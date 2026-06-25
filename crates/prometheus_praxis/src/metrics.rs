use once_cell::sync::Lazy;
use prometheus::{Gauge, IntCounter};

pub static DECODER_ACCURACY: Lazy<Gauge> = Lazy::new(|| {
    Gauge::new("prometheus_praxis_decoder_accuracy", "DecoderAccuracy 0-1").unwrap()
});

pub static ECO_IMPACT_SCORE: Lazy<Gauge> = Lazy::new(|| {
    Gauge::new("prometheus_praxis_eco_impact_score", "EcoImpactScore 0-1").unwrap()
});

pub static AVG_DAILY_DEVICE_HOURS_REDUCED: Lazy<Gauge> = Lazy::new(|| {
    Gauge::new(
        "prometheus_praxis_avg_daily_device_hours_reduced",
        "Normalized AvgDailyDeviceHoursReduced 0-1",
    )
    .unwrap()
});

pub static FATIGUE_INDEX: Lazy<Gauge> = Lazy::new(|| {
    Gauge::new("prometheus_praxis_fatigue_index", "FatigueIndex 0-1").unwrap()
});

pub static SAFETY_INVARIANT_VIOLATION_COUNT: Lazy<IntCounter> = Lazy::new(|| {
    IntCounter::new(
        "prometheus_praxis_safety_invariant_violation_count",
        "Count of safety invariant violations detected by guards",
    )
    .unwrap()
});

pub static EQUALITY_VIOLATION_COUNT: Lazy<IntCounter> = Lazy::new(|| {
    IntCounter::new(
        "prometheus_praxis_equality_violation_count",
        "Count of equality/non-exclusion violations",
    )
    .unwrap()
});
