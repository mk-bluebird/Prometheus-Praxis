#pragma once

#include <string>
#include <vector>
#include <cstdint>

namespace ppx::waste::wastewater::governance {

struct PumpTelemetry {
    std::string asset_id;
    std::string site_code;
    std::string region;
    std::string lane;

    double flow_m3_per_h;
    double head_m;
    double energy_kw;
    double energy_kwh_window;

    double turbidity_ntu;
    double dissolved_oxygen_mg_per_l;
    double temperature_c;
};

struct WastewaterScreenTelemetry {
    std::string asset_id;
    std::string screen_code;

    double solids_capture_fraction;
    double differential_pressure_pa;
    double backwash_events;
    double runtime_h;
};

struct PumpCorridorStatus {
    bool corridor_satisfied;
    std::string corridor_status;

    double vt_before_mean;
    double vt_after_mean;
    double delta_vt;

    double r_carbon;
    double r_biodiv;

    std::string decision_mode;
};

struct PumpAccountabilityRecord {
    std::string asset_id;
    std::string site_code;
    std::string region;
    std::string lane;

    std::string window_start_utc;
    std::string window_end_utc;

    double energy_kwh;
    double energy_kwh_solar;
    double energy_kwh_grid;
    double co2e_kg;

    double vt_before_mean;
    double vt_after_mean;
    double delta_vt;

    double r_carbon;
    double r_biodiv;

    std::string corridor_status;
    std::string decision_mode;

    std::string shard_id;
    std::string steward_did;
    std::string evidence_hex;
};

class EcoPumpDiagnosticsClient {
public:
    explicit EcoPumpDiagnosticsClient(const std::string& shared_lib_path);

    PumpCorridorStatus summarize_corridor(const std::string& db_path,
                                          const std::string& asset_id,
                                          const std::string& window_start_utc,
                                          const std::string& window_end_utc) const;

private:
    std::string lib_path_;
};

class DeploymentAccountabilityClient {
public:
    explicit DeploymentAccountabilityClient(const std::string& shared_lib_path);

    void log_pump_record(const std::string& db_path,
                         const PumpAccountabilityRecord& record) const;

private:
    std::string lib_path_;
};

class PumpGovernanceAdapter {
public:
    PumpGovernanceAdapter(const std::string& diagnostics_lib_path,
                          const std::string& accountability_lib_path);

    PumpAccountabilityRecord build_record(const PumpTelemetry& pump,
                                          const WastewaterScreenTelemetry& screen,
                                          const PumpCorridorStatus& corridor,
                                          const std::string& steward_did,
                                          const std::string& shard_id,
                                          const std::string& window_start_utc,
                                          const std::string& window_end_utc) const;

    void send_to_accountability(const std::string& db_path,
                                const PumpAccountabilityRecord& record) const;

private:
    EcoPumpDiagnosticsClient diagnostics_client_;
    DeploymentAccountabilityClient accountability_client_;
};

} // namespace ppx::waste::wastewater::governance
