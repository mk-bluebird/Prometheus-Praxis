// File: cpp/simulation/carbon_and_water_risk_models.cpp

#include <Eigen/Dense>
#include <sqlite3.h>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <numeric>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace eco_restoration {
constexpr double eps = 1e-9;

struct Garch { double mean, omega, alpha, beta, residual, variance; };
struct Water { Eigen::VectorXd x; double y; std::string site; };
struct Logistic { Eigen::VectorXd mean, scale, coefficient; };

void require(bool ok, const std::string& message) {
    if (!ok) throw std::runtime_error(message);
}

void readable(const std::string& path) {
    require(std::filesystem::is_regular_file(path), "input file is unavailable: " + path);
}

void writable_parent(const std::string& path) {
    std::filesystem::path parent = std::filesystem::path(path).parent_path();
    if (parent.empty()) parent = ".";
    require(std::filesystem::is_directory(parent), "output directory is unavailable: " + parent.string());
}

std::vector<std::string> fields(const std::string& line) {
    std::vector<std::string> result;
    std::stringstream input(line);
    std::string field;
    while (std::getline(input, field, ',')) result.push_back(field);
    return result;
}

double logistic(double value) {
    return value >= 0.0 ? 1.0 / (1.0 + std::exp(-value))
                        : std::exp(value) / (1.0 + std::exp(value));
}

int site_bucket(const std::string& site) {
    unsigned value = 0;
    for (unsigned char c : site) value = value * 131U + c;
    return static_cast<int>(value % 5U);
}

std::vector<double> carbon_history(const std::string& path) {
    readable(path);
    std::ifstream input(path);
    std::string line;
    std::getline(input, line);
    std::vector<double> values;

    while (std::getline(input, line)) {
        const auto row = fields(line);
        require(row.size() == 2U, "carbon CSV requires timestamp,intensity");
        const double value = std::stod(row[1]);
        require(std::isfinite(value) && value >= 0.0, "invalid carbon intensity");
        values.push_back(value);
    }
    require(values.size() >= 48U, "carbon history requires at least 48 observations");
    return values;
}

Garch unpack(const Eigen::Vector4d& p, double residual, double variance) {
    const double alpha = 0.995 * logistic(p[2]);
    const double beta = (0.995 - alpha) * logistic(p[3]);
    return {p[0], std::exp(p[1]), alpha, beta, residual, variance};
}

double garch_loss(const Eigen::Vector4d& p, const std::vector<double>& values) {
    Garch m = unpack(p, values.front() - p[0], 1.0);
    double v = std::max(eps, m.omega / std::max(eps, 1.0 - m.alpha - m.beta));
    double loss = 0.0;

    for (double value : values) {
        const double residual = value - m.mean;
        v = std::max(eps, m.omega + m.alpha * m.residual * m.residual + m.beta * v);
        loss += 0.5 * (std::log(v) + residual * residual / v);
        m.residual = residual;
    }
    return loss / static_cast<double>(values.size());
}

Garch fit_garch(const std::vector<double>& values) {
    const double mean = std::accumulate(values.begin(), values.end(), 0.0) / values.size();
    double variance = 0.0;
    for (double value : values) variance += (value - mean) * (value - mean);
    variance /= values.size();

    Eigen::Vector4d p(mean, std::log(std::max(eps, variance * 0.03)), -2.0, 1.5);
    double step = 0.03;

    for (int iteration = 0; iteration < 800; ++iteration) {
        Eigen::Vector4d gradient;
        for (int i = 0; i < 4; ++i) {
            const double d = 1e-5 * std::max(1.0, std::abs(p[i]));
            Eigen::Vector4d high = p, low = p;
            high[i] += d;
            low[i] -= d;
            gradient[i] = (garch_loss(high, values) - garch_loss(low, values)) / (2.0 * d);
        }
        const Eigen::Vector4d proposal = p - step * gradient;
        if (garch_loss(proposal, values) <= garch_loss(p, values)) {
            p = proposal;
            step = std::min(0.10, step * 1.01);
        } else {
            step *= 0.5;
        }
        if (gradient.norm() < 1e-6 || step < 1e-8) break;
    }

    Garch m = unpack(p, values.front() - p[0], 1.0);
    m.variance = std::max(eps, m.omega / std::max(eps, 1.0 - m.alpha - m.beta));
    for (double value : values) {
        m.residual = value - m.mean;
        m.variance = std::max(eps, m.omega + m.alpha * m.residual * m.residual + m.beta * m.variance);
    }
    return m;
}

