use serde::{Deserialize, Serialize};

/// Eco impact recording for each pass.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct EcoImpact {
    /// Baseline kWh per car trip avoided.
    pub eco_kwh_baseline_per_trip: f32,
    /// Transit kWh per trip.
    pub eco_kwh_transit_per_trip: f32,
    /// Accumulated savings in kWh for this binding.
    pub eco_savings_kwh: f32,
}

impl EcoImpact {
    /// Update eco_savings_kwh when a new tap is performed.
    /// Mathematical model: \(\Delta E_k = E_{\text{online},k} - E_{\text{offline},k}\) plus modal shift savings.
    pub fn add_tap_savings(&mut self) {
        let delta = self.eco_kwh_baseline_per_trip - self.eco_kwh_transit_per_trip;
        if delta > 0.0 {
            self.eco_savings_kwh += delta;
        }
    }
}

/// Eco configuration for kiosks, linking empirical measurements into the model.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct EcoConfig {
    pub p_radio_watts: f32,
    pub t_net_seconds: f32,
    pub e_backend_joules: f32,
    pub p_cpu_watts: f32,
    pub t_local_verify_seconds: f32,
}

impl EcoConfig {
    /// Compute E_online = P_radio * t_net + E_backend.
    pub fn e_online_joules(&self) -> f32 {
        self.p_radio_watts * self.t_net_seconds + self.e_backend_joules
    }

    /// Compute E_offline = P_cpu * t_local_verify.
    pub fn e_offline_joules(&self) -> f32 {
        self.p_cpu_watts * self.t_local_verify_seconds
    }

    /// Per-tap savings: \(\Delta E_k = E_{\text{online},k} - E_{\text{offline},k}\).
    pub fn delta_e_per_tap_joules(&self) -> f32 {
        self.e_online_joules() - self.e_offline_joules()
    }
}
