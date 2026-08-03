# C++ Eco-Restoration Style and Naming Guide

This document defines preferred naming conventions, planes vocabulary, KER variables, and types for eco-restoration C++ modules in Prometheus-Praxis. It also maps Rust crate names and roles to C++ module names for coherence across the eco_restoration_shard.

## 1. General C++ Style

- Use `snake_case` for free functions (e.g., `ker_score`, `lyapunov_residual`).
- Use `PascalCase` for structs and classes (e.g., `MaterialTestParams`, `PFASState`, `BlastRisk`).
- Namespace layout:
  - `eco_tools` for generic scoring utilities (KER, Lyapunov).
  - `eco_restoration` for material eco-impact and core eco models.
  - `eco_pfas` for PFAS fate and corridor logic.
  - `phoenix_canal` for blast-radius and canal hydraulics.
  - `phoenix_hex` for hex-anchor risk aggregation.
  - `eco_config` for configuration loaders and canal/hex/workload corridor types.

- All modules must be non-actuating: they compute scores, residuals, and routes, but do not directly actuate machinery.

## 2. Planes Vocabulary and KER Variables

### Risk Planes

Standard planes (matching governance docs):

- Hydraulics: `r_hydraulics`
- Energy: `r_energy`
- Topology: `r_topology`
- Biodiversity: `r_biodiversity`

Each plane uses normalized risk coordinates in `[0, 1]` and nonnegative weights:

```cpp
std::vector<double> weights = {w_h, w_e, w_t, w_b};
std::vector<double> risks   = {r_hydraulics, r_energy, r_topology, r_biodiversity};
double Vt = eco_tools::lyapunov_residual(weights, risks);
```

### KER Variables

K/E/R are defined as:

- `k`: Knowledge/safe-step fraction over a window (fraction of Lyapunov-safe steps).
- `e`: Eco-impact margin, often `e = 1.0 - r_max` where `r_max` is the maximum risk coordinate.
- `r`: Maximum risk-of-harm coordinate (`r_max`).

Composite score:

```cpp
double s = eco_tools::ker_score(k, e, r_max);
```

Constraints:

- `k`, `e`, `r` must lie in `[0,1]`.
- Positive `s` implies Lyapunov residual drift is non-positive under corridor design.

## 3. PFAS Corridor Types and Parameters

PFAS corridor state struct:

```cpp
namespace eco_pfas {

struct PFASState {
    double mass_kg;              // Total PFAS mass in kg.
    double sorbed_fraction;      // Sorbed fraction.[0][1]
    double cold_survival_factor; // Cold-survival multiplier >= 0.
};

PFASState step_pfas_corridor(const PFASState& state,
                             double base_degradation_rate,
                             double current_temp_C,
                             double cold_temp_C,
                             double sorption_increment);

} // namespace eco_pfas
```

PFAS corridor rules (aligned with ALN and SQL):

- `mass_kg` must not increase when `cold_survival_factor >= 1.0`.
- `sorbed_fraction` must remain in `[0.0, 1.0]`.
- `cold_survival_factor` must be non-negative and bounded by ALN corridor max.

## 4. Blast-Radius Types

Blast-radius risk struct:

```cpp
namespace phoenix_canal {

struct BlastRisk {
    double r_hydraulics;
    double r_energy;
    double r_topology;
};

BlastRisk run_blast_radius_step();

} // namespace phoenix_canal
```

Risk coordinates are normalized `[0, 1]` values derived from surcharge energy field decay around canal nodes.

## 5. Configuration Types

Configuration types:

```cpp
namespace eco_config {

struct CanalNodeConfig {
    std::string node_code;
    std::string description;
    std::string ker_band;    // RESEARCH / EXPPROD / PROD
    std::string fog_band;    // FOG:...
    std::string canal_plane; // HYDRAULICS / ENERGY / TOPOLOGY / BIODIVERSITY
};

struct HexAnchorConfig {
    std::string hex_id;
    std::string domain;
    std::string subdomain;
    std::string owner_did;
};

struct WorkloadCorridorConfig {
    double max_energy_J;
    double max_deltaVt;
    double w_energy;
    double w_topology;
};

} // namespace eco_config
```

## 6. Rust Crate to C++ Module Mapping

For coherence, use the following naming alignment:

- Rust crate `eco_materials` → C++ `cpp/eco_restoration/material_eco_impact.cpp`
- Rust crate `eco_pfas_corridor` → C++ `cpp/eco_restoration/pfas_fate_corridor.cpp`
- Rust crate `phoenix_canal_blast` → C++ `cpp/simulation/phoenix_canal_blast_radius.cpp`
- Rust crate `eco_ker_core` → C++ `cpp/tools/ker_lyapunov_utils.cpp`
- Rust crate `eco_hex_registry` → C++ `cpp/tools/phoenix_hex_registry_client.cpp`

Bindings:

- Rust uses `#[no_mangle] extern "C"` functions with `*_C` suffix for FFI.
- C++ exposes `extern "C"` wrappers where needed but primary APIs live in typed namespaces.

All bindings must be non-actuating and preserve KER semantics as per ALN governance particles.
