// File: crates/cyboquatic-ecosafety-core/src/lib.rs
// Target repo: github.com/Doctor0Evil/eco_restoration_shard (new crate in /crates)
// Purpose: Shared ecosafety spine + blast-radius scoring for Cyboquatic industrial machinery
// Language stack: Rust core; ALN/SQLite/other stacks sit on top and never widen this grammar.

#![forbid(unsafe_code)]
#![deny(clippy::all)]

use std::fmt;

/// Normalized risk coordinate in [0,1].
/// Planes: energy, hydraulics, biology, carbon, materials, biodiversity, uncertainty, etc.
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct RiskCoord(pub f32);

impl RiskCoord {
    pub fn new_clamped(raw: f32) -> Self {
        Self(raw.max(0.0).min(1.0))
    }

    pub fn zero() -> Self {
        Self(0.0)
    }

    pub fn one() -> Self {
        Self(1.0)
    }
}

/// Named risk plane identifier.
#[derive(Clone, Copy, Debug, PartialEq, Eq, Hash)]
pub enum RiskPlane {
    Energy,
    Hydraulics,
    Biology,
    Carbon,
    Materials,
    Biodiversity,
    Uncertainty, // r_sigma / r_calib
    Custom(u8),  // extension slot, must be corridor-governed upstream
}

/// One entry in the risk vector.
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct RiskEntry {
    pub plane: RiskPlane,
    pub coord: RiskCoord,
}

/// Full risk vector for a node or subsystem at a timestep.
#[derive(Clone, Debug, PartialEq)]
pub struct RiskVector {
    pub entries: Vec<RiskEntry>,
}

impl RiskVector {
    pub fn new(entries: Vec<RiskEntry>) -> Self {
        Self { entries }
    }

    pub fn max_coord(&self) -> RiskCoord {
        self.entries
            .iter()
            .map(|e| e.coord.0)
            .fold(0.0_f32, |acc, v| acc.max(v))
            .pipe(RiskCoord)
    }

    pub fn get_plane(&self, plane: RiskPlane) -> Option<RiskCoord> {
        self.entries
            .iter()
            .find(|e| e.plane == plane)
            .map(|e| e.coord)
    }
}

trait Pipe: Sized {
    fn pipe<F, T>(self, f: F) -> T
    where
        F: FnOnce(Self) -> T;
}

impl<T> Pipe for T {
    fn pipe<F, U>(self, f: F) -> U
    where
        F: FnOnce(Self) -> U,
    {
        f(self)
    }
}

/// Quadratic Lyapunov residual V_t = Σ_j w_j * r_j^2.
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct Residual(pub f32);

impl Residual {
    pub fn zero() -> Self {
        Self(0.0)
    }
}

/// Weights for each risk plane in the residual.
#[derive(Clone, Debug, PartialEq)]
pub struct LyapunovWeights {
    pub w_energy: f32,
    pub w_hydraulics: f32,
    pub w_biology: f32,
    pub w_carbon: f32,
    pub w_materials: f32,
    pub w_biodiversity: f32,
    pub w_uncertainty: f32,
    pub w_custom: f32,
}

impl LyapunovWeights {
    pub fn default_ecosafety() -> Self {
        // Emphasize long-horizon planes (carbon, biodiversity, materials, uncertainty)
        // while keeping energy/hydraulics strongly represented.
        Self {
            w_energy: 1.0,
            w_hydraulics: 1.2,
            w_biology: 1.3,
            w_carbon: 1.4,
            w_materials: 1.3,
            w_biodiversity: 1.4,
            w_uncertainty: 1.1,
            w_custom: 1.0,
        }
    }

    pub fn weight_for(&self, plane: RiskPlane) -> f32 {
        match plane {
            RiskPlane::Energy => self.w_energy,
            RiskPlane::Hydraulics => self.w_hydraulics,
            RiskPlane::Biology => self.w_biology,
            RiskPlane::Carbon => self.w_carbon,
            RiskPlane::Materials => self.w_materials,
            RiskPlane::Biodiversity => self.w_biodiversity,
            RiskPlane::Uncertainty => self.w_uncertainty,
            RiskPlane::Custom(_) => self.w_custom,
        }
    }
}

/// Compute Lyapunov residual for a risk vector and weights.
pub fn compute_residual(rx: &RiskVector, w: &LyapunovWeights) -> Residual {
    let mut acc = 0.0_f32;
    for entry in &rx.entries {
        let r = entry.coord.0;
        let weight = w.weight_for(entry.plane);
        acc += weight * r * r;
    }
    Residual(acc)
}

/// Corridor definition for a single plane: safe, gold, hard.
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct CorridorBands {
    pub safe_max: f32,
    pub gold_max: f32,
    pub hard_max: f32,
}

impl CorridorBands {
    pub fn new(safe_max: f32, gold_max: f32, hard_max: f32) -> Self {
        assert!(0.0 <= safe_max && safe_max <= gold_max && gold_max <= hard_max && hard_max <= 1.0);
        Self {
            safe_max,
            gold_max,
            hard_max,
        }
    }

