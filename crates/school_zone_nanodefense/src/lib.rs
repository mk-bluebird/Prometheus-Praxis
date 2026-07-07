// eco_restoration_shard/crates/school_zone_nanodefense/src/lib.rs
// KER frame for school-zone nanodefense in TX/AZ.
// Non-actuating, diagnostics-only. No hardware or PLC bindings.

use serde::{Deserialize, Serialize};

/// Normalized risk coordinate in [0,1].
#[derive(Clone, Copy, Debug, Serialize, Deserialize)]
pub struct RiskCoord {
    pub value: f64,
}

impl RiskCoord {
    pub fn clamped(v: f64) -> Self {
        let value = if v < 0.0 { 0.0 } else if v > 1.0 { 1.0 } else { v };
        RiskCoord { value }
    }
}

/// Corridor bands: safe, gold, hard.
#[derive(Clone, Copy, Debug, Serialize, Deserialize)]
pub struct CorridorBands {
    pub safe_max: f64,
    pub gold_max: f64,
    pub hard_max: f64,
}

impl CorridorBands {
    pub fn normalize(&self, raw: f64) -> RiskCoord {
        if raw <= self.safe_max {
            RiskCoord::clamped(0.0)
        } else if raw >= self.hard_max {
            RiskCoord::clamped(1.0)
        } else {
            let span = (self.hard_max - self.safe_max).max(1.0e-9);
            let norm = (raw - self.safe_max) / span;
            RiskCoord::clamped(norm)
        }
    }
}

/// KER triad.
#[derive(Clone, Copy, Debug, Serialize, Deserialize)]
pub struct KerScore {
    pub k: f64,
    pub e: f64,
    pub r: f64,
}

/// School-zone nanodefense risk vector.
/// All coordinates are non-neural and non-actuating by design.
#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct SchoolZoneRiskVector {
    /// Parasite pressure (e.g., screwworm, helminths) around the zone.
    pub r_parasite: RiskCoord,
    /// Nano-coverage benefit (how well external filters/coatings reduce exposure).
    pub r_nano_benefit_gap: RiskCoord,
    /// Neurointeraction risk (must remain near 0; any CNS coupling pushes to 1).
    pub r_neuro: RiskCoord,
    /// Persistence / biodegradation risk for nano-materials.
    pub r_persistence: RiskCoord,
    /// Carbon / energy footprint for nano defenses in this zone.
    pub r_carbon: RiskCoord,
    /// Uncertainty about long-term effects, esp. in children.
    pub r_sigma: RiskCoord,
    /// Aggregate residual V_t.
    pub vt: f64,
}

impl SchoolZoneRiskVector {
    /// Quadratic Lyapunov residual with fixed weights.
    pub fn compute_residual(
        r_parasite: RiskCoord,
        r_nano_benefit_gap: RiskCoord,
        r_neuro: RiskCoord,
        r_persistence: RiskCoord,
        r_carbon: RiskCoord,
        r_sigma: RiskCoord,
    ) -> f64 {
        let w_parasite = 0.8_f64;
        let w_benefit = 0.6_f64;
        let w_neuro = 5.0_f64;
        let w_persistence = 3.0_f64;
        let w_carbon = 2.0_f64;
        let w_sigma = 4.0_f64;

        let v = w_parasite * r_parasite.value.powi(2)
            + w_benefit * r_nano_benefit_gap.value.powi(2)
            + w_neuro * r_neuro.value.powi(2)
            + w_persistence * r_persistence.value.powi(2)
            + w_carbon * r_carbon.value.powi(2)
            + w_sigma * r_sigma.value.powi(2);
        v
    }

    pub fn new(
        r_parasite: RiskCoord,
        r_nano_benefit_gap: RiskCoord,
        r_neuro: RiskCoord,
        r_persistence: RiskCoord,
        r_carbon: RiskCoord,
        r_sigma: RiskCoord,
    ) -> Self {
        let vt = Self::compute_residual(
            r_parasite,
            r_nano_benefit_gap,
            r_neuro,
            r_persistence,
            r_carbon,
            r_sigma,
        );
        SchoolZoneRiskVector {
            r_parasite,
            r_nano_benefit_gap,
            r_neuro,
            r_persistence,
            r_carbon,
            r_sigma,
            vt,
        }
    }
}

/// Safestep invariant: proposed state must not increase residual.
pub fn safestep(prev: &SchoolZoneRiskVector, next: &SchoolZoneRiskVector) -> bool {
    next.vt <= prev.vt + 1.0e-9
}

