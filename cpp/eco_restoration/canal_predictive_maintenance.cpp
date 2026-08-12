// File: cpp/eco_restoration/canal_predictive_maintenance.cpp
#include <Eigen/Dense>

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <vector>

namespace eco_restoration {

struct PumpTelemetry {
    double runtime_hours{};
    double vibration_rms_mm_s{};
    double temperature_c{};
    double observed_remaining_hours{};
};

struct MaintenanceEstimate {
    double remaining_useful_hours{};
    double maintenance_risk{};
    double combined_lane_risk{};
    double knowledge_factor{};
    double eco_impact_value{};
};

class PumpRemainingUsefulLifeModel {
public:
    void fit(const std::vector<PumpTelemetry>& samples, double ridge = 1e-6) {
        if (samples.size() < 4 || ridge < 0.0) throw std::invalid_argument("insufficient maintenance samples");
        Eigen::MatrixXd x(samples.size(), 4);
        Eigen::VectorXd y(samples.size());
        for (std::size_t i = 0; i < samples.size(); ++i) {
            const auto& s = samples[i];
            if (s.runtime_hours < 0.0 || s.vibration_rms_mm_s < 0.0 ||
                s.observed_remaining_hours < 0.0) throw std::invalid_argument("invalid pump telemetry");
            x.row(i) << 1.0, s.runtime_hours, s.vibration_rms_mm_s, s.temperature_c;
            y(i) = s.observed_remaining_hours;
        }
        coefficients_ = (x.transpose() * x + ridge * Eigen::Matrix4d::Identity())
                            .ldlt().solve(x.transpose() * y);
        fitted_ = true;
    }

    MaintenanceEstimate assess(double runtime_hours, double vibration_rms_mm_s,
                               double temperature_c, double nominal_remaining_hours,
                               double existing_lane_risk, double telemetry_confidence) const {
        if (!fitted_ || runtime_hours < 0.0 || vibration_rms_mm_s < 0.0 ||
            nominal_remaining_hours <= 0.0 || existing_lane_risk < 0.0 ||
            telemetry_confidence < 0.0 || telemetry_confidence > 1.0)
            throw std::invalid_argument("invalid maintenance assessment");

        const double predicted = std::max(0.0, (Eigen::Vector4d(
            1.0, runtime_hours, vibration_rms_mm_s, temperature_c).transpose() * coefficients_)(0));
        const double maintenance_risk = std::clamp(
            1.0 - predicted / nominal_remaining_hours, 0.0, 1.0);
        const double lane_risk = std::max(std::clamp(existing_lane_risk, 0.0, 1.0), maintenance_risk);
        const double knowledge = std::clamp(telemetry_confidence *
            std::min(1.0, samples_fit_quality_), 0.0, 1.0);
        return {predicted, maintenance_risk, lane_risk, knowledge,
                knowledge * (1.0 - lane_risk)};
    }

    void set_fit_quality(double r_squared) {
        samples_fit_quality_ = std::clamp(r_squared, 0.0, 1.0);
    }

private:
    Eigen::Vector4d coefficients_ = Eigen::Vector4d::Zero();
    double samples_fit_quality_{0.0};
    bool fitted_{false};
};

}  // namespace eco_restoration
