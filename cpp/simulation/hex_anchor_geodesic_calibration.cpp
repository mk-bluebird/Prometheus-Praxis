// File: cpp/simulation/hex_anchor_geodesic_calibration.cpp

#include <Eigen/Dense>
#include <geodesic.h>
#include <proj.h>
#include <sqlite3.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

namespace eco_restoration {

constexpr double pi = 3.14159265358979323846;

struct Point {
    double x{};
    double y{};
};

struct Sample {
    Point planar{};
    Point truth{};
};

class UtmCalibration {
public:
    UtmCalibration(double latitude_deg, double longitude_deg)
        : context_(proj_context_create()) {
        if (context_ == nullptr) {
            throw std::runtime_error("PROJ context creation failed");
        }
        PJ* raw = proj_create_crs_to_crs(context_, "EPSG:4326", "EPSG:32612", nullptr);
        transform_ = proj_normalize_for_visualization(context_, raw);
        proj_destroy(raw);
        if (transform_ == nullptr) {
            proj_context_destroy(context_);
            throw std::runtime_error("UTM transformation creation failed");
        }
        origin_ = project(latitude_deg, longitude_deg);
        geod_init(&geodesic_, 6378137.0, 1.0 / 298.257223563);
        latitude_deg_ = latitude_deg;
        longitude_deg_ = longitude_deg;
    }

    ~UtmCalibration() {
        proj_destroy(transform_);
        proj_context_destroy(context_);
    }

    UtmCalibration(const UtmCalibration&) = delete;
    UtmCalibration& operator=(const UtmCalibration&) = delete;

    [[nodiscard]] Sample sample(double distance_m, double bearing_deg) const {
        double latitude = 0.0;
        double longitude = 0.0;
        geod_direct(&geodesic_, latitude_deg_, longitude_deg_, bearing_deg, distance_m,
                    &latitude, &longitude, nullptr);

        const double radians = bearing_deg * pi / 180.0;
        const Point planar{
            origin_.x + distance_m * std::sin(radians),
            origin_.y + distance_m * std::cos(radians)
        };
        return {planar, project(latitude, longitude)};
    }

    [[nodiscard]] std::vector<Sample> training_samples(
        double maximum_distance_m, std::size_t radial_steps, std::size_t bearings) const {

        std::vector<Sample> samples;
        for (std::size_t r = 1; r <= radial_steps; ++r) {
            const double distance = maximum_distance_m * static_cast<double>(r) /
                                    static_cast<double>(radial_steps);
            for (std::size_t b = 0; b < bearings; ++b) {
                samples.push_back(sample(distance, 360.0 * static_cast<double>(b) /
                                                    static_cast<double>(bearings)));
            }
        }
        return samples;
    }

    [[nodiscard]] Eigen::VectorXd fit_correction(const std::vector<Sample>& samples, bool x_axis) const {
        Eigen::MatrixXd design(samples.size(), 6);
        Eigen::VectorXd target(samples.size());

        for (std::size_t i = 0; i < samples.size(); ++i) {
            const double x = samples[i].planar.x - origin_.x;
            const double y = samples[i].planar.y - origin_.y;
            design.row(static_cast<Eigen::Index>(i)) << 1.0, x, y, x * x, x * y, y * y;
            target(static_cast<Eigen::Index>(i)) =
                (x_axis ? samples[i].truth.x : samples[i].truth.y) -
                (x_axis ? samples[i].planar.x : samples[i].planar.y);
        }
        return design.colPivHouseholderQr().solve(target);
    }

    [[nodiscard]] Point corrected(const Point& planar, const Eigen::VectorXd& x, const Eigen::VectorXd& y) const {
        const double local_x = planar.x - origin_.x;
        const double local_y = planar.y - origin_.y;
        Eigen::Matrix<double, 6, 1> basis;
        basis << 1.0, local_x, local_y, local_x * local_x, local_x * local_y, local_y * local_y;
        return {planar.x + basis.dot(x), planar.y + basis.dot(y)};
    }