    pub fn band(&self, coord: RiskCoord) -> CorridorBand {
        let v = coord.0;
        if v <= self.safe_max {
            CorridorBand::Safe
        } else if v <= self.gold_max {
            CorridorBand::Gold
        } else if v <= self.hard_max {
            CorridorBand::NearBreach
        } else {
            CorridorBand::OutOfCorridor
        }
    }
}

#[derive(Clone, Copy, Debug, PartialEq)]
pub enum CorridorBand {
    Safe,
    Gold,
    NearBreach,
    OutOfCorridor,
}

/// Blast-radius classification for a node or workload.
#[derive(Clone, Copy, Debug, PartialEq)]
pub enum BlastRadiusClass {
    /// Localized, low-risk: all coordinates in safe/gold, low residual.
    LocalLow,
    /// Localized but moderate: some near-breach coordinates, residual below threshold.
    LocalModerate,
    /// Basin-scale: high residual but bounded, no out-of-corridor planes.
    Basin,
    /// Constellation-scale: any out-of-corridor plane or high uncertainty.
    Constellation,
}

/// KER metrics over a window.
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct KerTriad {
    pub k_knowledge: f32,
    pub e_ecoimpact: f32,
    pub r_risk_of_harm: f32,
}

impl KerTriad {
    pub fn research_band() -> Self {
        Self {
            k_knowledge: 0.94,
            e_ecoimpact: 0.90,
            r_risk_of_harm: 0.13,
        }
    }

    pub fn production_gate() -> Self {
        Self {
            k_knowledge: 0.90,
            e_ecoimpact: 0.90,
            r_risk_of_harm: 0.13,
        }
    }
}

/// Rolling KER window state.
#[derive(Clone, Debug, PartialEq)]
pub struct KerWindow {
    pub steps_total: u64,
    pub steps_safe: u64,
    pub max_coord: f32,
}

impl KerWindow {
    pub fn new() -> Self {
        Self {
            steps_total: 0,
            steps_safe: 0,
            max_coord: 0.0,
        }
    }

    pub fn update(&mut self, residual_ok: bool, rx: &RiskVector) {
        self.steps_total += 1;
        if residual_ok {
            self.steps_safe += 1;
        }
        let max = rx.max_coord().0;
        if max > self.max_coord {
            self.max_coord = max;
        }
    }

    pub fn triad(&self) -> KerTriad {
        if self.steps_total == 0 {
            return KerTriad {
                k_knowledge: 0.0,
                e_ecoimpact: 0.0,
                r_risk_of_harm: 1.0,
            };
        }
        let k = self.steps_safe as f32 / self.steps_total as f32;
        let r = self.max_coord;
        let e = 1.0 - r;
        KerTriad {
            k_knowledge: k,
            e_ecoimpact: e,
            r_risk_of_harm: r,
        }
    }
}

/// Decision for a proposed actuation step.
#[derive(Clone, Copy, Debug, PartialEq)]
pub enum SafeStepDecision {
    Ok,
    Derate,
    Stop,
}

/// Configuration for safestep gating.
#[derive(Clone, Debug, PartialEq)]
pub struct SafeStepConfig {
    pub vt_max: f32,
    pub vt_non_increase_eps: f32,
    pub allow_near_breach: bool,
}

impl SafeStepConfig {
    pub fn default_conservative() -> Self {
        Self {
            vt_max: 1.0,
            vt_non_increase_eps: 1.0e-4,
            allow_near_breach: false,
        }
    }
}

/// Result of evaluating safestep: includes decision and blast-radius.
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct SafeStepResult {
    pub decision: SafeStepDecision,
    pub blast_radius: BlastRadiusClass,
}

/// Evaluate safestep contract between previous and candidate risk states.
pub fn evaluate_safestep(
    prev_rx: &RiskVector,
    prev_vt: Residual,
    candidate_rx: &RiskVector,
    candidate_vt: Residual,
    weights: &LyapunovWeights,
    config: &SafeStepConfig,
    per_plane_corridors: &[(RiskPlane, CorridorBands)],
) -> SafeStepResult {
    let _ = weights; // weights used via vt inputs; kept for future extensions.

    let mut max_band = CorridorBand::Safe;
    let mut has_out_of_corridor = false;
    let mut high_uncertainty = false;

    for (plane, bands) in per_plane_corridors {
        if let Some(coord) = candidate_rx.get_plane(*plane) {
            let band = bands.band(coord);
            if band as u8 > max_band as u8 {
                max_band = band;
            }
            if band == CorridorBand::OutOfCorridor {
                has_out_of_corridor = true;
            }
            if *plane == RiskPlane::Uncertainty && coord.0 > bands.gold_max {
                high_uncertainty = true;
            }
        }
    }

    let vt_ok = candidate_vt.0 <= config.vt_max
        && candidate_vt.0 <= prev_vt.0 + config.vt_non_increase_eps;

    let decision = if !vt_ok || has_out_of_corridor || high_uncertainty {
        SafeStepDecision::Stop
    } else if !config.allow_near_breach && max_band == CorridorBand::NearBreach {
        SafeStepDecision::Derate
    } else {
        SafeStepDecision::Ok
    };

    let blast_radius = if has_out_of_corridor || high_uncertainty {
        BlastRadiusClass::Constellation
    } else if candidate_vt.0 > 0.8 {
        BlastRadiusClass::Basin
    } else if max_band == CorridorBand::NearBreach {
        BlastRadiusClass::LocalModerate
    } else {
        BlastRadiusClass::LocalLow
    };

    SafeStepResult {
        decision,
        blast_radius,
    }
}

