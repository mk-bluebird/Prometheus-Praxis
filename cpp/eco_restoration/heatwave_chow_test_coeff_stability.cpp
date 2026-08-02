// File: cpp/eco_restoration/heatwave_chow_test_coeff_stability.cpp

#include <vector>
#include <string>
#include <cmath>
#include <iostream>
#include <stdexcept>

/**
 * Robustness of α, β, γ under extreme events:
 *
 * We consider two regimes of Landsat-8 scenes over Phoenix:
 *   - Regime 1: average summer days (no NWS heat advisory).
 *   - Regime 2: extreme heatwave days (NWS heat advisory in effect).[168][165][59]
 *
 * For each regime, we estimate the regression:
 *   ΔT_h = α V_h + β B_h + γ W_h + δ
 *
 * and apply a Chow test to detect structural breaks in coefficients
 * between the two regimes.
 *
 * Chow F-statistic:
 *   Let RSS_R   = residual sum of squares for pooled regression (all scenes).
 *       RSS_U   = RSS_1 + RSS_2: sum of residuals for separate regressions.
 *       k       = number of parameters (including intercept, here 4).
 *       n       = total sample size (n = n_1 + n_2).[162][164][167][170]
 *
 *   F = ((RSS_R - RSS_U)/k) / (RSS_U / (n - 2k))
 *
 * Under H0 (no structural break), F ~ F(k, n - 2k).
 * A large F and small p-value indicate that α, β, γ, or δ differ
 * between heatwave and average regimes.
 */

struct HexSample {
    double V;
    double B;
    double W;
    double delta_T;
};

struct RegressionResult {
    double alpha;
    double beta;
    double gamma;
    double delta;
    double rss;
    int n;
};

RegressionResult fit_regression(const std::vector<HexSample>& data) {
    int n = static_cast<int>(data.size());
    if (n < 4) {
        throw std::invalid_argument("Need at least 4 samples for regression.");
    }

    // Design matrix X: columns [V, B, W, 1], response y = ΔT.
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

    // Solve XtX * beta = XtY (4x4 system).
    double A[4][5]; // augmented matrix
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            A[i][j] = XtX[i][j];
        }
        A[i][4] = XtY[i];
    }

    // Gaussian elimination.
    for (int k = 0; k < 4; ++k) {
        // Pivot
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
            throw std::runtime_error("Singular XtX matrix.");
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

    // Compute RSS.
    double rss = 0.0;
    for (const auto& s : data) {
        double y_hat = beta[0] * s.V + beta[1] * s.B + beta[2] * s.W + beta[3];
        double diff = s.delta_T - y_hat;
        rss += diff * diff;
    }

    RegressionResult res;
    res.alpha = beta[0];
    res.beta  = beta[1];
    res.gamma = beta[2];
    res.delta = beta[3];
    res.rss   = rss;
    res.n     = n;
    return res;
}

double chow_F_statistic(const RegressionResult& pooled,
                        const RegressionResult& reg1,
                        const RegressionResult& reg2,
                        int k) {
    int n = reg1.n + reg2.n;
    double RSS_R = pooled.rss;
    double RSS_U = reg1.rss + reg2.rss;

    double numerator = (RSS_R - RSS_U) / static_cast<double>(k);
    double denominator = RSS_U / static_cast<double>(n - 2 * k);
    return numerator / denominator;
}

int main() {
    // Synthetic example: average vs heatwave scenes grouped by NWS advisory.
    std::vector<HexSample> avg_days;
    std::vector<HexSample> heatwave_days;

    // Populate with synthetic data; in practice, use Landsat-8 NDVI/NDBI/NDWI/LST
    // grouped by NWS heat advisory status.[52][65][165]
    for (int i = 0; i < 100; ++i) {
        double V = 0.3 + 0.002 * i;
        double B = 0.4 + 0.001 * i;
        double W = 0.05 + 0.0015 * i;
        double delta_T = -8.0 * V + 3.0 * B - 5.0 * W + 0.5;
        avg_days.push_back({V, B, W, delta_T});
    }
    for (int i = 0; i < 80; ++i) {
        double V = 0.3 + 0.002 * i;
        double B = 0.4 + 0.0015 * i;
        double W = 0.05 + 0.001 * i;
        double delta_T = -7.0 * V + 3.5 * B - 4.5 * W + 1.0; // altered coefficients on heatwave days
        heatwave_days.push_back({V, B, W, delta_T});
    }

    // Pooled regression.
    std::vector<HexSample> pooled = avg_days;
    pooled.insert(pooled.end(), heatwave_days.begin(), heatwave_days.end());

    RegressionResult reg_pooled = fit_regression(pooled);
    RegressionResult reg_avg    = fit_regression(avg_days);
    RegressionResult reg_heat   = fit_regression(heatwave_days);

    int k = 4; // α, β, γ, δ
    double F = chow_F_statistic(reg_pooled, reg_avg, reg_heat, k);

    std::cout << "Chow F-statistic (heatwave vs average): " << F << "\n";
    std::cout << "Avg days:   alpha=" << reg_avg.alpha << " beta=" << reg_avg.beta
              << " gamma=" << reg_avg.gamma << " delta=" << reg_avg.delta << "\n";
    std::cout << "Heatwave:   alpha=" << reg_heat.alpha << " beta=" << reg_heat.beta
              << " gamma=" << reg_heat.gamma << " delta=" << reg_heat.delta << "\n";
    return 0;
}
