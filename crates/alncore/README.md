# alncore

`alncore` is the non-actuating core library for ALNv2 documents, parsing, evaluation, and governance logic in the Prometheus-Praxis / eco_restoration_shard mono-repo.[file:99]

It provides:

- A typed `AlnDocument` model for ALNv2 corridor contracts.
- A parser and validators for ALN text.
- Evaluation functions for SafeStep rules and deploy kernels.
- Canonical JSON serialization for cross-language conformance.
- Hex-anchor DID binding verification via the `prometheus_praxis_hex_anchor` crate, keeping ALNv2 particles KER-safe and non-actuating.[file:99]

## Modules and re-exports

The crate is organized around three modules and re-exports their key types and functions:

- `model`:
  - `AlnDocument`
  - `DeployDecisionKernel`
  - `KerCompleteness`
  - `KerSnapshot`
  - `Lane`
  - `RepoManifest`
  - `SafeStepRule`
- `parser`:
  - `AlnParseError`
  - `parse_aln_str`
  - `validate_aln_document`
  - `validate_deploy_kernel`
  - `validate_ker_snapshot`
  - `validate_safesteep_rule`
- `eval`:
  - `check_move`
  - `eval_deploy`
  - `eval_safesteep`
  - `explain_deploy`
  - `DeployDecision`

These are all re-exported at the root of the crate for convenience.

## Canonical JSON bridge for ALNv2

To support cross-language tooling (C++, Kotlin, ALN), `alncore` exposes a canonical JSON view of an `AlnDocument`.[file:99]

- Function: `to_canonical_json(doc: &AlnDocument) -> String`
- Purpose:
  - Serialize all SafeStep rules, deploy kernels, override policies, KER snapshots, and the repo manifest into a single JSON object.
  - Act as a bridge format for C++ and other languages to consume ALNv2 contracts consistently.

Example:

```rust
use alncore::{AlnDocument, to_canonical_json};

fn dump_doc(doc: &AlnDocument) {
    let json = to_canonical_json(doc);
    println!("{}", json);
}
```

This JSON is the reference format for FFI functions like `ffi_eval_deploy_from_json`, ensuring all languages see the same contract structure.[file:99]

## Parsing and validating ALN

`alncore` provides a high-level helper to parse and validate ALN text:

- Function: `parse_and_validate_aln(input: &str) -> Result<AlnDocument, AlnParseError>`

It:

- Calls `parse_aln_str` to build an `AlnDocument` from ALN text.
- Runs `validate_aln_document` to enforce schema and corridor constraints.
- Returns either a fully validated document or an `AlnParseError` summarizing validation failures.

Example:

```rust
use alncore::{parse_and_validate_aln, AlnDocument};

let aln_text = r#"ALN ..."#; // full ALNv2 document
let doc: AlnDocument = parse_and_validate_aln(aln_text)?;
```

This is the entry point `alnctl` and other tools should use before any evaluation or deployment logic.

## SafeStep evaluation

SafeStep rules encode Lyapunov-style corridor constraints on residuals. `alncore` provides:

- `eval_safestep_rule(rule: &SafeStepRule, vt_prev: f64, vt_next: f64) -> bool`
  - Evaluates a single SafeStep rule against a residual step \(V_t\).
- `eval_all_safestep_rules(doc: &AlnDocument, vt_prev: f64, vt_next: f64) -> bool`
  - Applies all SafeStep rules in a document; returns true only if every rule admits the step.[file:99]

Example:

```rust
use alncore::{AlnDocument, eval_all_safestep_rules};

fn check_residual_step(doc: &AlnDocument, vt_prev: f64, vt_next: f64) -> bool {
    eval_all_safestep_rules(doc, vt_prev, vt_next)
}
```

These functions are the Rust source-of-truth for ALNv2 SafeStep logic; C++ or other languages should mirror them when implementing their own gates.

## Deploy kernel evaluation and move gating

Deploy kernels capture K,E,R corridor thresholds by lane. `alncore` exposes:

- `eval_deploy_kernels(doc: &AlnDocument, k: f32, e: f32, r: f32, lane: Lane) -> DeployDecision`
  - Evaluates K,E,R against all kernels in the document for a given lane.
- `check_move_with_snapshot(doc: &AlnDocument, snapshot: &KerSnapshot) -> DeployDecision`
  - Convenience function that uses a full `KerSnapshot` to gate a corridor move.[file:99]

Example:

