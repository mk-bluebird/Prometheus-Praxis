// File: cpp/simulation/cyboquatic_workload_model.cpp
#include <iostream>
#include <vector>
#include <string>
#include <cmath>
#include <chrono>
#include <random>
#include <sstream>
#include <iomanip>

/**
 * Cyboquatic workload model for ecological restoration machinery.
 *
 * Models energy requirements (energyreqJ) and terminal velocity shifts (ΔVt)
 * for pump-drive and aeration workloads in cyboquatic basins.
 *
 * Design goals:
 * - Simple, verifiable physics-inspired model.
 * - Energy-efficiency and carbon-negative operation via optimization hooks.
 * - Ready for linkage with SQL telemetry (see telemetry schema).
 */

namespace cyboquatic {

struct WorkloadSample {
    std::string basin_id;
    double timestamp_s;
    double flow_rate_m3_s;      // Instantaneous flow rate of treated water.
    double head_m;              // Pump head or elevation difference.
    double motor_efficiency;    // Fraction [0,1].
    double aeration_factor;     // 0..1 scaling for aeration workload.
    double energyreqJ;          // Computed energy requirement in Joules.
    double deltaVt_m_s;         // Computed change in terminal velocity (m/s).
};

class CyboquaticWorkloadModel {
public:
    CyboquaticWorkloadModel(double water_density_kg_m3 = 1000.0,
                            double gravity_m_s2 = 9.80665)
        : rho(water_density_kg_m3),
          g(gravity_m_s2)
    {}

    WorkloadSample compute_sample(const std::string& basin_id,
                                  double timestamp_s,
                                  double flow_rate_m3_s,
                                  double head_m,
                                  double motor_efficiency,
                                  double aeration_factor) const
    {
        WorkloadSample s{};
        s.basin_id = basin_id;
        s.timestamp_s = timestamp_s;
        s.flow_rate_m3_s = flow_rate_m3_s;
        s.head_m = head_m;
        s.motor_efficiency = clamp(motor_efficiency, 0.1, 1.0);
        s.aeration_factor = clamp(aeration_factor, 0.0, 1.0);

        // Pump power requirement: P = rho * g * Q * H / eta
        // Approximate short-interval energy: E = P * dt, assume dt = 1 s for telemetry sampling.
        double power_W = rho * g * flow_rate_m3_s * head_m / s.motor_efficiency;
        double energy_J = power_W; // dt = 1 s

        // Aeration workload modeled as additional fraction on energy.
        double aeration_multiplier = 1.0 + 0.5 * s.aeration_factor;
        s.energyreqJ = energy_J * aeration_multiplier;

        // ΔVt estimates change in effective terminal velocity due to aeration turbulence
        // Simple model: proportional to aeration_factor and square root of head.
        s.deltaVt_m_s = 0.05 * s.aeration_factor * std::sqrt(std::max(head_m, 0.0));

        return s;
    }

    std::vector<WorkloadSample> simulate_timeseries(const std::string& basin_id,
                                                    double start_timestamp_s,
                                                    double duration_s,
                                                    double base_flow_m3_s,
                                                    double base_head_m,
                                                    double motor_efficiency,
                                                    double aeration_factor,
                                                    double sampling_interval_s) const
    {
        std::vector<WorkloadSample> series;
        if (sampling_interval_s <= 0.0 || duration_s <= 0.0) {
            return series;
        }

        std::mt19937_64 rng(static_cast<unsigned long long>(
            std::chrono::high_resolution_clock::now().time_since_epoch().count()));
        std::normal_distribution<double> flow_noise(0.0, 0.05 * base_flow_m3_s);
        std::normal_distribution<double> head_noise(0.0, 0.05 * base_head_m);

        double current_time = start_timestamp_s;
        double end_time = start_timestamp_s + duration_s;

        while (current_time <= end_time) {
            double flow = std::max(base_flow_m3_s + flow_noise(rng), 0.0);
            double head = std::max(base_head_m + head_noise(rng), 0.0);
            WorkloadSample s = compute_sample(
                basin_id,
                current_time,
                flow,
                head,
                motor_efficiency,
                aeration_factor
            );
            series.push_back(s);
            current_time += sampling_interval_s;
        }
        return series;
    }

    static std::string to_csv(const std::vector<WorkloadSample>& series) {
        std::ostringstream oss;
        oss << "basin_id,timestamp_s,flow_rate_m3_s,head_m,motor_efficiency,"
               "aeration_factor,energyreqJ,deltaVt_m_s\n";
        oss << std::fixed << std::setprecision(6);
        for (const auto& s : series) {
            oss << s.basin_id << ","
                << s.timestamp_s << ","
                << s.flow_rate_m3_s << ","
                << s.head_m << ","
                << s.motor_efficiency << ","
                << s.aeration_factor << ","
                << s.energyreqJ << ","
                << s.deltaVt_m_s << "\n";
        }
        return oss.str();
    }

private:
    double rho;
    double g;

    static double clamp(double v, double lo, double hi) {
        if (v < lo) return lo;
        if (v > hi) return hi;
        return v;
    }
};

} // namespace cyboquatic

int main() {
    cyboquatic::CyboquaticWorkloadModel model;
    auto series = model.simulate_timeseries(
        "basin-A",
        0.0,
        3600.0,   // duration 1 hour
        0.15,     // base flow rate (m^3/s)
        4.0,      // base head (m)
        0.75,     // motor efficiency
        0.6,      // aeration factor
        60.0      // sampling interval (s)
    );

    std::cout << cyboquatic::CyboquaticWorkloadModel::to_csv(series);
    return 0;
}
