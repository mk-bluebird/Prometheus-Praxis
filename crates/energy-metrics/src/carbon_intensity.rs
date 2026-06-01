//! Carbon Intensity Modeling for Phoenix, AZ Energy Grid
//!
//! Computes time-varying carbon intensity based on Phoenix grid composition,
//! accounting for solar generation peaks and natural gas baseload.

#![forbid(unsafe_code)]
#![deny(warnings)]

use chrono::{DateTime, Timelike, Utc};
use serde::{Deserialize, Serialize};

pub const PHOENIX_BASELINE_CI: f32 = 0.45;
pub const CARBON_BUDGET_PER_CYCLE_KG: f32 = 0.01;
pub const JOULES_PER_KWH: f32 = 3.6e6;
pub const MST_UTC_OFFSET: i32 = -7;

#[derive(Debug, Clone, Copy, Serialize, Deserialize)]
pub struct CarbonIntensity {
    pub kg_co2e_per_kwh: f32,
    pub hour_local: u8,
    pub source_mix: SourceMix,
}

#[derive(Debug, Clone, Copy, Serialize, Deserialize)]
pub struct SourceMix {
    pub solar_fraction: f32,
    pub natural_gas_fraction: f32,
    pub other_fraction: f32,
}

impl SourceMix {
    pub fn validate(&self) -> bool {
        let total = self.solar_fraction + self.natural_gas_fraction + self.other_fraction;
        (total - 1.0).abs() < 1e-3
    }
}

#[derive(Debug, Clone, Copy, Serialize, Deserialize)]
pub struct EnergyMetrics {
    pub joules_per_cycle: f32,
    pub carbon_offset_kg: f32,
    pub carbon_intensity: f32,
    pub timestamp: i64,
}

impl EnergyMetrics {
    pub fn new(joules_per_cycle: f32, timestamp: DateTime<Utc>) -> Self {
        compute_carbon_offset(joules_per_cycle, timestamp)
    }

    pub fn is_eco_violation(&self) -> bool {
        self.carbon_offset_kg >= 0.0
    }

    pub fn requires_redesign(&self) -> bool {
        self.carbon_offset_kg >= 1e-6
    }

    pub fn requires_immediate_action(&self) -> bool {
        self.carbon_offset_kg >= CARBON_BUDGET_PER_CYCLE_KG
    }

    pub fn carbon_budget_usage_percent(&self) -> f32 {
        let actual_carbon = self.carbon_offset_kg + CARBON_BUDGET_PER_CYCLE_KG;
        (actual_carbon / CARBON_BUDGET_PER_CYCLE_KG) * 100.0
    }
}

pub fn phoenix_carbon_intensity(timestamp: DateTime<Utc>) -> CarbonIntensity {
    let hour_local = ((timestamp.hour() as i32 + 24 + MST_UTC_OFFSET) % 24) as u8;
    
    let (kg_co2e_per_kwh, source_mix) = match hour_local {
        0..=5 => (
            0.38,
            SourceMix {
                solar_fraction: 0.0,
                natural_gas_fraction: 0.85,
                other_fraction: 0.15,
            },
        ),
        6..=9 => (
            0.42,
            SourceMix {
                solar_fraction: 0.20,
                natural_gas_fraction: 0.70,
                other_fraction: 0.10,
            },
        ),
        10..=16 => (
            0.35,
            SourceMix {
                solar_fraction: 0.55,
                natural_gas_fraction: 0.35,
                other_fraction: 0.10,
            },
        ),
        17..=20 => (
            0.52,
            SourceMix {
                solar_fraction: 0.10,
                natural_gas_fraction: 0.80,
                other_fraction: 0.10,
            },
        ),
        21..=23 => (
            0.45,
            SourceMix {
                solar_fraction: 0.0,
                natural_gas_fraction: 0.85,
                other_fraction: 0.15,
            },
        ),
        _ => (
            PHOENIX_BASELINE_CI,
            SourceMix {
                solar_fraction: 0.0,
                natural_gas_fraction: 0.85,
                other_fraction: 0.15,
            },
        ),
    };
    
    CarbonIntensity {
        kg_co2e_per_kwh,
        hour_local,
        source_mix,
    }
}

