// File: roh_kernel_mt6883.cpp
// Destination: Prometheus-Praxis/roh-kernel-mt6883/roh_kernel_mt6883.cpp
// License: MIT OR Apache-2.0

#include <cmath>
#include <cstdint>
#include <jni.h>

extern "C" {

// -------------------------------------------------------------------
// ROH kernel telemetry schema (MT6883_2026v1)
// -------------------------------------------------------------------
//
// Telemetry arrays represent synchronized samples over a window:
//   power_mw          : instantaneous power in milliwatts
//   temperature_c     : sensor temperature in degrees Celsius
//   error_rate        : normalized error rate (0.0 .. 1.0)
//   exposure_factor   : environmental exposure factor (0.0 .. 1.0)
//   n_samples         : number of samples in each array
//
// The risk-of-harm kernel computes a ceiling score in [0.0, 1.0] by
// combining normalized power, temperature, error, and exposure
// contributions. This is designed to be deterministic and
// side-effect free for Rust FFI and JNI use.

// Core computation: MT6883 RoH kernel (2026v1)
double roh_kernel_mt6883_compute(
    const double* power_mw,
    const double* temperature_c,
    const double* error_rate,
    const double* exposure_factor,
    std::size_t n_samples
) {
    if (power_mw == nullptr || temperature_c == nullptr ||
        error_rate == nullptr || exposure_factor == nullptr ||
        n_samples == 0) {
        return 0.0;
    }

    // Ceiling thresholds (tunable, but fixed for 2026v1)
    const double P_MAX_SAFE_MW   = 250.0;  // safe power ceiling
    const double T_MAX_SAFE_C    = 65.0;   // safe temperature ceiling
    const double E_MAX_SAFE      = 0.02;   // safe error-rate ceiling
    const double EXP_MAX_SAFE    = 0.30;   // safe exposure ceiling

    double max_risk = 0.0;

    for (std::size_t i = 0; i < n_samples; ++i) {
        const double p  = power_mw[i];
        const double t  = temperature_c[i];
        const double e  = error_rate[i];
        const double ex = exposure_factor[i];

        // Normalize contributions (0.0 safe → 1.0 extreme)
        double p_norm  = p  <= P_MAX_SAFE_MW ? p  / P_MAX_SAFE_MW  : 1.0 + (p  - P_MAX_SAFE_MW)  / (P_MAX_SAFE_MW  * 2.0);
        double t_norm  = t  <= T_MAX_SAFE_C  ? t  / T_MAX_SAFE_C   : 1.0 + (t  - T_MAX_SAFE_C)   / (T_MAX_SAFE_C   * 2.0);
        double e_norm  = e  <= E_MAX_SAFE    ? e  / E_MAX_SAFE     : 1.0 + (e  - E_MAX_SAFE)     / (E_MAX_SAFE     * 5.0);
        double ex_norm = ex <= EXP_MAX_SAFE  ? ex / EXP_MAX_SAFE   : 1.0 + (ex - EXP_MAX_SAFE)   / (EXP_MAX_SAFE   * 3.0);

        // Clamp to a reasonable upper bound before weighting
        if (p_norm  < 0.0) p_norm  = 0.0;
        if (p_norm  > 2.0) p_norm  = 2.0;
        if (t_norm  < 0.0) t_norm  = 0.0;
        if (t_norm  > 2.0) t_norm  = 2.0;
        if (e_norm  < 0.0) e_norm  = 0.0;
        if (e_norm  > 3.0) e_norm  = 3.0;
        if (ex_norm < 0.0) ex_norm = 0.0;
        if (ex_norm > 2.0) ex_norm = 2.0;

        // Weighted combination
        const double w_p  = 0.35;
        const double w_t  = 0.30;
        const double w_e  = 0.20;
        const double w_ex = 0.15;

        double combined = w_p * p_norm + w_t * t_norm + w_e * e_norm + w_ex * ex_norm;

        // Ceiling mapping: convert combined to [0.0, 1.0]
        double roh = combined / 4.0;
        if (roh < 0.0) roh = 0.0;
        if (roh > 1.0) roh = 1.0;

        if (roh > max_risk) {
            max_risk = roh;
        }
    }

    return max_risk;
}

// -------------------------------------------------------------------
// C FFI API for Rust
// -------------------------------------------------------------------
//
// Rust can declare:
//
//   extern "C" {
//       fn roh_kernel_mt6883_ceiling(
//           power_mw: *const f64,
//           temperature_c: *const f64,
//           error_rate: *const f64,
//           exposure_factor: *const f64,
//           n_samples: usize
//       ) -> f64;
//   }
//
// and call this function from a safe wrapper.

// C FFI entry point
double roh_kernel_mt6883_ceiling(
    const double* power_mw,
    const double* temperature_c,
    const double* error_rate,
    const double* exposure_factor,
    std::size_t n_samples
) {
    return roh_kernel_mt6883_compute(
        power_mw,
        temperature_c,
        error_rate,
        exposure_factor,
        n_samples
    );
}

// -------------------------------------------------------------------
// JNI wrapper for Kotlin / Java
// -------------------------------------------------------------------
//
// Kotlin usage (example):
//
//   external fun rohKernelCeiling(
//       powerMw: DoubleArray,
//       temperatureC: DoubleArray,
//       errorRate: DoubleArray,
//       exposureFactor: DoubleArray
//   ): Double
//
// The arrays must have the same length; JNI wrapper will use
// that length and compute the kernel ceiling score.
//
// JNI name must match the package/class signature used by Kotlin:
//
//   package org.prometheuspraxis.mt6883
//   class RoHKernelMT6883 {
//       init { System.loadLibrary("roh_kernel_mt6883") }
//       external fun rohKernelCeiling(
//           powerMw: DoubleArray,
//           temperatureC: DoubleArray,
//           errorRate: DoubleArray,
//           exposureFactor: DoubleArray
//       ): Double
//   }

JNIEXPORT jdouble JNICALL
Java_org_prometheuspraxis_mt6883_RoHKernelMT6883_rohKernelCeiling(
    JNIEnv* env,
    jobject /* thisObj */,
    jdoubleArray j_power_mw,
    jdoubleArray j_temperature_c,
    jdoubleArray j_error_rate,
    jdoubleArray j_exposure_factor
) {
    if (j_power_mw == nullptr || j_temperature_c == nullptr ||
        j_error_rate == nullptr || j_exposure_factor == nullptr) {
        return 0.0;
    }

    jsize n0 = env->GetArrayLength(j_power_mw);
    jsize n1 = env->GetArrayLength(j_temperature_c);
    jsize n2 = env->GetArrayLength(j_error_rate);
    jsize n3 = env->GetArrayLength(j_exposure_factor);

    // Ensure all arrays match in length
    if (n0 <= 0 || n1 != n0 || n2 != n0 || n3 != n0) {
        return 0.0;
    }

    jboolean isCopyP = JNI_FALSE;
    jboolean isCopyT = JNI_FALSE;
    jboolean isCopyE = JNI_FALSE;
    jboolean isCopyX = JNI_FALSE;

    jdouble* power_mw = env->GetDoubleArrayElements(j_power_mw, &isCopyP);
    jdouble* temperature_c = env->GetDoubleArrayElements(j_temperature_c, &isCopyT);
    jdouble* error_rate = env->GetDoubleArrayElements(j_error_rate, &isCopyE);
    jdouble* exposure_factor = env->GetDoubleArrayElements(j_exposure_factor, &isCopyX);

    if (power_mw == nullptr || temperature_c == nullptr ||
        error_rate == nullptr || exposure_factor == nullptr) {

        if (power_mw != nullptr) {
            env->ReleaseDoubleArrayElements(j_power_mw, power_mw, JNI_ABORT);
        }
        if (temperature_c != nullptr) {
            env->ReleaseDoubleArrayElements(j_temperature_c, temperature_c, JNI_ABORT);
        }
        if (error_rate != nullptr) {
            env->ReleaseDoubleArrayElements(j_error_rate, error_rate, JNI_ABORT);
        }
        if (exposure_factor != nullptr) {
            env->ReleaseDoubleArrayElements(j_exposure_factor, exposure_factor, JNI_ABORT);
        }

        return 0.0;
    }

    double roh = roh_kernel_mt6883_compute(
        reinterpret_cast<const double*>(power_mw),
        reinterpret_cast<const double*>(temperature_c),
        reinterpret_cast<const double*>(error_rate),
        reinterpret_cast<const double*>(exposure_factor),
        static_cast<std::size_t>(n0)
    );

    env->ReleaseDoubleArrayElements(j_power_mw, power_mw, 0);
    env->ReleaseDoubleArrayElements(j_temperature_c, temperature_c, 0);
    env->ReleaseDoubleArrayElements(j_error_rate, error_rate, 0);
    env->ReleaseDoubleArrayElements(j_exposure_factor, exposure_factor, 0);

    return static_cast<jdouble>(roh);
}

} // extern "C"
