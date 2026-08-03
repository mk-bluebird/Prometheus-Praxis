// File: cpp/eco_restoration/actuation_gate_synapse.cpp
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <limits>
#include <cmath>
#include <chrono>

namespace eco_restoration {

// Simple KER triad and derived score.
struct KerProfile {
    double ker_k;  // knowledge-factor 0..1
    double ker_e;  // eco-impact value 0..1 (higher is better)
    double ker_r;  // risk-of-harm 0..1 (higher is worse)
    double ker_s;  // scalar corridor = ker_k * ker_e - ker_r

    KerProfile(double k = 0.0, double e = 0.0, double r = 0.0)
        : ker_k(k), ker_e(e), ker_r(r), ker_s(k * e - r) {}

    void recompute() {
        ker_s = ker_k * ker_e - ker_r;
    }
};

// Lane semantics: RESEARCH, EXPPROD, PROD.
enum class Lane {
    RESEARCH,
    EXPPROD,
    PROD
};

static Lane lane_from_string(const std::string &s) {
    if (s == "PROD") return Lane::PROD;
    if (s == "EXPPROD") return Lane::EXPPROD;
    return Lane::RESEARCH;
}

static std::string lane_to_string(Lane lane) {
    switch (lane) {
        case Lane::RESEARCH: return "RESEARCH";
        case Lane::EXPPROD:  return "EXPPROD";
        case Lane::PROD:     return "PROD";
    }
    return "RESEARCH";
}

// Governance flags for a synapse endpoint.
struct SynapseEndpointGovernance {
    std::string relpath;
    std::string synapse_class;  // "ACTUATION_GATE"
    bool allows_actuation;
    bool non_actuating;
    bool neuro_flag;        // true if neuro-adjacent
    bool citizen_ready;     // true if suitable for citizen operation
    Lane lane_default;
    KerProfile ker;

    SynapseEndpointGovernance()
        : relpath("cpp/eco_restoration/actuation_gate_synapse.cpp"),
          synapse_class("ACTUATION_GATE"),
          allows_actuation(true),
          non_actuating(false),
          neuro_flag(false),
          citizen_ready(false),
          lane_default(Lane::EXPPROD),
          ker(0.9, 0.85, 0.25) {
        ker.recompute();
    }

    // Check ALN-style invariants.
    bool ker_scalar_consistency() const {
        double expected = ker.ker_k * ker.ker_e - ker.ker_r;
        return std::fabs(expected - ker.ker_s) < 1e-9;
    }

    bool prod_lane_governance_ok() const {
        if (lane_default != Lane::PROD) {
            return true;
        }
        // For PROD, apply stricter corridor.
        return ker.ker_s > 0.2 && ker.ker_e >= 0.7 && ker.ker_r <= 0.5 && citizen_ready;
    }

    bool actuation_gate_semantics_ok() const {
        if (synapse_class != "ACTUATION_GATE") {
            // Only analytic bridges should be non-actuating.
            return !allows_actuation;
        }
        // Actuation gate must be actuating but governed by positive corridor.
        return allows_actuation && !non_actuating && ker.ker_s > 0.3;
    }

