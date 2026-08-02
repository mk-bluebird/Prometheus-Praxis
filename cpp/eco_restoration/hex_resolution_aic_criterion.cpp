// File: cpp/eco_restoration/hex_resolution_aic_criterion.cpp

#include <vector>
#include <string>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include "hex_models.hpp"

using namespace hex_analytics;

/**
 * 47. Optimal hex resolution for policy trade-offs using an AIC-based criterion.
 *
 * Theory:
 *  - We consider nested hex grids G_r with resolution r (e.g., edge length or H3 index),
 *    where smaller r means finer cells (micro interventions) and more cells (computational cost).
 *  - For each grid G_r, we fit a regression model predicting ΔT_h (or UHI_h) from V_h, B_h, W_h:
 *
 *        ΔT_h = α_r V_h + β_r B_h + γ_r W_h + δ_r + ε_h
 *
 *  - We compute the Akaike Information Criterion (AIC) for each resolution r:[190][194][198]
 *
 *        AIC_r = 2 k_r - 2 ln( L̂_r )
 *
 *    where:
 *      k_r is the number of free parameters (here 4 for α_r, β_r, γ_r, δ_r),
 *      L̂_r is the maximized likelihood under Gaussian errors:
 *
 *        ln(L̂_r) = -n_r/2 ⋅ [ ln(2π σ̂_r^2) + 1 ]
 *
 *      n_r is number of hexes at resolution r, σ̂_r^2 is residual variance.
 *
 *  - To balance spatial granularity with computational cost, we define an
 *    adjusted information criterion:
 *
 *        IC_r = AIC_r + λ ⋅ C_r
 *
 *    where C_r is a computational penalty (e.g., proportional to n_r or the
 *    runtime of the corridor detection algorithm), and λ encodes policy
 *    preference for efficiency.
 *
 *  - The theoretically optimal hex resolution r* minimizes IC_r over candidate
 *    resolutions, subject to constraints such as minimum hex size to resolve
 *    single parks or blocks.[188][191][195][198]
 *
 * This file implements the IC_r computation for a given resolution, assuming
 * we already aggregated LST/UHI and V/B/W per hex at that resolution.
 */

struct HexRegressionSample {
    double V;
    double B;
    double W;
    double delta_T;
};

struct OffsetCoeffs {
    double alpha;
    double beta;
    double gamma;
    double delta;
};

OffsetCoeffs fit_offset_coeffs(const std::vector<HexRegressionSample>& data) {
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

double compute_residual_variance(const std::vector<HexRegressionSample>& data,
                                 const OffsetCoeffs& coeffs) {
    int n = static_cast<int>(data.size());
    double rss = 0.0;
    for (const auto& s : data) {
        double y_hat = coeffs.alpha * s.V + coeffs.beta * s.B +
                       coeffs.gamma * s.W + coeffs.delta;
        double e = s.delta_T - y_hat;
        rss += e * e;
    }
    if (n <= 4) return 0.0;
    return rss / (n - 4);
}

double compute_aic(int n, int k, double sigma2) {
    if (sigma2 <= 0.0 || n <= 0) {
        throw std::invalid_argument("Invalid sigma2 or n for AIC.");
    }
    double logL = -0.5 * n * (std::log(2.0 * M_PI * sigma2) + 1.0);
    double AIC = 2.0 * k - 2.0 * logL;
    return AIC;
}

double compute_ic(double AIC, double comp_penalty, double lambda) {
    return AIC + lambda * comp_penalty;
}

int main_hex_resolution_aic() {
    std::vector<HexRegressionSample> data_r1;
    for (int i = 0; i < 80; ++i) {
        double V = 0.30 + 0.001 * i;
        double B = 0.45 + 0.0008 * i;
        double W = 0.04 + 0.0005 * i;
        double delta_T = -8.0 * V + 3.0 * B - 5.0 * W + 0.5;
        data_r1.push_back({V, B, W, delta_T});
    }

    std::vector<HexRegressionSample> data_r2;
    for (int i = 0; i < 40; ++i) {
        double V = 0.32 + 0.0015 * i;
        double B = 0.43 + 0.0012 * i;
        double W = 0.05 + 0.0007 * i;
        double delta_T = -7.8 * V + 3.1 * B - 4.9 * W + 0.6;
        data_r2.push_back({V, B, W, delta_T});
    }

    OffsetCoeffs c1 = fit_offset_coeffs(data_r1);
    double sigma2_1 = compute_residual_variance(data_r1, c1);
    double AIC_1 = compute_aic(static_cast<int>(data_r1.size()), 4, sigma2_1);
    double comp_penalty_1 = static_cast<double>(data_r1.size());
    double lambda = 0.05;
    double IC_1 = compute_ic(AIC_1, comp_penalty_1, lambda);

    OffsetCoeffs c2 = fit_offset_coeffs(data_r2);
    double sigma2_2 = compute_residual_variance(data_r2, c2);
    double AIC_2 = compute_aic(static_cast<int>(data_r2.size()), 4, sigma2_2);
    double comp_penalty_2 = static_cast<double>(data_r2.size());
    double IC_2 = compute_ic(AIC_2, comp_penalty_2, lambda);

    std::cout << "Resolution r1: AIC=" << AIC_1 << " IC=" << IC_1 << "\n";
    std::cout << "Resolution r2: AIC=" << AIC_2 << " IC=" << IC_2 << "\n";

    return 0;
}
