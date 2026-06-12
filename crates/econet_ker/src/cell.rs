// File: crates/econet_ker/src/cell.rs

use crate::env::EnvClass;
use crate::roles::CyboRole;
use crate::policy::KerPolicy;

/// Core EcoNet energy cell definition.
#[derive(Debug, Clone)]
pub struct EcoCell {
    pub id: String,
    pub chemistry_family: String,
    pub environment_classes_allowed: Vec<EnvClass>,
    pub k: f32,
    pub e: f32,
    pub r: f32,
    pub ker: f32,
    pub eol_protocol_ref: String,
    pub preferred_roles: Vec<CyboRole>,
}

impl EcoCell {
    pub fn new(
        id: impl Into<String>,
        chemistry_family: impl Into<String>,
        environment_classes_allowed: Vec<EnvClass>,
        k: f32,
        e: f32,
        r: f32,
        eol_protocol_ref: impl Into<String>,
        preferred_roles: Vec<CyboRole>,
        policy: &KerPolicy,
    ) -> Self {
        let ker = policy.compute_ker(k, e, r);
        Self {
            id: id.into(),
            chemistry_family: chemistry_family.into(),
            environment_classes_allowed,
            k,
            e,
            r,
            ker,
            eol_protocol_ref: eol_protocol_ref.into(),
            preferred_roles,
        }
    }

    pub fn can_operate_in(&self, env: EnvClass) -> bool {
        self.environment_classes_allowed.contains(&env)
    }

    pub fn passes_policy(&self, env: EnvClass, policy: &KerPolicy, critical: bool) -> bool {
        if !self.can_operate_in(env) {
            return false;
        }
        if !policy.safety_gate(env, self.k, self.e, self.r) {
            return false;
        }
        if self.ker < policy.ker_min_general {
            return false;
        }
        if critical && self.ker < policy.ker_min_critical {
            return false;
        }
        true
    }
}
