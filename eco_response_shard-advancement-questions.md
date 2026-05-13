## 1. Manifest & Governance Finalization

### 1. Research-Question: How should the `econetrepoindex.sql` manifest be versioned and how do we handle breaking changes to its own schema?
The manifest itself may need to evolve (e.g., new fields for KER‑target corridors). A strategy for manifest schema evolution without breaking existing CI linters must be designed, including version tags and backward‑compatible defaults.

### 2. Detail-Query: What exact SQL column types and constraints should be used for the `kertargetk`, `kertargete`, `kertargetr` fields to enforce research‑lane bounds in the manifest?
These need `CHECK` constraints to automatically reject values outside the research corridor (e.g., `kertargetk >= 0.94`), but the bounds may differ for other repos later. The DDL must be finalised.

### 3. Objection-Identifier: Could an incorrect `ecosafetybinding` path in the manifest silently drift the repo away from the frozen grammar?
If the binding string is a plain text field, a typo would not be detected by SQL, only by a CI lint that checks against a whitelist. The whitelist maintenance and its own governance need to be defined.

### 4. Definition-Request: Define the exact format and location of the migration governance shard that proves eco_response_shard continuity across the mk‑bluebird move.
Fields required: old repo slug, new repo slug, mapping of old DB file paths to new, hex‑stamped spec hashes, `didowner`, `evidencehex`, `signinghex`. The ALN particle name and version must be frozen.

### 5. Research-Question: Should the manifest include an explicit registry of allowed dependent repositories (spine‑only, research‑only) to prevent accidental coupling to an ENGINE crate?
This would strengthen the `nonactuatingonly` guarantee but adds maintenance overhead. Need to evaluate trade‑offs and CI performance.

---
## 2. Bostrom DID Anchoring & Provenance

### 6. Definition-Request: Provide the canonical serialization order (field list and delimiters) for a `ResponseShardEcoMetrics2026v1` row that will be hashed into `evidencehex`.
The order must be deterministic and agreed upon globally. Should it follow the field declaration order in the ALN particle, or an explicit ordered tuple? Encoding rules for NULLs and floats must be specified.

### 7. Detail-Query: What exact key management scheme will sign `eco_response_shard` rows? Is it a single Bostrom key stored in CI secrets, or a multi‑sig signing service?
The security model affects whether the signing happens at backfill time or in a separate CI job. The risk of key leakage and signing authority delegation must be addressed.

### 8. Objection-Identifier: The current backfill code uses a hardcoded fallback DID (`bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7`) when the source `shardinstance` has an empty `signingdid`. This weakens provenance – is it acceptable, or should rows with missing DIDs be rejected entirely?
There is a risk of attaching a false authorship. A decision is needed: fallback DID as “backfill operator” or strict rejection with a special `unknown_did` marker.

### 9. Research-Question: How can the provenance chain be made economical for backfilled historical shards that lack parent evidence hashes?
We could generate fingerprints retroactively from the earliest source shard hashes, but that may require a separate bootstrap process and a “genesis” evidence hex. Determine the protocol for bootstrapping the chain.

### 10. Detail-Query: In the `response_shard` table, what is the exact hash function input for computing `evidencehex` from `parentevidencehex` plus row payload? Must it include the ALN spec hash of `ResponseShardEcoMetrics2026v1` to prevent schema‑version confusion?
The formula H( parentevidencehex || spec_hash || serialized_payload || kernel_version ) needs to be specified in the ProvenanceKernel ALN.

### 11. Definition-Request: Define the `SignerShard2026v1` schema that maps Bostrom addresses to public verification keys and permitted roles (e.g., backfill operator, researcher, auditor).
This shard must be query‑able by the CI provenance verifier and eventually published on‑chain.

---
## 3. Uncertainty Planes – rcalib & rsigma

### 12. Definition-Request: Provide the piecewise corridor kernel function that maps the ingest error scalar I (I = m N_missing + s N_schema + c N_corridor + u N_unit) to a normalised rcalib ∈ [0,1].
The function shape (linear, exponential, threshold‑based) affects how data quality risks amplify. It must be frozen as `RcalibKernel2026v1.aln` with explicit safe/gold/hard bands.

