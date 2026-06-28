// eco_restoration_shard/src/offroad/neuroload_corridor_v1.rs
// Rust 2024, rust-version = "1.85", dual-licensed, production-grade module
// Role: Sovereign off-road NeuroLoad corridor coordinate and validator.

#![allow(clippy::missing_docs_in_private_items)]
#![forbid(unsafe_code)]

use std::cmp::Ordering;
use std::fmt;
use std::time::Duration;

/// Status of a NeuroLoad coordinate relative to its corridor bounds.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum NeuroLoadStatus {
    Ok,
    SoftViolation,
    HardViolation,
}

impl fmt::Display for NeuroLoadStatus {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        let s = match self {
            NeuroLoadStatus::Ok => "OK",
            NeuroLoadStatus::SoftViolation => "SoftViolation",
            NeuroLoadStatus::HardViolation => "HardViolation",
        };
        f.write_str(s)
    }
}

impl NeuroLoadStatus {
    pub fn from_str(raw: &str) -> Option<Self> {
        match raw {
            "OK" => Some(Self::Ok),
            "SoftViolation" => Some(Self::SoftViolation),
            "HardViolation" => Some(Self::HardViolation),
            _ => None,
        }
    }
}

/// Sovereign NeuroLoad coordinate, aligned with NeuroLoadCoord.v1 in ALN.
#[derive(Debug, Clone)]
pub struct NeuroLoadCoord {
    pub id: String,
    pub node_id: String,
    pub value: f64,       // normalized [0,1]
    pub minsafe: f64,     // normalized [0,1]
    pub maxsafe: f64,     // normalized [0,1]
    pub status: NeuroLoadStatus,
    pub timestamp_ns: u64,
    pub k_factor: f64,    // [0,1]
    pub e_factor: f64,    // [0,1]
    pub r_factor: f64,    // [0,1]
    pub author_did: String,
    pub corridor_id: Option<String>,
    pub notes: Option<String>,
}

impl NeuroLoadCoord {
    /// Create a new coordinate with explicit bounds and factors.
    pub fn new(
        id: impl Into<String>,
        node_id: impl Into<String>,
        value: f64,
        minsafe: f64,
        maxsafe: f64,
        timestamp_ns: u64,
        k_factor: f64,
        e_factor: f64,
        r_factor: f64,
        author_did: impl Into<String>,
        corridor_id: Option<String>,
        notes: Option<String>,
    ) -> Result<Self, NeuroLoadError> {
        let coord = Self {
            id: id.into(),
            node_id: node_id.into(),
            value,
            minsafe,
            maxsafe,
            status: NeuroLoadStatus::Ok,
            timestamp_ns,
            k_factor,
            e_factor,
            r_factor,
            author_did: author_did.into(),
            corridor_id,
            notes,
        };
        coord.validate_invariants()?;
        Ok(coord)
    }

    /// Validate ALN invariants on minsafe/maxsafe/value and K/E/R.
    pub fn validate_invariants(&self) -> Result<(), NeuroLoadError> {
        if !(0.0..=1.0).contains(&self.minsafe) {
            return Err(NeuroLoadError::OutOfRange("minsafe"));
        }
        if !(0.0..=1.0).contains(&self.maxsafe) {
            return Err(NeuroLoadError::OutOfRange("maxsafe"));
        }
        if self.minsafe > self.maxsafe {
            return Err(NeuroLoadError::InvalidBounds);
        }
        if !(0.0..=1.0).contains(&self.value) {
            return Err(NeuroLoadError::OutOfRange("value"));
        }
        if !(0.0..=1.0).contains(&self.k_factor) {
            return Err(NeuroLoadError::OutOfRange("k_factor"));
        }
        if !(0.0..=1.0).contains(&self.e_factor) {
            return Err(NeuroLoadError::OutOfRange("e_factor"));
        }
        if !(0.0..=1.0).contains(&self.r_factor) {
            return Err(NeuroLoadError::OutOfRange("r_factor"));
        }
        Ok(())
    }

    /// Apply NeuroLoadStatusUpdate.v1 logic to recompute status.
    pub fn update_status(&mut self) {
        if self.value < self.minsafe || self.value > self.maxsafe {
            self.status = NeuroLoadStatus::SoftViolation;
        } else {
            self.status = NeuroLoadStatus::Ok;
        }
    }

    /// Apply NeuroLoadHardStop.v1 logic to escalate to hard violation.
    pub fn apply_hard_stop(&mut self) {
        let threshold = self.maxsafe + 0.10;
        if self.value > threshold {
            self.status = NeuroLoadStatus::HardViolation;
        }
    }

    /// Apply NeuroLoadKERUpdate.v1 transition to k/e/r factors.
    pub fn update_ker_factors(&mut self) {
        match self.status {
            NeuroLoadStatus::Ok => {
                self.k_factor = (self.k_factor + 0.02).min(1.0);
                self.r_factor = (self.r_factor - 0.02).max(0.0);
            }
            NeuroLoadStatus::SoftViolation => {
                self.r_factor = (self.r_factor + 0.03).min(1.0);
            }
            NeuroLoadStatus::HardViolation => {
                self.k_factor = (self.k_factor - 0.05).max(0.0);
                self.r_factor = (self.r_factor + 0.10).min(1.0);
            }
        }
    }

    /// Convenience: update status, hard stop, and K/E/R in one step.
    pub fn step(&mut self) -> Result<(), NeuroLoadError> {
        self.validate_invariants()?;
        self.update_status();
        self.apply_hard_stop();
        self.update_ker_factors();
        self.validate_invariants()?;
        Ok(())
    }

