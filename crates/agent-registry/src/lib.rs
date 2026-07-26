#![forbid(unsafe_code)]

// Prometheus-Praxis agent registry core.
//
// This crate provides:
// - RoleBand: canonical role-band enum (must match prometheus-role-bands.v1.aln).
// - ShardBinding: agent -> shard mapping consistent with prometheus-shard-layout.v1.aln.
// - AgentManifest: registry entry tying agent id, role-band, shard-id, and ALN shard id.

use cosmwasm_std::Uint128;
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

    pub fn as_str(self) -> &'static str {
        match self {
            RoleBand::DataIngestor => "DataIngestor",
            RoleBand::Validator => "Validator",
            RoleBand::Coordinator => "Coordinator",
            RoleBand::Guardian => "Guardian",
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

    pub fn role_band_str(&self) -> &'static str {
        self.role_band.as_str()
    }
}

/// Minimal KV store abstraction compatible with CosmWasm Storage API.
pub trait KVStore {
    fn get(&self, key: &[u8]) -> Option<Vec<u8>>;
    fn set(&self, key: &[u8], value: &[u8]);
    fn remove(&self, key: &[u8]);
}

pub struct AgentGasStore<'a> {
    pub store: &'a dyn KVStore,
}

impl<'a> AgentGasStore<'a> {
    pub fn key(ticket_id: &str) -> Vec<u8> {
        let mut k = Vec::with_capacity("agent_gas_used/".len() + ticket_id.len());
        k.extend_from_slice(b"agent_gas_used/");
        k.extend_from_slice(ticket_id.as_bytes());
        k
    }

    pub fn get_gas_used(&self, ticket_id: &str) -> Uint128 {
        if let Some(bz) = self.store.get(&Self::key(ticket_id)) {
            Uint128::from(u128::from_le_bytes(Self::bytes_to_u128(&bz)))
        } else {
            Uint128::zero()
        }
    }

    pub fn set_gas_used(&self, ticket_id: &str, value: Uint128) {
        let mut buf = [0u8; 16];
        buf.copy_from_slice(&value.u128().to_le_bytes());
        self.store.set(&Self::key(ticket_id), &buf);
    }

    fn bytes_to_u128(bz: &[u8]) -> [u8; 16] {
        let mut out = [0u8; 16];
        let len = bz.len().min(16);
        out[..len].copy_from_slice(&bz[..len]);
        out
    }
}
