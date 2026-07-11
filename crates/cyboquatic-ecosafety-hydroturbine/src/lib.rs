// File: crates/cyboquatic-ecosafety-hydroturbine/src/lib.rs
// License: MIT OR Apache-2.0
// rust-version = "1.85"
// edition = "2024"
// Kani verifier version: 0.67 (must be present in workspace, non-optional)

#![forbid(unsafe_code)]

use std::time::{Duration, SystemTime};

use cyboquatic_ecosafety_core::{
    KerWindow,
    LyapunovWeights,
    Residual,
    RiskCoord,
    RiskVector,
    SafeController,
    SafeStepDecision,
    SafeStepGate,
};

/// PHX-CANAL-NODE-WL-01 specific risk coordinates for fish-safe hydropower.
/// All coordinates are normalized RiskCoord in [0.0, 1.0] and are strictly diagnostic.
/// No actuator bindings are allowed in this crate.
#[derive(Clone, Copy, Debug)]
pub struct TurbineRiskVector {
    pub r_fish_shear: RiskCoord,
    pub r_habitat: RiskCoord,
    pub r_surcharge: RiskCoord,
    pub r_energy_eff: RiskCoord,
    pub r_pathogen: RiskCoord,
    pub r_biodiversity: RiskCoord,
    pub r_sigma: RiskCoord,
}

impl TurbineRiskVector {
    /// Lift turbine-plane risks into the universal RiskVector used by the ecosafety core.
    /// Mapping:
    /// - r_fish_shear      -> biology plane
    /// - r_habitat         -> biodiversity plane
    /// - r_surcharge       -> hydraulics plane
    /// - r_energy_eff      -> energy plane
    /// - r_pathogen        -> biology plane (aggregated with shear)
    /// - r_biodiversity    -> biodiversity plane (aggregated with habitat)
    /// - r_sigma           -> uncertainty plane
    pub fn into_core_vector(self) -> RiskVector {
        // Conservative aggregation: use max for planes with multiple contributors.
        let r_biology = RiskCoord::new(self.r_fish_shear.value.max(self.r_pathogen.value));
        let r_biodiv = RiskCoord::new(self.r_habitat.value.max(self.r_biodiversity.value));
        RiskVector {
            renergy: self.r_energy_eff,
            rhydraulics: self.r_surcharge,
            rbiology: r_biology,
            rcarbon: RiskCoord::new(0.0),
            rmaterials: RiskCoord::new(0.0),
            rbiodiversity: r_biodiv,
            rsigma: self.r_sigma,
        }
    }
}

/// Corridor bands for PHX-CANAL-NODE-WL-01.
/// All raw inputs are physical measurements; kernels below normalize them into [0.0, 1.0].
#[derive(Clone, Copy, Debug)]
pub struct FishShearCorridor {
    pub l_safe: f64,  // safe lethality (e.g. survival > 98%)
    pub l_gold: f64,  // gold band threshold
    pub l_hard: f64,  // hard corridor limit (e.g. survival 95%)
}

#[derive(Clone, Copy, Debug)]
pub struct SurchargeCorridor {
    pub hlr_safe: f64, // safe hydraulic loading rate m/h
    pub hlr_gold: f64,
    pub hlr_hard: f64,
}

#[derive(Clone, Copy, Debug)]
pub struct HabitatCorridor {
    pub accel_safe: f64, // ramp acceleration safe bound (m/s^2 or g-equivalent)
    pub accel_gold: f64,
    pub accel_hard: f64,
}

#[derive(Clone, Copy, Debug)]
pub struct EnergyEfficiencyCorridor {
    pub kwh_per_kg_safe: f64,
    pub kwh_per_kg_gold: f64,
    pub kwh_per_kg_hard: f64,
}

#[derive(Clone, Copy, Debug)]
pub struct PathogenCorridor {
    pub index_safe: f64,
    pub index_gold: f64,
    pub index_hard: f64,
}

