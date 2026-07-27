// filename: Prometheus-Praxis/src/cpp/waste/shredding/shredder_controller.hpp
// destination: github.com/mk-bluebird/Prometheus-Praxis/src/cpp/waste/shredding/shredder_controller.hpp
// license: MIT OR Apache-2.0
//
// Non-actuating public SDK header for mechanical waste processors,
// aligned with PrometheusPraxis ecosafety and governance principles.
// This header defines configuration and telemetry structures for
// hammermill and shredder devices, with no actuator-facing methods
// or hardware control surfaces. [file:6][file:8][file:15]

#ifndef PROMETHEUS_PRAXIS_CPP_WASTE_SHREDDING_SHREDDER_CONTROLLER_HPP
#define PROMETHEUS_PRAXIS_CPP_WASTE_SHREDDING_SHREDDER_CONTROLLER_HPP

#include <cstdint>
#include <cstddef>
#include <array>
#include <string>
#include <string_view>
#include <optional>

namespace prometheus_praxis::waste::shredding {

// ---------------------------------------------------------------------
// Enumerations and small types
// ---------------------------------------------------------------------

/// High-level device class for waste processors. [file:6][file:8]
enum class DeviceClass : std::uint8_t {
    Shredder = 0,
    Hammermill = 1
};

/// Mechanical drive type classification. [file:6][file:8]
enum class DriveType : std::uint8_t {
    Unknown = 0,
    DirectDrive = 1,
    BeltDrive = 2,
    HydraulicDrive = 3,
    ElectricDrive = 4
};

/// Feedstock class describes material families allowed into the chamber. [file:6][file:8]
/// These classes represent mechanical and ecological bands, not actuator commands.
enum class FeedstockClass : std::uint8_t {
    Unknown = 0,
    MixedMunicipalSolidWaste = 1,
    OrganicsOnly = 2,
    PlasticsLightFraction = 3,
    ConstructionAndDemolition = 4,
    E_WasteLowDensity = 5,
    BiomassWood = 6,
    MetalsLightGauge = 7
};

/// Nominal lane classification for research vs production usage. [file:6][file:8]
enum class OperationLane : std::uint8_t {
    Research = 0,
    Pilot = 1,
    Production = 2
};

/// Simple boolean-and-string flag for corridor admissibility diagnostics. [file:8]
struct CorridorFlag {
    bool ok;
    std::string message;
};

// ---------------------------------------------------------------------
// Geometry and mechanical descriptor structs
// ---------------------------------------------------------------------

/// Rectangular aperture geometry (chute in/out). [file:4][file:8]
struct RectangularAperture {
    double width_m;   // internal width, meters
    double length_m;  // internal length/height, meters

    constexpr RectangularAperture(double width, double length) noexcept
        : width_m(width), length_m(length) {}
};

/// Chamber geometry for a shredding or hammermill machine. [file:4][file:8]
struct ChamberGeometry {
    double length_m;          // axial length of chamber, meters
    double diameter_m;        // inner diameter or equivalent width, meters
    double liner_thickness_m; // liner or wall thickness, meters
    double rotor_diameter_m;  // rotor diameter, meters
    double rotor_length_m;    // rotor axial length, meters
    std::size_t rotor_teeth;  // number of cutting teeth / hammers
    RectangularAperture feed_aperture;
    RectangularAperture discharge_aperture;

    ChamberGeometry(double length,
                    double diameter,
                    double liner_thickness,
                    double rotor_diameter,
                    double rotor_length,
                    std::size_t rotor_teeth_count,
                    const RectangularAperture &feed,
                    const RectangularAperture &discharge) noexcept
        : length_m(length),
          diameter_m(diameter),
          liner_thickness_m(liner_thickness),
          rotor_diameter_m(rotor_diameter),
          rotor_length_m(rotor_length),
          rotor_teeth(rotor_teeth_count),
          feed_aperture(feed),
          discharge_aperture(discharge) {}
};

/// Rated RPM band for safe operation. [file:4][file:8]
struct RatedRpmBand {
    double min_rpm;      // minimum continuous RPM
    double nominal_rpm;  // nominal continuous RPM
    double max_rpm;      // maximum continuous RPM before corridor violation

