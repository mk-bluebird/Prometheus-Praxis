// filename: crates/steward_identity/src/lib.rs

//! Steward identity primitives and ALN ↔ Rust helpers.
//!
//! This crate is non-actuating and intended for use in qpudatashards,
//! governance spine rows, and ERSI / response shards.

use serde::{Deserialize, Serialize};
use thiserror::Error;

/// Canonical steward identity bound to a Bostrom DID and secondary identifiers.
#[derive(Debug, Clone, Serialize, Deserialize, PartialEq, Eq)]
pub struct StewardIdentity {
    /// Canonical Bostrom DID, e.g. `bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7`.
    pub signinghex: String,
    /// Human-facing alias, e.g. `wallet_fetch18sd2uj`.
    pub steward_id: String,
    /// Stable UUID for governance joins, e.g. `87cb8e02-c918-4b2a-aa40-36a8efa37e52`.
    pub steward_uuid: String,
    /// Role of this identity in the shard: `STEWARD`, `OPERATOR`, `AUDITOR`, ...
    pub role: String,
    /// Governance lane: `RESEARCH`, `EXP`, `PROD`, ...
    pub lane: String,
}

#[derive(Debug, Error)]
pub enum StewardIdentityError {
    #[error("signinghex is empty")]
    EmptySigningHex,
    #[error("steward_id is empty")]
    EmptyStewardId,
    #[error("steward_uuid is empty")]
    EmptyStewardUuid,
    #[error("role is empty")]
    EmptyRole,
    #[error("lane is empty")]
    EmptyLane,
    #[error("signinghex `{0}` does not start with required prefix `{1}`")]
    InvalidPrefix(String, String),
}

impl StewardIdentity {
    /// Construct a new identity, enforcing non-empty fields.
    pub fn new(
        signinghex: impl Into<String>,
        steward_id: impl Into<String>,
        steward_uuid: impl Into<String>,
        role: impl Into<String>,
        lane: impl Into<String>,
    ) -> Result<Self, StewardIdentityError> {
        let signinghex = signinghex.into();
        let steward_id = steward_id.into();
        let steward_uuid = steward_uuid.into();
        let role = role.into();
        let lane = lane.into();

        if signinghex.is_empty() {
            return Err(StewardIdentityError::EmptySigningHex);
        }
        if steward_id.is_empty() {
            return Err(StewardIdentityError::EmptyStewardId);
        }
        if steward_uuid.is_empty() {
            return Err(StewardIdentityError::EmptyStewardUuid);
        }
        if role.is_empty() {
            return Err(StewardIdentityError::EmptyRole);
        }
        if lane.is_empty() {
            return Err(StewardIdentityError::EmptyLane);
        }

        Ok(Self {
            signinghex,
            steward_id,
            steward_uuid,
            role,
            lane,
        })
    }

    /// Enforce that `signinghex` starts with the expected Bostrom DID prefix.
    ///
    /// This is a soft guard; full DID validation happens in higher layers.
    pub fn assert_bostrom_prefix(&self, expected_prefix: &str) -> Result<(), StewardIdentityError> {
        if !self.signinghex.starts_with(expected_prefix) {
            return Err(StewardIdentityError::InvalidPrefix(
                self.signinghex.clone(),
                expected_prefix.to_string(),
            ));
        }
        Ok(())
    }
}

/// Flattened EnergyMassWindow shard with embedded steward identity.
///
/// This mirrors the ALN schema used in hydrological / PFAS qpudatashards.
#[derive(Debug, Clone, Serialize, Deserialize, PartialEq)]
pub struct EnergyMassWindow {
    pub nodeid: String,
    pub region: String,
    pub medium: String,
    pub contaminant: String,
    pub twindowstart: String,
    pub twindowend: String,
    pub cin: f64,
    pub cout: f64,
    pub q: f64,
    pub powerw: f64,
    pub energyj: f64,
    pub massremovedkg: f64,
    pub etajperkg: f64,
    pub k: f64,
    pub e: f64,
    pub r: f64,
    pub vtmax: f64,
    pub corridorpresent: bool,
    pub safestepok: bool,
    pub evidencehex: String,
    /// Steward identity (Bostrom DID + aliases).
    pub steward: StewardIdentity,
}

