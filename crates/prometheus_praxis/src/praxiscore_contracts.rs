// FILE: crates/prometheus_praxis/src/praxiscore_contracts.rs
// ROLE: Core contracts and safety predicates for Prometheus-Praxis.
//       Mirrors ALN shard qpudatashards/PrometheusPraxis.Core.v1.aln,
//       enforces OwnerBinding, and exposes Kani-ready invariants.
// REQUIREMENTS:
//   - Rust edition 2024, rust-version 1.85.
//   - !forbid_unsafecode, no IO, no network, no hardware calls.

#![forbid(unsafe_code)]

use chrono::{DateTime, Utc};
use rust_decimal::Decimal;
use serde::{Deserialize, Serialize};

use crate::praxisgovernancekernel::{
    ActionDomain,
    ActionLane,
    AlnShardId,
    GovernanceDecision,
    KerSnapshot,
    LyapunovResidualSnapshot,
    MacroActionContext,
    PraxisGovernanceConfig,
    PraxisGovernanceKernel,
    QpuDataShardDescriptor,
    RohSnapshot,
    ROHCEILING,
};

/// OwnerBinding mirrors OwnerBindingCore table in qpudatashards/PrometheusPraxis.Core.v1.aln.
/// It ties the core engine to a host DID and the canonical Bostrom address.
#[derive(Debug, Clone, Serialize, Deserialize, PartialEq, Eq)]
pub struct OwnerBinding {
    pub hostdid: String,
    pub bostromaddress: String,
    pub coreid: String,
}

impl OwnerBinding {
    pub const CANONICAL_HOSTDID: &'static str = "did.aln.organic-host";
    pub const CANONICAL_BOSTROM: &'static str =
        "bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7";
    pub const CANONICAL_COREID: &'static str = "prometheus_praxis.core.v1";

    /// Construct a canonical OwnerBinding for this Prometheus-Praxis core.
    pub fn canonical() -> Self {
        Self {
            hostdid: Self::CANONICAL_HOSTDID.to_string(),
            bostromaddress: Self::CANONICAL_BOSTROM.to_string(),
            coreid: Self::CANONICAL_COREID.to_string(),
        }
    }

    /// Verify that the binding matches the canonical host DID, Bostrom address, and coreid.
    pub fn is_valid(&self) -> bool {
        self.hostdid == Self::CANONICAL_HOSTDID
            && self.bostromaddress == Self::CANONICAL_BOSTROM
            && self.coreid == Self::CANONICAL_COREID
    }
}

/// KEREnvelope encodes lane-specific thresholds for K, E, and R.
/// It is the Rust mirror of the KER envelope referenced in MonotoneSafetyCore.
#[derive(Debug, Clone, Serialize, Deserialize, PartialEq, Eq)]
pub struct KEREnvelope {
    pub kmin_production: Decimal,
    pub emin_production: Decimal,
    pub rmax_production: Decimal,
    pub kmin_pilot: Decimal,
    pub emin_pilot: Decimal,
    pub rmax_pilot: Decimal,
    pub kmin_research: Decimal,
    pub emin_research: Decimal,
    pub rmax_research: Decimal,
}

impl KEREnvelope {
    /// Check that a KerSnapshot is within the envelope for the given lane.
    pub fn ker_within_lane(&self, lane: ActionLane, ker: KerSnapshot) -> bool {
        let KerSnapshot { k, e, r } = ker;
        match lane {
            ActionLane::Production => {
                k >= self.kmin_production
                    && e >= self.emin_production
                    && r <= self.rmax_production
            }
            ActionLane::Pilot => {
                k >= self.kmin_pilot
                    && e >= self.emin_pilot
                    && r <= self.rmax_pilot
            }
            ActionLane::Research => {
                k >= self.kmin_research
                    && e >= self.emin_research
                    && r <= self.rmax_research
            }
        }
    }
}

/// RoHEnvelope encodes the global and optional per-domain RoH ceilings.
#[derive(Debug, Clone, Serialize, Deserialize, PartialEq, Eq)]
pub struct RoHEnvelope {
    pub global_ceiling: Decimal,
    pub eco_restoration_ceiling: Option<Decimal>,
    pub city_operations_ceiling: Option<Decimal>,
    pub cosmic_energy_ceiling: Option<Decimal>,
    pub macro_health_ceiling: Option<Decimal>,
}

impl RoHEnvelope {
    /// Check that the RoH snapshot is within ceilings.
    /// Global ceiling is hard; per-domain ceilings may be stricter.
    pub fn roh_within_ceiling(&self, roh: &RohSnapshot) -> bool {
        if roh.roh > self.global_ceiling {
            return false;
        }
        let domain_ceiling = match roh.domain {
            ActionDomain::EcoRestoration => self.eco_restoration_ceiling,
            ActionDomain::CityOperations => self.city_operations_ceiling,
            ActionDomain::CosmicEnergy => self.cosmic_energy_ceiling,
            ActionDomain::MacroHealth => self.macro_health_ceiling,
        };
        if let Some(dc) = domain_ceiling {
            if roh.roh > dc {
                return false;
            }
        }
        true
    }
}

/// TsafeEnvelope encodes corridor IDs and allowed signed distance.
/// This stays simple here; detailed Tsafe math lives in dedicated crates.
#[derive(Debug, Clone, Serialize, Deserialize, PartialEq, Eq)]
pub struct TsafeEnvelope {
    pub corridor_id: String,
    pub max_negative_distance: Decimal, // how far below Tsafe is allowed
}

impl TsafeEnvelope {
    /// Enforce Tsafe corridor: telemetry must not exceed the allowed negative distance.
    pub fn tsafe_within_band(&self, tsafe_signed_distance: Decimal) -> bool {
        // By convention, tsafe_signed_distance >= 0 is safe, negative is inside risk band.
        // We allow some negative distance but not beyond max_negative_distance.
        if tsafe_signed_distance >= Decimal::ZERO {
            true
        } else {
            // tsafe_signed_distance is negative; ensure |distance| <= |max_negative_distance|
            tsafe_signed_distance >= -self.max_negative_distance
        }
    }
}

/// LyapunovEnvelope encodes per-object Lyapunov caps and epsilon bands.
/// Here we use a simple global envelope; city-object envelopes live in a separate crate.
#[derive(Debug, Clone, Serialize, Deserialize, PartialEq, Eq)]
pub struct LyapunovEnvelope {
    pub vmax_global: Decimal,
    pub epsilon_research: Decimal,
    pub epsilon_pilot: Decimal,
    pub epsilon_production: Decimal,
}

impl LyapunovEnvelope {
    /// Enforce non-increasing Lyapunov residual within lane-specific epsilon bands.
    pub fn lyapunov_non_increasing(
        &self,
        lane: ActionLane,
        lyap: &LyapunovResidualSnapshot,
    ) -> bool {
        let eps = match lane {
            ActionLane::Research => self.epsilon_research,
            ActionLane::Pilot 
