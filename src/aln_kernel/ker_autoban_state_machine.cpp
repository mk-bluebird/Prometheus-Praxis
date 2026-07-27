// filename: src/aln_kernel/ker_autoban_state_machine.cpp
// target-repo: https://github.com/mk-bluebird/Prometheus-Praxis
// language: C++ (non-actuating)
// license: MIT OR Apache-2.0

#include <cstdint>
#include <cstddef>

// Lane semantics: ACTIVE -> PROBATION -> BANNED, with BANNED absorbing.[file:11]
enum class KerLane : uint8_t {
    ACTIVE    = 0,
    PROBATION = 1,
    BANNED    = 2
};

// KER triad and autoban state for a single DID.[file:11]
struct KerAutobanState {
    // Bostrom / ALN DID is tracked externally and keyed into this state;
    // this struct holds only KER and lane information.[file:11]
    float    K;         // Knowledge factor in [0,1]
    float    E;         // Eco-impact factor in [0,1]
    float    R;         // Risk-of-harm factor in [0,1]
    KerLane  lane;      // ACTIVE, PROBATION, or BANNED
    uint32_t counter;   // consecutive high-risk events
};

// Static thresholds calibrated in ALN governance;
// this module only applies them.[file:11]
struct KerAutobanConfig {
    float   r_high;          // R threshold above which an event is “high risk”
    uint32_t probation_limit; // high-risk events before PROBATION
    uint32_t ban_limit;       // high-risk events before BANNED
};

// Clamp helper to keep K,E,R in [0,1].[file:11]
static float clamp01(float v) {
    if (v < 0.0f) return 0.0f;
    if (v > 1.0f) return 1.0f;
    return v;
}

// Initialize autoban state for a DID.[file:11]
static KerAutobanState ker_autoban_init(float K, float E, float R) {
    KerAutobanState st{};
    st.K       = clamp01(K);
    st.E       = clamp01(E);
    st.R       = clamp01(R);
    st.lane    = KerLane::ACTIVE;
    st.counter = 0U;
    return st;
}

// Apply ALN v2 autoban rule to a state given a new KER triad.
// This function enforces:
// - Absorbing BANNED state (no lane upgrades once banned).
// - Counter increments on high-risk events (R >= r_high).
// - Lane transitions ACTIVE→PROBATION→BANNED when limits are reached.[file:11]
static void ker_autoban_update(KerAutobanState& st,
                               float K_new,
                               float E_new,
                               float R_new,
                               const KerAutobanConfig& cfg)
{
    // Once banned, K/E/R may be updated for diagnostics, but lane
    // must remain BANNED (absorbing semantics).[file:11]
    if (st.lane == KerLane::BANNED) {
        st.K = clamp01(K_new);
        st.E = clamp01(E_new);
        st.R = clamp01(R_new);
        return;
    }

    // Update KER triad.[file:11]
    st.K = clamp01(K_new);
    st.E = clamp01(E_new);
    st.R = clamp01(R_new);

    // High-risk event detection.[file:11]
    bool high_risk = (st.R >= cfg.r_high);

    if (high_risk) {
        // Increment consecutive high-risk counter.[file:11]
        if (st.counter < UINT32_MAX) {
            st.counter += 1U;
        }
    } else {
        // Reset counter on safe event.[file:11]
        st.counter = 0U;
    }

    // Lane transitions under absorbing ban semantics.[file:11]
    switch (st.lane) {
    case KerLane::ACTIVE:
        if (st.counter >= cfg.probation_limit &&
            st.counter < cfg.ban_limit)
        {
            st.lane = KerLane::PROBATION;
        } else if (st.counter >= cfg.ban_limit) {
            st.lane = KerLane::BANNED;
        }
        break;
    case KerLane::PROBATION:
        if (st.counter >= cfg.ban_limit) {
            st.lane = KerLane::BANNED;
        }
        // Note: no automatic upgrade back to ACTIVE; ALN governance
        // must explicitly reset state if allowed.[file:11]
        break;
    case KerLane::BANNED:
        // Already handled above; this branch is unreachable.[file:11]
        break;
    }
}

// -----------------------------------------------------------------------------
// C ABI for Rust / ALN integration
// -----------------------------------------------------------------------------

extern "C" {

// Initialize a DID’s autoban state.[file:11]
void ker_autoban_init_state(float K,
                            float E,
                            float R,
                            KerAutobanState* out_state)
{
    if (!out_state) return;
    *out_state = ker_autoban_init(K, E, R);
}

// Update autoban state with new KER values and configuration.[file:11]
void ker_autoban_apply(KerAutobanState* state,
                       float K_new,
                       float E_new,
                       float R_new,
                       const KerAutobanConfig* cfg)
{
    if (!state || !cfg) return;
    ker_autoban_update(*state, K_new, E_new, R_new, *cfg);
}

}