```rust
use alncore::{AlnDocument, KerSnapshot, Lane, eval_deploy_kernels, check_move_with_snapshot};

fn gate_move(doc: &AlnDocument, snapshot: &KerSnapshot) -> bool {
    let decision = check_move_with_snapshot(doc, snapshot);
    decision.admissible
}

fn gate_deploy_direct(doc: &AlnDocument, k: f32, e: f32, r: f32, lane: Lane) -> bool {
    let decision = eval_deploy_kernels(doc, k, e, r, lane);
    decision.admissible
}
```

These functions encode the canonical K,E,R deploy decision logic used across Prometheus-Praxis.

## FFI-friendly evaluation from JSON

To enable C++ and other languages to use ALNv2 contracts, `alncore` provides an FFI-friendly struct and helper:

- `#[repr(C)] pub struct AlnKerDecision { pub admissible: bool, pub lane: i32, pub code: i32 }`
- `ffi_eval_deploy_from_json(doc_json: &str, k: f32, e: f32, r: f32, lane_code: i32) -> AlnKerDecision`

Usage pattern:

```rust
use alncore::{ffi_eval_deploy_from_json, Lane, to_canonical_json};

let doc_json = to_canonical_json(&doc);
let lane_code = 0; // 0=RESEARCH, 1=EXP, 2=SIM, 3=PROD

let decision = ffi_eval_deploy_from_json(&doc_json, k, e, r, lane_code);
assert_eq!(decision.lane, lane_code);
```

On the C++ side, you can:

- Expose `ffi_eval_deploy_from_json` via `extern "C"`.
- Map `lane_code` and `code` to your own enums.
- Use the canonical JSON produced by `to_canonical_json` to ensure contract alignment.

## Hex-anchor DID binding and zk corridor verification

`alncore` wires into the `prometheus_praxis_hex_anchor` crate to keep ALNv2 documents:

- Bound to the governance DID `bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7`.
- KER-safe and non-actuating according to RepoManifest corridors.[file:99]

Key helpers:

- `ker_policy_from_repo_manifest(doc: &AlnDocument) -> Option<KerPolicy>`
  - Maps `RepoManifest` `ker_target_k`, `ker_target_e`, `ker_target_r`, and `non_actuating_only` into a `KerPolicy`.
- `verify_document_hex_anchor(doc: &AlnDocument, zk_proof_bytes: &[u8]) -> Result<HexAnchorVerificationResult, String>`
  - Verifies:
    - `owner_did` matches `GOVERNANCE_DID`.
    - `evidence_hex` and `signing_hex` fields are bound to governance Ed25519 public key.
    - A zk-proof asserts that the hidden particle document obeys `KerPolicy` corridor bounds.

Example:

```rust
use alncore::{parse_and_validate_aln, verify_document_hex_anchor};

let aln_text = r#"ALN ..."#; // full ALNv2 document
let doc = parse_and_validate_aln(aln_text)?;

// zk_proof_bytes should come from an external prover that checks the hidden
// particle document against KerPolicy (K,E,R, non_actuating).
let zk_proof_bytes = vec![0x42, 0x99]; // placeholder for real proof

let hex_anchor_result = verify_document_hex_anchor(&doc, &zk_proof_bytes)?;
assert!(hex_anchor_result.ker_safe);
assert!(hex_anchor_result.non_actuating);
```

You can also verify a `RepoManifest` directly:

- `verify_repo_manifest_hex_anchor(manifest: &RepoManifest, zk_proof_bytes: &[u8], doc_signing_hex: &str) -> Result<HexAnchorVerificationResult, String>`

This keeps DID-bound corridor proofs and K,E,R triads inside `alncore`, allowing `alnctl`, Rust guard crates, and FFI layers to rely on a single, non-actuating source of truth for governance-safe ALNv2 particles.[file:99]

## Non-actuating corridor doctrine

All functions in `alncore`:

- Operate purely on documents, KER residuals, and signatures.
- Never control hardware, pumps, valves, or any actuators.
- Are intended for governance, audit, and AI tooling (e.g., `alnctl`, local LLM agents).[file:99]

This aligns with the Prometheus-Praxis doctrine:

- Keep EcoNet and ALNv2 contracts **readonly** and evidence-driven.
- Enforce K,E,R corridors and SafeStep rules before any downstream actuation.
- Maintain sovereignty and safety for augmented citizens, non-augmented citizens, and all lifeforms across the eco_restoration_shard.[file:99]
