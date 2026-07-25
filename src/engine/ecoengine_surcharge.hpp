
#pragma once
#include <cstddef>
#include <cstdint>

extern "C" {

// Flat POD input, no dynamic members.
struct SurchargeEventInput {
    double canal_length_m;
    double canal_width_m;
    double upstream_flow_m3s;
    double surcharge_depth_m;
    double gate_open_fraction;   // 0..1
    double soil_cec_cmolkg;
    double bod_mgl;
    double tss_mgl;
    double vt_before;            // Lyapunov residual slice
    uint32_t hex_id;             // Phoenix hex anchor id
};

// Flat POD output, sized for blast-radius diagnostics.
struct BlastRadiusOutput {
    double max_depth_downstream_m;
    double max_velocity_mps;
    double radius_overtop_m;     // lateral extent of overtopping
    double radius_scour_m;       // scour footprint along canal
    double pfos_risk_coord;      // 0..1, PFAS/FOG risk plane
    double k_factor;             // Knowledge
    double e_factor;             // Eco-impact
    double r_factor;             // Risk-of-harm
    uint32_t evidence_hex;       // hex-stamp for this event
};

// Compute blast radius using a flat buffer:
// [SurchargeEventInput][BlastRadiusOutput]
// in_buffer points to start of SurchargeEventInput.
// out_buffer points to start of BlastRadiusOutput (may alias).
int compute_blast_radius_flat(
    const void* in_buffer,
    void* out_buffer,
    std::size_t in_size,
    std::size_t out_size
);

} // extern "C"
