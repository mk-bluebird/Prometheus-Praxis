// filename: kerresidual/src/ecounit.rs
// destination: kerresidual/src/ecounit.rs

use serde::{Deserialize, Serialize};

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct EcoUnit {
    pub steward_id: String,
    pub region: String,
    pub lane: EcoLane,

    pub k_mean: f64,
    pub e_mean: f64,
    pub r_mean: f64,
    pub vt_max_window: f64,

    pub s_region: f64,
    pub w_s: f64,
    pub b_s: f64,

    pub alpha_lane: f64,
    pub beta_lane: f64,
    pub gamma_lane: f64,

    pub eco_unit_raw: f64,
    pub eco_unit_after_education: f64,
    pub eco_unit_final: f64,

    pub mk_education: f64,
    pub delta_e_phys: f64,

    pub plane_contract_id: String,
    pub corridor_set_id: String,
    pub eco_wealth_kernel_id: String,
    pub lane_policy_id: String,
}

#[derive(Debug, Clone, Copy, Serialize, Deserialize)]
pub enum EcoLane {
    Research,
    Pilot,
    Prod,
    Exp,
}

impl EcoUnit {
    pub fn compute_raw(
        k_mean: f64,
        e_mean: f64,
        r_mean: f64,
        s_region: f64,
        w_s: f64,
        b_s: f64,
        alpha_lane: f64,
        beta_lane: f64,
        gamma_lane: f64,
    ) -> f64 {
        let k_term = k_mean.clamp(0.0, 1.0).powf(alpha_lane);
        let e_term = e_mean.clamp(0.0, 1.0).powf(beta_lane);
        let r_term = (1.0 - r_mean.clamp(0.0, 1.0)).powf(gamma_lane);

        let w_scalar = w_s * k_term * e_term * r_term;
        let base = s_region * (w_scalar + b_s);

        if base.is_nan() || base.is_sign_negative() {
            0.0
        } else {
            base
        }
    }

    pub fn recompute(&mut self) {
        self.eco_unit_raw = Self::compute_raw(
            self.k_mean,
            self.e_mean,
            self.r_mean,
            self.s_region,
            self.w_s,
            self.b_s,
            self.alpha_lane,
            self.beta_lane,
            self.gamma_lane,
        );
        self.eco_unit_after_education = self.eco_unit_raw * self.mk_education;
        self.eco_unit_final = self.eco_unit_after_education.max(0.0);
    }
}
