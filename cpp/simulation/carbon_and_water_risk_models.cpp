// File: cpp/simulation/carbon_and_water_risk_models.cpp

#include <Eigen/Dense>
#include <sqlite3.h>

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace eco_restoration {

struct GarchModel {
    double mean{};
    double omega{};
    double alpha{};
    double beta{};
    double last_residual{};
    double last_variance{};
};

std::vector<double> load_carbon(const std::string& path) {
    std::ifstream input(path);
    if (!input) throw std::runtime_error("cannot open carbon CSV");
    std::string line;
    std::getline(input, line);
    std::vector<double> values;
    while (std::getline(input, line)) {
        const auto comma = line.find(',');
        values.push_back(std::stod(line.substr(comma + 1U)));
    }
    if (values.size() < 48U) throw std::invalid_argument("carbon history requires at least 48 records");
    return values;
}

double sigmoid(double value) {
    return value >= 0.0 ? 1.0 / (1.0 + std::exp(-value))
                        : std::exp(value) / (1.0 + std::exp(value));
}

GarchModel decode(const Eigen::Vector4d& parameter, double last_residual, double last_variance) {
    const double alpha = 0.999 * sigmoid(parameter[2]);
    const double beta = (0.999 - alpha) * sigmoid(parameter[3]);
    return {parameter[0], std::exp(parameter[1]), alpha, beta, last_residual, last_variance};
}

double garch_loss(const Eigen::Vector4d& parameter, const std::vector<double>& values) {
    const double mean = parameter[0];
    const double variance0 = std::max(1e-9, std::exp(parameter[1]) / 0.02);
    GarchModel model = decode(parameter, values.front() - mean, variance0);
    double variance = variance0;
    double loss = 0.0;

    for (const double value : values) {
        const double residual = value - model.mean;
        variance = std::max(1e-9, model.omega + model.alpha * model.last_residual * model.last_residual +
                                      model.beta * variance);
        loss += 0.5 * (std::log(variance) + residual * residual / variance);
        model.last_residual = residual;
    }
    return loss / static_cast<double>(values.size());
}

GarchModel fit_garch(const std::vector<double>& values) {
    const double mean = std::accumulate(values.begin(), values.end(), 0.0) / values.size();
    double variance = 0.0;
    for (double value : values) variance += (value - mean) * (value - mean);
    variance /= values.size();

    Eigen::Vector4d parameter(mean, std::log(std::max(1e-8, variance * 0.05)), -2.0, 2.0);
    double step = 0.05;

    for (int iteration = 0; iteration < 600; ++iteration) {
        Eigen::Vector4d gradient;
        for (int i = 0; i < 4; ++i) {
            Eigen::Vector4d plus = parameter;
            Eigen::Vector4d minus = parameter;
            const double epsilon = 1e-5 * std::max(1.0, std::abs(parameter[i]));
            plus[i] += epsilon;
            minus[i] -= epsilon;
            gradient[i] = (garch_loss(plus, values) - garch_loss(minus, values)) / (2.0 * epsilon);
        }

        const double prior_loss = garch_loss(parameter, values);
        Eigen::Vector4d proposal = parameter - step * gradient;
        if (garch_loss(proposal, values) < prior_loss) {
            parameter = proposal;
            step = std::min(0.15, step * 1.02);
        } else {
            step *= 0.5;
        }
        if (gradient.norm() < 1e-6 || step < 1e-8) break;
    }

    const double residual = values.back() - parameter[0];
    const GarchModel provisional = decode(parameter, residual, variance);
    const double final_variance = provisional.omega +
        provisional.alpha * residual * residual + provisional.beta * variance;
    return decode(parameter, residual, std::max(1e-9, final_variance));
}

void store_garch(sqlite3* database, const GarchModel& model) {
    sqlite3_exec(database,
        "CREATE TABLE IF NOT EXISTS carbon_garch_model("
        "model_id TEXT PRIMARY KEY,mean REAL,omega REAL,alpha REAL,beta REAL,"
        "last_residual REAL,last_variance REAL) STRICT;",
        nullptr, nullptr, nullptr);
    sqlite3_stmt* statement = nullptr;
    sqlite3_prepare_v2(database,
        "INSERT INTO carbon_garch_model VALUES('default',?,?,?,?,?,?) "
        "ON CONFLICT(model_id) DO UPDATE SET mean=excluded.mean,omega=excluded.omega,"
        "alpha=excluded.alpha,beta=excluded.beta,last_residual=excluded.last_residual,"
        "last_variance=excluded.last_variance;",
        -1, &statement, nullptr);
    sqlite3_bind_double(statement, 1, model.mean);
    sqlite3_bind_double(statement, 2, model.omega);
    sqlite3_bind_double(statement, 3, model.alpha);
    sqlite3_bind_double(statement, 4, model.beta);
    sqlite3_bind_double(statement, 5, model.last_residual);
    sqlite3_bind_double(statement, 6, model.last_variance);
    if (sqlite3_step(statement) != SQLITE_DONE) throw std::runtime_error("cannot store GARCH model");
    sqlite3_finalize(statement);
}

struct WaterRecord {
    Eigen::Matrix<double, 5, 1> features;
    double outcome{};
    std::string site;
};

std::vector<WaterRecord> load_water(const std::string& path) {
    std::ifstream input(path);
    if (!input) throw std::runtime_error("cannot open water outcome CSV");
    std::string line;
    std::getline(input, line);
    std::vector<WaterRecord> records;

    while (std::getline(input, line)) {
        std::replace(line.begin(), line.end(), ',', ' ');
        std::istringstream row(line);
        double wqi, turbidity, oxygen, temperature, flow, outcome;
        std::string site;
        row >> wqi >> turbidity >> oxygen >> temperature >> flow >> outcome >> site;
        if (!row || outcome < 0.0 || outcome > 1.0 || site.empty()) {
            throw std::invalid_argument("invalid water record");
        }
        records.push_back({Eigen::Matrix<double, 5, 1>(1.0 - wqi, turbidity, oxygen, temperature, flow), outcome, site});
    }
    if (records.size() < 30U) throw std::invalid_argument("water outcome history requires 30 records");
    return records;
}

int site_fold(const std::string& site) {
    unsigned value = 0;
    for (unsigned char character : site) value = value * 131U + character;
    return static_cast<int>(value % 5U);
}

struct LogisticModel {
    Eigen::Matrix<double, 5, 1> mean;
    Eigen::Matrix<double, 5, 1> deviation;
    Eigen::Matrix<double, 6, 1> weights;
};

LogisticModel train_water(const std::vector<WaterRecord>& records, int excluded_fold, double lambda) {
    LogisticModel model{};
    int count = 0;

    for (const auto& record : records) {
        if (site_fold(record.site) != excluded_fold) {
            model.mean += record.features;
            ++count;
        }
    }
    model.mean /= std::max(1, count);

    for (const auto& record : records) {
        if (site_fold(record.site) != excluded_fold) {
            model.deviation += (record.features - model.mean).cwiseAbs2();
        }
    }
    model.deviation = (model.deviation / std::max(1, count)).cwiseSqrt().cwiseMax(1e-8);

    for (int iteration = 0; iteration < 100; ++iteration) {
        Eigen::Matrix<double, 6, 6> hessian = lambda * Eigen::Matrix<double, 6, 6>::Identity();
        Eigen::Matrix<double, 6, 1> gradient = lambda * model.weights;
        gradient[0] = 0.0;

        for (const auto& record : records) {
            if (site_fold(record.site) == excluded_fold) continue;
            Eigen::Matrix<double, 6, 1> x;
            x[0] = 1.0;
            x.tail<5>() = (record.features - model.mean).cwiseQuotient(model.deviation);
            const double probability = sigmoid(model.weights.dot(x));
            gradient += (probability - record.outcome) * x;
            hessian += probability * (1.0 - probability) * x * x.transpose();
        }
        const Eigen::Matrix<double, 6, 1> update = hessian.ldlt().solve(gradient);
        model.weights -= update;
        if (update.norm() < 1e-8) break;
    }
    return model;
}

double calibration_error(const LogisticModel& model, const std::vector<WaterRecord>& records, int fold) {
    double total = 0.0;
    int count = 0;
    for (const auto& record : records) {
        if (site_fold(record.site) != fold) continue;
        Eigen::Matrix<double, 6, 1> x;
        x[0] = 1.0;
        x.tail<5>() = (record.features - model.mean).cwiseQuotient(model.deviation);
        total += std::pow(sigmoid(model.weights.dot(x)) - record.outcome, 2.0);
        ++count;
    }
    return count == 0 ? 0.0 : total / count;
}

void export_lua(const LogisticModel& model, const std::string& path) {
    std::ofstream output(path);
    if (!output) throw std::runtime_error("cannot write Lua model");
    output << "local model={mean={";
    for (int i = 0; i < 5; ++i) output << model.mean[i] << (i == 4 ? "},deviation={" : ",");
    for (int i = 0; i < 5; ++i) output << model.deviation[i] << (i == 4 ? "},weights={" : ",");
    for (int i = 0; i < 6; ++i) output << model.weights[i] << (i == 5 ? "}}\n" : ",");
    output << "function model.predict(wqi,turbidity,oxygen,temperature,flow) "
              "local v={1-wqi,turbidity,oxygen,temperature,flow};local z=model.weights[1];"
              "for i=1,5 do z=z+model.weights[i+1]*(v[i]-model.mean[i])/model.deviation[i] end "
              "return 1/(1+math.exp(-z)) end return model\n";
}

}  // namespace eco_restoration

