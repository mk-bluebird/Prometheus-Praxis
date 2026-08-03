// File: cpp/include/eco_restoration.hpp
#ifndef ECO_RESTORATION_HPP
#define ECO_RESTORATION_HPP

#include <vector>
#include <string>

/**
 * @file eco_restoration.hpp
 * @brief Aggregated public interfaces for eco-restoration C++ modules.
 *
 * This header provides a small but complete overview of the main eco-restoration
 * utilities: KER/Lyapunov scoring, material eco-impact, PFAS fate corridors,
 * Phoenix canal blast-radius, hex-anchor risk, and configuration loading.
 * It is intended as an entry point for new coders and AI agents exploring
 * the Prometheus-Praxis eco_restoration C++ suite.[59][78]
 */

namespace eco_tools {

/**
 * @brief Compute KER composite score s = k * e - r.
 */
double ker_score(double k, double e, double r);

/**
 * @brief Compute Lyapunov residual V_t = sum_j w_j r_j^2.
 */
double lyapunov_residual(const std::vector<double>& w,
                         const std::vector<double>& r);

} // namespace eco_tools

namespace eco_restoration {

/**
 * @brief Material test parameters (ISO 14851/14855, OECD 301 style).
 */
struct MaterialTestParams {
    double oxygen_depletion_percent;
    double co2_evolution_percent;
    double bod_removal_percent;
    double doc_removal_percent;
    double days_to_pass_window;
    double toxicity_score;
    double pfas_presence;
};

/**
 * @brief Material eco-impact outputs (KER + biodegradability).
 */
struct MaterialEcoImpact {
    double k_safe_fraction;
    double e_eco_benefit_band;
    double r_risk_max;
    double ker_score;
    double biodegradability_score;
};

/**
 * @brief Compute material eco-impact (C++ interface).
 */
MaterialEcoImpact compute_material_eco_impact_cpp(const MaterialTestParams& params);

} // namespace eco_restoration

namespace eco_pfas {

/**
 * @brief PFAS corridor state variables.
 */
struct PFASState {
    double mass_kg;
    double sorbed_fraction;
    double cold_survival_factor;
};

/**
 * @brief Step PFAS corridor state forward one discrete step.
 */
PFASState step_pfas_corridor(const PFASState& state,
                             double base_degradation_rate,
                             double current_temp_C,
                             double cold_temp_C,
                             double sorption_increment);

} // namespace eco_pfas

namespace phoenix_canal {

/**
 * @brief Phoenix canal blast-radius risk coordinates.
 */
struct BlastRisk {
    double r_hydraulics;
    double r_energy;
    double r_topology;
};

/**
 * @brief Run a simple blast-radius update over a pre-defined grid and compute risk.
 *
 * The grid and thresholds are typically loaded from SQLite, but this function
 * provides a direct interface for simulations.
 */
BlastRisk run_blast_radius_step();

} // namespace phoenix_canal

namespace phoenix_hex {

/**
 * @brief Risk coordinates and Lyapunov residual for a Phoenix hex.
 */
struct HexRisk {
    std::string hex_id;
    double r_hydraulics;
    double r_energy;
    double r_topology;
    double r_biodiversity;
    double Vt;
};

/**
 * @brief Load hex risks from a Phoenix registry SQLite DB.
 */
std::vector<HexRisk> load_hex_risks(const std::string& db_path);

} // namespace phoenix_hex

namespace eco_config {

/**
 * @brief Canal node configuration.
 */
struct CanalNodeConfig {
    std::string node_code;
    std::string description;
    std::string ker_band;
    std::string fog_band;
    std::string canal_plane;
};

/**
 * @brief Hex anchor configuration.
 */
struct HexAnchorConfig {
    std::string hex_id;
    std::string domain;
    std::string subdomain;
    std::string owner_did;
};

/**
 * @brief Workload corridor configuration.
 */
struct WorkloadCorridorConfig {
    double max_energy_J;
    double max_deltaVt;
    double w_energy;
    double w_topology;
};

/**
 * @brief Load canal node configs from a JSON config file.
 */
std::vector<CanalNodeConfig> load_canal_nodes(const std::string& path);

/**
 * @brief Load hex anchor configs from a JSON config file.
 */
std::vector<HexAnchorConfig> load_hex_anchors(const std::string& path);

/**
 * @brief Load workload corridor config from a JSON config file.
 */
WorkloadCorridorConfig load_workload_corridor(const std::string& path);

} // namespace eco_config

#endif // ECO_RESTORATION_HPP
