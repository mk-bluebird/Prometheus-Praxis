// File: Prometheus-Praxis/src/cpp/waste/wastewater/deployment_accountability_adapter.hpp
// License: MIT OR Apache-2.0
//
// Thin, non-actuating governance adapter that turns PumpTelemetry
// and corridor decisions into payloads for the deployment
// accountability core (Rust/SQLite) via C-ABI. Defines
// PumpAccountabilityRecord with window timestamps, K/E/R, RoH,
// turbidity/DO bands, and lane decisions, suitable for audit and
// ecowealth scoring, never direct pump control.[file:6][file:8][web:73][web:77]

#ifndef PROMETHEUS_PRAXIS_WASTE_WASTEWATER_DEPLOYMENT_ACCOUNTABILITY_ADAPTER_HPP
#define PROMETHEUS_PRAXIS_WASTE_WASTEWATER_DEPLOYMENT_ACCOUNTABILITY_ADAPTER_HPP

#include <cstdint>
#include <string>

#include "pump_telemetry.hpp"

namespace prometheus_praxis {
namespace waste {
namespace wastewater {

/// Lane decision for accountability records.
/// Mirrors EcoNet lanes and governance decisions.
enum class PumpLaneDecision : std::uint8_t {
    UNKNOWN   = 0,
    RESEARCH  = 1,
    PILOT     = 2,
    PRODUCTION= 3,
    BLOCKED   = 4
};

/// Turbidity and DO bands used for accountability.
/// These are simple categorical bands derived from
/// raw telemetry; classification is performed in
/// implementation, not here.[web:54][web:59]
struct WaterQualityBands {
    std::string turbidity_band; // e.g., "LOW", "MEDIUM", "HIGH"
    std::string do_band;        // e.g., "LOW", "OK", "HIGH"

    WaterQualityBands() = default;
};

/// Immutable record representing a single pump or screen
/// window as persisted into the deployment accountability
/// core (Rust/SQLite). This struct is designed to be
/// passed across C-ABI into Rust, where it is inserted
/// into an audit / ecowealth ledger.[web:74][web:77]
struct PumpAccountabilityRecord {
    // Identity and timing
    std::string window_id;      // unique window identifier
    std::string asset_id;       // pump_id or screen_id
    std::string corridor_id;    // corridor identifier
    std::string timestamputc;   // ISO-8601 UTC timestamp for window start

    // Telemetry summary
    double turbidity_ntu;
    double dissolved_oxygen_mg_l;
    double flow_m3_s;
    double head_m;
    double energy_kwh;

    WaterQualityBands bands;

    // Governance metrics
    KerTriad  ker;
    RohCeiling roh;
    PumpCorridorStatus corridor_status;
    PumpLaneDecision lane_decision;

    PumpAccountabilityRecord()
        : turbidity_ntu(0.0),
          dissolved_oxygen_mg_l(0.0),
          flow_m3_s(0.0),
          head_m(0.0),
          energy_kwh(0.0),
          ker(),
          roh(),
          corridor_status(),
          lane_decision(PumpLaneDecision::UNKNOWN) {}
};

/// Pure helper signature: derive a PumpAccountabilityRecord from
/// PumpTelemetry and PumpWindowKerRoh, including lane decision
/// and water quality bands. Implementation will call into the
/// deployment accountability core via Rust C-ABI to persist, but
/// this header exposes only the struct and function prototype.
PumpAccountabilityRecord make_pump_accountability_record(
    const std::string& window_id,
    const std::string& timestamputc,
    const PumpTelemetry& telemetry,
    const PumpWindowKerRoh& ker_roh,
    PumpLaneDecision lane_decision);

/// Pure helper signature: derive a PumpAccountabilityRecord for
/// a screen telemetry window, reusing the same record structure
/// for unified accountability across pumps and screens.
PumpAccountabilityRecord make_screen_accountability_record(
    const std::string& window_id,
    const std::string& timestamputc,
    const WastewaterScreenTelemetry& telemetry,
    const PumpWindowKerRoh& ker_roh,
    PumpLaneDecision lane_decision);

/// C-ABI adapter signature: send a PumpAccountabilityRecord into
/// the deployment accountability core. Implemented in Rust and
/// backed by SQLite, this function is responsible for persisting
/// the record; it must remain non-actuating and governance-only.
extern "C" void deployment_accountability_core_ingest_pump_record(
    const PumpAccountabilityRecord* record);

} // namespace wastewater
} // namespace waste
} // namespace prometheus_praxis

#endif // PROMETHEUS_PRAXIS_WASTE_WASTEWATER_DEPLOYMENT_ACCOUNTABILITY_ADAPTER_HPP