pub fn compute_carbon_offset(joules_per_cycle: f32, timestamp: DateTime<Utc>) -> EnergyMetrics {
    let kwh = joules_per_cycle / JOULES_PER_KWH;
    let ci_info = phoenix_carbon_intensity(timestamp);
    let carbon_kg = kwh * ci_info.kg_co2e_per_kwh;
    
    EnergyMetrics {
        joules_per_cycle,
        carbon_offset_kg: carbon_kg - CARBON_BUDGET_PER_CYCLE_KG,
        carbon_intensity: ci_info.kg_co2e_per_kwh,
        timestamp: timestamp.timestamp(),
    }
}

pub fn optimal_execution_window(
    current_time: DateTime<Utc>,
    window_hours: u32,
) -> Option<DateTime<Utc>> {
    if window_hours == 0 {
        return None;
    }

    let mut min_ci = f32::MAX;
    let mut optimal_time = current_time;
    
    for offset in 0..window_hours {
        let candidate_time = current_time + chrono::Duration::hours(offset as i64);
        let ci = phoenix_carbon_intensity(candidate_time);
        
        if ci.kg_co2e_per_kwh < min_ci {
            min_ci = ci.kg_co2e_per_kwh;
            optimal_time = candidate_time;
        }
    }
    
    if min_ci < phoenix_carbon_intensity(current_time).kg_co2e_per_kwh {
        Some(optimal_time)
    } else {
        None
    }
}

pub fn energy_optimization_suggestion(metrics: &EnergyMetrics) -> String {
    if !metrics.requires_redesign() {
        return "No optimization needed - within eco budget".to_string();
    }
    
    let excess_carbon = metrics.carbon_offset_kg;
    let reduction_needed_percent = (excess_carbon / CARBON_BUDGET_PER_CYCLE_KG) * 100.0;
    
    if reduction_needed_percent > 50.0 {
        format!(
            "Critical: Reduce energy consumption by {:.1}%. Consider algorithmic optimization or caching.",
            reduction_needed_percent
        )
    } else if reduction_needed_percent > 20.0 {
        format!(
            "Warning: Reduce energy consumption by {:.1}%. Consider lazy evaluation.",
            reduction_needed_percent
        )
    } else {
        format!(
            "Minor: Reduce energy consumption by {:.1}%. Consider batching operations.",
            reduction_needed_percent
        )
    }
}

pub fn get_daily_carbon_curve() -> Vec<CarbonIntensity> {
    let base_time = Utc::now().date_naive().and_hms_opt(0, 0, 0).unwrap();
    let base_datetime = DateTime::<Utc>::from_naive_utc_and_offset(base_time, Utc);
    
    (0..24)
        .map(|hour| {
            let timestamp = base_datetime + chrono::Duration::hours(hour);
            phoenix_carbon_intensity(timestamp)
        })
        .collect()
}

pub fn estimate_daily_carbon_savings(
    joules_per_cycle: f32,
    cycles_per_day: u32,
    use_optimal_timing: bool,
) -> f32 {
    let daily_curve = get_daily_carbon_curve();
    
    let avg_ci = if use_optimal_timing {
        daily_curve
            .iter()
            .min_by(|a, b| a.kg_co2e_per_kwh.partial_cmp(&b.kg_co2e_per_kwh).unwrap())
            .map(|ci| ci.kg_co2e_per_kwh)
            .unwrap_or(PHOENIX_BASELINE_CI)
    } else {
        daily_curve.iter().map(|ci| ci.kg_co2e_per_kwh).sum::<f32>() / daily_curve.len() as f32
    };
    
    let kwh_per_cycle = joules_per_cycle / JOULES_PER_KWH;
    let daily_carbon_kg = kwh_per_cycle * cycles_per_day as f32 * avg_ci;
    
    let baseline_carbon_kg = kwh_per_cycle * cycles_per_day as f32 * PHOENIX_BASELINE_CI;
    
    baseline_carbon_kg - daily_carbon_kg
}

