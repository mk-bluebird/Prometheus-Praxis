// filename: Prometheus-Praxis/src/cpp/waste/shredding/screening_module.cpp
// destination: github.com/mk-bluebird/Prometheus-Praxis/src/cpp/waste/shredding/screening_module.cpp
// license: MIT OR Apache-2.0
//
// Role:
// Implementation that converts drum speed, differential pressure, feed rate,
// and cut‑point estimates into EcoNet blast‑radius and KER diagnostic calls
// (via Rust CDylib), tagging each screening lane with RoH ceilings and
// corridor status for governance views while remaining strictly observational.

#include "screening_module.hpp"

#include <cstddef>
#include <cstdint>

namespace prometheus_praxis {
namespace waste {
namespace shredding {

// ---------------------------------------------------------------------------
// FFI to Rust CDylib: EcoNet blast-radius and KER diagnostics
// ---------------------------------------------------------------------------
//
// The Rust CDylib exposes non-actuating diagnostic functions that operate on
// normalized telemetry only. C++ never touches hardware or fieldbus APIs.

// KER diagnostic snapshot for a single screening lane.
struct KerLaneSnapshot
{
    double k;            // knowledge factor [0,1]
    double e;            // eco-impact factor [0,1]
    double r;            // risk factor [0,1]
    double vt_residual;  // Lyapunov residual slice [>=0]
    double roh_score;    // RoH ceiling [0,1]
    double blast_radius; // EcoNet blast radius [km]
};

// Corridor status flags emitted by Rust governance kernel.
struct CorridorStatus
{
    bool within_corridor;
    bool monotone_safe;
    bool blast_radius_ok;
};

// Rust CDylib FFI signatures.
// The implementation lives in a Rust crate compiled as CDylib,
// for example: crates/econet-governance-spine or crates/econet-blast-radius.
// These functions are pure diagnostics with no actuation side-effects.

extern "C" KerLaneSnapshot econet_compute_blast_radius_and_ker(
    double feed_rate_kg_per_h,
    double rotational_speed_rpm,
    double pressure_drop_pa,
    double cut_point_mm
);

extern "C" CorridorStatus econet_check_corridor_status(
    std::uint32_t lane_index,
    double vt_residual,
    double roh_score,
    double blast_radius_km,
    double roh_ceiling,
    double vt_ceiling
);

// ---------------------------------------------------------------------------
// Internal helpers: normalization and blast-radius hints
// ---------------------------------------------------------------------------

// Normalize drum speed in rpm into [0,1] band.
static double normalize_drum_speed(double rpm)
{
    if (rpm <= 0.0) {
        return 0.0;
    }
    const double RPM_MAX = 30.0;
    double ratio = rpm / RPM_MAX;
    if (ratio < 0.0) ratio = 0.0;
    if (ratio > 1.0) ratio = 1.0;
    return ratio;
}

// Normalize differential pressure into [0,1] band.
static double normalize_pressure_drop(double pressure_drop_pa)
{
    if (pressure_drop_pa <= 0.0) {
        return 0.0;
    }
    const double P_MAX = 1500.0;
    double ratio = pressure_drop_pa / P_MAX;
    if (ratio < 0.0) ratio = 0.0;
    if (ratio > 1.0) ratio = 1.0;
    return ratio;
}

// EcoNet blast-radius hint based on feed rate and overs fraction.
// This is a local heuristic; the authoritative blast radius comes from Rust.
static double local_blast_radius_hint(
    double feed_rate_kg_per_h,
    double overs_fraction
)
{
    if (feed_rate_kg_per_h <= 0.0) {
        return 0.0;
    }
    if (overs_fraction < 0.0) overs_fraction = 0.0;
    if (overs_fraction > 1.0) overs_fraction = 1.0;

    // Simple proportional model: higher feed and overs => larger blast radius.
    double scaled = (feed_rate_kg_per_h / 1000.0) * overs_fraction;
    if (scaled < 0.0) scaled = 0.0;
    const double BR_MAX = 5.0;
    if (scaled > BR_MAX) scaled = BR_MAX;
    return scaled;
}

// ---------------------------------------------------------------------------
// Public API: lane tagging and telemetry enrichment
// ---------------------------------------------------------------------------

// Tag a single drum-screen lane with RoH ceilings and corridor status.
// This function is strictly observational and does not drive hardware.
bool tag_drum_screen_lane(
    std::uint32_t lane_index,
    const ScreenDrumTelemetry &drum,
    const ScreenDrumGeometry &geometry,
    ScreenLaneCorridor &lane_corridor,
    KerLaneSnapshot &lane_ker,
    CorridorStatus &lane_status
)
{
    if (geometry.lanes == 0U) {
        return false;
    }
    if (lane_index >= geometry.lanes) {
        return false;
    }

    // Normalize key telemetry dimensions.
    double norm_speed = normalize_drum_speed(drum.rotational_speed_rpm);
    double norm_occupancy = drum_occupancy_roh_hint(drum.occupancy_frac);
    double norm_feed = drum.feed_rate_kg_per_h <= 0.0
        ? 0.0
        : drum.feed_rate_kg_per_h / 1000.0;

    // Air-side proxy: differential pressure approximated from occupancy and speed.
    double pressure_proxy_pa = (norm_speed + norm_occupancy) * 500.0;

    // Cut-point proxy: geometry tilt and open area modulate effective cut-point.
    double cut_point_mm = drum.avg_particle_size_mm;

    // Call into Rust EcoNet blast-radius and KER diagnostics.
    KerLaneSnapshot snap = econet_compute_blast_radius_and_ker(
        drum.feed_rate_kg_per_h,
        drum.rotational_speed_rpm,
        pressure_proxy_pa,
        cut_point_mm
    );

    // Copy back KER and residual into lane corridor and local snapshot.
    lane_ker = snap;

    lane_corridor.max_roh_score = snap.roh_score;
    lane_corridor.max_vt_residual = snap.vt_residual;

    // Compute local blast-radius hint for governance dashboards.
    double br_hint = local_blast_radius_hint(
        drum.feed_rate_kg_per_h,
        drum.overs_fraction
    );

    // Corridor ceilings are derived from Rust vt_residual and roh_score.
    double roh_ceiling = snap.roh_score;
    double vt_ceiling = snap.vt_residual;

    // Corridor status via Rust governance kernel.
    CorridorStatus status = econet_check_corridor_status(
        lane_index,
        snap.vt_residual,
        snap.roh_score,
        snap.blast_radius,
        roh_ceiling,
        vt_ceiling
    );

    lane_status = status;

    // Telemetry struct remains untouched for upstream analytics;
    // we only annotate corridor and KER hints.

    (void)norm_feed;
    (void)br_hint;

    return true;
}

// Tag air-classifier lanes with RoH ceilings and corridor status.
// Observational only, suitable for EcoNet governance views.
bool tag_air_classifier_lane(
    std::uint32_t lane_index,
    const AirClassifierTelemetry &classifier,
    const AirClassifierGeometry &geometry,
    ScreenLaneCorridor &lane_corridor,
    KerLaneSnapshot &lane_ker,
    CorridorStatus &lane_status
)
{
    if (geometry.lanes == 0U) {
        return false;
    }
    if (lane_index >= geometry.lanes) {
        return false;
    }

    // Normalize air velocity and pressure drop.
    double norm_velocity = classifier.air_velocity_m_per_s <= 0.0
        ? 0.0
        : classifier.air_velocity_m_per_s / 30.0;
    double roh_hint = air_velocity_roh_hint(classifier.air_velocity_m_per_s);
    double pressure_drop_pa = classifier.pressure_drop_pa;

    // Cut-point from geometry, feed-rate from telemetry.
    double cut_point_mm = geometry.cut_point_mm;

    KerLaneSnapshot snap = econet_compute_blast_radius_and_ker(
        classifier.feed_rate_kg_per_h,
        norm_velocity * 30.0,
        pressure_drop_pa,
        cut_point_mm
    );

    lane_ker = snap;

    lane_corridor.max_roh_score = snap.roh_score;
    lane_corridor.max_vt_residual = snap.vt_residual;

    double roh_ceiling = snap.roh_score;
    double vt_ceiling = snap.vt_residual;

    CorridorStatus status = econet_check_corridor_status(
        lane_index,
        snap.vt_residual,
        snap.roh_score,
        snap.blast_radius,
        roh_ceiling,
        vt_ceiling
    );

    lane_status = status;

    (void)roh_hint;

    return true;
}

} // namespace shredding
} // namespace waste
} // namespace prometheus_praxis
