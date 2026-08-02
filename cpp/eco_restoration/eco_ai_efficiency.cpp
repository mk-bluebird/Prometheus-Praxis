// File: cpp/eco_restoration/eco_ai_efficiency.cpp
#include <iostream>
#include <vector>
#include <cmath>
#include <string>

// Eco-AI efficiency metric:
//   E_eco = (ΔBI × ha) / (Joules_consumed_by_GPU)
//
// where:
//   - ΔBI: improvement in biodiversity index over a target area.
//   - ha : area in hectares influenced by the AI-driven action.
//   - Joules_consumed_by_GPU: energy used for AI inference and training.
//
// This module encodes the metric and sketches how to estimate ΔBI from
// species count models trained on iNaturalist observations.

struct BiodiversityIndexSnapshot {
    double shannon_index; // e.g., H' computed from species probabilities
    double richness;      // number of species detected
};

struct EnergyUsage {
    double gpu_joules;
};

double compute_E_eco(double delta_bi, double hectares, double gpu_joules) {
    if (gpu_joules <= 0.0) return 0.0;
    return (delta_bi * hectares) / gpu_joules;
}

// Species probability model output for a given hex or site.
struct SpeciesProbabilities {
    std::vector<std::string> species;
    std::vector<double> probabilities; // p_i for each species
};

// Compute Shannon biodiversity index from species probabilities.
double compute_shannon_index(const SpeciesProbabilities& sp) {
    double H = 0.0;
    for (double p : sp.probabilities) {
        if (p <= 0.0) continue;
        H -= p * std::log(p);
    }
    return H;
}

// Estimate ΔBI from iNaturalist-trained species models:
// - Train models to predict species presence probabilities per hex from environmental covariates.
// - Use pre- and post-intervention model outputs to approximate change in biodiversity index.
double estimate_delta_bi(const SpeciesProbabilities& pre,
                         const SpeciesProbabilities& post) {
    double H_pre = compute_shannon_index(pre);
    double H_post = compute_shannon_index(post);
    return H_post - H_pre;
}

int main() {
    // Example: pre- and post-intervention biodiversity modeled from iNaturalist observations.
    SpeciesProbabilities pre;
    SpeciesProbabilities post;

    pre.species = {"Quercus", "Pinus", "Larrea"};
    post.species = pre.species;

    pre.probabilities = {0.5, 0.3, 0.2};
    post.probabilities = {0.4, 0.35, 0.25};

    double delta_bi = estimate_delta_bi(pre, post);
    double hectares = 10.0;
    EnergyUsage energy{1.5e8}; // 150 MJ consumed by GPU

    double Eeco = compute_E_eco(delta_bi, hectares, energy.gpu_joules);

    std::cout << "ΔBI = " << delta_bi << "\n";
    std::cout << "E_eco = " << Eeco << " (ΔBI·ha per Joule)\n";

    return 0;
}
