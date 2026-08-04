// File: cpp/eco_restoration/net_ecosystem_carbon_balance.cpp

#include <iostream>
#include <vector>
#include <string>
#include <cmath>
#include <stdexcept>
#include <limits>

// Net ecosystem carbon balance (NECB) with CO2, CH4, N2O in CO2-equivalents,
// using conservative GWP values and uncertainty propagation.
//
// Flux inputs are per unit area and time (e.g., g/m^2/day or kg/ha/day), with
// associated uncertainties. This module computes:
//
//   NECB_CO2eq = CO2_flux
//                + GWP_CH4 * CH4_flux
//                + GWP_N2O * N2O_flux
//
// and an uncertainty σ_NECB via linear error propagation. A conservative
// carbon-negative claim condition is:
//
//   NECB_CO2eq + z_95 * σ_NECB <= 0
//
// where z_95 ≈ 1.96 is the 95% confidence quantile for a normal approximation.
//
// The "carbon plane coordinate" is a scalar representing net balance, used
// by governance (KER) to decide whether a site is verifiably carbon-negative.

struct GasFlux {
    double co2_flux;   // net CO2 flux (positive = emission, negative = uptake)
    double ch4_flux;   // net CH4 flux
    double n2o_flux;   // net N2O flux

    double co2_sigma;  // std dev / uncertainty for CO2 flux
    double ch4_sigma;  // uncertainty for CH4 flux
    double n2o_sigma;  // uncertainty for N2O flux

    // Optional metadata
    std::string timestamp;
    std::string site_id;
};

// Conservative GWP parameters (e.g., long-horizon, high-end estimates).
struct GwpParams {
    double gwp_ch4;  // CO2-eq per unit CH4
    double gwp_n2o;  // CO2-eq per unit N2O
};

// Result coordinate in carbon plane.
struct CarbonPlaneCoordinate {
    double nec_co2eq;      // net ecosystem carbon balance in CO2-eq units
    double nec_sigma;      // uncertainty (std dev)
    double z_critical;     // z-value used (e.g., 1.96)
    bool   is_carbon_negative_95; // NECB + z * σ <= 0
};

// Compute NECB and uncertainty for a single flux observation.
CarbonPlaneCoordinate compute_necb_coordinate(const GasFlux& flux,
                                              const GwpParams& gwp,
                                              double z_critical = 1.96) {
    // CO2-eq balance.
    double nec = flux.co2_flux
                 + gwp.gwp_ch4 * flux.ch4_flux
                 + gwp.gwp_n2o * flux.n2o_flux;

    // Uncertainty propagation assuming independent flux errors:
    // σ_NECB^2 = σ_CO2^2 + (GWP_CH4^2 * σ_CH4^2) + (GWP_N2O^2 * σ_N2O^2).
    double var_co2 = flux.co2_sigma * flux.co2_sigma;
    double var_ch4 = gwp.gwp_ch4 * gwp.gwp_ch4 * flux.ch4_sigma * flux.ch4_sigma;
    double var_n2o = gwp.gwp_n2o * gwp.gwp_n2o * flux.n2o_sigma * flux.n2o_sigma;
    double nec_var = var_co2 + var_ch4 + var_n2o;
    if (nec_var < 0.0) nec_var = 0.0;
    double nec_sigma = std::sqrt(nec_var);

    bool negative_95 = (nec + z_critical * nec_sigma <= 0.0);

    CarbonPlaneCoordinate coord;
    coord.nec_co2eq = nec;
    coord.nec_sigma = nec_sigma;
    coord.z_critical = z_critical;
    coord.is_carbon_negative_95 = negative_95;
    return coord;
}

