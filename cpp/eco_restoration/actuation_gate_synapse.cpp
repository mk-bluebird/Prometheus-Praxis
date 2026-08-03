// File: cpp/eco_restoration/actuation_gate_synapse.cpp
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <limits>
#include <cmath>
#include <sqlite3.h>

extern "C" {

static double ker_scalar(double k, double e, double r) {
    if (k < 0.0) k = 0.0;
    if (k > 1.0) k = 1.0;
    if (e < 0.0) e = 0.0;
    if (e > 1.0) e = 1.0;
    if (r < 0.0) r = 0.0;
    if (r > 1.0) r = 1.0;
    return k * e - r;
}

static double carbon_corridor(double ci, double max_carbon) {
    if (max_carbon <= 0.0) return 0.0;
    double c = 1.0 - ci / max_carbon;
    if (c < 0.0) c = 0.0;
    if (c > 1.0) c = 1.0;
    return c;
}

// JNI-facing corridor-safe actuation evaluation for a series of samples.
void ActuationGateSynapse_evaluate_series(
    const double* k_arr,
    const double* e_arr,
    const double* r_arr,
    const double* eco_energy_arr,
    const double* eco_eff_arr,
    const double* ci_arr,
    const double* max_carbon_arr,
    int len,
    double alpha,
    double beta,
    double gamma,
    double delta,
    double delta_v_max,
    double* out_ker_s,
    double* out_carbon_corridor,
    double* out_delta_v_t,
    int* out_allowed
) {
    for (int i = 0; i < len; ++i) {
        double k = k_arr[i];
        double e = e_arr[i];
        double r = r_arr[i];
        double eco_energy = eco_energy_arr[i];
        double eco_eff = eco_eff_arr[i];
        double ci = ci_arr[i];
        double maxC = max_carbon_arr[i];

        double s = ker_scalar(k, e, r);
        double c = carbon_corridor(ci, maxC);

        double eff = eco_eff;
        if (eff < 0.0) eff = 0.0;
        if (eff > 1.0) eff = 1.0;
        double E_eco = eco_energy * (1.0 + alpha * (1.0 - eff));

        double dV = beta * E_eco;
        double bound_ker = gamma * s;
        double bound_carbon = delta * c;
        double upper = delta_v_max;
        if (bound_ker < upper) upper = bound_ker;
        if (bound_carbon < upper) upper = bound_carbon;
        if (dV > upper) dV = upper;
        if (dV < 0.0) dV = 0.0;

        out_ker_s[i] = s;
        out_carbon_corridor[i] = c;
        out_delta_v_t[i] = dV;
        out_allowed[i] = (dV <= upper && s >= 0.0 && c > 0.0) ? 1 : 0;
    }
}

} // extern "C"

namespace eco_restoration {

struct HexResidualState {
    double total_delta_v_t;
    double v_corridor_max;

    HexResidualState() : total_delta_v_t(0.0), v_corridor_max(0.0) {}
};

static HexResidualState loadHexResidualFromSQLite(sqlite3* db, const std::string& hex_id) {
    HexResidualState state;

    const char* sql =
        "SELECT total_delta_v_t, v_corridor_max "
        "FROM v_hex_stability_ker_dvt_carbon "
        "WHERE hex_id = ?";

    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        return state;
    }

    sqlite3_bind_text(stmt, 1, hex_id.c_str(), -1, SQLITE_STATIC);

    if (sqlite3_step(stmt) == SQLITE_ROW) {
        state.total_delta_v_t = sqlite3_column_double(stmt, 0);
        const unsigned char* vtmax = sqlite3_column_text(stmt, 1);
        if (vtmax) {
            try {
                state.v_corridor_max = std::stod(reinterpret_cast<const char*>(vtmax));
            } catch (...) {
                state.v_corridor_max = 0.1;
            }
        } else {
            state.v_corridor_max = 0.1;
        }
    }

    sqlite3_finalize(stmt);
    return state;
}

static bool corridor_safe(double dvt_pred,
                          const HexResidualState& state,
                          double net_margin,
                          double hex_margin) {
    double vt_after = state.total_delta_v_t + dvt_pred;
    return vt_after <= state.v_corridor_max + hex_margin && dvt_pred <= net_margin;
}

struct KerProfile {
    double ker_k;
    double ker_e;
    double ker_r;
    double ker_s;

    KerProfile(double k = 0.0, double e = 0.0, double r = 0.0)
        : ker_k(k), ker_e(e), ker_r(r), ker_s(k * e - r) {}

