// File: Prometheus-Praxis/src/cpp/waste/wastewater/pump_telemetry.cpp
// License: MIT OR Apache-2.0
//
// Non-actuating implementation that maps raw pump/screen sensor
// data into normalized, corridor-checked telemetry. It calls into
// EcoNet / EcoRestoration Rust crates (via cdylib FFI backed by
// SQLite governance views) to read KER windows, plane-weights,
// and Lyapunov residuals, and returns immutable PumpCorridorStatus
// records without issuing any start/stop commands.[file:6][file:8][web:64][web:67][web:72]

#include "pump_telemetry.hpp"

#include <algorithm>
#include <stdexcept>

extern "C" {

/// Snapshot returned by EcoNet / EcoRestoration governance spine
/// for a pump or screen window. This is computed from SQLite views
/// and Rust governance crates, not from CPP directly.[web:64][web:67]
struct EcoNetPumpSnapshot {
    double k;            // [0,1]
    double e;            // [0,1]
    double r;            // [0,1]
    double roh;          // [0,1]
    bool   within_corridor;
    bool   no_build;
    bool   start_allowed;
    bool   stop_allowed;
};

/// Rust FFI: compute governance snapshot for a pump window.
/// Implemented in EcoNet / EcoRestoration Rust crates, using
/// SQLite KER windows, plane-weights, and Lyapunov residuals.
EcoNetPumpSnapshot econet_compute_pump_snapshot(
    const char* pump_id,
    const char* corridor_id,
    double turbidity_ntu,
    double dissolved_oxygen_mg_l,
    double flow_m3_s,
    double head_m,
    double energy_kwh);

/// Rust FFI: compute governance snapshot for a screen window.
EcoNetPumpSnapshot econet_compute_screen_snapshot(
    const char* screen_id,
    const char* corridor_id,
    double influent_turbidity_ntu,
    double effluent_turbidity_ntu,
    double solids_loading_kg_h,
    double delta_head_m,
    double energy_kwh);

} // extern "C"

namespace prometheus_praxis {
namespace waste {
namespace wastewater {

namespace {

inline double clamp01(double x) {
    if (x < 0.0) return 0.0;
    if (x > 1.0) return 1.0;
    return x;
}

} // namespace

void PumpTelemetry::validate_and_normalize() {
    if (turbidity_ntu < 0.0) {
        throw std::invalid_argument("PumpTelemetry.turbidity_ntu must be non-negative.");
    }
    if (dissolved_oxygen_mg_l < 0.0) {
        throw std::invalid_argument("PumpTelemetry.dissolved_oxygen_mg_l must be non-negative.");
    }
    if (flow_m3_s < 0.0) {
        throw std::invalid_argument("PumpTelemetry.flow_m3_s must be non-negative.");
    }
    if (head_m < 0.0) {
        throw std::invalid_argument("PumpTelemetry.head_m must be non-negative.");
    }
    if (energy_kwh < 0.0) {
        throw std::invalid_argument("PumpTelemetry.energy_kwh must be non-negative.");
    }

    ker.clamp_unit_interval();
    roh.clamp_unit_interval();
}

void WastewaterScreenTelemetry::validate_and_normalize() {
    if (influent_turbidity_ntu < 0.0) {
        throw std::invalid_argument("WastewaterScreenTelemetry.influent_turbidity_ntu must be non-negative.");
    }
    if (effluent_turbidity_ntu < 0.0) {
        throw std::invalid_argument("WastewaterScreenTelemetry.effluent_turbidity_ntu must be non-negative.");
    }
    if (solids_loading_kg_h < 0.0) {
        throw std::invalid_argument("WastewaterScreenTelemetry.solids_loading_kg_h must be non-negative.");
    }
    if (delta_head_m < 0.0) {
        throw std::invalid_argument("WastewaterScreenTelemetry.delta_head_m must be non-negative.");
    }
    if (energy_kwh < 0.0) {
        throw std::invalid_argument("WastewaterScreenTelemetry.energy_kwh must be non-negative.");
    }

    ker.clamp_unit_interval();
    roh.clamp_unit_interval();
}

/// Internal helper: convert an EcoNetPumpSnapshot into PumpWindowKerRoh.
/// This function is strictly non-actuating; it builds immutable corridor
/// status records and KER/RoH envelopes that higher-level Rust code can
/// use for start/stop gating and ecowealth scoring.[file:6][file:8]
static PumpWindowKerRoh from_snapshot(const EcoNetPumpSnapshot& snap) {
    PumpWindowKerRoh out;
    out.ker.k = clamp01(snap.k);
    out.ker.e = clamp01(snap.e);
    out.ker.r = clamp01(snap.r);
    out.ker.clamp_unit_interval();

    out.roh.roh_ceiling = clamp01(snap.roh);
    out.roh.clamp_unit_interval();

    out.corridor_status.within_corridor = snap.within_corridor;
    out.corridor_status.no_build        = snap.no_build;
    out.corridor_status.start_allowed   = snap.start_allowed;
    out.corridor_status.stop_allowed    = snap.stop_allowed;

    return out;
}

PumpWindowKerRoh compute_pump_window_ker_roh(
    const PumpTelemetry& window)
{
    PumpTelemetry tmp = window;
    tmp.validate_and_normalize();

    EcoNetPumpSnapshot snap =
        econet_compute_pump_snapshot(
            tmp.pump_id.c_str(),
            tmp.corridor_id.c_str(),
            tmp.turbidity_ntu,
            tmp.dissolved_oxygen_mg_l,
            tmp.flow_m3_s,
            tmp.head_m,
            tmp.energy_kwh);

    return from_snapshot(snap);
}

PumpWindowKerRoh compute_screen_window_ker_roh(
    const WastewaterScreenTelemetry& window)
{
    WastewaterScreenTelemetry tmp = window;
    tmp.validate_and_normalize();

    EcoNetPumpSnapshot snap =
        econet_compute_screen_snapshot(
            tmp.screen_id.c_str(),
            tmp.corridor_id.c_str(),
            tmp.influent_turbidity_ntu,
            tmp.effluent_turbidity_ntu,
            tmp.solids_loading_kg_h,
            tmp.delta_head_m,
            tmp.energy_kwh);

    return from_snapshot(snap);
}

} // namespace wastewater
} // namespace waste
} // namespace prometheus_praxis