#[cfg(test)]
mod tests {
    use super::*;
    use chrono::TimeZone;

    #[test]
    fn test_carbon_intensity_solar_peak() {
        let noon_phx = Utc.with_ymd_and_hms(2026, 6, 1, 19, 0, 0).unwrap();
        let ci = phoenix_carbon_intensity(noon_phx);
        
        assert_eq!(ci.hour_local, 12);
        assert!(ci.kg_co2e_per_kwh < PHOENIX_BASELINE_CI);
        assert!(ci.source_mix.solar_fraction > 0.5);
        assert!(ci.source_mix.validate());
    }

    #[test]
    fn test_carbon_intensity_evening_peak() {
        let evening_phx = Utc.with_ymd_and_hms(2026, 6, 1, 2, 0, 0).unwrap();
        let ci = phoenix_carbon_intensity(evening_phx);
        
        assert_eq!(ci.hour_local, 19);
        assert!(ci.kg_co2e_per_kwh > PHOENIX_BASELINE_CI);
        assert!(ci.source_mix.natural_gas_fraction > 0.7);
        assert!(ci.source_mix.validate());
    }

    #[test]
    fn test_carbon_offset_calculation() {
        let timestamp = Utc::now();
        let joules = 10000.0;
        
        let metrics = compute_carbon_offset(joules, timestamp);
        
        assert_eq!(metrics.joules_per_cycle, joules);
        assert!(metrics.carbon_intensity > 0.0);
    }

    #[test]
    fn test_eco_violation_threshold() {
        let timestamp = Utc::now();
        
        let low_energy = compute_carbon_offset(1000.0, timestamp);
        assert!(!low_energy.is_eco_violation());
        
        let high_energy = compute_carbon_offset(1_000_000.0, timestamp);
        assert!(high_energy.is_eco_violation());
    }

    #[test]
    fn test_optimal_execution_window() {
        let morning = Utc.with_ymd_and_hms(2026, 6, 1, 14, 0, 0).unwrap();
        
        let optimal = optimal_execution_window(morning, 12);
        
        assert!(optimal.is_some());
        
        let optimal_time = optimal.unwrap();
        let optimal_ci = phoenix_carbon_intensity(optimal_time);
        let morning_ci = phoenix_carbon_intensity(morning);
        
        assert!(optimal_ci.kg_co2e_per_kwh <= morning_ci.kg_co2e_per_kwh);
    }

    #[test]
    fn test_energy_metrics_new() {
        let timestamp = Utc::now();
        let metrics = EnergyMetrics::new(5000.0, timestamp);
        
        assert_eq!(metrics.joules_per_cycle, 5000.0);
        assert!(metrics.carbon_budget_usage_percent() > 0.0);
    }

    #[test]
    fn test_daily_carbon_curve() {
        let curve = get_daily_carbon_curve();
        
        assert_eq!(curve.len(), 24);
        
        let min_ci = curve.iter().min_by(|a, b| a.kg_co2e_per_kwh.partial_cmp(&b.kg_co2e_per_kwh).unwrap()).unwrap();
        assert!(min_ci.hour_local >= 10 && min_ci.hour_local <= 16);
    }

    #[test]
    fn test_optimal_timing_carbon_savings() {
        let joules = 100000.0;
        let cycles = 100;
        
        let savings = estimate_daily_carbon_savings(joules, cycles, true);
        
        assert!(savings >= 0.0);
    }

    #[test]
    fn test_source_mix_validation() {
        let valid_mix = SourceMix {
            solar_fraction: 0.5,
            natural_gas_fraction: 0.4,
            other_fraction: 0.1,
        };
        assert!(valid_mix.validate());
        
        let invalid_mix = SourceMix {
            solar_fraction: 0.6,
            natural_gas_fraction: 0.6,
            other_fraction: 0.1,
        };
        assert!(!invalid_mix.validate());
    }

    #[test]
    fn test_zero_window_hours() {
        let now = Utc::now();
        let result = optimal_execution_window(now, 0);
        assert!(result.is_none());
    }
}
