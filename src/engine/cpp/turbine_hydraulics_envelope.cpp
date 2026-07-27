// filename: src/engine/cpp/turbine_hydraulics_envelope.cpp
// target-repo: https://github.com/mk-bluebird/Prometheus-Praxis
// language: C++ (C ABI, ARM-friendly, non-actuating numerical kernel)
// license: MIT OR Apache-2.0

#include <cstddef>
#include <cstdint>

extern "C" {

// Input POD: Phoenix canal node hydraulic snapshot for turbine envelope.[file:3]
struct TurbineHydraulicsInput {
    double canal_width_m;        // Inner canal width [m]
    double canal_depth_m;        // Water depth at node [m]
    double discharge_m3s;        // Canal discharge Q [m^3/s]
    double hlr_m;                // Hydraulic loading rate HLR [m^3/(m^2·s)]
    double head_m;               // Available gross head [m]
    double surcharge_index;      // Normalised rsurcharge coordinate [0..1]
    double vt_before;           // Lyapunov residual slice before envelope eval
    std::uint32_t node_hex_id;   // PHX-CANAL-NODE-WL-01 hex/evidence id
};

// Output POD: envelope metrics + normalised rsurcharge band + KER.[file:3]
struct TurbineHydraulicsEnvelopeOutput {
    double velocity_mps;          // Mean velocity in cross-section [m/s]
    double specific_energy_Jkg;   // Specific hydraulic energy E [J/kg]
    double cavitation_index;      // Cavitation risk index (dimensionless)
    double overpressure_index;    // Overpressure risk index (dimensionless)
    double rsurcharge_safe;       // Safe band [0..1] for rsurcharge
    double rsurcharge_gold;       // Gold band upper bound [0..1]
    double rsurcharge_hard;       // Hard band upper bound [0..1]
    double k_factor;              // Knowledge K [0..1]
    double e_factor;              // Eco-impact E [0..1]
    double r_factor;              // Risk-of-harm R [0..1]
    std::uint32_t evidence_hex;   // Evidence hex propagated from node_hex_id
};

// Simple clamp helper.[file:3]
static inline double clamp01(double x) {
    if (x < 0.0) return 0.0;
    if (x > 1.0) return 1.0;
    return x;
}

// Compute turbine hydraulic envelope for PHX-CANAL-NODE-WL-01.
// - Calibrated from Phoenix canal telemetry semantics: Q, HLR, head, surcharge.
// - Non-actuating: does not write to actuators, only diagnostics POD.[file:3]
int compute_turbine_hydraulics_envelope(const void* in_buffer,
                                        void* out_buffer,
                                        std::size_t insize,
                                        std::size_t outsize)
{
    if (in_buffer == nullptr || out_buffer == nullptr) {
        return 1; // null pointer
    }

    if (insize != sizeof(TurbineHydraulicsInput) ||
        outsize != sizeof(TurbineHydraulicsEnvelopeOutput)) {
        return 2; // size mismatch
    }

    const auto* in = static_cast<const TurbineHydraulicsInput*>(in_buffer);
    auto* out = static_cast<TurbineHydraulicsEnvelopeOutput*>(out_buffer);

    // Basic safety: geometry and discharge must be nonnegative.[file:3]
    if (in->canal_width_m <= 0.0 ||
        in->canal_depth_m <= 0.0 ||
        in->discharge_m3s < 0.0) {
        out->velocity_mps         = 0.0;
        out->specific_energy_Jkg  = 0.0;
        out->cavitation_index     = 0.0;
        out->overpressure_index   = 0.0;
        out->rsurcharge_safe      = 0.0;
        out->rsurcharge_gold      = 0.0;
        out->rsurcharge_hard      = 1.0;
        out->k_factor             = 0.0;
        out->e_factor             = 0.0;
        out->r_factor             = 1.0;
        out->evidence_hex         = in->node_hex_id;
        return 3; // numerically unsafe inputs
    }

    // 1. Velocity and specific energy.[file:3]
    const double area_m2 = in->canal_width_m * in->canal_depth_m;
    double velocity_mps = 0.0;
    if (area_m2 > 0.0) {
        velocity_mps = in->discharge_m3s / area_m2;
        if (velocity_mps < 0.0) velocity_mps = 0.0;
    }

    // Specific energy per unit mass: g*h + v^2/2.[file:3]
    const double g = 9.80665;
    const double specific_energy_Jkg = g * in->head_m + 0.5 * velocity_mps * velocity_mps;

    // 2. Cavitation and overpressure indices (normalized diagnostics).[file:3]
    // Cavitation index ~ head / velocity term: higher is safer.
    double cav_raw = 0.0;
    if (velocity_mps > 0.0) {
        cav_raw = in->head_m / (velocity_mps * velocity_mps);
    }
    // Map cav_raw into [0..1] via a corridor-inspired scaling.[file:3]
    // Assume cav_raw >= 0; typical safe cavitation index ~ O(1).
    double cavitation_index = cav_raw / (1.0 + cav_raw);
    cavitation_index = clamp01(cavitation_index);

    // Overpressure index: function of HLR and surcharge.[file:3]
    // HLR scaled such that HLR_ref ~ safe Phoenix canal loading.
    const double hlr_ref = 0.05; // [m^3/(m^2·s)] reference; tune from telemetry.[file:3]
    const double hlr_norm = in->hlr_m / (hlr_ref + in->hlr_m);
    double overpressure_index = hlr_norm + 0.5 * in->surcharge_index;
    overpressure_index = clamp01(overpressure_index);

    // 3. rsurcharge bands: safegoldhard corridor mapping.[file:3]
    // Use input surcharge_index as the current rsurcharge and derive bands.[file:3]
    const double r_surcharge = clamp01(in->surcharge_index);

    // Safe band upper bound: typical daily Phoenix operations, below 0.3.[file:3]
    const double rs_safe = 0.3;
    // Gold band upper bound: elevated but acceptable surcharge, 0.6.[file:3]
    const double rs_gold = 0.6;
    // Hard band upper bound: above this is hard corridor, 1.0.[file:3]
    const double rs_hard = 1.0;

    // 4. Lyapunov residual and KER for envelope gating.[file:3]
    const double vt_before = (in->vt_before < 0.0) ? 0.0 : in->vt_before;
    // Residual update: use surcharge and overpressure as risk coordinates.[file:3]
    double vt_after = vt_before
                      + 0.4 * r_surcharge * r_surcharge
                      + 0.3 * overpressure_index * overpressure_index;
    vt_after = clamp01(vt_after);
    const double delta_vt = vt_after - vt_before;

    // Knowledge factor: high when cavitation index is safe and residual improving.[file:3]
    double k = 0.9;
    k -= 0.2 * (1.0 - cavitation_index); // penalize low cavitation safety.[file:3]
    if (delta_vt > 0.0) {
        k -= 0.2; // penalize residual increase.[file:3]
    }
    if (k < 0.0) k = 0.0;
    if (k > 1.0) k = 1.0;

    // Eco-impact factor: 0.9 minus residual + surcharge penalty.[file:3]
    double e = 0.9 - vt_after;
    e -= 0.1 * r_surcharge;
    if (delta_vt > 0.0) {
        e -= 0.1;
    }
    if (e < 0.0) e = 0.0;
    if (e > 1.0) e = 1.0;

    // Risk-of-harm factor: combine surcharge and overpressure residual.[file:3]
    double r = vt_after;
    r += 0.2 * r_surcharge;
    r += 0.2 * overpressure_index;
    r = clamp01(r);

    // 5. Populate output POD.[file:3]
    out->velocity_mps         = velocity_mps;
    out->specific_energy_Jkg  = specific_energy_Jkg;
    out->cavitation_index     = cavitation_index;
    out->overpressure_index   = overpressure_index;
    out->rsurcharge_safe      = rs_safe;
    out->rsurcharge_gold      = rs_gold;
    out->rsurcharge_hard      = rs_hard;
    out->k_factor             = k;
    out->e_factor             = e;
    out->r_factor             = r;
    out->evidence_hex         = in->node_hex_id;

    return 0;
}

} // extern "C"