    [[nodiscard]] static double error_m(const Point& a, const Point& b) {
        return std::hypot(a.x - b.x, a.y - b.y);
    }

private:
    [[nodiscard]] Point project(double latitude_deg, double longitude_deg) const {
        const PJ_COORD coordinate = proj_coord(longitude_deg, latitude_deg, 0.0, 0.0);
        const PJ_COORD result = proj_trans(transform_, PJ_FWD, coordinate);
        if (!std::isfinite(result.enu.e) || !std::isfinite(result.enu.n)) {
            throw std::runtime_error("PROJ coordinate transformation failed");
        }
        return {result.enu.e, result.enu.n};
    }

    PJ_CONTEXT* context_{};
    PJ* transform_{};
    geod_geodesic geodesic_{};
    Point origin_{};
    double latitude_deg_{};
    double longitude_deg_{};
};

struct Cube {
    double q{};
    double r{};
    double s{};
};

std::pair<int, int> anchor_for(double x, double y, double edge_m) {
    const double q = ((std::sqrt(3.0) / 3.0) * x - y / 3.0) / edge_m;
    const double r = (2.0 * y / 3.0) / edge_m;
    Cube cube{q, r, -q - r};

    int qi = static_cast<int>(std::round(cube.q));
    int ri = static_cast<int>(std::round(cube.r));
    const int si = static_cast<int>(std::round(cube.s));

    const double q_error = std::abs(static_cast<double>(qi) - cube.q);
    const double r_error = std::abs(static_cast<double>(ri) - cube.r);
    const double s_error = std::abs(static_cast<double>(si) - cube.s);

    if (q_error > r_error && q_error > s_error) {
        qi = -ri - si;
    } else if (r_error > s_error) {
        ri = -qi - si;
    }
    return {qi, ri};
}

double inconsistent_anchor_probability(
    double edge_m,
    double sigma_m,
    std::size_t draws,
    double multipath_probability,
    double multipath_sigma_m) {

    std::mt19937_64 generator(0xECOA12026ULL);
    std::normal_distribution<double> unit_normal(0.0, 1.0);
    std::bernoulli_distribution multipath(clamp(multipath_probability, 0.0, 1.0));

    std::size_t mismatches = 0;
    for (std::size_t i = 0; i < draws; ++i) {
        const double first_sigma = multipath(generator) ? multipath_sigma_m : sigma_m;
        const double second_sigma = multipath(generator) ? multipath_sigma_m : sigma_m;
        const auto first = anchor_for(first_sigma * unit_normal(generator),
                                      first_sigma * unit_normal(generator), edge_m);
        const auto second = anchor_for(second_sigma * unit_normal(generator),
                                       second_sigma * unit_normal(generator), edge_m);
        mismatches += first != second ? 1U : 0U;
    }
    return static_cast<double>(mismatches) / static_cast<double>(draws);
}

double required_edge_for_consistency(
    double sigma_m,
    double target_consistency,
    std::size_t draws,
    double multipath_probability,
    double multipath_sigma_m) {

    double low = 0.01;
    double high = 1.0;
    while (1.0 - inconsistent_anchor_probability(
                     high, sigma_m, draws, multipath_probability, multipath_sigma_m) <
           target_consistency) {
        high *= 2.0;
        if (high > 100000.0) {
            throw std::runtime_error("unable to satisfy requested consistency");
        }
    }

    for (int iteration = 0; iteration < 32; ++iteration) {
        const double middle = (low + high) * 0.5;
        const double consistency = 1.0 - inconsistent_anchor_probability(
            middle, sigma_m, draws, multipath_probability, multipath_sigma_m);
        if (consistency >= target_consistency) {
            high = middle;
        } else {
            low = middle;
        }
    }
    return high;
}

double clamp(double value, double low, double high) {
    return std::max(low, std::min(value, high));
}

void persist_coefficients(const std::string& database_path,
                          const Eigen::VectorXd& x,
                          const Eigen::VectorXd& y) {
    sqlite3* database = nullptr;
    if (sqlite3_open(database_path.c_str(), &database) != SQLITE_OK) {
        throw std::runtime_error("SQLite database open failed");
    }

    const char* schema =
        "CREATE TABLE IF NOT EXISTS hex_anchor_correction ("
        "term INTEGER PRIMARY KEY, x_coefficient REAL NOT NULL, y_coefficient REAL NOT NULL);";
    if (sqlite3_exec(database, schema, nullptr, nullptr, nullptr) != SQLITE_OK) {
        sqlite3_close(database);
        throw std::runtime_error("SQLite schema creation failed");
    }

    sqlite3_stmt* statement = nullptr;
    sqlite3_prepare_v2(database,
        "INSERT INTO hex_anchor_correction(term,x_coefficient,y_coefficient) VALUES(?,?,?) "
        "ON CONFLICT(term) DO UPDATE SET x_coefficient=excluded.x_coefficient,"
        "y_coefficient=excluded.y_coefficient;",
        -1, &statement, nullptr);

    for (int i = 0; i < x.size(); ++i) {
        sqlite3_bind_int(statement, 1, i);
        sqlite3_bind_double(statement, 2, x[i]);
        sqlite3_bind_double(statement, 3, y[i]);
        if (sqlite3_step(statement) != SQLITE_DONE) {
            sqlite3_finalize(statement);
            sqlite3_close(database);
            throw std::runtime_error("SQLite coefficient persistence failed");
        }
        sqlite3_reset(statement);
        sqlite3_clear_bindings(statement);
    }
    sqlite3_finalize(statement);
    sqlite3_close(database);
}

}  // namespace eco_restoration

