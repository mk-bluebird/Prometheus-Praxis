// File: cpp/eco_restoration/hex_arrival_rate_model.cpp
#include <algorithm>
#include <cmath>
#include <sstream>
#include <sqlite3.h>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace ppx::eco_restoration {

struct HexArrivalModel {
    std::vector<std::size_t> lags;
    std::vector<double> weights;
    double bias{};

    [[nodiscard]] double forecast(const std::vector<double>& counts) const {
        if (counts.empty() || lags.size() != weights.size()) {
            throw std::invalid_argument("invalid arrival-rate model");
        }
        double log_rate = bias;
        for (std::size_t i = 0; i < lags.size(); ++i) {
            if (counts.size() < lags[i]) throw std::invalid_argument("insufficient arrival history");
            log_rate += weights[i] * std::log1p(std::max(0.0, counts[counts.size() - lags[i]]));
        }
        return std::max(0.0, std::exp(log_rate) - 1.0);
    }
};

HexArrivalModel train_hex_arrival_model(
    const std::vector<double>& hourly_counts,
    std::vector<std::size_t> lags,
    double learning_rate,
    std::size_t epochs) {
    if (hourly_counts.empty() || lags.empty() || learning_rate <= 0.0) {
        throw std::invalid_argument("invalid arrival-rate training inputs");
    }
    const std::size_t maximum_lag = *std::max_element(lags.begin(), lags.end());
    if (hourly_counts.size() <= maximum_lag) {
        throw std::invalid_argument("arrival history is shorter than maximum lag");
    }

    HexArrivalModel model{std::move(lags), std::vector<double>(lags.size(), 0.0), 0.0};
    for (std::size_t epoch = 0; epoch < epochs; ++epoch) {
        for (std::size_t t = maximum_lag; t < hourly_counts.size(); ++t) {
            double prediction = model.bias;
            for (std::size_t j = 0; j < model.lags.size(); ++j) {
                prediction += model.weights[j] *
                    std::log1p(std::max(0.0, hourly_counts[t - model.lags[j]]));
            }
            const double error = prediction - std::log1p(std::max(0.0, hourly_counts[t]));
            model.bias -= learning_rate * error;
            for (std::size_t j = 0; j < model.lags.size(); ++j) {
                const double feature = std::log1p(std::max(0.0, hourly_counts[t - model.lags[j]]));
                model.weights[j] -= learning_rate * error * feature;
            }
        }
    }
    return model;
}

void persist_arrival_model(
    sqlite3* database, std::int64_t hex_anchor, const std::string& trained_utc,
    const HexArrivalModel& model) {
    sqlite3_exec(database,
        "CREATE TABLE IF NOT EXISTS ppx_hex_arrival_model("
        "hex_anchor INTEGER PRIMARY KEY,trained_utc TEXT NOT NULL,"
        "lags_csv TEXT NOT NULL,weights_csv TEXT NOT NULL,bias REAL NOT NULL) STRICT;",
        nullptr, nullptr, nullptr);

    std::ostringstream lags, weights;
    for (std::size_t i = 0; i < model.lags.size(); ++i) {
        if (i != 0) { lags << ','; weights << ','; }
        lags << model.lags[i];
        weights << model.weights[i];
    }

    sqlite3_stmt* statement = nullptr;
    const char* sql =
        "INSERT INTO ppx_hex_arrival_model VALUES(?,?,?,?,?) "
        "ON CONFLICT(hex_anchor) DO UPDATE SET trained_utc=excluded.trained_utc,"
        "lags_csv=excluded.lags_csv,weights_csv=excluded.weights_csv,bias=excluded.bias;";
    if (sqlite3_prepare_v2(database, sql, -1, &statement, nullptr) != SQLITE_OK) {
        throw std::runtime_error(sqlite3_errmsg(database));
    }
    sqlite3_bind_int64(statement, 1, hex_anchor);
    sqlite3_bind_text(statement, 2, trained_utc.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement, 3, lags.str().c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement, 4, weights.str().c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_double(statement, 5, model.bias);
    if (sqlite3_step(statement) != SQLITE_DONE) {
        sqlite3_finalize(statement);
        throw std::runtime_error(sqlite3_errmsg(database));
    }
    sqlite3_finalize(statement);
}

}  // namespace ppx::eco_restoration