std::vector<Water> water_history(const std::string& path) {
    readable(path);
    std::ifstream input(path);
    std::string line;
    std::getline(input, line);
    std::vector<Water> records;

    while (std::getline(input, line)) {
        const auto row = fields(line);
        require(row.size() == 7U, "water CSV requires wqi,turbidity,oxygen,temperature,flow,outcome,site");
        const double wqi = std::stod(row[0]);
        const double turbidity = std::stod(row[1]);
        const double oxygen = std::stod(row[2]);
        const double temperature = std::stod(row[3]);
        const double flow = std::stod(row[4]);
        const double outcome = std::stod(row[5]);

        require(std::isfinite(wqi) && std::isfinite(turbidity) && std::isfinite(oxygen) &&
                    std::isfinite(temperature) && std::isfinite(flow) &&
                    outcome >= 0.0 && outcome <= 1.0 && !row[6].empty(),
                "invalid water observation");

        Eigen::VectorXd x(5);
        x << 1.0 - std::clamp(wqi, 0.0, 1.0), turbidity, oxygen, temperature, flow;
        records.push_back({x, outcome, row[6]});
    }
    require(records.size() >= 30U, "water history requires at least 30 observations");
    return records;
}

Logistic train_water(const std::vector<Water>& records, int omitted_bucket, double l2) {
    Logistic m{Eigen::VectorXd::Zero(5), Eigen::VectorXd::Zero(5), Eigen::VectorXd::Zero(6)};
    int count = 0;

    for (const Water& record : records) {
        if (site_bucket(record.site) != omitted_bucket) {
            m.mean += record.x;
            ++count;
        }
    }
    require(count >= 7, "insufficient water training observations");
    m.mean /= count;

    for (const Water& record : records) {
        if (site_bucket(record.site) != omitted_bucket) {
            m.scale += (record.x - m.mean).cwiseAbs2();
        }
    }
    m.scale = (m.scale / count).cwiseSqrt().cwiseMax(1e-8);

    for (int iteration = 0; iteration < 100; ++iteration) {
        Eigen::MatrixXd h = l2 * Eigen::MatrixXd::Identity(6, 6);
        Eigen::VectorXd g = l2 * m.coefficient;
        g[0] = 0.0;

        for (const Water& record : records) {
            if (site_bucket(record.site) == omitted_bucket) continue;
            Eigen::VectorXd x(6);
            x[0] = 1.0;
            x.tail(5) = (record.x - m.mean).cwiseQuotient(m.scale);
            const double p = logistic(m.coefficient.dot(x));
            g += (p - record.y) * x;
            h += p * (1.0 - p) * x * x.transpose();
        }

        const Eigen::VectorXd delta = h.ldlt().solve(g);
        m.coefficient -= delta;
        if (delta.norm() < 1e-8) break;
    }
    return m;
}

double brier(const Logistic& m, const std::vector<Water>& records, int bucket) {
    double score = 0.0;
    int count = 0;
    for (const Water& record : records) {
        if (site_bucket(record.site) != bucket) continue;
        Eigen::VectorXd x(6);
        x[0] = 1.0;
        x.tail(5) = (record.x - m.mean).cwiseQuotient(m.scale);
        score += std::pow(logistic(m.coefficient.dot(x)) - record.y, 2.0);
        ++count;
    }
    return count == 0 ? 0.0 : score / count;
}

void check_sql(int result, sqlite3* db, const std::string& operation) {
    if (result != SQLITE_OK && result != SQLITE_DONE) {
        throw std::runtime_error(operation + ": " + sqlite3_errmsg(db));
    }
}

