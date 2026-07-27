// filename: Prometheus-Praxis/src/cpp/waste/shredding/shredding_governance_adapter.cpp
// destination: github.com/mk-bluebird/Prometheus-Praxis/src/cpp/waste/shredding/shredding_governance_adapter.cpp
// license: MIT OR Apache-2.0
//
// Role:
// Implementation that composes CPP shredding/screening telemetry into a single
// KER snapshot by calling the non-actuating Rust FFI (blastradius-cross-spine
// style) and encoding KER, RoH, and blast-radius radii in CPP structs for
// Prometheus-Praxis governance tools; diagnostics only, no actuator surfaces.

#include "shredding_governance_adapter.hpp"

#include <cstddef>
#include <cstdint>

// Non-actuating Rust FFI surface.
// This follows the blastradius-cross-spine pattern where C/CPP provides
// POD structs and plain functions that Rust calls via extern "C" to compute
// KER and blast-radius metrics, or vice versa. Here, CPP calls into Rust.
extern "C" {

struct EcoNetKerInput
{
    double k_hint;
    double e_hint;
    double r_hint;
    double vt_slice;
    double roh_ceiling;

    double r_blast_radius_km;
    double r_topology;
};

struct EcoNetKerOutput
{
    double k;
    double e;
    double r;

    double vt;
    double roh;

    double blast_radius_km;
    double residual_ker_score;
    std::uint32_t lane_code;
};

// Rust-side function (non-actuating) that composes KER, RoH, and blast-radius
// for a shredding corridor window. Implemented in the EcoNet governance spine
// crate and exported with C ABI.
EcoNetKerOutput econet_governance_compute_shredding_ker(
    const EcoNetKerInput *input
);

} // extern "C"

