#pragma once

#include <string>
#include <vector>
#include <cstdint>

namespace ppx::waste::shredding::governance {

struct ShredderTelemetry {
    std::string machine_id;
    std::string region;
    std::string lane;

    double motor_current_a;
    double chamber_torque_nm;
    double vibration_g;
    double throughput_kg_per_h;
    double temperature_c;

    std::string feedstock_class;
};

struct ScreenDrumTelemetry {
    std::string machine_id;

    double drum_speed_rpm;
    double feed_rate_kg_per_h;
    double dp_pa;
    double cut_point_mm;
    double fines_fraction;
};

struct KerSnapshot {
    std::string machine_id;
    std::string region;
    std::string lane;

    double carbon_radius;
    double biodiversity_radius;

    double k_score;
    double e_score;
    double r_score;

    double vt_residual;
    double roh_scalar;

    double ker_weighted_carbon_radius;
    double ker_weighted_biodiversity_radius;
};

struct ShreddingKerSnapshot {
    ShredderTelemetry shredder;
    ScreenDrumTelemetry screen;
    KerSnapshot ker;

    bool carbon_negative_ok;
    bool restoration_ok;
    bool lane_admissible;
};

class BlastRadiusClient {
public:
    explicit BlastRadiusClient(const std::string& shared_lib_path);

    KerSnapshot fetch_ker_snapshot(const std::string& db_path,
                                   const std::string& machine_id) const;

private:
    std::string lib_path_;
};

class LaneGovernanceClient {
public:
    explicit LaneGovernanceClient(const std::string& shared_lib_path);

    bool check_lane_admissible(const std::string& db_path,
                               const std::string& shard_id,
                               std::string& reason_out) const;

private:
    std::string lib_path_;
};

class ShreddingGovernanceAdapter {
public:
    ShreddingGovernanceAdapter(const std::string& blastradius_lib_path,
                               const std::string& governance_lib_path);

    ShreddingKerSnapshot compute_snapshot(const std::string& db_path,
                                          const ShredderTelemetry& shredder,
                                          const ScreenDrumTelemetry& screen) const;

private:
    BlastRadiusClient blastradius_client_;
    LaneGovernanceClient lane_governance_client_;
};

} // namespace ppx::waste::shredding::governance