    bool invariants_ok(std::string &reason) const {
        if (!ker_scalar_consistency()) {
            reason = "KER scalar consistency violated: ker_s != ker_k * ker_e - ker_r";
            return false;
        }
        if (!actuation_gate_semantics_ok()) {
            reason = "Actuation gate semantics violated: synapse_class="
                     + synapse_class + " allows_actuation=" + (allows_actuation ? "true" : "false");
            return false;
        }
        if (!prod_lane_governance_ok()) {
            reason = "PROD lane governance violated: KER corridor or citizen_ready not satisfied";
            return false;
        }
        return true;
    }
};

// Telemetry input, aligned with CyboquaticWorkloadSimulator.
struct CanalNodeTelemetry {
    std::string node_id;
    double flow_rate_m3s;
    double head_loss_m;
    double pump_power_kw;
    double lift_height_m;
    double water_density_kgm3;
    double gravity_ms2;
    double eco_efficiency;
    double delta_v_t;
    double timestamp_seconds;
    double carbon_intensity_gCO2eq_kWh;
};

// Workload result with Lyapunov corridor checks.
struct WorkloadResult {
    double energy_req_j;
    double eco_weighted_energy_j;
    double delta_v_t;
    bool lyapunov_within_corridor;
};

// Energy and ΔVt model (non-actuating analytics reused by the gate).
class CyboquaticWorkloadModel {
public:
    CyboquaticWorkloadModel(double corridor_alpha,
                            double corridor_beta,
                            double max_allowed_delta_v)
        : alpha_(corridor_alpha),
          beta_(corridor_beta),
          max_allowed_delta_v_(max_allowed_delta_v),
          last_timestamp_(std::numeric_limits<double>::quiet_NaN()),
          cumulative_energy_j_(0.0),
          cumulative_eco_energy_j_(0.0),
          cumulative_delta_v_t_(0.0) {}

    WorkloadResult step(const CanalNodeTelemetry &telemetry) {
        double dt = compute_dt(telemetry.timestamp_seconds);
        double hydraulic_energy_j = telemetry.water_density_kgm3 *
                                    telemetry.gravity_ms2 *
                                    telemetry.flow_rate_m3s *
                                    telemetry.lift_height_m *
                                    dt;
        if (hydraulic_energy_j < 0.0) {
            hydraulic_energy_j = 0.0;
        }

        // Eco-weighting: incorporate eco_efficiency and grid carbon intensity.
        // Higher eco_efficiency and lower carbon intensity reduce eco_weighted_energy_j.
        double eco_factor = telemetry.eco_efficiency;
        if (eco_factor < 0.0) eco_factor = 0.0;
        if (eco_factor > 1.0) eco_factor = 1.0;

        // Normalize carbon intensity (assume 0..1000 gCO2eq/kWh range) to 0..1 risk factor.
        double carbon_norm = telemetry.carbon_intensity_gCO2eq_kWh / 1000.0;
        if (carbon_norm < 0.0) carbon_norm = 0.0;
        if (carbon_norm > 1.0) carbon_norm = 1.0;

        double eco_weight = alpha_ * eco_factor * (1.0 - carbon_norm);
        if (eco_weight < 0.0) eco_weight = 0.0;

        double eco_weighted_energy_j = hydraulic_energy_j * (1.0 + beta_ * (1.0 - eco_weight));

        double delta_v_t_step = beta_ * eco_weighted_energy_j;
        if (delta_v_t_step > max_allowed_delta_v_) {
            delta_v_t_step = max_allowed_delta_v_;
        }

        cumulative_energy_j_ += hydraulic_energy_j;
        cumulative_eco_energy_j_ += eco_weighted_energy_j;
        cumulative_delta_v_t_ += delta_v_t_step;
        last_timestamp_ = telemetry.timestamp_seconds;

        WorkloadResult result;
        result.energy_req_j = hydraulic_energy_j;
        result.eco_weighted_energy_j = eco_weighted_energy_j;
        result.delta_v_t = delta_v_t_step;
        result.lyapunov_within_corridor = (delta_v_t_step <= max_allowed_delta_v_);
        return result;
    }

    double cumulative_energy_j() const {
        return cumulative_energy_j_;
    }

    double cumulative_eco_energy_j() const {
        return cumulative_eco_energy_j_;
    }

    double cumulative_delta_v_t() const {
        return cumulative_delta_v_t_;
    }

private:
    double alpha_;
    double beta_;
    double max_allowed_delta_v_;
    double last_timestamp_;
    double cumulative_energy_j_;
    double cumulative_eco_energy_j_;
    double cumulative_delta_v_t_;