    /// Check whether this coordinate remains inside its corridor.
    pub fn is_inside_corridor(&self) -> bool {
        (self.value >= self.minsafe) && (self.value <= self.maxsafe)
    }

    /// Compute a simple Lyapunov-style residual for this coordinate.
    /// Residual = (value - mid)^2, where mid is the center of [minsafe, maxsafe].
    pub fn residual(&self) -> f64 {
        let mid = 0.5 * (self.minsafe + self.maxsafe);
        let diff = self.value - mid;
        diff * diff
    }
}

/// Binding of a node to a NeuroLoad corridor, aligned with NeuroLoadCorridorBinding.v1.
#[derive(Debug, Clone)]
pub struct NeuroLoadCorridorBinding {
    pub corridor_id: String,
    pub node_id: String,
    pub minsafe_default: f64,
    pub maxsafe_default: f64,
    pub author_did: String,
    pub created_ns: u64,
    pub updated_ns: u64,
}

impl NeuroLoadCorridorBinding {
    pub fn new(
        corridor_id: impl Into<String>,
        node_id: impl Into<String>,
        minsafe_default: f64,
        maxsafe_default: f64,
        author_did: impl Into<String>,
        created_ns: u64,
        updated_ns: u64,
    ) -> Result<Self, NeuroLoadError> {
        let binding = Self {
            corridor_id: corridor_id.into(),
            node_id: node_id.into(),
            minsafe_default,
            maxsafe_default,
            author_did: author_did.into(),
            created_ns,
            updated_ns,
        };
        binding.validate_invariants()?;
        Ok(binding)
    }

    pub fn validate_invariants(&self) -> Result<(), NeuroLoadError> {
        if !(0.0..=1.0).contains(&self.minsafe_default) {
            return Err(NeuroLoadError::OutOfRange("minsafe_default"));
        }
        if !(0.0..=1.0).contains(&self.maxsafe_default) {
            return Err(NeuroLoadError::OutOfRange("maxsafe_default"));
        }
        if self.minsafe_default > self.maxsafe_default {
            return Err(NeuroLoadError::InvalidBounds);
        }
        Ok(())
    }

    /// Initialize a coordinate from this binding and a measured value.
    pub fn init_coord(
        &self,
        id: impl Into<String>,
        value: f64,
        timestamp_ns: u64,
        author_did: impl Into<String>,
    ) -> Result<NeuroLoadCoord, NeuroLoadError> {
        NeuroLoadCoord::new(
            id,
            &self.node_id,
            value,
            self.minsafe_default,
            self.maxsafe_default,
            timestamp_ns,
            0.5,          // initial k_factor
            0.5,          // initial e_factor
            0.5,          // initial r_factor
            author_did,
            Some(self.corridor_id.clone()),
            None,
        )
    }
}

/// Errors for NeuroLoad invariants and corridor operations.
#[derive(Debug)]
pub enum NeuroLoadError {
    OutOfRange(&'static str),
    InvalidBounds,
}

impl fmt::Display for NeuroLoadError {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            NeuroLoadError::OutOfRange(field) => {
                write!(f, "field '{}' out of allowed [0,1] range", field)
            }
            NeuroLoadError::InvalidBounds => {
                write!(f, "minsafe must be <= maxsafe")
            }
        }
    }
}

impl std::error::Error for NeuroLoadError {}

/// Sovereign policy fragment for data-as-labor and access control.
#[derive(Debug, Clone)]
pub enum AccessClass {
    Open,
    Restricted,
    Controlled,
}

impl fmt::Display for AccessClass {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        let s = match self {
            AccessClass::Open => "open",
            AccessClass::Restricted => "restricted",
            AccessClass::Controlled => "controlled",
        };
        f.write_str(s)
    }
}

/// Knowledge Object wrapper for NeuroLoad coordinates under a data-as-labor policy.
#[derive(Debug, Clone)]
pub struct NeuroLoadKnowledgeObject {
    pub ko_id: String,
    pub coord: NeuroLoadCoord,
    pub author_did: String,
    pub access_class: AccessClass,
    pub allowed_roles: Vec<String>,
    pub allowed_actions: Vec<String>,
    pub chat_cost_unit: f64,
}

impl NeuroLoadKnowledgeObject {
    pub fn validate(&self) -> Result<(), NeuroLoadError> {
        if self.chat_cost_unit < 0.0 {
            return Err(NeuroLoadError::OutOfRange("chat_cost_unit"));
        }
        self.coord.validate_invariants()
    }

    /// Simple check: can a given role perform a given action on this KO.
    pub fn is_action_allowed(&self, role: &str, action: &str) -> bool {
        self.allowed_roles.iter().any(|r| r == role)
            && self.allowed_actions.iter().any(|a| a == action)
    }
}

/// Monotonic tick step helper for integration with corridor engines.
pub fn step_coord_with_tick(
    coord: &mut NeuroLoadCoord,
    tick_duration: Duration,
) -> Result<(), NeuroLoadError> {
    let increment = tick_duration.as_nanos() as u64;
    coord.timestamp_ns = coord
        .timestamp_ns
        .checked_add(increment)
        .ok_or(NeuroLoadError::OutOfRange("timestamp_ns"))?;
    coord.step()
}

/// Compare residuals to decide which coord is closer to the corridor center.
pub fn compare_residuals(a: &NeuroLoadCoord, b: &NeuroLoadCoord) -> Ordering {
    a.residual()
        .partial_cmp(&b.residual())
        .unwrap_or(Ordering::Equal)
}
