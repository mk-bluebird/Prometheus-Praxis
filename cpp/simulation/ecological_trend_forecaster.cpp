// File: cpp/simulation/ecological_trend_forecaster.cpp
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <numeric>
#include <sqlite3.h>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace eco_restoration {

struct EcologicalObservation {
    std::int64_t observed_unix_s{};
    double value{};
};

struct EcologicalForecast {
    std::int64_t forecast_unix_s{};
    double predicted_value{};
    double uncertainty{};
};

class Arima110Forecaster {
public:
    void fit(const std::vector<EcologicalObservation>& observations) {
        if (observations.size() < 4) throw std::invalid_argument("at least four observations required");
        differences_.clear();
        for (std::size_t i = 1; i < observations.size(); ++i) {
            if (observations[i].observed_unix_s <= observations[i - 1].observed_unix_s ||
                observations[i].value < 0.0 || observations[i].value > 1.0)
                throw std::invalid_argument("invalid ecological time series");
            differences_.push_back(observations[i].value - observations[i - 1].value);
        }
        mean_difference_ = std::accumulate(differences_.begin(), differences_.end(), 0.0) /
                           differences_.size();
        double numerator = 0.0, denominator = 0.0;
        for (std::size_t i = 1; i < differences_.size(); ++i) {
            numerator += (differences_[i - 1] - mean_difference_) * (differences_[i] - mean_difference_);
            denominator += std::pow(differences_[i - 1] - mean_difference_, 2);
        }
        autoregression_ = denominator > 1e-12 ? std::clamp(numerator / denominator, -0.99, 0.99) : 0.0;
        residual_variance_ = 0.0;
        for (std::size_t i = 1; i < differences_.size(); ++i) {
            const double residual = differences_[i] - mean_difference_ -
                autoregression_ * (differences_[i - 1] - mean_difference_);
            residual_variance_ += residual * residual;
        }
        residual_variance_ /= std::max<std::size_t>(1, differences_.size() - 1);
        last_value_ = observations.back().value;
        last_difference_ = differences_.back();
        interval_s_ = observations.back().observed_unix_s - observations[observations.size() - 2].observed_unix_s;
    }

    std::vector<EcologicalForecast> forecast(std::size_t steps) const {
        std::vector<EcologicalForecast> output;
        double value = last_value_, difference = last_difference_;
        for (std::size_t step = 1; step <= steps; ++step) {
            difference = mean_difference_ + autoregression_ * (difference - mean_difference_);
            value = std::clamp(value + difference, 0.0, 1.0);
            output.push_back({static_cast<std::int64_t>(step) * interval_s_, value,
                              std::sqrt(residual_variance_ * step)});
        }
        return output;
    }

private:
    std::vector<double> differences_;
    double mean_difference_{}, autoregression_{}, residual_variance_{};
    double last_value_{}, last_difference_{};
    std::int64_t interval_s_{};
};

void persist_ecological_forecast(sqlite3* database, std::uint64_t hex_anchor,
                                 const std::string& metric, std::int64_t base_unix_s,
                                 const std::vector<EcologicalForecast>& forecasts) {
    if (!database || metric.empty()) throw std::invalid_argument("invalid forecast persistence input");
    sqlite3_exec(database,
        "CREATE TABLE IF NOT EXISTS hex_ecological_forecast("
        "hex_anchor INTEGER NOT NULL,metric TEXT NOT NULL,forecast_unix_s INTEGER NOT NULL,"
        "predicted_value REAL NOT NULL CHECK(predicted_value BETWEEN 0 AND 1),"
        "uncertainty REAL NOT NULL CHECK(uncertainty>=0),"
        "PRIMARY KEY(hex_anchor,metric,forecast_unix_s)) STRICT;", nullptr, nullptr, nullptr);

    sqlite3_stmt* raw = nullptr;
    sqlite3_prepare_v2(database,
        "INSERT INTO hex_ecological_forecast VALUES(?,?,?,?,?) "
        "ON CONFLICT(hex_anchor,metric,forecast_unix_s) DO UPDATE SET "
        "predicted_value=excluded.predicted_value,uncertainty=excluded.uncertainty;",
        -1, &raw, nullptr);
    std::unique_ptr<sqlite3_stmt, decltype(&sqlite3_finalize)> statement(raw, sqlite3_finalize);
    for (const auto& prediction : forecasts) {
        sqlite3_bind_int64(statement.get(), 1, static_cast<sqlite3_int64>(hex_anchor));
        sqlite3_bind_text(statement.get(), 2, metric.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int64(statement.get(), 3, base_unix_s + prediction.forecast_unix_s);
        sqlite3_bind_double(statement.get(), 4, prediction.predicted_value);
        sqlite3_bind_double(statement.get(), 5, prediction.uncertainty);
        if (sqlite3_step(statement.get()) != SQLITE_DONE) throw std::runtime_error("forecast upsert failed");
        sqlite3_reset(statement.get());
        sqlite3_clear_bindings(statement.get());
    }
}

}  // namespace eco_restoration
