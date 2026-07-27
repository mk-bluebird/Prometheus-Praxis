# econet_tool_schema

Provider-agnostic JSON tool schema generator for EcoNet / Prometheus-Praxis AIsafe catalogs.

This crate turns ALN function catalogs and Rust type definitions into machine-readable tool schemas that AI-chat platforms and agents can use for structured tool-calling, without requiring cargo-based environments or external installers.

## Role

- Reads `econet.agent.function.catalog.v1.aln` and other approved ALN shards.
- Filters to AIsafe, non-actuating functions, based on:
  - `blastradiusclass = NONACTUATING_DIAGNOSTIC` or `GOVERNANCE_GUARD`.
  - `status = ACTIVE`.
- Emits JSON Schema / tool descriptors under `schemas/`:
  - Input schema files (per function).
  - Output schema files (per function).
  - Composite tool schema files (binding catalog metadata to schemas).

These artifacts are consumed by AI-chat backends, MCP tools, and other agents to discover and call EcoNet governance/diagnostic functions safely.

## Non-actuating constraints

- No actuators, hardware control, or ledger writes.
- Only non-actuating diagnostics and governance guards are emitted as tools.
- Actuating functions in healthcare/cybernetics/nanoswarm lanes remain governed by separate CI and ALN rules and are **not** exposed by this crate.

## Usage

- Offline / CI:
  - Run the `econet_tool_schema_gen` binary against the ALN catalog:
    - It reads `econet.agent.function.catalog.v1.aln`.
    - It emits JSON Schema files into `schemas/`.
  - These JSON files can be checked into the monorepo and served to AI platforms.

- Library:
  - Use `generate_tool_schema` with a `SchemaProvider` implementation to derive tool schemas from Rust types and governance metadata.
  - Use `generate_all_tool_schemas_from_aln` (with `aln_catalog` feature) to bulk-generate schemas from the ALN catalog.

## Prometheus-Praxis Shredding Snapshot Schema

This crate provides a dedicated helper function for generating the tool schema for the Prometheus-Praxis shredding snapshot diagnostic:

```rust
use econet_tool_schema::generate_prometheus_praxis_shredding_snapshot_schema;

let schema = generate_prometheus_praxis_shredding_snapshot_schema()?;
```

This generates:

- `schemas/tools.prometheus_praxis_get_shredding_snapshot_json.v1.json` — The full tool schema binding governance metadata to input/output JSON schemas.
- `schemas/prometheus_praxis_get_shredding_snapshot_json.input.json` — The input JSON schema.
- `schemas/prometheus_praxis_get_shredding_snapshot_json.output.json` — The output JSON schema.

### Non-actuating diagnostic role

The `prometheus_praxis_get_shredding_snapshot_json` function is strictly **non-actuating**:

- **Lanescope:** `RESEARCH`
- **Roleband:** `DIAGNOSTIC`
- **Blast-radius class:** `NONACTUATING_DIAGNOSTIC`
- **AI capability level:** `DIAGNOSTICONLY`

It returns a KER-weighted blast-radius and lane verdict snapshot for a Prometheus-Praxis shredding machine, suitable for AI-chat diagnostic queries but never for direct actuation.

## Sovereignty and safety

This crate is designed to:

- Respect sovereignty and data-as-labor policies.
- Avoid blacklisted primitives and hidden control panels.
- Provide transparent, typed tool surfaces for eco-restoration, governance, and diagnostics, while keeping actuation in explicitly gated, separately governed lanes.
