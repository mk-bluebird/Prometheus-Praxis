// filename: crates/econet_tool_schema/src/main.rs

use econet_tool_schema::generate_prometheus_praxis_shredding_snapshot_schema;
use std::fs;
use std::path::Path;

fn main() -> anyhow::Result<()> {
    // Generate the tool schema using the library function
    let tool_schema = generate_prometheus_praxis_shredding_snapshot_schema()?;

    // Create schemas directory if it doesn't exist
    let schemas_dir = Path::new("schemas");
    fs::create_dir_all(schemas_dir)?;

    // Write the main tool schema JSON
    let tool_schema_json = serde_json::to_string_pretty(&tool_schema)?;
    fs::write(
        schemas_dir.join("tools.prometheus_praxis_get_shredding_snapshot_json.v1.json"),
        tool_schema_json,
    )?;

    // Write input schema separately
    let input_schema_json = serde_json::to_string_pretty(&tool_schema.input_schema)?;
    fs::write(
        schemas_dir.join("prometheus_praxis_get_shredding_snapshot_json.input.json"),
        input_schema_json,
    )?;

    // Write output schema separately
    let output_schema_json = serde_json::to_string_pretty(&tool_schema.output_schema)?;
    fs::write(
        schemas_dir.join("prometheus_praxis_get_shredding_snapshot_json.output.json"),
        output_schema_json,
    )?;

    println!("Generated tool schema files in schemas/:");
    println!("  - tools.prometheus_praxis_get_shredding_snapshot_json.v1.json");
    println!("  - prometheus_praxis_get_shredding_snapshot_json.input.json");
    println!("  - prometheus_praxis_get_shredding_snapshot_json.output.json");

    Ok(())
}
