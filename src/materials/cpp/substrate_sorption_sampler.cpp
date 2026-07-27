// filename: src/materials/cpp/substrate_sorption_sampler.cpp
// destination: Prometheus-Praxis/src/materials/cpp/substrate_sorption_sampler.cpp
// license: MIT OR Apache-2.0
// language: C++ (C17, non-actuating)

#include <cstddef>
#include <cstdint>
#include <cmath>

// Sorption parameters for a single contaminant (PFAS or CEC).[file:80]
struct SorptionParams {
    float k_ads;       // adsorption rate [volume / (mass * time)] simplified as [1/day] with normalized units
    float k_des;       // desorption rate [1/day]
    float M_capacity;  // maximum sorption capacity per substrate area [mass/area]
};

// Passive sampler configuration: exposure time and water concentration profile.[file:80]
struct PassiveSamplerConfig {
    float dt_days;     // integration time step [days]
    std::size_t steps; // number of integration steps
    float C_water;     // assumed constant water concentration [mass/volume] over window
};

// Output for one contaminant channel.[file:80]
struct SorptionSamplerOutput {
    float M_sorb_final;   // final sorbed mass per area [mass/area]
    float rsorbed_mass;   // normalized risk coordinate in [0,1]
};

// Risk corridor bands for sorbed mass, consistent with PFAS/CEC inventory grammar.[file:80]
struct SorbedMassCorridor {
    float safe;  // safe band upper bound [mass/area]
    float gold;  // gold band upper bound [mass/area]
    float hard;  // hard band upper bound [mass/area]
};

// Explicit Euler integration of dM_sorb/dt = k_ads*C_water - k_des*M_sorb.[file:80]
static float integrate_sorption(const SorptionParams& p,
                                const PassiveSamplerConfig& cfg)
{
    float M_sorb = 0.0f;

    const float dt = cfg.dt_days;
    const std::size_t n = cfg.steps;

    float k_ads = p.k_ads;
    float k_des = p.k_des;
    if (k_ads < 0.0f) {
        k_ads = 0.0f;
    }
    if (k_des < 0.0f) {
        k_des = 0.0f;
    }

    for (std::size_t i = 0; i < n; ++i) {
        float dMdt = k_ads * cfg.C_water - k_des * M_sorb;
        M_sorb += dt * dMdt;
        if (M_sorb < 0.0f) {
            M_sorb = 0.0f;
        }
        if (M_sorb > p.M_capacity) {
            M_sorb = p.M_capacity;
        }
    }

    return M_sorb;
}

// Normalize sorbed mass into rsorbed_mass in [0,1] using safegoldhard bands.[file:80]
static float normalize_sorbed_mass(float M_sorb,
                                   const SorbedMassCorridor& corridor)
{
    if (M_sorb <= corridor.safe) {
        return 0.0f;
    }
    if (M_sorb >= corridor.hard) {
        return 1.0f;
    }
    if (M_sorb <= corridor.gold) {
        float t = (M_sorb - corridor.safe) /
                  (corridor.gold - corridor.safe);
        return 0.5f * t; // safe→gold mapped into [0,0.5].[file:80]
    }
    float t = (M_sorb - corridor.gold) /
              (corridor.hard - corridor.gold);
    return 0.5f + 0.5f * t; // gold→hard mapped into [0.5,1].[file:80]
}

// Main kernel: sorption ODE + passive sampler, yielding rsorbed_mass for PFAS/CEC.[file:80]
extern "C" void substrate_sorption_sampler_run(const SorptionParams* params,
                                              const PassiveSamplerConfig* cfg,
                                              const SorbedMassCorridor* corridor,
                                              SorptionSamplerOutput* out)
{
    if (!params || !cfg || !corridor || !out) {
        return;
    }

    float M_sorb_final = integrate_sorption(*params, *cfg);
    float r = normalize_sorbed_mass(M_sorb_final, *corridor);

    out->M_sorb_final = M_sorb_final;
    out->rsorbed_mass = r;
}
