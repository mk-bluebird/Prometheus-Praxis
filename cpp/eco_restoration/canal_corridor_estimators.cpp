// File: cpp/eco_restoration/canal_corridor_estimators.cpp

#include <Eigen/Dense>
#include <gdal_priv.h>
#include <sqlite3.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <queue>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace eco_restoration {

struct BiodiversityMetrics {
    double effective_mesh_m2{};
    std::uint32_t patch_count{};
    double connectivity{};
    double fragmentation{};
    double risk{};
};

BiodiversityMetrics biodiversity_from_raster(
    const std::string& raster_path,
    double center_x,
    double center_y,
    int habitat_class,
    double buffer_m = 1000.0) {

    GDALAllRegister();
    GDALDataset* dataset = static_cast<GDALDataset*>(GDALOpen(raster_path.c_str(), GA_ReadOnly));
    if (dataset == nullptr || dataset->GetRasterCount() < 1) {
        if (dataset != nullptr) GDALClose(dataset);
        throw std::runtime_error("cannot open land-cover raster");
    }

    const int width = dataset->GetRasterXSize();
    const int height = dataset->GetRasterYSize();
    double transform[6]{};
    if (dataset->GetGeoTransform(transform) != CE_None) {
        GDALClose(dataset);
        throw std::runtime_error("raster requires geotransform");
    }

    const double pixel_area = std::abs(transform[1] * transform[5] - transform[2] * transform[4]);
    if (pixel_area <= 0.0) {
        GDALClose(dataset);
        throw std::runtime_error("invalid raster pixel area");
    }

    std::vector<int> classes(static_cast<std::size_t>(width) * height);
    if (dataset->GetRasterBand(1)->RasterIO(
            GF_Read, 0, 0, width, height, classes.data(), width, height, GDT_Int32, 0, 0) != CE_None) {
        GDALClose(dataset);
        throw std::runtime_error("cannot read land-cover raster");
    }
    GDALClose(dataset);

    std::vector<std::uint8_t> habitat(classes.size(), 0);
    std::vector<std::uint8_t> visited(classes.size(), 0);
    std::size_t habitat_pixels = 0;
    std::size_t buffered_pixels = 0;
    std::vector<double> patch_areas;

    const auto offset = [width](int x, int y) {
        return static_cast<std::size_t>(y) * static_cast<std::size_t>(width) + x;
    };

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const double px = transform[0] + (x + 0.5) * transform[1] + (y + 0.5) * transform[2];
            const double py = transform[3] + (x + 0.5) * transform[4] + (y + 0.5) * transform[5];
            if (std::hypot(px - center_x, py - center_y) > buffer_m) continue;
            ++buffered_pixels;
            if (classes[offset(x, y)] == habitat_class) {
                habitat[offset(x, y)] = 1;
                ++habitat_pixels;
            }
        }
    }

    constexpr int neighbors[8][2]{{-1,-1},{-1,0},{-1,1},{0,-1},{0,1},{1,-1},{1,0},{1,1}};
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            if (!habitat[offset(x, y)] || visited[offset(x, y)]) continue;
            std::queue<std::pair<int, int>> frontier;
            frontier.push({x, y});
            visited[offset(x, y)] = 1;
            std::size_t pixels = 0;

            while (!frontier.empty()) {
                const auto [cx, cy] = frontier.front();
                frontier.pop();
                ++pixels;
                for (const auto& delta : neighbors) {
                    const int nx = cx + delta[0];
                    const int ny = cy + delta[1];
                    if (nx < 0 || ny < 0 || nx >= width || ny >= height) continue;
                    const std::size_t index = offset(nx, ny);
                    if (habitat[index] && !visited[index]) {
                        visited[index] = 1;
                        frontier.push({nx, ny});
                    }
                }
            }
            patch_areas.push_back(static_cast<double>(pixels) * pixel_area);
        }
    }

    const double buffer_area = std::max(1.0, static_cast<double>(buffered_pixels) * pixel_area);
    double squared_area_sum = 0.0;
    for (double area : patch_areas) squared_area_sum += area * area;

    const double mesh = squared_area_sum / buffer_area;
    const double connectivity = std::clamp(mesh / buffer_area, 0.0, 1.0);
    const double fragmentation = habitat_pixels == 0U
        ? 1.0
        : std::clamp(static_cast<double>(patch_areas.size()) / static_cast<double>(habitat_pixels), 0.0, 1.0);

    return {
        mesh,
        static_cast<std::uint32_t>(patch_areas.size()),
        connectivity,
        fragmentation,
        std::clamp(0.65 * (1.0 - connectivity) + 0.35 * fragmentation, 0.0, 1.0)
    };
}

struct SedimentObservation {
    double velocity_m_s{};
    double concentration_kg_m3{};
    double d50_m{};
    double observed_deposition_kg_m2{};
};

struct SedimentCalibration {
    double drag_coefficient{};
    double critical_shields{};
    double root_mean_square_error{};
};

double predicted_deposition(const SedimentObservation& observation, double drag, double shields_critical) {
    constexpr double water_density = 1000.0;
    constexpr double sediment_density = 2650.0;
    constexpr double gravity = 9.80665;

    const double shear_stress = drag * water_density * observation.velocity_m_s * observation.velocity_m_s;
    const double shields = shear_stress /
        std::max(1e-12, (sediment_density - water_density) * gravity * observation.d50_m);
    return observation.concentration_kg_m3 * std::max(0.0, shields_critical - shields);
}

