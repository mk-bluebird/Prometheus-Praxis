// filename: src/materials/cpp/substrate_structural_disintegration.cpp
// destination: Prometheus-Praxis/src/materials/cpp/substrate_structural_disintegration.cpp
// license: MIT OR Apache-2.0
// language: C++ (C17, non-actuating)

#include <cstddef>
#include <cstdint>
#include <cmath>

// Two-channel substrate state: mechanical integrity and fragmentation.[file:80]
struct StructuralState {
    float integrity;    // mechanical integrity channel I(t) in [0,1]
    float frag_index;   // fragmentation index F(t) in [0,1]
};

// Input parameters for structural disintegration kinetics.[file:80]
struct StructuralKineticsInput {
    // Base rates.[file:80]
    float k_int_base;   // base decay rate for integrity [1/day]
    float k_frag_base;  // base growth rate for fragmentation [1/day]

    // Coupling and thresholds.[file:80]
    float coupling;     // coupling factor from integrity loss to fragmentation
    float integrity_threshold; // integrity threshold for disintegration onset I_onset

    // Environment modifiers (e.g., flow regime, temperature).[file:80]
    float env_int_factor;   // environment multiplier for integrity channel
    float env_frag_factor;  // environment multiplier for fragmentation channel

    // Microfragment yield parameters.[file:80]
    float micro_yield_peak;   // peak microfragment yield at fragmentation events [mass/area]
    float micro_yield_decay;  // exponential decay rate of pulses [1/day]
};

// Output summary of structural disintegration.[file:80]
struct StructuralKineticsOutput {
    // Effective rates.[file:80]
    float k_int;      // effective integrity decay rate [1/day]
    float k_frag;     // effective fragmentation growth rate [1/day]

    // Event times.[file:80]
    float t_onset;   // time when integrity crosses threshold (disintegration onset) [days]
    float t_half;    // time when integrity reaches 0.5 (mechanical half-life) [days]

    // Microfragment pulse metrics.[file:80]
    float pulse_peak;    // peak microfragment pulse amplitude [mass/area]
    float pulse_integral;// integrated microfragment release over a window [mass/area * day]
};

// Compute effective rates with environment modifiers.[file:80]
static void compute_effective_rates(const StructuralKineticsInput& in,
                                    float& k_int,
                                    float& k_frag)
{
    k_int  = in.k_int_base  * in.env_int_factor;
    k_frag = in.k_frag_base * in.env_frag_factor;

    if (k_int <= 0.0f) {
        k_int = 1e-6f; // prevent non-physical zero/negative.[file:80]
    }
    if (k_frag < 0.0f) {
        k_frag = 0.0f;
    }
}

// Compute deterministic event times under simple exponential integrity decay:
// I(t) = exp(-k_int * t), with I(0) = 1.[file:80]
static void compute_event_times(float k_int,
                                float integrity_threshold,
                                float& t_onset,
                                float& t_half)
{
    const float ln2 = 0.693147180f;
    t_half = ln2 / k_int;

    float thr = integrity_threshold;
    if (thr <= 0.0f) {
        t_onset = 0.0f;
    } else if (thr >= 1.0f) {
        t_onset = 0.0f;
    } else {
        t_onset = -std::log(thr) / k_int;
    }
}

// Model fragmentation index F(t) as driven by integrity loss:
// F(t) = 1 - exp(-k_frag * coupling * (1 - I(t))).[file:80]
static float fragmentation_index_at(float t,
                                    float k_int,
                                    float k_frag,
                                    float coupling)
{
    float I = std::exp(-k_int * t);
    float drive = 1.0f - I;
    float arg = -k_frag * coupling * drive;
    if (arg < -50.0f) {
        arg = -50.0f;
    }
    return 1.0f - std::exp(arg);
}

// Compute microfragment pulse metrics:
// - A peak at disintegration onset.
// - Exponential tail with rate micro_yield_decay.[file:80]
static void compute_microfragment_pulse(const StructuralKineticsInput& in,
                                        float t_onset,
                                        float& pulse_peak,
                                        float& pulse_integral)
{
    // Peak is scaled by fragmentation index at onset.[file:80]
    float k_int, k_frag;
    compute_effective_rates(in, k_int, k_frag);
    float F_onset = fragmentation_index_at(t_onset, k_int, k_frag, in.coupling);

    pulse_peak = in.micro_yield_peak * F_onset;

    // Integrated pulse over [t_onset, infinity) for exponential tail J(t) = peak * exp(-k * (t - t_onset)).[file:80]
    float k = in.micro_yield_decay;
    if (k <= 0.0f) {
        // No decay => treat as bounded over a nominal window.[file:80]
        pulse_integral = pulse_peak * 1.0f;
    } else {
        pulse_integral = pulse_peak / k;
    }
}

// Main kernel: two-channel structural disintegration model.[file:80]
extern "C" void substrate_structural_disintegration_run(const StructuralKineticsInput* in,
                                                        StructuralKineticsOutput* out)
{
    if (!in || !out) {
        return;
    }

    float k_int = 0.0f;
    float k_frag = 0.0f;
    compute_effective_rates(*in, k_int, k_frag);

    float t_onset = 0.0f;
    float t_half  = 0.0f;
    compute_event_times(k_int, in.integrity_threshold, t_onset, t_half);

    float pulse_peak     = 0.0f;
    float pulse_integral = 0.0f;
    compute_microfragment_pulse(*in, t_onset, pulse_peak, pulse_integral);

    out->k_int         = k_int;
    out->k_frag        = k_frag;
    out->t_onset       = t_onset;
    out->t_half        = t_half;
    out->pulse_peak    = pulse_peak;
    out->pulse_integral= pulse_integral;
}
