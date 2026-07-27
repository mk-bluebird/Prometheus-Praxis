// filename: Prometheus-Praxis/src/cpp/waste/shredding/shredder_controller.cpp
// destination: github.com/mk-bluebird/Prometheus-Praxis/src/cpp/waste/shredding/shredder_controller.cpp
// license: MIT OR Apache-2.0
//
// Role:
// Non‑actuating helper surface for the shredding band.
// Responsibilities:
// - Accept raw sensor frames (motor current, vibration, temperature, throughput).
// - Normalize them into KER coordinates and blast‑radius inputs.
// - Call the Rust blastradiuskernel FFI to obtain KER‑weighted radii.
// - Never issue hardware commands or touch actuator APIs.
//
// This file is designed to align with the Prometheus-Praxis ecosafety grammar,
// Lyapunov/KER math, and nonactuating constraints described for multi‑band kernels
// (workload, drainage, AI‑node) and extended here for waste/shredding.
// It assumes a Rust crate `prometheuspraxisai` (or similar) exposes a C ABI
// for `blastradiuskernel` that consumes normalized risk coordinates and returns
// a scalar blast‑radius score.

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <array>
#include <string>
#include <optional>

// We deliberately avoid any hardware control headers or OS‑specific APIs.
// No sockets, no device handles, no PLC/embedded HAL includes.

// ---------------------------------------------------------------------------
// C ABI for Rust blastradiuskernel FFI
// ---------------------------------------------------------------------------
//
// The Rust side should expose a function with the following signature:
//
// #[no_mangle]
// pub extern "C" fn blastradiuskernel_ceiling(
//     motor_current_norm: f64,
//     vibration_norm: f64,
//     temperature_norm: f64,
//     throughput_norm: f64,
//     vt_before: f64,
//     vt_after: f64,
//     k_knowledge: f64,
//     e_ecoimpact: f64,
//     r_risk: f64
// ) -> f64
//
// This C declaration mirrors that ABI. The Rust implementation owns the
// Lyapunov residual math and KER scoring, keeping C strictly non‑actuating.

extern "C" double blastradiuskernel_ceiling(
    double motor_current_norm,
    double vibration_norm,
    double temperature_norm,
    double throughput_norm,
    double vt_before,
    double vt_after,
    double k_knowledge,
    double e_ecoimpact,
    double r_risk
);

// ---------------------------------------------------------------------------
// Shredding sensor frame and normalized KER coordinates
// ---------------------------------------------------------------------------

struct ShredderSensorFrame
{
    // Raw telemetry, one instantaneous frame or short window aggregates.
    double motor_current_a;   // motor phase current [A]
    double vibration_ms2;     // vibration magnitude [m/s^2]
    double temperature_c;     // housing / bearing temperature [°C]
    double throughput_kg_per_h; // material throughput [kg/h]

    // Optional previous residual slice (used to compute vt delta).
    double vt_before;         // Lyapunov residual before this frame (dimensionless)
};

struct ShredderKerCoordinates
{
    // Normalized risk coordinates in [0, 1], consistent with KER grammar.
    double r_current;      // motor current overload risk
    double r_vibration;    // mechanical instability risk
    double r_temperature;  // thermal risk
    double r_throughput;   // workload / congestion risk

    // KER triad in [0, 1].
    double k_knowledge;    // knowledge factor
    double e_ecoimpact;    // eco‑impact factor
    double r_risk;         // risk‑of‑harm factor

    // Residual slice.
    double vt_before;
    double vt_after;
    double delta_vt;

    // Derived blast radius (as returned by Rust kernel).
    double blast_radius;
};

// ---------------------------------------------------------------------------
// Normalization helpers
// ---------------------------------------------------------------------------
//
// These functions map raw physical metrics into [0, 1] corridors.
// Bounds are chosen to be physically reasonable for industrial shredders,
// and can be tightened later via ALN/PlaneWeights shards without changing
// the nonactuating nature of this file.

// Clamp helper.
static inline double clamp01(double x)
{
    if (x < 0.0) return 0.0;
    if (x > 1.0) return 1.0;
    return x;
}

// Motor current normalization.
// 0 A -> 0.0, I_safe_max A -> ~0.0‑0.2, beyond overload band -> towards 1.0.
static double normalize_motor_current(double motor_current_a)
{
    // Safe nominal current band up to 80 A, hard ceiling at 200 A.
    const double ISAFE_A   = 80.0;
    const double ICEILING_A = 200.0;

    if (motor_current_a <= 0.0) {
        return 0.0;
    }

    if (motor_current_a <= ISAFE_A) {
        // Within nominal band, very low risk.
        return 0.1 * (motor_current_a / ISAFE_A);
    }

    if (motor_current_a >= ICEILING_A) {
        return 1.0;
    }

    const double excess = motor_current_a - ISAFE_A;
    const double span   = ICEILING_A - ISAFE_A;
    double risk = 0.2 + 0.8 * (excess / span);
    return clamp01(risk);
}

