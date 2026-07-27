// File: Prometheus-Praxis/src/cpp/waste/conveyance/conveyor_graph.cpp
// License: MIT OR Apache-2.0
//
// Non-actuating implementation that maps conveyor sensor telemetry
// (belt speed, load cell readings, chute occupancy) into blast-radius
// diagnostics per segment via Rust EcoNet FFI. The resulting annotations
// resemble KerBlastRadiusSnapshot data (carbon/biodiversity radii,
// K/E/R, RoH) so higher-level governance code can apply Lyapunov and
// lane guards without any motor control.[file:6][file:8]

#include "conveyor_graph.hpp"

#include <algorithm>
#include <stdexcept>

extern "C" {

/// Rust EcoNet FFI surface (to be provided by governance crates).
/// This function consumes per-segment telemetry aggregates and
/// returns a blast-radius / KER / RoH snapshot for governance.
/// The exact Rust implementation lives in the EcoNet spine and
/// is compiled as a cdylib for C ABI use.[web:34][web:39]
struct KerBlastRadiusSnapshot {
    double carbon_radius;      // [0,1]
    double biodiversity_radius;// [0,1]
    double k;                  // [0,1]
    double e;                  // [0,1]
    double r;                  // [0,1]
    double roh;                // [0,1]
};

/// FFI: compute blast-radius snapshot for a conveyor segment.
/// All inputs are telemetry aggregates over a time window.
/// Returns a KerBlastRadiusSnapshot with bounded values.
KerBlastRadiusSnapshot econet_compute_conveyor_blast_radius(
    const char* segment_id,
    double belt_speed_m_s,
    double load_cell_kg,
    double chute_occupancy_frac,
    double nominal_load_kg_h,
    double peak_load_kg_h,
    std::uint8_t material_class_code);

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

/// Telemetry aggregate for a conveyor segment over a window.
struct ConveyorTelemetryWindow {
    double belt_speed_m_s;        // belt speed [m/s]
    double load_cell_kg;         // instantaneous / average load [kg]
    double chute_occupancy_frac; // chute occupancy fraction [0,1]

    ConveyorTelemetryWindow()
        : belt_speed_m_s(0.0),
          load_cell_kg(0.0),
          chute_occupancy_frac(0.0) {}

    void validate_and_normalize() {
        if (belt_speed_m_s < 0.0) {
            throw std::invalid_argument("belt_speed_m_s must be non-negative.");
        }
        if (load_cell_kg < 0.0) {
            throw std::invalid_argument("load_cell_kg must be non-negative.");
        }
        chute_occupancy_frac = clamp01(chute_occupancy_frac);
    }
};

/// Annotate a single ConveyorSegment with KerBlastRadiusSnapshot-like data
/// using Rust EcoNet FFI. This function does not modify actuators; it
/// updates telemetry-only KER and RoH fields, and blast-radius weights.
static void annotate_segment_with_blast_radius(
    ConveyorSegment& seg,
    const ConveyorTelemetryWindow& telemetry)
{
    ConveyorTelemetryWindow tw = telemetry;
    tw.validate_and_normalize();
    seg.validate_and_normalize();

    const std::uint8_t material_code =
        static_cast<std::uint8_t>(seg.material_class);

    KerBlastRadiusSnapshot snapshot =
        econet_compute_conveyor_blast_radius(
            seg.segment_id.c_str(),
            tw.belt_speed_m_s,
            tw.load_cell_kg,
            tw.chute_occupancy_frac,
            seg.nominal_load_kg_h,
            seg.peak_load_kg_h,
            material_code);

    // Clamp all snapshot fields into [0,1] defensively.
    const double carbon_radius       = clamp01(snapshot.carbon_radius);
    const double biodiversity_radius = clamp01(snapshot.biodiversity_radius);
    const double k                   = clamp01(snapshot.k);
    const double e                   = clamp01(snapshot.e);
    const double r                   = clamp01(snapshot.r);
    const double roh_scalar          = clamp01(snapshot.roh);

    // Update KER triad.
    seg.ker.k = k;
    seg.ker.e = e;
    seg.ker.r = r;
    seg.ker.clamp_unit_interval();

    // Update RoH ceiling telemetry.
    seg.roh.roh_ceiling = roh_scalar;
    seg.roh.clamp_unit_interval();

    // Update blast-radius plane weight as a simple combination
    // of carbon and biodiversity radii, suitable for diagnostics.
    double combined_radius =
        0.6 * carbon_radius + 0.4 * biodiversity_radius;
    seg.plane_weight = clamp01(combined_radius);
}

/// Public helper: annotate all segments in the graph with blast-radius
/// diagnostics based on the provided telemetry windows.
/// The telemetry map is keyed by segment_id and contains window
/// aggregates for belt speed, load, and chute occupancy.
/// Missing entries are skipped and leave existing KER/RoH intact.
void annotate_graph_with_blast_radius(
    ConveyorGraph& graph,
    const std::unordered_map<std::string, ConveyorTelemetryWindow>& telemetry_by_segment)
{
    // Iterate over all segments and annotate those with telemetry.
    for (auto& kv : graph.segments_) {
        const std::string& seg_id = kv.first;
        ConveyorSegment& seg      = kv.second;

        auto it = telemetry_by_segment.find(seg_id);
        if (it == telemetry_by_segment.end()) {
            // No telemetry for this segment in the current window;
            // keep existing annotations unchanged.
            continue;
        }

        annotate_segment_with_blast_radius(seg, it->second);
    }

    // Mark adjacency as dirty so any downstream routing diagnostics
    // can be recalculated with updated segment state.
    graph.adjacency_dirty_ = true;
}

/// Optional helper: derive a KerBlastRadiusSnapshot-like summary for a node.
/// This aggregates outgoing segment blast-radius annotations into a single
/// diagnostic snapshot, useful for governance dashboards. No actuation.
KerBlastRadiusSnapshot node_blast_radius_summary(
    const ConveyorGraph& graph,
    const std::string& node_id)
{
    KerBlastRadiusSnapshot summary{};
    summary.carbon_radius       = 0.0;
    summary.biodiversity_radius = 0.0;
    summary.k                   = 0.0;
    summary.e                   = 0.0;
    summary.r                   = 0.0;
    summary.roh                 = 0.0;

    const auto outgoing = graph.outgoing_from(node_id);
    if (outgoing.empty()) {
        return summary;
    }

    double total_weight = 0.0;
    for (const ConveyorSegment* seg : outgoing) {
        double w = clamp01(seg->plane_weight);
        if (w <= 0.0) {
            continue;
        }
        total_weight += w;

        const double k = clamp01(seg->ker.k);
        const double e = clamp01(seg->ker.e);
        const double r = clamp01(seg->ker.r);
        const double roh_scalar = clamp01(seg->roh.roh_ceiling);

        // Approximate carbon/biodiversity radii from plane_weight and material.
        const double carbon_radius =
            (seg->material_class == MaterialClass::PFAS_RISK ||
             seg->material_class == MaterialClass::METALS)
                ? w
                : 0.5 * w;
        const double biodiversity_radius =
            (seg->material_class == MaterialClass::ORGANICS)
                ? w
                : 0.5 * w;

        summary.carbon_radius       += w * carbon_radius;
        summary.biodiversity_radius += w * biodiversity_radius;
        summary.k                   += w * k;
        summary.e                   += w * e;
        summary.r                   += w * r;
        summary.roh                 += w * roh_scalar;
    }

    if (total_weight > 0.0) {
        const double inv = 1.0 / total_weight;
        summary.carbon_radius       = clamp01(summary.carbon_radius * inv);
        summary.biodiversity_radius = clamp01(summary.biodiversity_radius * inv);
        summary.k                   = clamp01(summary.k * inv);
        summary.e                   = clamp01(summary.e * inv);
        summary.r                   = clamp01(summary.r * inv);
        summary.roh                 = clamp01(summary.roh * inv);
    }

    return summary;
}

} // namespace conveyance
} // namespace waste
} // namespace prometheus_praxis
