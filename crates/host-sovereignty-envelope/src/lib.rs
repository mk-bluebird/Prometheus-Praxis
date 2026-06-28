// filename: ecore_restoration_shard/crates/host-sovereignty-envelope/src/lib.rs
// repo: mk-bluebird/eco_restoration_shard

#![forbid(unsafe_code)]
#![deny(warnings)]

use serde::{Deserialize, Serialize};
use std::time::SystemTime;

/// Lane classification reused across ecosafety, KER, and healthcare corridors.
/// Matches existing lane semantics (Research, ExpProd, Prod) without introducing new lanes.
#[derive(Debug, Clone, Copy, Serialize, Deserialize, PartialEq, Eq)]
pub enum Lane {
    Research,
    ExpProd,
    Prod,
    Quarantine,
}

/// High-level capability family.
/// These are non-actuating descriptors of what the system can do for a host.
#[derive(Debug, Clone, Copy, Serialize, Deserialize, PartialEq, Eq, Hash)]
pub enum CapabilityKind {
    Healthcare,
    Cybernetics,
    AugmentedCitizenship,
    EcoRestoration,
    NeuralInterface,
    TransitAccess,
}

/// Scalar corridors for knowledge, eco-impact, and residual risk.
/// These align with existing KER semantics (K, E, R in [0,1]) and RoH 0.30 ceilings.
#[derive(Debug, Clone, Copy, Serialize, Deserialize)]
pub struct KerBands {
    /// Knowledge factor, 0.0..1.0.
    pub k: f32,
    /// Eco-impact factor, 0.0..1.0.
    pub e: f32,
    /// Residual risk-of-harm, 0.0..1.0.
    pub r: f32,
}

impl KerBands {
    pub fn clamped(self) -> Self {
        Self {
            k: self.k.clamp(0.0, 1.0),
            e: self.e.clamp(0.0, 1.0),
            r: self.r.clamp(0.0, 1.0),
        }
    }
}

/// Core Risk-of-Harm corridors.
/// roh_ceiling must never exceed 0.30, matching the global Lyapunov RoH barrier.
#[derive(Debug, Clone, Copy, Serialize, Deserialize)]
pub struct RohCorridor {
    /// Maximum allowed RoH scalar for this envelope, must be <= 0.30.
    pub roh_ceiling: f32,
    /// Current RoH snapshot, normalized 0.0..1.0.
    pub roh_current: f32,
}

impl RohCorridor {
    pub fn is_within_ceiling(&self) -> bool {
        self.roh_ceiling <= 0.30 && self.roh_current <= self.roh_ceiling
    }
}

/// Neurorights floors encoded as booleans.
/// These are non-derogable; once true, they must never be flipped to false.
#[derive(Debug, Clone, Copy, Serialize, Deserialize)]
pub struct NeurorightsFloors {
    pub mental_privacy: bool,
    pub cognitive_liberty: bool,
    pub mental_integrity: bool,
    pub psychological_continuity: bool,
    pub mental_identity: bool,
}

impl NeurorightsFloors {
    pub fn all_satisfied(&self) -> bool {
        self.mental_privacy
            && self.cognitive_liberty
            && self.mental_integrity
            && self.psychological_continuity
            && self.mental_identity
    }
}

/// Host identity binding.
/// Tied to DID and Bostrom address and the authoritative eco_restoration_shard repo.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct HostIdentity {
    pub host_did: String,
    pub bostrom_address: String,
    pub repo_authority: String,
}

impl HostIdentity {
    pub fn is_primary_host(&self) -> bool {
        self.host_did == "did.aln.organic-host"
            && self.bostrom_address == "bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7"
            && self.repo_authority == "https://github.com/mk-bluebird/eco_restoration_shard"
    }
}

/// Sovereign host-level envelope.
/// This is the top-level, non-actuating summary of host corridors and identity.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct HostSovereigntyEnvelope {
    /// Immutable host identity spine.
    pub identity: HostIdentity,
    /// Global RoH corridor for this host.
    pub roh: RohCorridor,
    /// Global KER bands describing current state.
    pub ker: KerBands,
    /// Neurorights floors that must never be weakened.
    pub neurorights: NeurorightsFloors,
    /// Current lane classification for this envelope.
    pub lane: Lane,
    /// Timestamp when this envelope snapshot was observed.
    pub observed_at: SystemTime,
}

impl HostSovereigntyEnvelope {
    /// Hard invariant: identity must be the primary host and neurorights floors must hold.
    pub fn invariants_hold(&self) -> bool {
        self.identity.is_primary_host()
            && self.neurorights.all_satisfied()
            && self.roh.is_within_ceiling()
    }

