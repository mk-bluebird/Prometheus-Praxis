// filename: src/materials/cpp/substrate_flowregime_kinetics.cpp
// destination: Prometheus-Praxis/src/materials/cpp/substrate_flowregime_kinetics.cpp
// license: MIT OR Apache-2.0
// language: C++ (C17, non-actuating)

#include <cstddef>
#include <cstdint>
#include <cmath>

// Flow regime classification based on Reynolds number.[file:80]
enum class FlowRegime {
    Laminar,
    Transitional,
    Turbulent
};

// Input parameters for biodegradable substrate kinetics in a Cyboquatic reach.[file:80]
struct SubstrateKineticsInput {
    // Hydraulics and transport.[file:80]
    float reynolds;     // Reynolds number Re
    float velocity;     // bulk velocity [m/s]
    float diameter;     // characteristic length (e.g. hydraulic diameter) [m]

    // Biodegradation and mechanical erosion parameters.[file:80]
    float k_chem_base;  // base first-order chemical biodegradation rate [1/day]
    float k_mech_base;  // base mechanical erosion rate [1/day]

    // Temperature and matrix correction factors.[file:80]
    float temp_factor;  // dimensionless Arrhenius-like temperature multiplier
    float matrix_factor;// dimensionless Phoenix-matrix water correction

    // Micro-residue yield coefficients.[file:80]
    float yield_laminar;      // microflux coefficient for laminar regime
    float yield_transitional; // microflux coefficient for transitional regime
    float yield_turbulent;    // microflux coefficient for turbulent regime
};

// Output kinetic quantities and normalized risk coordinates.[file:80]
struct SubstrateKineticsOutput {
    // Effective rates.[file:80]
    float k_chem;   // effective chemical biodegradation rate [1/day]
    float k_mech;   // effective mechanical erosion rate [1/day]

    // Time to 90% mass loss assuming first-order decay: t90 = ln(10) / k_chem.[file:80]
    float t90_days;

    // Micro-residue flux J_micro(t,Re) [mass/area/time] (modelled as regime-dependent).[file:80]
    float J_micro;

    // Normalized risk coordinates in [0,1] for materials plane.[file:80]
    float r_t90;
    float r_micro;
};

// Shared corridor bands for t90 and micro-residue, consistent with econet-material-cybo pattern.[file:80]
struct MaterialsCorridor {
    float t90_safe;
    float t90_gold;
    float t90_hard;

    float micro_safe;
    float micro_gold;
    float micro_hard;
};

// Classify flow regime using standard Re thresholds.[file:80]
static FlowRegime classify_flow_regime(float reynolds) {
    if (reynolds < 2300.0f) {
        return FlowRegime::Laminar;
    }
    if (reynolds < 4000.0f) {
        return FlowRegime::Transitional;
    }
    return FlowRegime::Turbulent;
}

// Compute effective chemical and mechanical rates given flow regime and correction factors.
/// k_chem = k_chem_base * temp_factor * matrix_factor * f_regime(Re)
/// k_mech = k_mech_base * f_regime_mech(Re).[file:80]
static void compute_effective_rates(const SubstrateKineticsInput& in,
                                    float& k_chem,
                                    float& k_mech)
{
    FlowRegime regime = classify_flow_regime(in.reynolds);

    // Regime multipliers: laminar baseline, transitional moderate, turbulent high.[file:80]
    float f_chem = 1.0f;
    float f_mech = 1.0f;

    switch (regime) {
    case FlowRegime::Laminar:
        f_chem = 1.0f;
        f_mech = 0.5f;
        break;
    case FlowRegime::Transitional:
        f_chem = 1.5f;
        f_mech = 1.0f;
        break;
    case FlowRegime::Turbulent:
        f_chem = 2.0f;
        f_mech = 2.0f;
        break;
    }

    k_chem = in.k_chem_base * in.temp_factor * in.matrix_factor * f_chem;
    k_mech = in.k_mech_base * f_mech;

    // Guard against non-physical negative or zero rates.[file:80]
    if (k_chem <= 0.0f) {
        k_chem = 1e-6f;
    }
    if (k_mech < 0.0f) {
        k_mech = 0.0f;
    }
}

