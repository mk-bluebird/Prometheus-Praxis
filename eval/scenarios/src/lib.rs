// Path: Prometheus-Praxis/eval/scenarios/src/lib.rs
// License: MIT OR Apache-2.0

#![deny(unsafe_code)]
#![forbid(hidden_glob_reexports)]

use serde::{Deserialize, Serialize};

use ppx_eval_components::{
    AdvectionKernel,
    MarlArchitecture,
    PhoenixContext,
    PhoenixStack,
    StreamingPipeline,
};
use ppx_eval_rubric::{
    ComponentEvaluable,
    Dimension,
    PhoenixEligibilityThresholds,
    SevenDimProfile,
    SystemEvaluable,
};

/// Identifier for the arid cities modeled in AridCityTransferability.aln.
///
/// Mirrors the `city Phoenix`, `city Tucson`, `city LasVegas`, and `city Albuquerque`
/// entries in the ALN module.
#[derive(Debug, Clone, Copy, Serialize, Deserialize, PartialEq, Eq)]
pub enum CityId {
    Phoenix,
    Tucson,
    LasVegas,
    Albuquerque,
}

/// Scenario identifier mirroring the ALN `scenario` entries.
///
/// - PhoenixPrimary
/// - TucsonDiagnostic
/// - LasVegasDiagnostic
/// - AlbuquerqueDiagnostic
#[derive(Debug, Clone, Copy, Serialize, Deserialize, PartialEq, Eq)]
pub enum ScenarioId {
    PhoenixPrimary,
    TucsonDiagnostic,
    LasVegasDiagnostic,
    AlbuquerqueDiagnostic,
}

/// Archetype label for city-level geometry and climate context.
///
/// Examples:
/// - "arid_urban_canyon"
/// - "hyper_arid_urban_basin"
/// - "semi_arid_plateau"
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct Archetype {
    pub name: String,
}

/// Configuration for a city, mirroring the ALN `city` records.
///
/// These parameters are intentionally aligned with `PhoenixContext`
/// so that they can be injected directly into component evaluation.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct CityConfig {
    pub id: CityId,
    pub archetype: Archetype,
    pub monsoon_intensity_index: f32,
    pub canyon_heat_gradient: f32,
    pub fog_channel_density: f32,
    pub industrial_waste_load: f32,
    pub sovereignty_weight: f32,
    pub energy_constraint: f32,
}

/// Configuration for a scenario, binding a city to a role and scoring dimensions.
///
/// Scoring dimensions are expressed via the rubric `Dimension` enum.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ScenarioConfig {
    pub id: ScenarioId,
    pub city: CityId,
    pub role: String,
    pub scoring_dimensions: Vec<Dimension>,
}

/// City configurations as defined in `arid_city_transfer_aln.aln`.
///
/// These functions provide canonical parameters for the four cities
/// and should be kept in sync with the ALN module.
pub fn phoenix_config() -> CityConfig {
    CityConfig {
        id: CityId::Phoenix,
        archetype: Archetype {
            name: "arid_urban_canyon".to_string(),
        },
        monsoon_intensity_index: 0.85,
        canyon_heat_gradient: 0.90,
        fog_channel_density: 0.75,
        industrial_waste_load: 0.80,
        sovereignty_weight: 0.95,
        energy_constraint: 0.70,
    }
}

pub fn tucson_config() -> CityConfig {
    CityConfig {
        id: CityId::Tucson,
        archetype: Archetype {
            name: "arid_urban_canyon".to_string(),
        },
        monsoon_intensity_index: 0.65,
        canyon_heat_gradient: 0.80,
        fog_channel_density: 0.60,
        industrial_waste_load: 0.55,
        sovereignty_weight: 0.90,
        energy_constraint: 0.65,
    }
}

pub fn las_vegas_config() -> CityConfig {
    CityConfig {
        id: CityId::LasVegas,
        archetype: Archetype {
            name: "hyper_arid_urban_basin".to_string(),
        },
        monsoon_intensity_index: 0.40,
        canyon_heat_gradient: 0.95,
        fog_channel_density: 0.50,
        industrial_waste_load: 0.75,
        sovereignty_weight: 0.92,
        energy_constraint: 0.80,
    }
}

pub fn albuquerque_config() -> CityConfig {
    CityConfig {
        id: CityId::Albuquerque,
        archetype: Archetype {
            name: "semi_arid_plateau".to_string(),
        },
        monsoon_intensity_index: 0.55,
        canyon_heat_gradient: 0.70,
        fog_channel_density: 0.65,
        industrial_waste_load: 0.60,
        sovereignty_weight: 0.93,
        energy_constraint: 0.60,
    }
}

/// Scenario configurations mirroring ALN `scenario` entries.
///
/// These are lightweight bindings from city + role to the full seven
/// scoring dimensions, consistent with AridCityTransferability.
pub fn phoenix_primary_scenario() -> ScenarioConfig {
    ScenarioConfig {
        id: ScenarioId::PhoenixPrimary,
        city: CityId::Phoenix,
        role: "primary_deployment_evaluation".to_string(),
        scoring_dimensions: all_seven_dimensions(),
    }
}

pub fn tucson_diagnostic_scenario() -> ScenarioConfig {
    ScenarioConfig {
        id: ScenarioId::TucsonDiagnostic,
        city: CityId::Tucson,
        role: "transferability_diagnostic".to_string(),
        scoring_dimensions: all_seven_dimensions(),
    }
}

