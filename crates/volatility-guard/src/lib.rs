// File: crates/volatility-guard/src/lib.rs
#![forbid(unsafe_code)]

mod checksafestep;
mod non_offsettable_plane;

pub use checksafestep::{SafeStepContext, SafeStepVerdict};
pub use non_offsettable_plane::{EcoPlane, NonOffsettablePlane};

use std::collections::{HashMap, VecDeque};

#[derive(Clone, Debug, PartialEq)]
pub struct WorkloadObservation {
    pub canal_node: String,
    pub observed_utc: String,
    pub eco_impact_value: f64,
    pub delta_vt: f64,
}

#[derive(Clone, Copy, Debug, PartialEq)]
pub struct VolatilityThresholds {
    pub minimum_samples: usize,
    pub eco_impact_range_max: f64,
    pub delta_vt_range_max: f64,
}

#[derive(Clone, Debug, PartialEq)]
pub struct GovernanceAlert {
    pub canal_node: String,
    pub observed_utc: String,
    pub sample_count: usize,
    pub eco_impact_range: f64,
    pub delta_vt_range: f64,
    pub reason: String,
}

#[derive(Clone, Debug, PartialEq)]
pub enum VolatilityGuardError {
    InvalidObservation,
    InvalidThresholds,
    AlertDeliveryFailed,
}

pub trait EcoGuardSpine {
    fn emit_governance_alert(&self, alert: &GovernanceAlert) -> Result<(), VolatilityGuardError>;
}

#[derive(Default)]
struct NodeWindow {
    observations: VecDeque<WorkloadObservation>,
    alerted: bool,
}

pub struct VolatilityGuard {
    capacity: usize,
    thresholds: VolatilityThresholds,
    windows: HashMap<String, NodeWindow>,
}

impl VolatilityGuard {
    pub fn new(
        capacity: usize,
        thresholds: VolatilityThresholds,
    ) -> Result<Self, VolatilityGuardError> {
        if capacity == 0
            || thresholds.minimum_samples == 0
            || thresholds.minimum_samples > capacity
            || !thresholds.eco_impact_range_max.is_finite()
            || !thresholds.delta_vt_range_max.is_finite()
            || thresholds.eco_impact_range_max < 0.0
            || thresholds.delta_vt_range_max < 0.0
        {
            return Err(VolatilityGuardError::InvalidThresholds);
        }

        Ok(Self {
            capacity,
            thresholds,
            windows: HashMap::new(),
        })
    }

    pub fn observe(
        &mut self,
        observation: WorkloadObservation,
        spine: &impl EcoGuardSpine,
    ) -> Result<Option<GovernanceAlert>, VolatilityGuardError> {
        if observation.canal_node.trim().is_empty()
            || observation.observed_utc.trim().is_empty()
            || !observation.eco_impact_value.is_finite()
            || !observation.delta_vt.is_finite()
            || !(0.0..=1.0).contains(&observation.eco_impact_value)
        {
            return Err(VolatilityGuardError::InvalidObservation);
        }

        let node_id = observation.canal_node.clone();
        let window = self.windows.entry(node_id).or_default();
        if window.observations.len() == self.capacity {
            window.observations.pop_front();
        }
        window.observations.push_back(observation);

        if window.observations.len() < self.thresholds.minimum_samples {
            return Ok(None);
        }

        let eco_min = window
            .observations
            .iter()
            .map(|frame| frame.eco_impact_value)
            .fold(f64::INFINITY, f64::min);
        let eco_max = window
            .observations
            .iter()
            .map(|frame| frame.eco_impact_value)
            .fold(f64::NEG_INFINITY, f64::max);
        let delta_min = window
            .observations
            .iter()
            .map(|frame| frame.delta_vt)
            .fold(f64::INFINITY, f64::min);
        let delta_max = window
            .observations
            .iter()
            .map(|frame| frame.delta_vt)
            .fold(f64::NEG_INFINITY, f64::max);

        let eco_impact_range = eco_max - eco_min;
        let delta_vt_range = delta_max - delta_min;
        let within_corridor = eco_impact_range <= self.thresholds.eco_impact_range_max
            && delta_vt_range <= self.thresholds.delta_vt_range_max;

        if within_corridor {
            window.alerted = false;
            return Ok(None);
        }
        if window.alerted {
            return Ok(None);
        }

        let Some(latest) = window.observations.back() else {
            return Ok(None);
        };
        let alert = GovernanceAlert {
            canal_node: latest.canal_node.clone(),
            observed_utc: latest.observed_utc.clone(),
            sample_count: window.observations.len(),
            eco_impact_range,
            delta_vt_range,
            reason: "workload volatility exceeds configured ecological corridor".into(),
        };

        spine
            .emit_governance_alert(&alert)
            .map_err(|_| VolatilityGuardError::AlertDeliveryFailed)?;
        window.alerted = true;
        Ok(Some(alert))
    }

    pub fn clear_node(&mut self, canal_node: &str) {
        self.windows.remove(canal_node);
    }
}
