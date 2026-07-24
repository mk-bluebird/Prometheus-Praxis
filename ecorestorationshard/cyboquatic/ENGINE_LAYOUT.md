# Cyboquatic Engine Layout (CPP/ALN/SQL)

This document describes the layout of core C++ engine files, ALN v2 particles, and SQL schemas for the Cyboquatic eco-restoration engine in the main (non–time-stamped) directories.

## Directory Structure

```
/workspace
├── crates/prometheus_praxis_ai/
│   ├── aln/                          # ALN v2 particle specifications
│   │   ├── WorkloadKernel2026v1.aln2
│   │   ├── DrainageDecayKernel2026v1.aln2
│   │   └── AiDatacenterNode2026v1.aln2
│   └── src/engine/cpp/               # Core C++ engine kernels
│       ├── eco_engine_workload.hpp
│       ├── eco_engine_workload.cpp
│       ├── eco_engine_drainage.hpp
│       ├── eco_engine_drainage.cpp
│       ├── eco_engine_ai_node.hpp
│       └── eco_engine_ai_node.cpp
├── ecorestorationshard/
│   ├── cyboquatic/                   # Main cyboquatic directory (created)
│   ├── cyboquaticprogress/           # Time-stamped daily progress shards
│   │   └── YYYYMMDD-*/               # Daily shard directories
│   │       ├── cpp/                  # Daily CPP snapshots
│   │       └── aln/                  # Daily ALN particles
│   └── sql/                          # Cyboquatic SQL schemas
├── sql/                              # Global SQL schemas
│   ├── dbcyboquatic_blastradius_spine.sql
│   ├── db_cyboquatic_machinery_index.sql
│   └── dbcyboquatic_ker_window_planes_2026v1.sql
└── db/                               # Additional DB schemas
    ├── db_cyboquatic_machinery_spine.sql
    └── dbcyboquatic_ker_window_planes_2026v1.sql
```

## C++ Engine Modules

### 1. Workload Kernel (`eco_engine_workload.cpp/.hpp`)

**Location:** `crates/prometheus_praxis_ai/src/engine/cpp/`

**Purpose:** Computes workload energetics residuals for eco-restoration tasks (sensor deployment, debris clearing, biocompound application).

**Key Functions:**
- `compute_workload_residual()` - C-compatible FFI function

**Input Metrics:**
- `energyreqJ` - Required energy for workload [J]
- `energysurplusJ` - Surplus energy available [J]
- `hydraulicrisk` - Normalized hydraulics risk proxy [0,1]
- `uncertaintyrisk` - Telemetry/model trust risk [0,1]

**Output Risk Coordinates:**
- `r_energy` - Energy shortfall vs tailwind risk
- `r_hydraulics` - Surcharge/instability risk
- `r_uncertainty` - Data trust/sensor health risk

**KER Planes:**
- Knowledge (K): Derived from uncertainty risk and telemetry quality
- Eco-Impact (E): Derived from energy surplus ratio
- Risk (R): Weighted quadratic form over risk coordinates

**Residual Formula (V_t):**
```
V_t = 0.8 * r_energy² + 1.0 * r_hydraulics² + 0.6 * r_uncertainty²
```

**ALN Particle Binding:** `WorkloadKernel2026v1.aln2`

**SQL Tables Fed:**
- `cybo_workload_ledger` (dbcyboquatic_blastradius_spine.sql)
- `cybo_workload_window` (db_cyboquatic_machinery_index.sql)

---

### 2. Drainage Kernel (`eco_engine_drainage.cpp/.hpp`)

**Location:** `crates/prometheus_praxis_ai/src/engine/cpp/`

**Purpose:** High-performance hydraulics/drainage decay kernel for water quality monitoring in canal systems.

**Key Functions:**
- `compute_drainage_decay()` - C-compatible FFI function

**Input Metrics:**
- `bod_mg_l` - Biochemical Oxygen Demand [mg/L], range [0, 80]
- `tss_mg_l` - Total Suspended Solids [mg/L], range [0, 500]
- `cec_cmol_per_kg` - Cation Exchange Capacity [cmol(+)/kg], range [0, 50]
- `flow_rate_m3s` - Canal flow rate [m³/s]
- `water_temp_c` - Water temperature [°C], range [0, 45]
- `elevation_m` - Elevation [m], range [-100, 2000]

**Output Risk Coordinates:**
- `r_bod` - BOD risk normalized [0,1]
- `r_tss` - TSS risk normalized [0,1]
- `r_cec` - CEC risk normalized [0,1]
- `r_hydraulics` - Flow surcharge/instability proxy [0,1]
- `r_uncertainty` - Data trust/sensor health [0,1]

**KER Planes:**
- Knowledge (K): Derived from sensor calibration status and data quality
- Eco-Impact (E): Derived from water quality improvement metrics
- Risk (R): Weighted quadratic form over drainage risk coordinates

**Residual Formula (V_t):**
```
V_t = 0.9 * r_bod² + 0.7 * r_tss² + 0.6 * r_cec² + 1.0 * r_hydraulics² + 0.8 * r_uncertainty²
```

**ALN Particle Binding:** `DrainageDecayKernel2026v1.aln2`

**SQL Tables Fed:**
- `cybo_ecosafety_binding` (db_cyboquatic_machinery_index.sql)
- `vcyboquaticwindowwithplanes` (dbcyboquatic_ker_window_planes_2026v1.sql)