impl EnergyMassWindow {
    /// Minimal constructor for test / tooling usage.
    ///
    /// Physics fields are placeholders here; real kernels fill them from CEIM.
    #[allow(clippy::too_many_arguments)]
    pub fn new(
        nodeid: impl Into<String>,
        region: impl Into<String>,
        medium: impl Into<String>,
        contaminant: impl Into<String>,
        twindowstart: impl Into<String>,
        twindowend: impl Into<String>,
        cin: f64,
        cout: f64,
        q: f64,
        powerw: f64,
        energyj: f64,
        massremovedkg: f64,
        etajperkg: f64,
        k: f64,
        e: f64,
        r: f64,
        vtmax: f64,
        corridorpresent: bool,
        safestepok: bool,
        evidencehex: impl Into<String>,
        steward: StewardIdentity,
    ) -> Self {
        Self {
            nodeid: nodeid.into(),
            region: region.into(),
            medium: medium.into(),
            contaminant: contaminant.into(),
            twindowstart: twindowstart.into(),
            twindowend: twindowend.into(),
            cin,
            cout,
            q,
            powerw,
            energyj,
            massremovedkg,
            etajperkg,
            k,
            e,
            r,
            vtmax,
            corridorpresent,
            safestepok,
            evidencehex: evidencehex.into(),
            steward,
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use serde_json::{from_str, to_string_pretty};

    const BOSTROM_DID: &str = "bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7";
    const STEWARD_ID: &str = "wallet_fetch18sd2uj";
    const STEWARD_UUID: &str = "87cb8e02-c918-4b2a-aa40-36a8efa37e52";

    #[test]
    fn steward_identity_round_trip() {
        let ident = StewardIdentity::new(
            BOSTROM_DID,
            STEWARD_ID,
            STEWARD_UUID,
            "STEWARD",
            "RESEARCH",
        )
        .expect("valid identity");

        ident
            .assert_bostrom_prefix("bostrom18sd2uj")
            .expect("prefix ok");

        let json = to_string_pretty(&ident).expect("serialize");
        let decoded: StewardIdentity = from_str(&json).expect("deserialize");

        assert_eq!(ident, decoded);
    }

    #[test]
    fn energy_mass_window_aln_like_round_trip() {
        let steward = StewardIdentity::new(
            BOSTROM_DID,
            STEWARD_ID,
            STEWARD_UUID,
            "STEWARD",
            "RESEARCH",
        )
        .expect("valid identity");

        let window = EnergyMassWindow::new(
            "Node-Gila-001",
            "Phoenix-AZ",
            "water",
            "PFBS",
            "2026-07-16T00:00:00Z",
            "2026-07-16T06:00:00Z",
            3.9e-9,
            3.9e-10,
            0.5,
            1200.0,
            2.592e7,
            1.0,
            2.592e7,
            0.93,
            0.91,
            0.13,
            0.45,
            true,
            true,
            "0xa1b2c3d4e5f67890",
            steward,
        );

        let json = to_string_pretty(&window).expect("serialize");
        let decoded: EnergyMassWindow = from_str(&json).expect("deserialize");

        assert_eq!(window.nodeid, decoded.nodeid);
        assert_eq!(window.steward.signinghex, decoded.steward.signinghex);
        assert_eq!(window.steward.steward_id, decoded.steward.steward_id);
        assert_eq!(window.steward.steward_uuid, decoded.steward.steward_uuid);
    }

    #[test]
    fn empty_signinghex_rejected() {
        let err = StewardIdentity::new(
            "",
            STEWARD_ID,
            STEWARD_UUID,
            "STEWARD",
            "RESEARCH",
        )
        .expect_err("empty signinghex should fail");

        matches!(err, StewardIdentityError::EmptySigningHex);
    }

    #[test]
    fn wrong_prefix_rejected() {
        let ident = StewardIdentity::new(
            "otherchain1xyz",
            STEWARD_ID,
            STEWARD_UUID,
            "STEWARD",
            "RESEARCH",
        )
        .expect("identity");

        let res = ident.assert_bostrom_prefix("bostrom18sd2uj");
        assert!(matches!(res, Err(StweardIdentityError::InvalidPrefix(_, _)) | Err(_)));
    }
}
