// Filename: crates/econet_governance_spine/src/hydraulics_constraint_feeder.rs
// Destination: crates/econet_governance_spine/src/hydraulics_constraint_feeder.rs
// License: MIT OR Apache-2.0
// Edition: 2024
// rust-version = "1.85"
// !forbid_unsafecode

use std::time::{Duration, SystemTime};

use crate::{
    CyboquaticMetrics,
    GovernanceSpine,
    LaneStatus,
    SpineError,
};

/// Inputs required to compute hydraulics-aware eco-per-joule for a Cyboquatic node.
/// These are derived from CEIM and hydrology/hydraulics corridor planes wired elsewhere.
#[derive(Debug, Clone)]
pub struct HydraulicsConstraintInputs {
    pub node_id: String,
    pub shard_id: String,
    // CEIM energy-per-mass for pollutant x, in kWh/kg removed.
    pub energy_per_mass_kwh_per_kg: f64,
    // Raw energy for the run, in kWh.
    pub energy_run_kwh: f64,
    // Mass removed during the run, in kg.
    pub mass_removed_kg: f64,
    // Corridor-normalized risk coordinates rx ∈ [0,1] for hydraulics and treatment planes.
    pub r_hydraulics: f64,
    pub r_treatment_sat: f64,
    pub r_fouling_om: f64,
    // Data trust coordinates from rcalib / rsigma planes.
    pub r_calib: f64,
    pub r_sigma: f64,
    // Layer-2 gate predicates already evaluated upstream.
    pub hydraulic_structural_ok: bool,
    pub treatment_sat_ok: bool,
    pub fouling_om_ok: bool,
    pub social_license_ok: bool,
}

/// Outputs of the hydraulics-constraint feeder.
/// These values are intended to be materialized into SQL views such as vhydraulicsconstraintfeeder
/// and consumed by vcyboquaticecoperjoule and lane guards.
#[derive(Debug, Clone)]
pub struct HydraulicsConstraintFeederOutput {
    pub node_id: String,
    pub shard_id: String,
    // Base ecoperjoule from CEIM kernel (kWh/kg removed).
    pub ecoperjoule_raw: f64,
    // Hydraulics-aware ecoperjoule; zeroed or discounted when constraints fail.
    pub ecoperjoule_hydraulics: f64,
    // Whether hydraulics/treatment/fouling/social gates and data trust planes are acceptable.
    pub hydraulics_valid: bool,
    // Trust-scaled knowledge and eco-impact factors in [0,1].
    pub knowledge_factor_adj: f64,
    pub eco_impact_adj: f64,
}

/// Static thresholds for corridor bands and trust scalars.
/// In production these should be loaded from ecosafety.corridors.v2 and trust-plane schemas.
#[derive(Debug, Clone)]
pub struct HydraulicsConstraintConfig {
    pub r_hydraulics_gold_max: f64,
    pub r_treatment_sat_gold_max: f64,
    pub r_fouling_om_gold_max: f64,
    pub r_calib_hard_max: f64,
    pub r_sigma_hard_max: f64,
    pub trust_weight_calib: f64,
    pub trust_weight_sigma: f64,
}

impl Default for HydraulicsConstraintConfig {
    fn default() -> Self {
        HydraulicsConstraintConfig {
            r_hydraulics_gold_max: 0.5,
            r_treatment_sat_gold_max: 0.5,
            r_fouling_om_gold_max: 0.5,
            r_calib_hard_max: 0.8,
            r_sigma_hard_max: 0.8,
            trust_weight_calib: 0.5,
            trust_weight_sigma: 0.5,
        }
    }
}

impl HydraulicsConstraintInputs {
    /// Compute base ecoperjoule from run energy and mass removed.
    /// Guards against division by zero by returning 0.0 when mass_removed_kg == 0.
    pub fn base_ecoperjoule(&self) -> f64 {
        if self.mass_removed_kg <= 0.0 {
            0.0
        } else {
            self.energy_run_kwh / self.mass_removed_kg
        }
    }
}

