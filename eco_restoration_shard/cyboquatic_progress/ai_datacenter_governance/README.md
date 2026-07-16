<!-- filename: eco_restoration_shard/cyboquatic_progress/ai_datacenter_governance/README.md
     purpose : Overview of the AI datacenter governance shard for collaborators and agents. -->

# AI Datacenter Governance Shard (2026‑07‑16)

This directory contains the foundational artifacts that integrate AI data centres
as first‑class Cyboquatic nodes within Prometheus‑Praxis.  They are part of
the daily progress lineage and are governed by the same Lyapunov/KER grammar as
canal machinery and bioremediation trays.

## Contents

| File | Description |
|------|-------------|
| `aln/AiDatacenterNode2026v1.aln` | Template ALN v2 particle: defines primary and guidance planes, corridor bounds, and the KER engine. |
| `docs/10axis_to_risk_mapping.md` | Exact mapping from the ten measurement axes to risk coordinates and K‑boost. |
| `docs/ker_derivation.md` | Mathematical derivation of K,E,R formulas with monotonicity proof. |
| `sql/daily_progress_ai_node.sql` | SQLite schema for daily AI node telemetry and KER evidence, plus a seed row. |
| `sql/register_ai_governance_anchor.sql` | Injects the hex anchor and file/particle bindings into the global Phoenix Hex Registry. |

## Quick Start for Agents / Contributors

1. **Instantiate a node:** copy `AiDatacenterNode2026v1.aln`, set the `node_id` and fill in the metric values for a given day.
2. **Compute telemetry:** using the mapping from `10axis_to_risk_mapping.md`, normalize raw sensor data and run the `compute_ker` function from the ALN particle (or a reference implementation).
3. **Log to SQLite:** insert a row into `daily_progress_ai_node` with the computed K,E,R and the appropriate `evidence_hex`.
4. **Register the hex:** update `phoenix_hex_registry.sqlite` with a new anchor for that day’s evidence, and link the SQLite file.

## Next‑Step Objects

- **Object 8:** CI lint rule that requires any new `.aln` or `.sql` file under this tree to be registered with a `phoenix_hex_anchor`.
- **Object 9:** ALN‑to‑SQL code generator that reads `AiDatacenterNode2026v1.aln` and produces the `daily_progress_ai_node` schema automatically.
- **Object 14:** Lane promotion/demotion ALN contract for AI nodes.

All contributions must be bound to the DID `bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7` and follow the non‑actuating, always‑improve principles of Cyboquatics.
