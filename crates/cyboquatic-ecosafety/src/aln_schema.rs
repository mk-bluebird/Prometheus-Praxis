// Filename: crates/cyboquatic-ecosafety/src/aln_schema.rs
// Shard schema and row validation for CyboquaticEcosafetyEnvelopePhoenix2026v1.aln.

use serde::{Deserialize, Serialize};

/// Raw ALN schema embedded at compile time.
///
/// This is the canonical source; all SQLite-bound rows must conform to it.[file:24]
pub const ECOSAFETY_ENVELOPE_PHOENIX_2026V1_ALN: &str =
    include_str!("../../qpudatashards/particles/CyboquaticEcosafetyEnvelopePhoenix2026v1.aln");

/// Allowed scalar kinds for shard fields.
#[derive(Clone, Copy, Debug, Eq, PartialEq, Serialize, Deserialize)]
pub enum ShardFieldKind {
    String,
    Float,
    Bool,
    Enum,
    Hex64Evidence,
    Hex64Signature,
    RiskCoord01,
    TrustScalar01,
}

/// Column descriptor parsed from ALN.[file:24]
#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct ShardField {
    pub name: String,
    pub kind: ShardFieldKind,
    pub mandatory: bool,
    pub tags: Vec<String>,
}

/// Parsed shard schema for a given ALN family.
#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct ShardSchema {
    pub family_id: String,
    pub fields: Vec<ShardField>,
}

/// Simplified shard update payload prior to SQLite insert.
///
/// All values are carried as strings and validated against `ShardSchema`
/// before any type-specific decoding.[file:23][file:25]
#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct ShardUpdate {
    pub family_id: String,
    pub values: std::collections::BTreeMap<String, String>,
}

/// Validation error kinds for shard updates.
#[derive(thiserror::Error, Debug)]
pub enum ShardValidationError {
    #[error("schema family mismatch: expected {expected}, got {got}")]
    FamilyMismatch { expected: String, got: String },
    #[error("missing mandatory field {0}")]
    MissingField(String),
    #[error("unexpected field {0}")]
    UnexpectedField(String),
    #[error("type error for field {field}: {reason}")]
    TypeError { field: String, reason: String },
    #[error("tag violation for field {field}: {reason}")]
    TagViolation { field: String, reason: String },
}

/// Parse a minimal ALN schema for `CyboquaticEcosafetyEnvelopePhoenix2026v1`.
///
/// This parser is intentionally narrow and tailored to the known schema layout,
/// which mirrors `CyboquaticIndustrialEcosafety2026v1.aln` and related shards.[file:24]
pub fn parse_ecosafety_envelope_schema() -> ShardSchema {
    // For robustness and zero guesswork, the schema is encoded explicitly here
    // based on the documented ALN particle.[file:24]
    use ShardFieldKind::*;

    let fields = vec![
        ShardField {
            name: "nodeid".into(),
            kind: String,
            mandatory: true,
            tags: vec!["identity".into()],
        },
        ShardField {
            name: "nodetype".into(),
            kind: Enum,
            mandatory: true,
            tags: vec!["identity".into()],
        },
        ShardField {
            name: "medium".into(),
            kind: Enum,
            mandatory: true,
            tags: vec!["identity".into()],
        },
        ShardField {
            name: "region".into(),
            kind: String,
            mandatory: true,
            tags: vec!["identity".into()],
        },
        ShardField {
            name: "site".into(),
            kind: String,
            mandatory: true,
            tags: vec!["identity".into()],
        },
        ShardField {
            name: "lat".into(),
            kind: Float,
            mandatory: true,
            tags: vec!["geo".into()],
        },
        ShardField {
            name: "lon".into(),
            kind: Float,
            mandatory: true,
            tags: vec!["geo".into()],
        },
        ShardField {
            name: "twindowstart".into(),
            kind: String,
            mandatory: true,
            tags: vec!["time".into()],
        },
        ShardField {
            name: "twindowend".into(),
            kind: String,
            mandatory: true,
            tags: vec!["time".into()],
        },
        ShardField {
            name: "mcapturedkg".into(),
            kind: Float,
            mandatory: true,
            tags: vec!["ceim".into()],
        },
        ShardField {
            name: "membodiedkg".into(),
            kind: Float,
            mandatory: true,
            tags: vec!["ceim".into()],
        },
        ShardField {
            name: "mpowerkgco2".into(),
            kind: Float,
            mandatory: true,
            tags: vec!["ceim".into()],
        },
        ShardField {
            name: "mrefkg".into(),
            kind: Float,
            mandatory: true,
            tags: vec!["ceim".into()],
        },
        ShardField {
            name: "ecobraw".into(),
            kind: Float,
            mandatory: true,
            tags: vec!["espd".into()],
        },
        ShardField {
            name: "k_knowledge".into(),
            kind: TrustScalar01,
            mandatory: true,
            tags: vec!["ker".into()],
        },
        ShardField {
            name: "e_ecoimpact".into(),
            kind: TrustScalar01,
            mandatory: true,
            tags: vec!["ker".into()],
        },
        ShardField {
            name: "r_riskofharm".into(),
            kind: RiskCoord01,
            mandatory: true,
            tags: vec!["ker".into()],
        },
        ShardField {
            name: "renergy".into(),
            kind: RiskCoord01,
            mandatory: true,
            tags: vec!["riskplane".into(), "energy".into()],
        },
        ShardField {
            name: "rhydraulics".into(),
            kind: RiskCoord01,
            mandatory: true,
            tags: vec!["riskplane".into(), "hydraulics".into()],
        },
        ShardField {
            name: "rbiology".into(),
            kind: RiskCoord01,
            mandatory: true,
            tags: vec!["riskplane".into(), "biology".into()],
        },
        ShardField {
            name: "rcarbon".into(),
            kind: RiskCoord01,
            mandatory: true,
            tags: vec!["riskplane".into(), "carbon".into()],
        },
        ShardField {
            name: "rmaterials".into(),
            kind: RiskCoord01,
            mandatory: true,
            tags: vec!["riskplane".into(), "materials".into()],
        },
        ShardField {
            name: "rbiodiv".into(),
            kind: RiskCoord01,
            mandatory: true,
            tags: vec!["riskplane".into(), "biodiversity".into()],
        },
        ShardField {
            name: "vresidual".into(),
            kind: Float,
            mandatory: true,
            tags: vec!["lyapunov".into()],
        },
        ShardField {
            name: "vresidualmax".into(),
            kind: Float,
            mandatory: true,
            tags: vec!["lyapunov".into()],
        },
        ShardField {
            name: "dttrust".into(),
            kind: TrustScalar01,
            mandatory: true,
            tags: vec!["trust".into()],
        },
        ShardField {
            name: "lane".into(),
            kind: Enum,
            mandatory: true,
            tags: vec!["lane".into()],
        },
        ShardField {
            name: "corridorpresent".into(),
            kind: Bool,
            mandatory: true,
            tags: vec!["contract".into()],
        },
        ShardField {
            name: "safestepok".into(),
            kind: Bool,
            mandatory: true,
            tags: vec!["contract".into()],
        },
        ShardField {
            name: "kerdeployable".into(),
            kind: Bool,
            mandatory: true,
            tags: vec!["contract".into()],
        },
        ShardField {
            name: "riskkernelversion".into(),
            kind: String,
            mandatory: true,
            tags: vec!["provenance".into()],
        },
        ShardField {
            name: "corridortableid".into(),
            kind: String,
            mandatory: true,
            tags: vec!["provenance".into()],
        },
        ShardField {
            name: "evidencehex".into(),
            kind: Hex64Evidence,
            mandatory: true,
            tags: vec!["provenance".into()],
        },
        ShardField {
            name: "signinghex".into(),
            kind: Hex64Signature,
            mandatory: true,
            tags: vec!["provenance".into()],
        },
    ];

    ShardSchema {
        family_id: "CyboquaticEcosafetyEnvelopePhoenix2026v1".into(),
        fields,
    }
}