### 13. Detail-Query: What are the initial numeric values for the weights m, s, c, u in the error scalar I? Should they be configurable per region/medium, or global constants?
These weights determine sensitivity; they need justification and possibly a calibration study against known “good” vs “bad” ingest windows.

### 14. Research-Question: How should rsigma be decomposed into sub‑components (rdrift, rnoise, rbias, rloss) and how are those sub‑components aggregated into a single rsigma value?
Should it be a weighted sum, a maximum, or a Euclidean sum? The aggregation rule must preserve the property that each component can only add non‑negative risk.

### 15. Objection-Identifier: If rcalib and rsigma are derived from ingestion diagnostics that may themselves be incomplete, there is a recursion problem: what calibrates the calibrator?
Need a meta‑level data quality score for the ingest diagnostics shards themselves, or a bounded‑error assumption from a trusted calibration set.

### 16. Detail-Query: In the `IngestRcalibPhoenix2026v1` qpudatashard, what is the semantic of `twindowstart` and `twindowend`? Are they UTC timestamps or monotonic block numbers? How should they align with `shardinstance` time windows?
The linkage to backfill logic requires that these windows exactly match the time range used for K/E/R windowing in the spine.

### 17. Research-Question: Should `rcalib` and `rsigma` be stored as raw values in the `response_shard` table, or only their corridor‑mapped risks and derived trust metrics (D_data, D_combined)?
Storing both allows auditability but doubling columns may lead to inconsistency if not kept in sync. Determine the minimal truthful schema.

---
## 4. CI Invariants & Enforcement

### 18. Detail-Query: For the “no corridor, no build” CI rule, what is the exact source of truth for corridor definitions? Is it the `corridordefinition` table from the spine SQLite, or a canonical ALN file like `ecosafety.corridors.v2.aln`?
The CI script must load the same set as the governance plane; any discrepancy would cause false passes or failures.

### 19. Definition-Request: Define the ALN particle `ecosafety.dataqualityinvariants.v1` that formally states the monotonicity condition: If rcalib or rsigma increases between updates, then K_next ≤ K_prev, E_next ≤ E_prev, R_next ≥ R_prev.
The condition must be expressed in a machine‑readable form that `kerdeployable` can parse and enforce.

### 20. Objection-Identifier: The CI enforcement of monotonicity depends on reliable “previous state”. How do we guarantee that the history of `response_shard` rows is immutable and free from tampering?
If an attacker can delete previous rows, the CI could be bypassed. This suggests the need for hash‑chained immutability (like a simple append‑only log) within the SQLite database.

### 21. Detail-Query: What is the precise SQL query and tolerance used to detect a violation of vt_after ≥ vt_before in the `response_calib_sigma` table, accounting for floating‑point rounding errors?
The code snippet uses `vt_after + 1e-12 < vt_before`. Is 1e‑12 appropriate for all shard families, or should it be scaled by typical vt magnitudes?

### 22. Research-Question: How should the CI handle cases where a new shard lacks a prior version for comparison? Should it be treated as always passing the monotonicity check, or be gated by a “baseline” requirement?
Determining initial state trust is essential for first‑time submissions.

### 23. Detail-Query: For the `response_calib_sigma.deploy_decision` consistency check, what is the exact mapping from (rcalib, rsigma, lane, K, E, R) to decisions like “BlockedByCalib” or “BlockedByRisk”? Provide a decision table or function.
This must be codified in the `kerdeployable` kernel and CI rule, not left to ad‑hoc SQL.

---
## 5. Schema Evolution & ALN Registry

### 24. Definition-Request: Formalise the `SchemaEvolution2026v1` qpudatashard that proves a schema upgrade does not regress KER.
Fields required: `old_spec_hash`, `new_spec_hash`, `replay_dataset_hash`, `K_before`, `E_before`, `R_before`, `K_after`, `E_after`, `R_after`, `proof_type` (e.g., “replay-over-matching-keys”), `evidence_hex`.

