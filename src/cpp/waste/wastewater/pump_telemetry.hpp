// File: Prometheus-Praxis/src/cpp/waste/wastewater/pump_telemetry.hpp
// License: MIT OR Apache-2.0
//
// Non-actuating telemetry and normalization header for wastewater pumps
// and screens. Defines PumpModelMetadata, PumpTelemetry (raw + normalized),
// WastewaterScreenTelemetry, KER/RoH structs, and corridor status flags.
// All functions are numeric/governance-only and never issue actuator commands.

#ifndef PROMETHEUS_PRAXIS_WASTE_WASTEWATER_PUMP_TELEMETRY_HPP
#define PROMETHEUS_PRAXIS_WASTE_WASTEWATER_PUMP_TELEMETRY_HPP

#include <cstddef>
#include <cstdint>
#include <string>
#include <stdexcept>
#include <cmath>

namespace prometheus_praxis {
namespace waste {
namespace wastewater {

inline double clamp01(double x) {
    if (x < 0.0) return 0.0;
    if (x > 1.0) return 1.0;
    return x;
}

// KER triad for pump/screen telemetry.
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
};

// Risk-of-harm ceiling scalar for pump windows.
struct RohCeiling {
    double roh_ceiling; // [0,1]

    RohCeiling() : roh_ceiling(0.0) {}
    explicit RohCeiling(double v) : roh_ceiling(clamp01(v)) {}

    void clamp_unit_interval() {
        roh_ceiling = clamp01(roh_ceiling);
    }
};

// Lyapunov corridor flags for pump operation windows.
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

// Model metadata for municipal and edge wastewater pumps.
enum class PumpBrand {
    XylemFlygt,
    Grundfos,
    KSB,
    Sulzer,
    Liberty,
    Zoeller,
    Unknown
};

enum class ImpellerType {
    SuperVortex,
    ChannelSingle,
    ChannelDouble,
    ChannelTriple,
    Grinder,
    Unknown
};

enum class InstallType {
    SubmersibleWet,
    DryPit,
    AutoCouplingSubmerged,
    Unknown
};

enum class PumpDesignFamily {
    CityLiftStation,
    WwtpInfluent,
    WwtpEffluent,
    BuildingEjector,
    IndustrialEffluent,
    Unknown
};

enum class PumpRunState {
    Stopped,
    Starting,
    Running,
    Stopping,
    Fault
};

struct PumpModelMetadata {
    std::string      model_id;
    PumpBrand        brand           {PumpBrand::Unknown};
    PumpDesignFamily design_family   {PumpDesignFamily::Unknown};

    double rated_flow_m3_h      {0.0};
    double rated_head_m         {0.0};
    double rated_power_kw       {0.0};

    double max_flow_m3_h        {0.0};
    double max_head_m           {0.0};
    double min_flow_fraction    {0.1};

    double solids_passage_mm    {0.0};
    ImpellerType impeller       {ImpellerType::Unknown};
    InstallType  install_type   {InstallType::Unknown};

    double npsh_req_m           {0.0};
    double efficiency_peak      {0.0};

    void validate() const {
        if (max_flow_m3_h <= 0.0) {
            throw std::invalid_argument("PumpModelMetadata: max_flow_m3_h must be positive.");
        }
        if (max_head_m <= 0.0) {
            throw std::invalid_argument("PumpModelMetadata: max_head_m must be positive.");
        }
        if (rated_power_kw < 0.0) {
            throw std::invalid_argument("PumpModelMetadata: rated_power_kw must be non-negative.");
        }
        if (min_flow_fraction <= 0.0 || min_flow_fraction > 1.0) {
            throw std::invalid_argument("PumpModelMetadata: min_flow_fraction must be in (0,1].");
        }
    }
};

// Raw pump telemetry for a single evidence window.
struct PumpTelemetry {
    std::string pump_id;
    std::string corridor_id;
    std::string window_id;
    std::string timestamp_utc;

    PumpRunState run_state    {PumpRunState::Stopped};
    int          fault_code   {0};

    double flow_m3_s          {0.0};
    double head_m             {0.0};
    double power_kw           {0.0};
    double energy_kwh         {0.0};

    double turbidity_ntu      {0.0};
    double dissolved_oxygen_mg_l {0.0};
    double bod_mg_l           {0.0};
    double tss_mg_l           {0.0};
    double cec_mmol_kg        {0.0};

    double fluid_temp_c       {0.0};
    double motor_temp_c       {0.0};
    double bearing_temp_c     {0.0};

    double npsh_available_m   {0.0};
    double vibration_rms_mm_s {0.0};

