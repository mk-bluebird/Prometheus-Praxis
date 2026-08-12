// File: cpp/simulation/uncertainty_and_goes_heat_pipeline.cpp

#include <Eigen/Dense>
#include <gdal_priv.h>
#include <sqlite3.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

namespace eco_restoration {

enum class LaneAction { Proceed, Derate, Halt };

struct RiskState {
    double energy_kwh{};
    double carbon_g{};
    double heat_risk{};
    double water_risk{};
    double combined_risk{};
    LaneAction action{};
};

RiskState evaluate_workload(const Eigen::Vector4d& input) {
    const double seconds = std::max(0.0, input[0]);
    const double power_w = std::max(0.0, input[1]);
    const double temperature_c = input[2];
    const double water_quality = std::clamp(input[3], 0.0, 1.0);
    const double energy_kwh = seconds * power_w / 3'600'000.0;
    const double carbon_g = energy_kwh * 420.0 * 0.40;
    const double energy_risk = std::clamp(carbon_g / 300.0, 0.0, 1.0);
    const double heat_risk = std::clamp((temperature_c - 30.0) / 15.0, 0.0, 1.0);
    const double water_risk = std::clamp(1.0 - water_quality, 0.0, 1.0);
    const double risk = std::max({energy_risk, heat_risk, water_risk});
    const LaneAction action = risk > 0.70 ? LaneAction::Halt :
                              risk > 0.35 ? LaneAction::Derate : LaneAction::Proceed;
    return {energy_kwh, carbon_g, heat_risk, water_risk, risk, action};
}

struct UncertaintyResult {
    RiskState unscented_mean;
    Eigen::Matrix<double, 5, 1> standard_deviation;
    std::array<double, 3> monte_carlo_action_probability{};
};

UncertaintyResult propagate_uncertainty(
    const Eigen::Vector4d& mean,
    const Eigen::Matrix4d& covariance,
    std::size_t monte_carlo_samples = 100000U) {

    constexpr int dimensions = 4;
    constexpr double alpha = 0.25;
    constexpr double beta = 2.0;
    constexpr double kappa = 0.0;
    const double lambda = alpha * alpha * (dimensions + kappa) - dimensions;
    const double scale = dimensions + lambda;

    Eigen::LLT<Eigen::Matrix4d> decomposition(scale * covariance);
    if (decomposition.info() != Eigen::Success) {
        throw std::invalid_argument("sensor covariance must be positive definite");
    }

    std::vector<Eigen::Vector4d> sigma_points{mean};
    for (int i = 0; i < dimensions; ++i) {
        sigma_points.push_back(mean + decomposition.matrixL().col(i));
        sigma_points.push_back(mean - decomposition.matrixL().col(i));
    }

    const double wm0 = lambda / scale;
    const double wc0 = wm0 + (1.0 - alpha * alpha + beta);
    const double wi = 1.0 / (2.0 * scale);

    Eigen::Matrix<double, 5, 1> output_mean = Eigen::Matrix<double, 5, 1>::Zero();
    std::vector<Eigen::Matrix<double, 5, 1>> outputs;

    for (std::size_t i = 0; i < sigma_points.size(); ++i) {
        const RiskState state = evaluate_workload(sigma_points[i]);
        Eigen::Matrix<double, 5, 1> output(
            state.energy_kwh, state.carbon_g, state.heat_risk,
            state.water_risk, state.combined_risk);
        outputs.push_back(output);
        output_mean += (i == 0U ? wm0 : wi) * output;
    }

    Eigen::Matrix<double, 5, 5> output_covariance = Eigen::Matrix<double, 5, 5>::Zero();
    for (std::size_t i = 0; i < outputs.size(); ++i) {
        const Eigen::Matrix<double, 5, 1> residual = outputs[i] - output_mean;
        output_covariance += (i == 0U ? wc0 : wi) * residual * residual.transpose();
    }

    std::mt19937_64 generator(0x1652026ULL);
    std::normal_distribution<double> normal(0.0, 1.0);
    std::array<std::size_t, 3> counts{};
    const Eigen::Matrix4d square_root = covariance.llt().matrixL();

    for (std::size_t i = 0; i < monte_carlo_samples; ++i) {
        Eigen::Vector4d standard;
        for (int j = 0; j < dimensions; ++j) standard[j] = normal(generator);
        const RiskState state = evaluate_workload(mean + square_root * standard);
        ++counts[static_cast<std::size_t>(state.action)];
    }

    const RiskState mean_state = evaluate_workload(mean);
    return {
        {output_mean[0], output_mean[1], output_mean[2], output_mean[3], output_mean[4], mean_state.action},
        output_covariance.diagonal().cwiseMax(0.0).cwiseSqrt(),
        {
            static_cast<double>(counts[0]) / monte_carlo_samples,
            static_cast<double>(counts[1]) / monte_carlo_samples,
            static_cast<double>(counts[2]) / monte_carlo_samples
        }
    };
}

struct HexCell {
    std::uint64_t anchor{};
    double x{};
    double y{};
};

struct HeatObservation {
    HexCell cell;
    double lst_c{};
    bool observed{};
};

double gaussian_kriging(
    const HexCell& target,
    const std::vector<HeatObservation>& observations,
    double range_m = 6000.0,
    double nugget = 0.05) {

    std::vector<const HeatObservation*> valid;
    for (const auto& observation : observations) {
        if (observation.observed) valid.push_back(&observation);
    }
    if (valid.empty()) throw std::runtime_error("no valid heat observations");

    const Eigen::Index count = static_cast<Eigen::Index>(valid.size());
    Eigen::MatrixXd covariance(count, count);
    Eigen::VectorXd target_covariance(count);
    const auto kernel = [range_m](double distance) { return std::exp(-distance / range_m); };

    for (Eigen::Index i = 0; i < count; ++i) {
        target_covariance[i] = kernel(std::hypot(
            valid[i]->cell.x - target.x, valid[i]->cell.y - target.y));
        for (Eigen::Index j = 0; j < count; ++j) {
            covariance(i, j) = kernel(std::hypot(
                valid[i]->cell.x - valid[j]->cell.x, valid[i]->cell.y - valid[j]->cell.y));
        }
        covariance(i, i) += nugget;
    }

    const Eigen::VectorXd weights = covariance.ldlt().solve(target_covariance);
    double value = 0.0;
    for (Eigen::Index i = 0; i < count; ++i) value += weights[i] * valid[i]->lst_c;
    return value;
}

std::vector<HeatObservation> aggregate_goes_hour(
    const std::string& netcdf_path,
    const std::vector<HexCell>& cells,
    double baseline_c,
    double range_c) {

    GDALAllRegister();
    GDALDataset* dataset = static_cast<GDALDataset*>(GDALOpen(netcdf_path.c_str(), GA_ReadOnly));
    if (dataset == nullptr || dataset->GetRasterCount() < 1) {
        if (dataset != nullptr) GDALClose(dataset);
        throw std::runtime_error("cannot open GOES LST dataset");
    }

    double transform[6]{};
    if (dataset->GetGeoTransform(transform) != CE_None || range_c <= 0.0) {
        GDALClose(dataset);
        throw std::invalid_argument("dataset requires projected geotransform and positive range");
    }

    double inverse[6]{};
    if (!GDALInvGeoTransform(transform, inverse)) {
        GDALClose(dataset);
        throw std::runtime_error("cannot invert geotransform");
    }

    GDALRasterBand* band = dataset->GetRasterBand(1);
    int has_nodata = 0;
    const double nodata = band->GetNoDataValue(&has_nodata);
    std::vector<HeatObservation> result;

    for (const HexCell& cell : cells) {
        double pixel_x = 0.0;
        double pixel_y = 0.0;
        GDALApplyGeoTransform(inverse, cell.x, cell.y, &pixel_x, &pixel_y);
        const int x = static_cast<int>(std::floor(pixel_x));
        const int y = static_cast<int>(std::floor(pixel_y));
        double kelvin = 0.0;

        const bool inside = x >= 0 && y >= 0 && x < dataset->GetRasterXSize() && y < dataset->GetRasterYSize();
        const bool read_ok = inside &&
            band->RasterIO(GF_Read, x, y, 1, 1, &kelvin, 1, 1, GDT_Float64, 0, 0) == CE_None;
        const bool valid = read_ok && std::isfinite(kelvin) && (!has_nodata || kelvin != nodata);
        result.push_back({cell, valid ? kelvin - 273.15 : 0.0, valid});
    }
    GDALClose(dataset);

    for (auto& observation : result) {
        if (!observation.observed) {
            observation.lst_c = gaussian_kriging(observation.cell, result);
        }
    }
    return result;
}

void persist_hourly_heat(
    sqlite3* database,
    std::int64_t observed_unix_s,
    const std::vector<HeatObservation>& observations,
    double baseline_c,
    double range_c) {

    sqlite3_exec(database,
        "CREATE TABLE IF NOT EXISTS hex_hourly_heat("
        "hex_anchor INTEGER NOT NULL,observed_unix_s INTEGER NOT NULL,lst_c REAL NOT NULL,"
        "r_heat REAL NOT NULL CHECK(r_heat BETWEEN 0 AND 1),interpolated INTEGER NOT NULL,"
        "PRIMARY KEY(hex_anchor,observed_unix_s)) STRICT;",
        nullptr, nullptr, nullptr);

    sqlite3_stmt* statement = nullptr;
    sqlite3_prepare_v2(database,
        "INSERT INTO hex_hourly_heat VALUES(?,?,?,?,?) ON CONFLICT(hex_anchor,observed_unix_s) "
        "DO UPDATE SET lst_c=MAX(lst_c,excluded.lst_c),r_heat=MAX(r_heat,excluded.r_heat),"
        "interpolated=MIN(interpolated,excluded.interpolated);",
        -1, &statement, nullptr);

    for (const auto& observation : observations) {
        const double risk = std::clamp((observation.lst_c - baseline_c) / range_c, 0.0, 1.0);
        sqlite3_bind_int64(statement, 1, static_cast<sqlite3_int64>(observation.cell.anchor));
        sqlite3_bind_int64(statement, 2, static_cast<sqlite3_int64>(observed_unix_s));
        sqlite3_bind_double(statement, 3, observation.lst_c);
        sqlite3_bind_double(statement, 4, risk);
        sqlite3_bind_int(statement, 5, observation.observed ? 0 : 1);
        if (sqlite3_step(statement) != SQLITE_DONE) {
            sqlite3_finalize(statement);
            throw std::runtime_error("cannot persist hourly heat result");
        }
        sqlite3_reset(statement);
        sqlite3_clear_bindings(statement);
    }
    sqlite3_finalize(statement);
}

}  // namespace eco_restoration