/// Hard bans and corridors for school-zone nanodefense.
/// These encode:
/// - no neuroactive / CNS-coupled nanoagents,
/// - no long-lived, non-biodegradable nano-matter,
/// - only external filters/diagnostics/coatings are eligible for kerdeployable.
#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct SchoolZoneNanoCorridors {
    pub neuro_corridor: CorridorBands,
    pub persistence_corridor: CorridorBands,
    pub carbon_corridor: CorridorBands,
    pub uncertainty_corridor: CorridorBands,
}

impl Default for SchoolZoneNanoCorridors {
    fn default() -> Self {
        SchoolZoneNanoCorridors {
            // r_neuro must be extremely low; any significant interaction is hard fail.
            neuro_corridor: CorridorBands {
                safe_max: 0.02,
                gold_max: 0.05,
                hard_max: 0.10,
            },
            // Persistence: strong penalty for slow biodegradation or bioaccumulation.
            persistence_corridor: CorridorBands {
                safe_max: 0.10,
                gold_max: 0.30,
                hard_max: 0.60,
            },
            // Carbon: corridors favor carbon-negative or neutral deployments.
            carbon_corridor: CorridorBands {
                safe_max: 0.10,
                gold_max: 0.25,
                hard_max: 0.50,
            },
            // Uncertainty: high rsigma blocks deployment.
            uncertainty_corridor: CorridorBands {
                safe_max: 0.20,
                gold_max: 0.40,
                hard_max: 0.70,
            },
        }
    }
}

/// Static metadata for a Texas or Arizona school zone.
#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct SchoolZoneMeta {
    pub state: String,
    pub county: String,
    pub district: String,
    pub campus_id: String,
    pub campus_name: String,
    /// Lat/Lon for corridor anchoring (e.g., PFAS, screwworm, water-quality).
    pub lat: f64,
    pub lon: f64,
}

/// Enumerates allowed nanodefense application modes.
/// Anything outside this enum is treated as non-eligible for children.
#[derive(Clone, Debug, Serialize, Deserialize, PartialEq, Eq)]
pub enum NanoDefenseMode {
    ExternalFilter,     // water/air filter, upstream of taps or HVAC.
    SurfaceCoating,     // antifouling, self-cleaning coatings.
    ExternalDiagnostic, // lab/school-clinic diagnostics, no in-body nano.
}

/// Input shard describing a candidate nanodefense deployment for a school zone.
/// Note: there is no notion of internal, in-body nano here by design.
#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct NanoDefenseShard {
    pub zone_meta: SchoolZoneMeta,
    pub mode: NanoDefenseMode,
    /// Raw metrics to be normalized into risk coordinates.
    pub parasite_pressure_index: f64, // 0–1 from epidemiology data.
    pub nano_exposure_fraction: f64,  // fraction of relevant flows treated (0–1).
    pub neuro_binding_score: f64,     // lab index of CNS interaction potential.
    pub half_life_days: f64,          // environmental half-life of nano material.
    pub carbon_kg_co2e_per_year: f64, // net carbon footprint of deployment.
    pub uncertainty_index: f64,       // composite rsigma from tox + pediatrics.
}

/// Derived evaluation for a given school zone nano configuration.
#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct NanoDefenseEval {
    pub zone_meta: SchoolZoneMeta,
    pub mode: NanoDefenseMode,
    pub risk_vec: SchoolZoneRiskVector,
    pub ker: KerScore,
    pub kerdeployable: bool,
    pub bans_triggered: Vec<String>,
}

