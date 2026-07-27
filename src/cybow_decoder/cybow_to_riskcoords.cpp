// filename: src/cybow_decoder/cybow_to_riskcoords.cpp
// target-repo: https://github.com/mk-bluebird/Prometheus-Praxis
// language: C++ (POD-style, no corridors, no actuation)
// license: MIT OR Apache-2.0

#include <cstddef>
#include <cstdint>

// Canonical RiskCoords vector as consumed by ecosafety-core:
// All coordinates are normalized to [0,1], but corridor semantics
// (safe/gold/hard bands, non-compensatable planes) are NOT defined here.[file:3]
struct RiskCoords {
    float r_energy;      // energy tailwind vs deficit
    float r_hydraulics;  // hydraulic stress / surcharge
    float r_bio;         // biology / biodiversity
    float r_materials;   // materials / built environment
    float r_carbon;      // carbon / climate
    float r_tox;         // toxicity / pollutants
    float r_micro;       // microresidues / microplastics
    float r_calib;       // calibration / data quality
    float r_sigma;       // model / sensor uncertainty
};

// Simple KER slice: just the triad and scalar score.
// ecosafety-core is responsible for full windowing and lane decisions.[file:3]
struct KerSlice {
    float k;       // Knowledge
    float e;       // Ecoimpact
    float r;       // Risk of harm
    float score;   // k e - r
};

// Minimal decoded payload view from CYBOW + manifest.
// In practice, `manifest_handle` comes from aln_manifest_cache and
// points to a schema description that maps raw bytes to physical fields.[file:3]
struct CybowDecodedPayload {
    const uint8_t* bytes;
    std::size_t    len;
    const void*    manifest_handle;  // opaque ALN v2 manifest descriptor
};

// Output bundle for Rust ecosafety-core consumption.
struct RiskEnvelope {
    RiskCoords risk;
    float      vt;      // Lyapunov residual V_t = sum_j w_j * r_j^2
    KerSlice   ker;     // single-step KER slice
};

// -----------------------------------------------------------------------------
// Helper extraction functions
// -----------------------------------------------------------------------------
//
// In a real implementation, these would use `manifest_handle` to interpret
// `bytes` according to the ALN schema for this frame type. Here we keep
// them as simple stubs that demonstrate the mapping pattern, without
// hard-coding any corridor thresholds or physical units.[file:3]

static float clamp01(float x) {
    if (x < 0.0f) return 0.0f;
    if (x > 1.0f) return 1.0f;
    return x;
}

// These functions should be replaced with schema-aware decoding:
// for example, reading floats/ints from `bytes` at positions
// defined by the manifest, and normalizing them to [0,1].[file:3]
static float extract_energy(const CybowDecodedPayload& p) {
    if (p.len == 0 || p.bytes == nullptr) return 0.0f;
    // Example placeholder normalization: number of Joules scaled.
    // Real implementation uses manifest-driven field offsets.[file:3]
    float raw = static_cast<float>(p.bytes[0]); // dummy
    return clamp01(raw / 255.0f);
}

static float extract_hydraulics(const CybowDecodedPayload& p) {
    if (p.len < 2 || p.bytes == nullptr) return 0.0f;
    float raw = static_cast<float>(p.bytes[1]);
    return clamp01(raw / 255.0f);
}

static float extract_bio(const CybowDecodedPayload& p) {
    if (p.len < 3 || p.bytes == nullptr) return 0.0f;
    float raw = static_cast<float>(p.bytes[2]);
    return clamp01(raw / 255.0f);
}

static float extract_materials(const CybowDecodedPayload& p) {
    if (p.len < 4 || p.bytes == nullptr) return 0.0f;
    float raw = static_cast<float>(p.bytes[3]);
    return clamp01(raw / 255.0f);
}

static float extract_carbon(const CybowDecodedPayload& p) {
    if (p.len < 5 || p.bytes == nullptr) return 0.0f;
    float raw = static_cast<float>(p.bytes[4]);
    return clamp01(raw / 255.0f);
}

static float extract_tox(const CybowDecodedPayload& p) {
    if (p.len < 6 || p.bytes == nullptr) return 0.0f;
    float raw = static_cast<float>(p.bytes[5]);
    return clamp01(raw / 255.0f);
}

static float extract_micro(const CybowDecodedPayload& p) {
    if (p.len < 7 || p.bytes == nullptr) return 0.0f;
    float raw = static_cast<float>(p.bytes[6]);
    return clamp01(raw / 255.0f);
}

static float extract_calib(const CybowDecodedPayload& p) {
    if (p.len < 8 || p.bytes == nullptr) return 0.0f;
    float raw = static_cast<float>(p.bytes[7]);
    return clamp01(raw / 255.0f);
}

