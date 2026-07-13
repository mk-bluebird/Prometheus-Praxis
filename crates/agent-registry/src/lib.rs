#![forbid(unsafe_code)]

// Prometheus-Praxis agent registry core.
//
// This crate provides:
// - RoleBand: canonical role-band enum (must match prometheus-role-bands.v1.aln).
// - ShardBinding: agent -> shard mapping consistent with prometheus-shard-layout.v1.aln.
// - AgentManifest: registry entry tying agent id, role-band, shard-id, and ALN shard id.

use serde::{Deserialize, Serialize};

pub const HOST_DID: &str = "didalnorganic-host";
pub const PRIMARY_BOSTROM_ADDRESS: &str =
    "bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7";
pub const ALN_MIGRATION_AUTHORITY: &str =
    "ALN.MIGRATION.CYBERCOREAUTHORITY.v1";

/// Role-band for agents, mirroring prometheus-role-bands.v1.aln.
#[derive(Clone, Copy, Debug, Serialize, Deserialize, PartialEq, Eq)]
pub enum RoleBand {
    DataIngestor,
    Validator,
    Coordinator,
    Guardian,
}

impl RoleBand {
    pub fn trust_band(self) -> &'static str {
        match self {
            RoleBand::DataIngestor => "low",
            RoleBand::Validator => "medium",
            RoleBand::Coordinator => "high",
            RoleBand::Guardian => "sovereign",
        }
    }
}

/// Parse a string into a RoleBand; used when loading ALN or config.
pub fn parse_role_band(s: &str) -> Option<RoleBand> {
    match s {
        "DataIngestor" => Some(RoleBand::DataIngestor),
        "Validator" => Some(RoleBand::Validator),
        "Coordinator" => Some(RoleBand::Coordinator),
        "Guardian" => Some(RoleBand::Guardian),
        _ => None,
    }
}

/// Agent -> shard binding (must satisfy prometheus-shard-layout.v1.aln invariants).
#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct ShardBinding {
    pub agent_id: String,
    pub role_band: RoleBand,
    pub shard_id: String,
}

impl ShardBinding {
    /// Check whether this binding is allowed under the shard layout ALN.
    pub fn is_allowed(&self) -> bool {
        match (self.role_band, self.shard_id.as_str()) {
            (RoleBand::DataIngestor, "Shard-1") => true,
            (RoleBand::Validator, "Shard-1") | (RoleBand::Validator, "Shard-2") => true,
            (RoleBand::Coordinator, "Shard-2") | (RoleBand::Coordinator, "Shard-3") => true,
            (RoleBand::Guardian, "Shard-Guard") => true,
            _ => false,
        }
    }
}

/// Registry manifest tying an agent id to its role-band, shard, and ALN spec.
#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct AgentManifest {
    pub agent_id: String,
    pub role_band: RoleBand,
    pub shard_id: String,
    pub aln_shard_id: String,
}

impl AgentManifest {
    /// Construct a manifest from primitive fields, failing if the role-band or shard are invalid.
    pub fn new(
        agent_id: String,
        role_band_str: &str,
        shard_id: String,
        aln_shard_id: String,
    ) -> Option<Self> {
        let role_band = parse_role_band(role_band_str)?;
        let binding = ShardBinding {
            agent_id: agent_id.clone(),
            role_band,
            shard_id: shard_id.clone(),
        };
        if !binding.is_allowed() {
            return None;
        }
        Some(Self {
            agent_id,
            role_band,
            shard_id,
            aln_shard_id,
        })
    }
}