// Vibration normalization.
// Safe baseline up to 5 m/s^2, ceiling at 50 m/s^2.
static double normalize_vibration(double vibration_ms2)
{
    const double VSAFE_MS2   = 5.0;
    const double VCEILING_MS2 = 50.0;

    if (vibration_ms2 <= 0.0) {
        return 0.0;
    }

    if (vibration_ms2 <= VSAFE_MS2) {
        return 0.1 * (vibration_ms2 / VSAFE_MS2);
    }

    if (vibration_ms2 >= VCEILING_MS2) {
        return 1.0;
    }

    const double excess = vibration_ms2 - VSAFE_MS2;
    const double span   = VCEILING_MS2 - VSAFE_MS2;
    double risk = 0.2 + 0.8 * (excess / span);
    return clamp01(risk);
}

// Temperature normalization.
// Safe up to 60 °C, ceiling at 120 °C.
static double normalize_temperature(double temperature_c)
{
    const double TSAFE_C   = 60.0;
    const double TCEILING_C = 120.0;

    if (temperature_c <= 0.0) {
        return 0.0;
    }

    if (temperature_c <= TSAFE_C) {
        return 0.1 * (temperature_c / TSAFE_C);
    }

    if (temperature_c >= TCEILING_C) {
        return 1.0;
    }

    const double excess = temperature_c - TSAFE_C;
    const double span   = TCEILING_C - TSAFE_C;
    double risk = 0.2 + 0.8 * (excess / span);
    return clamp01(risk);
}

// Throughput normalization.
// Safe nominal throughput up to TSAFE, hard ceiling at TCEILING.
static double normalize_throughput(double throughput_kg_per_h)
{
    const double TSAFE_KG_PER_H   = 1000.0;  // nominal band
    const double TCEILING_KG_PER_H = 5000.0; // severe overload

    if (throughput_kg_per_h <= 0.0) {
        return 0.0;
    }

    if (throughput_kg_per_h <= TSAFE_KG_PER_H) {
        return 0.1 * (throughput_kg_per_h / TSAFE_KG_PER_H);
    }

    if (throughput_kg_per_h >= TCEILING_KG_PER_H) {
        return 1.0;
    }

    const double excess = throughput_kg_per_h - TSAFE_KG_PER_H;
    const double span   = TCEILING_KG_PER_H - TSAFE_KG_PER_H;
    double risk = 0.2 + 0.8 * (excess / span);
    return clamp01(risk);
}

// ---------------------------------------------------------------------------
// Residual and KER triad computation (non‑actuating)
// ---------------------------------------------------------------------------
//
// Residual Vt is modeled as a weighted quadratic form over the shredding
// risk coordinates, mirroring the drainage/workload kernels:
//
// Vt = w_current * r_current^2
//    + w_vibration * r_vibration^2
//    + w_temperature * r_temperature^2
//    + w_throughput * r_throughput^2
//
// Weights reflect non‑offsettable planes (current, vibration) and are
// subject to ALN PlaneWeights shards in the Rust governance layer.

static double compute_residual_vt(
    double r_current,
    double r_vibration,
    double r_temperature,
    double r_throughput
)
{
    const double W_CURRENT    = 1.0;
    const double W_VIBRATION  = 0.9;
    const double W_TEMPERATURE = 0.7;
    const double W_THROUGHPUT = 0.6;

    const double vt =
        W_CURRENT    * r_current    * r_current +
        W_VIBRATION  * r_vibration  * r_vibration +
        W_TEMPERATURE * r_temperature * r_temperature +
        W_THROUGHPUT * r_throughput * r_throughput;

    return vt < 0.0 ? 0.0 : vt;
}

// Simple KER triad mapping for shredding band.
//
// K (knowledge) is high when residual is low and coordinates are moderate,
// E (ecoimpact) is penalized by overload in throughput and temperature,
// R (risk) grows with residual and high vibration/current.
static void compute_ker_triads(
    double r_current,
    double r_vibration,
    double r_temperature,
    double r_throughput,
    double vt_before,
    double vt_after,
    double &k_out,
    double &e_out,
    double &r_out
)
{
    const double vt_delta = vt_after - vt_before;

    // Baseline K starts near 1.0, reduced by residual and coordinate magnitudes.
    double k = 1.0
        - 0.4 * clamp01(vt_after)
        - 0.2 * clamp01(r_current)
        - 0.2 * clamp01(r_vibration)
        - 0.1 * clamp01(r_temperature)
        - 0.1 * clamp01(r_throughput);

    // Eco‑impact: penalize overload in throughput and temperature, plus residual growth.
    double e = 1.0
        - 0.3 * clamp01(r_throughput)
        - 0.3 * clamp01(r_temperature)
        - 0.2 * clamp01(vt_after)
        - 0.2 * (vt_delta > 0.0 ? clamp01(vt_delta) : 0.0);

    // Risk‑of‑harm: grows with residual, vt increase, and high current/vibration.
    double r = 0.0
        + 0.4 * clamp01(vt_after)
        + 0.3 * (vt_delta > 0.0 ? clamp01(vt_delta) : 0.0)
        + 0.2 * clamp01(r_current)
        + 0.1 * clamp01(r_vibration);

    k_out = clamp01(k);
    e_out = clamp01(e);
    r_out = clamp01(r);
}

