// filename: src/engine/cpp/turbine_habitat_dynamics.cpp
// target-repo: https://github.com/mk-bluebird/Prometheus-Praxis
// language: C++ (C ABI, ARM-friendly, non-actuating numerical kernel)
// license: MIT OR Apache-2.0

#include <cstddef>
#include <cstdint>

extern "C" {

// Input POD: turbine/habitat dynamics snapshot.[file:3]
struct TurbineHabitatInput {
    double discharge_ramp_m3s2;     // dQ/dt ramp rate [m^3/(s^2)]
    double head_ramp_ms2;           // dH/dt ramp rate [m/s^2]
    double turbulence_intensity;    // TI (u'/U) as fraction [0..1]
    double micro_scale_s;           // Micro-scale exposure time [s]
    double vt_before;               // Lyapunov residual slice before event
    std::uint32_t node_hex_id;      // Hex/evidence id for habitat corridor
};

// Output POD: rramp, rturbulence, rhabitat + KER.[file:3]
struct TurbineHabitatOutput {
    double ramp_index;              // Combined ramp index (dimensionless)
    double turbulence_index;        // Combined turbulence index (dimensionless)
    double r_ramp;                  // Normalised rramp [0..1]
    double r_turbulence;            // Normalised rturbulence [0..1]
    double r_habitat;               // Aggregated rhabitat [0..1]
    double k_factor;                // Knowledge K [0..1]
    double e_factor;                // Eco-impact E [0..1]
    double r_factor;                // Risk-of-harm R [0..1]
    std::uint32_t evidence_hex;     // Evidence hex propagated from node_hex_id
};

static inline double clamp01(double x) {
    if (x < 0.0) return 0.0;
    if (x > 1.0) return 1.0;
    return x;
}

// Kernel:
// - Aggregates ramp rate and turbulence intensity into rramp, rturbulence.
// - Constructs rhabitat as a weighted combination.
// - Updates Lyapunov residual and KER, enforcing habitat corridors for fish/microbe passage.[file:3]
int compute_turbine_habitat_dynamics(const void* in_buffer,
                                     void* out_buffer,
                                     std::size_t insize,
                                     std::size_t outsize)
{
    if (in_buffer == nullptr || out_buffer == nullptr) {
        return 1; // null buffer
    }

    if (insize != sizeof(TurbineHabitatInput) ||
        outsize != sizeof(TurbineHabitatOutput)) {
        return 2; // size mismatch
    }

    const auto* in = static_cast<const TurbineHabitatInput*>(in_buffer);
    auto* out = static_cast<TurbineHabitatOutput*>(out_buffer);

    // Basic input sanity.[file:3]
    if (in->micro_scale_s < 0.0 ||
        in->turbulence_intensity < 0.0) {
        out->ramp_index        = 0.0;
        out->turbulence_index  = 0.0;
        out->r_ramp            = 0.0;
        out->r_turbulence      = 0.0;
        out->r_habitat         = 1.0;
        out->k_factor          = 0.0;
        out->e_factor          = 0.0;
        out->r_factor          = 1.0;
        out->evidence_hex      = in->node_hex_id;
        return 3;
    }

    // 1. Ramp index: combine discharge and head ramp.[file:3]
    // Use safe thresholds from corridor design (Phoenix-class microturbine).[file:3]
    const double dQdt_safe = 0.02;   // [m^3/(s^2)] safe ramp baseline.[file:3]
    const double dQdt_hard = 0.20;   // hard corridor.[file:3]
    const double dHdt_safe = 0.005;  // [m/s^2] safe head ramp.[file:3]
    const double dHdt_hard = 0.05;   // hard corridor.[file:3]

    const double dQdt_abs = (in->discharge_ramp_m3s2 >= 0.0)
                            ? in->discharge_ramp_m3s2
                            : -in->discharge_ramp_m3s2;
    const double dHdt_abs = (in->head_ramp_ms2 >= 0.0)
                            ? in->head_ramp_ms2
                            : -in->head_ramp_ms2;

    const double r_dQ =
        clamp01((dQdt_abs - dQdt_safe) / (dQdt_hard - dQdt_safe));
    const double r_dH =
        clamp01((dHdt_abs - dHdt_safe) / (dHdt_hard - dHdt_safe));

    const double ramp_index = 0.6 * r_dQ + 0.4 * r_dH;
    const double r_ramp     = clamp01(ramp_index);

    // 2. Turbulence index: turbulence intensity + micro-scale exposure.[file:3]
    // TI safe corridor ~ 0.1, hard ~ 0.4 (representative for fish/microbe).[file:3]
    const double ti_safe  = 0.10;
    const double ti_hard  = 0.40;
    const double tau_safe = 0.05;   // s.[file:3]
    const double tau_hard = 0.50;   // s.[file:3]

    const double ti = clamp01(in->turbulence_intensity);

    const double r_ti =
        clamp01((ti - ti_safe) / (ti_hard - ti_safe));
    const double r_tau =
        clamp01((in->micro_scale_s - tau_safe) / (tau_hard - tau_safe));

    const double turbulence_index = 0.7 * r_ti + 0.3 * r_tau;
    const double r_turbulence     = clamp01(turbulence_index);

    // 3. Habitat risk coordinate rhabitat.[file:3]
    // Aggregate ramp and turbulence with bias toward turbulence for fish/microbe.[file:3]
    double r_habitat = 0.4 * r_ramp + 0.6 * r_turbulence;
    r_habitat = clamp01(r_habitat);

    // 4. Lyapunov residual update and KER.[file:3]
    const double vt_before = (in->vt_before < 0.0) ? 0.0 : in->vt_before;
    // Treat ramp and turbulence as habitat plane contributions.[file:3]
    double vt_after = vt_before
                      + 0.4 * r_ramp * r_ramp
                      + 0.6 * r_turbulence * r_turbulence;
    vt_after = clamp01(vt_after);
    const double delta_vt = vt_after - vt_before;

    // Knowledge factor: high when ramp and turbulence are low and residual improves.[file:3]
    double k = 0.9 - 0.3 * r_habitat;
    if (delta_vt > 0.0) {
        k -= 0.2;
    }
    if (k < 0.0) k = 0.0;
    if (k > 1.0) k = 1.0;

    // Eco-impact factor: 0.9 minus vt_after minus habitat penalty.[file:3]
    double e = 0.9 - vt_after;
    e -= 0.2 * r_habitat;
    if (delta_vt > 0.0) {
        e -= 0.1;
    }
    if (e < 0.0) e = 0.0;
    if (e > 1.0) e = 1.0;

    // Risk-of-harm factor: residual plus direct habitat risk.[file:3]
    double r = vt_after + 0.3 * r_habitat;
    r = clamp01(r);

    // 5. Populate output POD.[file:3]
    out->ramp_index       = ramp_index;
    out->turbulence_index = turbulence_index;
    out->r_ramp           = r_ramp;
    out->r_turbulence     = r_turbulence;
    out->r_habitat        = r_habitat;
    out->k_factor         = k;
    out->e_factor         = e;
    out->r_factor         = r;
    out->evidence_hex     = in->node_hex_id;

    return 0;
}

} // extern "C"
