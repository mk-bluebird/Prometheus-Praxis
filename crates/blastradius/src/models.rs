use serde::{Deserialize, Serialize};
use uuid::Uuid;

use aln_core::HexHash;
use ecospine::KER;

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct Blastradius {
    pub object_id: Uuid,
    pub object_type: String,        // "node", "shard"
    pub scope: String,              // "local", "regional", "constellation"
    pub region_id: String,
    pub radius_km: f64,
    pub ker_band: (f64, f64),       // min, max for E or K/E composite
    pub continuity_grade: f64,      // 0..1
    pub sovereignty_tags: Vec<String>,
    pub governance_profile: String, // JSON string or policy ID
    pub hex_descriptor: HexHash,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct BlastradiusDescriptor {
    pub version: u8,
    pub scope_code: u8,
    pub continuity_grade: u8, // 0..100 mapped to 0..255
    pub ker_min: u16,         // KER band compressed
    pub ker_max: u16,
    pub sovereignty_mask: u32,
}

impl Blastradius {
    pub fn encode_hex(&self, descriptor: &BlastradiusDescriptor) -> HexHash {
        let mut bytes = Vec::with_capacity(10);
        bytes.push(descriptor.version);
        bytes.push(descriptor.scope_code);
        bytes.push(descriptor.continuity_grade);

        bytes.extend_from_slice(&descriptor.ker_min.to_be_bytes());
        bytes.extend_from_slice(&descriptor.ker_max.to_be_bytes());
        bytes.extend_from_slice(&descriptor.sovereignty_mask.to_be_bytes());

        HexHash::from_bytes(&bytes)
    }

    pub fn decode_hex(hex: &HexHash) -> Option<BlastradiusDescriptor> {
        let bytes = hex.to_bytes();
        if bytes.len() < 10 {
            return None;
        }

        let version = bytes[0];
        let scope_code = bytes[1];
        let continuity_grade = bytes[2];

        let ker_min = u16::from_be_bytes([bytes[3], bytes[4]]);
        let ker_max = u16::from_be_bytes([bytes[5], bytes[6]]);

        let sovereignty_mask = u32::from_be_bytes([bytes[7], bytes[8], bytes[9], bytes[10]]);

        Some(BlastradiusDescriptor {
            version,
            scope_code,
            continuity_grade,
            ker_min,
            ker_max,
            sovereignty_mask,
        })
    }
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct BlastradiusIntersection {
    pub object_a: Uuid,
    pub object_b: Uuid,
    pub compatible: bool,
    pub compatibility_score: f64,
    pub conflicts: Vec<String>,
}

pub fn intersect(
    a: &BlastradiusDescriptor,
    b: &BlastradiusDescriptor,
) -> BlastradiusIntersection {
    let sovereignty_overlap = a.sovereignty_mask & b.sovereignty_mask;
    let sovereignty_conflict = sovereignty_overlap != 0;

    let ker_overlap = !(a.ker_max < b.ker_min || b.ker_max < a.ker_min);
    let ker_conflict = !ker_overlap;

    let continuity_diff =
        (a.continuity_grade as f64 - b.continuity_grade as f64).abs() / 100.0;

    let mut conflicts = Vec::new();
    if sovereignty_conflict {
        conflicts.push("Sovereignty overlap detected".to_string());
    }
    if ker_conflict {
        conflicts.push("KER bands incompatible".to_string());
    }

    let compatible = !sovereignty_conflict && !ker_conflict;
    let compatibility_score = if compatible {
        1.0 - continuity_diff
    } else {
        0.0
    };

    BlastradiusIntersection {
        object_a: Uuid::nil(),
        object_b: Uuid::nil(),
        compatible,
        compatibility_score,
        conflicts,
    }
}