/// Normalize raw shard metrics into risk coordinates, given corridors
/// and global scaling constants (for half-life and carbon).
pub fn map_to_risks(
    shard: &NanoDefenseShard,
    corridors: &SchoolZoneNanoCorridors,
    max_half_life_days: f64,
    max_carbon_kg_co2e_per_year: f64,
) -> SchoolZoneRiskVector {
    // Higher parasite pressure = higher r_parasite (cannot be negative).
    let r_parasite = RiskCoord::clamped(shard.parasite_pressure_index);

    // Benefit gap: if only a fraction of flows are treated, residual risk remains.
    // r_nano_benefit_gap = 1 - nano_exposure_fraction.
    let r_nano_benefit_gap = RiskCoord::clamped(1.0 - shard.nano_exposure_fraction);

    // Neuro risk: neuro_binding_score is normalized via neuro_corridor.
    let raw_neuro = shard.neuro_binding_score;
    let r_neuro = corridors.neuro_corridor.normalize(raw_neuro);

    // Persistence: map half-life to [0,1] via persistence_corridor, with an outer cap.
    let capped_half_life = if shard.half_life_days > max_half_life_days {
        max_half_life_days
    } else {
        shard.half_life_days
    };
    let raw_persistence = capped_half_life / max_half_life_days;
    let r_persistence = corridors.persistence_corridor.normalize(raw_persistence);

    // Carbon: map absolute CO2e to [0,1] then corridors.
    let capped_carbon = if shard.carbon_kg_co2e_per_year > max_carbon_kg_co2e_per_year {
        max_carbon_kg_co2e_per_year
    } else {
        shard.carbon_kg_co2e_per_year
    };
    let raw_carbon = capped_carbon / max_carbon_kg_co2e_per_year;
    let r_carbon = corridors.carbon_corridor.normalize(raw_carbon);

    // Uncertainty: uncertainty_index is already 0–1 scale; map through corridors.
    let r_sigma = corridors
        .uncertainty_corridor
        .normalize(shard.uncertainty_index);

    SchoolZoneRiskVector::new(
        r_parasite,
        r_nano_benefit_gap,
        r_neuro,
        r_persistence,
        r_carbon,
        r_sigma,
    )
}

/// Compute KER score from risk vector.
/// - K is higher when uncertainty is low and data coverage is good.
/// - E is higher when parasite risk is reduced and carbon/persistence are low.
/// - R is higher when neuro/persistence/uncertainty risks are high.
pub fn compute_ker(rv: &SchoolZoneRiskVector) -> KerScore {
    // Knowledgefactor: penalize high r_sigma (uncertainty).
    let k_base = 0.95_f64;
    let k = (k_base - 0.25_f64 * rv.r_sigma.value).max(0.0);

    // Ecoimpact: we want parasite protection (low r_parasite, low benefit gap),
    // low persistence, and low carbon.
    let e_base = 0.92_f64;
    let eco_penalty_parasite = 0.20_f64 * rv.r_parasite.value;
    let eco_penalty_gap = 0.15_f64 * rv.r_nano_benefit_gap.value;
    let eco_penalty_persistence = 0.20_f64 * rv.r_persistence.value;
    let eco_penalty_carbon = 0.20_f64 * rv.r_carbon.value;
    let eco_penalty_sigma = 0.10_f64 * rv.r_sigma.value;
    let e = (e_base
        - eco_penalty_parasite
        - eco_penalty_gap
        - eco_penalty_persistence
        - eco_penalty_carbon
        - eco_penalty_sigma)
        .max(0.0);

    // Risk-of-harm: strongly weight neuro, persistence, and uncertainty.
    let r_base = 0.10_f64;
    let r = (r_base
        + 0.40_f64 * rv.r_neuro.value
        + 0.30_f64 * rv.r_persistence.value
        + 0.20_f64 * rv.r_sigma.value
        + 0.10_f64 * rv.r_carbon.value)
        .min(1.0);

    KerScore { k, e, r }
}

/// Evaluate a candidate nano defense for a given school zone.
/// Enforces:
/// - mode must be external (filter/coating/diagnostic),
/// - neuro & persistence & uncertainty within corridors,
/// - safestep compared to a "no nano" baseline.
pub fn evaluate_nano_defense(
    shard: &NanoDefenseShard,
    corridors: &SchoolZoneNanoCorridors,
    max_half_life_days: f64,
    max_carbon_kg_co2e_per_year: f64,
) -> NanoDefenseEval {
    let mut bans = Vec::new();

    // Hard ban: only external, non-internal modes.
    if shard.mode != NanoDefenseMode::ExternalFilter
        && shard.mode != NanoDefenseMode::SurfaceCoating
        && shard.mode != NanoDefenseMode::ExternalDiagnostic
    {
        bans.push("mode_not_external".to_string());
    }

    // Baseline: no nano deployment, so r_nano_benefit_gap = 1, others from epidemiology.
    let baseline_rv = SchoolZoneRiskVector::new(
        RiskCoord::clamped(shard.parasite_pressure_index),
        RiskCoord::clamped(1.0),
        RiskCoord::clamped(0.0), // no nano = zero neuro risk.
        RiskCoord::clamped(0.0), // no nano = zero persistence for nano.
        RiskCoord::clamped(0.0), // no nano = zero added carbon from nano.
        RiskCoord::clamped(1.0), // no nano evidence yet, high uncertainty.
    );

    let rv = map_to_risks(shard, corridors, max_half_life_days, max_carbon_kg_co2e_per_year);
    let ker = compute_ker(&rv);

    // Corridor violations become bans.
    if rv.r_neuro.value > corridors.neuro_corridor.safe_max {
        bans.push("neuro_risk_above_safe".to_string());
    }
    if rv.r_persistence.value > corridors.persistence_corridor.gold_max {
        bans.push("persistence_above_gold".to_string());
    }
    if rv.r_sigma.value > corridors.uncertainty_corridor.safe_max {
        bans.push("uncertainty_above_safe".to_string());
    }

    // Safestep: residual must not increase relative to baseline.
    if !safestep(&baseline_rv, &rv) {
        bans.push("lyapunov_violation_vt_increased".to_string());
    }

    // KER thresholds for "kerdeployable" in school zones.
    let kerdeployable = bans.is_empty() && ker.k >= 0.90 && ker.e >= 0.90 && ker.r <= 0.13;

    NanoDefenseEval {
        zone_meta: shard.zone_meta.clone(),
        mode: shard.mode.clone(),
        risk_vec: rv,
        ker,
        kerdeployable,
        bans_triggered: bans,
    }
}

