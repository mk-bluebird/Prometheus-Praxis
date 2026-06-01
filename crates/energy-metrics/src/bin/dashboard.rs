//! Energy Dashboard CLI Tool

use energy_metrics::{DashboardConfig, EnergyDashboard, EnergyMetrics};
use chrono::Utc;

fn main() {
    let config = DashboardConfig::default();
    let mut dashboard = EnergyDashboard::new(config);

    let sample_metrics = vec![
        EnergyMetrics {
            joules_per_cycle: 5000.0,
            carbon_offset_kg: -0.002,
            carbon_intensity: 0.35,
            timestamp: Utc::now().timestamp(),
        },
    ];

    dashboard.add_repo_metrics("eco_restoration_shard".to_string(), sample_metrics);

    println!("=== Energy Dashboard ===");
    println!("Total Energy: {:.2} J", dashboard.total_energy_consumption());
    println!("Total Carbon: {:.6} kg CO2e", dashboard.total_carbon_footprint());
    println!("\nAlerts:");
    
    for alert in dashboard.generate_alert_report() {
        println!("  {}", alert);
    }
}
