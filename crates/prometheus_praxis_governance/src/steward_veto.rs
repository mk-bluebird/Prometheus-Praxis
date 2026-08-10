// File: crates/prometheus_praxis_governance/src/steward_veto.rs
#![forbid(unsafe_code)]

use std::collections::HashMap;

#[derive(Clone, Debug, PartialEq)]
pub struct WorkloadFrameRef {
    pub frame_id: String,
    pub canal_node: String,
    pub submitted_unix_s: i64,
}

#[derive(Clone, Debug, PartialEq)]
pub struct StewardVeto {
    pub frame_id: String,
    pub canal_node: String,
    pub steward_did: String,
    pub vetoed_unix_s: i64,
    pub reason: String,
}

#[derive(Clone, Copy, Debug, PartialEq)]
pub struct VetoPolicy {
    pub veto_window_s: i64,
}

#[derive(Clone, Debug, PartialEq)]
pub enum VetoError {
    InvalidInput,
    WindowExpired,
    UnauthorizedSteward,
    ConflictingVeto,
}

pub trait StewardAuthority {
    fn is_authorized_for_node(&self, steward_did: &str, canal_node: &str) -> bool;
}

#[derive(Default)]
pub struct StewardVetoRegistry {
    vetos: HashMap<String, StewardVeto>,
}

impl StewardVetoRegistry {
    pub fn submit(
        &mut self,
        authority: &impl StewardAuthority,
        policy: VetoPolicy,
        frame: &WorkloadFrameRef,
        veto: StewardVeto,
    ) -> Result<(), VetoError> {
        if policy.veto_window_s <= 0
            || frame.frame_id.trim().is_empty()
            || frame.canal_node.trim().is_empty()
            || veto.frame_id != frame.frame_id
            || veto.canal_node != frame.canal_node
            || veto.steward_did.trim().is_empty()
            || veto.reason.trim().is_empty()
            || veto.vetoed_unix_s < frame.submitted_unix_s
        {
            return Err(VetoError::InvalidInput);
        }

        if veto.vetoed_unix_s - frame.submitted_unix_s > policy.veto_window_s {
            return Err(VetoError::WindowExpired);
        }
        if !authority.is_authorized_for_node(&veto.steward_did, &frame.canal_node) {
            return Err(VetoError::UnauthorizedSteward);
        }
        if self.vetos.contains_key(&frame.frame_id) {
            return Err(VetoError::ConflictingVeto);
        }

        self.vetos.insert(frame.frame_id.clone(), veto);
        Ok(())
    }

    pub fn blocks(&self, frame_id: &str) -> bool {
        self.vetos.contains_key(frame_id)
    }

    pub fn veto_for(&self, frame_id: &str) -> Option<&StewardVeto> {
        self.vetos.get(frame_id)
    }
}

pub fn workload_is_admissible(
    accepted: bool,
    frame_id: &str,
    vetoes: &StewardVetoRegistry,
) -> bool {
    accepted && !vetoes.blocks(frame_id)
}

#[cfg(test)]
mod tests {
    use super::*;

    struct Authority;

    impl StewardAuthority for Authority {
        fn is_authorized_for_node(&self, did: &str, node: &str) -> bool {
            did == "bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7" && node == "canal-a"
        }
    }

    #[test]
    fn authorized_veto_blocks_an_accepted_frame() {
        let frame = WorkloadFrameRef {
            frame_id: "frame-1".into(),
            canal_node: "canal-a".into(),
            submitted_unix_s: 100,
        };
        let veto = StewardVeto {
            frame_id: "frame-1".into(),
            canal_node: "canal-a".into(),
            steward_did: "bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7".into(),
            vetoed_unix_s: 130,
            reason: "field inspection requires review".into(),
        };
        let mut registry = StewardVetoRegistry::default();

        registry
            .submit(&Authority, VetoPolicy { veto_window_s: 60 }, &frame, veto)
            .unwrap();

        assert!(!workload_is_admissible(true, "frame-1", &registry));
    }

    #[test]
    fn expired_veto_is_rejected() {
        let frame = WorkloadFrameRef {
            frame_id: "frame-2".into(),
            canal_node: "canal-a".into(),
            submitted_unix_s: 100,
        };
        let veto = StewardVeto {
            frame_id: "frame-2".into(),
            canal_node: "canal-a".into(),
            steward_did: "bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7".into(),
            vetoed_unix_s: 161,
            reason: "outside review window".into(),
        };
        let mut registry = StewardVetoRegistry::default();

        assert_eq!(
            registry.submit(&Authority, VetoPolicy { veto_window_s: 60 }, &frame, veto),
            Err(VetoError::WindowExpired)
        );
    }
}