// Compute t90 (time to 90% mass loss) under first-order kinetics: t90 = ln(10) / k_chem.[file:80]
static float compute_t90_days(float k_chem) {
    const float ln10 = 2.302585093f;
    return ln10 / k_chem;
}

// Compute micro-residue flux J_micro(t,Re) using a simple regime-dependent model:
// J_micro = yield_regime * k_mech * velocity, evaluated at a representative time t (here, t90).[file:80]
static float compute_J_micro(const SubstrateKineticsInput& in,
                             float k_mech,
                             float t90_days)
{
    FlowRegime regime = classify_flow_regime(in.reynolds);
    float yield = in.yield_laminar;

    switch (regime) {
    case FlowRegime::Laminar:
        yield = in.yield_laminar;
        break;
    case FlowRegime::Transitional:
        yield = in.yield_transitional;
        break;
    case FlowRegime::Turbulent:
        yield = in.yield_turbulent;
        break;
    }

    // Convert t90_days to seconds for scaling if needed; here we keep units simple.[file:80]
    (void)t90_days; // placeholder: model currently depends on Re and k_mech, not explicitly on t.

    float J = yield * k_mech * in.velocity;
    if (J < 0.0f) {
        J = 0.0f;
    }
    return J;
}

// Piecewise-linear normalization into [0,1] using safe/gold/hard bands.
// For t90: shorter decay (small t90) is safer => r_t90 increases with t90.[file:80]
static float normalize_t90(float t90_days, const MaterialsCorridor& c) {
    if (t90_days <= c.t90_safe) {
        return 0.0f; // fast decay, low risk.[file:80]
    }
    if (t90_days >= c.t90_hard) {
        return 1.0f; // too persistent, high risk.[file:80]
    }
    if (t90_days <= c.t90_gold) {
        float t = (t90_days - c.t90_safe) / (c.t90_gold - c.t90_safe);
        return 0.5f * t; // map safe→gold into [0,0.5].[file:80]
    }
    float t = (t90_days - c.t90_gold) / (c.t90_hard - c.t90_gold);
    return 0.5f + 0.5f * t; // map gold→hard into [0.5,1].[file:80]
}

// For micro-residue flux: higher J_micro is worse => r_micro increases with J_micro.[file:80]
static float normalize_micro(float J_micro, const MaterialsCorridor& c) {
    if (J_micro <= c.micro_safe) {
        return 0.0f;
    }
    if (J_micro >= c.micro_hard) {
        return 1.0f;
    }
    if (J_micro <= c.micro_gold) {
        float t = (J_micro - c.micro_safe) / (c.micro_gold - c.micro_safe);
        return 0.5f * t;
    }
    float t = (J_micro - c.micro_gold) / (c.micro_hard - c.micro_gold);
    return 0.5f + 0.5f * t;
}

// Main kernel: compute flow-regime-dependent kinetics and normalized materials-plane risks.
// This function is non-actuating and intended to feed r_t90 and r_micro into the shared Vt and KER.[file:80]
extern "C" void substrate_flowregime_kinetics_run(const SubstrateKineticsInput* in,
                                                  const MaterialsCorridor* corridor,
                                                  SubstrateKineticsOutput* out)
{
    if (!in || !corridor || !out) {
        return;
    }

    float k_chem = 0.0f;
    float k_mech = 0.0f;
    compute_effective_rates(*in, k_chem, k_mech);

    float t90_days = compute_t90_days(k_chem);
    float J_micro  = compute_J_micro(*in, k_mech, t90_days);

    float r_t90   = normalize_t90(t90_days, *corridor);
    float r_micro = normalize_micro(J_micro, *corridor);

    out->k_chem   = k_chem;
    out->k_mech   = k_mech;
    out->t90_days = t90_days;
    out->J_micro  = J_micro;
    out->r_t90    = r_t90;
    out->r_micro  = r_micro;
}
