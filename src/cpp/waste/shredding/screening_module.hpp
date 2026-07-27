// filename: Prometheus-Praxis/src/cpp/waste/shredding/screening_module.hpp
// destination: github.com/mk-bluebird/Prometheus-Praxis/src/cpp/waste/shredding/screening_module.hpp
// license: MIT OR Apache-2.0
//
// Role:
// Header for drum screen and air classifier telemetry in the shredding band.
// Responsibilities:
// - Define POD telemetry structs for drum screen and air classifier.
// - Define screen geometry descriptors and particle size bands.
// - Attach RoH (Risk-of-Harm) and Lyapunov corridor tags per screen lane.
// - Expose only telemetry transforms and corridor checks (no actuation).

#ifndef PROMETHEUS_PRAXIS_WASTE_SHREDDING_SCREENING_MODULE_HPP
#define PROMETHEUS_PRAXIS_WASTE_SHREDDING_SCREENING_MODULE_HPP

#include <cstddef>
#include <cstdint>

namespace prometheus_praxis {
namespace waste {
namespace shredding {

// ---------------------------------------------------------------------------
// Particle size bands and screen geometry descriptors
// ---------------------------------------------------------------------------

// Simple size band classification in millimetres.
// Bands are non-overlapping and collectively cover the expected shredding range.
//
// fine:    0   .. 10 mm
// medium:  10  .. 50 mm
// coarse:  50  .. 150 mm
// oversize: >  150 mm
struct ParticleSizeBands
{
    double fine_max_mm;
    double medium_max_mm;
    double coarse_max_mm;
    double oversize_min_mm;
};

// Drum screen geometry descriptor.
// This describes the physical properties of the rotating drum screen
// without any actuation hooks.
struct ScreenDrumGeometry
{
    double length_m;        // drum length [m]
    double diameter_m;      // drum diameter [m]
    double tilt_deg;        // drum tilt angle [degrees]
    double open_area_frac;  // fraction of open area [0,1]
    std::uint32_t lanes;    // number of lanes / outlets
};

// Air classifier geometry descriptor.
// Captures basic duct and cut-point configuration.
struct AirClassifierGeometry
{
    double duct_length_m;       // duct length [m]
    double duct_diameter_m;     // duct diameter [m]
    double cut_point_mm;        // nominal aerodynamic cut-point [mm equivalent]
    double fan_power_kw;        // fan power [kW], for telemetry consistency only
    std::uint32_t lanes;        // number of classifier lanes
};

// ---------------------------------------------------------------------------
// RoH / Lyapunov corridor tags for screen lanes
// ---------------------------------------------------------------------------
//
// Each lane is tagged with:
// - roh_corridor_tag: qualitative risk-of-harm corridor classification.
// - vt_corridor_tag: Lyapunov residual corridor classification.
//
// These tags are used for telemetry-only checks; they do not drive hardware.

enum class CorridorTag : std::uint8_t
{
    RESEARCH = 0,
    PILOT    = 1,
    PRODUCTION = 2,
    BLOCKED  = 3
};

// Lane-level corridor configuration.
struct ScreenLaneCorridor
{
    std::uint32_t lane_index;   // 0-based lane index
    CorridorTag roh_corridor_tag;
    CorridorTag vt_corridor_tag;

