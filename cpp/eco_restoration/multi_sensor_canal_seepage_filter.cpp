// File: cpp/eco_restoration/multi_sensor_canal_seepage_filter.cpp
#include <Eigen/Dense>

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <vector>

namespace eco_restoration {

struct GaugePair {
    double upstream_flow_m3_s{};
    double downstream_flow_m3_s{};
    double upstream_standard_deviation{};
    double downstream_standard_deviation{};
};

struct ReachEstimate {
    double storage_m3{};
    double seepage_m3_s{};
    double reach_averaged_flow_m3_s{};
    double seepage_standard_deviation{};
    double flow_standard_deviation{};
    double flow_ci95_low_m3_s{};
    double flow_ci95_high_m3_s{};
    double knowledge_factor{};
    double eco_impact_value{};
};

class MultiSensorCanalSeepageFilter {
public:
    MultiSensorCanalSeepageFilter(double initial_storage_m3,
                                  double initial_seepage_m3_s,
                                  double initial_flow_m3_s)
        : state_(initial_storage_m3, initial_seepage_m3_s, initial_flow_m3_s) {
        if (initial_storage_m3 < 0.0 || initial_flow_m3_s < 0.0) {
            throw std::invalid_argument("initial storage and flow must be non-negative");
        }
        covariance_.setZero();
        covariance_.diagonal() << 25.0, 0.04, 1.0;
    }

    ReachEstimate update(double observed_storage_m3, double storage_standard_deviation,
                         double dt_s, const std::vector<GaugePair>& pairs) {
        if (observed_storage_m3 < 0.0 || storage_standard_deviation <= 0.0 ||
            dt_s <= 0.0 || pairs.empty()) {
            throw std::invalid_argument("invalid reach observation");
        }
        for (const auto& pair : pairs) {
            if (pair.upstream_flow_m3_s < 0.0 || pair.downstream_flow_m3_s < 0.0 ||
                pair.upstream_standard_deviation <= 0.0 ||
                pair.downstream_standard_deviation <= 0.0) {
                throw std::invalid_argument("invalid gauge pair");
            }
        }

        Eigen::Matrix3d transition = Eigen::Matrix3d::Identity();
        transition(0, 1) = -dt_s;
        Eigen::Matrix3d process_noise = Eigen::Matrix3d::Zero();
        process_noise.diagonal() << 0.10 * dt_s, 0.0004 * dt_s, 0.02 * dt_s;

        state_ = transition * state_;
        covariance_ = transition * covariance_ * transition.transpose() + process_noise;

        const Eigen::Index rows = 1 + static_cast<Eigen::Index>(pairs.size()) * 2;
        Eigen::MatrixXd measurement = Eigen::MatrixXd::Zero(rows, 3);
        Eigen::VectorXd observations(rows);
        Eigen::MatrixXd noise = Eigen::MatrixXd::Zero(rows, rows);

        measurement(0, 0) = 1.0;
        observations(0) = observed_storage_m3;
        noise(0, 0) = storage_standard_deviation * storage_standard_deviation;

        for (std::size_t i = 0; i < pairs.size(); ++i) {
            const Eigen::Index upstream_row = 1 + static_cast<Eigen::Index>(i) * 2;
            const Eigen::Index downstream_row = upstream_row + 1;
            const auto& pair = pairs[i];

            measurement(upstream_row, 1) = 0.5;
            measurement(upstream_row, 2) = 1.0;
            observations(upstream_row) = pair.upstream_flow_m3_s;
            noise(upstream_row, upstream_row) =
                pair.upstream_standard_deviation * pair.upstream_standard_deviation;

            measurement(downstream_row, 1) = -0.5;
            measurement(downstream_row, 2) = 1.0;
            observations(downstream_row) = pair.downstream_flow_m3_s;
            noise(downstream_row, downstream_row) =
                pair.downstream_standard_deviation * pair.downstream_standard_deviation;
        }

        const Eigen::VectorXd innovation = observations - measurement * state_;
        const Eigen::MatrixXd innovation_covariance =
            measurement * covariance_ * measurement.transpose() + noise;
        const Eigen::MatrixXd gain =
            covariance_ * measurement.transpose() * innovation_covariance.ldlt().solve(
                Eigen::MatrixXd::Identity(rows, rows));

        state_ += gain * innovation;
        const Eigen::Matrix3d identity = Eigen::Matrix3d::Identity();
        covariance_ = (identity - gain * measurement) * covariance_ *
                          (identity - gain * measurement).transpose() +
                      gain * noise * gain.transpose();

        const double seepage_sd = std::sqrt(std::max(0.0, covariance_(1, 1)));
        const double flow_sd = std::sqrt(std::max(0.0, covariance_(2, 2)));
        const double knowledge = std::clamp(
            1.0 / (1.0 + flow_sd + seepage_sd + storage_standard_deviation), 0.0, 1.0);
        const double eco_impact = std::clamp(
            knowledge * (1.0 - std::min(1.0, std::abs(state_(1)) / 2.0)), 0.0, 1.0);

        return {state_(0), state_(1), state_(2), seepage_sd, flow_sd,
                state_(2) - 1.96 * flow_sd, state_(2) + 1.96 * flow_sd,
                knowledge, eco_impact};
    }

private:
    Eigen::Vector3d state_;
    Eigen::Matrix3d covariance_;
};

}  // namespace eco_restoration