    long   starts_count       {0};
    double run_hours_total    {0.0};

    KerTriad   ker;
    RohCeiling roh;

    PumpTelemetry()
        : ker(), roh() {}

    void validate_and_normalize() {
        if (flow_m3_s < 0.0)      flow_m3_s = 0.0;
        if (head_m   < 0.0)       head_m   = 0.0;
        if (power_kw < 0.0)       power_kw = 0.0;
        if (energy_kwh < 0.0)     energy_kwh = 0.0;

        if (turbidity_ntu < 0.0)  turbidity_ntu = 0.0;
        if (dissolved_oxygen_mg_l < 0.0) dissolved_oxygen_mg_l = 0.0;
        if (bod_mg_l     < 0.0)   bod_mg_l     = 0.0;
        if (tss_mg_l     < 0.0)   tss_mg_l     = 0.0;
        if (cec_mmol_kg  < 0.0)   cec_mmol_kg  = 0.0;

        if (npsh_available_m < 0.0)    npsh_available_m    = 0.0;
        if (vibration_rms_mm_s < 0.0)  vibration_rms_mm_s  = 0.0;

        if (starts_count < 0)          starts_count        = 0;
        if (run_hours_total < 0.0)     run_hours_total     = 0.0;

        ker.clamp_unit_interval();
        roh.clamp_unit_interval();
    }
};

// Telemetry for a wastewater screen window.
struct WastewaterScreenTelemetry {
    std::string screen_id;
    std::string corridor_id;
    std::string window_id;
    std::string timestamp_utc;

    double influent_turbidity_ntu {0.0};
    double effluent_turbidity_ntu {0.0};
    double solids_loading_kg_h    {0.0};
    double delta_head_m           {0.0};
    double energy_kwh             {0.0};

    KerTriad   ker;
    RohCeiling roh;

    WastewaterScreenTelemetry()
        : ker(), roh() {}