/// Computes a hydraulics-valid predicate and hydraulics-aware ecoperjoule and trust-adjusted K/E.
pub fn compute_hydraulics_constraint(
    inputs: &HydraulicsConstraintInputs,
    config: &HydraulicsConstraintConfig,
    base_knowledge_factor: f64,
    base_eco_impact: f64,
) -> HydraulicsConstraintFeederOutput {
    let ecoperjoule_raw = if inputs.energy_per_mass_kwh_per_kg > 0.0 {
        inputs.energy_per_mass_kwh_per_kg
    } else {
        inputs.base_ecoperjoule()
    };

    // Corridor checks for hydraulics and treatment/fouling planes.
    let hydraulics_ok = inputs.r_hydraulics <= config.r_hydraulics_gold_max;
    let treatment_ok = inputs.r_treatment_sat <= config.r_treatment_sat_gold_max;
    let fouling_ok = inputs.r_fouling_om <= config.r_fouling_om_gold_max;

    // Data trust planes: rcalib and rsigma must remain below hard bands.
    let calib_ok = inputs.r_calib <= config.r_calib_hard_max;
    let sigma_ok = inputs.r_sigma <= config.r_sigma_hard_max;

    let gates_ok = inputs.hydraulic_structural_ok
        && inputs.treatment_sat_ok
        && inputs.fouling_om_ok
        && inputs.social_license_ok;

    // Hydraulics-valid only if all corridor and gate predicates and trust planes are satisfied.
    let hydraulics_valid = hydraulics_ok && treatment_ok && fouling_ok && calib_ok && sigma_ok && gates_ok;

    // Trust scalars: down-scaling K and E according to rcalib and rsigma.
    // Ddata = 1 - rcalib, Dsigma = 1 - rsigma; combined as weighted average.
    let d_data = 1.0 - inputs.r_calib;
    let d_sigma = 1.0 - inputs.r_sigma;
    let trust_den = config.trust_weight_calib + config.trust_weight_sigma;
    let d_combined = if trust_den > 0.0 {
        let num = config.trust_weight_calib * d_data + config.trust_weight_sigma * d_sigma;
        if num < 0.0 {
            0.0
        } else if num > trust_den {
            trust_den
        } else {
            num
        } / trust_den
    } else {
        1.0
    };

    let knowledge_factor_adj = (base_knowledge_factor * d_combined).clamp(0.0, 1.0);
    let eco_impact_adj = (base_eco_impact * d_combined).clamp(0.0, 1.0);

    let ecoperjoule_hydraulics = if hydraulics_valid {
        ecoperjoule_raw
    } else {
        0.0
    };

    HydraulicsConstraintFeederOutput {
        node_id: inputs.node_id.clone(),
        shard_id: inputs.shard_id.clone(),
        ecoperjoule_raw,
        ecoperjoule_hydraulics,
        hydraulics_valid,
        knowledge_factor_adj,
        eco_impact_adj,
    }
}

