// filename: src/lanes/lane_gates.rs
// destination: eco_restoration_shard/src/lanes/lane_gates.rs

use super::{LaneKind, LaneStatus};

#[derive(Debug)]
pub enum LaneViolation {
    ResidualSlopePositive { b_slope: f64 },
    KBandViolation,
    EBandViolation,
    RBandViolation,
    HydrologyViolation,
    SensorHealthViolation,
    TopologyViolation,
    DisallowedTransition { from: LaneKind, to: LaneKind },
}

#[derive(Debug)]
pub struct CrateLaneConfig {
    pub crate_name: String,
    pub k_min: f64,
    pub e_min: f64,
    pub r_max: f64,
    pub require_hydrology: bool,
    pub require_sensor_health: bool,
    pub require_topology: bool,
}

impl CrateLaneConfig {
    pub fn prod_defaults(crate_name: impl Into<String>) -> Self {
        Self {
            crate_name: crate_name.into(),
            k_min: 0.90,
            e_min: 0.90,
            r_max: 0.13,
            require_hydrology: true,
            require_sensor_health: true,
            require_topology: true,
        }
    }
}

pub fn check_crate_gates(
    _crate_name: &str,
    status: &LaneStatus,
    cfg: &CrateLaneConfig,
    hydrology_ok: bool,
    sensor_ok: bool,
    topology_ok: bool,
) -> Result<(), Vec<LaneViolation>> {
    let mut violations = Vec::new();

    if status.b_slope > 0.0 {
        violations.push(LaneViolation::ResidualSlopePositive {
            b_slope: status.b_slope,
        });
    }

    if status.k < cfg.k_min {
        violations.push(LaneViolation::KBandViolation);
    }

    if status.e < cfg.e_min {
        violations.push(LaneViolation::EBandViolation);
    }

    if status.r > cfg.r_max {
        violations.push(LaneViolation::RBandViolation);
    }

    if cfg.require_hydrology && !hydrology_ok {
        violations.push(LaneViolation::HydrologyViolation);
    }

    if cfg.require_sensor_health && !sensor_ok {
        violations.push(LaneViolation::SensorHealthViolation);
    }

    if cfg.require_topology && !topology_ok {
        violations.push(LaneViolation::TopologyViolation);
    }

    if violations.is_empty() {
        Ok(())
    } else {
        Err(violations)
    }
}

pub struct LaneGate {
    pub allow_research_to_expprod: bool,
    pub allow_expprod_to_prod: bool,
    pub k_min: f64,
    pub e_min: f64,
    pub r_max: f64,
}

impl LaneGate {
    pub fn new() -> Self {
        Self {
            allow_research_to_expprod: true,
            allow_expprod_to_prod: true,
            k_min: 0.90,
            e_min: 0.90,
            r_max: 0.13,
        }
    }

    fn check_bands(&self, status: &LaneStatus, violations: &mut Vec<LaneViolation>) {
        if status.k < self.k_min || !status.k_band_ok {
            violations.push(LaneViolation::KBandViolation);
        }

        if status.e < self.e_min || !status.e_band_ok {
            violations.push(LaneViolation::EBandViolation);
        }

        if status.r > self.r_max || !status.r_band_ok {
            violations.push(LaneViolation::RBandViolation);
        }
    }

    pub fn check(&self, status: &LaneStatus) -> Result<(), Vec<LaneViolation>> {
        let mut violations = Vec::new();

        if status.b_slope > 0.0 {
            violations.push(LaneViolation::ResidualSlopePositive {
                b_slope: status.b_slope,
            });
        }

        self.check_bands(status, &mut violations);

        if violations.is_empty() {
            Ok(())
        } else {
            Err(violations)
        }
    }

    pub fn can_promote(
        &self,
        from: &LaneKind,
        to: &LaneKind,
        status: &LaneStatus,
    ) -> Result<(), Vec<LaneViolation>> {
        match (from, to) {
            (LaneKind::Research, LaneKind::ExpProd) if self.allow_research_to_expprod => {
                self.check(status)
            }
            (LaneKind::ExpProd, LaneKind::Prod) if self.allow_expprod_to_prod => self.check(status),
            _ => Err(vec![LaneViolation::DisallowedTransition {
                from: *from,
                to: *to,
            }]),
        }
    }
}
