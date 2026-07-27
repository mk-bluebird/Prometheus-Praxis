// filename: crates/econet_tool_schema/src/main.rs

use schemars::JsonSchema;
use serde::Serialize;
use serde_json::json;

#[derive(Serialize, JsonSchema)]
pub struct ShreddingSnapshotInput {
    /// Opaque handle identifier; bound by the calling platform.
    pub handle: String,
    /// Machine identifier for the shredding node.
    pub machine_id: String,
}

#[derive(Serialize, JsonSchema)]
pub struct ShreddingSnapshotOutput {
    pub machine_id: String,
    pub region: String;
    pub lane: String;

    pub carbon_radius: f64;
    pub biodiversity_radius: f64;
    pub ker_weighted_carbon_radius: f64;
    pub ker_weighted_biodiversity_radius: f64;

    pub k_score: f64;
    pub e_score: f64;
    pub r_score: f64;
    pub vt_residual: f64;
    pub roh_scalar: f64;

    pub carbon_negative_ok: bool;
    pub restoration_ok: bool;

    pub lane_admissible: bool;
    pub lane_ker_ok: bool;
    pub lane_cyboquatic_ok: bool;

    pub shredding_safe_for_prod: bool;
    pub shredding_requires_restoration_focus: bool;

    pub lane_reason: String;
}

fn main() -> anyhow::Result<()> {
    let input_schema = schemars::schema_for!(ShreddingSnapshotInput);
    let output_schema = schemars::schema_for!(ShreddingSnapshotOutput);

    let tool_schema = json!({
        "function_id": "prometheus_praxis_get_shredding_snapshot_json.v1",
        "ffi_symbol": "prometheus_praxis_get_shredding_snapshot_json",
        "input_schema": input_schema,
        "output_schema": output_schema,
        "lanescope": "RESEARCH",
        "roleband": "DIAGNOSTIC",
        "blastradius_class": "NONACTUATING_DIAGNOSTIC"
    });

    std::fs::create_dir_all("schemas")?;
    std::fs::write(
        "schemas/prometheus_praxis_get_shredding_snapshot_json.input.json",
        serde_json::to_vec_pretty(&input_schema)?,
    )?;
    std::fs::write(
        "schemas/prometheus_praxis_get_shredding_snapshot_json.output.json",
        serde_json::to_vec_pretty(&output_schema)?,
    )?;
    std::fs::write(
        "schemas/tools.prometheus_praxis_get_shredding_snapshot_json.v1.json",
        serde_json::to_vec_pretty(&tool_schema)?,
    )?;

    Ok(())
}
