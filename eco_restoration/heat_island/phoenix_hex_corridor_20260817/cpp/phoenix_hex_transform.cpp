#include <cmath>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>

struct Cube {
    long long q;
    long long r;
    long long s;
};

struct HexResult {
    double x_m;
    double y_m;
    Cube cube;
    double center_x_m;
    double center_y_m;
    double center_latitude_deg;
    double center_longitude_deg;
};

static double round_half_away_from_zero(double value) {
    return value >= 0.0 ? std::floor(value + 0.5) : std::ceil(value - 0.5);
}

static Cube cube_round(double q_float, double r_float, double s_float) {
    long long q = static_cast<long long>(round_half_away_from_zero(q_float));
    long long r = static_cast<long long>(round_half_away_from_zero(r_float));
    long long s = static_cast<long long>(round_half_away_from_zero(s_float));

    const double q_error = std::abs(static_cast<double>(q) - q_float);
    const double r_error = std::abs(static_cast<double>(r) - r_float);
    const double s_error = std::abs(static_cast<double>(s) - s_float);

    if (q_error > r_error && q_error > s_error) {
        q = -r - s;
    } else if (r_error > s_error) {
        r = -q - s;
    } else {
        s = -q - r;
    }

    return {q, r, s};
}

static HexResult encode(
    double latitude_deg,
    double longitude_deg,
    double origin_latitude_deg,
    double origin_longitude_deg,
    double meters_per_degree_latitude,
    double meters_per_degree_longitude,
    double side_length_m
) {
    if (!std::isfinite(latitude_deg) || !std::isfinite(longitude_deg) ||
        !std::isfinite(origin_latitude_deg) || !std::isfinite(origin_longitude_deg) ||
        meters_per_degree_latitude <= 0.0 || meters_per_degree_longitude <= 0.0 ||
        side_length_m <= 0.0) {
        throw std::invalid_argument("coordinates and transform parameters must be finite; scale values must be positive");
    }

    const double x_m = (longitude_deg - origin_longitude_deg) * meters_per_degree_longitude;
    const double y_m = (latitude_deg - origin_latitude_deg) * meters_per_degree_latitude;

    const double q_float = (std::sqrt(3.0) / 3.0 * x_m - y_m / 3.0) / side_length_m;
    const double r_float = (2.0 / 3.0 * y_m) / side_length_m;
    const Cube cube = cube_round(q_float, r_float, -q_float - r_float);

    const double center_x_m = side_length_m * std::sqrt(3.0) * (
        static_cast<double>(cube.q) + static_cast<double>(cube.r) / 2.0
    );
    const double center_y_m = side_length_m * 1.5 * static_cast<double>(cube.r);
    const double center_latitude_deg = origin_latitude_deg + center_y_m / meters_per_degree_latitude;
    const double center_longitude_deg = origin_longitude_deg + center_x_m / meters_per_degree_longitude;

    return {
        x_m, y_m, cube,
        center_x_m, center_y_m,
        center_latitude_deg, center_longitude_deg
    };
}

int main(int argc, char** argv) {
    if (argc != 8) {
        std::cerr
            << "usage: " << argv[0]
            << " <lat_deg> <lon_deg> <origin_lat_deg> <origin_lon_deg>"
            << " <meters_per_degree_lat> <meters_per_degree_lon> <hex_side_m>\n";
        return 64;
    }

    try {
        const HexResult result = encode(
            std::stod(argv[1]),
            std::stod(argv[2]),
            std::stod(argv[3]),
            std::stod(argv[4]),
            std::stod(argv[5]),
            std::stod(argv[6]),
            std::stod(argv[7])
        );

        std::cout << std::fixed << std::setprecision(10);
        std::cout << "x_m=" << result.x_m << '\n';
        std::cout << "y_m=" << result.y_m << '\n';
        std::cout << "q=" << result.cube.q << '\n';
        std::cout << "r=" << result.cube.r << '\n';
        std::cout << "s=" << result.cube.s << '\n';
        std::cout << "hex_center_x_m=" << result.center_x_m << '\n';
        std::cout << "hex_center_y_m=" << result.center_y_m << '\n';
        std::cout << "hex_center_lat_deg=" << result.center_latitude_deg << '\n';
        std::cout << "hex_center_lon_deg=" << result.center_longitude_deg << '\n';
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "error: " << error.what() << '\n';
        return 65;
    }
}
