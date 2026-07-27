// File: src/cpp/waste/wastewater/sump_station.hpp
// License: MIT OR Apache-2.0
//
// Sump and station metadata + telemetry + corridor helpers,
// derived from Flygt/Xylem pump station design manuals and
// common municipal practice. Non-actuating, numeric-only.[web:109][web:110][web:112][web:114][web:115][web:116]

#pragma once

#include <string>
#include <stdexcept>
#include <cmath>

#include "pump_telemetry.hpp"  // existing header with PumpTelemetry, PumpModelMetadata, PumpNormalizedCoordinates.[file:8]

namespace prometheus_praxis {
namespace waste {
namespace wastewater {

enum class SumpType {
    CircularWetWell,
    RectangularWetWell,
    DryWellWithSuction,
    Unknown
};

struct SumpDesignMetadata {
    std::string sump_id;
    SumpType    sump_type      {SumpType::Unknown};

    double      pipe_diam_m    {0.0};  // inlet pipe diameter
    double      min_submergence_m {0.0}; // vendor/design minimum submergence[web:110]
    double      floor_slope_ratio {0.0}; // e.g., 0.05 means 5 % slope toward pump[web:109][web:110]
    bool        has_benching      {false}; // fillets/benching at walls to avoid stagnant zones[web:109][web:110]
    bool        has_scum_management{false}; // scum board, skimming, periodic low-level flushing[web:109][web:110][web:115][web:116]

    double      design_min_velocity_m_s {0.0}; // minimum velocity for solids transport[web:109][web:110]
    double      design_max_velocity_m_s {0.0}; // max to avoid erosion/air entrainment[web:109][web:110]

    int         elbow_count          {0};    // suction/inlet elbows
    int         elbow_plane_count    {0};    // distinct planes with elbows[web:115]
    double      straight_run_before_pump_diam {0.0}; // straight length in pipe diameters before pump[web:110][web:115]

    void validate() const {
        if (pipe_diam_m <= 0.0) {
            throw std::invalid_argument("SumpDesignMetadata: pipe_diam_m must be > 0.");
        }
        if (design_min_velocity_m_s < 0.0 || design_max_velocity_m_s <= 0.0) {
            throw std::invalid_argument("SumpDesignMetadata: invalid velocity design range.");
        }
    }
};

struct SumpTelemetry {
    std::string sump_id;
    std::string station_id;

    double      water_level_m        {0.0};
    double      inlet_velocity_m_s   {0.0};
    double      inlet_submergence_m  {0.0}; // from water level vs inlet centreline[web:110]
    double      observed_scum_area_m2{0.0}; // rough indicator from inspections/sensors
    double      observed_sediment_volume_m3 {0.0};
    double      time_since_last_flush_hours {0.0};

    void sanitize() {
        if (water_level_m < 0.0)        water_level_m = 0.0;
        if (inlet_velocity_m_s < 0.0)   inlet_velocity_m_s = 0.0;
        if (inlet_submergence_m < 0.0)  inlet_submergence_m = 0.0;
        if (observed_scum_area_m2 < 0.0) observed_scum_area_m2 = 0.0;
        if (observed_sediment_volume_m3 < 0.0) observed_sediment_volume_m3 = 0.0;
        if (time_since_last_flush_hours < 0.0) time_since_last_flush_hours = 0.0;
    }
};

struct StationTelemetry {
    std::string station_id;

    double      inflow_m3_s      {0.0};
    double      outflow_m3_s     {0.0};
    double      wetwell_volume_m3{0.0};
    double      level_m          {0.0};

    int         active_pump_count{0};
    int         installed_pump_count{0};