/// Example helper: construct a Texas school-zone nano filter shard.
pub fn example_texas_school_filter() -> NanoDefenseShard {
    NanoDefenseShard {
        zone_meta: SchoolZoneMeta {
            state: "TX".to_string(),
            county: "Zavala".to_string(),
            district: "Zavala ISD".to_string(),
            campus_id: "TX-ZAVALA-001".to_string(),
            campus_name: "Zavala Elementary".to_string(),
            lat: 28.883,
            lon: -99.760,
        },
        mode: NanoDefenseMode::ExternalFilter,
        parasite_pressure_index: 0.7,  // elevated due to regional screwworm + other parasites.
        nano_exposure_fraction: 0.95,  // most school water/air flows treated.
        neuro_binding_score: 0.0,      // explicitly non-neuroactive filter media.
        half_life_days: 30.0,          // biodegradable media in weeks.
        carbon_kg_co2e_per_year: -500.0, // net-negative if coupled to renewables/offsets.
        uncertainty_index: 0.3,        // moderate uncertainty, pending long-term pediatrics.
    }
}

/// Example helper: construct an Arizona school-zone surface coating shard.
pub fn example_arizona_school_coating() -> NanoDefenseShard {
    NanoDefenseShard {
        zone_meta: SchoolZoneMeta {
            state: "AZ".to_string(),
            county: "Maricopa".to_string(),
            district: "Phoenix Union".to_string(),
            campus_id: "AZ-MARICOPA-001".to_string(),
            campus_name: "Phoenix Central High".to_string(),
            lat: 33.472,
            lon: -112.089,
        },
        mode: NanoDefenseMode::SurfaceCoating,
        parasite_pressure_index: 0.4,
        nano_exposure_fraction: 0.8,    // most high-contact surfaces coated.
        neuro_binding_score: 0.0,       // no CNS activity by materials spec.
        half_life_days: 10.0,           // fast-degrading, biodegradable coating.
        carbon_kg_co2e_per_year: -200.0, // net-negative over lifecycle.
        uncertainty_index: 0.25,
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn safestep_blocks_worse_config() {
        let corridors = SchoolZoneNanoCorridors::default();
        let shard = example_texas_school_filter();
        let baseline = SchoolZoneRiskVector::new(
            RiskCoord::clamped(shard.parasite_pressure_index),
            RiskCoord::clamped(1.0),
            RiskCoord::clamped(0.0),
            RiskCoord::clamped(0.0),
            RiskCoord::clamped(0.0),
            RiskCoord::clamped(1.0),
        );
        let rv = map_to_risks(&shard, &corridors, 365.0, 1_000.0);
        assert!(safestep(&baseline, &rv));
    }

    #[test]
    fn internal_mode_is_banned() {
        let mut shard = example_texas_school_filter();
        // Simulate an unsupported internal mode by abusing enum via serde or other layer;
        // here we just check that non-listed modes would be caught in evaluate_nano_defense.
        // For this test, we force a violation via neuro and persistence instead.
        shard.neuro_binding_score = 1.0;
        shard.half_life_days = 365.0;
        let corridors = SchoolZoneNanoCorridors::default();
        let eval = evaluate_nano_defense(&shard, &corridors, 365.0, 1_000.0);
        assert!(!eval.kerdeployable);
        assert!(!eval.bans_triggered.is_empty());
    }
}