#[derive(Clone, Copy, Debug)]
pub struct BiodiversityCorridor {
    pub conn_safe: f64,
    pub conn_gold: f64,
    pub conn_hard: f64,
    pub comp_safe: f64,
    pub comp_gold: f64,
    pub comp_hard: f64,
    pub colon_safe: f64,
    pub colon_gold: f64,
    pub colon_hard: f64,
}

#[derive(Clone, Copy, Debug)]
pub struct SigmaCorridor {
    pub sigma_safe: f64,
    pub sigma_gold: f64,
    pub sigma_hard: f64,
}

/// Piecewise linear normalization for harmful-increasing metrics.
/// Lower raw values are better; risk is 0 in safe band, 1 at hard limit and above.
fn normalize_harmful(raw: f64, safe: f64, gold: f64, hard: f64) -> RiskCoord {
    let x = raw;
    let r = if x <= safe {
        0.0
    } else if x <= gold {
        // safe -> gold maps to [0.0, 0.4]
        let frac = (x - safe) / (gold - safe);
        0.4 * frac
    } else if x <= hard {
        // gold -> hard maps to [0.4, 1.0]
        let frac = (x - gold) / (hard - gold);
        0.4 + 0.6 * frac
    } else {
        1.0
    };
    RiskCoord::new(r)
}

/// Piecewise linear normalization for beneficial-increasing metrics.
/// Higher raw values are better; risk is 1 at low values and tends to 0 at gold/safe.
fn normalize_beneficial(raw: f64, safe: f64, gold: f64, hard: f64) -> RiskCoord {
    let x = raw;
    let r = if x >= safe {
        0.0
    } else if x >= gold {
        // gold -> safe: risk [0.4, 0.0]
        let frac = (safe - x) / (safe - gold);
        0.4 * frac
    } else if x >= hard {
        // hard -> gold: risk [1.0, 0.4]
        let frac = (gold - x) / (gold - hard);
        0.4 + 0.6 * frac
    } else {
        1.0
    };
    RiskCoord::new(r)
}

/// Compute fish shear risk from a lethality index derived from CFD or sensor fish.
/// The lethality index must be calibrated empirically; this kernel just normalizes it.
pub fn compute_r_fish_shear(lethality_index: f64, corridor: FishShearCorridor) -> RiskCoord {
    normalize_harmful(lethality_index, corridor.l_safe, corridor.l_gold, corridor.l_hard)
}

/// Compute surcharge risk from hydraulic loading rate (HLR) in m/h.
pub fn compute_r_surcharge(hlr_m_per_h: f64, corridor: SurchargeCorridor) -> RiskCoord {
    normalize_harmful(
        hlr_m_per_h,
        corridor.hlr_safe,
        corridor.hlr_gold,
        corridor.hlr_hard,
    )
}

/// Compute habitat risk from ramp acceleration (g-equivalent).
pub fn compute_r_habitat(accel_g: f64, corridor: HabitatCorridor) -> RiskCoord {
    normalize_harmful(
        accel_g,
        corridor.accel_safe,
        corridor.accel_gold,
        corridor.accel_hard,
    )
}

/// Compute energy efficiency risk from specific energy kWh/kg removed.
/// Higher specific energy is worse; lower is better.
pub fn compute_r_energy_eff(
    kwh_per_kg: f64,
    corridor: EnergyEfficiencyCorridor,
) -> RiskCoord {
    normalize_harmful(
        kwh_per_kg,
        corridor.kwh_per_kg_safe,
        corridor.kwh_per_kg_gold,
        corridor.kwh_per_kg_hard,
    )
}

/// Compute pathogen risk from a normalized pathogen index.
pub fn compute_r_pathogen(index: f64, corridor: PathogenCorridor) -> RiskCoord {
    normalize_harmful(index, corridor.index_safe, corridor.index_gold, corridor.index_hard)
}

