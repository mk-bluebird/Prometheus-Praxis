// File: cpp/eco_restoration/ppx_decision_persistence_and_simd_risk.cpp
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <sqlite3.h>
#include <stdexcept>
#include <string>
#include <vector>

#if defined(__x86_64__) || defined(_M_X64)
#include <immintrin.h>
#endif

namespace ppx::eco_restoration {

struct LaneDecision {
    std::string decision_id;
    std::string workload_id;
    std::string observed_utc;
    double k{};
    double e{};
    double r{};
    std::string action;
    std::string reason;
};

class DecisionLog {
public:
    explicit DecisionLog(const char* sqlite_path) {
        if (sqlite3_open_v2(sqlite_path, &database_, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, nullptr) != SQLITE_OK) {
            throw std::runtime_error("cannot open decision database");
        }
        sqlite3_busy_timeout(database_, 100);
        execute(
            "CREATE TABLE IF NOT EXISTS ppx_lane_decision_log("
            "decision_id TEXT PRIMARY KEY,workload_id TEXT NOT NULL,observed_utc TEXT NOT NULL,"
            "k REAL NOT NULL CHECK(k BETWEEN 0 AND 1),e REAL NOT NULL CHECK(e BETWEEN 0 AND 1),"
            "r REAL NOT NULL CHECK(r BETWEEN 0 AND 1),action TEXT NOT NULL,reason TEXT NOT NULL) STRICT;"
        );
    }

    ~DecisionLog() { sqlite3_close(database_); }

    [[nodiscard]] LaneDecision persist_with_conservative_retry(LaneDecision decision,
                                                                 double risk_increment = 0.05) {
        if (insert(decision)) return decision;

        decision.r = std::clamp(decision.r + risk_increment, 0.0, 1.0);
        decision.action = decision.r > 0.25 ? "HALT" : "DERATE";
        decision.reason = "persistence_retry_with_conservative_risk";
        if (!insert(decision)) {
            throw std::runtime_error("decision persistence unavailable after conservative retry");
        }
        return decision;
    }

private:
    sqlite3* database_{};

    void execute(const char* sql) {
        if (sqlite3_exec(database_, sql, nullptr, nullptr, nullptr) != SQLITE_OK) {
            throw std::runtime_error(sqlite3_errmsg(database_));
        }
    }

    bool insert(const LaneDecision& decision) {
        sqlite3_stmt* statement = nullptr;
        const char* sql =
            "INSERT OR IGNORE INTO ppx_lane_decision_log VALUES(?,?,?,?,?,?,?,?,?);";
        if (sqlite3_prepare_v2(database_, sql, -1, &statement, nullptr) != SQLITE_OK) return false;

        sqlite3_bind_text(statement, 1, decision.decision_id.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(statement, 2, decision.workload_id.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(statement, 3, decision.observed_utc.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_double(statement, 4, decision.k);
        sqlite3_bind_double(statement, 5, decision.e);
        sqlite3_bind_double(statement, 6, decision.r);
        sqlite3_bind_text(statement, 7, decision.action.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(statement, 8, decision.reason.c_str(), -1, SQLITE_TRANSIENT);

        const int result = sqlite3_step(statement);
        sqlite3_finalize(statement);
        return result == SQLITE_DONE && sqlite3_changes(database_) == 1;
    }
};

double weighted_clamped_risk_sum(
    const double* raw_coordinates,
    const double* weights,
    std::size_t count) {
    double total = 0.0;
    std::size_t i = 0;

#if defined(__AVX__)
    const __m256d zero = _mm256_setzero_pd();
    const __m256d one = _mm256_set1_pd(1.0);
    alignas(32) std::array<double, 4> partial{};
    for (; i + 4 <= count; i += 4) {
        const __m256d raw = _mm256_loadu_pd(raw_coordinates + i);
        const __m256d clamped = _mm256_min_pd(one, _mm256_max_pd(zero, raw));
        const __m256d weighted = _mm256_mul_pd(clamped, _mm256_loadu_pd(weights + i));
        _mm256_store_pd(partial.data(), weighted);
        total += partial[0] + partial[1] + partial[2] + partial[3];
    }
#elif defined(__SSE2__)
    const __m128d zero = _mm_setzero_pd();
    const __m128d one = _mm_set1_pd(1.0);
    alignas(16) std::array<double, 2> partial{};
    for (; i + 2 <= count; i += 2) {
        const __m128d raw = _mm_loadu_pd(raw_coordinates + i);
        const __m128d clamped = _mm_min_pd(one, _mm_max_pd(zero, raw));
        const __m128d weighted = _mm_mul_pd(clamped, _mm_loadu_pd(weights + i));
        _mm_store_pd(partial.data(), weighted);
        total += partial[0] + partial[1];
    }
#endif

    for (; i < count; ++i) {
        total += std::clamp(raw_coordinates[i], 0.0, 1.0) * weights[i];
    }
    return total;
}

void benchmark_weighted_risk_sum() {
    constexpr std::size_t coordinate_count = 256;
    constexpr std::size_t iterations = 2'000'000;
    std::vector<double> coordinates(coordinate_count);
    std::vector<double> weights(coordinate_count, 1.0 / static_cast<double>(coordinate_count));
    for (std::size_t i = 0; i < coordinate_count; ++i) {
        coordinates[i] = static_cast<double>(i % 13) / 10.0 - 0.1;
    }

    volatile double sink = 0.0;
    const auto start = std::chrono::steady_clock::now();
    for (std::size_t i = 0; i < iterations; ++i) {
        sink += weighted_clamped_risk_sum(coordinates.data(), weights.data(), coordinate_count);
    }
    const double seconds = std::chrono::duration<double>(
        std::chrono::steady_clock::now() - start).count();
    std::cout << "risk_frames_per_second=" << static_cast<double>(iterations) / seconds
              << "\tchecksum=" << sink << '\n';
}

}  // namespace ppx::eco_restoration
