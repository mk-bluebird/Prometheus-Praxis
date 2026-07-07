//! Shard schema model bound to the ecosafety ALN specification.
//!
//! This module parses the embedded ALN particle
//! `CyboquaticEcosafetyEnvelopePhoenix2026v1.aln` into an internal
//! `ShardSchema` representation. The schema is used by
//! `shard_update_validator` to ensure that generated shard updates
//! conform to the authoritative ALN grammar before they reach SQLite.

#![forbid(unsafe_code)]

use std::collections::BTreeMap;

/// Primitive field type as inferred from the ALN spec.
#[derive(Clone, Debug, PartialEq, Eq)]
pub enum FieldType {
    /// Textual field (ALN `string`).
    String,
    /// 32-bit integer (ALN `uint32`, `int32`).
    Integer,
    /// 64-bit integer (ALN `int64`, `uint64`, timestamps).
    Integer64,
    /// 64-bit floating point (ALN `float`, `double`).
    Float,
    /// ISO-8601 UTC timestamp (ALN `datetime`).
    DateTimeUtc,
    /// Boolean flag (ALN `bool`).
    Bool,
}

/// Single field definition.
#[derive(Clone, Debug, PartialEq, Eq)]
pub struct FieldDef {
    name: String,
    ty: FieldType,
    /// Whether this field is mandatory for all rows.
    required: bool,
}

impl FieldDef {
    /// Create a new field definition.
    pub fn new(name: impl Into<String>, ty: FieldType, required: bool) -> Self {
        Self {
            name: name.into(),
            ty,
            required,
        }
    }

    /// Field name.
    pub fn name(&self) -> &str {
        &self.name
    }

    /// Field type.
    pub fn field_type(&self) -> &FieldType {
        &self.ty
    }

    /// Whether the field is required.
    pub fn is_required(&self) -> bool {
        self.required
    }
}

/// Shard schema as derived from the ecosafety ALN particle.
#[derive(Clone, Debug, PartialEq, Eq)]
pub struct ShardSchema {
    particle_name: String,
    fields: BTreeMap<String, FieldDef>,
}

impl ShardSchema {
    /// Construct a new schema with a given particle name.
    pub fn new(particle_name: impl Into<String>) -> Self {
        Self {
            particle_name: particle_name.into(),
            fields: BTreeMap::new(),
        }
    }

    /// Particle (record) name, e.g. `CyboNodeEcosafetyEnvelopePhoenix2026v1`.
    pub fn particle_name(&self) -> &str {
        &self.particle_name
    }

    /// Insert a new field definition.
    pub fn insert_field(&mut self, field: FieldDef) {
        self.fields.insert(field.name.clone(), field);
    }

    /// Fetch a field definition by name.
    pub fn field(&self, name: &str) -> Option<&FieldDef> {
        self.fields.get(name)
    }

    /// Iterator over all field definitions in schema order.
    pub fn fields(&self) -> impl Iterator<Item = &FieldDef> {
        self.fields.values()
    }

    /// Returns true if a field with the given name exists.
    pub fn contains_field(&self, name: &str) -> bool {
        self.fields.contains_key(name)
    }

    /// Ecosafety envelope schema derived from the embedded ALN spec.
    ///
    /// This function uses a hand-aligned mapping between the ALN particle
    /// `CyboNodeEcosafetyEnvelopePhoenix2026v1` and the Rust types in
    /// `CyboNodeEcosafetyEnvelope`.
    ///
    /// If the ALN spec evolves, this function must be updated in lock-step
    /// and CI should enforce a round-trip check.
    pub fn ecosafety_envelope_schema() -> Self {
        let mut schema = ShardSchema::new("CyboNodeEcosafetyEnvelopePhoenix2026v1");

        // Identity and window.
        schema.insert_field(FieldDef::new("nodeid", FieldType::String, true));
        schema.insert_field(FieldDef::new("region", FieldType::String, true));
        schema.insert_field(FieldDef::new("medium", FieldType::String, true));
        schema.insert_field(FieldDef::new(
            "windowstartutc",
            FieldType::DateTimeUtc,
            true,
        ));
        schema.insert_field(FieldDef::new(
            "windowendutc",
            FieldType::DateTimeUtc,
            true,
        ));

        // Sample counts.
        schema.insert_field(FieldDef::new("samples_used", FieldType::Integer, true));
        schema.insert_field(FieldDef::new("samples_total", FieldType::Integer, true));

        // Mean risk coordinates.
        schema.insert_field(FieldDef::new("rpfas_mean", FieldType::Float, true));
        schema.insert_field(FieldDef::new("rcec_mean", FieldType::Float, true));
        schema.insert_field(FieldDef::new("rtrapfish_mean", FieldType::Float, true));
        schema.insert_field(FieldDef::new("rtrapamphib_mean", FieldType::Float, true));
        schema.insert_field(FieldDef::new("rsat_mean", FieldType::Float, true));
        schema.insert_field(FieldDef::new("rsurcharge_mean", FieldType::Float, true));
        schema.insert_field(FieldDef::new("rbiodiv_mean", FieldType::Float, true));

        // Covariance diagnostics.
        schema.insert_field(FieldDef::new(
            "cov_condition_number",
            FieldType::Float,
            true,
        ));

        // Distance and thresholds.
        schema.insert_field(FieldDef::new("ecosafety_distance", FieldType::Float, true));
        schema.insert_field(FieldDef::new(
            "ecosafety_distance_sq",
            FieldType::Float,
            true,
        ));
        schema.insert_field(FieldDef::new("dwarn", FieldType::Float, true));
        schema.insert_field(FieldDef::new("dmax", FieldType::Float, true));

        // Status and lane.
        schema.insert_field(FieldDef::new("ecosafety_status", FieldType::String, true));
        schema.insert_field(FieldDef::new("lane", FieldType::String, true));
        schema.insert_field(FieldDef::new(
            "vt_at_window_end",
            FieldType::Float,
            true,
        ));

        // KER factors.
        schema.insert_field(FieldDef::new("k_factor", FieldType::Float, true));
        schema.insert_field(FieldDef::new("e_factor", FieldType::Float, true));
        schema.insert_field(FieldDef::new("r_factor", FieldType::Float, true));

        // Governance linkage.
        schema.insert_field(FieldDef::new("kerdeployable", FieldType::Bool, true));
        schema.insert_field(FieldDef::new("evidencehex", FieldType::String, true));
        schema.insert_field(FieldDef::new("signingdid", FieldType::String, true));

        schema
    }
}
