// File: cpp/eco_restoration/heat_cobenefit_and_wbgt.cpp

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace eco_restoration {

struct CoolingSensitivity {
    double cooling_kwh_per_m2_c{};
    double conditioned_floor_area_m2{};
    double lst_to_indoor_temperature_transfer{};
    double implementation_fraction{};
};

double cooling_energy_reduction_kwh(
    double lst_reduction_c,
    const CoolingSensitivity& sensitivity) {

    if (lst_reduction_c < 0.0 || sensitivity.cooling_kwh_per_m2_c < 0.0 ||
        sensitivity.conditioned_floor_area_m2 < 0.0 ||
        sensitivity.lst_to_indoor_temperature_transfer < 0.0 ||
        sensitivity.lst_to_indoor_temperature_transfer > 1.0 ||
        sensitivity.implementation_fraction < 0.0 ||
        sensitivity.implementation_fraction > 1.0) {
        throw std::invalid_argument("invalid cooling co-benefit parameters");
    }

    return lst_reduction_c *
           sensitivity.lst_to_indoor_temperature_transfer *
           sensitivity.cooling_kwh_per_m2_c *
           sensitivity.conditioned_floor_area_m2 *
           sensitivity.implementation_fraction;
}

struct WeatherHeatInput {
    double air_temperature_c{};
    double relative_humidity_percent{};
    double net_radiant_heat_w_m2{};
    double wind_speed_m_s{};
};

struct WbgtResult {
    double natural_wet_bulb_c{};
    double globe_temperature_c{};
    double wbgt_c{};
};

double natural_wet_bulb_c(double air_temperature_c, double relative_humidity_percent) {
    const double rh = std::clamp(relative_humidity_percent, 1.0, 100.0);
    return air_temperature_c *
               std::atan(0.151977 * std::sqrt(rh + 8.313659)) +
           std::atan(air_temperature_c + rh) -
           std::atan(rh - 1.676331) +
           0.00391838 * std::pow(rh, 1.5) * std::atan(0.023101 * rh) -
           4.686035;
}

WbgtResult outdoor_wbgt(const WeatherHeatInput& input) {
    if (input.relative_humidity_percent < 0.0 || input.relative_humidity_percent > 100.0 ||
        input.net_radiant_heat_w_m2 < 0.0 || input.wind_speed_m_s < 0.0) {
        throw std::invalid_argument("invalid weather heat input");
    }

    const double wet_bulb = natural_wet_bulb_c(
        input.air_temperature_c, input.relative_humidity_percent);
    const double convection_w_m2_c = 5.7 + 3.8 * std::sqrt(std::max(0.0, input.wind_speed_m_s));
    const double globe = input.air_temperature_c + input.net_radiant_heat_w_m2 / convection_w_m2_c;
    const double wbgt = 0.7 * wet_bulb + 0.2 * globe + 0.1 * input.air_temperature_c;

    return {wet_bulb, globe, wbgt};
}

enum class ThermalLaneAction {
    Proceed,
    Derate,
    OperatorReview
};

ThermalLaneAction worker_heat_lane_action(
    double current_wbgt_c,
    double predicted_ai_temperature_increment_c,
    double derate_wbgt_c,
    double maximum_wbgt_c) {

    if (current_wbgt_c < -50.0 || predicted_ai_temperature_increment_c < 0.0 ||
        derate_wbgt_c > maximum_wbgt_c) {
        throw std::invalid_argument("invalid worker heat corridor");
    }

    const double projected_wbgt = current_wbgt_c + predicted_ai_temperature_increment_c;
    if (projected_wbgt >= maximum_wbgt_c) return ThermalLaneAction::OperatorReview;
    if (projected_wbgt >= derate_wbgt_c) return ThermalLaneAction::Derate;
    return ThermalLaneAction::Proceed;
}

}  // namespace eco_restoration
