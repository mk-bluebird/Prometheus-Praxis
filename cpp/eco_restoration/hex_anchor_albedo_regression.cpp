// File: cpp/eco_restoration/hex_anchor_albedo_regression.cpp

#include <iostream>
#include <vector>
#include <cmath>
#include <stdexcept>
#include <limits>
#include <string>
#include <tuple>

struct HexSample {
    // Observed change in MRT (e.g., ENVI-met simulated delta relative to baseline)
    double delta_mrt;              // ΔMRT [°C]
    double delta_albedo;           // Δalbedo (roof/surface albedo change)
    double canyon_aspect_ratio;    // building height / street width or similar
};

struct RegressionResult {
    double beta0;
    double beta1;
    double beta2;
    double r2;
    std::size_t n;
};

class HexAnchorAlbedoRegression {
public:
    RegressionResult fit(const std::vector<HexSample>& samples) const {
        if (samples.size() < 3) {
            throw std::runtime_error("Not enough samples for regression.");
        }

        // Multiple linear regression using normal equations:
        // y = beta0 + beta1 * x1 + beta2 * x2
        // We build design matrix with columns: [1, x1, x2]
        double s_y = 0.0;
        double s_x1 = 0.0;
        double s_x2 = 0.0;
        double s_x1x1 = 0.0;
        double s_x2x2 = 0.0;
        double s_x1x2 = 0.0;
        double s_x1y = 0.0;
        double s_x2y = 0.0;

        const std::size_t n = samples.size();
        for (const auto& s : samples) {
            const double y = s.delta_mrt;
            const double x1 = s.delta_albedo;
            const double x2 = s.canyon_aspect_ratio;

            s_y      += y;
            s_x1     += x1;
            s_x2     += x2;
            s_x1x1   += x1 * x1;
            s_x2x2   += x2 * x2;
            s_x1x2   += x1 * x2;
            s_x1y    += x1 * y;
            s_x2y    += x2 * y;
        }

        // Build normal equations matrix (3x3) and RHS (3x1)
        // [ n        s_x1      s_x2   ] [beta0] = [ s_y   ]
        // [ s_x1     s_x1x1    s_x1x2 ] [beta1]   [ s_x1y ]
        // [ s_x2     s_x1x2    s_x2x2 ] [beta2]   [ s_x2y ]
        double a00 = static_cast<double>(n);
        double a01 = s_x1;
        double a02 = s_x2;
        double a10 = s_x1;
        double a11 = s_x1x1;
        double a12 = s_x1x2;
        double a20 = s_x2;
        double a21 = s_x1x2;
        double a22 = s_x2x2;

        double b0 = s_y;
        double b1 = s_x1y;
        double b2 = s_x2y;

        // Solve 3x3 linear system via Gaussian elimination with partial pivoting
        double A[3][3] = {
            {a00, a01, a02},
            {a10, a11, a12},
            {a20, a21, a22}
        };
        double B[3] = {b0, b1, b2};
        solve3x3(A, B);

        RegressionResult res;
        res.beta0 = B[0];
        res.beta1 = B[1];
        res.beta2 = B[2];
        res.n     = n;

        // Compute R^2
        double mean_y = s_y / static_cast<double>(n);
        double ss_tot = 0.0;
        double ss_res = 0.0;
        for (const auto& s : samples) {
            const double y = s.delta_mrt;
            const double y_pred = res.beta0 + res.beta1 * s.delta_albedo
                                  + res.beta2 * s.canyon_aspect_ratio;
            ss_tot += (y - mean_y) * (y - mean_y);
            ss_res += (y - y_pred) * (y - y_pred);
        }
        res.r2 = (ss_tot > 0.0) ? (1.0 - ss_res / ss_tot) : 0.0;

        return res;
    }

