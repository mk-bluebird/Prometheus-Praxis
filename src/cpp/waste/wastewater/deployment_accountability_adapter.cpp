// File: Prometheus-Praxis/src/cpp/waste/wastewater/deployment_accountability_adapter.cpp
// License: MIT OR Apache-2.0
//
// Non-actuating implementation that serializes pump/screen telemetry
// and corridor decisions into governance-ready payloads and calls the
// EcoNet / EcoRestoration deployment accountability FFI (write-once,
// governance-bound). Every corridor-checked start/stop decision is
// logged as an evidence row; hardware actuation lives in separate,
// guarded systems.[file:6][file:8][web:79][web:83]

#include "deployment_accountability_adapter.hpp"

#include <sstream>
#include <stdexcept>

extern "C" {

/// Rust FFI: ingest a JSON-encoded accountability payload.
/// Implemented in EcoNet / EcoRestoration crates using SQLite
/// and rusqlite bindings; responsible for persisting evidence
/// rows in the deployment accountability core.[web:80][web:87]
void deployment_accountability_core_ingest_json_payload(
    const char* json_payload);

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

/// Simple, dependency-light JSON serializer for
/// PumpAccountabilityRecord. This avoids external
/// JSON libraries while still producing a well-formed
/// JSON object suitable for Rust-side parsing.[web:79][web:83]
std::string to_json(const PumpAccountabilityRecord& rec) {
    std::ostringstream oss;
    oss.precision(6);
    oss << "{";

    oss << "\"window_id\":\""      << rec.window_id      << "\",";
    oss << "\"asset_id\":\""       << rec.asset_id       << "\",";
    oss << "\"corridor_id\":\""    << rec.corridor_id    << "\",";
    oss << "\"timestamputc\":\""   << rec.timestamputc   << "\",";

    oss << "\"turbidity_ntu\":"        << rec.turbidity_ntu        << ",";
    oss << "\"dissolved_oxygen_mg_l\":"<< rec.dissolved_oxygen_mg_l<< ",";
    oss << "\"flow_m3_s\":"            << rec.flow_m3_s            << ",";
    oss << "\"head_m\":"               << rec.head_m               << ",";
    oss << "\"energy_kwh\":"           << rec.energy_kwh           << ",";

    oss << "\"turbidity_band\":\"" << rec.bands.turbidity_band << "\",";
    oss << "\"do_band\":\""        << rec.bands.do_band        << "\",";

    oss << "\"k\":"   << clamp01(rec.ker.k)   << ",";
    oss << "\"e\":"   << clamp01(rec.ker.e)   << ",";
    oss << "\"r\":"   << clamp01(rec.ker.r)   << ",";
    oss << "\"roh\":" << clamp01(rec.roh.roh_ceiling) << ",";

    oss << "\"within_corridor\":" << (rec.corridor_status.within_corridor ? "true" : "false") << ",";
    oss << "\"no_build\":"        << (rec.corridor_status.no_build        ? "true" : "false") << ",";
    oss << "\"start_allowed\":"   << (rec.corridor_status.start_allowed   ? "true" : "false") << ",";
    oss << "\"stop_allowed\":"    << (rec.corridor_status.stop_allowed    ? "true" : "false") << ",";

    oss << "\"lane_decision\":"   << static_cast<unsigned>(rec.lane_decision);

    oss << "}";
    return oss.str();
}

/// Classify turbidity into simple bands for accountability.
std::string classify_turbidity(double ntu) {
    if (ntu < 5.0)   return "LOW";
    if (ntu < 50.0)  return "MEDIUM";
    return "HIGH";
}

/// Classify dissolved oxygen into bands for accountability.
/// Typical wastewater ranges: <2 mg/L low, 2–8 mg/L ok, >8 mg/L high.[web:59]
std::string classify_do(double do_mg_l) {
    if (do_mg_l < 2.0)  return "LOW";
    if (do_mg_l <= 8.0) return "OK";
    return "HIGH";
}

} // namespace

PumpAccountabilityRecord make_pump_accountability_record(
    const std::string& window_id,
    const std::string& timestamputc,
    const PumpTelemetry& telemetry,
    const PumpWindowKerRoh& ker_roh,
    PumpLaneDecision lane_decision)
{
    PumpTelemetry tmp = telemetry;
    tmp.validate_and_normalize();

    PumpAccountabilityRecord rec;
    rec.window_id   = window_id;
    rec.asset_id    = tmp.pump_id;
    rec.corridor_id = tmp.corridor_id;
    rec.timestamputc= timestamputc;

    rec.turbidity_ntu        = tmp.turbidity_ntu;
    rec.dissolved_oxygen_mg_l= tmp.dissolved_oxygen_mg_l;
    rec.flow_m3_s            = tmp.flow_m3_s;
    rec.head_m               = tmp.head_m;
    rec.energy_kwh           = tmp.energy_kwh;

    rec.bands.turbidity_band = classify_turbidity(tmp.turbidity_ntu);
    rec.bands.do_band        = classify_do(tmp.dissolved_oxygen_mg_l);

    rec.ker           = ker_roh.ker;
    rec.roh           = ker_roh.roh;
    rec.corridor_status = ker_roh.corridor_status;
    rec.lane_decision = lane_decision;

    return rec;
}

PumpAccountabilityRecord make_screen_accountability_record(
    const std::string& window_id,
    const std::string& timestamputc,
    const WastewaterScreenTelemetry& telemetry,
    const PumpWindowKerRoh& ker_roh,
    PumpLaneDecision lane_decision)
{
    WastewaterScreenTelemetry tmp = telemetry;
    tmp.validate_and_normalize();

    PumpAccountabilityRecord rec;
    rec.window_id   = window_id;
    rec.asset_id    = tmp.screen_id;
    rec.corridor_id = tmp.corridor_id;
    rec.timestamputc= timestamputc;

    // For screens, use influent turbidity and derived DO band placeholder
    // (if DO is not measured at the screen, governance may substitute
    // corridor-level DO estimates upstream).
    rec.turbidity_ntu        = tmp.influent_turbidity_ntu;
    rec.dissolved_oxygen_mg_l= 0.0; // unknown / not measured here
    rec.flow_m3_s            = 0.0; // not applicable for screen telemetry
    rec.head_m               = tmp.delta_head_m;
    rec.energy_kwh           = tmp.energy_kwh;

    rec.bands.turbidity_band = classify_turbidity(tmp.influent_turbidity_ntu);
    rec.bands.do_band        = classify_do(rec.dissolved_oxygen_mg_l);

    rec.ker           = ker_roh.ker;
    rec.roh           = ker_roh.roh;
    rec.corridor_status = ker_roh.corridor_status;
    rec.lane_decision = lane_decision;

    return rec;
}

/// C-ABI adapter: ingest a PumpAccountabilityRecord into the
/// deployment accountability core by serializing it to JSON
/// and calling the Rust FFI. This function is non-actuating:
/// it never sends start/stop commands; it only logs evidence
/// rows for corridor-checked decisions.[web:79][web:83][web:80]
extern "C" void deployment_accountability_core_ingest_pump_record(
    const PumpAccountabilityRecord* record)
{
    if (record == nullptr) {
        throw std::invalid_argument("deployment_accountability_core_ingest_pump_record: record must not be null.");
    }

    std::string json = to_json(*record);
    deployment_accountability_core_ingest_json_payload(json.c_str());
}

} // namespace wastewater
} // namespace waste
} // namespace prometheus_praxis