    constexpr RatedRpmBand(double min_rpm_in,
                           double nominal_rpm_in,
                           double max_rpm_in) noexcept
        : min_rpm(min_rpm_in),
          nominal_rpm(nominal_rpm_in),
          max_rpm(max_rpm_in) {}
};

/// Torque band specification in SI units. [file:4][file:8]
struct TorqueBand {
    double min_torque_Nm;
    double nominal_torque_Nm;
    double max_torque_Nm;

    constexpr TorqueBand(double min_torque_in,
                         double nominal_torque_in,
                         double max_torque_in) noexcept
        : min_torque_Nm(min_torque_in),
          nominal_torque_Nm(nominal_torque_in),
          max_torque_Nm(max_torque_in) {}
};

/// Axial and radial load limits for bearings and frame. [file:4][file:8]
struct LoadLimits {
    double max_radial_load_kN;
    double max_axial_load_kN;

    constexpr LoadLimits(double max_radial_kN,
                         double max_axial_kN) noexcept
        : max_radial_load_kN(max_radial_kN),
          max_axial_load_kN(max_axial_kN) {}
};

/// Feedstock corridor configuration for a device. [file:4][file:8]
struct FeedstockCorridorConfig {
    std::array<FeedstockClass, 8> allowed_classes{};
    std::size_t allowed_count{0};

    double max_nominal_particle_size_mm;  // e.g. pre-shred size
    double max_inert_fraction_mass_ratio; // e.g. stones, metals fraction 0..1
    double max_moisture_mass_ratio;       // 0..1, for organics and MSW

    FeedstockCorridorConfig() noexcept = default;

    FeedstockCorridorConfig(const std::array<FeedstockClass, 8> &classes,
                            std::size_t count,
                            double max_particle_mm,
                            double max_inert_ratio,
                            double max_moisture_ratio) noexcept
        : allowed_classes(classes),
          allowed_count(count),
          max_nominal_particle_size_mm(max_particle_mm),
          max_inert_fraction_mass_ratio(max_inert_ratio),
          max_moisture_mass_ratio(max_moisture_ratio) {}
};

// ---------------------------------------------------------------------
// Shredder and hammermill configuration structs
// ---------------------------------------------------------------------

/// Static configuration for a rotary shredder device. [file:4][file:6][file:8]
struct ShredderConfig {
    std::string device_id;         // logical identifier in EcoNet / EcoFort
    DeviceClass device_class;      // always DeviceClass::Shredder
    DriveType drive_type;

    OperationLane lane;            // Research, Pilot, Production
    RatedRpmBand rpm_band;
    TorqueBand torque_band;
    LoadLimits load_limits;
    ChamberGeometry chamber;

    FeedstockCorridorConfig feedstock_corridor;

    // Governance and ecosafety metadata for non-actuating diagnostics. [file:6][file:8]
    std::string ecosafety_plane;   // e.g. "WASTE_SHREDDING"
    std::string ker_profile_name;  // e.g. "WasteShredderKER2026v1"
    std::string hex_anchor;        // Phoenix hex anchor string
    std::string aln_shard_name;    // ALN spec binding this configuration

    ShredderConfig(const std::string &id,
                   DeviceClass cls,
                   DriveType drive,
                   OperationLane lane_in,
                   const RatedRpmBand &rpm,
                   const TorqueBand &torque,
                   const LoadLimits &limits,
                   const ChamberGeometry &geom,
                   const FeedstockCorridorConfig &feed_cfg,
                   const std::string &plane,
                   const std::string &ker_profile,
                   const std::string &hex,
                   const std::string &aln) noexcept
        : device_id(id),
          device_class(cls),
          drive_type(drive),
          lane(lane_in),
          rpm_band(rpm),
          torque_band(torque),
          load_limits(limits),
          chamber(geom),
          feedstock_corridor(feed_cfg),
          ecosafety_plane(plane),
          ker_profile_name(ker_profile),
          hex_anchor(hex),
          aln_shard_name(aln) {}
};

/// Static configuration for a hammermill device. [file:4][file:6][file:8]
struct HammermillConfig {
    std::string device_id;
    DeviceClass device_class;      // always DeviceClass::Hammermill
    DriveType drive_type;

    OperationLane lane;
    RatedRpmBand rpm_band;
    TorqueBand torque_band;
    LoadLimits load_limits;
    ChamberGeometry chamber;

    FeedstockCorridorConfig feedstock_corridor;

