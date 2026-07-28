# filename: docs/CYBOQUATICCODEGENINTEGRATION.md
# license: MIT OR Apache-2.0

## Overview

This document describes how the ALN-driven code generation pipeline is integrated into the Prometheus-Praxis monorepo for cyboquatic bands. The pipeline consumes a single JSON contract file and produces C++ struct headers and SQL DDL that mirror ALN v2 particles exactly, eliminating manual drift and strengthening eco-governance.

## Inputs

- Contract file:
  - Path: `aln/aln_particles_cyboquatic.json`
  - Contents: Machine-readable definitions of four particles:
    - `cyboquatic.workload.kernel`
    - `cyboquatic.drainagedecay.kernel`
    - `cyboquatic.blastradius.governance`
    - `cyboquatic.energy.ecoperjoule.restoration`
  - Fields: `name`, `kind`, `domain`, `subdomain` for each particle.

The JSON file is derived from `.aln2` specifications and acts as the single source of truth for schema generation.

## Codegen tool

- Crate: `crates/aln-cyboquatic-codegen`
- Binary: `aln_codegen_main` (installed via `cargo run -p aln-cyboquatic-codegen` or as a standalone binary)
- Responsibilities:
  - Parse `aln/aln_particles_cyboquatic.json`.
  - For each particle, generate:
    - C++ header: `src/cpp/generated/<particle_name>_struct.hpp`.
    - SQL DDL: `db/generated/<particle_name>.sql`.

## Invocation

From the monorepo root:

```bash
cargo run --manifest-path crates/aln-cyboquatic-codegen/Cargo.toml -- \
  aln/aln_particles_cyboquatic.json \
  src/cpp/generated \
  db/generated
```

- Argument 1: Path to the JSON contract file.
- Argument 2: Output directory for C++ headers.
- Argument 3: Output directory for SQL DDL.

The tool creates or overwrites the following files:

- `src/cpp/generated/cyboquatic_workload_kernel_struct.hpp`
- `src/cpp/generated/cyboquatic_drainagedecay_kernel_struct.hpp`
- `src/cpp/generated/cyboquatic_blastradius_governance_struct.hpp`
- `src/cpp/generated/cyboquatic_energy_ecoperjoule_restoration_struct.hpp`
- `db/generated/cyboquatic_workload_kernel.sql`
- `db/generated/cyboquatic_drainagedecay_kernel.sql`
- `db/generated/cyboquatic_blastradius_governance.sql`
- `db/generated/cyboquatic_energy_ecoperjoule_restoration.sql`

## Integration points

### C++ engines

- Include generated headers in the corresponding engine files:
  - `src/cpp/cyboquatic_workload_engine.cpp`:
    - `#include "generated/cyboquatic_workload_kernel_struct.hpp"`
  - `src/cpp/cyboquatic_drainagedecay_engine.cpp`:
    - `#include "generated/cyboquatic_drainagedecay_kernel_struct.hpp"`
  - `src/cpp/cyboquatic_blastradius_engine.cpp`:
    - `#include "generated/cyboquatic_blastradius_governance_struct.hpp"`
  - Energy/restoration tooling:
    - `#include "generated/cyboquatic_energy_ecoperjoule_restoration_struct.hpp"`

Engines populate these structs when emitting diagnostic frames, ensuring field layouts match ALN particles.

### SQLite schemas

- Use generated DDL files to create staging or contract-aligned tables:
  - `db/generated/cyboquatic_workload_kernel.sql`
  - `db/generated/cyboquatic_drainagedecay_kernel.sql`
  - `db/generated/cyboquatic_blastradius_governance.sql`
  - `db/generated/cyboquatic_energy_ecoperjoule_restoration.sql`

These tables can be:

- Used directly as canonical storage for ALN-aligned frames.
- Joined or migrated into existing main-tree tables (`cyboquatic_daily_progress`, `cyboquatic_drainagedecay_index`, `cyboquatic_blast_radius_index`, `cyboquatic_energy_ecoperjoule_restoration`) with explicit mapping.

Triggers and invariants in main-tree schemas remain hand-authored, while structural alignment is guaranteed by codegen.

## Workflow

1. Edit ALN contracts:
   - Update `.aln2` files and regenerate `aln/aln_particles_cyboquatic.json` via internal tooling or manual export.

2. Regenerate code:
   - Run the codegen command to refresh `src/cpp/generated/*.hpp` and `db/generated/*.sql`.

3. Compile and migrate:
   - Rebuild C++ engines with the new headers.
   - Apply DDL migrations to development or test SQLite instances.

4. Verify:
   - Confirm that engine outputs populate all generated struct fields.
   - Validate that DB records conform to generated schemas and existing triggers.

## Governance and safety

- Non-actuating:
  - Generated C++ structs are pure data carriers; they do not introduce IO or hardware interactions.
  - Generated SQL DDL defines diagnostic tables only; triggers and actuation logic remain separate.

- Contract alignment:
  - All generated artifacts mirror ALN particles as represented in JSON, eliminating manual drift.
  - Any change to ALN contracts flows automatically into C++ and SQL via regeneration.

This integration closes the loop between ALN contracts, engine implementations, and DB schemas, moving cyboquatic work from research into a deployable, governance-aligned action pipeline.
