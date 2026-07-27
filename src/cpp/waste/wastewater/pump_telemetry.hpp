// File: Prometheus-Praxis/src/cpp/waste/wastewater/pump_telemetry.hpp
// License: MIT OR Apache-2.0
//
// Non-actuating telemetry header for wastewater pumps and screens.
// Defines PumpTelemetry, WastewaterScreenTelemetry, and PumpCorridorStatus
// with turbidity, dissolved oxygen (DO), flow, head, energy, and Lyapunov
// corridor flags. Includes pure helper signatures for computing expected
// K/E/R and RoH for a pump window, to be populated via EcoNet governance
// spine calls (Rust/SQLite), not direct control.[file:6][file:8][web:54][web:59]

#ifndef PROMETHEUS_PRAXIS_WASTE_WASTEWATER_PUMP_TELEMETRY_HPP
#define PROMETHEUS_PRAXIS_WASTE_WASTEWATER_PUMP_TELEMETRY_HPP

#include <cstddef>
#include <cstdint>
#include <string>

namespace prometheus_praxis {
namespace waste {
namespace wastewater {

/// Simple KER triad for pump/screen telemetry.
struct KerTriad {
    double k; // knowledge / observability [0,1]
    double e; // eco-impact [0,1]
    double r; // risk-of-harm [0,1]

    KerTriad() : k(0.0), e(0.0), r(0.0) {}
    KerTriad(double k_, double e_, double r_) : k(k_), e(e_), r(r_) {}

    void clamp_unit_interval() {
        k = clamp01(k);
        e = clamp01(e);
        r = clamp01(r);
    }

    double kerscore() const {
        const double ck = clamp01(k);
        const double ce = clamp01(e);
        const double cr = clamp01(r);
        return ck * ce - cr;
    }

private:
    static double clamp01(double x) {
        if (x < 0.0) return 0.0;
        if (x > 1.0) return 1.0;
        return x;
    }
};

/// Risk-of-harm ceiling scalar for pump windows.
struct RohCeiling {
    double roh_ceiling; // [0,1]

    RohCeiling() : roh_ceiling(0.0) {}
    explicit RohCeiling(double v) : roh_ceiling(v) {
        roh_ceiling = clamp01(roh_ceiling);
    }

    void clamp_unit_interval() {
        roh_ceiling = clamp01(roh_ceiling);
    }

private:
    static double clamp01(double x) {
        if (x < 0.0) return 0.0;
        if (x > 1.0) return 1.0;
        return x;
    }
};

/// Lyapunov corridor flags for pump operation windows.
/// These are governance-only flags and never actuate pumps.
/// - within_corridor: true if V_t stays within allowed bounds.
/// - no_build: true if corridor prohibits new infrastructure.
/// - start_allowed / stop_allowed: governance decisions only.
struct PumpCorridorStatus {
    bool within_corridor;
    bool no_build;
    bool start_allowed;
    bool stop_allowed;

    PumpCorridorStatus()
        : within_corridor(false),
          no_build(false),
          start_allowed(false),
          stop_allowed(false) {}
};

/// Telemetry for a pump window (e.g., 1–15 minutes).
/// All units follow common wastewater monitoring practice:
/// - turbidity: NTU
/// - DO: mg/L
/// - flow: m^3/s
/// - head: m
/// - energy: kWh over window.[web:54][web:55][web:58][web:59]
struct PumpTelemetry {
    std::string pump_id;
    std::string corridor_id;

    double turbidity_ntu;       // [0, 1000+] NTU
    double dissolved_oxygen_mg_l; // DO [mg/L]
    double flow_m3_s;           // hydraulic flow [m^3/s]
    double head_m;              // pump head [m]
    double energy_kwh;          // energy consumed in window [kWh]

    KerTriad  ker;
    RohCeiling roh;

    PumpTelemetry()
        : turbidity_ntu(0.0),
          dissolved_oxygen_mg_l(0.0),
          flow_m3_s(0.0),
          head_m(0.0),
          energy_kwh(0.0),
          ker(),
          roh() {}

    /// Validate invariants and clamp bounded values where applicable.
    /// Throws std::invalid_argument for clearly invalid inputs.
    void validate_and_normalize();
};

/// Telemetry for a wastewater screen window (coarse/fine screens).
/// Includes solids loading and differential head across the screen.
/// All fields are descriptive and feed governance math only.
struct WastewaterScreenTelemetry {
    std::string screen_id;
    std::string corridor_id;

    double influent_turbidity_ntu;
    double effluent_turbidity_ntu;
    double solids_loading_kg_h; // screen solids loading [kg/h]
    double delta_head_m;        // head loss across screen [m]
    double energy_kwh;         // energy over window [kWh]

    KerTriad  ker;
    RohCeiling roh;

    WastewaterScreenTelemetry()
        : influent_turbidity_ntu(0.0),
          effluent_turbidity_ntu(0.0),
          solids_loading_kg_h(0.0),
          delta_head_m(0.0),
          energy_kwh(0.0),
          ker(),
          roh() {}

    void validate_and_normalize();
};

/// Expected KER and RoH for a pump window, derived from
/// EcoNet governance spine (Rust/SQLite). This struct is
/// returned by pure helpers that call into Rust FFI but
/// perform no actuation.
struct PumpWindowKerRoh {
    KerTriad  ker;
    RohCeiling roh;
    PumpCorridorStatus corridor_status;

    PumpWindowKerRoh() : ker(), roh(), corridor_status() {}
};

/// Pure helper signature: compute expected KER/RoH and corridor
/// flags for a given PumpTelemetry window. Implementation will
/// call EcoNet governance spine via Rust FFI, but this header
/// remains non-actuating.
PumpWindowKerRoh compute_pump_window_ker_roh(
    const PumpTelemetry& window);

/// Pure helper signature: compute expected KER/RoH and corridor
/// flags for a screen telemetry window, using EcoNet governance
/// spine via Rust FFI.
PumpWindowKerRoh compute_screen_window_ker_roh(
    const WastewaterScreenTelemetry& window);

} // namespace wastewater
} // namespace waste
} // namespace prometheus_praxis

#endif // PROMETHEUS_PRAXIS_WASTE_WASTEWATER_PUMP_TELEMETRY_HPP
