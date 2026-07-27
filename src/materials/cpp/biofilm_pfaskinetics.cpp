// filename: src/materials/cpp/biofilm_pfaskinetics.cpp
// destination: Prometheus-Praxis/src/materials/cpp/biofilm_pfaskinetics.cpp
// license: MIT OR Apache-2.0
// language: C++ (C17, non-actuating)

#include <cstddef>
#include <cstdint>
#include <cmath>

// Phoenix canal biofilm context: biomass and guild-level PFAS behaviour.[file:80]
struct BiofilmGuildDescriptor {
    float biomass;          // biofilm biomass per area [g/m^2]
    float hydrophobicity;   // dimensionless 0..1, PFAS affinity
    float turnover_rate;    // biomass turnover [1/day]
    float exudate_factor;   // exudate-driven mobilisation factor 0..1
};

// Canal environmental modifiers (Phoenix matrix water).[file:80]
struct CanalEnvironment {
    float temperature_C;    // 30-40 C typical Phoenix canals.[file:80]
    float pH;               // 7.8-8.5 alkaline.[file:80]
    float hardness_mgL;     // high hardness indicator.[file:80]
    float flow_m_s;         // local flow velocity [m/s]
};

// PFAS mobilisation risk corridors for rbiofilmpfas.[file:80]
struct BiofilmPfasCorridor {
    float safe;  // safe band upper PFAS mobilisation index
    float gold;  // gold band upper index
    float hard;  // hard band upper index
};

// Output: PFAS mobilisation index and normalized rbiofilmpfas.[file:80]
struct BiofilmPfasOutput {
    float mobilisation_index; // dimensionless 0..1 PFAS mobilisation potential
    float rbiofilmpfas;       // normalized risk coordinate in [0,1]
};

// Temperature and pH modifiers for biofilm activity.[file:80]
static float env_activity_modifier(const CanalEnvironment& env)
{
    // Simple bell-shaped response around Phoenix matrix (35 C, pH 8.2).[file:80]
    float dT = (env.temperature_C - 35.0f) / 10.0f;
    float dP = (env.pH - 8.2f) / 0.5f;

    float fT = std::exp(-dT * dT);
    float fP = std::exp(-dP * dP);

    // Hardness modestly increases PFAS sorption but also potential mobilisation.[file:80]
    float fH = 1.0f + 0.0005f * env.hardness_mgL;
    if (fH > 1.5f) {
        fH = 1.5f;
    }

    float m = fT * fP * fH;
    if (m < 0.0f) {
        m = 0.0f;
    }
    if (m > 2.0f) {
        m = 2.0f;
    }
    return m;
}

// Compute a PFAS mobilisation index from biofilm guild and canal environment.[file:80]
static float compute_mobilisation_index(const BiofilmGuildDescriptor& guild,
                                        const CanalEnvironment& env)
{
    // Base propensity from biomass, hydrophobicity, and exudate.[file:80]
    float base = guild.biomass * guild.hydrophobicity * guild.exudate_factor;

    // Turnover increases mobilisation via sloughing.[file:80]
    float turnover = guild.turnover_rate;
    if (turnover < 0.0f) {
        turnover = 0.0f;
    }
    float f_turnover = 1.0f + 0.5f * turnover;

    // Flow-driven detachment.[file:80]
    float f_flow = 1.0f + 2.0f * env.flow_m_s;
    if (f_flow < 0.0f) {
        f_flow = 0.0f;
    }

    float env_mod = env_activity_modifier(env);

    float idx = base * f_turnover * f_flow * env_mod;

    // Normalize to 0..1 by a conservative scaling constant.[file:80]
    float scale = 1.0f;
    if (idx <= 0.0f) {
        return 0.0f;
    }
    float norm = idx / (idx + scale);
    if (norm > 1.0f) {
        norm = 1.0f;
    }
    return norm;
}

// Safegoldhard normalization into rbiofilmpfas.[file:80]
static float normalize_rbiofilmpfas(float mobilisation_index,
                                    const BiofilmPfasCorridor& corridor)
{
    if (mobilisation_index <= corridor.safe) {
        return 0.0f;
    }
    if (mobilisation_index >= corridor.hard) {
        return 1.0f;
    }
    if (mobilisation_index <= corridor.gold) {
        float t = (mobilisation_index - corridor.safe) /
                  (corridor.gold - corridor.safe);
        return 0.5f * t;
    }
    float t = (mobilisation_index - corridor.gold) /
              (corridor.hard - corridor.gold);
    return 0.5f + 0.5f * t;
}

// Main kernel: biofilm PFAS kinetics under Phoenix canal conditions.[file:80]
extern "C" void biofilm_pfaskinetics_run(const BiofilmGuildDescriptor* guild,
                                         const CanalEnvironment* env,
                                         const BiofilmPfasCorridor* corridor,
                                         BiofilmPfasOutput* out)
{
    if (!guild || !env || !corridor || !out) {
        return;
    }

    float idx = compute_mobilisation_index(*guild, *env);
    float r   = normalize_rbiofilmpfas(idx, *corridor);

    out->mobilisation_index = idx;
    out->rbiofilmpfas       = r;
}
