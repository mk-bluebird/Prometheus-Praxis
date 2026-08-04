// File: cpp/eco_restoration/albedo_error_propagation.cpp

#include <cmath>
#include <iostream>

// Simple struct for propagation parameters over long-term ker_e
struct AlbedoChainParams {
    double alpha_lst;    // dLST/dAlpha (°C per albedo unit)
    double beta_energy;  // dEnergy/dLST (energy per °C)
    double gamma_ker_e;  // dKerE/dEnergy (ker_e per energy unit)
};

// Compute long-term bias in cumulative ker_e over horizon years due to systematic albedo error.
double kerEBiasOverYears(double albedo_error,
                         const AlbedoChainParams& params,
                         double baseline_daily_energy,
                         int years) {
    // Single-step bias in ker_e per day:
    double dKerE_per_day = params.gamma_ker_e
                           * params.beta_energy
                           * params.alpha_lst
                           * albedo_error;

    // Approximate cumulative bias over years with 365 days per year
    double days = 365.0 * static_cast<double>(years);
    double cumulative_bias = dKerE_per_day * days;
    return cumulative_bias;
}

// Compute required recalibration interval (years) so that relative error in cumulative ker_e
// stays below epsilon (e.g., 0.05 for 5% over 10 years).
double requiredRecalibrationYears(double albedo_error,
                                  const AlbedoChainParams& params,
                                  double baseline_daily_energy,
                                  double epsilon,
                                  int totalYears) {
    // Cumulative ker_e baseline over totalYears:
    double baseline_kerE_per_day =
        params.gamma_ker_e * baseline_daily_energy;
    double baseline_cumulative =
        baseline_kerE_per_day * 365.0 * static_cast<double>(totalYears);

    // Allowed absolute bias:
    double allowed_bias = epsilon * baseline_cumulative;

    // Bias per year from albedo error:
    double bias_per_year = kerEBiasOverYears(albedo_error, params, baseline_daily_energy, 1);

    // Required recalibration interval:
    if (bias_per_year <= 0.0) return static_cast<double>(totalYears);
    double years_between_recalibration = allowed_bias / bias_per_year;
    if (years_between_recalibration > totalYears) years_between_recalibration = static_cast<double>(totalYears);
    return years_between_recalibration;
}

int main() {
    AlbedoChainParams params;
    params.alpha_lst   = -10.0;  // °C drop per albedo unit (example)
    params.beta_energy = 0.5;    // energy reduction per °C
    params.gamma_ker_e = 1.0;    // ker_e scaling

    double albedo_error = 0.02; // systematic A0/A1 error
    double baseline_daily_energy = 100.0; // arbitrary units
    int years = 10;

    double bias10 = kerEBiasOverYears(albedo_error, params, baseline_daily_energy, years);
    double recYears = requiredRecalibrationYears(albedo_error, params, baseline_daily_energy, 0.05, years);

    std::cout << "Cumulative ker_e bias over " << years << " years: " << bias10 << "\n";
    std::cout << "Recalibration interval (years) to keep error <5%: " << recYears << "\n";
    return 0;
}