namespace prometheus_praxis {
namespace waste {
namespace shredding {

ShreddingKerSnapshot ShreddingKerAdapter::computeKerSnapshot(
    const ShredderTelemetry &shredder,
    const ScreenDrumTelemetry &drum
) const noexcept
{
    EcoNetKerInput ker_in{};

    // Hints for KER composition based on local CPP normalization.
    double k_local = clamp01(
        0.6 * normalize_knowledge(shredder, drum) +
        0.4 * (1.0 - normalize_risk(shredder, drum))
    );
    double e_local = clamp01(
        normalize_eco_impact(shredder, drum)
    );
    double r_local = clamp01(
        normalize_risk(shredder, drum)
    );

    ker_in.k_hint = k_local;
    ker_in.e_hint = e_local;
    ker_in.r_hint = r_local;

    // Residual and RoH slice fed into Rust spine.
    double vt_local = shredder.vt_residual + drum.vt_residual;
    if (vt_local < 0.0) vt_local = 0.0;
    ker_in.vt_slice = vt_local;

    double roh_local = (shredder.roh_score > drum.roh_score)
        ? shredder.roh_score
        : drum.roh_score;
    ker_in.roh_ceiling = clamp01(roh_local);

    // Blast-radius and topology risk radii are diagnostics only:
    // blast-radius radius in km (e.g., waste impact footprint),
    // r_topology derived from corridor topology risk tables.
    ker_in.r_blast_radius_km = compute_blast_radius_km(shredder, drum);
    ker_in.r_topology = compute_topology_risk(shredder, drum);

    EcoNetKerOutput ker_out = econet_governance_compute_shredding_ker(&ker_in);

    ShreddingKerSnapshot out{};

    out.k = clamp01(ker_out.k);
    out.e = clamp01(ker_out.e);
    out.r = clamp01(ker_out.r);

    out.vt = (ker_out.vt < 0.0) ? 0.0 : ker_out.vt;
    out.roh = clamp01(ker_out.roh);

    out.plane_waste_id = plane_waste_id_;
    out.plane_topology_id = plane_topology_id_;

    // Map Rust lane code into CPP CorridorTag.
    out.lane_tag = lane_from_code(ker_out.lane_code);

    return out;
}

ShreddingKerSnapshot ShreddingKerAdapter::computeKerSnapshot(
    const ShredderTelemetry &shredder,
    const ScreenDrumTelemetry &drum,
    const AirClassifierTelemetry &classifier
) const noexcept
{
    EcoNetKerInput ker_in{};

    double k_local = clamp01(
        normalize_knowledge(shredder, drum, classifier)
    );
    double e_local = clamp01(
        normalize_eco_impact(shredder, drum, classifier)
    );
    double r_local = clamp01(
        normalize_risk(shredder, drum, classifier)
    );

    ker_in.k_hint = k_local;
    ker_in.e_hint = e_local;
    ker_in.r_hint = r_local;

    double vt_local =
        shredder.vt_residual +
        drum.vt_residual +
        classifier.vt_residual;
    if (vt_local < 0.0) vt_local = 0.0;
    ker_in.vt_slice = vt_local;

    double roh_local = shredder.roh_score;
    if (drum.roh_score > roh_local) roh_local = drum.roh_score;
    if (classifier.roh_score > roh_local) roh_local = classifier.roh_score;
    ker_in.roh_ceiling = clamp01(roh_local);

    ker_in.r_blast_radius_km =
        compute_blast_radius_km(shredder, drum, classifier);
    ker_in.r_topology =
        compute_topology_risk(shredder, drum, classifier);

    EcoNetKerOutput ker_out = econet_governance_compute_shredding_ker(&ker_in);

    ShreddingKerSnapshot out{};

    out.k = clamp01(ker_out.k);
    out.e = clamp01(ker_out.e);
    out.r = clamp01(ker_out.r);

    out.vt = (ker_out.vt < 0.0) ? 0.0 : ker_out.vt;
    out.roh = clamp01(ker_out.roh);

    out.plane_waste_id = plane_waste_id_;
    out.plane_topology_id = plane_topology_id_;

    out.lane_tag = lane_from_code(ker_out.lane_code);

    return out;
}

// ---------------------------------------------------------------------------
// Internal helpers (CPP-side diagnostics only)
// ---------------------------------------------------------------------------

CorridorTag ShreddingKerAdapter::lane_from_code(
    std::uint32_t code
) const noexcept
{
    switch (code) {
        case 0u: return CorridorTag::RESEARCH;
        case 1u: return CorridorTag::PILOT;
        case 2u: return CorridorTag::PRODUCTION;
        case 3u: return CorridorTag::BLOCKED;
        default: return CorridorTag::BLOCKED;
    }
}

double ShreddingKerAdapter::compute_blast_radius_km(
    const ShredderTelemetry &shredder,
    const ScreenDrumTelemetry &drum
) const noexcept
{
    // Simple diagnostic blast-radius heuristic:
    // combine power and overs fraction into a radius in km
    // bounded for governance plots; no physical actuation.
    double p_kw = (shredder.motor_power_kw <= 0.0)
        ? 0.0
        : shredder.motor_power_kw;
    double overs = drum.overs_fraction;
    if (overs < 0.0) overs = 0.0;
    if (overs > 1.0) overs = 1.0;

    double radius = 0.001 * p_kw * (0.5 + overs); // rough km footprint
    if (radius < 0.0) radius = 0.0;
    if (radius > 50.0) radius = 50.0; // governance cap
    return radius;
}

double ShreddingKerAdapter::compute_blast_radius_km(
    const ShredderTelemetry &shredder,
    const ScreenDrumTelemetry &drum,
    const AirClassifierTelemetry &classifier
) const noexcept
{
    double base = compute_blast_radius_km(shredder, drum);
    double heavies = classifier.heavies_fraction;
    if (heavies < 0.0) heavies = 0.0;
    if (heavies > 1.0) heavies = 1.0;

    double radius = base * (1.0 + 0.5 * heavies);
    if (radius < 0.0) radius = 0.0;
    if (radius > 50.0) radius = 50.0;
    return radius;
}

double ShreddingKerAdapter::compute_topology_risk(
    const ShredderTelemetry &shredder,
    const ScreenDrumTelemetry &drum
) const noexcept
{
    // Topology risk proxy using vt slice and overs fraction;
    // normalized into 0..1 for use as r_topology.
    double vt = shredder.vt_residual + drum.vt_residual;
    if (vt < 0.0) vt = 0.0;

    double overs = drum.overs_fraction;
    if (overs < 0.0) overs = 0.0;
    if (overs > 1.0) overs = 1.0;

    // vt corridor scaling: assume Vt up to 10 is typical.
    double vt_norm = (vt >= 10.0) ? 1.0 : (vt / 10.0);
    double rt = 0.6 * vt_norm + 0.4 * overs;
    return clamp01(rt);
}

double ShreddingKerAdapter::compute_topology_risk(
    const ShredderTelemetry &shredder,
    const ScreenDrumTelemetry &drum,
    const AirClassifierTelemetry &classifier
) const noexcept
{
    double base = compute_topology_risk(shredder, drum);
    double fines = classifier.fines_fraction;
    if (fines < 0.0) fines = 0.0;
    if (fines > 1.0) fines = 1.0;

    double rt = 0.7 * base + 0.3 * fines;
    return clamp01(rt);
}

} // namespace shredding
} // namespace waste
} // namespace prometheus_praxis
