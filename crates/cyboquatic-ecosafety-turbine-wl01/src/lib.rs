// Filename: crates/cyboquatic-ecosafety-turbine-wl01/src/lib.rs

#![forbid(unsafe_code)]

use std::time::SystemTime;
use cyboquatic_ecosafety_core::{
    RiskCoord, RiskVector, LyapunovWeights, Residual, KerWindow, SafeStepGate, SafeStepDecision,
};
use cyboquatic_ecosafety_hydroturbine::{
    HydraulicRisk, HydraulicWeights, HabitatDynamics, HabitatWeights, FishShearInputs,
    LethalityCorridor, lethality_index,
};

/// Struct representing one PHX-CANAL-NODE-WL-01 turbine shard row.
#[derive(Clone, Debug)]
pub struct TurbineShard {
    pub qm3s: f64,
    pub hlrmperh: f64,
    pub rsurcharge: RiskCoord,
    pub rcavitation: RiskCoord,
    pub roverpressure: RiskCoord,
    pub renergy_hydraulic: RiskCoord,
    pub headm: f64,
    pub vtip_ms: f64,
    pub shear_tau_pa: f64,
    pub delta_p_pa: f64,
    pub lethality_index: f64,
    pub rfishshear: RiskCoord,
    pub ramp_rate_du_dt: f64,
    pub turbulence_I: f64,
    pub rramp: RiskCoord,
    pub rturbulence: RiskCoord,
    pub rhabitat: RiskCoord,
    pub rbiodiversity: RiskCoord,
    pub rpathogen: RiskCoord,
    pub renergy: RiskCoord,
    pub rcarbon: RiskCoord,
    pub rmaterials: RiskCoord,
    pub rsigma: RiskCoord,
    pub vt: f64,
}

/// Hydraulic aggregation for turbine workloads.
pub fn aggregate_hydraulics(shard: &TurbineShard, w: HydraulicWeights) -> RiskCoord {
    let h = HydraulicRisk {
        rsurcharge: shard.rsurcharge,
        rcavitation: shard.rcavitation,
        roverpressure: shard.roverpressure,
        renergy_hydraulic: shard.renergy_hydraulic,
    };
    h.aggregate(w)
}

/// Habitat aggregation for turbine workloads.
pub fn aggregate_habitat(shard: &TurbineShard, w: HabitatWeights) -> RiskCoord {
    let h = HabitatDynamics {
        r_ramp: shard.rramp,
        r_turbulence: shard.rturbulence,
    };
    h.aggregate(w)
}

/// Compute fish shear risk from physical inputs and lethality corridor.
pub fn compute_fish_shear(
    shard: &TurbineShard,
    k_v: f64,
    k_tau: f64,
    k_dp: f64,
    corridor: LethalityCorridor,
) -> RiskCoord {
    let inputs = FishShearInputs {
        v_tip: shard.vtip_ms,
        tau: shard.shear_tau_pa,
        delta_p: shard.delta_p_pa,
    };
    let l = lethality_index(inputs, k_v, k_tau, k_dp);
    corridor.normalize(l)
}

/// Build a full RiskVector for the turbine workload.
pub fn build_risk_vector(
    shard: &TurbineShard,
    rhydraulics: RiskCoord,
    rbiology: RiskCoord,
) -> RiskVector {
    RiskVector {
        renergy: shard.renergy,
        rhydraulics,
        rbiology,
        rcarbon: shard.rcarbon,
        rmaterials: shard.rmaterials,
        rbiodiversity: shard.rbiodiversity,
        rsigma: shard.rsigma,
    }
}

/// Evaluate SafeStepGate for a proposed turbine workload step.
pub fn evaluate_turbine_step(
    shard_next: &TurbineShard,
    weights: LyapunovWeights,
    vt_current: Residual,
    rhydraulics: RiskCoord,
    rbiology: RiskCoord,
) -> (Residual, SafeStepDecision) {
    let rv_next = build_risk_vector(shard_next, rhydraulics, rbiology);
    let gate = SafeStepGate::new(weights, vt_current);
    gate.evaluate_next(rv_next)
}

/// Compute KER window scores over a series of turbine residuals and max risks.
pub fn ker_window_for_turbine(
    residual_series: &[Residual],
    max_risks: &[f64],
) -> Option<KerWindow> {
    if residual_series.len() < 2 || residual_series.len() != max_risks.len() {
        return None;
    }
    let vts: Vec<f64> = residual_series.iter().map(|r| r.vt).collect();
    KerWindow::from_residual_series(&vts, max_risks)
}