pub fn las_vegas_diagnostic_scenario() -> ScenarioConfig {
    ScenarioConfig {
        id: ScenarioId::LasVegasDiagnostic,
        city: CityId::LasVegas,
        role: "transferability_diagnostic".to_string(),
        scoring_dimensions: all_seven_dimensions(),
    }
}

pub fn albuquerque_diagnostic_scenario() -> ScenarioConfig {
    ScenarioConfig {
        id: ScenarioId::AlbuquerqueDiagnostic,
        city: CityId::Albuquerque,
        role: "transferability_diagnostic".to_string(),
        scoring_dimensions: all_seven_dimensions(),
    }
}

/// Utility: list all seven rubric dimensions.
pub fn all_seven_dimensions() -> Vec<Dimension> {
    vec![
        Dimension::KnowledgeFactor,
        Dimension::EcoImpact,
        Dimension::RiskOfHarm,
        Dimension::Robustness,
        Dimension::Sovereignty,
        Dimension::EnergyEfficiency,
        Dimension::GovernanceAlignment,
    ]
}

/// Convert a `CityConfig` into a `PhoenixContext`
/// for component-level evaluation.
///
/// This function intentionally mirrors the fields used in
/// `AdvectionKernel`, `MarlArchitecture`, and `StreamingPipeline`.
pub fn city_to_phoenix_context(cfg: &CityConfig) -> PhoenixContext {
    PhoenixContext {
        monsoon_intensity_index: cfg.monsoon_intensity_index,
        canyon_heat_gradient: cfg.canyon_heat_gradient,
        fog_channel_density: cfg.fog_channel_density,
        industrial_waste_load: cfg.industrial_waste_load,
        sovereignty_weight: cfg.sovereignty_weight,
        energy_constraint: cfg.energy_constraint,
    }
}

/// Construct a canonical component triple for a given city.
///
/// These are conceptual defaults intended to be calibrated against
/// real data (DUSTIEAIM, local sensors, governance constraints) when
/// the stack is deployed.
pub fn canonical_components_for_city(cfg: &CityConfig) -> (AdvectionKernel, MarlArchitecture, StreamingPipeline) {
    let ctx = city_to_phoenix_context(cfg);

    let adv = AdvectionKernel {
        scheme_name: "upwind_cfl_safe".to_string(),
        cfl_safety_margin: 0.9,
        physical_fidelity_index: 0.90,
        restored_flow_ratio: 0.80,
        numerical_robustness_index: 0.88,
        ctx: ctx.clone(),
    };

    let marl = MarlArchitecture {
        policy_alignment_index: 0.88,
        rogue_pattern_resilience: 0.86,
        multi_actor_scalability: 0.84,
        consent_corridor_strength: 0.92,
        cybercore_binding_strength: 0.94,
        ctx: ctx.clone(),
    };

    let stream = StreamingPipeline {
        end_to_end_latency_ms: 200.0,
        failure_recovery_index: 0.85,
        data_sovereignty_index: cfg.sovereignty_weight,
        energy_cost_per_event: 0.35,
        biosignal_integration_index: 0.80,
        ctx,
    };

    (adv, marl, stream)
}

/// Evaluate a city and scenario into a `SevenDimProfile`.
///
/// This uses `PhoenixStack` and `PhoenixEligibilityThresholds::default()`
/// to compute an aggregated system profile, ignoring governance gates.
/// Governance evidence should be layered on separately.
pub fn scenario_profile_for_city(city: CityId, scenario: ScenarioId) -> SevenDimProfile {
    let cfg = match city {
        CityId::Phoenix => phoenix_config(),
        CityId::Tucson => tucson_config(),
        CityId::LasVegas => las_vegas_config(),
        CityId::Albuquerque => albuquerque_config(),
    };

    let (_scenario_cfg, thresholds) = match scenario {
        ScenarioId::PhoenixPrimary => (phoenix_primary_scenario(), PhoenixEligibilityThresholds::default()),
        ScenarioId::TucsonDiagnostic => (tucson_diagnostic_scenario(), PhoenixEligibilityThresholds::default()),
        ScenarioId::LasVegasDiagnostic => (las_vegas_diagnostic_scenario(), PhoenixEligibilityThresholds::default()),
        ScenarioId::AlbuquerqueDiagnostic => (albuquerque_diagnostic_scenario(), PhoenixEligibilityThresholds::default()),
    };

    let (adv, marl, stream) = canonical_components_for_city(&cfg);
    let stack = PhoenixStack::new(adv.clone(), marl.clone(), stream.clone(), thresholds);

    let components: Vec<Box<dyn ComponentEvaluable>> =
        vec![Box::new(adv), Box::new(marl), Box::new(stream)];

    let eligibility = stack.evaluate_system(&components);
    eligibility.profile
}

/// Lightweight serializable snapshot for AI/chat and reporting.
///
/// This struct is designed to be easy to consume by tooling and
/// documentation generators, without exposing internal crate details.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ScenarioProfileSnapshot {
    pub city: CityId,
    pub scenario: ScenarioId,
    pub archetype: String,
    pub profile: SevenDimProfile,
}

impl ScenarioProfileSnapshot {
    pub fn from_city_and_scenario(city: CityId, scenario: ScenarioId) -> Self {
        let cfg = match city {
            CityId::Phoenix => phoenix_config(),
            CityId::Tucson => tucson_config(),
            CityId::LasVegas => las_vegas_config(),
            CityId::Albuquerque => albuquerque_config(),
        };

        let profile = scenario_profile_for_city(city, scenario);

        ScenarioProfileSnapshot {
            city,
            scenario,
            archetype: cfg.archetype.name.clone(),
            profile,
        }
    }
}
