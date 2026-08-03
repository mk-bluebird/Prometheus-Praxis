// File: cpp/simulation/cyboquatic_workload_simulator.cpp
// Destination: mk-bluebird/Prometheus-Praxis/cpp/simulation/cyboquatic_workload_simulator.cpp

#include <iostream>
#include <vector>
#include <string>
#include <cmath>
#include <limits>
#include <chrono>
#include <iomanip>

namespace cyboquatic {

struct CanalNodeTelemetry {
    std::string node_id;
    double flow_rate_m3s;          // instantaneous flow rate
    double head_loss_m;            // hydraulic head loss at node
    double pump_power_kw;          // electrical power demand
    double lift_height_m;          // water lift height
    double water_density_kgm3;     // usually ~1000 kg/m^3
    double gravity_ms2;           // use 9.81 m/s^2
    double eco_efficiency;         // 0..1, corridor-governed eco-efficiency
    double delta_v_t;              // workload-specific ΔVt contribution (dimensionless risk drift)
    double timestamp_seconds;      // monotonic time
};

struct WorkloadResult {
    double energy_req_j;           // instantaneous energy requirement in Joules per second step
    double eco_weighted_energy_j;  // eco-adjusted energy (lower is better)
    double delta_v_t;              // risk residual change
};

class CyboquaticWorkloadSimulator {
public:
    CyboquaticWorkloadSimulator(double corridor_alpha,
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
        validate_telemetry(telemetry);

        double dt = compute_dt(telemetry.timestamp_seconds);
        // Hydraulic lifting energy per second: E = rho * g * Q * H * dt
        double hydraulic_energy_j = telemetry.water_density_kgm3 *
                                    telemetry.gravity_ms2 *
                                    telemetry.flow_rate_m3s *
                                    telemetry.lift_height_m *
                                    dt;

        // Electrical energy per second: E = P * 1000 * dt (kW to W)
        double electrical_energy_j = telemetry.pump_power_kw * 1000.0 * dt;

        double energy_req_j = hydraulic_energy_j + electrical_energy_j;

        // eco_efficiency in [0,1]; eco_weighted energy penalizes low efficiency
        double eco_factor = 1.0 + alpha_ * (1.0 - clamp01(telemetry.eco_efficiency));
        double eco_weighted_energy_j = energy_req_j * eco_factor;

        // ΔVt corridor: we treat beta_ as sensitivity of ΔVt to eco_weighted energy
        double delta_v_t = beta_ * eco_weighted_energy_j;

        // enforce Lyapunov-style bound: ΔVt cannot exceed max_allowed_delta_v_
        if (delta_v_t > max_allowed_delta_v_) {
            delta_v_t = max_allowed_delta_v_;
        }

        cumulative_energy_j_ += energy_req_j;
        cumulative_eco_energy_j_ += eco_weighted_energy_j;
        cumulative_delta_v_t_ += delta_v_t;

        last_timestamp_ = telemetry.timestamp_seconds;

        WorkloadResult result;
        result.energy_req_j = energy_req_j;
        result.eco_weighted_energy_j = eco_weighted_energy_j;
        result.delta_v_t = delta_v_t;
        return result;
    }

    double cumulative_energy() const {
        return cumulative_energy_j_;
    }

    double cumulative_eco_energy() const {
        return cumulative_eco_energy_j_;
    }

    double cumulative_delta_v_t() const {
        return cumulative_delta_v_t_;
    }

    void reset() {
        last_timestamp_ = std::numeric_limits<double>::quiet_NaN();
        cumulative_energy_j_ = 0.0;
        cumulative_eco_energy_j_ = 0.0;
        cumulative_delta_v_t_ = 0.0;
    }

private:
    double alpha_;
    double beta_;
    double max_allowed_delta_v_;
    double last_timestamp_;
    double cumulative_energy_j_;
    double cumulative_eco_energy_j_;
    double cumulative_delta_v_t_;

    static double clamp01(double v) {
        if (v < 0.0) return 0.0;
        if (v > 1.0) return 1.0;
        return v;
    }

    static void validate_telemetry(const CanalNodeTelemetry &t) {
        if (t.flow_rate_m3s < 0.0) {
            throw std::invalid_argument("flow_rate_m3s must be non-negative");
        }
        if (t.lift_height_m < 0.0) {
            throw std::invalid_argument("lift_height_m must be non-negative");
        }
        if (t.pump_power_kw < 0.0) {
            throw std::invalid_argument("pump_power_kw must be non-negative");
        }
        if (t.water_density_kgm3 <= 0.0) {
            throw std::invalid_argument("water_density_kgm3 must be positive");
        }
        if (t.gravity_ms2 <= 0.0) {
            throw std::invalid_argument("gravity_ms2 must be positive");
        }
        if (t.eco_efficiency < 0.0 || t.eco_efficiency > 1.0) {
            throw std::invalid_argument("eco_efficiency must be in [0,1]");
        }
    }

    double compute_dt(double current_timestamp) const {
        if (std::isnan(last_timestamp_)) {
            // first step: assume unit interval
            return 1.0;
        }
        double dt = current_timestamp - last_timestamp_;
        if (dt <= 0.0) {
            // enforce minimal positive dt to keep simulation progressing
            return 1.0;
        }
        return dt;
    }
};

} // namespace cyboquatic

int main() {
    using namespace cyboquatic;

    CyboquaticWorkloadSimulator simulator(
        0.5,   // alpha: eco-efficiency sensitivity
        1e-6,  // beta: ΔVt sensitivity to energy
        0.05   // max allowed ΔVt per step
    );

    std::vector<CanalNodeTelemetry> series;
    auto now = std::chrono::steady_clock::now().time_since_epoch();
    double base_ts = std::chrono::duration<double>(now).count();

    series.push_back(CanalNodeTelemetry{
        "node-A",
        0.4,        // flow_rate_m3s
        0.2,        // head_loss_m
        1.2,        // pump_power_kw
        2.0,        // lift_height_m
        1000.0,     // water_density_kgm3
        9.81,       // gravity_ms2
        0.9,        // eco_efficiency
        0.0,        // delta_v_t (input drift placeholder)
        base_ts
    });

    series.push_back(CanalNodeTelemetry{
        "node-A",
        0.6,
        0.3,
        1.5,
        2.5,
        1000.0,
        9.81,
        0.8,
        0.0,
        base_ts + 60.0
    });

    series.push_back(CanalNodeTelemetry{
        "node-A",
        0.5,
        0.25,
        1.1,
        2.2,
        1000.0,
        9.81,
        0.95,
        0.0,
        base_ts + 120.0
    });

    std::cout << std::fixed << std::setprecision(3);
    for (const auto &telemetry : series) {
        WorkloadResult r = simulator.step(telemetry);
        std::cout << "Node: " << telemetry.node_id
                  << " ts=" << telemetry.timestamp_seconds
                  << " energy_req_j=" << r.energy_req_j
                  << " eco_energy_j=" << r.eco_weighted_energy_j
                  << " delta_v_t=" << r.delta_v_t << "\n";
    }

    std::cout << "Cumulative energy_req_j=" << simulator.cumulative_energy()
              << " cumulative_eco_energy_j=" << simulator.cumulative_eco_energy()
              << " cumulative_delta_v_t=" << simulator.cumulative_delta_v_t()
              << "\n";

    return 0;
}
