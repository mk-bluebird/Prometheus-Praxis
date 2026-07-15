// ecore_restoration_shard/crates/drainage_decay/src/lifeforce_duty.rs

#![forbid(unsafe_code)]

use serde::{Deserialize, Serialize};

pub const ROHCEILINGGLOBAL: f64 = 0.30;
pub const HFLOWHARDCEILING: f64 = 0.30;

#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct DrainageRiskParams {
    pub lambda_bod: f64,
    pub lambda_tss: f64,
    pub gamma_bod: f64,
    pub gamma_tss: f64,
    pub lveto_bod: f64,
    pub lveto_tss: f64,
    pub monitoring_dt_hours: f64,
}

impl DrainageRiskParams {
    pub fn clamped(self) -> Self {
        Self {
            lambda_bod: self.lambda_bod.max(0.0),
            lambda_tss: self.lambda_tss.max(0.0),
            gamma_bod: self.gamma_bod.max(0.0),
            gamma_tss: self.gamma_tss.max(0.0),
            lveto_bod: self.lveto_bod.max(0.0),
            lveto_tss: self.lveto_tss.max(0.0),
            monitoring_dt_hours: self.monitoring_dt_hours.max(0.0),
        }
    }
}

#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct DrainageStateSnapshot {
    pub corridorid: String,
    pub bod_mg_per_l: f64,
    pub tss_mg_per_l: f64,
    pub cec_cmol_per_kg: f64,
    pub temperature_c: f64,
    pub flow_lps: f64,
    pub k_score: f64,
    pub e_score: f64,
    pub r_score: f64,
}

#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct DecayKernelParams {
    pub k_bod_per_day: f64,
    pub k_tss_per_day: f64,
    pub theta: f64,
    pub ref_temp_c: f64,
}

#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct DrainageDutyDecision {
    pub corridorid: String,
    pub prob_hit_bod: f64,
    pub prob_hit_tss: f64,
    pub prob_hit_any: f64,
    pub allowed: bool,
    pub reasons: Vec<String>,
}

pub fn decay_to_horizon(
    state: &DrainageStateSnapshot,
    params: &DecayKernelParams,
    dt_hours: f64,
) -> (f64, f64) {
    let dt_hours = dt_hours.max(0.0);

    let temp_factor = temperature_factor(params.theta, params.ref_temp_c, state.temperature_c);

    let k_bod_per_hour = params.k_bod_per_day * 24.0 * temp_factor;
    let k_tss_per_hour = params.k_tss_per_day * 24.0 * temp_factor;

    let bod_next = first_order_decay(state.bod_mg_per_l, k_bod_per_hour, dt_hours);
    let tss_next = first_order_decay(state.tss_mg_per_l, k_tss_per_hour, dt_hours);

    let bod_clamped = bod_next.max(0.0);
    let tss_clamped = tss_next.max(0.0);

    (bod_clamped, tss_clamped)
}

pub fn evaluate_duty_window(
    state: &DrainageStateSnapshot,
    decay_params: &DecayKernelParams,
    risk: DrainageRiskParams,
    aln_floor_bod: f64,
    aln_floor_tss: f64,
) -> DrainageDutyDecision {
    let risk = risk.clamped();

    let (bod_next, tss_next) = decay_to_horizon(state, decay_params, risk.monitoring_dt_hours);

    let prob_hit_bod = hitting_prob_ou(
        state.bod_mg_per_l,
        risk.lveto_bod,
        risk.lambda_bod,
        risk.gamma_bod,
        risk.monitoring_dt_hours,
    );
    let prob_hit_tss = hitting_prob_ou(
        state.tss_mg_per_l,
        risk.lveto_tss,
        risk.lambda_tss,
        risk.gamma_tss,
        risk.monitoring_dt_hours,
    );

    let prob_hit_any = 1.0 - (1.0 - prob_hit_bod).max(0.0) * (1.0 - prob_hit_tss).max(0.0);

    let mut reasons = Vec::new();
    let mut allowed = true;

    if prob_hit_any >= 1e-12 {
        allowed = false;
        reasons.push(format!(
            "Hitting probability {:.3e} exceeds 1e-12 corridor limit.",
            prob_hit_any,
        ));
    }

    if bod_next < aln_floor_bod || tss_next < aln_floor_tss {
        allowed = false;
        reasons.push("Deterministic decay crosses ALN floor for BOD/TSS.".to_string());
    }

    if state.r_score > 0.25 {
        allowed = false;
        reasons.push(
            "R_score above 0.25 corridor bound; duty window must remain in RESEARCH.".to_string(),
        );
    }

    DrainageDutyDecision {
        corridorid: state.corridorid.clone(),
        prob_hit_bod,
        prob_hit_tss,
        prob_hit_any,
        allowed,
        reasons,
    }
}

fn decay_to_horizon(
    state: &DrainageStateSnapshot,
    params: &DecayKernelParams,
    dt_hours: f64,
) -> (f64, f64) {
    let dt = dt_hours.max(0.0);
    let temp_factor = temperature_factor(params.theta, params.ref_temp_c, state.temperature_c);

    let k_bod_per_hour = params.k_bod_per_day * 24.0 * temp_factor;
    let k_tss_per_hour = params.k_tss_per_day * 24.0 * temp_factor;

    let bod_next = first_order_decay(state.bod_mg_per_l, k_bod_per_hour, dt);
    let tss_next = first_order_decay(state.tss_mg_per_l, k_tss_per_hour, dt);

    let bod_clamped = bod_next.max(0.0);
    let tss_clamped = tss_next.max(0.0);

    (bod_clamped, tss_clamped)
}

fn first_order_decay(initial: f64, k_per_hour: f64, dt_hours: f64) -> f64 {
    if initial <= 0.0 {
        return 0.0;
    }
    if k_per_hour <= 0.0 || dt_hours <= 0.0 {
        return initial.max(0.0);
    }
    let exponent = -k_per_hour * dt_hours;
    let factor = exponent.exp();
    (initial * factor).max(0.0)
}

fn hitting_prob_ou(x0: f64, lveto: f64, lambda: f64, gamma: f64, t: f64) -> f64 {
    if x0 >= lveto {
        return 1.0;
    }
    if t <= 0.0 || gamma <= 0.0 || lambda < 0.0 {
        return 0.0;
    }

    let drift = -lambda * (x0 - lveto) * t;
    let var = gamma * gamma * t;
    if var <= 0.0 {
        return 0.0;
    }

    let z = (lveto - (x0 + drift)).max(0.0) / var.sqrt();
    upper_tail_gaussian(z)
}

fn upper_tail_gaussian(z: f64) -> f64 {
    let za = z.abs();
    let approx = 1.0 / (1.0 + za);
    approx.min(1.0).max(0.0)
}

fn temperature_factor(theta: f64, ref_temp_c: f64, current_temp_c: f64) -> f64 {
    let delta = current_temp_c - ref_temp_c;
    let ln_theta = if theta <= 0.0 { 0.0 } else { theta.ln() };
    (ln_theta * (delta / 10.0)).exp()
}
