// filename: crates/ecoper-joule/src/ceco.rs
#![forbid(unsafe_code)]

use serde::{Deserialize, Serialize};

use crate::models::EcoperJouleRecord;

#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct CecoCredit {
    pub recordid: uuid::Uuid,
    pub ceco: f64,
}

pub fn compute_ceco(record: &EcoperJouleRecord, delta_vt: f64, nonoffsettable_ok: bool) -> Option<CecoCredit> {
    if !nonoffsettable_ok {
        return None;
    }
    // Strict improvement required.
    if delta_vt >= 0.0 {
        return None;
    }
    // Normalize by energy; avoid division by zero.
    if record.energyjoules <= 0.0 {
        return None;
    }
    let raw = -delta_vt / record.energyjoules;
    // Corridor-based scaling: clamp to a reasonable band.
    let ceco = if raw < 0.0 {
        0.0
    } else if raw > 10.0 {
        10.0
    } else {
        raw
    };
    Some(CecoCredit {
        recordid: record.recordid,
        ceco,
    })
}
