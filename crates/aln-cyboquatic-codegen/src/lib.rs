// filename: crates/aln-cyboquatic-codegen/src/lib.rs

//! ALN-driven code generation for cyboquatic C++ and SQLite schemas.
//!
//! This crate is non-actuating and focused on schema integrity. It parses ALN v2
//! particles and emits C++ header files and SQL DDL for Prometheus-Praxis.

#![forbid(unsafe_code)]
#![deny(missing_docs)]

use serde::Deserialize;

/// Simple representation of an ALN v2 particle field.
#[derive(Debug, Clone, Deserialize)]
pub struct AlnField {
    pub name: String,
    pub kind: String,
}

/// Representation of an ALN v2 particle used for code generation.
#[derive(Debug, Clone, Deserialize)]
pub struct AlnParticle {
    pub id: String,
    pub name: String,
    pub domain: String,
    pub fields: Vec<AlnField>,
}

/// A minimal ALN v2 document containing particles.
#[derive(Debug, Clone, Deserialize)]
pub struct AlnDocument {
    pub particles: Vec<AlnParticle>,
}

/// Map ALN field kind to C++ type.
fn cpp_type_for_kind(kind: &str) -> &'static str {
    match kind {
        "text" => "const char*",
        "float" => "double",
        "bool" => "bool",
        "hex256" => "const char*",
        _ => "const char*",
    }
}

/// Map ALN field kind to SQLite column type.
fn sql_type_for_kind(kind: &str) -> &'static str {
    match kind {
        "text" => "TEXT",
        "float" => "REAL",
        "bool" => "INTEGER",
        "hex256" => "TEXT",
        _ => "TEXT",
    }
}

/// Generate a C++ struct definition from an ALN particle.
pub fn generate_cpp_struct(particle: &AlnParticle, struct_name: &str) -> String {
    let mut out = String::new();
    out.push_str("// generated from ALN particle ");
    out.push_str(&particle.id);
    out.push('\n');
    out.push_str("struct ");
    out.push_str(struct_name);
    out.push_str(" {\n");
    for field in &particle.fields {
        let cpp_type = cpp_type_for_kind(&field.kind);
        out.push_str("    ");
        out.push_str(cpp_type);
        out.push(' ');
        out.push_str(&field.name);
        out.push_str(";\n");
    }
    out.push_str("};\n");
    out
}

/// Generate a SQLite table definition from an ALN particle.
pub fn generate_sql_table(particle: &AlnParticle, table_name: &str) -> String {
    let mut out = String::new();
    out.push_str("-- generated from ALN particle ");
    out.push_str(&particle.id);
    out.push('\n');
    out.push_str("CREATE TABLE IF NOT EXISTS ");
    out.push_str(table_name);
    out.push_str(" (\n");
    for (idx, field) in particle.fields.iter().enumerate() {
        let sql_type = sql_type_for_kind(&field.kind);
        out.push_str("    ");
        out.push_str(&field.name);
        out.push(' ');
        out.push_str(sql_type);
        if field.kind == "bool" {
            out.push_str(" NOT NULL CHECK (");
            out.push_str(&field.name);
            out.push_str(" IN (0, 1))");
        } else {
            out.push_str(" NOT NULL");
        }
        if idx + 1 < particle.fields.len() {
            out.push_str(",\n");
        } else {
            out.push('\n');
        }
    }
    out.push_str(");\n");
    out
}