    // Additional hammermill-specific descriptors.
    double screen_open_area_fraction; // 0..1, ratio of open screen area
    double hammer_mass_kg;            // nominal mass per hammer
    std::size_t hammer_count;         // number of hammers on rotor

    std::string ecosafety_plane;
    std::string ker_profile_name;
    std::string hex_anchor;
    std::string aln_shard_name;

    HammermillConfig(const std::string &id,
                     DeviceClass cls,
                     DriveType drive,
                     OperationLane lane_in,
                     const RatedRpmBand &rpm,
                     const TorqueBand &torque,
                     const LoadLimits &limits,
                     const ChamberGeometry &geom,
                     const FeedstockCorridorConfig &feed_cfg,
                     double screen_open_area,
                     double hammer_mass,
                     std::size_t hammer_cnt,
                     const std::string &plane,
                     const std::string &ker_profile,
                     const std::string &hex,
                     const std::string &aln) noexcept
        : device_id(id),
          device_class(cls),
          drive_type(drive),
          lane(lane_in),
          rpm_band(rpm),
          torque_band(torque),
          load_limits(limits),
          chamber(geom),
          feedstock_corridor(feed_cfg),
          screen_open_area_fraction(screen_open_area),
          hammer_mass_kg(hammer_mass),
          hammer_count(hammer_cnt),
          ecosafety_plane(plane),
          ker_profile_name(ker_profile),
          hex_anchor(hex),
          aln_shard_name(aln) {}
};

// ---------------------------------------------------------------------
// Telemetry structs (purely observational, non-actuating)
// ---------------------------------------------------------------------

/// Base struct for instantaneous mechanical telemetry. [file:4][file:6][file:8]
struct MechanicalInstantTelemetry {
    double timestamp_s;           // seconds since a reference epoch
    double rotor_speed_rpm;       // measured rotor speed
    double torque_Nm;             // measured shaft torque
    double axial_load_kN;         // axial load estimate
    double radial_load_kN;        // radial load estimate
    double chamber_fill_fraction; // 0..1, approximate fill ratio

    double inlet_throughput_tph;  // tonnes per hour, inlet
    double outlet_throughput_tph; // tonnes per hour, outlet

    double power_kw;              // electrical or hydraulic power draw
    double vibration_rms_g;       // dimensionless, RMS vibration

    MechanicalInstantTelemetry(double ts,
                               double speed,
                               double torque_val,
                               double axial,
                               double radial,
                               double fill,
                               double inlet_tph,
                               double outlet_tph,
                               double power,
                               double vibration) noexcept
        : timestamp_s(ts),
          rotor_speed_rpm(speed),
          torque_Nm(torque_val),
          axial_load_kN(axial),
          radial_load_kN(radial),
          chamber_fill_fraction(fill),
          inlet_throughput_tph(inlet_tph),
          outlet_throughput_tph(outlet_tph),
          power_kw(power),
          vibration_rms_g(vibration) {}
};

/// Feedstock telemetry aggregates for corridor diagnostics. [file:4][file:8]
struct FeedstockTelemetry {
    double avg_particle_size_mm;
    double moisture_mass_ratio;       // 0..1
    double inert_fraction_mass_ratio; // 0..1
    FeedstockClass dominant_class;

    FeedstockTelemetry(double particle_mm,
                       double moisture_ratio,
                       double inert_ratio,
                       FeedstockClass dom_class) noexcept
        : avg_particle_size_mm(particle_mm),
          moisture_mass_ratio(moisture_ratio),
          inert_fraction_mass_ratio(inert_ratio),
          dominant_class(dom_class) {}
};

/// Shredder telemetry struct (non-actuating, EcoNet-ready). [file:4][file:6][file:8]
struct ShredderTelemetry {
    std::string device_id;     // logical ID to join with ShredderConfig and SQL views
    MechanicalInstantTelemetry mechanical;
    FeedstockTelemetry feed;

    // Bounded ecosafety/ker bands (0..1) for diagnostics only. [file:6][file:8]
    double k_knowledge_factor;   // 0..1
    double e_ecoimpact_factor;   // 0..1
    double r_risk_factor;        // 0..1
    double ker_score;            // k + e - r corridor value (not enforced here)

    double vt_before;            // Lyapunov residual before window
    double vt_after;             // Lyapunov residual after window;
    double roh_coordinate;       // 0..1 Risk-of-Harm scalar

