// filename: crates/econet_tool_schema/src/lib.rs

#![forbid(unsafe_code)]

use serde::Serialize;
use schemars::JsonSchema;
use serde_json::Value;

use thiserror::Error;

#[derive(Debug, Error)]
pub enum ToolSchemaError {
    #[error("ALN catalog parsing failed: {0}")]
    AlnParse(String),

    #[error("Function descriptor not found for id: {0}")]
    FunctionNotFound(String),

    #[error("Schema generation failed: {0}")]
    Schema(String),

    #[error("IO error: {0}")]
    Io(String),
}

#[derive(Debug, Clone, Serialize, JsonSchema)]
pub struct FunctionDescriptor {
    /// Unique function ID from econet.agent.function.catalog.v1.aln.
    pub function_id: String,
    /// Backing FFI symbol name or view identifier.
    pub backing_symbol: String,
    /// Human-readable summary for AI agents.
    pub summary: String,
    /// Lane scope (e.g., RESEARCH, EXPPROD, PROD).
    pub lanescope: String,
    /// Role band (e.g., DIAGNOSTIC, GOVERNANCE).
    pub roleband: String,
    /// Blast-radius classification (e.g., NONACTUATING_DIAGNOSTIC).
    pub blastradius_class: String,
    /// AI capability level (e.g., DIAGNOSTICONLY).
    pub ai_capability_level: String,
    /// Path to input schema file or inline name.
    pub inputschema_path: String,
    /// Path to output schema file or inline name.
    pub outputschema_path: String,
    /// Version tag (e.g., 2026v1).
    pub versiontag: String,
    /// Status (e.g., ACTIVE, DEPRECATED).
    pub status: String,
}

#[derive(Debug, Clone, Serialize, JsonSchema)]
pub struct ToolSchema {
    /// Function ID from ALN catalog.
    pub function_id: String,
    /// Backing FFI symbol or view.
    pub backing_symbol: String,
    /// Summary description for AI-chat tool calling.
    pub summary: String,
    /// JSON Schema for inputs.
    pub input_schema: Value,
    /// JSON Schema for outputs.
    pub output_schema: Value,
    /// Lane scope for governance.
    pub lanescope: String,
    /// Role band (governance/diagnostic).
    pub roleband: String,
    /// Blast-radius classification.
    pub blastradius_class: String,
    /// AI capability level.
    pub ai_capability_level: String,
    /// Version tag.
    pub versiontag: String,
    /// Status in AIsafe catalog.
    pub status: String,
}

pub trait SchemaProvider {
    type Input: Serialize + JsonSchema;
    type Output: Serialize + JsonSchema;

    fn function_id(&self) -> &str;
    fn backing_symbol(&self) -> &str;
    fn summary(&self) -> &str;
    fn lanescope(&self) -> &str;
    fn roleband(&self) -> &str;
    fn blastradius_class(&self) -> &str;
    fn ai_capability_level(&self) -> &str;
    fn versiontag(&self) -> &str;
    fn status(&self) -> &str;
}

/// Generate a ToolSchema value using a SchemaProvider implementor.
///
/// This is used by binaries (src/main.rs) and offline scripts; it does not
/// require cargo tooling beyond Rust itself.
pub fn generate_tool_schema<P>(provider: P) -> Result<ToolSchema, ToolSchemaError>
where
    P: SchemaProvider,
{
    let input = schemars::schema_for!(P::Input);
    let output = schemars::schema_for!(P::Output);

    let input_schema =
        serde_json::to_value(&input).map_err(|e| ToolSchemaError::Schema(e.to_string()))?;
    let output_schema =
        serde_json::to_value(&output).map_err(|e| ToolSchemaError::Schema(e.to_string()))?;

    Ok(ToolSchema {
        function_id: provider.function_id().to_string(),
        backing_symbol: provider.backing_symbol().to_string(),
        summary: provider.summary().to_string(),
        input_schema,
        output_schema,
        lanescope: provider.lanescope().to_string(),
        roleband: provider.roleband().to_string(),
        blastradius_class: provider.blastradius_class().to_string(),
        ai_capability_level: provider.ai_capability_level().to_string(),
        versiontag: provider.versiontag().to_string(),
        status: provider.status().to_string(),
    })
}

/// Read econet.agent.function.catalog.v1.aln and derive ToolSchema entries.
///
/// This uses your existing ALN catalog parser (aln_catalog crate) and emits
/// tool schema JSONs for all ACTIVE, non‑actuating functions.
#[cfg(feature = "aln_catalog")]
pub fn generate_all_tool_schemas_from_aln(
    aln_path: &str,
) -> Result<Vec<ToolSchema>, ToolSchemaError> {
    use aln_catalog::FunctionCatalog;

    let catalog = FunctionCatalog::from_file(aln_path)
        .map_err(|e| ToolSchemaError::AlnParse(e.to_string()))?;

    let mut out = Vec::new();

    for f in catalog.functions() {
        if f.status != "ACTIVE" {
            continue;
        }

        if f.blastradius_class != "NONACTUATING_DIAGNOSTIC"
            && f.blastradius_class != "GOVERNANCE_GUARD"
        {
            continue;
        }

        let descriptor = FunctionDescriptor {
            function_id: f.function_id.clone(),
            backing_symbol: f.backingffi.clone(),
            summary: f.summary.clone(),
            lanescope: f.lanescope.clone(),
            roleband: f.roleband.clone(),
            blastradius_class: f.blastradius_class.clone(),
            ai_capability_level: f.aicapabilitylevel.clone(),
            inputschema_path: f.inputschema.clone(),
            outputschema_path: f.outputschema.clone(),
            versiontag: f.versiontag.clone(),
            status: f.status.clone(),
        };

        let tool_schema = ToolSchema {
            function_id: descriptor.function_id,
            backing_symbol: descriptor.backing_symbol,
            summary: descriptor.summary,
            input_schema: serde_json::json!({
                "$ref": descriptor.inputschema_path
            }),
            output_schema: serde_json::json!({
                "$ref": descriptor.outputschema_path
            }),
            lanescope: descriptor.lanescope,
            roleband: descriptor.roleband,
            blastradius_class: descriptor.blastradius_class,
            ai_capability_level: descriptor.ai_capability_level,
            versiontag: descriptor.versiontag,
            status: descriptor.status,
        };

        out.push(tool_schema);
    }

    Ok(out)
}
