// File: crates/econet_ker/src/module.rs

use crate::cell::EcoCell;
use crate::env::EnvClass;
use crate::policy::KerPolicy;
use crate::roles::CyboRole;

/// A Cyboquatic module that consumes one or more EcoCells.
#[derive(Debug, Clone)]
pub struct EcoModule {
    pub id: String,
    pub env_class: EnvClass,
    pub role: CyboRole,
    pub cells: Vec<EcoCell>,
    pub critical: bool,
}

impl EcoModule {
    pub fn new(
        id: impl Into<String>,
        env_class: EnvClass,
        role: CyboRole,
        cells: Vec<EcoCell>,
        critical: bool,
    ) -> Self {
        Self {
            id: id.into(),
            env_class,
            role,
            cells,
            critical,
        }
    }

    /// Validate all cells in this module against KER policy and environment.
    pub fn validate(&self, policy: &KerPolicy) -> bool {
        if self.cells.is_empty() {
            return false;
        }
        self.cells.iter().all(|cell| {
            // Safety and KER thresholds.
            if !cell.passes_policy(self.env_class, policy, self.critical) {
                return false;
            }
            // Role-aware constraint: at least one preferred role should match for critical modules.
            if self.critical && !cell.preferred_roles.contains(&self.role) {
                return false;
            }
            true
        })
    }
}