fn parse_float_in_01(v: &str) -> Option<f64> {
    let val: f64 = v.parse().ok()?;
    if (0.0..=1.0).contains(&val) {
        Some(val)
    } else {
        None
    }
}

fn parse_hex64(v: &str) -> bool {
    let s = v.strip_prefix("0x").unwrap_or(v);
    s.len() == 64 && s.chars().all(|c| c.is_ascii_hexdigit())
}

/// Validate that a `ShardUpdate` conforms to `ShardSchema` before SQLite insertion.
///
/// This enforces:
/// - Family id match.
/// - Presence of all mandatory fields.
/// - No unexpected fields.
/// - Type correctness and simple tag-based invariants.[file:23][file:24][file:25]
pub fn validate_update(
    update: &ShardUpdate,
    schema: &ShardSchema,
) -> Result<(), ShardValidationError> {
    if update.family_id != schema.family_id {
        return Err(ShardValidationError::FamilyMismatch {
            expected: schema.family_id.clone(),
            got: update.family_id.clone(),
        });
    }

    let mut known = std::collections::BTreeSet::new();
    for field in &schema.fields {
        known.insert(field.name.as_str());
        if field.mandatory && !update.values.contains_key(&field.name) {
            return Err(ShardValidationError::MissingField(field.name.clone()));
        }
    }

    for key in update.values.keys() {
        if !known.contains(key.as_str()) {
            return Err(ShardValidationError::UnexpectedField(key.clone()));
        }
    }

    for field in &schema.fields {
        if let Some(raw) = update.values.get(&field.name) {
            if raw.is_empty() {
                return Err(ShardValidationError::TypeError {
                    field: field.name.clone(),
                    reason: "empty string".into(),
                });
            }

            use ShardFieldKind::*;
            match field.kind {
                String | Enum => {
                    // Any non-empty string acceptable; enums are checked at application layer.
                }
                Float => {
                    if raw.parse::<f64>().is_err() {
                        return Err(ShardValidationError::TypeError {
                            field: field.name.clone(),
                            reason: "expected float".into(),
                        });
                    }
                }
                Bool => {
                    if raw != "true" && raw != "false" {
                        return Err(ShardValidationError::TypeError {
                            field: field.name.clone(),
                            reason: "expected boolean 'true' or 'false'".into(),
                        });
                    }
                }
                RiskCoord01 | TrustScalar01 => {
                    if parse_float_in_01(raw).is_none() {
                        return Err(ShardValidationError::TypeError {
                            field: field.name.clone(),
                            reason: "expected float in [0,1]".into(),
                        });
                    }
                }
                Hex64Evidence | Hex64Signature => {
                    if !parse_hex64(raw) {
                        return Err(ShardValidationError::TypeError {
                            field: field.name.clone(),
                            reason: "expected 64-nybble hex string, optional 0x prefix".into(),
                        });
                    }
                }
            }

            // Basic tag-based invariants.
            if field.tags.contains(&"ker".to_string()) && field.kind == RiskCoord01 {
                let val = parse_float_in_01(raw).unwrap();
                if !(0.0..=1.0).contains(&val) {
                    return Err(ShardValidationError::TagViolation {
                        field: field.name.clone(),
                        reason: "KER coordinate out of [0,1]".into(),
                    });
                }
            }
        }
    }

    Ok(())
}
