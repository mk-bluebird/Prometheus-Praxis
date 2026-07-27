// filename: src/engine/cpp/ecoengine_blastradius.cpp
// target-repo: https://github.com/mk-bluebird/Prometheus-Praxis
// language: C++ (C ABI, ARM-friendly, non-actuating numerical kernel)
// license: MIT OR Apache-2.0

#include <cstddef>
#include <cstdint>

// Numerical kernel for surcharge / blast-radius computation.
// - Flat POD buffers: SurchargeEventInput, BlastRadiusOutput.
// - Non-actuating diagnostics only: computes hydraulics, PFAS risk, plume extent, KER.
// - ARM-friendly: no dynamic allocation, no exceptions, small stack footprint.
// - Intended deployment: embedded controllers running non-actuating diagnostics,
//   with actuation handled by separate, spine-gated controllers in other crates.

// This header-style section mirrors the POD structs defined in your existing
// EcoNet / Cyboquatic surcharge kernel, but in C++ for this engine module.[file:3]
extern "C" {

// Flat POD input: matches hydraulics.corridors.phx.v1 surcharge event grammar.[file:3]
struct SurchargeEventInput {
    double canal_length_m;        // Physical canal length [m]
    double canal_width_m;         // Canal inner width [m]
    double upstream_flow_m3s;     // Upstream volumetric flow [m^3/s]
    double surcharge_depth_m;     // Depth above normal water level [m]
    double gate_open_fraction;    // Gate opening, normalized [0..1]
    double soil_cec_cmolkg;       // Soil CEC, cation exchange capacity [cmol/kg]
    double bod_mgL;               // BOD concentration [mg/L]
    double tss_mgL;               // TSS concentration [mg/L]
    double vt_before;             // Lyapunov residual slice before event
    std::uint32_t hex_id;         // Evidence hex-stamp / Phoenix hex id
};

// Flat POD output: blast radius diagnostics consistent with KER grammar.[file:3]
struct BlastRadiusOutput {
    double max_depth_downstream_m; // Max surcharge depth downstream [m]
    double max_velocity_mps;       // Maximum estimated velocity [m/s]
    double radius_overtop_m;       // Lateral overtopping radius [m]
    double radius_scour_m;         // Longitudinal scour radius [m]
    double pfos_risk_coord;        // PFAS/FOG risk plane coordinate [0..1]
    double k_factor;               // Knowledge factor K [0..1]
    double e_factor;               // Eco-impact factor E [0..1]
    double r_factor;               // Risk-of-harm factor R [0..1]
    std::uint32_t evidence_hex;    // Evidence hex-stamp propagated from input
};

// Clamp helper: keeps diagnostics in corridor-normalized [0..1] band.[file:3]
static inline double clamp01(double x) {
    if (x < 0.0) {
        return 0.0;
    }
    if (x > 1.0) {
        return 1.0;
    }
    return x;
}

// Core computation: surcharge hydraulics + PFAS risk + KER triad.
// in_buffer  points to SurchargeEventInput POD.
// out_buffer points to BlastRadiusOutput POD.
// insize     must equal sizeof(SurchargeEventInput).
// outsize    must equal sizeof(BlastRadiusOutput).
//
// Return codes:
//   0 -> success.
//   1 -> null buffer pointer.
//   2 -> size mismatch (insize/outsize do not match POD sizes).
//   3 -> numerically unsafe input (e.g., non-positive width/depth for division).
//
// This function performs no allocation and no I/O; it is pure numerics for
// non-actuating diagnostics on embedded ARM devices.[file:3]
int compute_blast_radius_flat(const void* in_buffer,
                              void* out_buffer,
                              std::size_t insize,
                              std::size_t outsize)
{
    if (in_buffer == nullptr || out_buffer == nullptr) {
        return 1;
    }

    if (insize != sizeof(SurchargeEventInput) ||
        outsize != sizeof(BlastRadiusOutput)) {
        return 2;
    }

    const auto* in = static_cast<const SurchargeEventInput*>(in_buffer);
    auto* out = static_cast<BlastRadiusOutput*>(out_buffer);

    // Sanity checks for numerically safe hydraulics.
    if (in->canal_width_m <= 0.0 ||
        in->surcharge_depth_m <= 0.0 ||
        in->canal_length_m <= 0.0) {
        // Avoid division by zero / negative geometry.
        out->max_depth_downstream_m = 0.0;
        out->max_velocity_mps       = 0.0;
        out->radius_overtop_m       = 0.0;
        out->radius_scour_m         = 0.0;
        out->pfos_risk_coord        = 0.0;
        out->k_factor               = 0.0;
        out->e_factor               = 0.0;
        out->r_factor               = 1.0;
        out->evidence_hex           = in->hex_id;
        return 3;
    }

    // 1. Hydraulics approximations (consistent with prior C kernel).[file:3]
    const double area_m2 = in->canal_width_m * in->surcharge_depth_m;
    double velocity_mps  = 0.0;
    if (area_m2 > 0.0) {
        velocity_mps = in->upstream_flow_m3s / area_m2;
        if (velocity_mps < 0.0) {
            velocity_mps = 0.0;
        }
    }

    // Corridor-derived decay factors: tuned in drainage-decay shards.[file:3]
    const double depth_decay = 0.35;
    const double scour_decay = 0.25;

    // Gate factor clamped to [0..1].
    const double gate_f = clamp01(in->gate_open_fraction);

    // Downstream depth: surcharge attenuates with gate opening and distance.[file:3]
    const double max_depth_downstream =
        in->surcharge_depth_m * (1.0 - depth_decay * gate_f);

    // Overtopping radius: scaled with canal width and gate fraction.[file:3]
    const double radius_overtop =
        in->canal_width_m * (0.5 + 0.5 * gate_f);

    // Scour radius: simple proportional to velocity and length with decay.[file:3]
    const double radius_scour =
        velocity_mps * scour_decay * (in->canal_length_m / 10.0);

    // 2. PFAS/FOG risk coordinate: normalized plane in [0..1].[file:3]
    // This matches the pattern described in your Cyboquatic spine:
    // r_pfos = 0.4 * BOD/20 + 0.4 * TSS/200 + 0.2 * CEC/25, clamped.[file:3]
    const double bod_term   = 0.4 * (in->bod_mgL / 20.0);
    const double tss_term   = 0.4 * (in->tss_mgL / 200.0);
    const double cec_term   = 0.2 * (in->soil_cec_cmolkg / 25.0);
    const double r_pfos_raw = bod_term + tss_term + cec_term;
    const double r_pfos     = clamp01(r_pfos_raw);

    // 3. Lyapunov residual update: vt_after and delta vt.[file:3]
    // vt_after := clamp01(0.3 * r_pfos + r_pfos + vt_before).[file:3]
    const double vt_before = (in->vt_before < 0.0) ? 0.0 : in->vt_before;
    double vt_after = 0.3 * r_pfos + r_pfos + vt_before;
    vt_after = clamp01(vt_after);
    const double delta_vt = vt_after - vt_before;

    // 4. KER scoring consistent with ecosafety core grammar.[file:3]
    // K: Knowledge factor decreases with PFOS risk and deteriorating residual.[file:3]
    double k = 0.9 - 0.3 * r_pfos;
    if (delta_vt > 0.0) {
        k -= 0.2;
    }
    if (k < 0.0) {
        k = 0.0;
    }
    if (k > 1.0) {
        k = 1.0;
    }

    // E: Eco-impact factor derived from 0.9 - vt_after, penalized by worsening vt.[file:3]
    double e = 0.9 - vt_after;
    if (delta_vt > 0.0) {
        e -= 0.15;
    }
    if (e < 0.0) {
        e = 0.0;
    }
    if (e > 1.0) {
        e = 1.0;
    }

    // R: Risk-of-harm coordinate from vt_after and delta vt.[file:3]
    // If vt worsens, R is vt_after; otherwise a smaller residual-based value.[file:3]
    double r = vt_after;
    if (delta_vt <= 0.0) {
        // Improvement or neutral; reduce risk proportional to improvement.[file:3]
        r = vt_after * 0.5;
    }
    r = clamp01(r);

    // 5. Populate output POD.[file:3]
    out->max_depth_downstream_m = (max_depth_downstream < 0.0)
                                  ? 0.0
                                  : max_depth_downstream;
    out->max_velocity_mps       = velocity_mps;
    out->radius_overtop_m       = (radius_overtop < 0.0)
                                  ? 0.0
                                  : radius_overtop;
    out->radius_scour_m         = (radius_scour < 0.0)
                                  ? 0.0
                                  : radius_scour;
    out->pfos_risk_coord        = r_pfos;
    out->k_factor               = k;
    out->e_factor               = e;
    out->r_factor               = r;
    out->evidence_hex           = in->hex_id;

    return 0;
}

} // extern "C"
