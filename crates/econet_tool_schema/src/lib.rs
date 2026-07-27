// filename: crates/econet_tool_schema/src/lib.rs

#![forbid(unsafe_code)]

use serde::Serialize;
use schemars::JsonSchema;
use serde_json::Value;
use thiserror::Error;

mod prometheus_praxis_shredding;
use prometheus_praxis_shredding::PrometheusPraxisShreddingSnapshotProvider;

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
    pub function_id: String,
    pub backing_symbol: String,
    pub summary: String,
    pub lanescope: String,
    pub roleband: String,
    pub blastradius_class: String,
    pub ai_capability_level: String,
    pub inputschema_path: String,
    pub outputschema_path: String,
    pub versiontag: String,
    pub status: String,
}

#[derive(Debug, Clone, Serialize, JsonSchema)]
pub struct ToolSchema {
    pub function_id: String,
    pub backing_symbol: String,
    pub summary: String,
    pub input_schema: Value,
    pub output_schema: Value,
    pub lanescope: String,
    pub roleband: String,
    pub blastradius_class: String,
    pub ai_capability_level: String,
    pub versiontag: String,
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
            input_schema: serde_json::json!({ "$ref": descriptor.inputschema_path }),
            output_schema: serde_json::json!({ "$ref": descriptor.outputschema_path }),
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

pub fn generate_prometheus_praxis_shredding_snapshot_schema(
) -> Result<ToolSchema, ToolSchemaError> {
    let provider = PrometheusPraxisShreddingSnapshotProvider;
    generate_tool_schema(provider)
}
