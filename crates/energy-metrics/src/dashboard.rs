//! Energy Dashboard for Real-Time Monitoring

#![forbid(unsafe_code)]
#![deny(warnings)]

use crate::carbon_intensity::{EnergyMetrics, get_daily_carbon_curve};
use chrono::{DateTime, Utc};
use serde::{Deserialize, Serialize};
use std::collections::HashMap;

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct DashboardConfig {
    pub refresh_interval_seconds: u64,
    pub alert_threshold_carbon_kg: f32,
    pub repos: Vec<String>,
}

impl Default for DashboardConfig {
    fn default() -> Self {
        Self {
            refresh_interval_seconds: 60,
            alert_threshold_carbon_kg: 0.005,
            repos: vec![
                "eco_restoration_shard".to_string(),
                "EcoNet".to_string(),
                "Data_Lake".to_string(),
            ],
        }
    }
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct RepoEnergyStats {
    pub repo_name: String,
    pub total_joules: f32,
    pub total_carbon_kg: f32,
    pub artifact_count: usize,
    pub violations: usize,
    pub last_updated: i64,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct EnergyDashboard {
    pub config: DashboardConfig,
    pub repo_stats: HashMap<String, RepoEnergyStats>,
    pub daily_carbon_curve: Vec<f32>,
}

impl EnergyDashboard {
    pub fn new(config: DashboardConfig) -> Self {
        let daily_curve = get_daily_carbon_curve()
            .iter()
            .map(|ci| ci.kg_co2e_per_kwh)
            .collect();

        Self {
            config,
            repo_stats: HashMap::new(),
            daily_carbon_curve: daily_curve,
        }
    }

    pub fn add_repo_metrics(&mut self, repo_name: String, metrics: Vec<EnergyMetrics>) {
        let total_joules = metrics.iter().map(|m| m.joules_per_cycle).sum();
        let total_carbon = metrics
            .iter()
            .map(|m| m.carbon_offset_kg.max(0.0))
            .sum();
        let violations = metrics.iter().filter(|m| m.is_eco_violation()).count();

        let stats = RepoEnergyStats {
            repo_name: repo_name.clone(),
            total_joules,
            total_carbon_kg: total_carbon,
            artifact_count: metrics.len(),
            violations,
            last_updated: Utc::now().timestamp(),
        };

        self.repo_stats.insert(repo_name, stats);
    }

    pub fn generate_alert_report(&self) -> Vec<String> {
        let mut alerts = Vec::new();

        for (repo, stats) in &self.repo_stats {
            if stats.total_carbon_kg > self.config.alert_threshold_carbon_kg {
                alerts.push(format!(
                    "ALERT: {} exceeds carbon threshold: {:.6} kg CO2e",
                    repo, stats.total_carbon_kg
                ));
            }

            if stats.violations > 0 {
                alerts.push(format!(
                    "WARNING: {} has {} eco violations",
                    repo, stats.violations
                ));
            }
        }

        alerts
    }

    pub fn total_energy_consumption(&self) -> f32 {
        self.repo_stats.values().map(|s| s.total_joules).sum()
    }

    pub fn total_carbon_footprint(&self) -> f32 {
        self.repo_stats.values().map(|s| s.total_carbon_kg).sum()
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_dashboard_creation() {
        let config = DashboardConfig::default();
        let dashboard = EnergyDashboard::new(config);

        assert_eq!(dashboard.daily_carbon_curve.len(), 24);
    }

    #[test]
    fn test_repo_metrics_aggregation() {
        let mut dashboard = EnergyDashboard::new(DashboardConfig::default());

        let metrics = vec![
            EnergyMetrics {
                joules_per_cycle: 1000.0,
                carbon_offset_kg: -0.005,
                carbon_intensity: 0.4,
                timestamp: 0,
            },
            EnergyMetrics {
                joules_per_cycle: 2000.0,
                carbon_offset_kg: 0.010,
                carbon_intensity: 0.5,
                timestamp: 0,
            },
        ];

        dashboard.add_repo_metrics("test_repo".to_string(), metrics);

        assert_eq!(dashboard.repo_stats.len(), 1);
        assert_eq!(dashboard.repo_stats["test_repo"].artifact_count, 2);
    }
}
