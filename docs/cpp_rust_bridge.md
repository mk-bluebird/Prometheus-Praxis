# C++–Rust Eco Bridge Guide

This document explains how existing Rust eco crates expose C interfaces, and how C++ modules should call them safely and eco‑aware within Prometheus‑Praxis.

## 1. Rust Export Pattern

Rust eco crates (e.g., `eco_materials`, `eco_pfas_corridor`, `eco_ker_core`) expose C‑compatible interfaces using:

```rust
#[repr(C)]
pub struct MaterialTestParamsC {
    pub oxygen_depletion_percent: f64,
    pub co2_evolution_percent: f64,
    pub bod_removal_percent: f64,
    pub doc_removal_percent: f64,
    pub days_to_pass_window: f64,
    pub toxicity_score: f64,
    pub pfas_presence: f64,
}

#[repr(C)]
pub struct MaterialEcoImpactC {
    pub k_safe_fraction: f64,
    pub e_eco_benefit_band: f64,
    pub r_risk_max: f64,
    pub ker_score: f64,
    pub biodegradability_score: f64,
}

#[no_mangle]
pub extern "C" fn compute_material_eco_impact(params: MaterialTestParamsC)
    -> MaterialEcoImpactC { /* ... */ }
```

Key points:

- `#[repr(C)]` ensures layout compatibility with C++ `struct` definitions.
- `#[no_mangle] extern "C"` exposes stable symbol names for linking.

Similar patterns are used for PFAS corridors and KER bridges:

```rust
#[repr(C)]
pub struct PFASStateC { pub mass_kg: f64, pub sorbed_fraction: f64, pub cold_survival_factor: f64; }

#[no_mangle]
pub extern "C" fn pfas_corridor_step(
    state: PFASStateC,
    base_degradation_rate: f64,
    current_temp_C: f64,
    cold_temp_C: f64,
    sorption_increment: f64
) -> PFASStateC { /* ... */ }

#[no_mangle]
pub extern "C" fn ker_score_bridge(k: f64, e: f64, r_max: f64) -> f64 { /* ... */ }
```

## 2. C++ FFI Declarations

C++ modules declare matching `extern "C"` types and functions:

```cpp
extern "C" {
    struct MaterialTestParamsC {
        double oxygen_depletion_percent;
        double co2_evolution_percent;
        double bod_removal_percent;
        double doc_removal_percent;
        double days_to_pass_window;
        double toxicity_score;
        double pfas_presence;
    };

    struct MaterialEcoImpactC {
        double k_safe_fraction;
        double e_eco_benefit_band;
        double r_risk_max;
        double ker_score;
        double biodegradability_score;
    };

    MaterialEcoImpactC compute_material_eco_impact(MaterialTestParamsC params);

    struct PFASStateC {
        double mass_kg;
        double sorbed_fraction;
        double cold_survival_factor;
    };

    PFASStateC pfas_corridor_step(PFASStateC state,
                                  double base_degradation_rate,
                                  double current_temp_C,
                                  double cold_temp_C,
                                  double sorption_increment);

    double ker_score_bridge(double k, double e, double r_max);
}
```

Linking:

- C++ code links against Rust‑produced static or shared libraries (`libeco_materials.a` / `.so`), using the stable symbol names.
- No new crates are introduced; existing crates provide the FFI entry points.

## 3. Eco‑Aware Usage Patterns

When calling Rust eco engines from C++:

1. **Preserve KER Semantics:**

   - Ensure `k`, `e`, and `r_max` lie in `[0,1]` before calling `ker_score_bridge`.
   - Treat positive KER (`s > 0`) as requiring non‑increasing Lyapunov residual `V_t`, enforced by C++ invariants and SQL checks.

2. **Respect PFAS Corridor Invariants:**

   - Always clamp PFAS state inputs to ALN/SQL corridor ranges:
     - `mass_kg` within `[0, max_mass_kg]`.
     - `sorbed_fraction` in `[0,1]`.
     - `cold_survival_factor` within ALN‑declared bounds.
   - After calling `pfas_corridor_step`, verify mass non‑increase when `cold_survival_factor >= 1.0`.

3. **Non‑Actuating Design:**

   - C++–Rust bridge functions must not actuate machinery directly.
   - Outputs (scores, residuals, routes) feed into higher‑level schedulers and governance layers only.

## 4. Example Scenario: Material Eco‑Impact

C++ harness:

```cpp
MaterialTestParamsC p{};
p.oxygen_depletion_percent = 65.0;
p.co2_evolution_percent    = 62.0;
p.bod_removal_percent      = 60.0;
p.doc_removal_percent      = 75.0;
p.days_to_pass_window      = 9.0;
p.toxicity_score           = 0.2;
p.pfas_presence            = 0.1;

MaterialEcoImpactC impact = compute_material_eco_impact(p);
// Use impact.k_safe_fraction, impact.e_eco_benefit_band, impact.ker_score
// in KER/Lyapunov calculations and corridor gating.
```

Eco‑aware steps:

- Inputs reflect ISO/OECD pass thresholds and toxicity/PFAS factors.
- Outputs are normalized scores fed into KER and Lyapunov logic in C++/SQL.

## 5. Safety and DID Binding

All C++–Rust bridges are conceptually covered by ALN governance particles (e.g., `eco_multilang_binding.aln2`), which:

- Bind the bridge to DID `bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7`.
- Declare invariants that bindings are non‑actuating and preserve KER semantics.

C++ modules must:

- Avoid hidden control paths or actuation.
- Keep risk semantics and threshold values in sync with ALN and SQL corridor definitions.
- Surface any mismatch via conformance checkers (e.g., `aln_conformance_checker.cpp`).

Following this guide ensures that C++ and Rust eco engines cooperate safely and consistently within Prometheus‑Praxis, maintaining eco‑restoration integrity across languages.