    // Given regression coefficients and canyon aspect ratio, compute minimum albedo increase
    // required to achieve target EcoImpact (e.g., EcoImpact >= 0.8), assuming a desired MRT
    // reduction per unit EcoImpact.
    double compute_min_albedo_increase(const RegressionResult& reg,
                                       double canyon_aspect_ratio,
                                       double target_eco_impact,
                                       double mrt_reduction_per_unit_eco) const
    {
        // EcoImpact is assumed to be proportional to MRT reduction:
        // EcoImpact = min(1.0, max(0.0, -ΔMRT / mrt_reduction_per_unit_eco)).
        // To satisfy EcoImpact >= target_eco_impact, we require:
        // -ΔMRT >= target_eco_impact * mrt_reduction_per_unit_eco
        // => ΔMRT <= -target_eco_impact * mrt_reduction_per_unit_eco.
        const double required_delta_mrt = -target_eco_impact * mrt_reduction_per_unit_eco;

        // Regression: ΔMRT = beta0 + beta1 * Δalbedo + beta2 * canyon_aspect_ratio.
        // Solve for Δalbedo:
        // beta1 * Δalbedo = required_delta_mrt - beta0 - beta2 * canyon_aspect_ratio.
        const double numerator = required_delta_mrt - reg.beta0 - reg.beta2 * canyon_aspect_ratio;

        if (std::abs(reg.beta1) < 1e-9) {
            throw std::runtime_error("beta1 is too small; albedo effect cannot be inferred.");
        }

        const double delta_albedo = numerator / reg.beta1;
        return delta_albedo;
    }

private:
    static void solve3x3(double A[3][3], double B[3]) {
        const int n = 3;
        for (int i = 0; i < n; ++i) {
            // Partial pivoting
            int max_row = i;
            double max_val = std::abs(A[i][i]);
            for (int r = i + 1; r < n; ++r) {
                double val = std::abs(A[r][i]);
                if (val > max_val) {
                    max_val = val;
                    max_row = r;
                }
            }
            if (max_val < 1e-12) {
                throw std::runtime_error("Singular matrix in regression.");
            }
            if (max_row != i) {
                for (int c = 0; c < n; ++c) {
                    std::swap(A[i][c], A[max_row][c]);
                }
                std::swap(B[i], B[max_row]);
            }

            // Elimination
            double pivot = A[i][i];
            for (int c = i; c < n; ++c) {
                A[i][c] /= pivot;
            }
            B[i] /= pivot;

            for (int r = 0; r < n; ++r) {
                if (r == i) continue;
                double factor = A[r][i];
                for (int c = i; c < n; ++c) {
                    A[r][c] -= factor * A[i][c];
                }
                B[r] -= factor * B[i];
            }
        }
    }
};

int main() {
    // Example usage with synthetic ENVI-met-like samples for Phoenix CBD.
    // In real deployment, samples would be loaded from simulation outputs.
    std::vector<HexSample> samples = {
        { -2.5, 0.20, 1.2 },
        { -1.8, 0.15, 1.0 },
        { -3.0, 0.25, 1.3 },
        { -2.2, 0.18, 1.1 },
        { -1.5, 0.12, 0.9 }
    };

    HexAnchorAlbedoRegression regressor;
    RegressionResult res = regressor.fit(samples);

    std::cout << "Regression coefficients for Phoenix CBD cool-roof effect:\n";
    std::cout << "beta0 = " << res.beta0 << "\n";
    std::cout << "beta1 = " << res.beta1 << " (ΔMRT per Δalbedo)\n";
    std::cout << "beta2 = " << res.beta2 << " (ΔMRT per canyon aspect ratio)\n";
    std::cout << "R^2   = " << res.r2 << "\n";

    // Suppose we require EcoImpact >= 0.8 and define mrt_reduction_per_unit_eco = 4°C.
    const double target_eco_impact = 0.8;
    const double mrt_reduction_per_unit_eco = 4.0;
    const double cbd_canyon_ratio = 1.1;

    double min_delta_albedo = regressor.compute_min_albedo_increase(
        res, cbd_canyon_ratio, target_eco_impact, mrt_reduction_per_unit_eco
    );

    std::cout << "Minimum albedo increase for EcoImpact >= " << target_eco_impact
              << " at canyon ratio " << cbd_canyon_ratio << " is Δalbedo = "
              << min_delta_albedo << "\n";

    return 0;
}
