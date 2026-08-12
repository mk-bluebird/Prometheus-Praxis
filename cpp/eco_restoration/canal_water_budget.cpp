// File: cpp/eco_restoration/canal_water_budget.cpp

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace eco_restoration {

struct DailyWaterInputs {
    double storage_start_m3{};
    double precipitation_mm{};
    double inflow_m3{};
    double outflow_m3{};
    double seepage_m3{};
    double area_m2{};
    double temperature_mean_c{};
    double temperature_min_c{};
    double temperature_max_c{};
    double extraterrestrial_radiation_mj_m2_day{};
    double storage_standard_deviation_m3{};
    double flow_standard_deviation_m3{};
    double seepage_standard_deviation_m3{};
};

struct WaterBudgetResult {
    double evapotranspiration_mm{};
    double evapotranspiration_m3{};
    double storage_end_m3{};
    double negative_storage_probability{};
    double water_risk{};
};

double normal_cdf(double value) {
    return 0.5 * (1.0 + std::erf(value / std::sqrt(2.0)));
}

WaterBudgetResult evaluate_daily_water_budget(const DailyWaterInputs& input) {
    if (input.storage_start_m3 < 0.0 || input.precipitation_mm < 0.0 ||
        input.inflow_m3 < 0.0 || input.outflow_m3 < 0.0 || input.seepage_m3 < 0.0 ||
        input.area_m2 <= 0.0 || input.temperature_max_c < input.temperature_min_c ||
        input.extraterrestrial_radiation_mj_m2_day < 0.0 ||
        input.storage_standard_deviation_m3 < 0.0 || input.flow_standard_deviation_m3 < 0.0 ||
        input.seepage_standard_deviation_m3 < 0.0) {
        throw std::invalid_argument("invalid canal water budget input");
    }

    const double temperature_range = input.temperature_max_c - input.temperature_min_c;
    const double et_mm = 0.0023 * (input.temperature_mean_c + 17.8) *
                         std::sqrt(temperature_range) *
                         input.extraterrestrial_radiation_mj_m2_day;
    const double et_m3 = std::max(0.0, et_mm) * input.area_m2 / 1000.0;
    const double rain_m3 = input.precipitation_mm * input.area_m2 / 1000.0;
    const double storage_end =
        input.storage_start_m3 + rain_m3 + input.inflow_m3 -
        input.outflow_m3 - et_m3 - input.seepage_m3;

    const double variance =
        input.storage_standard_deviation_m3 * input.storage_standard_deviation_m3 +
        2.0 * input.flow_standard_deviation_m3 * input.flow_standard_deviation_m3 +
        input.seepage_standard_deviation_m3 * input.seepage_standard_deviation_m3;

    const double negative_probability = variance <= 1e-12
        ? (storage_end < 0.0 ? 1.0 : 0.0)
        : normal_cdf(-storage_end / std::sqrt(variance));

    return {
        et_mm,
        et_m3,
        storage_end,
        std::clamp(negative_probability, 0.0, 1.0),
        std::clamp(negative_probability, 0.0, 1.0)
    };
}

}  // namespace eco_restoration
