// File: cpp/eco_restoration/climate_stress_test_offset_model.cpp

#include <vector>
#include <string>
#include <cmath>
#include <iostream>
#include "hex_models.hpp"

using namespace hex_analytics;

/**
 * 45. Long-term hex stability under climate change and stress testing.
 *
 * Climate context for Phoenix:
 *  - IPCC-style projections and regional studies indicate:
 *      * Significant increases in extreme heat days (>110°F), up to 3–5× by 2050.[175][179][182][185]
 *      * More frequent and intense drought, reducing soil moisture and vegetation.
 *      * Changes in background radiation and humidity.
 *
 * Impact on α, β, γ:
 *  - α (vegetation cooling) may strengthen in relative importance but face
 *    reduced effective leverage as NDVI declines under drought.
 *  - β (built/roof) may become more sensitive as hotter baselines amplify
 *    roof cooling benefits and anthropogenic heat.
 *  - γ (water) may become more constrained by availability but more valuable
 *    where water bodies persist.
 *
 * Stress-test method:
 *  1. Obtain bias-corrected climate model outputs for Phoenix:
 *      * Downscaled air temperature, radiation, humidity, wind, etc., using
 *        statistical/dynamical downscaling with multivariate bias correction.[176][177][180][183][186]
 *  2. Generate perturbed land-cover scenarios for future decades:
 *      * Reduced NDVI and increased impervious surface (ISA) consistent with
 *        urban growth and drought projections.[181][187]
 *  3. Refit the offset model under each scenario:
 *      * Calibrate α_s, β_s, γ_s for each scenario s using synthetic LST/UHI
 *        derived from the climate and land-cover inputs.
 *  4. Compare scenario-specific coefficients to baseline:
 *      * Track Δα_s, Δβ_s, Δγ_s as functions of climate forcing magnitude.
 *
 * We encode a simple stress-test scaffold that perturbs NDVI/B/W and baseline
 * ΔT per hex according to climate forcing, then recomputes α, β, γ via
 * regression to study drift.
 */

struct HexScenarioSample {
    double V;       // vegetation index
    double B;       // built/roof index
    double W;       // water/wetness index
    double delta_T; // simulated ΔT under scenario
};

struct OffsetCoeffs {
    double alpha;
    double beta;
    double gamma;
    double delta;
};

OffsetCoeffs fit_offset_coeffs(const std::vector<HexScenarioSample>& data) {
    int n = static_cast<int>(data.size());
    if (n < 4) {
        throw std::invalid_argument("Need at least 4 samples for regression.");
    }
    double XtX[4][4] = {{0.0}};
    double XtY[4] = {0.0};

    for (const auto& s : data) {
        double x[4] = {s.V, s.B, s.W, 1.0};
        for (int i = 0; i < 4; ++i) {
            XtY[i] += x[i] * s.delta_T;
            for (int j = 0; j < 4; ++j) {
                XtX[i][j] += x[i] * x[j];
            }
        }
    }

    double A[4][5];
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            A[i][j] = XtX[i][j];
        }
        A[i][4] = XtY[i];
    }

    for (int k = 0; k < 4; ++k) {
        int pivot = k;
        double max_val = std::fabs(A[k][k]);
        for (int i = k + 1; i < 4; ++i) {
            double val = std::fabs(A[i][k]);
            if (val > max_val) {
                max_val = val;
                pivot = i;
            }
        }
        if (max_val < 1e-12) {
            throw std::runtime_error("Singular XtX.");
        }
        if (pivot != k) {
            for (int j = 0; j < 5; ++j) {
                std::swap(A[k][j], A[pivot][j]);
            }
        }
        double diag = A[k][k];
        for (int j = k; j < 5; ++j) {
            A[k][j] /= diag;
        }
        for (int i = k + 1; i < 4; ++i) {
            double factor = A[i][k];
            for (int j = k; j < 5; ++j) {
                A[i][j] -= factor * A[k][j];
            }
        }
    }

    double beta[4];
    for (int i = 3; i >= 0; --i) {
        double sum = A[i][4];
        for (int j = i + 1; j < 4; ++j) {
            sum -= A[i][j] * beta[j];
        }
        beta[i] = sum;
    }

    return {beta[0], beta[1], beta[2], beta[3]};
}

int main_climate_stress() {
    // Baseline scenario (current climate).
    std::vector<HexScenarioSample> baseline;
    for (int i = 0; i < 50; ++i) {
        double V = 0.3 + 0.002 * i;
        double B = 0.4 + 0.001 * i;
        double W = 0.05 + 0.001 * i;
        double delta_T = -8.0 * V + 3.0 * B - 5.0 * W + 0.5;
        baseline.push_back({V, B, W, delta_T});
    }
    OffsetCoeffs coeffs_baseline = fit_offset_coeffs(baseline);

    // Future scenario under climate forcing: higher baseline ΔT, reduced V, increased B, reduced W.
    std::vector<HexScenarioSample> future;
    for (int i = 0; i < 50; ++i) {
        double V = 0.25 + 0.0015 * i; // reduced vegetation
        double B = 0.45 + 0.0015 * i; // more built area
        double W = 0.03 + 0.0008 * i; // less water
        double delta_T = -7.0 * V + 3.5 * B - 4.5 * W + 1.5; // hotter baseline
        future.push_back({V, B, W, delta_T});
    }
    OffsetCoeffs coeffs_future = fit_offset_coeffs(future);

    std::cout << "Baseline α=" << coeffs_baseline.alpha
              << " β=" << coeffs_baseline.beta
              << " γ=" << coeffs_baseline.gamma
              << " δ=" << coeffs_baseline.delta << "\n";
    std::cout << "Future   α=" << coeffs_future.alpha
              << " β=" << coeffs_future.beta
              << " γ=" << coeffs_future.gamma
              << " δ=" << coeffs_future.delta << "\n";

    return 0;
}