    ShredderTelemetry(const std::string &id,
                      const MechanicalInstantTelemetry &mech,
                      const FeedstockTelemetry &feed_tele,
                      double k,
                      double e,
                      double r,
                      double ker,
                      double vt_b,
                      double vt_a,
                      double roh) noexcept
        : device_id(id),
          mechanical(mech),
          feed(feed_tele),
          k_knowledge_factor(k),
          e_ecoimpact_factor(e),
          r_risk_factor(r),
          ker_score(ker),
          vt_before(vt_b),
          vt_after(vt_a),
          roh_coordinate(roh) {}
};

/// Hammermill telemetry struct (non-actuating). [file:4][file:6][file:8]
struct HammermillTelemetry {
    std::string device_id;
    MechanicalInstantTelemetry mechanical;
    FeedstockTelemetry feed;

    // Hammermill-specific diagnostic channels.
    double screen_loading_fraction; // 0..1, fraction of screen area actively passing material
    double fines_fraction_mass_ratio; // 0..1, fraction of fines in output

    // KER and Lyapunov diagnostics.
    double k_knowledge_factor;
    double e_ecoimpact_factor;
    double r_risk_factor;
    double ker_score;
    double vt_before;
    double vt_after;
    double roh_coordinate;

    HammermillTelemetry(const std::string &id,
                        const MechanicalInstantTelemetry &mech,
                        const FeedstockTelemetry &feed_tele,
                        double screen_load,
                        double fines_ratio,
                        double k,
                        double e,
                        double r,
                        double ker,
                        double vt_b,
                        double vt_a,
                        double roh) noexcept
        : device_id(id),
          mechanical(mech),
          feed(feed_tele),
          screen_loading_fraction(screen_load),
          fines_fraction_mass_ratio(fines_ratio),
          k_knowledge_factor(k),
          e_ecoimpact_factor(e),
          r_risk_factor(r),
          ker_score(ker),
          vt_before(vt_b),
          vt_after(vt_a),
          roh_coordinate(roh) {}
};

// ---------------------------------------------------------------------
// Non-actuating controller facades (configuration + validation only)
// ---------------------------------------------------------------------

/// Pure configuration/validation facade for a shredder.
/// No actuator methods; this type only provides helpers to reason
/// about corridor admissibility and basic bounds in user code. [file:6][file:8]
class ShredderController {
public:
    explicit ShredderController(const ShredderConfig &cfg) noexcept
        : config_(cfg) {}

    const ShredderConfig &config() const noexcept {
        return config_;
    }

    /// Check if a feedstock telemetry sample is admissible under
    /// the configured corridor. No state mutation or device IO. [file:4][file:8]
    CorridorFlag check_feedstock_corridor(const FeedstockTelemetry &telemetry) const {
        // Basic corridor checks; callers can extend or override in governance crates.
        bool size_ok = telemetry.avg_particle_size_mm <= config_.feedstock_corridor.max_nominal_particle_size_mm;
        bool inert_ok = telemetry.inert_fraction_mass_ratio <= config_.feedstock_corridor.max_inert_fraction_mass_ratio;
        bool moisture_ok = telemetry.moisture_mass_ratio <= config_.feedstock_corridor.max_moisture_mass_ratio;

        bool class_ok = false;
        for (std::size_t i = 0; i < config_.feedstock_corridor.allowed_count; ++i) {
            if (config_.feedstock_corridor.allowed_classes[i] == telemetry.dominant_class) {
                class_ok = true;
                break;
            }
        }

        if (size_ok && inert_ok && moisture_ok && class_ok) {
            return CorridorFlag{true, "Feedstock within configured corridor."};
        }

        std::string msg = "Feedstock corridor violation: ";
        if (!size_ok) {
            msg += "particle_size_exceeds_limit; ";
        }
        if (!inert_ok) {
            msg += "inert_fraction_exceeds_limit; ";
        }
        if (!moisture_ok) {
            msg += "moisture_fraction_exceeds_limit; ";
        }
        if (!class_ok) {
            msg += "feedstock_class_not_allowed; ";
        }
        return CorridorFlag{false, msg};
    }