### 25. Research-Question: When a new version of `EcoNetSchemaShard` (e.g., v2) mandates new columns like `D_combined`, how do we handle backfill of historical shards that lack those values?
Options: a default –1 (unknown) that blocks promotion, or a lazy computation from available data. The risk of pollution by default values must be assessed.

### 26. Detail-Query: In the ALN particle `ResponseShardEcoMetrics2026v1`, what field constraints (e.g., `residual_score ∈ [0,1]`, `rcalib ∈ [0,1]`) should be enforced at the database level via CHECK constraints, versus trusting the writer code?
Determining the boundary between database integrity and application logic.

### 27. Objection-Identifier: The current `ALNSPECHASHHEX` values (e.g., `0xe0c0a1f27b9384d5`) appear hardcoded. How are they generated? If they are fake placeholders, the system is vulnerable to collisions if real hashing is later introduced.
A serious security review: the spec hashing algorithm and seeding must be finalised before any production use.

---
## 6. Backfill Integrity & Migration

### 28. Detail-Query: The backfill tool’s `load_shard_summaries` performs a LEFT JOIN to `knowledgeecoscore` and falls back to the raw `kmetric` if no meta‑score exists. Is this fallback semantically correct? Should it instead insert a row with a special “raw” flag and a lower K/E?
The fallback may inflate perceived knowledge and thus risk.

### 29. Objection-Identifier: The backfill process writes `residual_score = vt_max`. However, the ALN spec says `residual_score` is a scalar in 0..1, while `vt_max` may not be normalized. This discrepancy will cause corridor violations and invalidate proofs.
An explicit normalization formula (e.g., vt_max / max_possible_Vt) or a corridor re‑mapping must be applied.

### 30. Research-Question: How do we handle duplicates in the backfill? If the script is re‑run on the same source data, should it overwrite existing `response_shard` rows, insert new ones, or be idempotent based on `shard_id` + `topic_tag`?
This affects CI replayability and data integrity.

### 31. Definition-Request: Define the backfill metadata table `response_backfill_meta` to include the ranges of `shard_id` covered, the source econet database hash (if available), and the backfill tool version.
This metadata will be essential for auditing what data the response index currently reflects.

---
## 7. Data Quality & Trust Metrics

### 32. Detail-Query: What is the formula for `D_sensor` and where is it stored? The trust metric `D_combined = D_sensor * D_data` relies on a sensor trust that may come from a separate plane. How is it linked to the response row?
If `D_sensor` is missing, does `D_combined` default to `D_data`, or must it be NULL and block deployment?

### 33. Research-Question: Should the `D_combined` threshold for PROD lanes be absolute (e.g., 0.85) or relative to the corridor‑mapped “gold” band of a combined trust risk coordinate?
An absolute threshold is simpler but may not adapt to different sensor families. A corridor‑based system would be more consistent with the rest of the design.

### 34. Definition-Request: Create an ALN variant of the `response_shard` that separates “adjusted K/E” (after data‑quality penalties) into `k_adj` and `e_adj`. Provide the exact penalty function: e.g., k_adj = k_raw * f(rcalib, rsigma) with f monotonically decreasing.
The penalty kernel must be publishable as an independent, hex‑stamped spec.

### 35. Objection-Identifier: Currently the backfill code computes `k_factor` etc. from `knowledgeecoscore`, but `k_factor` may already incorporate some uncertainty. If we later apply a fresh `rcalib` penalty, we risk double‑counting risk and over‑penalising K. Clarify the intended layering.
Need to define whether `k_factor` is a pure meta‑knowledge score independent of ingest quality, or if it already contains an ingest modifier.

---
## 8. Non‑compensation & PlaneWeights

### 36. Detail-Query: For eco_restoration_shard’s `PlaneWeightsShard2026v1`, how is the `carbon_nonoffsettable` flag actually enforced in `kerdeployable`? Provide the expected algorithm that rejects any compensation where r_carbon increases while other risks drop.
This will be a critical part of the CI suite; it must be specified with an example.

