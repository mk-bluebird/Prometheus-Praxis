// File: cpp/examples/ker_example.cpp
#include <iostream>
#include "eco_restoration.hpp"

int main() {
    // Example risk coordinates for one hex.
    double r_h = 0.20;
    double r_e = 0.15;
    double r_t = 0.10;
    double r_b = 0.12;

    std::vector<double> w = {0.25, 0.25, 0.25, 0.25};
    std::vector<double> r = {r_h, r_e, r_t, r_b};

    double Vt = eco_tools::lyapunov_residual(w, r);
    double r_max = std::max(std::max(r_h, r_e), std::max(r_t, r_b));
    double k = 0.94;
    double e = 1.0 - r_max;
    if (e < 0.0) e = 0.0;
    double s = eco_tools::ker_score(k, e, r_max);

    std::cout << "KER example:\n"
              << "  Vt=" << Vt << "\n"
              << "  r_max=" << r_max << "\n"
              << "  k=" << k << " e=" << e << "\n"
              << "  ker_score=" << s << "\n";
    return 0;
}