static float extract_sigma(const CybowDecodedPayload& p) {
    if (p.len < 9 || p.bytes == nullptr) return 0.0f;
    float raw = static_cast<float>(p.bytes[8]);
    return clamp01(raw / 255.0f);
}

// -----------------------------------------------------------------------------
// V_t and KER computation (no corridors)
// -----------------------------------------------------------------------------
//
// V_t is the Lyapunov residual V_t = sum_j w_j * r_j^2, but the choice of
// weights w_j belongs to ecosafety-core. Here we only compute a simple
// unweighted residual as a convenience; Rust can recompute the canonical
// value with Tree-of-Life weights.[file:3]
static float compute_vt_unweighted(const RiskCoords& rc) {
    float sum = 0.0f;
    sum += rc.r_energy     * rc.r_energy;
    sum += rc.r_hydraulics * rc.r_hydraulics;
    sum += rc.r_bio        * rc.r_bio;
    sum += rc.r_materials  * rc.r_materials;
    sum += rc.r_carbon     * rc.r_carbon;
    sum += rc.r_tox        * rc.r_tox;
    sum += rc.r_micro      * rc.r_micro;
    sum += rc.r_calib      * rc.r_calib;
    sum += rc.r_sigma      * rc.r_sigma;
    return sum;
}

// KER slice: derive k, e, r and score from RiskCoords and V_t.
// This implementation follows your canonical definition k e - r,
// but does not enforce any gates or lanes.[file:3]
static KerSlice compute_ker_slice(const RiskCoords& rc, float vt) {
    KerSlice ks;

    // Risk of harm r as the maximum coordinate across planes.[file:3]
    float r = rc.r_energy;
    if (rc.r_hydraulics > r) r = rc.r_hydraulics;
    if (rc.r_bio        > r) r = rc.r_bio;
    if (rc.r_materials  > r) r = rc.r_materials;
    if (rc.r_carbon     > r) r = rc.r_carbon;
    if (rc.r_tox        > r) r = rc.r_tox;
    if (rc.r_micro      > r) r = rc.r_micro;
    if (rc.r_calib      > r) r = rc.r_calib;
    if (rc.r_sigma      > r) r = rc.r_sigma;

    // Ecoimpact e as 1 - r (no corridor semantics here).[file:3]
    float e = 1.0f - r;
    if (e < 0.0f) e = 0.0f;
    if (e > 1.0f) e = 1.0f;

    // Knowledge k here is a simple proxy: 1 - vt normalized.
    // ecosafety-core can replace this with window-based K.[file:3]
    float k = 1.0f - vt;
    if (k < 0.0f) k = 0.0f;
    if (k > 1.0f) k = 1.0f;

    ks.k     = k;
    ks.e     = e;
    ks.r     = r;
    ks.score = k + e - r;

    return ks;
}

// -----------------------------------------------------------------------------
// Public API
// -----------------------------------------------------------------------------
//
// Map a decoded CYBOW payload into RiskCoords, V_t, and a KER slice.
// This is a pure function: no corridors, no governance, no actuation.
// Rust ecosafety-core is expected to:
// - Recompute canonical V_t with Tree-of-Life weights.
// - Compute window-level KER (K, E, R) and kerscore.
// - Apply corridor and lane logic.[file:3]

bool cybow_payload_to_riskcoords(const CybowDecodedPayload& payload,
                                 RiskEnvelope& out)
{
    if (payload.bytes == nullptr || payload.len == 0) {
        // Treat empty payload as zero-risk, but ecosafety-core
        // should still enforce data-quality corridors.[file:3]
        RiskCoords rc{};
        rc.r_energy     = 0.0f;
        rc.r_hydraulics = 0.0f;
        rc.r_bio        = 0.0f;
        rc.r_materials  = 0.0f;
        rc.r_carbon     = 0.0f;
        rc.r_tox        = 0.0f;
        rc.r_micro      = 0.0f;
        rc.r_calib      = 1.0f; // high calibration risk for missing data.[file:3]
        rc.r_sigma      = 1.0f; // high uncertainty.

        out.risk = rc;
        out.vt   = compute_vt_unweighted(rc);
        out.ker  = compute_ker_slice(rc, out.vt);
        return false; // signal that payload was missing / degenerate.[file:3]
    }

    RiskCoords rc;
    rc.r_energy     = extract_energy(payload);
    rc.r_hydraulics = extract_hydraulics(payload);
    rc.r_bio        = extract_bio(payload);
    rc.r_materials  = extract_materials(payload);
    rc.r_carbon     = extract_carbon(payload);
    rc.r_tox        = extract_tox(payload);
    rc.r_micro      = extract_micro(payload);
    rc.r_calib      = extract_calib(payload);
    rc.r_sigma      = extract_sigma(payload);

    out.risk = rc;
    out.vt   = compute_vt_unweighted(rc);
    out.ker  = compute_ker_slice(rc, out.vt);

    return true;
}