### 37. Research-Question: Should non‑offsettability be absolute, or should there be a grace region within the corridor’s safe band where minor fluctuations are allowed without triggering a hard degradation?
A too‑strict rule might block scientifically benign variation, while too loose defeats the purpose.

### 38. Definition-Request: Define the `PlaneWeightsShard2026v1.aln` particle, including the list of planes (carbon, materials, biodiversity, hydro, data), per‑plane weights, non‑offsettable flags, and uncertainty caps for PROD lanes.
This spec is foundational for eco‑restoration proofs and must be hex‑stamped and registered.

---
## 9. Discovery Spine & Agent Integration

### 39. Detail-Query: What exact SQL rows must be inserted into the econet‑index discovery tables (`repo`, `repofile`, `alnschema`, etc.) to register `eco_response_shard` and its `ResponseShardEcoMetrics2026v1` particle?
Provide the complete INSERT statements that would allow an AI agent to discover the repo’s role and shards.

### 40. Research-Question: How should agentic AI (e.g., a chatbot) query eco_response_shard for “all PROD shards with E ≥ 0.9 and D_combined ≥ 0.85” via a read‑only API? Design a minimal REST or SQL query interface that respects non‑actuation.
Define endpoints, authentication (none), rate limiting, and canonical SQL views.

### 41. Definition-Request: Produce a CSV “template” file for `ResponseShardEcoMetrics2026v1` with a header row and a single example row, so that researchers can easily contribute valid data.
This satisfies the “ready‑to‑use templates” quick action and lowers the barrier to entry.

---
## 10. Rust Crate Structure & Config.toml

### 42. Detail-Query: What should be the `Cargo.toml` workspace layout for `eco_response_shard`? It likely needs crates: `eco_response_shard_backfill` (binary), `eco_response_shard_cilib` (CI helpers), and `eco_response_shard` (library for reuse). Provide a sample workspace `Cargo.toml`.
This is essential for pre‑wiring the project.

### 43. Research-Question: How can we structure the `eco_response_shard` library crate to implement a `NonActuatingWorkload` trait (per your eco‑system) without introducing any actuator dependencies, even transitively?
This design must be proven at compile time using feature flags or conditional compilation.

### 44. Detail-Query: For the backfill binary, what is the minimal set of `[dependencies]` (rusqlite, serde, etc.) and their exact version ranges needed to build and pass CI without security warnings?
Creating a deterministic `Cargo.lock` requires pinned versions.

### 45. Objection-Identifier: If the backfill binary is run with write access to the eco_response_shard database, it could accidentally corrupt the index. Should it be strictly separated into a read‑only query layer and a one‑time backfill tool? How do we enforce that separation in CI?
The risk of an in‑place backfill rewriting data needs mitigation.

---
## 11. File & Directory Organization

### 46. Definition-Request: Provide a recommended directory tree for `eco_response_shard` including `src/`, `sql/`, `.econet/`, `qpudatashards/`, `research/`, `.github/workflows/`, and `config/`.
This standardisation aids all future contributors and CI scripts.

### 47. Detail-Query: Where should the compiled `kerdeployable` and `econetrepomanifest-lint` binaries reside relative to `eco_response_shard`? Should they be built from a central `ecosystem-tools` repository and symlinked, or vendored as sub‑crates?
Decision affects CI complexity and versioning.

---
## 12. Testing & Validation Strategies

### 48. Research-Question: How can we create a small, synthetic econet‑index SQLite database (with known good and bad shards) to use as a test fixture for the backfill and CI invariants?
This test harness would dramatically improve code quality and allow rapid experimentation with corridor thresholds.

### 49. Detail-Query: What test cases are needed to verify the monotonicity invariant across multiple backfill runs? Provide a matrix of scenarios: rcalib unchanged, rcalib increased, rcalib decreased, with expected KER outcomes.
This will guide the development of unit tests for `kerdeployable`.

### 50. Objection-Identifier: The CI system heavily relies on SQLite files that are mutable. How do we prevent a situation where a CI job reads an intermediate, partially written state from a concurrent build?
Need a strategy for exclusive locks or using temporary copies; otherwise, CI results may be non‑deterministic.
