// Filename: src/ker_cpp/ker_kernel.cpp

#include <algorithm>
#include <array>
#include <cmath>

struct KerResult {
    double vt;
    double k;
    double e;
    double r;
};

constexpr std::size_t NUM_PLANES = 7;

KerResult computeKer(const std::array<double, NUM_PLANES>& r,
                     const std::array<double, NUM_PLANES>& w,
                     double vtPrev) {
    double vt = 0.0;
    double maxRisk = 0.0;
    for (std::size_t i = 0; i < NUM_PLANES; ++i) {
        double ri = r[i];
        if (ri < 0.0) ri = 0.0;
        if (ri > 1.0) ri = 1.0;
        double term = w[i] * ri * ri;
        vt += term;
        if (ri > maxRisk) {
            maxRisk = ri;
        }
    }

    double deltaVt = vt - vtPrev;

    double k = 0.95 - 0.4 * maxRisk;
    if (deltaVt > 0.0) {
        k -= 0.25;
    }
    if (k < 0.0) k = 0.0;
    if (k > 1.0) k = 1.0;

    double e = 0.95 - vt;
    if (deltaVt > 0.0) {
        e -= 0.3;
    }
    if (e < 0.0) e = 0.0;
    if (e > 1.0) e = 1.0;

    double rCoord = vt + std::max(deltaVt, 0.0);
    if (rCoord > 1.0) rCoord = 1.0;
    if (rCoord < 0.0) rCoord = 0.0;

    KerResult result{vt, k, e, rCoord};
    return result;
}