void persist(sqlite3* db, const Garch& g, const Logistic& w) {
    check_sql(sqlite3_exec(db,
        "CREATE TABLE IF NOT EXISTS carbon_garch_model("
        "model_id TEXT PRIMARY KEY,mean REAL,omega REAL,alpha REAL,beta REAL,residual REAL,variance REAL) STRICT;"
        "CREATE TABLE IF NOT EXISTS carbon_garch_forecast("
        "forecast_hour INTEGER PRIMARY KEY,mean REAL,lower_95 REAL,upper_95 REAL,variance REAL) STRICT;"
        "CREATE TABLE IF NOT EXISTS water_risk_model("
        "feature_index INTEGER PRIMARY KEY,mean REAL,deviation REAL,coefficient REAL) STRICT;"
        "DELETE FROM carbon_garch_forecast;DELETE FROM water_risk_model;",
        nullptr, nullptr, nullptr), db, "SQLite schema update");

    sqlite3_stmt* model = nullptr;
    check_sql(sqlite3_prepare_v2(db,
        "INSERT INTO carbon_garch_model VALUES('default',?,?,?,?,?,?) "
        "ON CONFLICT(model_id) DO UPDATE SET mean=excluded.mean,omega=excluded.omega,"
        "alpha=excluded.alpha,beta=excluded.beta,residual=excluded.residual,variance=excluded.variance;",
        -1, &model, nullptr), db, "GARCH statement preparation");

    for (int i = 0; i < 6; ++i) sqlite3_bind_double(model, i + 1, (&g.mean)[i]);
    check_sql(sqlite3_step(model), db, "GARCH persistence");
    sqlite3_finalize(model);

    sqlite3_stmt* forecast = nullptr;
    check_sql(sqlite3_prepare_v2(db, "INSERT INTO carbon_garch_forecast VALUES(?,?,?,?,?);",
                                 -1, &forecast, nullptr), db, "forecast statement preparation");

    double variance = g.variance;
    for (int hour = 1; hour <= 24; ++hour) {
        variance = std::max(eps, hour == 1
            ? g.omega + g.alpha * g.residual * g.residual + g.beta * variance
            : g.omega + (g.alpha + g.beta) * variance);
        const double interval = 1.96 * std::sqrt(variance);
        sqlite3_bind_int(forecast, 1, hour);
        sqlite3_bind_double(forecast, 2, g.mean);
        sqlite3_bind_double(forecast, 3, std::max(0.0, g.mean - interval));
        sqlite3_bind_double(forecast, 4, g.mean + interval);
        sqlite3_bind_double(forecast, 5, variance);
        check_sql(sqlite3_step(forecast), db, "forecast persistence");
        sqlite3_reset(forecast);
        sqlite3_clear_bindings(forecast);
    }
    sqlite3_finalize(forecast);

    sqlite3_stmt* water = nullptr;
    check_sql(sqlite3_prepare_v2(db, "INSERT INTO water_risk_model VALUES(?,?,?,?);",
                                 -1, &water, nullptr), db, "water statement preparation");

    for (int i = 0; i < 6; ++i) {
        sqlite3_bind_int(water, 1, i);
        sqlite3_bind_double(water, 2, i == 0 ? 0.0 : w.mean[i - 1]);
        sqlite3_bind_double(water, 3, i == 0 ? 1.0 : w.scale[i - 1]);
        sqlite3_bind_double(water, 4, w.coefficient[i]);
        check_sql(sqlite3_step(water), db, "water-model persistence");
        sqlite3_reset(water);
        sqlite3_clear_bindings(water);
    }
    sqlite3_finalize(water);
}

void export_lua(const Logistic& w, const std::string& path) {
    writable_parent(path);
    std::ofstream output(path, std::ios::trunc);
    require(static_cast<bool>(output), "cannot create Lua module");

    output << std::setprecision(17) << "local m={mean={";
    for (int i = 0; i < 5; ++i) output << w.mean[i] << (i == 4 ? "},scale={" : ",");
    for (int i = 0; i < 5; ++i) output << w.scale[i] << (i == 4 ? "},coefficient={" : ",");
    for (int i = 0; i < 6; ++i) output << w.coefficient[i] << (i == 5 ? "}}\n" : ",");
    output << "function m.predict(wqi,turbidity,oxygen,temperature,flow)"
              "local x={1-wqi,turbidity,oxygen,temperature,flow};local z=m.coefficient[1];"
              "for i=1,5 do z=z+m.coefficient[i+1]*(x[i]-m.mean[i])/m.scale[i] end "
              "return 1/(1+math.exp(-z)) end return m\n";
}

}  // namespace eco_restoration

int main(int argc, char** argv) {
    using namespace eco_restoration;
    if (argc != 5) {
        std::cerr << "usage: carbon_and_water_risk_models carbon.csv water.csv models.sqlite water_risk.lua\n";
        return 2;
    }

    writable_parent(argv[3]);
    sqlite3* db = nullptr;
    if (sqlite3_open_v2(argv[3], &db, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, nullptr) != SQLITE_OK) {
        std::cerr << "{\"error\":\"cannot open model database\"}\n";
        return 1;
    }

    try {
        const Garch garch = fit_garch(carbon_history(argv[1]));
        const std::vector<Water> records = water_history(argv[2]);
        double validation_brier = 0.0;
        for (int bucket = 0; bucket < 5; ++bucket) {
            validation_brier += brier(train_water(records, bucket, 0.1), records, bucket);
        }

        const Logistic water = train_water(records, -1, 0.1);
        persist(db, garch, water);
        export_lua(water, argv[4]);
        sqlite3_close(db);

        std::cout << std::fixed << std::setprecision(6)
                  << "{\"garch_alpha\":" << garch.alpha
                  << ",\"garch_beta\":" << garch.beta
                  << ",\"held_out_brier\":" << validation_brier / 5.0
                  << ",\"forecast_hours\":24,\"water_features\":5}\n";
    } catch (const std::exception& error) {
        sqlite3_close(db);
        std::cerr << "{\"error\":\"" << error.what() << "\"}\n";
        return 1;
    }
}