/// Compute biodiversity risk from connectivity, complexity, and colonization metrics.
/// These metrics are beneficial-increasing; low values are high risk.
pub fn compute_r_biodiversity(
    conn: f64,
    comp: f64,
    colon: f64,
    corridor: BiodiversityCorridor,
) -> RiskCoord {
    let r_conn = normalize_beneficial(
        conn,
        corridor.conn_safe,
        corridor.conn_gold,
        corridor.conn_hard,
    );
    let r_comp = normalize_beneficial(
        comp,
        corridor.comp_safe,
        corridor.comp_gold,
        corridor.comp_hard,
    );
    let r_colon = normalize_beneficial(
        colon,
        corridor.colon_safe,
        corridor.colon_gold,
        corridor.colon_hard,
    );
    let r = r_conn.value.max(r_comp.value).max(r_colon.value);
    RiskCoord::new(r)
}

/// Compute rsigma uncertainty risk from a composite sigma index.
pub fn compute_r_sigma(sigma_index: f64, corridor: SigmaCorridor) -> RiskCoord {
    normalize_harmful(
        sigma_index,
        corridor.sigma_safe,
        corridor.sigma_gold,
        corridor.sigma_hard,
    )
}

/// TurbineShard represents one diagnostic window for PHX-CANAL-NODE-WL-01.
/// It binds raw telemetry and precomputed risk coordinates to the core Lyapunov logic.
#[derive(Clone, Debug)]
pub struct TurbineShard {
    pub shard_id: String,
    pub node_id: String,
    pub region: String,
    pub t_start: SystemTime,
    pub t_end: SystemTime,
    pub risk_turbine: TurbineRiskVector,
    pub vt: Residual,
    pub ker_window: Option<KerWindow>,
}

impl TurbineShard {
    pub fn duration(&self) -> Option<Duration> {
        self.t_end.duration_since(self.t_start).ok()
    }
}

/// TurbineSafeController is a non-actuating controller that only proposes diagnostic steps.
/// It implements SafeController but its Actuation type is a purely descriptive struct.
#[derive(Clone, Debug)]
pub struct TurbineActuationProposal {
    pub ramp_profile_id: String,
    pub target_discharge_m3s: f64,
    pub expected_risk: TurbineRiskVector,
}

#[derive(Clone, Debug)]
pub struct TurbineSafeController {
    pub weights: LyapunovWeights,
}

impl TurbineSafeController {
    pub fn new(weights: LyapunovWeights) -> Self {
        Self { weights }
    }

    pub fn evaluate_residual(&self, risk: TurbineRiskVector) -> Residual {
        let core = risk.into_core_vector();
        self.weights.evaluate(core)
    }
}

impl SafeController for TurbineSafeController {
    type Actuation = TurbineActuationProposal;

    fn propose_step(&self, now: SystemTime) -> (Self::Actuation, RiskVector) {
        // Example: diagnostic proposal with safe default discharge and conservative risk.
        let placeholder_risk = TurbineRiskVector {
            r_fish_shear: RiskCoord::new(0.1),
            r_habitat: RiskCoord::new(0.1),
            r_surcharge: RiskCoord::new(0.1),
            r_energy_eff: RiskCoord::new(0.2),
            r_pathogen: RiskCoord::new(0.1),
            r_biodiversity: RiskCoord::new(0.1),
            r_sigma: RiskCoord::new(0.05),
        };
        let act = TurbineActuationProposal {
            ramp_profile_id: format!("diagnostic-{}", now.duration_since(SystemTime::UNIX_EPOCH).unwrap_or(Duration::from_secs(0)).as_secs()),
            target_discharge_m3s: 0.0,
            expected_risk: placeholder_risk,
        };
        (act, placeholder_risk.into_core_vector())
    }
}

/// Verify Lyapunov residual monotonicity for a pair of diagnostic turbine steps.
/// SafeStepGate enforces V_{t+1} <= V_t and corridor membership.
pub fn verify_turbine_safestep(
    weights: LyapunovWeights,
    vt_current: Residual,
    risk_next: TurbineRiskVector,
) -> (Residual, SafeStepDecision) {
    let gate = SafeStepGate::new(weights, vt_current);
    let rv_next = risk_next.into_core_vector();
    gate.evaluate_next(rv_next)
}