    /// Check that mechanical telemetry stays within RPM, torque, and load bands. [file:4][file:8]
    CorridorFlag check_mechanical_bounds(const MechanicalInstantTelemetry &telemetry) const {
        bool rpm_ok = telemetry.rotor_speed_rpm >= config_.rpm_band.min_rpm &&
                      telemetry.rotor_speed_rpm <= config_.rpm_band.max_rpm;

        bool torque_ok = telemetry.torque_Nm >= config_.torque_band.min_torque_Nm &&
                         telemetry.torque_Nm <= config_.torque_band.max_torque_Nm;

        bool radial_ok = telemetry.radial_load_kN <= config_.load_limits.max_radial_load_kN;
        bool axial_ok = telemetry.axial_load_kN <= config_.load_limits.max_axial_load_kN;

        if (rpm_ok && torque_ok && radial_ok && axial_ok) {
            return CorridorFlag{true, "Mechanical telemetry within configured bands."};
        }

        std::string msg = "Mechanical band violation: ";
        if (!rpm_ok) {
            msg += "rpm_out_of_band; ";
        }
        if (!torque_ok) {
            msg += "torque_out_of_band; ";
        }
        if (!radial_ok) {
            msg += "radial_load_exceeds_limit; ";
        }
        if (!axial_ok) {
            msg += "axial_load_exceeds_limit; ";
        }
        return CorridorFlag{false, msg};
    }

private:
    ShredderConfig config_;
};

/// Pure configuration/validation facade for a hammermill. [file:4][file:6][file:8]
class HammermillController {
public:
    explicit HammermillController(const HammermillConfig &cfg) noexcept
        : config_(cfg) {}

    const HammermillConfig &config() const noexcept {
        return config_;
    }

    CorridorFlag check_feedstock_corridor(const FeedstockTelemetry &telemetry) const {
        bool size_ok = telemetry.avg_particle_size_mm <= config_.feedstock_corridor.max_nominal_particle_size_mm;
        bool inert_ok = telemetry.inert_fraction_mass_ratio <= config_.feedstock_corridor.max_inert_fraction_mass_ratio;
        bool moisture_ok = telemetry.moisture_mass_ratio <= config_.feedstock_corridor.max_moisture_mass_ratio;

        bool class_ok = false;
        for (std::size_t i = 0; i < config_.feedstock_corridor.allowed_count; ++i) {
            if (config_.feedstock_corridor.allowed_classes[i] == telemetry.dominant_class) {
                class_ok = true;
                break;
            }
        }

        if (size_ok && inert_ok && moisture_ok && class_ok) {
            return CorridorFlag{true, "Feedstock within configured corridor."};
        }

        std::string msg = "Feedstock corridor violation: ";
        if (!size_ok) {
            msg += "particle_size_exceeds_limit; ";
        }
        if (!inert_ok) {
            msg += "inert_fraction_exceeds_limit; ";
        }
        if (!moisture_ok) {
            msg += "moisture_fraction_exceeds_limit; ";
        }
        if (!class_ok) {
            msg += "feedstock_class_not_allowed; ";
        }
        return CorridorFlag{false, msg};
    }

    CorridorFlag check_mechanical_bounds(const MechanicalInstantTelemetry &telemetry) const {
        bool rpm_ok = telemetry.rotor_speed_rpm >= config_.rpm_band.min_rpm &&
                      telemetry.rotor_speed_rpm <= config_.rpm_band.max_rpm;

        bool torque_ok = telemetry.torque_Nm >= config_.torque_band.min_torque_Nm &&
                         telemetry.torque_Nm <= config_.torque_band.max_torque_Nm;

        bool radial_ok = telemetry.radial_load_kN <= config_.load_limits.max_radial_load_kN;
        bool axial_ok = telemetry.axial_load_kN <= config_.load_limits.max_axial_load_kN;

        if (rpm_ok && torque_ok && radial_ok && axial_ok) {
            return CorridorFlag{true, "Mechanical telemetry within configured bands."};
        }

        std::string msg = "Mechanical band violation: ";
        if (!rpm_ok) {
            msg += "rpm_out_of_band; ";
        }
        if (!torque_ok) {
            msg += "torque_out_of_band; ";
        }
        if (!radial_ok) {
            msg += "radial_load_exceeds_limit; ";
        }
        if (!axial_ok) {
            msg += "axial_load_exceeds_limit; ";
        }
        return CorridorFlag{false, msg};
    }

private:
    HammermillConfig config_;
};

} // namespace prometheus_praxis::waste::shredding

#endif // PROMETHEUS_PRAXIS_CPP_WASTE_SHREDDING_SHREDDER_CONTROLLER_HPP