    void sanitize() {
        if (inflow_m3_s   < 0.0) inflow_m3_s   = 0.0;
        if (outflow_m3_s  < 0.0) outflow_m3_s  = 0.0;
        if (wetwell_volume_m3 < 0.0) wetwell_volume_m3 = 0.0;
        if (level_m       < 0.0) level_m       = 0.0;
        if (active_pump_count < 0)   active_pump_count = 0;
        if (installed_pump_count < 0) installed_pump_count = 0;
    }
};

struct SumpStationRisk {
    double r_submergence {0.0};
    double r_sediment    {0.0};
    double r_scum        {0.0};
    double r_inlet_geom  {0.0}; // elbows, straight run, benching
    double r_balance     {0.0}; // inflow vs outflow
};

inline double clamp01(double x) {
    if (x < 0.0) return 0.0;
    if (x > 1.0) return 1.0;
    return x;
}

/// Compute minimum required submergence S for a given
/// Froude number and pipe diameter using typical Flygt
/// guidance S ≈ 1.7 * Fr * D, bounded to >= D.[web:110]
inline double compute_submergence_requirement(double froude_number,
                                              double pipe_diam_m)
{
    if (pipe_diam_m <= 0.0) {
        throw std::invalid_argument("compute_submergence_requirement: pipe_diam_m must be > 0.");
    }

    const double S_raw = 1.7 * froude_number * pipe_diam_m;
    const double S_min = pipe_diam_m;
    double S = std::max(S_raw, S_min);
    if (S < 0.0) S = 0.0;
    return S;
}

/// Normalize submergence risk based on design requirement.
/// r_submergence ≈ 0 when actual submergence >= required, rising
/// toward 1 when below requirement.[web:110]
inline double normalize_submergence_risk(double inlet_submergence_m,
                                         double required_submergence_m)
{
    if (required_submergence_m <= 0.0) {
        return 0.0;
    }

    if (inlet_submergence_m >= required_submergence_m) {
        return 0.0;
    }

    const double deficit = required_submergence_m - inlet_submergence_m;
    const double risk = deficit / required_submergence_m;
    return clamp01(risk);
}

/// Compute sump sediment risk based on observed sediment volume,
/// design velocities, and time since last flush. High sediment
/// plus low velocities and long intervals yield higher risk.[web:109][web:110][web:115][web:116]
inline double compute_sump_sediment_risk(const SumpTelemetry& raw,
                                         const SumpDesignMetadata& meta)
{
    meta.validate();
    SumpTelemetry t = raw;
    t.sanitize();

    // Velocity risk: below design_min_velocity_m_s increases sediment risk.
    double v_rel = 0.0;
    if (meta.design_min_velocity_m_s > 0.0) {
        v_rel = t.inlet_velocity_m_s / meta.design_min_velocity_m_s;
        if (v_rel >= 1.0) {
            v_rel = 0.0; // good velocities -> no added sediment risk
        } else {
            v_rel = clamp01(1.0 - v_rel); // lower velocities => higher risk
        }
    }

    // Sediment volume risk relative to a nominal corridor volume
    // (e.g., 1 % of wetwell volume).
    double vol_rel = 0.0;
    const double corridor_fraction = 0.01;
    const double corridor_volume = corridor_fraction * std::max(t.water_level_m * meta.pipe_diam_m, 1.0);
    if (corridor_volume > 0.0) {
        vol_rel = t.observed_sediment_volume_m3 / corridor_volume;
    }

    // Flush interval risk: longer than design flush interval => higher risk.
    double flush_rel = 0.0;
    const double design_flush_hours = 168.0; // weekly flush example[web:109][web:110][web:115][web:116]
    if (design_flush_hours > 0.0) {
        flush_rel = t.time_since_last_flush_hours / design_flush_hours;
    }

    double v_risk    = clamp01(v_rel);
    double vol_risk  = clamp01(vol_rel);
    double flush_risk= clamp01(flush_rel);

    const double w_v   = 0.4;
    const double w_vol = 0.3;
    const double w_f   = 0.3;

    double risk = w_v * v_risk + w_vol * vol_risk + w_f * flush_risk;

    // Floor slope and benching reduce risk slightly.
    if (meta.floor_slope_ratio > 0.02) { // slope ≥ 2 %
        risk *= 0.8;
    }
    if (meta.has_benching) {
        risk *= 0.9;
    }

    return clamp01(risk);
}

/// Compute scum risk based on observed scum area and presence
/// of scum management methods (boards, skimming, periodic flushing).[web:109][web:110][web:115][web:116]
inline double compute_sump_scum_risk(const SumpTelemetry& raw,
                                     const SumpDesignMetadata& meta)
{
    SumpTelemetry t = raw;
    t.sanitize();

    double area_rel = 0.0;
    const double corridor_scum_area = 1.0; // m² nominal safe scum area
    if (corridor_scum_area > 0.0) {
        area_rel = t.observed_scum_area_m2 / corridor_scum_area;
    }

    double risk = clamp01(area_rel);
    if (meta.has_scum_management) {
        risk *= 0.7; // mitigation in place
    }
    return clamp01(risk);
}

/// Normalize inlet geometry risk from elbows, planes, and straight
/// run before pump. Elbows in multiple planes and insufficient
/// straight run increase risk.[web:110][web:115][web:116]
inline double compute_inlet_geometry_risk(const SumpDesignMetadata& meta)
{
    meta.validate();

    double elbow_risk = 0.0;
    if (meta.elbow_count > 0) {
        elbow_risk = clamp01(static_cast<double>(meta.elbow_count) / 3.0);
    }

    double plane_risk = 0.0;
    if (meta.elbow_plane_count > 1) {
        plane_risk = clamp01(static_cast<double>(meta.elbow_plane_count - 1) / 3.0);
    }

    // Straight run risk: < 5 diameters considered higher risk.[web:110][web:115]
    double straight_risk = 0.0;
    if (meta.straight_run_before_pump_diam < 5.0) {
        straight_risk = clamp01((5.0 - meta.straight_run_before_pump_diam) / 5.0);
    }

    const double w_elbow   = 0.4;
    const double w_plane   = 0.3;
    const double w_straight= 0.3;

    double risk = w_elbow * elbow_risk + w_plane * plane_risk + w_straight * straight_risk;
    return clamp01(risk);
}

/// Station balance risk: difference between inflow and outflow
/// relative to wetwell capacity. High risk when inflow exceeds
/// outflow over the window and threatens flooding.[web:110][web:111]
inline double check_station_balance(const StationTelemetry& raw)
{
    StationTelemetry s = raw;
    s.sanitize();

    if (s.wetwell_volume_m3 <= 0.0) {
        return 0.0;
    }

    const double delta_q = s.inflow_m3_s - s.outflow_m3_s;
    if (delta_q <= 0.0) {
        return 0.0; // outflow >= inflow => no flooding risk from balance
    }

    // Rough risk proxy: how quickly the wetwell would fill given delta_q.
    // t_fill = wetwell_volume / delta_q; risk increases when t_fill is short.
    const double t_fill_s = s.wetwell_volume_m3 / delta_q;
    const double corridor_t_fill_min_s = 3600.0; // 1 hour buffer example[web:110][web:111]

    double risk = 0.0;
    if (corridor_t_fill_min_s > 0.0) {
        risk = clamp01((corridor_t_fill_min_s - t_fill_s) / corridor_t_fill_min_s);
    }
    return risk;
}

/// Aggregate sump and station risks into a SumpStationRisk object.
/// Can be consumed by your KER/ROH kernels and PumpAccountabilityRecord.[file:8]
inline SumpStationRisk compute_sump_station_risk(const SumpTelemetry& sump_raw,
                                                 const SumpDesignMetadata& meta,
                                                 const StationTelemetry& station_raw,
                                                 double froude_number)
{
    meta.validate();

    SumpTelemetry sump = sump_raw;
    sump.sanitize();
    StationTelemetry station = station_raw;
    station.sanitize();

    const double required_submergence_m = compute_submergence_requirement(froude_number,
                                                                          meta.pipe_diam_m);
    const double r_sub = normalize_submergence_risk(sump.inlet_submergence_m,
                                                    required_submergence_m);

    const double r_sed = compute_sump_sediment_risk(sump, meta);
    const double r_scum= compute_sump_scum_risk(sump, meta);
    const double r_geom= compute_inlet_geometry_risk(meta);
    const double r_bal = check_station_balance(station);

    SumpStationRisk risk;
    risk.r_submergence = r_sub;
    risk.r_sediment    = r_sed;
    risk.r_scum        = r_scum;
    risk.r_inlet_geom  = r_geom;
    risk.r_balance     = r_bal;
    return risk;
}

/// Example wiring: blend sump/station risk into pump-level normalized
/// coordinates, so your PumpAccountabilityRecord sees hydraulic risk
/// that respects both pump curves (PumpModelMetadata) and station
/// geometry (SumpDesignMetadata).[file:8][web:109][web:110][web:115][web:116]
inline PumpNormalizedCoordinates blend_pump_and_sump_station_risk(
    const PumpNormalizedCoordinates& pump_coords,
    const SumpStationRisk& sump_station_risk)
{
    PumpNormalizedCoordinates out = pump_coords;

    // Submergence, sediment, scum, inlet geometry and balance
    // all push hydraulics and uncertainty upward.
    const double w_sub = 0.3;
    const double w_sed = 0.2;
    const double w_scum= 0.1;
    const double w_geom= 0.2;
    const double w_bal = 0.2;

    const double extra_hydro =
        w_sub * sump_station_risk.r_submergence +
        w_sed * sump_station_risk.r_sediment +
        w_geom * sump_station_risk.r_inlet_geom +
        w_bal * sump_station_risk.r_balance;

    const double extra_uncert =
        w_scum * sump_station_risk.r_scum;

    out.r_hydraulics = clamp01(out.r_hydraulics + extra_hydro);
    out.r_uncertainty= clamp01(out.r_uncertainty + extra_uncert);

    // Reliability can also reflect persistent sediment/scum and poor balance.
    const double extra_rel =
        0.3 * sump_station_risk.r_sediment +
        0.3 * sump_station_risk.r_scum +
        0.4 * sump_station_risk.r_balance;

    out.r_reliability = clamp01(out.r_reliability + extra_rel);
    return out;
}

} // namespace wastewater
} // namespace waste
} // namespace prometheus_praxis
