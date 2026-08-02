// File: cpp/eco_restoration/material_circularity_potential.cpp
#include <iostream>
#include <string>
#include <cmath>

// Circularity potential of a material blend:
//   CP = (renewable_content × (1 − toxicity) × (1 − 1/(1+half_life_days))) / embodied_energy
//
// Variables:
//   - renewable_content ∈ [0,1]: fraction of inputs from renewable sources.
//   - toxicity ∈ [0,1]: normalized toxicity score (1 = highly toxic, 0 = non-toxic).
//   - half_life_days > 0: degradation half-life in days (shorter = more readily degradable).
//   - embodied_energy > 0: energy (MJ/kg or similar) required to produce the material.
//
// This module computes CP and sketches an interaction with a material eco-score model,
// including flags for knowledge-factor gaps.

struct MaterialProperties {
    std::string material_id;
    double renewable_content;   // [0,1]
    double toxicity;            // [0,1]
    double half_life_days;      // > 0
    double embodied_energy;     // > 0
};

struct CircularityResult {
    double cp_value;
    bool  missing_renewable;
    bool  missing_toxicity;
    bool  missing_half_life;
    bool  missing_embodied_energy;
};

double compute_cp_raw(double renewable_content,
                      double toxicity,
                      double half_life_days,
                      double embodied_energy) {
    if (embodied_energy <= 0.0) {
        return 0.0;
    }
    double term_renewable = renewable_content;
    double term_toxicity = 1.0 - toxicity;

    double decay_factor;
    if (half_life_days <= 0.0) {
        // Unknown or invalid half-life: treat as non-degradable in raw computation.
        decay_factor = 0.0;
    } else {
        decay_factor = 1.0 - 1.0 / (1.0 + half_life_days);
    }

    double numerator = term_renewable * term_toxicity * decay_factor;
    return numerator / embodied_energy;
}

CircularityResult compute_circularity_potential(const MaterialProperties& m) {
    CircularityResult res;
    res.missing_renewable = !(m.renewable_content >= 0.0 && m.renewable_content <= 1.0);
    res.missing_toxicity = !(m.toxicity >= 0.0 && m.toxicity <= 1.0);
    res.missing_half_life = !(m.half_life_days > 0.0);
    res.missing_embodied_energy = !(m.embodied_energy > 0.0);

    if (res.missing_renewable || res.missing_toxicity ||
        res.missing_half_life || res.missing_embodied_energy) {
        res.cp_value = 0.0;
        return res;
    }

    res.cp_value = compute_cp_raw(
        m.renewable_content,
        m.toxicity,
        m.half_life_days,
        m.embodied_energy
    );
    return res;
}

// Example interaction: map CP into a material eco-score band.
// In a full material_eco_score_model, CP would be one plane among others (carbon, toxicity, durability).
double map_cp_to_eco_score(double cp_value) {
    // Simple mapping: compress CP into [0,1] with a saturating function.
    double scaled = 1.0 - std::exp(-cp_value);
    if (scaled < 0.0) scaled = 0.0;
    if (scaled > 1.0) scaled = 1.0;
    return scaled;
}

int main() {
    MaterialProperties m;
    m.material_id = "phoenix_blend_A";
    m.renewable_content = 0.7;
    m.toxicity = 0.1;
    m.half_life_days = 60.0;
    m.embodied_energy = 50.0; // MJ/kg

    CircularityResult res = compute_circularity_potential(m);

    if (res.missing_renewable || res.missing_toxicity ||
        res.missing_half_life || res.missing_embodied_energy) {
        std::cout << "Knowledge-factor gap for material " << m.material_id << ": missing ";
        if (res.missing_renewable)      std::cout << "renewable_content ";
        if (res.missing_toxicity)       std::cout << "toxicity ";
        if (res.missing_half_life)      std::cout << "half_life_days ";
        if (res.missing_embodied_energy)std::cout << "embodied_energy ";
        std::cout << "\n";
    } else {
        double eco_score = map_cp_to_eco_score(res.cp_value);
        std::cout << "Material " << m.material_id
                  << " CP=" << res.cp_value
                  << " eco_score_from_CP=" << eco_score << "\n";
    }

    return 0;
}
