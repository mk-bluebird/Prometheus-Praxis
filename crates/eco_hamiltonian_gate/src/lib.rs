// Filename: crates/eco_hamiltonian_gate/src/lib.rs
use serde::{Deserialize, Serialize};

#[derive(Clone, Copy, Debug, Serialize, Deserialize)]
pub struct RiskCoord(pub f64); // clamped [0,1]

#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct RiskVector {
    pub r_energy: RiskCoord,
    pub r_hydraulics: RiskCoord,
    pub r_biology: RiskCoord,
    pub r_carbon: RiskCoord,
    pub r_materials: RiskCoord,
    pub r_biodiversity: RiskCoord,
    pub r_dataquality: RiskCoord,
}

#[derive(Clone, Copy, Debug, Serialize, Deserialize)]
pub struct LyapunovWeights {
    pub w_energy: f64,
    pub w_hydraulics: f64,
    pub w_biology: f64,
    pub w_carbon: f64,
    pub w_materials: f64,
    pub w_biodiversity: f64,
    pub w_dataquality: f64,
}

#[derive(Clone, Copy, Debug, Serialize, Deserialize)]
pub struct Residual {
    pub vt: f64,
}

#[derive(Clone, Copy, Debug, Serialize, Deserialize)]
pub struct JetsonLineBio {
    pub v_bio_0: f64,
    pub alpha: f64,
    pub t0: f64,
}

impl JetsonLineBio {
    pub fn value_at(&self, t: f64) -> f64 {
        let dt = t - self.t0;
        self.v_bio_0 * (-self.alpha * dt).exp()
    }
}

pub fn compute_vt(rv: &RiskVector, w: &LyapunovWeights) -> Residual {
    let sq = |x: f64| x * x;
    let vt =
        w.w_energy * sq(rv.r_energy.0) +
        w.w_hydraulics * sq(rv.r_hydraulics.0) +
        w.w_biology * sq(rv.r_biology.0) +
        w.w_carbon * sq(rv.r_carbon.0) +
        w.w_materials * sq(rv.r_materials.0) +
        w.w_biodiversity * sq(rv.r_biodiversity.0) +
        w.w_dataquality * sq(rv.r_dataquality.0);
    Residual { vt }
}

#[derive(Clone, Copy, Debug, Serialize, Deserialize)]
pub struct SafeStepConfig {
    pub interior_vt: f64,
    pub epsilon: f64,
}

#[derive(Clone, Copy, Debug, Serialize, Deserialize)]
pub struct SafeStepResult {
    pub allowed: bool,
    pub before: Residual,
    pub after: Residual,
}

pub fn safestep_gate(
    before_rv: &RiskVector,
    after_rv: &RiskVector,
    w: &LyapunovWeights,
    cfg: &SafeStepConfig,
) -> SafeStepResult {
    let before = compute_vt(before_rv, w);
    let after = compute_vt(after_rv, w);

    // hard-band: all coords must stay in [0,1]
    let hard_ok =
        after_rv.r_energy.0 >= 0.0 && after_rv.r_energy.0 <= 1.0 &&
        after_rv.r_hydraulics.0 >= 0.0 && after_rv.r_hydraulics.0 <= 1.0 &&
        after_rv.r_biology.0 >= 0.0 && after_rv.r_biology.0 <= 1.0 &&
        after_rv.r_carbon.0 >= 0.0 && after_rv.r_carbon.0 <= 1.0 &&
        after_rv.r_materials.0 >= 0.0 && after_rv.r_materials.0 <= 1.0 &&
        after_rv.r_biodiversity.0 >= 0.0 && after_rv.r_biodiversity.0 <= 1.0 &&
        after_rv.r_dataquality.0 >= 0.0 && after_rv.r_dataquality.0 <= 1.0;

    if !hard_ok {
        return SafeStepResult { allowed: false, before, after };
    }

    // Lyapunov gate
    let allowed = if before.vt > cfg.interior_vt {
        after.vt <= before.vt + cfg.epsilon
    } else {
        after.vt <= before.vt + cfg.epsilon
    };

    SafeStepResult { allowed, before, after }
}

#[derive(Clone, Copy, Debug, Serialize, Deserialize)]
pub struct JetsonGateResult {
    pub allowed: bool,
    pub v_bio_next: f64,
    pub j_bio_next: f64,
}

pub fn jetson_bio_gate(
    rv_next: &RiskVector,
    w: &LyapunovWeights,
    jetson: &JetsonLineBio,
    t_next: f64,
) -> JetsonGateResult {
    let v_bio_next =
        w.w_biology * rv_next.r_biology.0 * rv_next.r_biology.0;
    let j_bio_next = jetson.value_at(t_next);
    let allowed = v_bio_next <= j_bio_next;
    JetsonGateResult {
        allowed,
        v_bio_next,
        j_bio_next,
    }
}

#[derive(Clone, Copy, Debug, Serialize, Deserialize)]
pub struct LipschitzKernelBound {
    pub l_energy: f64,
    pub l_hydraulics: f64,
    pub l_biology: f64,
    pub l_carbon: f64,
    pub l_materials: f64,
    pub l_biodiversity: f64,
    pub l_dataquality: f64,
}

pub fn lipschitz_bound_vt(
    w: &LyapunovWeights,
    lk: &LipschitzKernelBound,
    delta_m: &[f64; 7],
) -> f64 {
    // Simple upper bound: sum_j 2 w_j L_j |delta m_j|
    let coeffs = [
        2.0 * w.w_energy * lk.l_energy,
        2.0 * w.w_hydraulics * lk.l_hydraulics,
        2.0 * w.w_biology * lk.l_biology,
        2.0 * w.w_carbon * lk.l_carbon,
        2.0 * w.w_materials * lk.l_materials,
        2.0 * w.w_biodiversity * lk.l_biodiversity,
        2.0 * w.w_dataquality * lk.l_dataquality,
    ];
    let mut sum = 0.0;
    for (c, dm) in coeffs.iter().zip(delta_m.iter()) {
        sum += c * dm.abs();
    }
    sum
}