/// Kani harness: formally verify that TurbineRiskVector always produces core RiskVector
/// with all coordinates in [0.0, 1.0] and that SafeStepGate enforces V_{t+1} <= V_t
/// when all RiskCoord values are clamped.
#[cfg(kani)]
mod kani_harnesses {
    use super::*;
    use kani::any;

    #[kani::proof]
    fn turbine_risk_clamping_and_residual_safety() {
        // Arbitrary raw risk values, unconstrained.
        let raw_fish: f64 = any();
        let raw_habitat: f64 = any();
        let raw_surcharge: f64 = any();
        let raw_energy: f64 = any();
        let raw_pathogen: f64 = any();
        let raw_biodiversity: f64 = any();
        let raw_sigma: f64 = any();

        // Construct clamped coordinates.
        let r_fish = RiskCoord::new(raw_fish);
        let r_habitat = RiskCoord::new(raw_habitat);
        let r_surcharge = RiskCoord::new(raw_surcharge);
        let r_energy = RiskCoord::new(raw_energy);
        let r_pathogen = RiskCoord::new(raw_pathogen);
        let r_biodiversity = RiskCoord::new(raw_biodiversity);
        let r_sigma = RiskCoord::new(raw_sigma);

        kani::assert!(r_fish.value >= 0.0 && r_fish.value <= 1.0);
        kani::assert!(r_habitat.value >= 0.0 && r_habitat.value <= 1.0);
        kani::assert!(r_surcharge.value >= 0.0 && r_surcharge.value <= 1.0);
        kani::assert!(r_energy.value >= 0.0 && r_energy.value <= 1.0);
        kani::assert!(r_pathogen.value >= 0.0 && r_pathogen.value <= 1.0);
        kani::assert!(r_biodiversity.value >= 0.0 && r_biodiversity.value <= 1.0);
        kani::assert!(r_sigma.value >= 0.0 && r_sigma.value <= 1.0);

        let turbine_risk = TurbineRiskVector {
            r_fish_shear: r_fish,
            r_habitat,
            r_surcharge,
            r_energy_eff: r_energy,
            r_pathogen,
            r_biodiversity,
            r_sigma,
        };

        let core_vec = turbine_risk.into_core_vector();
        // Check that all coordinates are within bounds.
        kani::assert!(core_vec.renergy.value >= 0.0 && core_vec.renergy.value <= 1.0);
        kani::assert!(core_vec.rhydraulics.value >= 0.0 && core_vec.rhydraulics.value <= 1.0);
        kani::assert!(core_vec.rbiology.value >= 0.0 && core_vec.rbiology.value <= 1.0);
        kani::assert!(core_vec.rcarbon.value >= 0.0 && core_vec.rcarbon.value <= 1.0);
        kani::assert!(core_vec.rmaterials.value >= 0.0 && core_vec.rmaterials.value <= 1.0);
        kani::assert!(core_vec.rbiodiversity.value >= 0.0 && core_vec.rbiodiversity.value <= 1.0);
        kani::assert!(core_vec.rsigma.value >= 0.0 && core_vec.rsigma.value <= 1.0);

        // Lyapunov residual non-negativity.
        let weights = LyapunovWeights::default_carbon_negative();
        let res = weights.evaluate(core_vec);
        kani::assert!(res.vt >= 0.0);

        // SafeStepGate monotonicity: if the next residual is <= current, Accept else Reject.
        let gate = SafeStepGate::new(weights, res);
        let (next_res, decision) = gate.evaluate_next(core_vec);

        if next_res.vt <= res.vt && core_vec.max_coord() <= 1.0 {
            kani::assert!(decision == SafeStepDecision::Accept);
        } else {
            kani::assert!(decision == SafeStepDecision::Reject);
        }
    }
}
