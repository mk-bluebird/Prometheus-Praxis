// File: Prometheus-Praxis/src/cpp/waste/conveyance/magnet_module.cpp
// License: MIT OR Apache-2.0
//
// Non-actuating implementation that converts magnet power,
// capture efficiency, and upstream/downstream flow telemetry
// into KER coordinates and blast-radius influences using
// EcoNet SQLite views (via Rust cdylib). It returns telemetry-
// only MagnetRoutingEnvelope objects suitable for lane-
// admissibility and ecowealth scoring logic, without any
// motor control.[file:6][file:8][web:46][web:52]

#include "magnet_module.hpp"

#include <algorithm>
#include <stdexcept>
#include <unordered_map>

extern "C" {

/// Rust EcoNet FFI surface exposed from a cdylib.
/// This function derives blast-radius and KER metrics for a
/// magnet node by querying EcoNet SQLite views and applying
/// governance math over the provided telemetry aggregates.[web:39]
struct EcoNetMagnetSnapshot {
    double carbon_radius;       // [0,1]
    double biodiversity_radius; // [0,1]
    double k;                   // [0,1]
    double e;                   // [0,1]
    double r;                   // [0,1]
    double roh;                 // [0,1]
    double eco_per_joule;       // [0,1]
};

EcoNetMagnetSnapshot econet_compute_magnet_snapshot(
    const char* node_id,
    const char* lane_id,
    double ferrous_capture_rate,
    double nonferrous_bypass_rate,
    double contamination_rate,
    double energy_draw_w,
    double energy_per_tonne_kwh);

} // extern "C"

namespace prometheus_praxis {
namespace waste {
namespace conveyance {

namespace {

inline double clamp01(double x) {
    if (x < 0.0) return 0.0;
    if (x > 1.0) return 1.0;
    return x;
}

} // namespace

void MagnetNodeTelemetry::validate_and_normalize() {
    // Energy metrics must be non-negative.
    if (energy_draw_w < 0.0) {
        throw std::invalid_argument("MagnetNodeTelemetry.energy_draw_w must be non-negative.");
    }
    if (energy_per_tonne_kwh < 0.0) {
        throw std::invalid_argument("MagnetNodeTelemetry.energy_per_tonne_kwh must be non-negative.");
    }

    // Capture and contamination fractions in [0,1].
    ferrous_capture_rate   = clamp01(ferrous_capture_rate);
    nonferrous_bypass_rate = clamp01(nonferrous_bypass_rate);
    contamination_rate     = clamp01(contamination_rate);

    // Eco-per-joule and KER/RoH bounded.
    eco_per_joule = clamp01(eco_per_joule);
    ker.clamp_unit_interval();
    roh.clamp_unit_interval();
}

void MagnetRoutingEnvelope::clamp_bounded_metrics() {
    carbon_radius       = clamp01(carbon_radius);
    biodiversity_radius = clamp01(biodiversity_radius);
    plane_weight        = clamp01(plane_weight);
    eco_per_joule       = clamp01(eco_per_joule);
    ker.clamp_unit_interval();
    roh.clamp_unit_interval();
}

/// Internal helper: build a MagnetRoutingEnvelope from a
/// MagnetNodeTelemetry record and an EcoNetMagnetSnapshot.
/// This function is strictly non-actuating and produces
/// governance-ready envelopes.
static MagnetRoutingEnvelope build_envelope_from_snapshot(
    const MagnetNodeTelemetry& telem,
    const EcoNetMagnetSnapshot& snap)
{
    MagnetRoutingEnvelope env;
    env.node_id              = telem.node_id;
    env.lane_id              = telem.lane_id;
    env.blast_radius_plane_id= telem.blast_radius_plane_id;

    // Copy bounded blast-radius and KER/RoH metrics.
    env.carbon_radius       = clamp01(snap.carbon_radius);
    env.biodiversity_radius = clamp01(snap.biodiversity_radius);
    env.plane_weight        = clamp01(
        0.5 * env.carbon_radius + 0.5 * env.biodiversity_radius);

    env.ker.k = clamp01(snap.k);
    env.ker.e = clamp01(snap.e);
    env.ker.r = clamp01(snap.r);

    env.roh.roh_ceiling = clamp01(snap.roh);

    // Energy and eco-per-joule telemetry.
    env.eco_per_joule       = clamp01(snap.eco_per_joule);
    env.energy_draw_w       = telem.energy_draw_w;
    env.energy_per_tonne_kwh= telem.energy_per_tonne_kwh;

    env.clamp_bounded_metrics();
    return env;
}

/// Public function: convert a single MagnetNodeTelemetry record
/// into a MagnetRoutingEnvelope by calling the EcoNet Rust cdylib.
/// This is the main entry point for governance code that wants
/// non-actuating magnet routing envelopes for lane admissibility
/// and ecowealth scoring.
MagnetRoutingEnvelope compute_magnet_routing_envelope(
    MagnetNodeTelemetry& telem)
{
    telem.validate_and_normalize();

    EcoNetMagnetSnapshot snap =
        econet_compute_magnet_snapshot(
            telem.node_id.c_str(),
            telem.lane_id.c_str(),
            telem.ferrous_capture_rate,
            telem.nonferrous_bypass_rate,
            telem.contamination_rate,
            telem.energy_draw_w,
            telem.energy_per_tonne_kwh);

    return build_envelope_from_snapshot(telem, snap);
}

/// Batch helper: compute routing envelopes for a set of
/// magnet telemetry records keyed by node_id. This is useful
/// when ingesting magnet lanes from telemetry streams into
/// EcoNet SQLite-backed governance code.
std::unordered_map<std::string, MagnetRoutingEnvelope>
compute_magnet_routing_envelopes_batch(
    std::unordered_map<std::string, MagnetNodeTelemetry>& telemetry_by_node)
{
    std::unordered_map<std::string, MagnetRoutingEnvelope> result;
    result.reserve(telemetry_by_node.size());

    for (auto& kv : telemetry_by_node) {
        MagnetNodeTelemetry& telem = kv.second;
        MagnetRoutingEnvelope env  = compute_magnet_routing_envelope(telem);
        result.emplace(telem.node_id, env);
    }

    return result;
}

} // namespace conveyance
} // namespace waste
} // namespace prometheus_praxis
