// File: crates/cyboquatic-ecosafety-core/src/lib.rs
// Target repo: github.com/Doctor0Evil/eco_restoration_shard
// Purpose: Shared ecosafety spine + blast-radius scoring for Cyboquatic industrial machinery.
// This crate is strictly non‑actuating and intended as a core library that other
// ecosafety crates and governance stacks can build upon.

//! Cyboquatic ecosafety core primitives.
//!
//! This crate provides non‑actuating ecosafety primitives and diagnostics for
//! Cyboquatic nodes and industrial machinery, aligned with the KER
//! (Knowledge, Eco‑impact, Risk‑of‑harm) framework and Lyapunov‑style residual
//! invariants used in EcoNet and eco_restoration_shard.
//!
//! It defines:
//! - Normalised risk coordinates and plane‑labelled risk vectors.
//! - Lyapunov‑style residual computation over risk planes.
//! - Corridor bands and blast‑radius classes for ecosafety corridors.
//! - Rolling KER window estimation over diagnostic histories.
//! - A safestep gating function that classifies candidate steps as Ok/Derate/Stop.
//!
//! This crate is strictly non‑actuating: it does not open device handles or
//! perform I/O beyond in‑memory computations. Higher‑level crates are
//! responsible for connecting these diagnostics to ALN, SQL shards, and
//! governance logic.

#![forbid(unsafe_code)]
#![deny(missing_docs)]
#![deny(clippy::all)]

use core::fmt;

/// Normalized risk coordinate in [0,1].
///
/// Risk coordinates encode unitless, corridor‑bounded measures on planes such as
/// energy, hydraulics, biology, carbon, materials, biodiversity, and uncertainty.
/// Values are clamped to the closed interval [0,1].
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct RiskCoord(pub f32);

impl RiskCoord {
    /// Creates a new clamped risk coordinate.
    ///
    /// Any value below 0 is mapped to 0; any value above 1 is mapped to 1.
    pub fn new_clamped(raw: f32) -> Self {
        Self(raw.max(0.0).min(1.0))
    }

    /// Zero risk.
    pub fn zero() -> Self {
        Self(0.0)
    }

    /// Maximum risk.
    pub fn one() -> Self {
        Self(1.0)
    }

    /// Returns the underlying scalar.
    pub fn value(self) -> f32 {
        self.0
    }
}

/// Named risk plane identifier.
///
/// Planes correspond to corridor‑governed dimensions that together form
/// a risk vector for a node or subsystem.
#[derive(Clone, Copy, Debug, PartialEq, Eq, Hash)]
pub enum RiskPlane {
    /// Energy plane (e.g., load, usage, intensity).
    Energy,
    /// Hydraulics plane (e.g., surcharge and hydraulic stress).
    Hydraulics,
    /// Biology plane (e.g., pathogen or bioload risk).
    Biology,
    /// Carbon plane (e.g., lifecycle CO₂e impact).
    Carbon,
    /// Materials plane (e.g., ecotoxicity, durability).
    Materials,
    /// Biodiversity plane (e.g., habitat impact).
    Biodiversity,
    /// Uncertainty plane (e.g., calibration, variance).
    Uncertainty,
    /// Custom, extension slot delegated to upstream corridor governance.
    Custom(u8),
}

/// One entry in the risk vector.
///
/// Each entry is a plane + normalised coordinate pair.
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct RiskEntry {
    /// Risk plane.
    pub plane: RiskPlane,
    /// Normalised coordinate on that plane.
    pub coord: RiskCoord,
}

/// Full risk vector for a node or subsystem at a timestep.
///
/// Entries are expected to have unique planes per vector; callers are
/// responsible for avoiding duplicates.
#[derive(Clone, Debug, PartialEq)]
pub struct RiskVector {
    /// Plane‑labelled risk entries.
    pub entries: Vec<RiskEntry>,
}

impl RiskVector {
    /// Constructs a new risk vector from entries.
    pub fn new(entries: Vec<RiskEntry>) -> Self {
        Self { entries }
    }

    /// Returns the maximum coordinate across all planes.
    pub fn max_coord(&self) -> RiskCoord {
        let max_val = self
            .entries
            .iter()
            .fold(0.0_f32, |acc, e| acc.max(e.coord.value()));
        RiskCoord(max_val)
    }