// ---------------------------------------------------------------------------
// Core helper: transform raw frame -> KER/shredding blast radius
// ---------------------------------------------------------------------------
//
// This is the main non‑actuating entry point for callers that want to
// understand shredding blast radius without driving hardware.
//
// It performs:
// 1. Normalization of raw telemetry into risk coordinates.
// 2. Residual Vt computation and delta.
// 3. KER triad mapping.
// 4. FFI call into Rust blastradiuskernel_ceiling.
//
// It does not open devices, write to PLCs, or issue motion commands.

static ShredderKerCoordinates compute_shredder_blast_radius_internal(
    const ShredderSensorFrame &frame
)
{
    ShredderKerCoordinates out{};

    // 1. Normalize raw metrics.
    out.r_current    = normalize_motor_current(frame.motor_current_a);
    out.r_vibration  = normalize_vibration(frame.vibration_ms2);
    out.r_temperature = normalize_temperature(frame.temperature_c);
    out.r_throughput = normalize_throughput(frame.throughput_kg_per_h);

    // 2. Residual slice.
    out.vt_before = frame.vt_before;
    out.vt_after  = compute_residual_vt(
        out.r_current,
        out.r_vibration,
        out.r_temperature,
        out.r_throughput
    );
    out.delta_vt  = out.vt_after - out.vt_before;

    if (out.vt_before < 0.0) {
        out.vt_before = 0.0;
        out.delta_vt  = out.vt_after;
    }

    // 3. KER triads.
    compute_ker_triads(
        out.r_current,
        out.r_vibration,
        out.r_temperature,
        out.r_throughput,
        out.vt_before,
        out.vt_after,
        out.k_knowledge,
        out.e_ecoimpact,
        out.r_risk
    );

    // 4. Call Rust blastradiuskernel via FFI.
    const double br = blastradiuskernel_ceiling(
        out.r_current,
        out.r_vibration,
        out.r_temperature,
        out.r_throughput,
        out.vt_before,
        out.vt_after,
        out.k_knowledge,
        out.e_ecoimpact,
        out.r_risk
    );

    out.blast_radius = br < 0.0 ? 0.0 : br;

    return out;
}

// Public non‑actuating API.
//
// This function can be used by Rust FFI, Python/Lua tooling, or diagnostic
// controllers. It only computes KER and blast radius; it does not perform
// any I/O beyond pure CPU work.

ShredderKerCoordinates shredder_compute_ker_blastradius(
    double motor_current_a,
    double vibration_ms2,
    double temperature_c,
    double throughput_kg_per_h,
    double vt_before
)
{
    ShredderSensorFrame frame{
        motor_current_a,
        vibration_ms2,
        temperature_c,
        throughput_kg_per_h,
        vt_before
    };

    return compute_shredder_blast_radius_internal(frame);
}

// Optional batch helper for windows of telemetry.
//
// Accepts arrays of frames and returns the worst‑case blast radius,
// plus the KER coordinates for that sample.

std::optional<ShredderKerCoordinates> shredder_compute_window_ker_blastradius(
    const double *motor_current_a,
    const double *vibration_ms2,
    const double *temperature_c,
    const double *throughput_kg_per_h,
    std::size_t   nsamples,
    double        vt_before
)
{
    if (!motor_current_a || !vibration_ms2 || !temperature_c || !throughput_kg_per_h) {
        return std::nullopt;
    }

    if (nsamples == 0) {
        return std::nullopt;
    }

    double vt_prev = vt_before;
    double max_radius = 0.0;
    ShredderKerCoordinates worst_sample{};

    for (std::size_t i = 0; i < nsamples; ++i) {
        ShredderSensorFrame frame{
            motor_current_a[i],
            vibration_ms2[i],
            temperature_c[i],
            throughput_kg_per_h[i],
            vt_prev
        };

        ShredderKerCoordinates sample = compute_shredder_blast_radius_internal(frame);

        if (sample.blast_radius > max_radius) {
            max_radius = sample.blast_radius;
            worst_sample = sample;
        }

        vt_prev = sample.vt_after;
    }

    return worst_sample;
}
