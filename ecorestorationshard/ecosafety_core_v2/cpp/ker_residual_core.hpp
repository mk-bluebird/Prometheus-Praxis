// filename: ecorestorationshard/ecosafety_core_v2/cpp/ker_residual_core.hpp
// destination: ecorestorationshard/ecosafety_core_v2/cpp/ker_residual_core.hpp
// repo-target: https://github.com/mk-bluebird/Prometheus-Praxis
//
// Purpose:
//   Header-only C++ Lyapunov residual and KER kernel for cyboquatic ecosafety.
//   This is non-actuating and designed for:
//     - Computing V_t from normalized risk coordinates.[4]
//     - Computing K,E,R per window.[4]
//     - Providing a shared residual engine that all domain-specific CPP
//       models must call, satisfying "always improve" obligations.[4][6]
//
//   This header does not include any hardware or fieldbus interfaces.
//   It is safe for CI, analysis, and superintelligence-aligned workloads.

#ifndef ECORESTORATIONSHARD_ECOSAFETY_CORE_V2_KER_RESIDUAL_CORE_HPP
#define ECORESTORATIONSHARD_ECOSAFETY_CORE_V2_KER_RESIDUAL_CORE_HPP

#include <array>
#include <cmath>
#include <stdexcept>
#include <string>

namespace ecosafety_core_v2 {

enum class RiskPlane {
    ENERGY = 0,
    HYDRAULICS = 1,
    PFAS = 2,
    COLD = 3,
    BOD = 4,
    TSS = 5,
    CEC = 6,
    CARBON = 7,
    BIODIVERSITY = 8,
    MATERIALS = 9,
    NEURORIGHTS = 10,
    TOPOLOGY = 11,
    DATAQUALITY = 12,
    UNCERTAINTY = 13
};

constexpr std::size_t RISK_PLANE_COUNT = 14;

// Normalized risk vector r_j in [0,1] for canonical planes.[4][6]
struct RiskVector {
    std::array<double, RISK_PLANE_COUNT> r{};

    double get(RiskPlane plane) const {
        const std::size_t idx = static_cast<std::size_t>(plane);
        return r.at(idx);
    }

    void set(RiskPlane plane, double value) {
        if (value < 0.0 || value > 1.0) {
            throw std::invalid_argument("RiskVector: value must be in [0,1]");
        }
        const std::size_t idx = static_cast<std::size_t>(plane);
        r.at(idx) = value;
    }
};

// Lyapunov weights w_j per plane.[4]
struct PlaneWeights {
    std::array<double, RISK_PLANE_COUNT> w{};

    double get(RiskPlane plane) const {
        const std::size_t idx = static_cast<std::size_t>(plane);
        return w.at(idx);
    }

    void set(RiskPlane plane, double value) {
        if (value < 0.0) {
            throw std::invalid_argument("PlaneWeights: weight must be non-negative");
        }
        const std::size_t idx = static_cast<std::size_t>(plane);
        w.at(idx) = value;
    }
};

// K,E,R triad and residual V_t for a window.[4]
struct KerResidual {
    double vt;
    double k;
    double e;
    double r;
};

// Super-quadratic penalty g_j(r) = alpha * r^2 + beta * r^p (p>2), per plane.[4][6]
inline double super_quadratic_penalty(double r, double alpha, double beta, int p) {
    if (r < 0.0 || r > 1.0) {
        throw std::invalid_argument("super_quadratic_penalty: r must be in [0,1]");
    }
    if (alpha < 0.0 || beta < 0.0 || p <= 2) {
        throw std::invalid_argument("super_quadratic_penalty: invalid parameters");
    }
    const double r2 = r * r;
    const double rp = std::pow(r, static_cast<double>(p));
    return alpha * r2 + beta * rp;
}

// Compute Lyapunov residual V_t and K,E,R from RiskVector and PlaneWeights.
//
// V_t = sum_j w_j * g_j(r_j), with:
//   - stronger penalties for PFAS, COLD, NEURORIGHTS planes.[4][6]
//   - K derived from fraction of planes with small residual.[4]
//   - E derived from eco-positive planes (low r in key corridors).[4]
//   - R as max risk coordinate.[4]
inline KerResidual compute_ker_residual(const RiskVector& rv,
                                        const PlaneWeights& pw) {
    const int p_high   = 4;
    const int p_medium = 3;

    double vt_sum = 0.0;

    for (std::size_t i = 0; i < RISK_PLANE_COUNT; ++i) {
        const double r = rv.r[i];
        const double w = pw.w[i];

        double alpha = 1.0;
        double beta  = 1.0;
        int    p     = p_medium;

        if (i == static_cast<std::size_t>(RiskPlane::PFAS) ||
            i == static_cast<std::size_t>(RiskPlane::COLD) ||
            i == static_cast<std::size_t>(RiskPlane::NEURORIGHTS)) {
            // Stronger tail penalty for PFAS, cold-survival, neurorights.[4][6]
            alpha = 1.0;
            beta  = 2.0;
            p     = p_high;
        }

        const double gj = super_quadratic_penalty(r, alpha, beta, p);
        vt_sum += w * gj;
    }

    // K,E,R heuristics aligned with existing KER grammar:
    //   - R = max r_j.[4]
    //   - K = 1 - average of r_j (knowledge high when risk low).[4]
    //   - E = 1 - R (ecoimpact high when max risk low).[4]
    double r_max = 0.0;
    double r_sum = 0.0;
    for (double r : rv.r) {
        if (r > r_max) {
            r_max = r;
        }
        r_sum += r;
    }
    const double r_avg = r_sum / static_cast<double>(RISK_PLANE_COUNT);

    KerResidual out;
    out.vt = vt_sum;
    out.r  = r_max;
    out.k  = 1.0 - r_avg;
    out.e  = 1.0 - r_max;

    if (out.k < 0.0) out.k = 0.0;
    if (out.e < 0.0) out.e = 0.0;

    return out;
}

// Always-improve check: new residual must not exceed previous residual + epsilon.
// This mirrors the CI proof obligation for energyreqJ and V_t.[4]
inline bool check_always_improve(const KerResidual& prev,
                                 const KerResidual& current,
                                 double epsilon = 1e-6) {
    return current.vt <= prev.vt + epsilon;
}

// Non-offsettable plane guard: returns true if CARBON/BIODIVERSITY/NEURORIGHTS
// are all within safe band.[4]
inline bool non_offsettable_safe(const RiskVector& rv) {
    const double r_carbon =
        rv.get(RiskPlane::CARBON);
    const double r_biod =
        rv.get(RiskPlane::BIODIVERSITY);
    const double r_neuro =
        rv.get(RiskPlane::NEURORIGHTS);

    const bool safe_carbon       = (r_carbon      <= 0.5);
    const bool safe_biodiversity = (r_biod       <= 0.5);
    const bool safe_neurorights  = (r_neuro      <= 0.0);

    return safe_carbon && safe_biodiversity && safe_neurorights;
}

} // namespace ecosafety_core_v2

#endif // ECORESTORATIONSHARD_ECOSAFETY_CORE_V2_KER_RESIDUAL_CORE_HPP