    /// Returns the coordinate for the given plane, if present.
    pub fn plane_coord(&self, plane: RiskPlane) -> Option<RiskCoord> {
        self.entries
            .iter()
            .find(|e| e.plane == plane)
            .map(|e| e.coord)
    }
}

/// Quadratic Lyapunov‑style residual V_t = Σ_j w_j * r_j^2.
///
/// This is a scalar summary of the risk vector under a given set of
/// weights, used as a monotone surrogate for ecosafety and stability.
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct Residual(pub f32);

impl Residual {
    /// Zero residual.
    pub fn zero() -> Self {
        Self(0.0)
    }

    /// Returns the underlying scalar.
    pub fn value(self) -> f32 {
        self.0
    }
}

/// Weights for each risk plane in the residual.
///
/// These weights encode how much each plane contributes to the global
/// residual. They are corridor‑governed and should not be adjusted
/// without a corresponding governance decision.
#[derive(Clone, Debug, PartialEq)]
pub struct LyapunovWeights {
    /// Energy plane weight.
    pub w_energy: f32,
    /// Hydraulics plane weight.
    pub w_hydraulics: f32,
    /// Biology plane weight.
    pub w_biology: f32,
    /// Carbon plane weight.
    pub w_carbon: f32,
    /// Materials plane weight.
    pub w_materials: f32,
    /// Biodiversity plane weight.
    pub w_biodiversity: f32,
    /// Uncertainty plane weight.
    pub w_uncertainty: f32,
    /// Custom plane weight.
    pub w_custom: f32,
}