SedimentCalibration calibrate_sediment(const std::vector<SedimentObservation>& observations) {
    if (observations.size() < 4U) throw std::invalid_argument("four sediment observations are required");

    Eigen::Vector2d parameter(0.01, 0.05);
    for (int iteration = 0; iteration < 300; ++iteration) {
        Eigen::Matrix2d hessian = Eigen::Matrix2d::Identity() * 1e-8;
        Eigen::Vector2d gradient = Eigen::Vector2d::Zero();

        for (const auto& observation : observations) {
            const double prediction = predicted_deposition(observation, parameter[0], parameter[1]);
            const double residual = prediction - observation.observed_deposition_kg_m2;
            const double epsilon = 1e-6;
            const double drag_derivative =
                (predicted_deposition(observation, parameter[0] + epsilon, parameter[1]) - prediction) / epsilon;
            const double shield_derivative =
                (predicted_deposition(observation, parameter[0], parameter[1] + epsilon) - prediction) / epsilon;
            Eigen::Vector2d jacobian(drag_derivative, shield_derivative);
            gradient += jacobian * residual;
            hessian += jacobian * jacobian.transpose();
        }

        const Eigen::Vector2d update = hessian.ldlt().solve(gradient);
        parameter = (parameter - update).cwiseMax(1e-8);
        if (update.norm() < 1e-9) break;
    }

    double squared_error = 0.0;
    for (const auto& observation : observations) {
        const double error = predicted_deposition(observation, parameter[0], parameter[1]) -
                             observation.observed_deposition_kg_m2;
        squared_error += error * error;
    }

    return {parameter[0], parameter[1], std::sqrt(squared_error / observations.size())};
}

struct SeepageEstimate {
    double storage_m3{};
    double seepage_m3_s{};
    double seepage_standard_deviation{};
};

class CanalSeepageFilter {
public:
    CanalSeepageFilter(double storage_m3, double seepage_m3_s)
        : state_(storage_m3, seepage_m3_s) {
        covariance_ << 10.0, 0.0, 0.0, 0.01;
    }

    SeepageEstimate update(
        double upstream_flow_m3_s,
        double downstream_flow_m3_s,
        double observed_storage_m3,
        double dt_s) {

        if (dt_s <= 0.0 || upstream_flow_m3_s < 0.0 || downstream_flow_m3_s < 0.0 ||
            observed_storage_m3 < 0.0) {
            throw std::invalid_argument("invalid canal gauge observation");
        }

        Eigen::Matrix2d transition;
        transition << 1.0, -dt_s, 0.0, 1.0;
        Eigen::Vector2d input(dt_s * (upstream_flow_m3_s - downstream_flow_m3_s), 0.0);
        const Eigen::Matrix2d process_noise = (Eigen::Vector2d(0.2, 0.002)).asDiagonal();
        const Eigen::RowVector2d measurement(1.0, 0.0);
        constexpr double measurement_noise = 1.0;

        state_ = transition * state_ + input;
        covariance_ = transition * covariance_ * transition.transpose() + process_noise;
        const double innovation = observed_storage_m3 - measurement.dot(state_);
        const double innovation_variance = (measurement * covariance_ * measurement.transpose())(0, 0) +
                                            measurement_noise;
        const Eigen::Vector2d gain = covariance_ * measurement.transpose() / innovation_variance;
        state_ += gain * innovation;
        covariance_ = (Eigen::Matrix2d::Identity() - gain * measurement) * covariance_;

        return {state_[0], state_[1], std::sqrt(std::max(0.0, covariance_(1, 1)))};
    }

private:
    Eigen::Vector2d state_;
    Eigen::Matrix2d covariance_;
};

void persist_seepage(
    sqlite3* database,
    std::uint64_t hex_anchor,
    std::int64_t observed_unix_s,
    const SeepageEstimate& estimate) {

    sqlite3_exec(database,
        "CREATE TABLE IF NOT EXISTS canal_seepage_estimate("
        "hex_anchor INTEGER NOT NULL,observed_unix_s INTEGER NOT NULL,"
        "storage_m3 REAL NOT NULL,seepage_m3_s REAL NOT NULL,seepage_sd REAL NOT NULL,"
        "PRIMARY KEY(hex_anchor,observed_unix_s)) STRICT;",
        nullptr, nullptr, nullptr);

    sqlite3_stmt* statement = nullptr;
    sqlite3_prepare_v2(database,
        "INSERT INTO canal_seepage_estimate VALUES(?,?,?,?,?) "
        "ON CONFLICT(hex_anchor,observed_unix_s) DO UPDATE SET "
        "storage_m3=excluded.storage_m3,seepage_m3_s=excluded.seepage_m3,seepage_sd=excluded.seepage_sd;",
        -1, &statement, nullptr);

    sqlite3_bind_int64(statement, 1, static_cast<sqlite3_int64>(hex_anchor));
    sqlite3_bind_int64(statement, 2, static_cast<sqlite3_int64>(observed_unix_s));
    sqlite3_bind_double(statement, 3, estimate.storage_m3);
    sqlite3_bind_double(statement, 4, estimate.seepage_m3_s);
    sqlite3_bind_double(statement, 5, estimate.seepage_standard_deviation);

    if (sqlite3_step(statement) != SQLITE_DONE) {
        sqlite3_finalize(statement);
        throw std::runtime_error("cannot persist seepage estimate");
    }
    sqlite3_finalize(statement);
}

}  // namespace eco_restoration