int main(int argc, char** argv) {
    using namespace eco_restoration;
    if (argc != 5) {
        std::cerr << "usage: carbon_and_water_risk_models carbon.csv water.csv models.sqlite water_risk.lua\n";
        return 2;
    }

    sqlite3* database = nullptr;
    if (sqlite3_open(argv[3], &database) != SQLITE_OK) return 1;

    try {
        const GarchModel garch = fit_garch(load_carbon(argv[1]));
        store_garch(database, garch);

        const auto records = load_water(argv[2]);
        double cross_validation_brier = 0.0;
        for (int fold = 0; fold < 5; ++fold) {
            cross_validation_brier += calibration_error(train_water(records, fold, 0.1), records, fold);
        }

        const LogisticModel water = train_water(records, -1, 0.1);
        export_lua(water, argv[4]);

        std::cout << std::fixed << std::setprecision(6)
                  << "{\"garch\":{\"mean\":" << garch.mean
                  << ",\"omega\":" << garch.omega
                  << ",\"alpha\":" << garch.alpha
                  << ",\"beta\":" << garch.beta
                  << ",\"next_hour_lower\":" << garch.mean - 1.96 * std::sqrt(garch.last_variance)
                  << ",\"next_hour_upper\":" << garch.mean + 1.96 * std::sqrt(garch.last_variance)
                  << "},\"water_cross_validation_brier\":" << cross_validation_brier / 5.0 << "}\n";
        sqlite3_close(database);
    } catch (const std::exception& error) {
        sqlite3_close(database);
        std::cerr << "{\"error\":\"" << error.what() << "\"}\n";
        return 1;
    }
}
