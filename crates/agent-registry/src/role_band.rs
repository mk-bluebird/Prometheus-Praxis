#![forbid(unsafe_code)]

use serde::{Deserialize, Serialize};

pub const HOST_DID: &str = "didalnorganic-host";
pub const PRIMARY_BOSTROM_ADDRESS: &str =
    "bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7";
pub const ALN_MIGRATION_AUTHORITY: &str =
    "ALN.MIGRATION.CYBERCOREAUTHORITY.v1";

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

pub fn parse_role_band(s: &str) -> Option<RoleBand> {
    match s {
        "DataIngestor" => Some(RoleBand::DataIngestor),
        "Validator" => Some(RoleBand::Validator),
        "Coordinator" => Some(RoleBand::Coordinator),
        "Guardian" => Some(RoleBand::Guardian),
        _ => None,
    }
}
