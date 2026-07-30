// filename: ecorestorationshard/ecosafety_core_v2/cpp/swarm_ker_gradient_control.hpp
// destination: ecorestorationshard/ecosafety_core_v2/cpp/swarm_ker_gradient_control.hpp
// repo-target: https://github.com/mk-bluebird/Prometheus-Praxis
//
// Purpose:
//   Non-actuating C++ skeleton for swarm-scale KER gradient following
//   with collision avoidance via artificial potential fields.
//   This header interacts with ecosafety_core_v2 risk/Lyapunov kernels,
//   but does not touch actuators.

#ifndef ECORESTORATIONSHARD_ECOSAFETY_CORE_V2_SWARM_KER_GRADIENT_CONTROL_HPP
#define ECORESTORATIONSHARD_ECOSAFETY_CORE_V2_SWARM_KER_GRADIENT_CONTROL_HPP

#include <vector>
#include <array>
#include <stdexcept>
#include <cmath>

namespace ecosafety_core_v2 {

struct RiskVectorSwarm {
    double r_energy;
    double r_pfas;
    double r_cold;
    double r_bod;
    double r_tss;
    double r_cec;
};

struct SwarmParticle {
    RiskVectorSwarm r;
};

struct SwarmAdjacencyEdge {
    std::size_t i;
    std::size_t j;
    double      weight;
};

struct SwarmControlParams {
    double step_size;   // eta
    double repulsion_gain; // kappa
};

// Placeholder: gradient of V_t with respect to risk vector.
// In practice, call your existing KER/Lyapunov kernel.[4]
inline RiskVectorSwarm grad_V(const RiskVectorSwarm& r) {
    // Simple example: gradient proportional to risk coordinates.
    RiskVectorSwarm g;
    g.r_energy = 2.0 * r.r_energy;
    g.r_pfas   = 2.0 * r.r_pfas;
    g.r_cold   = 2.0 * r.r_cold;
    g.r_bod    = 2.0 * r.r_bod;
    g.r_tss    = 2.0 * r.r_tss;
    g.r_cec    = 2.0 * r.r_cec;
    return g;
}

inline void apply_swarm_gradient_step(
    std::vector<SwarmParticle>& swarm,
    const std::vector<SwarmAdjacencyEdge>& edges,
    const SwarmControlParams& params)
{
    const std::size_t N = swarm.size();
    if (N == 0) {
        throw std::invalid_argument("Swarm is empty");
    }

    std::vector<RiskVectorSwarm> delta(N);

    // Gradient descent term.
    for (std::size_t i = 0; i < N; ++i) {
        const RiskVectorSwarm g = grad_V(swarm[i].r);
        delta[i].r_energy = -params.step_size * g.r_energy;
        delta[i].r_pfas   = -params.step_size * g.r_pfas;
        delta[i].r_cold   = -params.step_size * g.r_cold;
        delta[i].r_bod    = -params.step_size * g.r_bod;
        delta[i].r_tss    = -params.step_size * g.r_tss;
        delta[i].r_cec    = -params.step_size * g.r_cec;
    }

    // Repulsive artificial potential to avoid collisions in risk space.
    for (const auto& e : edges) {
        const std::size_t i = e.i;
        const std::size_t j = e.j;
        const double w = e.weight;

        const double dx_energy = swarm[i].r.r_energy - swarm[j].r.r_energy;
        const double dx_pfas   = swarm[i].r.r_pfas   - swarm[j].r.r_pfas;
        const double dx_cold   = swarm[i].r.r_cold   - swarm[j].r.r_cold;
        const double dx_bod    = swarm[i].r.r_bod    - swarm[j].r.r_bod;
        const double dx_tss    = swarm[i].r.r_tss    - swarm[j].r.r_tss;
        const double dx_cec    = swarm[i].r.r_cec    - swarm[j].r.r_cec;

        const double norm_sq =
            dx_energy*dx_energy + dx_pfas*dx_pfas + dx_cold*dx_cold +
            dx_bod*dx_bod + dx_tss*dx_tss + dx_cec*dx_cec;

        const double eps = 1e-6;
        const double inv = 1.0 / std::pow(norm_sq + eps, 1.5); // ~1/||dx||^3

        const double factor = params.repulsion_gain * w * inv;

        delta[i].r_energy += factor * dx_energy;
        delta[i].r_pfas   += factor * dx_pfas;
        delta[i].r_cold   += factor * dx_cold;
        delta[i].r_bod    += factor * dx_bod;
        delta[i].r_tss    += factor * dx_tss;
        delta[i].r_cec    += factor * dx_cec;

        // Equal and opposite repulsion for j.
        delta[j].r_energy -= factor * dx_energy;
        delta[j].r_pfas   -= factor * dx_pfas;
        delta[j].r_cold   -= factor * dx_cold;
        delta[j].r_bod    -= factor * dx_bod;
        delta[j].r_tss    -= factor * dx_tss;
        delta[j].r_cec    -= factor * dx_cec;
    }

    // Apply updates.
    for (std::size_t i = 0; i < N; ++i) {
        swarm[i].r.r_energy += delta[i].r_energy;
        swarm[i].r.r_pfas   += delta[i].r_pfas;
        swarm[i].r.r_cold   += delta[i].r_cold;
        swarm[i].r.r_bod    += delta[i].r_bod;
        swarm[i].r.r_tss    += delta[i].r_tss;
        swarm[i].r.r_cec    += delta[i].r.cec;
    }
}

} // namespace ecosafety_core_v2

#endif // ECORESTORATIONSHARD_ECOSAFETY_CORE_V2_SWARM_KER_GRADIENT_CONTROL_HPP
