// Filename: crates/ktsf_tolerance/src/lib.rs
use serde::{Deserialize, Serialize};

#[derive(Clone, Copy, Debug, Serialize, Deserialize)]
pub struct KarmaComponents {
    pub eco_impact: f64,   // E_i in [0,1]
    pub corridor_compliance: f64, // C_i in [0,1]
    pub safety_score: f64, // S_i in [0,1], e.g. 1 - R_i
}

#[derive(Clone, Copy, Debug, Serialize, Deserialize)]
pub struct KarmaWeights {
    pub alpha_e: f64,
    pub alpha_c: f64,
    pub alpha_s: f64,
}

impl KarmaWeights {
    pub fn normalized(self) -> Self {
        let sum = self.alpha_e + self.alpha_c + self.alpha_s;
        if sum == 0.0 {
            Self { alpha_e: 0.0, alpha_c: 0.0, alpha_s: 0.0 }
        } else {
            Self {
                alpha_e: self.alpha_e / sum,
                alpha_c: self.alpha_c / sum,
                alpha_s: self.alpha_s / sum,
            }
        }
    }
}

pub fn karma_value(kc: KarmaComponents, kw: KarmaWeights) -> f64 {
    let w = kw.normalized();
    w.alpha_e * kc.eco_impact
        + w.alpha_c * kc.corridor_compliance
        + w.alpha_s * kc.safety_score
}

#[derive(Clone, Copy, Debug, Serialize, Deserialize)]
pub struct ToleranceParams {
    pub t_min: f64,
    pub t_max: f64,
    pub k_mid: f64,
    pub slope_a: f64,
}

pub fn ktsf_tolerance_radius(k_i: f64, p: ToleranceParams) -> f64 {
    let a = p.slope_a;
    let k0 = p.k_mid;
    let sigma = 1.0 / (1.0 + (-a * (k_i - k0)).exp());
    p.t_min + (p.t_max - p.t_min) * sigma
}

#[derive(Clone, Copy, Debug, Serialize, Deserialize)]
pub struct PumpCommandGate {
    pub baseline_u: f64,  // baseline normalized pump speed [0,1]
}

impl PumpCommandGate {
    pub fn gate(
        &self,
        proposed_u: f64,
        k_i: f64,
        params: ToleranceParams,
    ) -> f64 {
        let t_i = ktsf_tolerance_radius(k_i, params);
        let delta = proposed_u - self.baseline_u;
        if delta.abs() <= t_i {
            proposed_u
        } else if delta > 0.0 {
            self.baseline_u + t_i
        } else {
            self.baseline_u - t_i
        }
    }
}