/// Trait implemented by any Cyboquatic controller that wants ecosafety gating.
/// Controllers must provide both a proposed actuation and a RiskVector.
pub trait SafeController {
    type Actuation;

    fn propose_step(&self) -> (Self::Actuation, RiskVector);

    fn apply_step(&mut self, act: Self::Actuation);
}

/// Wrapper that enforces ecosafety semantics around a controller.
pub struct SafeControllerWrapper<C: SafeController> {
    pub inner: C,
    pub weights: LyapunovWeights,
    pub config: SafeStepConfig,
    pub corridors: Vec<(RiskPlane, CorridorBands)>,
    pub ker_window: KerWindow,
    pub last_residual: Residual,
}

impl<C: SafeController> SafeControllerWrapper<C> {
    pub fn new(inner: C, corridors: Vec<(RiskPlane, CorridorBands)>) -> Self {
        let weights = LyapunovWeights::default_ecosafety();
        Self {
            inner,
            weights,
            config: SafeStepConfig::default_conservative(),
            corridors,
            ker_window: KerWindow::new(),
            last_residual: Residual::zero(),
        }
    }

    /// Evaluate one control step: gating, blast-radius scoring, and KER update.
    pub fn step(&mut self) -> (SafeStepResult, KerTriad) {
        let (act, rx) = self.inner.propose_step();
        let vt_candidate = compute_residual(&rx, &self.weights);

        let result = evaluate_safestep(
            &rx,             // previous rx is conservatively approximated by current for now
            self.last_residual,
            &rx,
            vt_candidate,
            &self.weights,
            &self.config,
            &self.corridors,
        );

        let residual_ok = matches!(result.decision, SafeStepDecision::Ok | SafeStepDecision::Derate);
        self.ker_window.update(residual_ok, &rx);

        if matches!(result.decision, SafeStepDecision::Ok) {
            self.inner.apply_step(act);
            self.last_residual = vt_candidate;
        }

        let triad = self.ker_window.triad();
        (result, triad)
    }
}

impl fmt::Display for BlastRadiusClass {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        let s = match self {
            BlastRadiusClass::LocalLow => "local_low",
            BlastRadiusClass::LocalModerate => "local_moderate",
            BlastRadiusClass::Basin => "basin",
            BlastRadiusClass::Constellation => "constellation",
        };
        write!(f, "{s}")
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[derive(Clone)]
    struct DummyController {
        pub energy: f32,
    }

    impl SafeController for DummyController {
        type Actuation = f32;

        fn propose_step(&self) -> (Self::Actuation, RiskVector) {
            let actuation = -0.1;
            let energy = (self.energy + actuation).max(0.0);
            let entries = vec![
                RiskEntry {
                    plane: RiskPlane::Energy,
                    coord: RiskCoord::new_clamped(energy),
                },
                RiskEntry {
                    plane: RiskPlane::Carbon,
                    coord: RiskCoord::new_clamped(0.2),
                },
                RiskEntry {
                    plane: RiskPlane::Materials,
                    coord: RiskCoord::new_clamped(0.1),
                },
                RiskEntry {
                    plane: RiskPlane::Uncertainty,
                    coord: RiskCoord::new_clamped(0.1),
                },
            ];
            (actuation, RiskVector::new(entries))
        }

        fn apply_step(&mut self, act: Self::Actuation) {
            self.energy = (self.energy + act).max(0.0);
        }
    }

    #[test]
    fn basic_safestep_and_ker() {
        let controller = DummyController { energy: 0.3 };

        let corridors = vec![
            (
                RiskPlane::Energy,
                CorridorBands::new(0.3, 0.6, 0.9),
            ),
            (
                RiskPlane::Carbon,
                CorridorBands::new(0.2, 0.4, 0.8),
            ),
            (
                RiskPlane::Materials,
                CorridorBands::new(0.2, 0.5, 0.9),
            ),
            (
                RiskPlane::Uncertainty,
                CorridorBands::new(0.2, 0.5, 0.8),
            ),
        ];

        let mut wrapper = SafeControllerWrapper::new(controller, corridors);

        for _ in 0..10 {
            let (result, triad) = wrapper.step();
            assert!(!matches!(result.decision, SafeStepDecision::Stop));
            assert!(triad.k_knowledge >= 0.0 && triad.k_knowledge <= 1.0);
            assert!(triad.e_ecoimpact >= 0.0 && triad.e_ecoimpact <= 1.0);
            assert!(triad.r_risk_of_harm >= 0.0 && triad.r_risk_of_harm <= 1.0);
        }
    }
}
