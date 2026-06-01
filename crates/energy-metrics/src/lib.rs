//! Energy Metrics and Carbon Intensity Analysis
//!
//! Provides tools for computing energy costs, carbon offsets, and optimal
//! execution timing for eco-restoration operations in Phoenix, AZ.

#![forbid(unsafe_code)]
#![deny(warnings)]

pub mod carbon_intensity;
pub mod dashboard;

pub use carbon_intensity::{
    phoenix_carbon_intensity, compute_carbon_offset, optimal_execution_window,
    energy_optimization_suggestion, get_daily_carbon_curve,
    estimate_daily_carbon_savings, CarbonIntensity, EnergyMetrics, SourceMix,
    PHOENIX_BASELINE_CI, CARBON_BUDGET_PER_CYCLE_KG, JOULES_PER_KWH,
};

pub use dashboard::{EnergyDashboard, DashboardConfig};