---

### 3. AI Node Kernel (`eco_engine_ai_node.cpp/.hpp`)

**Location:** `crates/prometheus_praxis_ai/src/engine/cpp/`

**Purpose:** AI datacenter node energetics kernel coupling compute workloads to thermal/ecological impact.

**Key Functions:**
- `compute_ai_node_residual()` - C-compatible FFI function

**Input Metrics:**
- `pue` - Power Usage Effectiveness, range [1.0, 3.5]
- `cue` - Cooling Usage Effectiveness, range [0.5, 5.0]
- `power_kw` - IT power draw [kW], range [0, 100000]
- `cooling_kw` - Cooling power [kW], range [0, 100000]
- `thermal_output_kw` - Waste heat discharge [kW]
- `carbon_intensity` - Carbon intensity of energy mix [0,1]
- `biodiversity_risk` - Local ecological/siting impact [0,1]
- `uncertainty_risk` - Telemetry/model trust [0,1]

**Output Risk Coordinates:**
- `r_energy_compute` - Energy intensity vs sustainable envelope
- `r_cooling_water` - Cooling water/hydraulics impact
- `r_carbon` - Carbon intensity risk
- `r_biodiversity` - Biodiversity/siting risk
- `r_uncertainty` - Data trust/telemetry health

**KER Planes:**
- Knowledge (K): Derived from telemetry quality and model confidence
- Eco-Impact (E): Derived from carbon/biodiversity performance
- Risk (R): Weighted quadratic form over AI node risk coordinates

**Residual Formula (V_t_ai):**
```
V_t_ai = 0.7 * r_energy_compute² + 0.6 * r_cooling_water² + 1.0 * r_carbon² + 1.0 * r_biodiversity² + 0.8 * r_uncertainty²
```

**ALN Particle Binding:** `AiDatacenterNode2026v1.aln2`

**SQL Tables Fed:**
- `cybo_blastradius_link` (dbcyboquatic_blastradius_spine.sql)
- Heat governance coupling tables (via tileId references)

---

## ALN v2 Particles

All ALN v2 particles are bound to DID: `bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7`

| Particle File | Domain | KER Corridor Invariants |
|---------------|--------|-------------------------|
| `WorkloadKernel2026v1.aln2` | workload | K,E,R ∈ [0,1], kerScore > 0, V_t monotonic in PRODUCTION |
| `DrainageDecayKernel2026v1.aln2` | hydraulics | K,E,R ∈ [0,1], kerScore > 0, V_t bounded growth |
| `AiDatacenterNode2026v1.aln2` | ai_datacenter | K,E,R ∈ [0,1], kerScore > 0, r_carbon ≤ 0.3 in PRODUCTION |

**Location:** `crates/prometheus_praxis_ai/aln/`

---

## SQL Schemas

### Main Cyboquatic Tables

| Schema File | Primary Tables | Purpose |
|-------------|---------------|---------|
| `dbcyboquatic_blastradius_spine.sql` | `cybo_blastradius_link`, `cybo_workload_ledger` | Blast-radius linking and workload ledger |
| `db_cyboquatic_machinery_index.sql` | `cybo_site`, `cybo_asset`, `cybo_instrument_profile`, `cybo_ecosafety_binding` | Site/asset taxonomy and instrumentation |
| `dbcyboquatic_ker_window_planes_2026v1.sql` | `vcyboquaticwindowwithplanes` (view) | Per-plane KER window aggregation |

---

## Relationship: Daily Progress Shards → Main Engine

Daily `cyboquaticprogress` shards (e.g., `ecorestorationshard/cyboquaticprogress/20260723-d-cyboquaticworkload/`) contain time-stamped snapshots of:

1. **CPP Snapshots:** Daily copies or variants of engine kernels for reproducibility
2. **ALN Particles:** Daily-specific particle instantiations with actual frame data
3. **SQL Exports:** Shard-specific data exports for audit/replay

**Non-Duplication Principle:**
- Daily shards **reference** main engine modules via `prior_frame_id` and `phoenix_hex_anchor`
- Daily shards do **not** duplicate kernel logic; they store frame instances
- Main engine directories contain canonical kernel implementations
- Daily shards feed into main tables via INSERT/UPDATE operations, not code duplication

**Data Flow:**
```
Daily Shard Frame → Main Engine Kernel (compute) → ALN Validation → SQL Insert
       ↓                                              ↓
prior_frame_id                                  cybo_workload_ledger
phoenix_hex_anchor                              vcyboquaticwindowwithplanes
```

---

## Traceability Matrix

| C++ Module | ALN Particle | SQL Table(s) | KER Planes |
|------------|--------------|--------------|------------|
| `eco_engine_workload.cpp` | `WorkloadKernel2026v1.aln2` | `cybo_workload_ledger`, `cybo_workload_window` | energy, hydraulics, uncertainty |
| `eco_engine_drainage.cpp` | `DrainageDecayKernel2026v1.aln2` | `cybo_ecosafety_binding`, `vcyboquaticwindowwithplanes` | bod, tss, cec, hydraulics, uncertainty |
| `eco_engine_ai_node.cpp` | `AiDatacenterNode2026v1.aln2` | `cybo_blastradius_link` | energy_compute, cooling_water, carbon, biodiversity, uncertainty |

---

*Document generated for Cyboquatic Engine & Tooling task list. No cargo or Rust tooling required for maintenance.*
