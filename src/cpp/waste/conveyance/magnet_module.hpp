// File: Prometheus-Praxis/src/cpp/waste/conveyance/magnet_module.hpp
// License: MIT OR Apache-2.0
//
// Non-actuating telemetry header for magnetic separation lanes.
// Describes MagnetNodeTelemetry and MagnetRoutingEnvelope with fields
// for ferrous capture rate, energy draw, eco-per-joule metrics, and
// blast-radius adjacency tags. These structs are designed as governance
// envelopes to feed EcoNet plane-weights and KER windows for magnet
// nodes, never motor control.[file:6][file:8]

#ifndef PROMETHEUS_PRAXIS_WASTE_CONVEYANCE_MAGNET_MODULE_HPP
#define PROMETHEUS_PRAXIS_WASTE_CONVEYANCE_MAGNET_MODULE_HPP

#include <cstddef>
#include <cstdint>
#include <string>

namespace prometheus_praxis {
namespace waste {
namespace conveyance {

/// Simple KER triad for magnet telemetry.
/// All values must be in [0.0, 1.0] and are telemetry-only.
/// k: knowledge factor (data quality, observability).
/// e: eco-impact (positive ecological benefit potential).
/// r: risk-of-harm (probability / severity of harm).
struct KerTriad {
    double k;
    double e;
    double r;

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

/// Risk-of-harm ceiling scalar for magnet nodes.
/// roh_ceiling: upper bound on acceptable risk-of-harm, [0,1].
struct RohCeiling {
    double roh_ceiling;

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

/// Telemetry for a magnet node over a window.
/// This struct is purely descriptive and feeds governance math.
/// No device IO or motor control is exposed.
struct MagnetNodeTelemetry {
    std::string node_id;              // magnet node identifier
    std::string lane_id;              // EcoNet lane identifier (research/pilot/prod)
    std::string blast_radius_plane_id;// non-offsettable plane id

    // Capture and stream metrics
    double ferrous_capture_rate;      // fraction of ferrous stream captured [0,1]
    double nonferrous_bypass_rate;    // fraction of non-ferrous bypass [0,1]
    double contamination_rate;        // fraction of unwanted material captured [0,1]

    // Energy and eco-per-joule metrics
    double energy_draw_w;             // instantaneous power draw [W] (>=0)
    double energy_per_tonne_kwh;      // energy per processed tonne [kWh/t] (>=0)
    double eco_per_joule;             // eco-benefit per Joule [0,1]

    // Local KER and RoH envelopes
    KerTriad  ker;                    // KER triad [0,1]
    RohCeiling roh;                   // RoH ceiling [0,1]

    MagnetNodeTelemetry()
        : ferrous_capture_rate(0.0),
          nonferrous_bypass_rate(0.0),
          contamination_rate(0.0),
          energy_draw_w(0.0),
          energy_per_tonne_kwh(0.0),
          eco_per_joule(0.0),
          ker(),
          roh() {}

    /// Validate invariants and clamp telemetry ranges.
    /// Throws std::invalid_argument on clearly invalid values.
    void validate_and_normalize();
};

/// Governance envelope used to route magnet telemetry into
/// EcoNet plane-weights and KER windows. This is a compact
/// diagnostic record that higher-level code can persist into
/// SQL, ALN, or Rust governance layers, without any actuator
/// decisions.
struct MagnetRoutingEnvelope {
    std::string node_id;              // magnet node identifier
    std::string lane_id;              // EcoNet lane identifier
    std::string blast_radius_plane_id;// plane id for adjacency

    // Aggregated eco and risk metrics
    double carbon_radius;             // carbon blast-radius contribution [0,1]
    double biodiversity_radius;       // biodiversity blast-radius contribution [0,1]
    double plane_weight;              // aggregate plane weight [0,1]

    KerTriad  ker;                    // KER triad [0,1]
    RohCeiling roh;                   // RoH ceiling [0,1]

    // Energy governance metrics
    double eco_per_joule;             // eco benefit per Joule [0,1]
    double energy_draw_w;             // instantaneous power [W]
    double energy_per_tonne_kwh;      // energy per tonne [kWh/t]

    MagnetRoutingEnvelope()
        : carbon_radius(0.0),
          biodiversity_radius(0.0),
          plane_weight(0.0),
          ker(),
          roh(),
          eco_per_joule(0.0),
          energy_draw_w(0.0),
          energy_per_tonne_kwh(0.0) {}

    /// Clamp all bounded metrics into [0,1] where applicable.
    void clamp_bounded_metrics();
};

} // namespace conveyance
} // namespace waste
} // namespace prometheus_praxis

#endif // PROMETHEUS_PRAXIS_WASTE_CONVEYANCE_MAGNET_MODULE_HPP