    double compute_dt(double current_timestamp) const {
        if (std::isnan(last_timestamp_)) {
            return 1.0;
        }
        double dt = current_timestamp - last_timestamp_;
        if (dt <= 0.0) {
            return 1.0;
        }
        return dt;
    }
};

// Actuation decision result, including reasons for traceability.
struct ActuationDecision {
    bool actuation_allowed;
    std::string reason;
    WorkloadResult workload;
    SynapseEndpointGovernance governance;
};

// Simple CSV reader: expects header row and then numeric fields in fixed order.
class CsvTelemetryReader {
public:
    explicit CsvTelemetryReader(const std::string &path)
        : path_(path) {}

    bool read_all(std::vector<CanalNodeTelemetry> &out_series, std::string &error) const {
        std::ifstream in(path_);
        if (!in) {
            error = "Failed to open telemetry CSV: " + path_;
            return false;
        }

        std::string line;
        if (!std::getline(in, line)) {
            error = "Telemetry CSV is empty: " + path_;
            return false;
        }
        // Skip header line.

        while (std::getline(in, line)) {
            if (line.empty()) continue;
            CanalNodeTelemetry t{};
            if (!parse_line(line, t, error)) {
                return false;
            }
            out_series.push_back(t);
        }

        if (out_series.empty()) {
            error = "Telemetry CSV contains no data rows: " + path_;
            return false;
        }
        return true;
    }

private:
    std::string path_;

    static bool parse_line(const std::string &line, CanalNodeTelemetry &telemetry, std::string &error) {
        std::istringstream ss(line);
        std::string field;
        std::vector<std::string> fields;
        while (std::getline(ss, field, ',')) {
            fields.push_back(field);
        }

        // Expected columns:
        // node_id,flow_rate_m3s,head_loss_m,pump_power_kw,lift_height_m,water_density_kgm3,
        // gravity_ms2,eco_efficiency,delta_v_t,timestamp_seconds,carbon_intensity_gCO2eq_kWh
        if (fields.size() != 11) {
            error = "Telemetry CSV row has " + std::to_string(fields.size()) + " fields, expected 11";
            return false;
        }

        telemetry.node_id = fields[0];
        try {
            telemetry.flow_rate_m3s = std::stod(fields[1]);
            telemetry.head_loss_m = std::stod(fields[2]);
            telemetry.pump_power_kw = std::stod(fields[3]);
            telemetry.lift_height_m = std::stod(fields[4]);
            telemetry.water_density_kgm3 = std::stod(fields[5]);
            telemetry.gravity_ms2 = std::stod(fields[6]);
            telemetry.eco_efficiency = std::stod(fields[7]);
            telemetry.delta_v_t = std::stod(fields[8]);
            telemetry.timestamp_seconds = std::stod(fields[9]);
            telemetry.carbon_intensity_gCO2eq_kWh = std::stod(fields[10]);
        } catch (const std::exception &e) {
            error = std::string("Failed to parse numeric telemetry field: ") + e.what();
            return false;
        }
        return true;
    }
};

// ACTUATION_GATE synapse: applies governance + Lyapunov + KER + carbon-aware corridor.
class ActuationGateSynapse {
public:
    ActuationGateSynapse()
        : governance_(),
          workload_model_(0.5, 1e-6, 0.05),
          current_v_t_(0.0),
          max_v_t_increase_(0.05) {}