    void recompute() {
        ker_s = ker_k * ker_e - ker_r;
    }
};

enum class Lane {
    RESEARCH,
    EXPPROD,
    PROD
};

static Lane lane_from_string(const std::string& s) {
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

struct SynapseEndpointGovernance {
    std::string relpath;
    std::string synapse_class;
    bool allows_actuation;
    bool non_actuating;
    bool neuro_flag;
    bool citizen_ready;
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

    bool ker_scalar_consistency() const {
        double expected = ker.ker_k * ker.ker_e - ker.ker_r;
        return std::fabs(expected - ker.ker_s) < 1e-9;
    }

    bool prod_lane_governance_ok() const {
        if (lane_default != Lane::PROD) {
            return true;
        }
        return ker.ker_s > 0.2 && ker.ker_e >= 0.7 && ker.ker_r <= 0.5 && citizen_ready;
    }

    bool actuation_gate_semantics_ok() const {
        if (synapse_class != "ACTUATION_GATE") {
            return !allows_actuation;
        }
        return allows_actuation && !non_actuating && ker.ker_s > 0.3;
    }

    bool invariants_ok(std::string& reason) const {
        if (!ker_scalar_consistency()) {
            reason = "KER scalar consistency violated: ker_s != ker_k * ker_e - ker_r";
            return false;
        }
        if (!actuation_gate_semantics_ok()) {
            reason = "Actuation gate semantics violated";
            return false;
        }
        if (!prod_lane_governance_ok()) {
            reason = "PROD lane governance violated";
            return false;
        }
        return true;
    }
};

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

struct WorkloadResult {
    double energy_req_j;
    double eco_weighted_energy_j;
    double delta_v_t;
    bool lyapunov_within_corridor;
};

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

    WorkloadResult step(const CanalNodeTelemetry& telemetry) {
        double dt = compute_dt(telemetry.timestamp_seconds);

        double hydraulic_energy_j =
            telemetry.water_density_kgm3 *
            telemetry.gravity_ms2 *
            telemetry.flow_rate_m3s *
            telemetry.lift_height_m *
            dt;

        if (hydraulic_energy_j < 0.0) {
            hydraulic_energy_j = 0.0;
        }

        double eco_factor = telemetry.eco_efficiency;
        if (eco_factor < 0.0) eco_factor = 0.0;
        if (eco_factor > 1.0) eco_factor = 1.0;

        double carbon_norm = telemetry.carbon_intensity_gCO2eq_kWh / 1000.0;
        if (carbon_norm < 0.0) carbon_norm = 0.0;
        if (carbon_norm > 1.0) carbon_norm = 1.0;

        double eco_weight = alpha_ * eco_factor * (1.0 - carbon_norm);
        if (eco_weight < 0.0) eco_weight = 0.0;

        double eco_weighted_energy_j =
            hydraulic_energy_j * (1.0 + beta_ * (1.0 - eco_weight));

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

struct ActuationDecision {
    bool actuation_allowed;
    std::string reason;
    WorkloadResult workload;
    SynapseEndpointGovernance governance;
};

class CsvTelemetryReader {
public:
    explicit CsvTelemetryReader(const std::string& path)
        : path_(path) {}

    bool read_all(std::vector<CanalNodeTelemetry>& out_series,
                  std::string& error) const {
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

    static bool parse_line(const std::string& line,
                           CanalNodeTelemetry& telemetry,
                           std::string& error) {
        std::istringstream ss(line);
        std::string field;
        std::vector<std::string> fields;
        while (std::getline(ss, field, ',')) {
            fields.push_back(field);
        }

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
        } catch (const std::exception& e) {
            error = std::string("Failed to parse numeric telemetry field: ") + e.what();
            return false;
        }
        return true;
    }
};

class ActuationGateSynapse {
public:
    explicit ActuationGateSynapse(sqlite3* db = nullptr)
        : db_(db),
          governance_(),
          workload_model_(0.5, 1e-6, 0.05),
          current_v_t_(0.0),
          max_v_t_increase_(0.05) {}

    struct ActuationCommand {
        double gate_position;
        bool is_safe_mode;
    };

    ActuationCommand computeHybridControl(const std::string& hex_id,
                                          const std::vector<double>& x_state,
                                          const ActuationCommand& u_lqr) {
        const double NET_DVT_MARGIN = 0.01;
        const double HEX_VT_MARGIN = 0.05;

        double dvt_pred = estimateDeltaVForControl(x_state, u_lqr);

        HexResidualState hex_state;
        if (db_) {
            hex_state = loadHexResidualFromSQLite(db_, hex_id);
        }

        bool safe = corridor_safe(dvt_pred, hex_state, NET_DVT_MARGIN, HEX_VT_MARGIN);

        ActuationCommand u_final;
        if (!safe) {
            u_final = safeControl();
            u_final.is_safe_mode = true;
            logActuationRequest(hex_id,
                                dvt_pred,
                                hex_state.total_delta_v_t,
                                hex_state.total_delta_v_t,
                                false);
        } else {
            u_final = u_lqr;
            u_final.is_safe_mode = false;
            double vt_after = hex_state.total_delta_v_t + dvt_pred;
            logActuationRequest(hex_id,
                                dvt_pred,
                                hex_state.total_delta_v_t,
                                vt_after,
                                true);
        }

        return u_final;
    }

    void logActuationRequest(const std::string& hex_id,
                             double dvt_pred,
                             double vt_before,
                             double vt_after,
                             bool corridor_ok) {
        if (!db_) return;

        const char* sql =
            "INSERT INTO actuation_request "
            "(hex_id, dvt_pred, vt_before, vt_after, corridor_ok) "
            "VALUES (?, ?, ?, ?, ?)";

        sqlite3_stmt* stmt = nullptr;
        int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
        if (rc != SQLITE_OK) {
            return;
        }

        sqlite3_bind_text(stmt, 1, hex_id.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_double(stmt, 2, dvt_pred);
        sqlite3_bind_double(stmt, 3, vt_before);
        sqlite3_bind_double(stmt, 4, vt_after);
        sqlite3_bind_int(stmt, 5, corridor_ok ? 1 : 0);

        rc = sqlite3_step(stmt);
        if (rc != SQLITE_DONE && corridor_ok) {
            std::cerr << "Actuation blocked by Lyapunov-KER corridor trigger\n";
        }

        sqlite3_finalize(stmt);
    }

    ActuationDecision evaluate_series(const std::vector<CanalNodeTelemetry>& series) {
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

        for (const auto& sample : series) {
            WorkloadResult r = workload_model_.step(sample);
            last_result = r;
            v_t_accum += r.delta_v_t;

            if (!r.lyapunov_within_corridor) {
                decision.reason = "Lyapunov corridor breach: delta_v_t_step=" +
                                  std::to_string(r.delta_v_t);
                decision.workload = r;
                return decision;
            }

            if (v_t_accum - current_v_t_ > max_v_t_increase_) {
                decision.reason = "Global Lyapunov residual breach: V_t increase=" +
                                  std::to_string(v_t_accum - current_v_t_);
                decision.workload = r;
                return decision;
            }

            if (sample.carbon_intensity_gCO2eq_kWh > 600.0) {
                decision.reason = "Carbon intensity corridor breach: carbon_intensity=" +
                                  std::to_string(sample.carbon_intensity_gCO2eq_kWh);
                decision.workload = r;
                return decision;
            }
        }

        current_v_t_ = v_t_accum;
        decision.workload = last_result;

        if (decision.governance.ker.ker_s <= 0.3) {
            decision.reason = "KER corridor too low: ker_s=" +
                              std::to_string(decision.governance.ker.ker_s);
            return decision;
        }

        decision.actuation_allowed = true;
        decision.reason = "Actuation allowed: KER, Lyapunov, and carbon corridors satisfied";
        return decision;
    }

private:
    sqlite3* db_;
    SynapseEndpointGovernance governance_;
    CyboquaticWorkloadModel workload_model_;
    double current_v_t_;
    double max_v_t_increase_;

    double estimateDeltaVForControl(const std::vector<double>& x_state,
                                    const ActuationCommand& u) const {
        if (x_state.empty()) return 0.0;
        double state_norm = 0.0;
        for (double x : x_state) {
            state_norm += x * x;
        }
        state_norm = std::sqrt(state_norm);
        double beta = 1e-6;
        return beta * std::fabs(u.gate_position) * state_norm;
    }

    ActuationCommand safeControl() const {
        ActuationCommand cmd;
        cmd.gate_position = 0.0;
        cmd.is_safe_mode = true;
        return cmd;
    }
};

int cli_main(int argc, char** argv) {
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

int main(int argc, char** argv) {
    return eco_restoration::cli_main(argc, argv);
}