    void validate_and_normalize() {
        if (influent_turbidity_ntu < 0.0) influent_turbidity_ntu = 0.0;
        if (effluent_turbidity_ntu < 0.0) effluent_turbidity_ntu = 0.0;
        if (solids_loading_kg_h < 0.0)    solids_loading_kg_h    = 0.0;
        if (delta_head_m < 0.0)          delta_head_m           = 0.0;
        if (energy_kwh < 0.0)            energy_kwh             = 0.0;

        ker.clamp_unit_interval();
        roh.clamp_unit_interval();
    }
};

// Normalized coordinates derived from telemetry and metadata.
struct PumpNormalizedCoordinates {
    double r_hydraulics {0.0};
    double r_energy     {0.0};
    double r_uncertainty{0.0};
    double r_reliability{0.0};
};

// Hydraulic normalization based on relative flow/head.
inline double normalize_hydraulics(const PumpTelemetry& t,
                                   const PumpModelMetadata& meta)
{
    meta.validate();

    const double flow_m3_h = t.flow_m3_s * 3600.0;
    const double q_rel = (meta.max_flow_m3_h > 0.0)
        ? flow_m3_h / meta.max_flow_m3_h
        : 1.0;

    const double h_rel = (meta.max_head_m > 0.0)
        ? t.head_m / meta.max_head_m
        : 1.0;

    double q_clamped = clamp01(q_rel);
    double h_clamped = clamp01(h_rel);

    if (q_clamped < meta.min_flow_fraction) {
        const double deficit = meta.min_flow_fraction - q_clamped;
        q_clamped = meta.min_flow_fraction + deficit;
    }

    const double w_q = 0.5;
    const double w_h = 0.5;
    const double risk = clamp01(w_q * q_clamped + w_h * h_clamped);
    return risk;
}

// Hydraulic efficiency using P_h = rho * g * Q * H.
inline double compute_efficiency(double flow_m3_s,
                                 double head_m,
                                 double power_kw)
{
    const double rho = 1000.0;
    const double g   = 9.81;
    const double p_electrical_w = power_kw * 1000.0;

    if (flow_m3_s <= 0.0 || head_m <= 0.0 || p_electrical_w <= 0.0) {
        return 0.0;
    }

    const double p_hydraulic_w = rho * g * flow_m3_s * head_m;
    const double eff = p_hydraulic_w / p_electrical_w;
    return clamp01(eff);
}

// Available NPSH using NPSH_avail = h_atm + h_static - h_vapour - h_losses.
inline double compute_npsh_available(double atm_head_m,
                                     double static_head_m,
                                     double vapour_head_m,
                                     double loss_head_m)
{
    double npsh = atm_head_m + static_head_m - vapour_head_m - loss_head_m;
    if (npsh < 0.0) npsh = 0.0;
    return npsh;
}

// Energy normalization relative to rated power and carbon intensity.
inline double normalize_energy(const PumpTelemetry& t,
                               const PumpModelMetadata& meta,
                               double carbon_intensity_kg_co2_per_kwh)
{
    meta.validate();

    double p_rel = 1.0;
    if (meta.rated_power_kw > 0.0) {
        p_rel = t.power_kw / meta.rated_power_kw;
    }

    double e_rel = 1.0;
    if (t.energy_kwh > 0.0 && meta.rated_power_kw > 0.0) {
        const double rated_window_kwh = meta.rated_power_kw;
        e_rel = t.energy_kwh / rated_window_kwh;
    }

    double c_rel = 0.0;
    const double corridor_max = 0.8;
    if (corridor_max > 0.0) {
        c_rel = carbon_intensity_kg_co2_per_kwh / corridor_max;
    }

    double p_clamped = clamp01(p_rel);
    double e_clamped = clamp01(e_rel);
    double c_clamped = clamp01(c_rel);

    const double w_p = 0.4;
    const double w_e = 0.3;
    const double w_c = 0.3;

    double risk = w_p * p_clamped + w_e * e_clamped + w_c * c_clamped;
    return clamp01(risk);
}

// Uncertainty normalization from vibration, temperature, and faults.
inline double normalize_uncertainty(const PumpTelemetry& t,
                                    const PumpModelMetadata& /*meta*/)
{
    double vib_rel = 0.0;
    const double vib_corridor_max = 7.0;
    if (vib_corridor_max > 0.0) {
        vib_rel = t.vibration_rms_mm_s / vib_corridor_max;
    }

    double temp_rel = 0.0;
    const double temp_corridor_max = 80.0;
    double temp_max = std::max(t.motor_temp_c, t.bearing_temp_c);
    if (temp_corridor_max > 0.0) {
        temp_rel = temp_max / temp_corridor_max;
    }

    double fault_rel = (t.fault_code != 0) ? 1.0 : 0.0;

    double vib_clamped   = clamp01(vib_rel);
    double temp_clamped  = clamp01(temp_rel);
    double fault_clamped = clamp01(fault_rel);

    const double w_v = 0.4;
    const double w_t = 0.4;
    const double w_f = 0.2;

    double risk = w_v * vib_clamped + w_t * temp_clamped + w_f * fault_clamped;
    return clamp01(risk);
}

// Compute normalized coordinates for corridor/KER use.
inline PumpNormalizedCoordinates compute_normalized_coordinates(
    const PumpTelemetry& raw,
    const PumpModelMetadata& meta,
    double carbon_intensity_kg_co2_per_kwh)
{
    PumpTelemetry t = raw;
    t.validate_and_normalize();

    PumpNormalizedCoordinates coords;
    coords.r_hydraulics = normalize_hydraulics(t, meta);
    coords.r_energy     = normalize_energy(t, meta, carbon_intensity_kg_co2_per_kwh);
    coords.r_uncertainty= normalize_uncertainty(t, meta);

    double starts_rel = 0.0;
    const double starts_corridor = 12.0;
    if (starts_corridor > 0.0) {
        starts_rel = static_cast<double>(t.starts_count) / starts_corridor;
    }

    double hours_rel = 0.0;
    const double hours_corridor = 24.0;
    if (hours_corridor > 0.0) {
        hours_rel = t.run_hours_total / hours_corridor;
    }

    double s_clamped = clamp01(starts_rel);
    double h_clamped = clamp01(hours_rel);

    const double w_u = 0.4;
    const double w_s = 0.3;
    const double w_h = 0.3;

    coords.r_reliability = clamp01(w_u * coords.r_uncertainty + w_s * s_clamped + w_h * h_clamped);
    return coords;
}

// Expected KER/RoH and corridor status for windows, via governance FFI.
struct PumpWindowKerRoh {
    KerTriad          ker;
    RohCeiling        roh;
    PumpCorridorStatus corridor_status;

    PumpWindowKerRoh() : ker(), roh(), corridor_status() {}
};

// Pure helper signatures to be implemented in a corresponding .cpp file.
PumpWindowKerRoh compute_pump_window_ker_roh(const PumpTelemetry& window);
PumpWindowKerRoh compute_screen_window_ker_roh(const WastewaterScreenTelemetry& window);

} // namespace wastewater
} // namespace waste
} // namespace prometheus_praxis

#endif // PROMETHEUS_PRAXIS_WASTE_WASTEWATER_PUMP_TELEMETRY_HPP
