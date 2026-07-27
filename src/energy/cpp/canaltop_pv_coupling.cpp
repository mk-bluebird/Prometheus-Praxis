// filename: src/energy/cpp/canaltop_pv_coupling.cpp
// destination: Prometheus-Praxis/src/energy/cpp/canaltop_pv_coupling.cpp
// license: MIT OR Apache-2.0
// language: C++ (C17, non-actuating)

#include <cstddef>
#include <cstdint>
#include <cmath>

// Input describing canal-top PV configuration and canal baseline.[file:80]
struct CanalTopPvInput {
    double pv_coverage_fraction;    // fraction of canal width covered by PV (0..1).
    double baseline_evap_mm_per_day; // baseline evaporation [mm/day] without PV.
    double baseline_temp_C;         // baseline water temperature [C] without PV.
    double canal_width_m;          // canal surface width [m].
    double canal_length_m;         // reach length [m].
    double node_energy_kwh_per_day; // electrical energy produced at node [kWh/day]. E_node.[file:80]
    double grid_intensity_kg_per_kwh; // g_grid as kg CO2e per kWh.[file:80]
};

// Corridor bands for energy and carbon risk.[file:80]
struct EnergyCorridor {
    double safe;  // safe band upper bound for energy intensity index.
    double gold;  // gold band upper bound.
    double hard;  // hard band upper bound.
};

struct CarbonEnergyCorridor {
    double safe;  // safe band upper bound for M_avoided index.
    double gold;  // gold band upper bound.
    double hard;  // hard band upper bound.
};

// Output: shading benefits and normalized risk coordinates.[file:80]
struct CanalTopPvOutput {
    double evap_reduction_mm_per_day; // reduction in evaporation [mm/day].
    double temp_reduction_C;          // reduction in water temperature [C].
    double M_avoided_kg_per_day;      // avoided grid emissions [kg CO2e/day].
    double renergy;                   // normalized energy-plane coordinate 0..1.
    double rcarbonenergy;             // normalized carbon-plane coordinate 0..1.
};

// Simple safegoldhard normalization helpers.[file:80]
static double normalize_index(double x, const EnergyCorridor& band)
{
    if (!std::isfinite(x)) {
        return 1.0;
    }
    if (x <= band.safe) {
        return 0.0;
    }
    if (x >= band.hard) {
        return 1.0;
    }
    if (x <= band.gold) {
        const double t = (x - band.safe) / (band.gold - band.safe);
        return 0.5 * t;
    }
    const double t = (x - band.gold) / (band.hard - band.gold);
    return 0.5 + 0.5 * t;
}

static double normalize_carbon_energy(double x, const CarbonEnergyCorridor& band)
{
    // Here x is an intensity or index derived from M_avoided.[file:80]
    if (!std::isfinite(x)) {
        return 1.0;
    }
    if (x <= band.safe) {
        return 0.0;
    }
    if (x >= band.hard) {
        return 1.0;
    }
    if (x <= band.gold) {
        const double t = (x - band.safe) / (band.gold - band.safe);
        return 0.5 * t;
    }
    const double t = (x - band.gold) / (band.hard - band.gold);
    return 0.5 + 0.5 * t;
}

// Estimate evaporation reduction from PV shading.[file:80]
static double compute_evap_reduction(const CanalTopPvInput& in)
{
    // Linear scaling: full coverage halves evaporation by shading and reduced insolation.[file:80]
    const double max_factor = 0.5;
    double frac = in.pv_coverage_fraction;
    if (frac < 0.0) {
        frac = 0.0;
    }
    if (frac > 1.0) {
        frac = 1.0;
    }
    return in.baseline_evap_mm_per_day * max_factor * frac;
}

// Estimate temperature reduction from PV shading.[file:80]
static double compute_temp_reduction(const CanalTopPvInput& in)
{
    // Simple linear proxy: full coverage drops a few degrees.[file:80]
    const double max_drop_C = 3.0;
    double frac = in.pv_coverage_fraction;
    if (frac < 0.0) {
        frac = 0.0;
    }
    if (frac > 1.0) {
        frac = 1.0;
    }
    return max_drop_C * frac;
}

// Compute avoided grid emissions M_avoided = g_grid * E_node.[file:80]
static double compute_M_avoided(const CanalTopPvInput& in)
{
    return in.grid_intensity_kg_per_kwh * in.node_energy_kwh_per_day;
}

// Compute an energy intensity index per canal area from node energy.[file:80]
static double compute_energy_index(const CanalTopPvInput& in)
{
    const double area_m2 = in.canal_width_m * in.canal_length_m;
    if (area_m2 <= 0.0) {
        return in.node_energy_kwh_per_day;
    }
    return in.node_energy_kwh_per_day / area_m2; // kWh/day/m^2.[file:80]
}

// Compute a carbon-energy index per canal area from M_avoided.[file:80]
static double compute_carbon_energy_index(double M_avoided_kg_per_day,
                                           const CanalTopPvInput& in)
{
    const double area_m2 = in.canal_width_m * in.canal_length_m;
    if (area_m2 <= 0.0) {
        return M_avoided_kg_per_day;
    }
    return M_avoided_kg_per_day / area_m2; // kg CO2e/day/m^2.[file:80]
}

// Main kernel: canal-top PV shading + energy/carbon normalization.[file:80]
extern "C" void canaltop_pv_coupling_run(const CanalTopPvInput* in,
                                         const EnergyCorridor* energy_band,
                                         const CarbonEnergyCorridor* carbon_band,
                                         CanalTopPvOutput* out)
{
    if (!in || !energy_band || !carbon_band || !out) {
        return;
    }

    const double evap_red = compute_evap_reduction(*in);
    const double temp_red = compute_temp_reduction(*in);
    const double M_avoided = compute_M_avoided(*in);

    const double e_idx  = compute_energy_index(*in);
    const double c_idx  = compute_carbon_energy_index(M_avoided, *in);

    out->evap_reduction_mm_per_day = evap_red;
    out->temp_reduction_C          = temp_red;
    out->M_avoided_kg_per_day      = M_avoided;
    out->renergy                   = normalize_index(e_idx, *energy_band);
    out->rcarbonenergy             = normalize_carbon_energy(c_idx, *carbon_band);
}
