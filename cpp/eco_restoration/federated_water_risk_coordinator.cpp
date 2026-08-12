// File: cpp/eco_restoration/federated_water_risk_coordinator.cpp
#include <algorithm>
#include <array>
#include <cmath>
#include <stdexcept>
#include <string>
#include <vector>

namespace eco_restoration {

struct WaterRiskModel {
    std::array<double, 6> coefficients{};
    std::uint64_t model_version{};
};

struct SiteModelUpdate {
    std::string site_id;
    std::uint64_t base_version{};
    std::uint64_t training_samples{};
    std::array<double, 6> coefficients{};
    double validation_loss{};
};

class FederatedWaterRiskCoordinator {
public:
    explicit FederatedWaterRiskCoordinator(WaterRiskModel initial) : global_(initial) {}

    WaterRiskModel aggregate(const std::vector<SiteModelUpdate>& updates,
                             std::size_t minimum_sites = 2) {
        if (updates.size() < minimum_sites) throw std::invalid_argument("insufficient site updates");

        std::array<double, 6> weighted_sum{};
        double total_weight = 0.0;
        for (const auto& update : updates) {
            if (update.site_id.empty() || update.base_version != global_.model_version ||
                update.training_samples == 0 || update.validation_loss < 0.0 ||
                !std::all_of(update.coefficients.begin(), update.coefficients.end(),
                             [](double value) { return std::isfinite(value); }))
                throw std::invalid_argument("invalid federated model update");

            const double weight = static_cast<double>(update.training_samples) /
                                  (1.0 + update.validation_loss);
            for (std::size_t i = 0; i < weighted_sum.size(); ++i)
                weighted_sum[i] += weight * update.coefficients[i];
            total_weight += weight;
        }
        if (total_weight <= 0.0) throw std::runtime_error("zero aggregate model weight");

        for (std::size_t i = 0; i < global_.coefficients.size(); ++i)
            global_.coefficients[i] = weighted_sum[i] / total_weight;
        ++global_.model_version;
        return global_;
    }

    double predict(double water_quality_index, double turbidity_ntu,
                   double oxygen_mg_l, double temperature_c, double flow_m3_s) const {
        const std::array<double, 5> features{
            1.0 - std::clamp(water_quality_index, 0.0, 1.0),
            std::max(0.0, turbidity_ntu), 1.0 / (1.0 + std::max(0.0, oxygen_mg_l)),
            temperature_c, std::max(0.0, flow_m3_s)};
        double logit = global_.coefficients[0];
        for (std::size_t i = 0; i < features.size(); ++i)
            logit += global_.coefficients[i + 1] * features[i];
        return 1.0 / (1.0 + std::exp(-std::clamp(logit, -40.0, 40.0)));
    }

private:
    WaterRiskModel global_;
};

}  // namespace eco_restoration
