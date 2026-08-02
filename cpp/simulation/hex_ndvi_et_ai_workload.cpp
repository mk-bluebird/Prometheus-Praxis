// File: cpp/simulation/hex_ndvi_et_ai_workload.cpp

#include <vector>
#include <string>
#include <cmath>
#include <iostream>

/**
 * 49. Canopy evapotranspiration to NDVI calibration for AI workloads.
 *
 * Concept:
 *  - NDVI_t at the hex level responds over time to irrigation, planting, and climate.
 *  - A recurrent neural network (RNN) or sequence model can learn NDVI_t trajectories,
 *    which, combined with α (NDVI→ΔT sensitivity), yields forecasts of cooling
 *    over a growing season.[203][205][212][209][215][210][207][216]
 *
 * Minimal feature set:
 *  - Time series at daily or weekly resolution per hex:
 *      * NDVI_t (target).
 *      * Irrigation_t (mm or relative index).
 *      * Soil moisture_t (volumetric, from sensors or AI soil models).
 *      * Species type / functional group (encoded as categorical or traits).
 *      * Air temperature_t, humidity_t, radiation_t (Penman-Monteith drivers).
 *      * Planting events (binary or impulse indicating new canopy).
 *  - Optionally: NDBI_t, NDWI_t, canopy temperature_t, and ET estimates.
 *
 * Training pattern:
 *  - Input sequence x_t per hex with features above; output NDVI_{t+Δ}.
 *  - Loss: mean squared error over NDVI predictions.
 *  - After training, we simulate NDVI_t under planned irrigation/planting and
 *    use α to forecast ΔT_t = α ⋅ NDVI_t + (other terms).
 *
 * This file provides scaffolding for an NDVI trajectory simulator using
 * simplified dynamics (not actual RNN), suitable for coupling with α.
 */

struct NDVISequenceSample {
    std::string hex_id;
    std::vector<double> ndvi;        // NDVI_t
    std::vector<double> irrigation;  // mm/day
    std::vector<double> soil_moist;  // volumetric fraction
    std::vector<double> temp;        // air temperature (°C)
    std::vector<double> humidity;    // relative humidity
    std::vector<int> planting_event; // 1 if planting at t, else 0
};

struct NDVIModelParams {
    double k_irrigation;
    double k_soil_moist;
    double k_temp_stress;
    double k_humidity;
    double k_growth_base;
    double k_planting_boost;
};

std::vector<double> simulate_ndvi_trajectory(const NDVISequenceSample& s,
                                             const NDVIModelParams& p) {
    size_t T = s.ndvi.size();
    std::vector<double> ndvi_pred(T);
    if (T == 0) return ndvi_pred;

    ndvi_pred[0] = s.ndvi[0];

    for (size_t t = 1; t < T; ++t) {
        double ndvi_prev = ndvi_pred[t - 1];
        double irrig = s.irrigation[t];
        double soil = s.soil_moist[t];
        double temp = s.temp[t];
        double hum  = s.humidity[t];
        int plant_evt = s.planting_event[t];

        double temp_opt = 28.0;
        double temp_diff = std::fabs(temp - temp_opt);
        double temp_factor = std::exp(-p.k_temp_stress * temp_diff);

        double humid_factor = 1.0 + p.k_humidity * (hum - 0.4);

        double growth = p.k_growth_base * temp_factor * humid_factor;

        double irrig_effect = p.k_irrigation * irrig;
        double soil_effect  = p.k_soil_moist * soil;
        double planting_effect = p.k_planting_boost * plant_evt;

        double ndvi_delta = growth + irrig_effect + soil_effect + planting_effect;

        ndvi_pred[t] = ndvi_prev + ndvi_delta;
        if (ndvi_pred[t] < 0.0) ndvi_pred[t] = 0.0;
        if (ndvi_pred[t] > 0.9) ndvi_pred[t] = 0.9;
    }

    return ndvi_pred;
}

std::vector<double> forecast_delta_T_from_ndvi(const std::vector<double>& ndvi_pred,
                                               double alpha,
                                               double beta,
                                               double gamma,
                                               double B_fixed,
                                               double W_fixed,
                                               double delta) {
    size_t T = ndvi_pred.size();
    std::vector<double> delta_T(T);
    for (size_t t = 0; t < T; ++t) {
        delta_T[t] = alpha * ndvi_pred[t] + beta * B_fixed + gamma * W_fixed + delta;
    }
    return delta_T;
}

int main_ndvi_ai() {
    NDVISequenceSample s;
    s.hex_id = "hex_10_20";
    size_t T = 30;
    s.ndvi.resize(T, 0.2);
    s.irrigation.resize(T);
    s.soil_moist.resize(T);
    s.temp.resize(T);
    s.humidity.resize(T);
    s.planting_event.resize(T, 0);

    for (size_t t = 0; t < T; ++t) {
        s.irrigation[t] = (t > 5 && t < 20) ? 5.0 : 1.0;
        s.soil_moist[t] = (t > 5 && t < 20) ? 0.25 : 0.15;
        s.temp[t]       = 32.0;
        s.humidity[t]   = 0.35;
    }
    s.planting_event[5] = 1;

    NDVIModelParams p;
    p.k_irrigation      = 0.002;
    p.k_soil_moist      = 0.05;
    p.k_temp_stress     = 0.05;
    p.k_humidity        = 0.3;
    p.k_growth_base     = 0.001;
    p.k_planting_boost  = 0.05;

    auto ndvi_pred = simulate_ndvi_trajectory(s, p);

    double alpha = -8.0;
    double beta  = 3.0;
    double gamma = -5.0;
    double B_fixed = 0.50;
    double W_fixed = 0.05;
    double delta = 0.5;

    auto delta_T = forecast_delta_T_from_ndvi(ndvi_pred, alpha, beta, gamma,
                                              B_fixed, W_fixed, delta);

    std::cout << "Cooling trajectory for " << s.hex_id << ":\n";
    for (size_t t = 0; t < T; ++t) {
        std::cout << "  day " << t
                  << " NDVI=" << ndvi_pred[t]
                  << " ΔT=" << delta_T[t] << " °C\n";
    }

    return 0;
}