impl LyapunovWeights {
    /// Default ecosafety weights.
    ///
    /// Emphasises long‑horizon planes (carbon, biodiversity, materials,
    /// uncertainty) while retaining substantial weight on energy and hydraulics.
    pub fn default_ecosafety() -> Self {
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

    /// Returns the weight for the given plane.
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

/// Computes a Lyapunov‑style residual for a risk vector and weights.
pub fn compute_residual(rx: &RiskVector, w: &LyapunovWeights) -> Residual {
    let mut acc = 0.0_f32;
    for entry in &rx.entries {
        let r = entry.coord.value();
        let weight = w.weight_for(entry.plane);
        acc += weight * r * r;
    }
    Residual(acc)
}

/// Corridor definition for a single plane.
///
/// `safe_max <= gold_max <= hard_max <= 1.0` holds by construction.
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct CorridorBands {
    /// Upper bound for the safe band (inclusive).
    pub safe_max: f32,
    /// Upper bound for the gold band (inclusive).
    pub gold_max: f32,
    /// Upper bound for the hard band (inclusive).
    pub hard_max: f32,
}

impl CorridorBands {
    /// Creates a new set of corridor bands.
    ///
    /// Panics if the invariants are violated.
    pub fn new(safe_max: f32, gold_max: f32, hard_max: f32) -> Self {
        assert!(0.0 <= safe_max);
        assert!(safe_max <= gold_max);
        assert!(gold_max <= hard_max);
        assert!(hard_max <= 1.0);
        Self {
            safe_max,
            gold_max,
            hard_max,
        }
    }

    /// Classifies a coordinate into a band.
    pub fn band(&self, coord: RiskCoord) -> CorridorBand {
        let v = coord.value();
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

/// Corridor classification band.
#[derive(Clone, Copy, Debug, PartialEq)]
pub enum CorridorBand {
    /// Inside the safe corridor.
    Safe,
    /// Inside the preferred (gold) corridor.
    Gold,
    /// Inside the hard corridor, near breach.
    NearBreach,
    /// Outside all corridors.
    OutOfCorridor,
}

/// Blast‑radius classification for a node or workload.
///
/// These labels summarise how widely a deviation may propagate.
#[derive(Clone, Copy, Debug, PartialEq)]
pub enum BlastRadiusClass {
    /// Localised, low‑risk changes; all coordinates in safe/gold, low residual.
    LocalLow,
    /// Localised but moderate risk; some near‑breach coordinates, residual below threshold.
    LocalModerate,
    /// Basin‑scale risk; residual elevated but no out‑of‑corridor planes.
    Basin,
    /// Constellation‑scale risk; any out‑of‑corridor plane or high uncertainty.
    Constellation,
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

/// KER metrics over a diagnostic window.
///
/// K is approximated from fraction of safe steps, E from residual
/// complement, and R from maximum observed risk coordinate.
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct KerTriad {
    /// Knowledge factor.
    pub k_knowledge: f32,
    /// Eco‑impact factor.
    pub e_ecoimpact: f32,
    /// Risk‑of‑harm factor.
    pub r_risk_of_harm: f32,
}

impl KerTriad {
    /// Reference triad for research‑band ecosafety envelopes.
    pub fn research_band() -> Self {
        Self {
            k_knowledge: 0.94,
            e_ecoimpact: 0.90,
            r_risk_of_harm: 0.13,
        }
    }

    /// Reference triad for production gating envelopes.
    pub fn production_gate() -> Self {
        Self {
            k_knowledge: 0.90,
            e_ecoimpact: 0.90,
            r_risk_of_harm: 0.13,
        }
    }
}

/// Rolling KER window state.
///
/// This approximates K/E/R from a history of diagnostic steps.
#[derive(Clone, Debug, PartialEq)]
pub struct KerWindow {
    steps_total: u64,
    steps_safe: u64,
    max_coord: f32,
}

impl KerWindow {
    /// Creates an empty KER window.
    pub fn new() -> Self {
        Self {
            steps_total: 0,
            steps_safe: 0,
            max_coord: 0.0,
        }
    }

    /// Updates the window with one step outcome.
    ///
    /// `residual_ok` indicates whether the step was within residual bounds.
    pub fn update(&mut self, residual_ok: bool, rx: &RiskVector) {
        self.steps_total = self.steps_total.saturating_add(1);
        if residual_ok {
            self.steps_safe = self.steps_safe.saturating_add(1);
        }
        let max = rx.max_coord().value();
        if max > self.max_coord {
            self.max_coord = max;
        }
    }

    /// Returns the current KER triad estimate.
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
        let e = (1.0 - r).clamp(0.0, 1.0);
        KerTriad {
            k_knowledge: k.clamp(0.0, 1.0),
            e_ecoimpact: e,
            r_risk_of_harm: r.clamp(0.0, 1.0),
        }
    }
}

/// Decision for a proposed step under ecosafety gating.
///
/// This is a diagnostic label only; applying steps is the
/// responsibility of an external controller.
#[derive(Clone, Copy, Debug, PartialEq)]
pub enum SafeStepDecision {
    /// Step is acceptable under current corridors.
    Ok,
    /// Step should be derated or softened.
    Derate,
    /// Step should be stopped.
    Stop,
}

/// Configuration for safestep gating.
///
/// `vt_max` and `vt_non_increase_eps` encode Lyapunov‑style constraints;
/// `allow_near_breach` controls whether near‑breach coordinates are allowed.
#[derive(Clone, Debug, PartialEq)]
pub struct SafeStepConfig {
    /// Maximum allowed residual.
    pub vt_max: f32,
    /// Allowed residual increase tolerance.
    pub vt_non_increase_eps: f32,
    /// Whether near‑breach coordinates are allowed.
    pub allow_near_breach: bool,
}

impl SafeStepConfig {
    /// Conservative default configuration.
    pub fn default_conservative() -> Self {
        Self {
            vt_max: 1.0,
            vt_non_increase_eps: 1.0e-4,
            allow_near_breach: false,
        }
    }
}

/// Result of evaluating a safestep contract.
///
/// Contains both the decision and a blast‑radius class.
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct SafeStepResult {
    /// Step decision.
    pub decision: SafeStepDecision,
    /// Blast‑radius classification.
    pub blast_radius: BlastRadiusClass,
}

/// Evaluates a safestep contract between previous and candidate risk states.
///
/// This function is purely diagnostic: it classifies the candidate step
/// but does not apply it.
pub fn evaluate_safestep(
    prev_rx: &RiskVector,
    prev_vt: Residual,
    candidate_rx: &RiskVector,
    candidate_vt: Residual,
    weights: &LyapunovWeights,
    config: &SafeStepConfig,
    per_plane_corridors: &[(RiskPlane, CorridorBands)],
) -> SafeStepResult {
    let _ = weights;

    let mut max_band = CorridorBand::Safe;
    let mut has_out_of_corridor = false;
    let mut high_uncertainty = false;

    for (plane, bands) in per_plane_corridors {
        if let Some(coord) = candidate_rx.plane_coord(*plane) {
            let band = bands.band(coord);
            if band_rank(band) > band_rank(max_band) {
                max_band = band;
            }
            if band == CorridorBand::OutOfCorridor {
                has_out_of_corridor = true;
            }
            if *plane == RiskPlane::Uncertainty && coord.value() > bands.gold_max {
                high_uncertainty = true;
            }
        }
    }

    let vt_ok = candidate_vt.value() <= config.vt_max
        && candidate_vt.value() <= prev_vt.value() + config.vt_non_increase_eps;

    let decision = if !vt_ok || has_out_of_corridor || high_uncertainty {
        SafeStepDecision::Stop
    } else if !config.allow_near_breach && max_band == CorridorBand::NearBreach {
        SafeStepDecision::Derate
    } else {
        SafeStepDecision::Ok
    };

    let blast_radius = if has_out_of_corridor || high_uncertainty {
        BlastRadiusClass::Constellation
    } else if candidate_vt.value() > 0.8 {
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

fn band_rank(band: CorridorBand) -> u8 {
    match band {
        CorridorBand::Safe => 0,
        CorridorBand::Gold => 1,
        CorridorBand::NearBreach => 2,
        CorridorBand::OutOfCorridor => 3,
    }
}

/// Trait implemented by any controller that wants ecosafety gating.
///
/// Controllers must provide a proposed actuation and associated risk vector.
/// This crate does not define actuation semantics; it only encapsulates
/// diagnostics around such controllers.
pub trait SafeController {
    /// Actuation type proposed by the controller.
    type Actuation;

    /// Returns the proposed actuation and its associated risk vector.
    fn propose_step(&self) -> (Self::Actuation, RiskVector);

    /// Applies an accepted actuation to the controller state.
    fn apply_step(&mut self, act: Self::Actuation);
}

/// Wrapper that enforces ecosafety semantics around a controller.
///
/// This type evaluates each proposed step, updates a KER window, and
/// only applies the step when the decision is `Ok`.
pub struct SafeControllerWrapper<C: SafeController> {
    /// Inner controller.
    pub inner: C,
    /// Residual weights.
    pub weights: LyapunovWeights,
    /// Gating configuration.
    pub config: SafeStepConfig,
    /// Per‑plane corridors.
    pub corridors: Vec<(RiskPlane, CorridorBands)>,
    /// Rolling KER window.
    pub ker_window: KerWindow,
    /// Last residual value.
    pub last_residual: Residual,
}

impl<C: SafeController> SafeControllerWrapper<C> {
    /// Creates a new wrapper with default weights and configuration.
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

    /// Evaluates one control step: gating, blast‑radius scoring, and KER update.
    ///
    /// This method is non‑actuating unless the decision is `Ok`, in which case
    /// it delegates to the inner controller's `apply_step`.
    pub fn step(&mut self) -> (SafeStepResult, KerTriad) {
        let (act, rx) = self.inner.propose_step();
        let vt_candidate = compute_residual(&rx, &self.weights);

        let result = evaluate_safestep(
            &rx,
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

#[cfg(test)]
mod tests {
    use super::*;

    #[derive(Clone)]
    struct DummyController {
        energy: f32,
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
            (RiskPlane::Energy, CorridorBands::new(0.3, 0.6, 0.9)),
            (RiskPlane::Carbon, CorridorBands::new(0.2, 0.4, 0.8)),
            (RiskPlane::Materials, CorridorBands::new(0.2, 0.5, 0.9)),
            (RiskPlane::Uncertainty, CorridorBands::new(0.2, 0.5, 0.8)),
        ];

        let mut wrapper = SafeControllerWrapper::new(controller, corridors);

        for _ in 0..10 {
            let (result, triad) = wrapper.step();
            assert_ne!(result.decision, SafeStepDecision::Stop);
            assert!((0.0..=1.0).contains(&triad.k_knowledge));
            assert!((0.0..=1.0).contains(&triad.e_ecoimpact));
            assert!((0.0..=1.0).contains(&triad.r_risk_of_harm));
        }
    }
}
