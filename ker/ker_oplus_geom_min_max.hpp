#ifndef PROMETHEUS_PRAXIS_KER_OPLUS_GEOM_MIN_MAX_HPP
#define PROMETHEUS_PRAXIS_KER_OPLUS_GEOM_MIN_MAX_HPP

#include <cstddef>
#include <cstdint>

extern "C" {

/// Plain-old-data representation of a base KERParticle2026v1 row.
/// All floats are assumed normalized to [0,1].
struct ker_particle2026v1 {
    const char* particle_id;      // null-terminated, non-null
    const char* topic_id;         // null-terminated, may be null if unused
    const char* lane;             // "RESEARCH", "PILOT", "PROD"
    float       K;                // knowledge  in [0,1]
    float       E;                // eco-impact in [0,1]
    float       R;                // risk       in [0,1]
    const char* evidencehex;      // hex string, precomputed
    const char* signinghex;       // DID-bound signature, precomputed
};

/// POD representation of a KERComposition2026v1 row.
struct ker_composition2026v1 {
    const char* left_particle_id;
    const char* right_particle_id;
    const char* combined_id;
    float       K_combined;
    float       E_combined;
    float       R_combined;
    const char* members;          // canonical "idmin,idmax"
    const char* rule_id;          // "keroplusgeomminmaxv1"
    const char* evidencehex;      // hex string, precomputed by caller
    const char* signinghex;       // DID-bound signature, precomputed
};

/// Status codes for C ABI; 0 = OK, non-zero = failure.
enum ker_status_code : int32_t {
    KER_STATUS_OK = 0,
    KER_STATUS_NULL_ARG = 1,
    KER_STATUS_INVALID_RANGE = 2
};

/// Compute K, E, R composition; does NOT compute evidencehex or signinghex.
/// Caller must:
/// - preallocate out_comp,
/// - pass non-null pointers for left, right, out_comp,
/// - ensure K in [0,1], E in [0,1], R in [0,1].
int32_t ker_oplus_geom_min_max(
    const ker_particle2026v1* left,
    const ker_particle2026v1* right,
    ker_composition2026v1* out_comp
);

} // extern "C"

#endif
