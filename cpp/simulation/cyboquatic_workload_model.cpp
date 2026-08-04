// File: cpp/simulation/cyboquatic_workload_model.cpp
#include <iostream>
#include <vector>
#include <string>
#include <cmath>
#include <chrono>
#include <random>
#include <sstream>
#include <iomanip>

namespace cyboquatic {

struct WorkloadSample {
    std::string basin_id;
    std::string h3_hex_id;
    double timestamp_s;
    double flow_rate_m3_s;
    double head_m;
    double motor_efficiency;
    double aeration_factor;
    double ndvi;
    double green_fraction;
    double energyreqJ;
    double deltaVt_m_s;
    double ker_k;
    double ker_e;
    double ker_r;
};

class CyboquaticWorkloadModel {
public:
    CyboquaticWorkloadModel(double water_density_kg_m3 = 1000.0,
                            double gravity_m_s2 = 9.80665,
                            double ndvi_min = 0.1,
                            double ndvi_max = 0.7,
                            double alpha_deltaVt = 0.04,
                            double beta_deltaVt = 0.8,
                            double alpha_ker_e = 1e-6)
        : rho_(water_density_kg_m3),
          g_(gravity_m_s2),
          ndvi_min_(ndvi_min),
          ndvi_max_(ndvi_max),
          alpha_deltaVt_(alpha_deltaVt),
          beta_deltaVt_(beta_deltaVt),
          alpha_ker_e_(alpha_ker_e)
    {}

    WorkloadSample compute_sample(const std::string& basin_id,
                                  const std::string& h3_hex_id,
                                  double timestamp_s,
                                  double flow_rate_m3_s,
                                  double head_m,
                                  double motor_efficiency,
                                  double aeration_factor,
                                  double ndvi,
                                  double ker_k,
                                  double ker_r) const
    {
        WorkloadSample s{};
        s.basin_id = basin_id;
        s.h3_hex_id = h3_hex_id;
        s.timestamp_s = timestamp_s;
        s.flow_rate_m3_s = flow_rate_m3_s;
        s.head_m = head_m;
        s.motor_efficiency = clamp(motor_efficiency, 0.1, 1.0);
        s.aeration_factor = clamp(aeration_factor, 0.0, 1.0);
        s.ndvi = ndvi;
        s.green_fraction = ndvi_to_green_fraction(ndvi);
        s.ker_k = clamp(ker_k, 0.0, 1.0);
        s.ker_r = clamp(ker_r, 0.0, 1.0);

        double power_W = rho_ * g_ * s.flow_rate_m3_s * s.head_m / s.motor_efficiency;
        double energy_J = power_W;

        double aeration_multiplier = 1.0 + 0.5 * s.aeration_factor;
        s.energyreqJ = energy_J * aeration_multiplier;

        double gf = s.green_fraction > 0.0 ? s.green_fraction : 0.0;
        double head_clamped = s.head_m > 0.0 ? s.head_m : 0.0;
        s.deltaVt_m_s = alpha_deltaVt_ * std::pow(gf, beta_deltaVt_) * std::sqrt(head_clamped);

        s.ker_e = -alpha_ker_e_ * s.energyreqJ;
        return s;
    }

    std::vector<WorkloadSample> simulate_timeseries(const std::string& basin_id,
                                                    const std::string& h3_hex_id,
                                                    double start_timestamp_s,
                                                    double duration_s,
                                                    double base_flow_m3_s,
                                                    double base_head_m,
                                                    double motor_efficiency,
                                                    double aeration_factor,
                                                    double ndvi,
                                                    double ker_k,
                                                    double ker_r,
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
        const double end_time = start_timestamp_s + duration_s;

        while (current_time <= end_time) {
            double flow = base_flow_m3_s + flow_noise(rng);
            if (flow < 0.0) flow = 0.0;

            double head = base_head_m + head_noise(rng);
            if (head < 0.0) head = 0.0;

            WorkloadSample s = compute_sample(
                basin_id,
                h3_hex_id,
                current_time,
                flow,
                head,
                motor_efficiency,
                aeration_factor,
                ndvi,
                ker_k,
                ker_r
            );
            series.push_back(s);
            current_time += sampling_interval_s;
        }
        return series;
    }

    static std::string to_csv(const std::vector<WorkloadSample>& series) {
        std::ostringstream oss;
        oss << "basin_id,h3_hex_id,timestamp_s,flow_rate_m3_s,head_m,"
               "motor_efficiency,aeration_factor,ndvi,green_fraction,"
               "energyreqJ,deltaVt_m_s,ker_k,ker_e,ker_r\n";
        oss << std::fixed << std::setprecision(6);
        for (const auto& s : series) {
            oss << s.basin_id << ","
                << s.h3_hex_id << ","
                << s.timestamp_s << ","
                << s.flow_rate_m3_s << ","
                << s.head_m << ","
                << s.motor_efficiency << ","
                << s.aeration_factor << ","
                << s.ndvi << ","
                << s.green_fraction << ","
                << s.energyreqJ << ","
                << s.deltaVt_m_s << ","
                << s.ker_k << ","
                << s.ker_e << ","
                << s.ker_r << "\n";
        }
        return oss.str();
    }

private:
    double rho_;
    double g_;
    double ndvi_min_;
    double ndvi_max_;
    double alpha_deltaVt_;
    double beta_deltaVt_;
    double alpha_ker_e_;

    static double clamp(double v, double lo, double hi) {
        if (v < lo) return lo;
        if (v > hi) return hi;
        return v;
    }

    double ndvi_to_green_fraction(double ndvi) const {
        double g = (ndvi - ndvi_min_) / (ndvi_max_ - ndvi_min_);
        if (g < 0.0) g = 0.0;
        if (g > 1.0) g = 1.0;
        return g;
    }
};

} // namespace cyboquatic

int main() {
    cyboquatic::CyboquaticWorkloadModel model;
    auto series = model.simulate_timeseries(
        "basin-A",
        "8726348ffffffff",
        0.0,
        3600.0,
        0.15,
        4.0,
        0.80,
        0.6,
        0.45,
        0.9,
        0.2,
        60.0
    );
    std::cout << cyboquatic::CyboquaticWorkloadModel::to_csv(series);
    return 0;
}
