// filename: src/energy/cpp/battery_lifecycle_carbon.cpp
// destination: Prometheus-Praxis/src/energy/cpp/battery_lifecycle_carbon.cpp
// license: MIT OR Apache-2.0
// language: C++ (C17, non-actuating)

#include <cstddef>
#include <cstdint>
#include <cmath>

// Battery chemistry enum.[file:80]
enum class BatteryChemistry : std::uint8_t {
    NaIon,
    LFP,
    NMC
};

// Per-chemistry LCA inputs for manufacturing and use.[file:80]
struct BatteryLcaEmfgUse {
    BatteryChemistry chemistry;
    double kg_co2_manufacturing; // cradle-to-gate per pack [kg CO2e]
    double kwh_throughput;       // total delivered energy over life Elife [kWh].[file:80]
    double grid_intensity_g_per_kwh; // operational grid intensity [g CO2e/kWh].[file:80]
};

// End-of-life LCA inputs.[file:80]
struct BatteryLcaEeol {
    BatteryChemistry chemistry;
    double kg_co2_eol_baseline;  // EoL emissions if no recycling [kg CO2e].
    double recycling_fraction;   // fraction of materials recovered 0..1.[file:80]
    double avoided_kg_co2;       // emissions avoided due to recovered materials [kg CO2e].
};

// Corridor bands for carbon risk normalization.[file:80]
struct CarbonCorridor {
    double safe;  // safe band upper bound [kg CO2e per kWh or per pack].
    double gold;  // gold band upper bound.
    double hard;  // hard band upper bound.
};

// Output: normalized carbon risk coordinates.[file:80]
struct BatteryCarbonOutput {
    double rcarbon_emfguse; // manufacturing+use carbon risk coordinate 0..1.
    double rcarbon_eeol;    // end-of-life carbon risk coordinate 0..1.
};

// Piecewise safegoldhard normalization.[file:80]
static double normalize_carbon(double x, const CarbonCorridor& band)
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

// Compute net kg CO2e per delivered kWh for manufacturing+use.[file:80]
static double compute_emfguse_intensity(const BatteryLcaEmfgUse& in)
{
    if (in.kwh_throughput <= 0.0) {
        return in.kg_co2_manufacturing;
    }

    // Manufacturing intensity per kWh.[file:80]
    const double emfg_per_kwh = in.kg_co2_manufacturing / in.kwh_throughput;

    // Use-phase intensity: grid intensity in g/kWh → kg/kWh.[file:80]
    const double euse_per_kwh = in.grid_intensity_g_per_kwh * 1e-3;

    // Total lifecycle intensity per delivered kWh.[file:80]
    return emfg_per_kwh + euse_per_kwh;
}

// Compute net end-of-life kg CO2e per pack.[file:80]
static double compute_eeol_net(const BatteryLcaEeol& in)
{
    double rf = in.recycling_fraction;
    if (rf < 0.0) {
        rf = 0.0;
    }
    if (rf > 1.0) {
        rf = 1.0;
    }

    // Recycling reduces baseline EoL emissions and adds avoided burdens.[file:80]
    const double residual_eol = in.kg_co2_eol_baseline * (1.0 - rf);
    const double net = residual_eol - in.avoided_kg_co2;

    return net;
}

// Main kernel: compute rcarbonemfguse and rcarboneeol for Na-ion, LFP, NMC.[file:80]
extern "C" void battery_lifecycle_carbon_run(const BatteryLcaEmfgUse* emfguse,
                                             const CarbonCorridor* corridor_emfguse,
                                             const BatteryLcaEeol* eeol,
                                             const CarbonCorridor* corridor_eeol,
                                             BatteryCarbonOutput* out)
{
    if (!emfguse || !corridor_emfguse || !eeol || !corridor_eeol || !out) {
        return;
    }

    const double emfguse_intensity = compute_emfguse_intensity(*emfguse);
    const double eeol_net          = compute_eeol_net(*eeol);

    out->rcarbon_emfguse = normalize_carbon(emfguse_intensity, *corridor_emfguse);
    out->rcarboneeol     = normalize_carbon(eeol_net, *corridor_eeol);
}
