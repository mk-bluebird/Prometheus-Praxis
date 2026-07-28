# filename: docs/CYBOQUATICCODEGENPIPELINE.md

## Overview

This document describes a schema-driven code generation pipeline that binds ALN v2 particles to C++ structs and SQLite schemas for cyboquatic workloads, drainage-decay, and blast-radius diagnostics.

The goal is to eliminate drift between ALN definitions, C++ kernels, and DB tables, while maintaining non-actuating, eco-governance-aligned behavior across Prometheus-Praxis.

## Design principles

- ALN-first: ALN v2 particles define the canonical schema for diagnostic frames.
- Generated kernels: C++ struct definitions and field layouts are derived from ALN, not hand-written.
- Generated DB schema: SQLite table definitions and invariants are produced from ALN field and rule metadata.
- Non-actuating: All generated code is diagnostic-only; no hardware interfaces or actuation APIs are allowed.

## Pipeline stages

1. ALN parsing
   - Input: ALN v2 files under `aln/`.
   - Output: In-memory representation of particles, fields, KER axis definitions, and `require` rules.

2. Struct generation
   - For each particle in cyboquatic bands:
     - Generate a C++ header file with `struct` types for inputs and outputs.
     - Map ALN field types (text, float, hex256, bool) to C++ types (`const char*`, `double`, fixed-size byte arrays, `bool`).

3. SQL schema generation
   - For each particle:
     - Generate a `.sql` file under `db/` that defines a table with columns corresponding to ALN fields.
     - Generate triggers that enforce KER consistency and Lyapunov or RoH corridor invariants derived from ALN `require` clauses.

4. Integration and verification
   - Prometheus-Praxis crates and tools:
     - Include generated C++ headers in engine implementations.
     - Use generated SQL to initialize or migrate DBs.
   - Verification:
     - Compare existing hand-written schemas and structs with generated code.
     - Incrementally move toward full codegen control.

## Target ALN particles

- `aln/alnCyboquaticWorkloadKernel2026v1.aln2` (particle `cyboquatic.workload.kernel`)
- `aln/alnCyboquaticDrainageDecayKernel2026v1.aln2` (particle `cyboquatic.drainagedecay.kernel`)
- `aln/alnCyboquaticBlastRadiusGovernance2026v1.aln2` (particle `cyboquatic.blastradius.governance`)

## Implementation sketch

The initial implementation will use a Rust crate `crates/aln-cyboquatic-codegen` that:

- Reads ALN files from `workspacePrometheus-Praxis/aln`.
- Emits C++ headers under `workspacePrometheus-Praxis/src/cpp/generated/`.
- Emits SQL schemas under `workspacePrometheus-Praxis/db/generated/`.

Future work can add Kani proofs to the Rust side to verify that generated schemas and kernels respect eco-governance invariants.
