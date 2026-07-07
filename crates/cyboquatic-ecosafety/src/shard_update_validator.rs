//! Shard update validator bound to the ecosafety ALN schema.
//!
//! The functions in this module enforce that `ShardUpdate` instances
//! produced by diagnostic frames conform to the ALN-derived `ShardSchema`
//! before they are handed off to any SQLite writer. This is a purely
//! diagnostic, non-actuating layer.

#![forbid(unsafe_code)]

use crate::shard_schema::{FieldDef, FieldType, ShardSchema};
use crate::types::CyboNodeEcosafetyEnvelope;

/// Validation error for shard updates.
#[derive(Clone, Debug)]
pub enum ValidationError {
    /// A required field is missing or has an invalid value.
    MissingOrInvalidField(String),
    /// A field had an unexpected type or encoding.
    TypeMismatch(String),
}

/// Result alias for shard validation.
pub type ValidationResult<T> = Result<T, ValidationError>;

/// Validate a `CyboNodeEcosafetyEnvelope` against the ecosafety schema.
///
/// This enforces:
/// - Presence of all required fields.
/// - Basic type and range checks for key numeric and boolean values.
/// - UTF-8 sanity for string fields (guaranteed by Rust `String`).
///
/// More detailed checks (e.g. corridor consistency) should be layered on
/// top of this structural validator.
pub fn validate_update(
    envelope: &CyboNodeEcosafetyEnvelope,
    schema: &ShardSchema,
) -> ValidationResult<()> {
    // Identity and window.
    check_string(schema, "nodeid", envelope.nodeid())?;
    check_string(schema, "region", envelope.region())?;
    check_string(schema, "medium", envelope.medium())?;

    check_datetime(schema, "windowstartutc")?;
    check_datetime(schema, "windowendutc")?;

    if envelope.samples_used() == 0 {
        return Err(ValidationError::MissingOrInvalidField(
            "samples_used must be > 0".to_string(),
        ));
    }

    if envelope.samples_total() < envelope.samples_used() {
        return Err(ValidationError::MissingOrInvalidField(
            "samples_total must be >= samples_used".to_string(),
        ));
    }

    // Numeric checks: means and covariance diagnostics.
    check_finite(schema, "rpfas_mean", envelope.rpfas_mean())?;
    check_finite(schema, "rcec_mean", envelope.rcec_mean())?;
    check_finite(schema, "rtrapfish_mean", envelope.rtrapfish_mean())?;
    check_finite(schema, "rtrapamphib_mean", envelope.rtrapamphib_mean())?;
    check_finite(schema, "rsat_mean", envelope.rsat_mean())?;
    check_finite(schema, "rsurcharge_mean", envelope.rsurcharge_mean())?;
    check_finite(schema, "rbiodiv_mean", envelope.rbiodiv_mean())?;

    if !envelope.cov_condition_number().is_finite()
        || envelope.cov_condition_number() <= 0.0
    {
        return Err(ValidationError::MissingOrInvalidField(
            "cov_condition_number must be finite and > 0".to_string(),
        ));
    }

    // Distance and thresholds.
    if envelope.ecosafety_distance() < 0.0 || !envelope.ecosafety_distance().is_finite() {
        return Err(ValidationError::MissingOrInvalidField(
            "ecosafety_distance must be finite and >= 0".to_string(),
        ));
    }

    if envelope.ecosafety_distance_sq() < 0.0
        || !envelope.ecosafety_distance_sq().is_finite()
    {
        return Err(ValidationError::MissingOrInvalidField(
            "ecosafety_distance_sq must be finite and >= 0".to_string(),
        ));
    }

    if envelope.dwarn() < 0.0 || envelope.dmax() <= 0.0 {
        return Err(ValidationError::MissingOrInvalidField(
            "dwarn must be >= 0 and dmax > 0".to_string(),
        ));
    }

    if envelope.dwarn() > envelope.dmax() {
        return Err(ValidationError::MissingOrInvalidField(
            "dwarn must be <= dmax".to_string(),
        ));
    }

    // Status, lane, Vt.
    check_string(schema, "ecosafety_status", envelope.ecosafety_status())?;
    check_string(schema, "lane", envelope.lane())?;
    check_finite(schema, "vt_at_window_end", envelope.vt_at_window_end())?;

    // KER factors within canonical ranges [0, 1].
    check_unit_interval("k_factor", envelope.k_factor())?;
    check_unit_interval("e_factor", envelope.e_factor())?;
    check_unit_interval("r_factor", envelope.r_factor())?;

    // Governance and evidence.
    if !schema.contains_field("kerdeployable") {
        return Err(ValidationError::MissingOrInvalidField(
            "kerdeployable field missing in schema".to_string(),
        ));
    }

    check_string(schema, "evidencehex", envelope.evidencehex())?;
    check_string(schema, "signingdid", envelope.signingdid())?;

    Ok(())
}

fn check_string(schema: &ShardSchema, name: &str, value: &str) -> ValidationResult<()> {
    let def = expect_field(schema, name)?;
    if !matches!(def.field_type(), FieldType::String) {
        return Err(ValidationError::TypeMismatch(name.to_string()));
    }
    if value.trim().is_empty() {
        return Err(ValidationError::MissingOrInvalidField(format!(
            "string field '{}' must not be empty",
            name
        )));
    }
    Ok(())
}

fn check_datetime(schema: &ShardSchema, name: &str) -> ValidationResult<()> {
    let def = expect_field(schema, name)?;
    if !matches!(def.field_type(), FieldType::DateTimeUtc) {
        return Err(ValidationError::TypeMismatch(name.to_string()));
    }
    // Actual DateTimeUtc validation happens at the SQL/ALN boundary; here we
    // only ensure the field exists and has the right logical type.
    Ok(())
}

fn check_finite(schema: &ShardSchema, name: &str, value: f32) -> ValidationResult<()> {
    let def = expect_field(schema, name)?;
    if !matches!(def.field_type(), FieldType::Float) {
        return Err(ValidationError::TypeMismatch(name.to_string()));
    }
    if !value.is_finite() {
        return Err(ValidationError::MissingOrInvalidField(format!(
            "field '{}' must be finite",
            name
        )));
    }
    Ok(())
}

fn check_unit_interval(name: &str, value: f32) -> ValidationResult<()> {
    if !value.is_finite() || value < 0.0 || value > 1.0 {
        return Err(ValidationError::MissingOrInvalidField(format!(
            "field '{}' must be in [0, 1]",
            name
        )));
    }
    Ok(())
}

fn expect_field<'a>(schema: &'a ShardSchema, name: &str) -> ValidationResult<&'a FieldDef> {
    schema.field(name).ok_or_else(|| {
        ValidationError::MissingOrInvalidField(format!(
            "schema missing required field '{}'",
            name
        ))
    })
}
