
#include "ecoengine_surcharge.hpp"

extern "C" {

static inline double clamp01(double x) {
    if (x < 0.0) return 0.0;
    if (x > 1.0) return 1.0;
    return x;
}

int compute_blast_radius_flat(
    const void* in_buffer,
    void* out_buffer,
    std::size_t in_size,
    std::size_t out_size
) {
    if (in_buffer == nullptr || out_buffer == nullptr) {
        return 1;
    }
    if (in_size < sizeof(SurchargeEventInput) ||
        out_size < sizeof(BlastRadiusOutput)) {
        return 2;
    }

    const auto* in = static_cast<const SurchargeEventInput*>(in_buffer);
    auto* out = static_cast<BlastRadiusOutput*>(out_buffer);

    // Simple hydraulics approximations; replace with calibrated kernels.
    const double area_m2 = in->canal_width_m * in->surcharge_depth_m;
    const double velocity_mps =
        (area_m2 > 0.0) ? in->upstream_flow_m3s / area_m2 : 0.0;

    const double depth_decay = 0.35;         // corridor-derived factor
    const double scour_decay = 0.25;

    const double max_depth_downstream =
        in->surcharge_depth_m * depth_decay * (1.0 + in->gate_open_fraction);

    const double radius_overtop =
        in->canal_width_m * (0.5 + 0.5 * in->gate_open_fraction);

    const double radius_scour =
        velocity_mps * scour_decay * in->canal_length_m / 10.0;

    // Risk coordinates (PFAS/FOG plane, simplified).
    const double r_pfos =
        clamp01(0.4 * (in->bod_mgl / 20.0) +
                0.4 * (in->tss_mgl / 200.0) +
                0.2 * (in->soil_cec_cmolkg / 25.0));

    // Lyapunov residual update (local slice, no global state).
    const double vt_after = clamp01(0.3 * r_pfos * r_pfos + in->vt_before);
    const double delta_vt = vt_after - in->vt_before;

    // KER scoring consistent with existing grammar.[file:11]
    double k = 0.9 - 0.3 * r_pfos;
    if (delta_vt > 0.0) {
        k -= 0.2;
    }
    if (k < 0.0) k = 0.0;

    double e = 0.9 - vt_after;
    if (delta_vt > 0.0) {
        e -= 0.15;
    }
    if (e < 0.0) e = 0.0;

    double r = clamp01(vt_after + (delta_vt > 0.0 ? delta_vt : 0.0));

    out->max_depth_downstream_m = max_depth_downstream;
    out->max_velocity_mps = velocity_mps;
    out->radius_overtop_m = radius_overtop;
    out->radius_scour_m = radius_scour;
    out->pfos_risk_coord = r_pfos;
    out->k_factor = k;
    out->e_factor = e;
    out->r_factor = r;
    out->evidence_hex = in->hex_id;

    return 0;
}

} // extern "C"
