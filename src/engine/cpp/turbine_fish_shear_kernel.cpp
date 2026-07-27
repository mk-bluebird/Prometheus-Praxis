// filename: src/engine/cpp/turbine_fish_shear_kernel.cpp
// target-repo: https://github.com/mk-bluebird/Prometheus-Praxis
// language: C++ (C ABI, ARM-friendly, non-actuating numerical kernel)
// license: MIT OR Apache-2.0

#include <cstddef>
#include <cstdint>

extern "C" {

// Input POD: microturbine operating point for fish-shear diagnostics.[file:3]
struct TurbineFishShearInput {
    double runner_diameter_m;    // Turbine runner diameter [m]
    double rotational_speed_rps; // Rotational speed [rev/s]
    double exposure_time_s;      // Exposure time τ [s] for fish in shear zone
    double delta_p_Pa;           // Pressure drop Δp across runner [Pa]
    double vt_before;           // Lyapunov residual slice before event
    std::uint32_t node_hex_id;   // Hex/evidence id for the turbine node
};

// Output POD: v_tip, shear, lethality and rfishshear + KER.[file:3]
struct TurbineFishShearOutput {
    double tip_velocity_mps;     // Blade tip velocity v_tip [m/s]
    double shear_rate_sinv;      // Shear rate γ [1/s]
    double pressure_drop_Pa;     // Δp [Pa]
    double lethality_index;      // Bioassay lethality L(v_tip, τ, Δp) [0..1]
    double r_fish_shear;         // Normalised rfishshear [0..1]
    double k_factor;             // Knowledge K [0..1]
    double e_factor;             // Eco-impact E [0..1]
    double r_factor;             // Risk-of-harm R [0..1]
    std::uint32_t evidence_hex;  // Evidence hex propagated from node_hex_id
};

static inline double clamp01(double x) {
    if (x < 0.0) return 0.0;
    if (x > 1.0) return 1.0;
    return x;
}

// Fish-shear kernel:
// - Computes v_tip, shear rate, Δp.
// - Maps bioassay lethality L(v_tip, τ, Δp) into rfishshear via safegoldhard corridors.
// - Produces KER triad consistent with hydraulic/bio planes for fish-safe operation.[file:3]
int compute_turbine_fish_shear(const void* in_buffer,
                               void* out_buffer,
                               std::size_t insize,
                               std::size_t outsize)
{
    if (in_buffer == nullptr || out_buffer == nullptr) {
        return 1;
    }

    if (insize != sizeof(TurbineFishShearInput) ||
        outsize != sizeof(TurbineFishShearOutput)) {
        return 2;
    }

    const auto* in = static_cast<const TurbineFishShearInput*>(in_buffer);
    auto* out = static_cast<TurbineFishShearOutput*>(out_buffer);

    if (in->runner_diameter_m <= 0.0 ||
        in->rotational_speed_rps < 0.0 ||
        in->exposure_time_s < 0.0 ||
        in->delta_p_Pa < 0.0) {
        out->tip_velocity_mps = 0.0;
        out->shear_rate_sinv  = 0.0;
        out->pressure_drop_Pa = in->delta_p_Pa;
        out->lethality_index  = 0.0;
        out->r_fish_shear     = 0.0;
        out->k_factor         = 0.0;
        out->e_factor         = 0.0;
        out->r_factor         = 1.0;
        out->evidence_hex     = in->node_hex_id;
        return 3;
    }

    // 1. Tip velocity v_tip.[file:3]
    // Circumference = π D, tip speed = circumference * rotational_speed_rps.[file:3]
    const double pi = 3.14159265358979323846;
    const double tip_velocity_mps =
        pi * in->runner_diameter_m * in->rotational_speed_rps;

    // 2. Shear rate γ ~ v_tip / characteristic length.[file:3]
    // Use diameter as characteristic length scale.
    double shear_rate_sinv = 0.0;
    if (in->runner_diameter_m > 0.0) {
        shear_rate_sinv = tip_velocity_mps / in->runner_diameter_m;
    }

    // 3. Pressure drop Δp is directly provided.[file:3]
    const double delta_p_Pa = in->delta_p_Pa;

    // 4. Bioassay lethality L(v_tip, τ, Δp).[file:3]
    // Use a sigmoidal/logistic-style mapping from thresholds:
    // - safe v_tip <= v_safe, tau <= tau_safe, Δp <= p_safe.
    // - lethality rises as these exceed safe thresholds.[file:3]
    const double v_safe   = 5.0;    // m/s, representative fish-safe tip speed.[file:3]
    const double v_hard   = 20.0;   // m/s, hard corridor.[file:3]
    const double tau_safe = 0.1;    // s, safe exposure time.[file:3]
    const double tau_hard = 1.0;    // s, hard exposure.[file:3]
    const double p_safe   = 5e4;    // Pa, safe pressure drop.[file:3]
    const double p_hard   = 3e5;    // Pa, hard corridor.[file:3]

    // Normalised hazard components.[file:3]
    const double v_norm =
        clamp01((tip_velocity_mps - v_safe) / (v_hard - v_safe));
    const double tau_norm =
        clamp01((in->exposure_time_s - tau_safe) / (tau_hard - tau_safe));
    const double p_norm =
        clamp01((delta_p_Pa - p_safe) / (p_hard - p_safe));

    // Weighted lethality index L in [0..1].[file:3]
    const double lethality_index =
        clamp01(0.5 * v_norm + 0.3 * tau_norm + 0.2 * p_norm);

    // 5. r_fish_shear via safegoldhard corridors.[file:3]
    // Interpret lethality_index as rfishshear directly, with corridor bands.[file:3]
    const double r_fish_shear = lethality_index;

    // 6. Lyapunov residual and KER triad for fish-shear plane.[file:3]
    const double vt_before = (in->vt_before < 0.0) ? 0.0 : in->vt_before;
    // Fish-shear is a biodiversity/hydraulics subplane; add its squared risk.[file:3]
    double vt_after = vt_before + 0.5 * r_fish_shear * r_fish_shear;
    vt_after = clamp01(vt_after);
    const double delta_vt = vt_after - vt_before;

    // Knowledge factor: high when lethality is low and residual improves.[file:3]
    double k = 0.9 - 0.4 * r_fish_shear;
    if (delta_vt > 0.0) {
        k -= 0.2;
    }
    if (k < 0.0) k = 0.0;
    if (k > 1.0) k = 1.0;

    // Eco-impact factor: 0.9 minus vt_after and fish-shear penalty.[file:3]
    double e = 0.9 - vt_after;
    e -= 0.2 * r_fish_shear;
    if (delta_vt > 0.0) {
        e -= 0.1;
    }
    if (e < 0.0) e = 0.0;
    if (e > 1.0) e = 1.0;

    // Risk-of-harm: use vt_after plus direct fish-shear risk component.[file:3]
    double r = vt_after + 0.3 * r_fish_shear;
    r = clamp01(r);

    // 7. Populate output POD.[file:3]
    out->tip_velocity_mps = tip_velocity_mps;
    out->shear_rate_sinv  = shear_rate_sinv;
    out->pressure_drop_Pa = delta_p_Pa;
    out->lethality_index  = lethality_index;
    out->r_fish_shear     = r_fish_shear;
    out->k_factor         = k;
    out->e_factor         = e;
    out->r_factor         = r;
    out->evidence_hex     = in->node_hex_id;

    return 0;
}

} // extern "C"
