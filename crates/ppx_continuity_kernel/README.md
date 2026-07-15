# ppx_continuity_kernel

`ppx_continuity_kernel` is the non-actuating continuity and neurorights governance kernel for Prometheus-Praxis under `eco_restoration_shard`.[file:22][file:21]

It provides a read-only facade over SQLite continuity tables and neuroright corridor specs, computing aggregate continuity metrics and a sovereignty-safe governance decision (`Allow`, `Warn`, `Stop`) without ever touching actuators.[file:22][file:21]

## Role in Prometheus-Praxis

- Serves as the continuity spine for augmented citizens and systems, mirroring the healthcare/cybernetics continuity layer described in the Prometheus-Praxis research documents.[file:21]  
- Aggregates psychological continuity evidence (`PsychContinuityEvidence`) and system well-being components (`SystemWellBeingComponent`) for a `(subject_did, system_id, context_tag)` tuple.[file:22]  
- Resolves preferred continuity floors from user preferences (`UserContinuityPreference`) with a conservative default when no explicit preference is present.[file:22]  
- Projects raw neuroright corridor specs (`NeurorightCorridorSpec`) into normalized protection and risk bands (`EffectiveNeurorightBand`).[file:22]  
- Produces a non-actuating governance decision and JSON envelope for higher-level governance kernels (Prometheus-Praxis, Dyus-Archon, treaty gates).[file:21][file:22]  

## Core types

- `ContinuityKernel`  
  - Low-level data access layer over the continuity and neurorights tables in the SQLite spine.[file:22]  
  - Exposes read-only list/get methods for continuity evidence, system wellbeing components, user preferences, and corridor specs.[file:22]  

- `ContinuityGovernanceKernel`  
  - High-level facade that implements the governance logic over `ContinuityKernel`.[file:22]  
  - Methods:
    - `evaluate(ctx: ContinuityGovernanceContext) -> ContinuityGovernanceResult`  
    - `json_evaluate(ctx: ContinuityGovernanceContext) -> String`[file:22]  

- `ContinuityGovernanceContext`  
  - Input envelope for evaluation: `subject_did`, `system_id`, `context_tag`, optional time window, optional continuity floor override, optional minimum evidence count for `Allow`.[file:22]  

- `ContinuityAggregate` and `SystemWellBeingAggregate`  
  - Aggregated statistics for continuity scores and system wellbeing values over the requested window/context.[file:22]  

- `EffectiveNeurorightBand`  
  - Normalized corridor band with `min_protection_level` and `max_risk_tolerance` in `[0.0, 1.0]`.[file:22]  

- `ContinuityDecision`  
  - Enum: `Allow`, `Warn`, `Stop`, used as the non-actuating decision surface.[file:22]  

## Invariants and safety constraints

This crate is explicitly non-actuating and sovereignty-safe.[file:22][file:21]

- **Read-only kernel**  
  - No writes to SQLite, no network I/O, no actuator FFI; all methods are pure readers plus local aggregation.[file:22]  

- **Neurorights and continuity alignment**  
  - No identity classification and no rights downgrade based on continuity or corridor metrics; lower scores cannot yield more permissive decisions than higher scores.[file:21][file:22]  
  - Global risk-of-harm ceiling aligned with Prometheus-Praxis (`roh_ceiling = 0.30`) via ALN envelope.[file:21][file:22]  

- **Evidence sparsity handling**  
  - When continuity evidence is sparse (below `min_evidence_count_for_allow`), the kernel prefers `Warn` over `Allow` unless hard breaches force `Stop`.[file:22]  

- **Corridor breach semantics**  
  - Any neuroright corridor breach (protection below band minimum or risk above tolerance) yields `Stop` regardless of continuity scores.[file:22]  
  - Protection proxy: `SystemWellBeingAggregate.min_value`.  
  - Risk proxy: `1.0 - SystemWellBeingAggregate.avg_value`.  

These invariants are documented and enforced in the ALN shard `PPX-CONTINUITY-GOVERNANCE-ENVELOPE-001.aln` under `praxiskernel/aln`, giving CI a canonical predicate set to align Rust and ALN.[file:22][file:21]

## ALN alignment

The crate is bound to the ALN shard:

- Path: `praxiskernel/aln/PPX-CONTINUITY-GOVERNANCE-ENVELOPE-001.aln`[file:22]  
- Binding: `binding.kernel-crate = ppx_continuity_kernel`, `binding.kernel-module = continuity_governance_kernel`.[file:22]  
- The shard specifies:
  - Context schema (`subject_did`, `system_id`, `context_tag`, windows, overrides).[file:22]  
  - Aggregate fields for continuity and wellbeing, all in `[0.0, 1.0]` bands.[file:22]  
  - Corridor bands and decision predicates (`SPARSE-EVIDENCE`, `CONTINUITY-BELOW-FLOOR`, `CORRIDOR-BREACH`, `EXTREME-CONTINUITY-BREACH`).[file:22]  
  - Decision rules mapping predicates to `Allow`, `Warn`, `Stop`.[file:22]  
  - Alignment invariants (`CLAMP01`, `NON-ACTUATING`, `NO-RIGHTS-DOWNGRADE-BY-METRIC`).[file:22]  

CI and Kani can use this shard to assert:

- That all normalized bands are clamped to `[0.0, 1.0]` in Rust (via `clamp01`).[file:22]  
- That no Rust struct or method introduces actuator fields or side effects forbidden by the ALN envelope.[file:21][file:22]  
- That the Rust decision function respects the monotone rights ordering documented in ALN.[file:21][file:22]  

## Kani verification

`ppx_continuity_kernel` is expected to ship with Kani harnesses under `crates/ppx_continuity_kernel/tests`, mirroring the workspace requirement that Kani is wired at the workspace level and never optional.[file:22]

Recommended harnesses:

- Property that `clamp01` always returns values in `[0.0, 1.0]` and is idempotent.[file:22]  
- Property that corridor breaches and extreme continuity breaches always imply `ContinuityDecision::Stop`.[file:22]  
- Property that, under fixed corridors and preferred floor, decreasing continuity or wellbeing cannot move a decision from `Warn` or `Stop` back to `Allow`.[file:21][file:22]  

These harnesses keep the Rust decision logic provably aligned with the ALN predicates and sovereignty constraints.

## Non-goals

- No actuation or mission routing; this crate does not schedule nanoswarms, cyboquatic flows, or city controllers.[file:21]  
- No direct policy or treaty enforcement; higher-level treaty and governance crates (e.g. `prometheuspraxis`, `prometheuspraxistreatygates`) consume this kernel but enforce legal/ethical gates elsewhere.[file:21]  
- No raw neural or psychometric data storage; only scalar continuity metrics and aggregates are used, consistent with HASHONLY and privacy-first patterns described in your continuity and neurorights documents.[file:21]  

---

If you’d like, the next step can be: a small Kani test file under `crates/ppx_continuity_kernel/tests/continuity_governance_kernel_kani.rs` that proves the ALN decision rules hold for all normalized inputs, using only pure scalar reasoning.
