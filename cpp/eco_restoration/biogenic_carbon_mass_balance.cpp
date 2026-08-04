// File: cpp/eco_restoration/biogenic_carbon_mass_balance.cpp

#include <vector>
#include <stdexcept>
#include <sqlite3.h>

// Mass balance state for biogenic carbon loop
struct BiogenicCarbonState {
    double inflow_c_kg;        // PFAS/organic carbon entering basin (kg C)
    double sequestered_c_kg;   // carbon stored in biomass (kg C)
    double harvested_c_kg;     // biomass carbon harvested for bioenergy (kg C)
    double respired_c_kg;      // carbon respired back as CO2 (kg C)
};

// Compute biogenic net carbon impact over a step using degradation curves
double biogenicNetCarbonImpact(const BiogenicCarbonState& s) {
    // Net loop: sequestered_c_kg is temporarily stored; harvested_c_kg reused;
    // respired_c_kg returns to atmosphere.
    // Effective net emission = respired_c_kg - sequestered_c_kg_recalc (depending on time horizon).
    double net = s.respired_c_kg - s.sequestered_c_kg;
    return net;
}

// Update ker_e to account for biogenic reuse: subtract reused harvested carbon from emissions
double updateKerEWithBiogenic(double ker_e_base,
                              const BiogenicCarbonState& s,
                              double gwp_factor) {
    double net_c_kg = biogenicNetCarbonImpact(s);
    double co2eq_kg = net_c_kg * gwp_factor;
    // ker_e_base already includes fossil/operational carbon; adjust by biogenic net.
    double ker_e_new = ker_e_base + co2eq_kg / 1000.0; // scale to ker_e units
    return ker_e_new;
}
