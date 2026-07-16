# ker-composition

`ker-composition` is a Rust crate that implements the KER composition algebra for the Prometheus-Praxis ecosafety spine. It provides a high-throughput, native implementation of the `ker_oplus_geom_min_max` operator with a stable C ABI, Rust API, and Lua-based integrity checks that AI agents and CI pipelines can run offline.

The crate is designed to be non-actuating: it only computes and validates KER composition values (K, E, R) and never interacts with hardware or external control systems. This aligns with the governance requirement that composition logic remains purely analytical and auditable.

---

## Features

- Geometric-min-max KER algebra:
  - \(K_p = \sqrt{K_1 \cdot K_2}\) (geometric mean of knowledge).
  - \(E_p = \min(E_1, E_2)\) (eco-impact capped by the weakest contributor).
  - \(R_p = \max(R_1, R_2)\) (risk monotone and non-decreasing). [file:1]
- Canonical member ordering:
  - Deterministic `combined_id` and `members` fields independent of operand order. [file:1]
- C ABI for high-throughput engines:
  - C-compatible structs for `KERParticle2026v1` and `KERComposition2026v1`.
  - `ker_oplus_geom_min_max` exported as a pure C function for native callers.
- Safe Rust wrapper:
  - `RustKerParticle` and `RustKerComposition` types.
  - `ker_oplus_geom_min_max_rust` as the primary Rust API.
- Lua integrity checks:
  - Lua module `ker_composition_validator.lua` loaded via `rlua`.
  - Validation of all five composition invariants for offline AI/CI checks. [file:1]
- Kani proof harnesses:
  - Commutativity proof for K, E, R and canonical IDs.
  - Local associativity proof over three particles, under KER corridor assumptions. [web:46][web:47]

---

## Layout

This crate lives under the monorepo `crates` directory:

- `crates/ker-composition/Cargo.toml`  
- `crates/ker-composition/src/lib.rs`  
- `crates/ker-composition/src/kani_harness.rs` (Kani proof harnesses, behind `cfg(kani)`)  
- `crates/ker-composition/lua/ker_composition_validator.lua` (Lua invariants)  
- `ker/ker_oplus_geom_min_max.hpp` (C++ header in the repo root hierarchy)  
- `ker/ker_oplus_geom_min_max.cpp` (C++ implementation compiled via `cc` crate) [file:1]

The crate assumes a mono-repo layout where native C++ sources are shared by multiple consumers but built into this crate via `build.rs`.

---

## KER types and semantics

### Base particle: `KERParticle2026v1`

The crate mirrors the ALN definition of base KER particles: [file:1]

- `particle_id: String` — globally unique identifier for the evidence shard.  
- `topic_id: String` — thematic grouping (e.g., sensor, study, corridor).  
- `lane: String` — governance lane (`RESEARCH`, `PILOT`, `PROD`).  
- `k: f32` — knowledge score normalized to \([0,1]\).  
- `e: f32` — eco-impact score normalized to \([0,1]\).  
- `r: f32` — risk score normalized to \([0,1]\).  
- `evidencehex: String` — hex-encoded provenance commit.  
- `signinghex: String` — DID-bound signature recorded by KER. [file:1]

### Composition particle: `KERComposition2026v1`

The composite type records a single KER composition under the geometric-min-max rule: [file:1]

- `left_particle_id: String`  
- `right_particle_id: String`  
- `combined_id: String` — canonical combination (e.g., `idmin+idmax`).  
- `k_combined: f32` — geometric mean of `k`.  
- `e_combined: f32` — minimum of `e`.  
- `r_combined: f32` — maximum of `r`.  
- `members: String` — canonical member list, `idmin,idmax`.  
- `rule_id: String` — fixed to `keroplusgeomminmaxv1`.  
- `evidencehex: Option<String>` — combined evidence hash, computed outside this crate.  
- `signinghex: Option<String>` — composition signature, also external. [file:1]

The crate intentionally does not compute hashes or signatures; those are handled by KER signing tooling to keep cryptographic primitives anchored and auditable.

---

## C ABI

The core C ABI is defined in `ker/ker_oplus_geom_min_max.hpp` and implemented in `ker/ker_oplus_geom_min_max.cpp`. [file:1]

### C structs

```c
struct ker_particle2026v1 {
    const char* particle_id;
    const char* topic_id;
    const char* lane;
    float       K;
    float       E;
    float       R;
    const char* evidencehex;
    const char* signinghex;
};

struct ker_composition2026v1 {
    const char* left_particle_id;
    const char* right_particle_id;
    const char* combined_id;
    float       K_combined;
    float       E_combined;
    float       R_combined;
    const char* members;
    const char* rule_id;
    const char* evidencehex;
    const char* signinghex;
};
```

### C function

```c
int32_t ker_oplus_geom_min_max(
    const ker_particle2026v1* left,
    const ker_particle2026v1* right,
    ker_composition2026v1* out_comp
);
```

- Returns `0` on success.  
- Validates that all `K`, `E`, and `R` values are in \([0,1]\).  
- Computes `K_combined`, `E_combined`, `R_combined`.  
- Sets `combined_id`, `members`, and `rule_id`.  
- Leaves `evidencehex` and `signinghex` as null; those must be filled by callers. [file:1]

