// File: cpp/eco_restoration/iot_bayesian_hex_calibration.cpp

#include <vector>
#include <string>
#include <cmath>
#include <iostream>

/**
 * 30. Hex-Level Microclimate Sensor Network Calibration (Bayesian Updating)
 *
 * Wiring pattern:
 *
 * 1. Sensor deployment:
 *    - Low-cost IoT temperature/humidity sensors placed in a subset of hexes.
 *    - Each sensor reports T_sensor,h(t), RH_h(t) at high frequency via MQTT/LoRa.
 *
 * 2. Data ingestion:
 *    - Rust backend aggregates sensor readings per hex over Landsat overpass windows
 *      to estimate local ground truth UHI and microclimate conditions:
 *        UHI_sensor,h = T_sensor,h - T_rural_ref(t)
 *
 * 3. Bayesian calibration:
 *    - Prior on α, β, γ per hex from satellite-only regression:
 *        α_h ~ N(α_prior,h, σ_α_prior,h^2)
 *        β_h ~ N(β_prior,h, σ_β_prior,h^2)
 *        γ_h ~ N(γ_prior,h, σ_γ_prior,h^2)
 *
 *    - Likelihood: sensor-derived ΔT_h conditioned on predictors:
 *        ΔT_h ~ N(α_h V_h + β_h B_h + γ_h W_h + δ_h, σ_obs^2)
 *
 *    - Posterior (conjugate normal-normal update for each coefficient):
 *        α_post,h = (σ_obs^{-2} Σ V_h ΔT_h + σ_α_prior^{-2} α_prior,h) /
 *                   (σ_obs^{-2} Σ V_h^2 + σ_α_prior^{-2})
 *        with similar updates for β_h, γ_h.
 *
 * 4. JSON integration:
 *    - Posterior means and variances are written back to calibration JSON:
 *        {
 *          "hex_id": "hex_10_20",
 *          "alpha": { "mean": α_post,h, "var": σ_α_post,h^2 },
 *          "beta":  { "mean": β_post,h, "var": σ_β_post,h^2 },
 *          "gamma": { "mean": γ_post,h, "var": σ_γ_post,h^2 },
 *          "source": "satellite+sensor"
 *        }
 *
 *    - Non-sensor hexes retain satellite-only priors.
 */

struct HexSensorSample {
    double V;          // vegetation index for the sensor window
    double B;          // built/roof index
    double W;          // water/wetness index
    double delta_T;    // sensor-derived ΔT (T_sensor - T_rural_ref)
};

struct CoeffPrior {
    double mean;
    double var;
};

struct CoeffPosterior {
    double mean;
    double var;
};

CoeffPosterior update_coefficient(const std::vector<HexSensorSample>& samples,
                                  double CoeffSensorSample::*x_member,
                                  CoeffPrior prior,
                                  double sigma_obs_sq) {
    double sum_x2 = 0.0;
    double sum_x_y = 0.0;
    for (const auto& s : samples) {
        double x = s.*x_member;
        sum_x2 += x * x;
        sum_x_y += x * s.delta_T;
    }

    double inv_var_obs = 1.0 / sigma_obs_sq;
    double inv_var_prior = 1.0 / prior.var;

    double var_post_inv = inv_var_obs * sum_x2 + inv_var_prior;
    double var_post = 1.0 / var_post_inv;

    double mean_post = var_post *
        (inv_var_obs * sum_x_y + inv_var_prior * prior.mean);

    return {mean_post, var_post};
}

int main_bayes() {
    // Synthetic sensor samples for one hex.
    std::vector<HexSensorSample> samples = {
        {0.30, 0.5, 0.06, -3.5},
        {0.32, 0.5, 0.06, -3.8},
        {0.31, 0.52, 0.07, -3.6}
    };

    CoeffPrior alpha_prior{-8.0, 0.8 * 0.8};
    CoeffPrior beta_prior{3.0, 0.6 * 0.6};
    CoeffPrior gamma_prior{-5.0, 0.7 * 0.7};

    double sigma_obs_sq = 0.5 * 0.5;

    CoeffPosterior alpha_post = update_coefficient(samples, &HexSensorSample::V,
                                                   alpha_prior, sigma_obs_sq);
    CoeffPosterior beta_post  = update_coefficient(samples, &HexSensorSample::B,
                                                   beta_prior, sigma_obs_sq);
    CoeffPosterior gamma_post = update_coefficient(samples, &HexSensorSample::W,
                                                   gamma_prior, sigma_obs_sq);

    std::cout << "Posterior coefficients for sensor-calibrated hex:\n"
              << "  alpha: mean=" << alpha_post.mean << " var=" << alpha_post.var << "\n"
              << "  beta:  mean=" << beta_post.mean  << " var=" << beta_post.var  << "\n"
              << "  gamma: mean=" << gamma_post.mean << " var=" << gamma_post.var << "\n";

    return 0;
}