int main(int argc, char** argv) {
    using namespace eco_restoration;

    if (argc != 4) {
        std::cerr << "usage: hex_anchor_geodesic_calibration latitude longitude coefficients.sqlite\n";
        return 2;
    }

    try {
        const UtmCalibration calibration{std::stod(argv[1]), std::stod(argv[2])};
        const std::vector<Sample> training = calibration.training_samples(50000.0, 100, 180);
        const Eigen::VectorXd correction_x = calibration.fit_correction(training, true);
        const Eigen::VectorXd correction_y = calibration.fit_correction(training, false);
        persist_coefficients(argv[3], correction_x, correction_y);

        std::cout << "distance_m,raw_max_error_m,corrected_max_error_m\n";
        for (int distance = 1000; distance <= 50000; distance += 1000) {
            double raw_maximum = 0.0;
            double corrected_maximum = 0.0;
            for (int bearing = 0; bearing < 360; ++bearing) {
                const Sample sample = calibration.sample(static_cast<double>(distance),
                                                          static_cast<double>(bearing));
                raw_maximum = std::max(raw_maximum, UtmCalibration::error_m(sample.planar, sample.truth));
                corrected_maximum = std::max(
                    corrected_maximum,
                    UtmCalibration::error_m(calibration.corrected(
                        sample.planar, correction_x, correction_y), sample.truth));
            }
            std::cout << distance << ',' << raw_maximum << ',' << corrected_maximum << '\n';
        }

        std::cout << "\nsigma_m,edge_m,mismatch_probability,required_edge_99_9_m\n";
        for (const double sigma : {0.5, 2.0, 5.0}) {
            const double required = required_edge_for_consistency(sigma, 0.999, 200000, 0.03, 3.0 * sigma);
            for (const double edge : {1.0, 2.0, 5.0, 10.0}) {
                const double mismatch = inconsistent_anchor_probability(
                    edge, sigma, 200000, 0.03, 3.0 * sigma);
                std::cout << sigma << ',' << edge << ',' << mismatch << ',' << required << '\n';
            }
        }
    } catch (const std::exception& error) {
        std::cerr << "{\"error\":\"" << error.what() << "\"}\n";
        return 1;
    }
}
