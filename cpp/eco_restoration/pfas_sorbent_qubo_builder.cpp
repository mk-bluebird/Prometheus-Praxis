// File: cpp/eco_restoration/pfas_sorbent_qubo_builder.cpp
// Repo path: cpp/eco_restoration/pfas_sorbent_qubo_builder.cpp
//
// Purpose:
//   Non-actuating C++ QUBO builder for PFAS sorbent deployment along
//   a 20-segment canal reach:
//     - Variables x_i ∈ {0,1}, i=0..N-1 (N segments).
//     - Objective: minimise PFAS mobile mass and cold-survival risk,
//       aligned with Lyapunov/KER corridors.
//   Outputs a QUBO matrix Q and linear term c suitable for quantum
//   annealing.

#include <array>
#include <iostream>
#include <vector>
#include <stdexcept>

struct QUBO {
    std::vector<std::vector<double>> Q;  // QUBO matrix (NxN)
    std::vector<double> c;               // linear term (N)
};

// Simple QUBO builder:
// - N segments.
// - base_mobile_mass[i]: PFAS mobile mass without sorbent.
// - cold_survival[i]: cold-survival factor (0..1).
// - lambda_mass: weight for mass reduction.
// - lambda_cold: weight for cold-survival mitigation.
QUBO build_pfas_sorbent_qubo(const std::vector<double>& base_mobile_mass,
                             const std::vector<double>& cold_survival,
                             double lambda_mass,
                             double lambda_cold)
{
    const std::size_t N = base_mobile_mass.size();
    if (cold_survival.size() != N) {
        throw std::invalid_argument("Input vectors must have same length");
    }

    QUBO qubo;
    qubo.Q.assign(N, std::vector<double>(N, 0.0));
    qubo.c.assign(N, 0.0);

    // Linear term: placing sorbent at i reduces mobile mass and cold-survival risk.
    for (std::size_t i = 0; i < N; ++i) {
        double mass_term = -lambda_mass * base_mobile_mass[i];   // negative: we want sorbent where mass is high
        double cold_term = -lambda_cold * cold_survival[i];      // negative: we want sorbent where cold-survival is high
        qubo.c[i] = mass_term + cold_term;
    }

    // Quadratic term: interactions between neighbouring segments.
    // Encourage contiguous sorbent placement (smooth deployment) and encode
    // downstream PFAS coupling.
    const double neighbor_coupling = lambda_mass * 0.5; // tune as needed
    for (std::size_t i = 0; i + 1 < N; ++i) {
        qubo.Q[i][i + 1] += neighbor_coupling;
        qubo.Q[i + 1][i] += neighbor_coupling;
    }

    // Optional self-penalty to avoid placing sorbent everywhere (cost).
    const double self_penalty = lambda_mass * 0.1;
    for (std::size_t i = 0; i < N; ++i) {
        qubo.Q[i][i] += self_penalty;
    }

    return qubo;
}

int main() {
    // Example data for 20 segments.
    const std::size_t N = 20;
    std::vector<double> base_mobile_mass(N);
    std::vector<double> cold_survival(N);

    // Populate with illustrative values; in practice, pull from KER/SQL.
    for (std::size_t i = 0; i < N; ++i) {
        base_mobile_mass[i] = 1.0 + 0.1 * i;  // increasing mass downstream
        cold_survival[i]    = 0.5;            // uniform cold-survival factor
    }

    double lambda_mass = 1.0;
    double lambda_cold = 0.5;

    QUBO qubo = build_pfas_sorbent_qubo(base_mobile_mass, cold_survival,
                                        lambda_mass, lambda_cold);

    // Output QUBO in a simple text format (e.g., for quantum annealer).
    std::cout << "QUBO matrix (" << N << "x" << N << "):\n";
    for (std::size_t i = 0; i < N; ++i) {
        for (std::size_t j = 0; j < N; ++j) {
            std::cout << qubo.Q[i][j] << (j + 1 < N ? " " : "");
        }
        std::cout << "\n";
    }

    std::cout << "Linear term c:\n";
    for (std::size_t i = 0; i < N; ++i) {
        std::cout << qubo.c[i] << (i + 1 < N ? " " : "");
    }
    std::cout << "\n";

    return 0;
}