/// Extension methods for GovernanceSpine to surface hydraulics-aware CyboquaticMetrics.
///
/// This keeps the spine read-only: all DB writes for feeder outputs should be performed
/// via separate non-actuating governance tools; the spine only reads canonical views.
impl GovernanceSpine {
    /// Fetch hydraulics-aware CyboquaticMetrics for a node by joining Cyboquatic views
    /// with lane status and hydraulics constraint feeder view.
    ///
    /// Assumes presence of SQL views:
    /// - vcyboquaticecoperjoule (DR-CYBO-ECO-VIEW-001)
    /// - vhydraulicsconstraintfeeder (new view materialized from feeder outputs)
    /// - vlanestatuslatest with carbonnegativeok and restorationok flags.
    pub fn get_hydraulics_coupled_cyboquatic_metrics(
        &self,
        node_id: &str,
    ) -> Result<CyboquaticMetrics, SpineError> {
        let conn = &self.conn;

        // Query hydraulics constraint feeder view.
        let mut stmt_h = conn.prepare(
            "SELECT shard_id, ecoperjoule_hydraulics, hydraulics_valid \
             FROM vhydraulicsconstraintfeeder \
             WHERE node_id = ?1 \
             ORDER BY window_end_utc DESC \
             LIMIT 1",
        )?;

        let feeder_row = stmt_h.query_row([node_id], |row| {
            let shard_id: String = row.get(0)?;
            let ecoperjoule_hydraulics: f64 = row.get(1)?;
            let hydraulics_valid: bool = row.get(2)?;
            Ok((shard_id, ecoperjoule_hydraulics, hydraulics_valid))
        })?;

        let (shard_id, ecoperjoule_hydraulics, hydraulics_valid) = feeder_row;

        if !hydraulics_valid {
            return Err(SpineError::MissingCyboquaticMetrics(format!(
                "Hydraulics constraints not satisfied for node {node_id} shard {shard_id}"
            )));
        }

        // Query lane status for carbonnegativeok and restorationok.
        let mut stmt_lane = conn.prepare(
            "SELECT carbonnegativeok, restorationok \
             FROM vlanestatuslatest \
             WHERE shard_id = ?1 \
             ORDER BY applied_at_utc DESC \
             LIMIT 1",
        )?;

        let lane_row = stmt_lane.query_row([&shard_id], |row| {
            let carbonnegative_ok: bool = row.get(0)?;
            let restoration_ok: bool = row.get(1)?;
            Ok((carbonnegative_ok, restoration_ok))
        })?;

        let (carbonnegative_ok, restoration_ok) = lane_row;

        if !carbonnegative_ok || !restoration_ok {
            return Err(SpineError::MissingCyboquaticMetrics(format!(
                "Cyboquatic lane flags not satisfied for node {node_id} shard {shard_id}"
            )));
        }

        // Query base Cyboquatic metrics view.
        let mut stmt_c = conn.prepare(
            "SELECT ecoperjoule \
             FROM vcyboquaticecoperjoule \
             WHERE node_id = ?1 AND shard_id = ?2 \
             ORDER BY window_end_utc DESC \
             LIMIT 1",
        )?;

        let base_ecoperjoule: f64 = stmt_c.query_row([node_id, &shard_id], |row| row.get(0))?;

        // Compose final CyboquaticMetrics, overriding ecoperjoule with hydraulics-aware value.
        let metrics = CyboquaticMetrics {
            nodeid: node_id.to_owned(),
            shardid: shard_id,
            ecoperjoule: ecoperjoule_hydraulics,
            carbonnegativeok: carbonnegative_ok,
            restorationok,
        };

        // Optional: sanity check that hydraulics-aware ecoperjoule does not exceed base view
        // by more than a small epsilon.
        if ecoperjoule_hydraulics > base_ecoperjoule + 1e-6 {
            return Err(SpineError::MissingCyboquaticMetrics(format!(
                "Hydraulics-aware ecoperjoule {} exceeds base view {} for node {}",
                ecoperjoule_hydraulics, base_ecoperjoule, node_id
            )));
        }

        Ok(metrics)
    }

    /// Convenience function to compute hydraulics constraint output from current shard telemetry
    /// and return both CyboquaticMetrics and feeder output without performing any DB writes.
    ///
    /// This is suitable for non-actuating CI harnesses and diagnostic frames.
    pub fn evaluate_hydraulics_constraint_for_node(
        &self,
        inputs: &HydraulicsConstraintInputs,
        config: &HydraulicsConstraintConfig,
        base_knowledge_factor: f64,
        base_eco_impact: f64,
    ) -> Result<(HydraulicsConstraintFeederOutput, CyboquaticMetrics), SpineError> {
        let feeder_output =
            compute_hydraulics_constraint(inputs, config, base_knowledge_factor, base_eco_impact);

        if !feeder_output.hydraulics_valid {
            return Err(SpineError::MissingCyboquaticMetrics(format!(
                "Hydraulics constraint invalid for node {} shard {}",
                feeder_output.node_id, feeder_output.shard_id
            )));
        }

        // LaneStatus is read-only within the spine; here we query latest lane row to
        // ensure carbonnegativeok and restorationok are consistent with hydrualics-valid.
        let conn = &self.conn;

        let mut stmt_lane = conn.prepare(
            "SELECT carbonnegativeok, restorationok \
             FROM vlanestatuslatest \
             WHERE shard_id = ?1 \
             ORDER BY applied_at_utc DESC \
             LIMIT 1",
        )?;

        let lane_row = stmt_lane.query_row([&feeder_output.shard_id], |row| {
            let carbonnegative_ok: bool = row.get(0)?;
            let restoration_ok: bool = row.get(1)?;
            Ok((carbonnegative_ok, restoration_ok))
        })?;

        let (carbonnegative_ok, restoration_ok) = lane_row;

        let metrics = CyboquaticMetrics {
            nodeid: feeder_output.node_id.clone(),
            shardid: feeder_output.shard_id.clone(),
            ecoperjoule: feeder_output.ecoperjoule_hydraulics,
            carbonnegativeok: carbonnegative_ok,
            restorationok,
        };

        Ok((feeder_output, metrics))
    }
}