// Aggregation over multiple flux samples (e.g., day/season/site):
// Sum fluxes and propagate uncertainties assuming independence.
CarbonPlaneCoordinate aggregate_necb_coordinates(const std::vector<GasFlux>& fluxes,
                                                 const GwpParams& gwp,
                                                 double z_critical = 1.96) {
    if (fluxes.empty()) {
        throw std::invalid_argument("fluxes must not be empty");
    }

    double sum_co2 = 0.0;
    double sum_ch4 = 0.0;
    double sum_n2o = 0.0;
    double var_co2 = 0.0;
    double var_ch4 = 0.0;
    double var_n2o = 0.0;

    for (const auto& f : fluxes) {
        sum_co2 += f.co2_flux;
        sum_ch4 += f.ch4_flux;
        sum_n2o += f.n2o_flux;

        var_co2 += f.co2_sigma * f.co2_sigma;
        var_ch4 += f.ch4_sigma * f.ch4_sigma;
        var_n2o += f.n2o_sigma * f.n2o_sigma;
    }

    GasFlux agg;
    agg.co2_flux = sum_co2;
    agg.ch4_flux = sum_ch4;
    agg.n2o_flux = sum_n2o;
    agg.co2_sigma = std::sqrt(var_co2);
    agg.ch4_sigma = std::sqrt(var_ch4);
    agg.n2o_sigma = std::sqrt(var_n2o);

    return compute_necb_coordinate(agg, gwp, z_critical);
}

// Sidecar-compatible ingestion interface: convert raw sensor readings
// from open-path sensors into GasFlux records with simple uncertainty heuristics.
struct RawSensorSample {
    double co2_flux_raw;
    double ch4_flux_raw;
    double n2o_flux_raw;
    double co2_flux_std;
    double ch4_flux_std;
    double n2o_flux_std;
    std::string timestamp;
    std::string site_id;
};

GasFlux from_raw_sensor(const RawSensorSample& sample) {
    GasFlux f;
    f.co2_flux = sample.co2_flux_raw;
    f.ch4_flux = sample.ch4_flux_raw;
    f.n2o_flux = sample.n2o_flux_raw;
    f.co2_sigma = sample.co2_flux_std;
    f.ch4_sigma = sample.ch4_flux_std;
    f.n2o_sigma = sample.n2o_flux_std;
    f.timestamp = sample.timestamp;
    f.site_id = sample.site_id;
    return f;
}

// Example governance decision helper: determine whether a site is
// verifiably carbon-negative at 95% confidence, and print explanation.
void report_site_carbon_status(const CarbonPlaneCoordinate& coord,
                               const std::string& site_id) {
    std::cout << "Site " << site_id << " NECB_CO2eq=" << coord.nec_co2eq
              << " ± " << coord.z_critical * coord.nec_sigma
              << " (95% interval)" << std::endl;
    if (coord.is_carbon_negative_95) {
        std::cout << "Status: VERIFIABLY CARBON-NEGATIVE at 95% confidence." << std::endl;
    } else {
        std::cout << "Status: NOT verifiably carbon-negative at 95% confidence." << std::endl;
    }
}

int main() {
    // Conservative GWP parameters (example values; in practice use IPCC or project-specific).
    GwpParams gwp;
    gwp.gwp_ch4 = 30.0;   // CH4 ~30x CO2 over 100-year horizon (conservative)
    gwp.gwp_n2o = 265.0;  // N2O ~265x CO2 over 100-year horizon (conservative)

    // Synthetic raw sensor data for demonstration; real data comes from open-path sidecar.
    std::vector<RawSensorSample> raw_samples = {
        { -100.0,  -0.5,  -0.05, 10.0, 0.1, 0.01, "2026-08-04T00:00:00Z", "basin_A" },
        { -90.0,   -0.4,  -0.04, 12.0, 0.1, 0.01, "2026-08-04T01:00:00Z", "basin_A" },
        { -110.0,  -0.6,  -0.06, 11.0, 0.1, 0.01, "2026-08-04T02:00:00Z", "basin_A" }
    };

    std::vector<GasFlux> fluxes;
    for (const auto& rs : raw_samples) {
        fluxes.push_back(from_raw_sensor(rs));
    }

    CarbonPlaneCoordinate coord = aggregate_necb_coordinates(fluxes, gwp, 1.96);
    report_site_carbon_status(coord, "basin_A");

    return 0;
}
