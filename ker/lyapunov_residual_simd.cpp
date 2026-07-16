#include "lyapunov_residual_simd.hpp"

#include <cstddef>
#include <immintrin.h>

extern "C" double lyapunov_residual_simd(
    const float* weights,
    const float* risks,
    std::size_t len
) {
    if (!weights || !risks || len == 0) {
        return 0.0;
    }

#if defined(__AVX2__)
    __m256 acc = _mm256_setzero_ps();
    std::size_t i = 0;
    const std::size_t step = 8;

    for (; i + step <= len; i += step) {
        __m256 w = _mm256_loadu_ps(weights + i);
        __m256 r = _mm256_loadu_ps(risks + i);
        __m256 r_sq = _mm256_mul_ps(r, r);
        __m256 term = _mm256_mul_ps(w, r_sq);
        acc = _mm256_add_ps(acc, term);
    }

    alignas(32) float acc_buf[8];
    _mm256_store_ps(acc_buf, acc);
    double sum = 0.0;
    for (int k = 0; k < 8; ++k) {
        sum += static_cast<double>(acc_buf[k]);
    }

    for (; i < len; ++i) {
        float r = risks[i];
        float w = weights[i];
        sum += static_cast<double>(w * r * r);
    }

    return sum;
#elif defined(__SSE2__)
    __m128 acc = _mm_setzero_ps();
    std::size_t i = 0;
    const std::size_t step = 4;

    for (; i + step <= len; i += step) {
        __m128 w = _mm_loadu_ps(weights + i);
        __m128 r = _mm_loadu_ps(risks + i);
        __m128 r_sq = _mm_mul_ps(r, r);
        __m128 term = _mm_mul_ps(w, r_sq);
        acc = _mm_add_ps(acc, term);
    }

    alignas(16) float acc_buf[4];
    _mm_store_ps(acc_buf, acc);
    double sum = 0.0;
    for (int k = 0; k < 4; ++k) {
        sum += static_cast<double>(acc_buf[k]);
    }

    for (; i < len; ++i) {
        float r = risks[i];
        float w = weights[i];
        sum += static_cast<double>(w * r * r);
    }

    return sum;
#else
    double sum = 0.0;
    for (std::size_t i = 0; i < len; ++i) {
        float r = risks[i];
        float w = weights[i];
        sum += static_cast<double>(w * r * r);
    }
    return sum;
#endif
}
