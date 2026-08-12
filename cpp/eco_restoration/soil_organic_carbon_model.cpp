// File: cpp/eco_restoration/soil_organic_carbon_model.cpp
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <stdexcept>

namespace eco_restoration {

struct SoilCarbonPools {
    double decomposable_kg_c_m2{};
    double resistant_kg_c_m2{};
    double microbial_kg_c_m2{};
    double humified_kg_c_m2{};
    double inert_kg_c_m2{};

    double total() const {
        return decomposable_kg_c_m2 + resistant_kg_c_m2 + microbial_kg_c_m2 +
               humified_kg_c_m2 + inert_kg_c_m2;
    }
};

struct RestorationAction {
    double organic_input_kg_c_m2_month{};
    double decomposable_fraction{};
    double cover_factor{1.0};
    double moisture_factor{1.0};
    double temperature_c{};
};

struct SoilCarbonResult {
    SoilCarbonPools pools;
    double stock_change_kg_c_m2{};
    double r_soil_carbon{};
    double knowledge_factor{};
    double eco_impact_value{};
};

class RothCStyleHexModel {
public:
    explicit RothCStyleHexModel(SoilCarbonPools initial, double target_stock_kg_c_m2)
        : pools_(initial), target_stock_(target_stock_kg_c_m2) {
        if (initial.total() < 0.0 || target_stock_kg_c_m2 <= 0.0)
            throw std::invalid_argument("invalid initial soil carbon state");
    }

    SoilCarbonResult step_month(const RestorationAction& action,
                                double measurement_confidence) {
        if (action.organic_input_kg_c_m2_month < 0.0 ||
            action.decomposable_fraction < 0.0 || action.decomposable_fraction > 1.0 ||
            action.cover_factor <= 0.0 || action.moisture_factor <= 0.0 ||
            measurement_confidence < 0.0 || measurement_confidence > 1.0)
            throw std::invalid_argument("invalid restoration action");

        const double before = pools_.total();
        const double temperature_factor = std::exp(0.047 * (action.temperature_c - 20.0));
        const double modifier = temperature_factor * action.moisture_factor / action.cover_factor;
        const std::array<double, 4> rates{10.0, 0.30, 0.66, 0.02};

        std::array<double*, 4> pool_ptrs{&pools_.decomposable_kg_c_m2,
                                         &pools_.resistant_kg_c_m2,
                                         &pools_.microbial_kg_c_m2,
                                         &pools_.humified_kg_c_m2};
        std::array<double, 4> losses{};
        for (std::size_t i = 0; i < pool_ptrs.size(); ++i) {
            losses[i] = *pool_ptrs[i] * (1.0 - std::exp(-rates[i] * modifier / 12.0));
            *pool_ptrs[i] = std::max(0.0, *pool_ptrs[i] - losses[i]);
        }

        const double decomposable_input = action.organic_input_kg_c_m2_month *
                                          action.decomposable_fraction;
        pools_.decomposable_kg_c_m2 += decomposable_input;
        pools_.resistant_kg_c_m2 += action.organic_input_kg_c_m2_month - decomposable_input;
        const double decomposed = losses[0] + losses[1] + losses[2] + losses[3];
        pools_.microbial_kg_c_m2 += 0.18 * decomposed;
        pools_.humified_kg_c_m2 += 0.28 * decomposed;

        const double change = pools_.total() - before;
        const double risk = std::clamp((target_stock_ - pools_.total()) / target_stock_, 0.0, 1.0);
        const double knowledge = std::clamp(measurement_confidence *
            std::min(1.0, pools_.total() / target_stock_), 0.0, 1.0);
        return {pools_, change, risk, knowledge, knowledge * (1.0 - risk)};
    }

private:
    SoilCarbonPools pools_;
    double target_stock_;
};

}  // namespace eco_restoration
