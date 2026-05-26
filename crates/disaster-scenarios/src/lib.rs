// filename: crates/disaster-scenarios/src/lib.rs
// destination: eco_restoration_shard/crates/disaster-scenarios/src/lib.rs

//! Disaster scenarios catalog for blast radius and ecological risk analysis.
//!
//! This crate is **non-actuating**. It only provides typed descriptions of
//! scenarios that other crates (e.g. T05_blastradius) can use for analysis,
//! simulation parameterization, or CI checks.

#![forbid(unsafe_code)]

use serde::{Deserialize, Serialize};
use std::collections::BTreeMap;
use thiserror::Error;

/// High-level type of scenario, matching hydrology/topology risk grammar.
#[derive(Debug, Clone, Copy, Serialize, Deserialize, PartialEq, Eq, PartialOrd, Ord)]
pub enum ScenarioKind {
    /// Hydrological events: floods, canal overflows, groundwater rise.
    Hydrology,
    /// Structural failures: levees, canals, barriers.
    StructuralFailure,
    /// Combined hydrology + contamination events.
    HydroContamination,
    /// Other environment-coupled scenarios.
    Other,
}

/// Severity band for quick filtering and KER scoring.
#[derive(Debug, Clone, Copy, Serialize, Deserialize, PartialEq, Eq, PartialOrd, Ord)]
pub enum SeverityBand {
    Low,
    Moderate,
    High,
    Extreme,
}

/// Simple jurisdiction tag for alignment with Phoenix/EcoFort configs.
#[derive(Debug, Clone, Serialize, Deserialize, PartialEq, Eq, PartialOrd, Ord)]
pub struct JurisdictionTag {
    pub region: String,
    pub code: String,
}

/// Core disaster scenario description.
///
/// This is intended to be stable and indexable from SQLite (via JSON).
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct DisasterScenario {
    /// Stable scenario identifier, e.g. "T05.PHX.CANAL.BREACH.V1".
    pub scenario_id: String,
    /// Short human-readable name.
    pub name: String,
    /// Narrative description for docs and UI.
    pub description: String,
    /// Scenario kind (hydrology, structural, etc.).
    pub kind: ScenarioKind,
    /// Severity band.
    pub severity: SeverityBand,
    /// Approximate affected radius in meters for first-band blast radius.
    pub approx_radius_m: f64,
    /// Jurisdiction tag (e.g., Phoenix canal grid).
    pub jurisdiction: JurisdictionTag,
    /// Optional tags for joining against planes/coordinates in Eco-Fort.
    pub tags: Vec<String>,
}

/// Error type for scenario lookups.
#[derive(Debug, Error)]
pub enum ScenarioError {
    #[error("scenario not found: {0}")]
    NotFound(String),
}

/// Minimal in-memory registry of scenarios.
/// In production, this can be hydrated from SQLite or JSON files.
#[derive(Debug, Default, Clone)]
pub struct ScenarioRegistry {
    scenarios: BTreeMap<String, DisasterScenario>,
}

impl ScenarioRegistry {
    /// Construct an empty registry.
    pub fn new() -> Self {
        Self {
            scenarios: BTreeMap::new(),
        }
    }

    /// Construct a registry seeded with built-in scenarios.
    ///
    /// These can be aligned with T05 blast radius test cases for Phoenix.
    pub fn with_builtins() -> Self {
        let mut reg = Self::new();

        let phx_canal_breach = DisasterScenario {
            scenario_id: "T05.PHX.CANAL.BREACH.V1".to_string(),
            name: "Phoenix Canal Breach T05".to_string(),
            description: "Canonical T05 blast radius scenario: Phoenix canal breach under peak inflow, used to validate blastradius envelopes and MAR-aware hydrological risk.".to_string(),
            kind: ScenarioKind::Hydrology,
            severity: SeverityBand::High,
            approx_radius_m: 2500.0,
            jurisdiction: JurisdictionTag {
                region: "Phoenix-AZ-US".to_string(),
                code: "PHX.CANAL.GRID".to_string(),
            },
            tags: vec![
                "T05".to_string(),
                "HYDRAULICS.HLR".to_string(),
                "TOPOLOGY.RCANAL".to_string(),
            ],
        };

        reg.insert(phx_canal_breach);
        reg
    }

    /// Insert or replace a scenario.
    pub fn insert(&mut self, scenario: DisasterScenario) {
        self.scenarios
            .insert(scenario.scenario_id.clone(), scenario);
    }

    /// Retrieve a scenario by its stable identifier.
    pub fn get(&self, scenario_id: &str) -> Result<&DisasterScenario, ScenarioError> {
        self.scenarios
            .get(scenario_id)
            .ok_or_else(|| ScenarioError::NotFound(scenario_id.to_string()))
    }

    /// List all scenarios.
    pub fn all(&self) -> impl Iterator<Item = &DisasterScenario> {
        self.scenarios.values()
    }

    /// Export registry to a JSON string for debugging or tooling.
    pub fn to_json(&self) -> serde_json::Result<String> {
        let list: Vec<&DisasterScenario> = self.scenarios.values().collect();
        serde_json::to_string_pretty(&list)
    }
}
