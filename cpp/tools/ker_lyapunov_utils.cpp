// File: cpp/tools/ker_lyapunov_utils.cpp
#include <vector>
#include <stdexcept>
#include <cmath>

namespace eco_tools {

// KER composite score s = k * e - r, with k,e,r assumed in [0,1].
// This matches the governance math where K is knowledge/safe-step fraction,
// E is eco-impact margin, and R is max risk coordinate.[59]
double ker_score(double k, double e, double r) {
    if (k < 0.0 || k > 1.0 ||
        e < 0.0 || e > 1.0 ||
        r < 0.0 || r > 1.0) {
        throw std::invalid_argument("k, e, r must be in [0,1]");
    }
    return k * e - r;
}

// Lyapunov residual V_t = sum_j w_j * r_j^2 with w_j >= 0,
// matching the governance docs' quadratic residual over risk planes.[59]
double lyapunov_residual(const std::vector<double>& w,
                         const std::vector<double>& r) {
    if (w.size() != r.size()) {
        throw std::invalid_argument("Weights and risk vectors must have same length");
    }
    double V = 0.0;
    for (std::size_t i = 0; i < w.size(); ++i) {
        double wi = w[i];
        double ri = r[i];
        if (wi < 0.0) {
            throw std::invalid_argument("Weights must be nonnegative");
        }
        if (ri < 0.0) {
            throw std::invalid_argument("Risk coordinates must be nonnegative");
        }
        V += wi * ri * ri;
    }
    return V;
}

} // namespace eco_tools
