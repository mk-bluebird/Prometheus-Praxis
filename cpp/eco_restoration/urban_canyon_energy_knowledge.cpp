// File: cpp/eco_restoration/urban_canyon_energy_knowledge.cpp
#include <iostream>
#include <vector>
#include <string>
#include <cmath>

// Dynamic energy balance for an urban canyon:
//   ΔQ_stored = (1 − albedo)·S↓ + L↓ − L↑ − H − LE
//
// Terms:
//   - albedo      : shortwave reflectance of canyon surfaces.
//   - S_down (S↓) : incoming shortwave radiation.
//   - L_down (L↓) : incoming longwave radiation.
//   - L_up   (L↑) : outgoing longwave radiation (surface thermal emission).
//   - H           : sensible heat flux.
//   - LE          : latent heat flux.
//
// H and LE are parameterized by per-surface roughness and moisture availability, e.g.:
//   H  ≈ ρ_air * c_p * C_H(roughness) * U * (T_surface − T_air)
//   LE ≈ ρ_air * L_v * C_E(roughness, moisture) * U * (q_surface − q_air)
//
// This module computes ΔQ_stored and defines a knowledge factor as the fraction
// of energy-balance terms that are directly measured vs. modeled.

struct EnergyTerms {
    double albedo;      // dimensionless
    double S_down;      // W/m^2
    double L_down;      // W/m^2
    double L_up;        // W/m^2
    double H;           // W/m^2
    double LE;          // W/m^2
};

struct MeasurementFlags {
    bool albedo_measured;
    bool S_down_measured;
    bool L_down_measured;
    bool L_up_measured;
    bool H_measured;
    bool LE_measured;
};

struct KnowledgeAssessment {
    double delta_Q_stored; // W/m^2
    double knowledge_factor; // fraction of directly measured terms in [0,1]
};

// Compute ΔQ_stored from energy terms.
double compute_delta_Q_stored(const EnergyTerms& terms) {
    double absorbed_shortwave = (1.0 - terms.albedo) * terms.S_down;
    return absorbed_shortwave + terms.L_down - terms.L_up - terms.H - terms.LE;
}

// Compute knowledge factor: fraction of terms directly measured.
// We count each term (albedo, S↓, L↓, L↑, H, LE) as one unit; KF = (#measured) / 6.
double compute_knowledge_factor(const MeasurementFlags& flags) {
    int total_terms = 6;
    int measured_terms = 0;
    if (flags.albedo_measured)  ++measured_terms;
    if (flags.S_down_measured)  ++measured_terms;
    if (flags.L_down_measured)  ++measured_terms;
    if (flags.L_up_measured)    ++measured_terms;
    if (flags.H_measured)       ++measured_terms;
    if (flags.LE_measured)      ++measured_terms;
    return static_cast<double>(measured_terms) / static_cast<double>(total_terms);
}

// Example: parameterize H and LE using roughness and moisture when not measured.
double parameterize_H(double rho_air, double cp,
                      double C_H, double U,
                      double T_surface, double T_air) {
    return rho_air * cp * C_H * U * (T_surface - T_air);
}

double parameterize_LE(double rho_air, double L_v,
                       double C_E, double U,
                       double q_surface, double q_air) {
    return rho_air * L_v * C_E * U * (q_surface - q_air);
}

int main() {
    EnergyTerms terms;
    terms.albedo  = 0.25;
    terms.S_down  = 650.0;
    terms.L_down  = 400.0;
    terms.L_up    = 520.0;

    double rho_air = 1.2;
    double cp      = 1005.0;
    double L_v     = 2.45e6;
    double U       = 2.0;
    double T_surf  = 320.0;
    double T_air   = 310.0;
    double q_surf  = 0.018;
    double q_air   = 0.012;
    double C_H     = 0.015; // roughness-dependent bulk transfer for H
    double C_E     = 0.010; // roughness + moisture-dependent bulk transfer for LE

    terms.H  = parameterize_H(rho_air, cp, C_H, U, T_surf, T_air);
    terms.LE = parameterize_LE(rho_air, L_v, C_E, U, q_surf, q_air);

    MeasurementFlags flags;
    flags.albedo_measured  = true;
    flags.S_down_measured  = true;
    flags.L_down_measured  = false; // modeled from sky temp
    flags.L_up_measured    = false; // modeled from surface temp
    flags.H_measured       = false; // modeled via roughness
    flags.LE_measured      = false; // modeled via moisture

    KnowledgeAssessment assess;
    assess.delta_Q_stored   = compute_delta_Q_stored(terms);
    assess.knowledge_factor = compute_knowledge_factor(flags);

    std::cout << "ΔQ_stored = " << assess.delta_Q_stored << " W/m^2\n";
    std::cout << "Knowledge factor = " << assess.knowledge_factor << "\n";

    return 0;
}
