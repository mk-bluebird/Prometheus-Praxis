// File: cpp/eco_restoration/water_quality_cusum_detector.cpp
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <sqlite3.h>
#include <memory>
#include <stdexcept>
#include <string>

namespace eco_restoration {

struct WaterQualityTelemetry {
    std::uint64_t hex_anchor{};
    std::int64_t observed_unix_s{};
    double water_quality_index{};
    double turbidity_ntu{};
    double dissolved_oxygen_mg_l{};
    double temperature_c{};
    double confidence{};
};

struct WaterQualityEvent {
    bool degradation_detected{};
    double degradation_score{};
    double cusum{};
    double knowledge_factor{};
    double eco_impact_value{};
    std::string review_status;
};

class WaterQualityCusumDetector {
public:
    WaterQualityCusumDetector(double baseline_mean, double baseline_standard_deviation,
                              double reference_shift, double decision_interval)
        : mean_(baseline_mean), standard_deviation_(baseline_standard_deviation),
          reference_shift_(reference_shift), decision_interval_(decision_interval) {
        if (baseline_standard_deviation <= 0.0 || reference_shift < 0.0 ||
            decision_interval <= 0.0) throw std::invalid_argument("invalid CUSUM calibration");
    }

    WaterQualityEvent update(const WaterQualityTelemetry& telemetry) {
        if (telemetry.water_quality_index < 0.0 || telemetry.water_quality_index > 1.0 ||
            telemetry.turbidity_ntu < 0.0 || telemetry.dissolved_oxygen_mg_l < 0.0 ||
            telemetry.confidence < 0.0 || telemetry.confidence > 1.0)
            throw std::invalid_argument("invalid water-quality telemetry");

        const double score = std::clamp(
            0.50 * (1.0 - telemetry.water_quality_index) +
            0.30 * (telemetry.turbidity_ntu / (telemetry.turbidity_ntu + 10.0)) +
            0.20 * (1.0 / (1.0 + telemetry.dissolved_oxygen_mg_l)), 0.0, 1.0);
        const double standardized = (score - mean_) / standard_deviation_;
        cusum_ = std::max(0.0, cusum_ + standardized - reference_shift_);
        const bool detected = cusum_ >= decision_interval_;
        const double knowledge = std::clamp(telemetry.confidence *
            (1.0 - std::min(1.0, standard_deviation_)), 0.0, 1.0);
        return {detected, score, cusum_, knowledge, knowledge * (1.0 - score),
                detected ? "OPERATOR_REVIEW" : "MONITORING"};
    }

private:
    double mean_{};
    double standard_deviation_{};
    double reference_shift_{};
    double decision_interval_{};
    double cusum_{};
};

void persist_water_quality_event(sqlite3* database, const WaterQualityTelemetry& telemetry,
                                 const WaterQualityEvent& event) {
    if (!database) throw std::invalid_argument("database is required");
    sqlite3_exec(database,
        "CREATE TABLE IF NOT EXISTS water_quality_event("
        "hex_anchor INTEGER NOT NULL,observed_unix_s INTEGER NOT NULL,"
        "degradation_score REAL NOT NULL CHECK(degradation_score BETWEEN 0 AND 1),"
        "cusum REAL NOT NULL,detected INTEGER NOT NULL CHECK(detected IN(0,1)),"
        "knowledge_factor REAL NOT NULL CHECK(knowledge_factor BETWEEN 0 AND 1),"
        "eco_impact_value REAL NOT NULL CHECK(eco_impact_value BETWEEN 0 AND 1),"
        "review_status TEXT NOT NULL,PRIMARY KEY(hex_anchor,observed_unix_s)) STRICT;",
        nullptr, nullptr, nullptr);

    sqlite3_stmt* raw = nullptr;
    sqlite3_prepare_v2(database,
        "INSERT INTO water_quality_event VALUES(?,?,?,?,?,?,?,?) "
        "ON CONFLICT(hex_anchor,observed_unix_s) DO UPDATE SET "
        "degradation_score=excluded.degradation_score,cusum=excluded.cusum,"
        "detected=excluded.detected,knowledge_factor=excluded.knowledge_factor,"
        "eco_impact_value=excluded.eco_impact_value,review_status=excluded.review_status;",
        -1, &raw, nullptr);
    std::unique_ptr<sqlite3_stmt, decltype(&sqlite3_finalize)> statement(raw, sqlite3_finalize);
    sqlite3_bind_int64(statement.get(), 1, static_cast<sqlite3_int64>(telemetry.hex_anchor));
    sqlite3_bind_int64(statement.get(), 2, telemetry.observed_unix_s);
    sqlite3_bind_double(statement.get(), 3, event.degradation_score);
    sqlite3_bind_double(statement.get(), 4, event.cusum);
    sqlite3_bind_int(statement.get(), 5, event.degradation_detected ? 1 : 0);
    sqlite3_bind_double(statement.get(), 6, event.knowledge_factor);
    sqlite3_bind_double(statement.get(), 7, event.eco_impact_value);
    sqlite3_bind_text(statement.get(), 8, event.review_status.c_str(), -1, SQLITE_TRANSIENT);
    if (sqlite3_step(statement.get()) != SQLITE_DONE)
        throw std::runtime_error("water-quality event upsert failed");
}

}  // namespace eco_restoration