---

## Rust API

The Rust side exposes safe, high-level types in `src/lib.rs`:

```rust
pub struct RustKerParticle {
    pub particle_id: String,
    pub topic_id: String,
    pub lane: String,
    pub k: f32,
    pub e: f32,
    pub r: f32,
    pub evidencehex: String,
    pub signinghex: String,
}

pub struct RustKerComposition {
    pub left_particle_id: String,
    pub right_particle_id: String,
    pub combined_id: String,
    pub k_combined: f32,
    pub e_combined: f32,
    pub r_combined: f32,
    pub members: String,
    pub rule_id: String,
    pub evidencehex: Option<String>,
    pub signinghex: Option<String>,
}

pub fn ker_oplus_geom_min_max_rust(
    left: &RustKerParticle,
    right: &RustKerParticle,
) -> Result<RustKerComposition, i32>;
```

- The function performs all K/E/R calculations via the C++ core and returns a Rust-idiomatic result.  
- Error codes are propagated as `Err(i32)` when the C ABI returns non-zero. [file:1]

---

## Lua invariants via `rlua`

To support lightweight offline integrity checks (e.g., by AI chat agents or CI jobs), the crate ships a Lua module `lua/ker_composition_validator.lua` that is loaded using `rlua`. [file:1]

The Lua validator:

- Expects a table describing the left and right base particles and the composite row.  
- Applies the five ALN invariants:

  - `kercombineriskcap` (risk cap with corridor threshold `theta`).  
  - `kercombineKEbounds` (K bounds and E non-compensation).  
  - `kercombineRmonotone` (risk monotonicity).  
  - `kercombineprovenance` (rule ID and structural provenance).  
  - `kercombinelanesafety` (lane safety between RESEARCH / PILOT / PROD). [file:1]

- Returns `true` if all invariants hold, `false` otherwise.

The Rust helper `validate_composition_with_lua` constructs the Lua tables and calls `validate`:

```rust
pub fn validate_composition_with_lua(
    row: &RustKerCompositionRow,
) -> Result<bool, rlua::Error>;
```

This enables cross-checking governance shards without any network or heavy dependencies.

---

## Kani verification

The crate includes Kani proof harnesses (under `src/kani_harness.rs`) that can be activated by running Kani on this crate. [web:46][web:47]

### Proofs

- `proof_commutativity`:

  - Generates arbitrary K, E, R in \([0,1]\) for two particles.  
  - Asserts that `ker_oplus_geom_min_max_rust(p1, p2)` and `ker_oplus_geom_min_max_rust(p2, p1)` produce identical `K`, `E`, `R`, `combined_id`, and `members` (bit-level equality via `to_bits`). [file:1]

- `proof_associativity_three_particles`:

  - Generates K, E, R in \([0,1]\) for three particles.  
  - Asserts that \((p_1 \oplus p_2) \oplus p_3` and `p_1 \oplus (p_2 \oplus p_3)\) yield identical K, E, R values. [file:1]

### Running Kani

From `crates/ker-composition`:

```bash
cargo kani --tests
```

This will run all `#[kani::proof]` functions using the installed `kani-verifier` toolchain. [web:43][web:46]

---

## Building and linking

### Dependencies

Declared in `Cargo.toml`:

- `rlua = "0.19"` — embedded Lua.  
- `serde = "1.0.203"` — serialization (if needed for future extensions).  
- `serde_json = "1.0.120"` — JSON handling.  
- `cc = "1.0"` (build-dependency) — compiles the C++ implementation. [web:29][file:1]

### Build script

`build.rs` compiles `ker_oplus_geom_min_max.cpp`:

```rust
fn main() {
    println!("cargo:rerun-if-changed=../../ker/ker_oplus_geom_min_max.cpp");
    println!("cargo:rerun-if-changed=../../ker/ker_oplus_geom_min_max.hpp");

    cc::Build::new()
        .cpp(true)
        .file("../../ker/ker_oplus_geom_min_max.cpp")
        .flag_if_supported("-std=c++20")
        .compile("ker_oplus_geom_min_max");
}
```

This ensures the crate rebuilds when the C++ source changes and provides a static library `libker_oplus_geom_min_max.a` that Rust links against. [web:22][file:1]

---

## Governance and safety posture

- **Risk monotonicity:**  
  - `R_combined` is always at least the greater of `R1` and `R2`; compositions cannot hide risk. [file:1]
- **Non-compensation:**  
  - `E_combined` is bounded by the worst eco-impact; high K cannot inflate E. [file:1]
- **Knowledge conservatism:**  
  - Geometric mean ensures low K in either parent drags down `K_combined`, matching weakest-link behavior. [file:1]
- **Provenance:**  
  - `evidencehex` and `signinghex` are passed through but not generated; they are expected to be produced by KER’s DID-bound signing tooling and validated by ALN invariants in `KERComposition2026v1.aln`. [file:1]

The crate is safe to embed in CI, AI agents, and governance dashboards to ensure all KER compositions respect the ecosafety corridor and compositional invariants before any deployment or policy decision.