    /// Monotone non-degradation check for envelope evolution.
    /// Returns false if any neurorights floor is weakened or RoH ceiling is raised.
    pub fn is_forward_only_evolution(&self, next: &HostSovereigntyEnvelope) -> bool {
        // Identity must remain bound to the same host and repo.
        if !self.identity.is_primary_host() || !next.identity.is_primary_host() {
            return false;
        }

        // Neurorights floors may stay the same or tighten; they must not flip from true to false.
        if self.neurorights.mental_privacy && !next.neurorights.mental_privacy {
            return false;
        }
        if self.neurorights.cognitive_liberty && !next.neurorights.cognitive_liberty {
            return false;
        }
        if self.neurorights.mental_integrity && !next.neurorights.mental_integrity {
            return false;
        }
        if self.neurorights.psychological_continuity && !next.neurorights.psychological_continuity {
            return false;
        }
        if self.neurorights.mental_identity && !next.neurorights.mental_identity {
            return false;
        }

        // RoH ceiling must not increase and must never exceed 0.30.
        if next.roh.roh_ceiling > self.roh.roh_ceiling {
            return false;
        }
        if next.roh.roh_ceiling > 0.30 {
            return false;
        }

        true
    }
}

/// Per-capability envelope.
/// These are non-transferable, forward-only capability bindings under the host envelope.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct CapabilityEnvelope {
    /// Capability family (healthcare, cybernetics, etc.).
    pub kind: CapabilityKind,
    /// Bound lane for this capability.
    pub lane: Lane,
    /// KER corridor specific to this capability.
    pub ker: KerBands,
    /// RoH corridor specific to this capability.
    pub roh: RohCorridor,
    /// Whether this capability is currently active.
    pub active: bool,
    /// Immutable binding to host sovereignty envelope identity.
    pub host_identity: HostIdentity,
    /// Timestamp for last capability evolution.
    pub last_evolved_at: SystemTime,
}

impl CapabilityEnvelope {
    /// Ensure identity binding and RoH corridor invariants for this capability.
    pub fn invariants_hold(&self, host: &HostSovereigntyEnvelope) -> bool {
        // Capability must be bound to the same primary host identity.
        if !self.host_identity.is_primary_host() {
            return false;
        }
        if !host.identity.is_primary_host() {
            return false;
        }

        // Capability RoH ceiling must not exceed host RoH ceiling and global 0.30 cap.
        if self.roh.roh_ceiling > host.roh.roh_ceiling {
            return false;
        }
        if self.roh.roh_ceiling > 0.30 {
            return false;
        }

        true
    }

    /// Forward-only evolution: cannot reduce K or E, cannot reduce lane, cannot raise RoH ceiling.
    pub fn is_forward_only_evolution(&self, next: &CapabilityEnvelope) -> bool {
        // Identity must remain bound to the same primary host.
        if !self.host_identity.is_primary_host() || !next.host_identity.is_primary_host() {
            return false;
        }

        // Lane monotonicity: lanes may only move toward stricter safety (Quarantine) or stay.
        match (self.lane, next.lane) {
            (Lane::Research, Lane::Research)
            | (Lane::Research, Lane::ExpProd)
            | (Lane::Research, Lane::Prod)
            | (Lane::Research, Lane::Quarantine)
            | (Lane::ExpProd, Lane::ExpProd)
            | (Lane::ExpProd, Lane::Prod)
            | (Lane::ExpProd, Lane::Quarantine)
            | (Lane::Prod, Lane::Prod)
            | (Lane::Prod, Lane::Quarantine)
            | (Lane::Quarantine, Lane::Quarantine) => {}
            // Any attempt to move from stricter to looser lane is forbidden.
            _ => return false,
        }

        // K and E must not decrease; R must not increase.
        let self_ker = self.ker.clamped();
        let next_ker = next.ker.clamped();

        if next_ker.k < self_ker.k {
            return false;
        }
        if next_ker.e < self_ker.e {
            return false;
        }
        if next_ker.r > self_ker.r {
            return false;
        }

        // RoH ceiling must not increase and must respect global 0.30 cap.
        if next.roh.roh_ceiling > self.roh.roh_ceiling {
            return false;
        }
        if next.roh.roh_ceiling > 0.30 {
            return false;
        }

        true
    }
}

/// Trait surface for Kani harness integration.
/// Any guard crate can implement this to expose sovereignty and capability evolution checks.
pub trait HostSovereigntyGuards {
    /// Evaluate whether a proposed host envelope evolution is admissible.
    fn evaluate_host_evolution(
        &self,
        before: &HostSovereigntyEnvelope,
        after: &HostSovereigntyEnvelope,
    ) -> bool;

    /// Evaluate whether a proposed capability envelope evolution is admissible.
    fn evaluate_capability_evolution(
        &self,
        before: &CapabilityEnvelope,
        after: &CapabilityEnvelope,
        host: &HostSovereigntyEnvelope,
    ) -> bool;
}

/// Default guard implementation wiring invariants directly.
#[derive(Debug, Default)]
pub struct DefaultHostSovereigntyGuards;

impl HostSovereigntyGuards for DefaultHostSovereigntyGuards {
    fn evaluate_host_evolution(
        &self,
        before: &HostSovereigntyEnvelope,
        after: &HostSovereigntyEnvelope,
    ) -> bool {
        before.invariants_hold() && after.invariants_hold() && before.is_forward_only_evolution(after)
    }

    fn evaluate_capability_evolution(
        &self,
        before: &CapabilityEnvelope,
        after: &CapabilityEnvelope,
        host: &HostSovereigntyEnvelope,
    ) -> bool {
        before.invariants_hold(host)
            && after.invariants_hold(host)
            && before.is_forward_only_evolution(after)
    }
}
