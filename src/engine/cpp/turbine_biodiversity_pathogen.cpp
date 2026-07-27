// filename: src/engine/cpp/turbine_biodiversity_pathogen.cpp
// target-repo: https://github.com/mk-bluebird/Prometheus-Praxis
// language: C++ (C ABI, ARM-friendly, non-actuating numerical kernel)
// license: MIT OR Apache-2.0

#include <cstddef>
#include <cstdint>

extern "C" {

// Input POD: biodiversity and pathogen-related diagnostics for a turbine node.[file:3]
struct TurbineBiodiversityPathogenInput {
    // Biodiversity/connectivity metrics.
    double connectivity_index;       // Habitat connectivity [0..1], higher is better.
    double complexity_index;         // Habitat structural complexity [0..1], higher is better.
    double colonisation_potential;   // Colonisation potential [0..1], higher is better.

    // Pathogen/warm-zone/stagnation indicators.
    double water_temperature_C;      // Water temperature [°C].
    double stagnation_index;         // Stagnation indicator [0..1], higher = more stagnation.
    double nutrient_index;           // Nutrient load [0..1], higher = more eutrophic risk.

    double vt_before;                // Lyapunov residual slice before this evaluation.
    std::uint32_t node_hex_id;       // Hex/evidence id for this turbine/canal node.
};

// Output POD: rbiodiversity, rpathogen, updated residual and KER.[file:3]
struct TurbineBiodiversityPathogenOutput {
    double r_biodiversity;           // Biodiversity risk coordinate [0..1] (lower is better).
    double r_pathogen;               // Pathogen risk coordinate [0..1] (higher is worse).
    double vt_after;                 // Updated Lyapunov residual slice [0..1].
    double k_factor;                 // Knowledge K [0..1].
    double e_factor;                 // Eco-impact E [0..1].
    double r_factor;                 // Risk-of-harm R [0..1].
    std::uint32_t evidence_hex;      // Evidence hex propagated from node_hex_id.
};

static inline double clamp01(double x) {
    if (x < 0.0) return 0.0;
    if (x > 1.0) return 1.0;
    return x;
}

// Kernel semantics:
// - Evaluates rbiodiversity from connectivity/complexity/colonisation metrics.
// - Updates rpathogen from warm-zone and stagnation indicators.
// - Ensures vt_after is never reduced by increasing pathogen risk: pathogen
//   contribution to Vt is always nonnegative and treated as a non-offsettable plane.[file:3]
int compute_turbine_biodiversity_pathogen(const void* in_buffer,
                                          void* out_buffer,
                                          std::size_t insize,
                                          std::size_t outsize)
{
    if (in_buffer == nullptr || out_buffer == nullptr) {
        return 1; // null pointer
    }

    if (insize != sizeof(TurbineBiodiversityPathogenInput) ||
        outsize != sizeof(TurbineBiodiversityPathogenOutput)) {
        return 2; // size mismatch
    }

    const auto* in = static_cast<const TurbineBiodiversityPathogenInput*>(in_buffer);
    auto* out = static_cast<TurbineBiodiversityPathogenOutput*>(out_buffer);

    // 1. Biodiversity risk coordinate rbiodiversity.[file:3]
    // Connectivity / complexity / colonisation are “good” metrics. Higher values
    // lower biodiversity risk. Use a simple complement-based mapping.[file:3]
    const double conn = clamp01(in->connectivity_index);
    const double comp = clamp01(in->complexity_index);
    const double colo = clamp01(in->colonisation_potential);

    // Aggregate a “health” index and convert to risk.[file:3]
    const double health_index = 0.4 * conn + 0.3 * comp + 0.3 * colo;
    double r_biodiversity = 1.0 - health_index;
    r_biodiversity = clamp01(r_biodiversity);

    // 2. Pathogen risk coordinate rpathogen from warm-zone/stagnation/nutrients.[file:3]
    // Warm zone: e.g. 20–30 °C corridor, >30 °C higher pathogen risk.[file:3]
    const double T = in->water_temperature_C;
    double r_warm = 0.0;
    if (T <= 15.0) {
        r_warm = 0.2; // cold but potentially low pathogen activity.[file:3]
    } else if (T >= 30.0) {
        r_warm = 1.0; // warm zone with high pathogen potential.[file:3]
    } else {
        // Linear mapping between 15 and 30 °C.[file:3]
        r_warm = (T - 15.0) / (30.0 - 15.0);
    }
    r_warm = clamp01(r_warm);

    const double r_stag = clamp01(in->stagnation_index);
    const double r_nutr = clamp01(in->nutrient_index);

    // Combine into pathogen risk coordinate.[file:3]
    double r_pathogen = 0.4 * r_warm + 0.3 * r_stag + 0.3 * r_nutr;
    r_pathogen = clamp01(r_pathogen);

    // 3. Lyapunov residual update with biodiversity and pathogen planes.[file:3]
    const double vt_before = (in->vt_before < 0.0) ? 0.0 : in->vt_before;

    // Weights: pathogen is treated as high-hazard, non-offsettable.[file:3]
    const double w_bio      = 0.8;  // biodiversity weight.[file:3]
    const double w_pathogen = 1.2;  // pathogen weight (non-offsettable). [file:3]

    // Residual contribution from biodiversity: lower rbiodiversity reduces Vt.[file:3]
    const double contrib_bio = w_bio * r_biodiversity * r_biodiversity;

    // Residual contribution from pathogen: strictly nonnegative, higher r_pathogen
    // always increases or maintains Vt; it can never reduce Vt.[file:3]
    const double contrib_pathogen = w_pathogen * r_pathogen * r_pathogen;

    double vt_raw = vt_before + contrib_bio + contrib_pathogen;
    double vt_after = clamp01(vt_raw);
    const double delta_vt = vt_after - vt_before;

    // 4. KER triad, respecting non-offsettable pathogen plane.[file:3]
    // Knowledge K: high when biodiversity risk is low and pathogen risk is low,
    // and residual does not increase.[file:3]
    double k = 0.9;
    k -= 0.3 * r_biodiversity;
    k -= 0.4 * r_pathogen;
    if (delta_vt > 0.0) {
        k -= 0.2;
    }
    if (k < 0.0) k = 0.0;
    if (k > 1.0) k = 1.0;

    // Eco-impact E: ecoimpact is high when both biodiversity is healthy (low
    // rbiodiversity) and pathogen risk is low (low r_pathogen), and residual is
    // not worsening.[file:3]
    double e = 0.9 - vt_after;
    e -= 0.2 * r_biodiversity;
    e -= 0.3 * r_pathogen;
    if (delta_vt > 0.0) {
        e -= 0.1;
    }
    if (e < 0.0) e = 0.0;
    if (e > 1.0) e = 1.0;

    // Risk-of-harm R: pathogen plane dominates, with biodiversity residual.[file:3]
    double r = vt_after;
    r += 0.3 * r_pathogen;
    r = clamp01(r);

    // 5. Populate output POD.[file:3]
    out->r_biodiversity = r_biodiversity;
    out->r_pathogen     = r_pathogen;
    out->vt_after       = vt_after;
    out->k_factor       = k;
    out->e_factor       = e;
    out->r_factor       = r;
    out->evidence_hex   = in->node_hex_id;

    return 0;
}

} // extern "C"