    ActuationDecision evaluate_series(const std::vector<CanalNodeTelemetry> &series) {
        ActuationDecision decision{};
        decision.actuation_allowed = false;
        decision.reason = "UNINITIALIZED";
        decision.governance = governance_;

        std::string gov_reason;
        if (!governance_.invariants_ok(gov_reason)) {
            decision.reason = "Governance invariants violated: " + gov_reason;
            return decision;
        }

        if (series.empty()) {
            decision.reason = "No telemetry samples available";
            return decision;
        }

        WorkloadResult last_result{};
        double v_t_accum = current_v_t_;
        for (const auto &sample : series) {
            WorkloadResult r = workload_model_.step(sample);
            last_result = r;
            v_t_accum += r.delta_v_t;

            // Lyapunov corridor: ΔVt per step and global corridor non-increase.
            if (!r.lyapunov_within_corridor) {
                decision.reason = "Lyapunov corridor breach: delta_v_t_step=" +
                                  std::to_string(r.delta_v_t) +
                                  " exceeds max_allowed_delta_v=" +
                                  std::to_string(max_v_t_increase_);
                decision.workload = r;
                return decision;
            }

            if (v_t_accum - current_v_t_ > max_v_t_increase_) {
                decision.reason = "Global Lyapunov residual breach: V_t increase=" +
                                  std::to_string(v_t_accum - current_v_t_) +
                                  " exceeds corridor=" + std::to_string(max_v_t_increase_);
                decision.workload = r;
                return decision;
            }

            // Carbon-aware corridor: prohibit actuation in high-carbon regime.
            if (sample.carbon_intensity_gCO2eq_kWh > 600.0) {
                decision.reason = "Carbon intensity corridor breach: carbon_intensity=" +
                                  std::to_string(sample.carbon_intensity_gCO2eq_kWh) +
                                  " gCO2eq/kWh above threshold 600";
                decision.workload = r;
                return decision;
            }
        }

        current_v_t_ = v_t_accum;
        decision.workload = last_result;

        // KER corridor: actuation only if ker_s positive and sufficiently high.
        if (governance_.ker.ker_s <= 0.3) {
            decision.reason = "KER corridor negative or too low: ker_s=" +
                              std::to_string(governance_.ker.ker_s);
            return decision;
        }

        // All invariants satisfied: actuation allowed.
        decision.actuation_allowed = true;
        decision.reason = "Actuation allowed: KER corridor positive, Lyapunov and carbon corridors satisfied";
        return decision;
    }

private:
    SynapseEndpointGovernance governance_;
    CyboquaticWorkloadModel workload_model_;
    double current_v_t_;
    double max_v_t_increase_;
};

// CLI interface: read telemetry CSV, evaluate actuation gate, print CSV result.
int cli_main(int argc, char **argv) {
    if (argc != 2) {
        std::cerr << "Usage: actuation_gate_synapse <telemetry.csv>\n";
        return 1;
    }

    std::string csv_path = argv[1];
    CsvTelemetryReader reader(csv_path);
    std::vector<CanalNodeTelemetry> series;
    std::string error;
    if (!reader.read_all(series, error)) {
        std::cerr << "Error: " << error << "\n";
        return 1;
    }

    ActuationGateSynapse gate;
    ActuationDecision decision = gate.evaluate_series(series);

    // Output a small CSV for cross-language synapse clients (Java, Lua, Kotlin).
    // Columns: actuation_allowed,reason,energy_req_j,eco_weighted_energy_j,delta_v_t,ker_k,ker_e,ker_r,ker_s,lane
    std::cout << "actuation_allowed,reason,energy_req_j,eco_weighted_energy_j,delta_v_t,"
                 "ker_k,ker_e,ker_r,ker_s,lane\n";
    std::cout << (decision.actuation_allowed ? "1" : "0") << ","
              << "\"" << decision.reason << "\","
              << decision.workload.energy_req_j << ","
              << decision.workload.eco_weighted_energy_j << ","
              << decision.workload.delta_v_t << ","
              << decision.governance.ker.ker_k << ","
              << decision.governance.ker.ker_e << ","
              << decision.governance.ker.ker_r << ","
              << decision.governance.ker.ker_s << ","
              << lane_to_string(decision.governance.lane_default)
              << "\n";

    return decision.actuation_allowed ? 0 : 2;
}

} // namespace eco_restoration

int main(int argc, char **argv) {
    return eco_restoration::cli_main(argc, argv);
}
