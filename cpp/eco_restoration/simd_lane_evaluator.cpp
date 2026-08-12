// File: cpp/eco_restoration/simd_lane_evaluator.cpp

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <immintrin.h>
#include <iostream>
#include <random>
#include <stdexcept>
#include <vector>

namespace eco_restoration {

enum class LaneAction : std::uint8_t { Proceed = 0, Derate = 1, Halt = 2 };

struct LaneThresholds {
    double minimum_knowledge{0.60};
    double minimum_impact{0.55};
    double maximum_risk{0.35};
};

struct LaneBatch {
    std::vector<double> confidence;
    std::vector<double> sensor_reliability;
    std::vector<double> restoration_value;
    std::vector<double> energy_risk;
    std::vector<double> heat_risk;
    std::vector<double> water_risk;
};

double clamp01(double value) {
    return std::clamp(value, 0.0, 1.0);
}

LaneAction scalar_action(
    double confidence,
    double reliability,
    double value,
    double energy,
    double heat,
    double water,
    const LaneThresholds& thresholds) {

    const double knowledge = std::sqrt(clamp01(confidence) * clamp01(reliability));
    const double risk = std::max({clamp01(energy), clamp01(heat), clamp01(water)});
    const double impact = knowledge * clamp01(value) * (1.0 - risk);

    if (knowledge < thresholds.minimum_knowledge || impact < thresholds.minimum_impact ||
        risk > thresholds.maximum_risk) return LaneAction::Halt;
    if (risk > thresholds.maximum_risk * 0.75 || impact < thresholds.minimum_impact + 0.08) {
        return LaneAction::Derate;
    }
    return LaneAction::Proceed;
}

void evaluate_scalar(
    const LaneBatch& batch,
    const LaneThresholds& thresholds,
    std::vector<LaneAction>& output) {

    const std::size_t count = batch.confidence.size();
    output.resize(count);

    for (std::size_t i = 0; i < count; ++i) {
        output[i] = scalar_action(
            batch.confidence[i], batch.sensor_reliability[i], batch.restoration_value[i],
            batch.energy_risk[i], batch.heat_risk[i], batch.water_risk[i], thresholds);
    }
}

void evaluate_simd(
    const LaneBatch& batch,
    const LaneThresholds& thresholds,
    std::vector<LaneAction>& output) {

    const std::size_t count = batch.confidence.size();
    output.resize(count);
    std::size_t i = 0;

#if defined(__AVX512F__)
    const __m512d zero = _mm512_setzero_pd();
    const __m512d one = _mm512_set1_pd(1.0);
    const __m512d knowledge_limit = _mm512_set1_pd(thresholds.minimum_knowledge);
    const __m512d impact_limit = _mm512_set1_pd(thresholds.minimum_impact);
    const __m512d risk_limit = _mm512_set1_pd(thresholds.maximum_risk);
    const __m512d derate_risk_limit = _mm512_set1_pd(thresholds.maximum_risk * 0.75);
    const __m512d derate_impact_limit = _mm512_set1_pd(thresholds.minimum_impact + 0.08);

    for (; i + 8U <= count; i += 8U) {
        const auto bounded = [&](const std::vector<double>& values) {
            return _mm512_min_pd(one, _mm512_max_pd(zero, _mm512_loadu_pd(values.data() + i)));
        };

        const __m512d knowledge = _mm512_sqrt_pd(
            _mm512_mul_pd(bounded(batch.confidence), bounded(batch.sensor_reliability)));
        const __m512d risk = _mm512_max_pd(
            bounded(batch.energy_risk),
            _mm512_max_pd(bounded(batch.heat_risk), bounded(batch.water_risk)));
        const __m512d impact = _mm512_mul_pd(
            _mm512_mul_pd(knowledge, bounded(batch.restoration_value)),
            _mm512_sub_pd(one, risk));

        const __mmask8 halt = _mm512_cmp_pd_mask(knowledge, knowledge_limit, _CMP_LT_OQ) |
                              _mm512_cmp_pd_mask(impact, impact_limit, _CMP_LT_OQ) |
                              _mm512_cmp_pd_mask(risk, risk_limit, _CMP_GT_OQ);
        const __mmask8 derate = (_mm512_cmp_pd_mask(risk, derate_risk_limit, _CMP_GT_OQ) |
                                 _mm512_cmp_pd_mask(impact, derate_impact_limit, _CMP_LT_OQ)) & ~halt;

        for (int lane = 0; lane < 8; ++lane) {
            output[i + static_cast<std::size_t>(lane)] =
                (halt & (1U << lane)) ? LaneAction::Halt :
                (derate & (1U << lane)) ? LaneAction::Derate : LaneAction::Proceed;
        }
    }
#endif

    for (; i < count; ++i) {
        output[i] = scalar_action(
            batch.confidence[i], batch.sensor_reliability[i], batch.restoration_value[i],
            batch.energy_risk[i], batch.heat_risk[i], batch.water_risk[i], thresholds);
    }
}

}  // namespace eco_restoration

int main() {
    using namespace eco_restoration;

    constexpr std::size_t count = 1U << 16U;
    LaneBatch batch;
    for (auto* series : {
        &batch.confidence, &batch.sensor_reliability, &batch.restoration_value,
        &batch.energy_risk, &batch.heat_risk, &batch.water_risk
    }) {
        series->resize(count);
    }

    std::mt19937_64 generator(0x2452026ULL);
    std::uniform_real_distribution<double> distribution(0.0, 1.0);
    for (std::size_t i = 0; i < count; ++i) {
        batch.confidence[i] = distribution(generator);
        batch.sensor_reliability[i] = distribution(generator);
        batch.restoration_value[i] = distribution(generator);
        batch.energy_risk[i] = distribution(generator);
        batch.heat_risk[i] = distribution(generator);
        batch.water_risk[i] = distribution(generator);
    }

    const LaneThresholds thresholds;
    std::vector<LaneAction> scalar;
    std::vector<LaneAction> simd;

    const auto scalar_begin = std::chrono::steady_clock::now();
    for (int run = 0; run < 200; ++run) evaluate_scalar(batch, thresholds, scalar);
    const auto scalar_end = std::chrono::steady_clock::now();

    const auto simd_begin = std::chrono::steady_clock::now();
    for (int run = 0; run < 200; ++run) evaluate_simd(batch, thresholds, simd);
    const auto simd_end = std::chrono::steady_clock::now();

    if (scalar != simd) throw std::runtime_error("SIMD output differs from scalar output");

    const double scalar_seconds = std::chrono::duration<double>(scalar_end - scalar_begin).count();
    const double simd_seconds = std::chrono::duration<double>(simd_end - simd_begin).count();

    std::cout << "{\"frames\":" << count
              << ",\"scalar_seconds\":" << scalar_seconds
              << ",\"simd_seconds\":" << simd_seconds
              << ",\"speedup\":" << scalar_seconds / std::max(simd_seconds, 1e-12)
              << "}\n";
}
