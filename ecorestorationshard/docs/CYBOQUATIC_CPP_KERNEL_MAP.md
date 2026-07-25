# Cyboquatic C++ Kernel Map

This document maps daily C++ kernels from `cyboquaticprogress/*` folders to their intended integration points in the main cyboquatic engine directories. This enables future work to reuse kernels without staying in dated folders.

## Daily Kernels

### 1. `20260718/cpp/workload_energyreq_deltaVt.cpp`

**Purpose:** Non-actuating C++ model computing `energyreqJ` and `deltaVt` per cyboquatic workload window using superquadratic Lyapunov penalties.

**Key Functions:**
- `gj_superquadratic(double r, double alpha2, double alpha_p, int p)` – convex penalty function
- `compute_residual(...)` – computes Lyapunov residual Vt from risk planes
- `evaluate_workload(...)` – main entry point for workload evaluation

**Integration Point:** Candidate numeric kernel for `cyboquatic_index` workload window calculations. Can be wired into frame-stream processing for energy/carbon risk scoring.

---

### 2. `20260719/cpp/cyboquatic_workload_20260719.cpp`

**Purpose:** Extended workload kernel with KER triad derivation and multi-plane risk coordinates (energy, carbon, hydraulics, materials, data quality).

**Key Functions:**
- `lyapunov_g(double r, double alpha, double beta, double p)` – strictly convex penalty
- `compute_vt(const WorkloadPlanes&)` – aggregate Lyapunov residual
- `derive_ker(const WorkloadPlanes&, double vt_curr)` – KER triad computation
- `compute_workload_window(...)` – main workload window evaluator

**Integration Point:** Workload frame processor for `cyboquatic_index/src`. Suitable for ingestion into `frame_stream.rs` as an FFI boundary or standalone telemetry preprocessor.

---

### 3. `20260723-d-cyboquaticworkload/cpp/CyboWorkloadKernel.cpp`

**Purpose:** Normalized risk coordinates and Lyapunov residual slice for `energyreqJ` and `ΔVt`, with explicit corridor-style normalization.

**Key Functions:**
- `normalize_energy(...)`, `normalize_hydraulics(...)`, `normalize_carbon(...)`, `normalize_uncertainty(...)` – risk plane normalizers
- `compute_vt(...)` – quadratic Lyapunov residual
- `cybo_compute_workload_residual(...)` – extern "C" entry point for FFI

**Integration Point:** Primary candidate for `cyboquaticindex` workload window calculations. The `extern "C"` interface makes it suitable for direct binding from Java/Kotlin/Python telemetry layers. Outputs feed into `cybo_workload_frame` table.

---

### 4. `20260724-g-blastradius/cpp/canal_blastradius_engine.cpp`

**Purpose:** Non-actuating C++ numeric kernel computing canal surcharge blast radii and KER-oriented risk coordinates for diagnostic ingestion into SQLite.

**Key Structs:**
- `CanalNodeEnvelope` – node parameters (max_diag_energy_j, topo_sensitivity)
- `SurchargeEventInput` – event telemetry (surcharge_m, hydraulic_head_m, diag_energy_j)
- `BlastRadiusOutput` – computed diagnostics (radius_m, risk planes, KER triad, vt_residual)

**Key Functions:**
- `compute_radius_m(...)` – hydraulic blast radius estimator
- `compute_blast_radius_diag(...)` – full diagnostic computation with risk planes and KER

**Integration Point:** Blast-radius risk-plane helper for ecosafety frames. Intended for use by EcoNet/Eco-Fort shard catalog queries reading from `blast_radius_diag` table. Pure function can be called from Python/Java after reading `surcharge_event` rows.

---

## Main Engine Directories

| Directory | Purpose | Relevant Kernels |
|-----------|---------|------------------|
| `ecorestorationshard/cyboquatic_index/src` | Frame stream processing, workload indexing | `CyboWorkloadKernel.cpp`, `cyboquatic_workload_20260719.cpp` |
| `ecorestorationshard/cyboquatic/` | Core cyboquatic types and utilities | All kernels (shared types) |
| `ecorestorationshard/sql/` | SQLite spine schemas | Schema companions to kernels |
| `Eco-Fort/db/` | Eco-Fort constellation DB | `canal_blastradius_schema.sql` wiring |

---

## Notes

- All kernels are **non-actuating**: they compute diagnostics only, no physical control.
- Use native C++ compiler (`g++ -std=c++17 -c`) for compilation checks; no `cargo` required.
- KER invariants enforced via SQLite triggers in companion schemas.
- Risk planes follow Prometheus-Praxis guidance: superquadratic penalties, band-agnostic weights.