    // Thresholds for telemetry-only checks.
    double max_roh_score;       // maximum acceptable RoH ceiling [0,1]
    double max_vt_residual;     // maximum acceptable Lyapunov residual
};

// ---------------------------------------------------------------------------
// Telemetry structs
// ---------------------------------------------------------------------------
//
// ScreenDrumTelemetry: per-interval summary of drum screen behavior.
// AirClassifierTelemetry: per-interval summary of air classifier behavior.
// Both are intentionally POD and non-actuating.

struct ScreenDrumTelemetry
{
    double feed_rate_kg_per_h;        // feed rate to drum [kg/h]
    double rotational_speed_rpm;      // drum speed [rpm]
    double inclination_deg;           // actual tilt [degrees]
    double occupancy_frac;            // occupancy fraction [0,1]
    double vt_residual;               // Lyapunov residual slice for drum [>=0]
    double roh_score;                 // RoH kernel ceiling for drum [0,1]
    double avg_particle_size_mm;      // average particle size at discharge [mm]
    double fines_fraction;            // fraction in fine band [0,1]
    double overs_fraction;            // fraction in oversize band [0,1]
};

struct AirClassifierTelemetry
{
    double feed_rate_kg_per_h;        // feed rate to classifier [kg/h]
    double air_velocity_m_per_s;      // air velocity [m/s]
    double pressure_drop_pa;          // pressure drop [Pa]
    double vt_residual;               // Lyapunov residual slice for classifier [>=0]
    double roh_score;                 // RoH kernel ceiling for classifier [0,1]
    double cut_efficiency;            // fraction correctly classified around cut-point [0,1]
    double fines_fraction;            // fraction to fines lane [0,1]
    double heavies_fraction;          // fraction to heavies lane [0,1]
};

// ---------------------------------------------------------------------------
// Telemetry transforms
// ---------------------------------------------------------------------------
//
// These helpers operate on telemetry and geometry only, and are pure functions.
// They provide normalized band fractions and basic RoH/Lyapunov safety checks.
// No hardware commands are issued.

// Classify average particle size into bands using the size descriptor.
inline CorridorTag classify_particle_lane(
    double avg_particle_size_mm,
    const ParticleSizeBands &bands
)
{
    if (avg_particle_size_mm <= bands.fine_max_mm) {
        return CorridorTag::RESEARCH;     // fine band routed to research/optimization lanes
    }
    if (avg_particle_size_mm <= bands.medium_max_mm) {
        return CorridorTag::PILOT;        // medium routed to pilot lanes
    }
    if (avg_particle_size_mm <= bands.coarse_max_mm) {
        return CorridorTag::PRODUCTION;   // coarse routed to production lanes
    }
    // oversize particles are treated as blocked until geometry or upstream shredding is corrected.
    return CorridorTag::BLOCKED;
}

// Normalize drum occupancy into a RoH hint (non-actuating).
// Simple mapping: low occupancy => low RoH, overload => higher RoH.
inline double drum_occupancy_roh_hint(double occupancy_frac)
{
    if (occupancy_frac <= 0.0) {
        return 0.0;
    }
    if (occupancy_frac >= 1.0) {
        return 1.0;
    }
    // A convex mapping emphasizing overload.
    double x = occupancy_frac;
    double roh = x * x;
    if (roh < 0.0) roh = 0.0;
    if (roh > 1.0) roh = 1.0;
    return roh;
}

// Air velocity-based RoH hint.
// Low velocity (poor separation) or very high velocity (erosion risk) both increase RoH.
inline double air_velocity_roh_hint(double air_velocity_m_per_s)
{
    // Nominal safe band: 10 .. 25 m/s.
    const double VSAFE_MIN = 10.0;
    const double VSAFE_MAX = 25.0;

    if (air_velocity_m_per_s <= 0.0) {
        return 0.0;
    }

    if (air_velocity_m_per_s >= VSAFE_MIN && air_velocity_m_per_s <= VSAFE_MAX) {
        // Inside nominal band: low RoH.
        return 0.1;
    }

    // Outside safe band: normalized risk up to 1.0.
    double delta = 0.0;
    if (air_velocity_m_per_s < VSAFE_MIN) {
        delta = VSAFE_MIN - air_velocity_m_per_s;
    } else {
        delta = air_velocity_m_per_s - VSAFE_MAX;
    }

    // Scale by 20 m/s span.
    double roh = delta / 20.0;
    if (roh < 0.0) roh = 0.0;
    if (roh > 1.0) roh = 1.0;
    return roh;
}

// ---------------------------------------------------------------------------
// Corridor checks (RoH / Lyapunov) for lanes
// ---------------------------------------------------------------------------
//
// These functions check whether a telemetry slice fits inside the configured
// RoH and Lyapunov corridors for a given lane. They are side-effect free and
// can be used by higher layers (Rust, Java, SQL) to gate actuation logic.

inline bool check_screen_lane_corridor(
    const ScreenLaneCorridor &lane,
    const ScreenDrumTelemetry &telemetry
)
{
    // RoH and residual must be within configured ceilings.
    if (telemetry.roh_score > lane.max_roh_score) {
        return false;
    }
    if (telemetry.vt_residual > lane.max_vt_residual) {
        return false;
    }
    // Non-negative residual is assumed by Lyapunov design.
    if (telemetry.vt_residual < 0.0) {
        return false;
    }
    // RoH within [0,1].
    if (telemetry.roh_score < 0.0 || telemetry.roh_score > 1.0) {
        return false;
    }
    return true;
}

inline bool check_classifier_lane_corridor(
    const ScreenLaneCorridor &lane,
    const AirClassifierTelemetry &telemetry
)
{
    if (telemetry.roh_score > lane.max_roh_score) {
        return false;
    }
    if (telemetry.vt_residual > lane.max_vt_residual) {
        return false;
    }
    if (telemetry.vt_residual < 0.0) {
        return false;
    }
    if (telemetry.roh_score < 0.0 || telemetry.roh_score > 1.0) {
        return false;
    }
    return true;
}

} // namespace shredding
} // namespace waste
} // namespace prometheus_praxis

#endif // PROMETHEUS_PRAXIS_WASTE_SHREDDING_SCREENING_MODULE_HPP
