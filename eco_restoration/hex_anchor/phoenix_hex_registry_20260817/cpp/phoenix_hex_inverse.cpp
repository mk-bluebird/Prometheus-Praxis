#include <cmath>
#include <iomanip>
#include <iostream>
#include <stdexcept>

struct HexCenter {
    double x_m;
    double y_m;
    double latitude_deg;
    double longitude_deg;
};

static HexCenter inverse_transform(
    long long q,
    long long r,
    long long s,
    double origin_latitude_deg,
    double origin_longitude_deg,
    double meters_per_degree_latitude,
    double meters_per_degree_longitude,
    double hex_side_length_m
) {
    if (s != -q - r) {
        throw std::invalid_argument("invalid cube coordinates: s must equal -q-r");
    }
    if (!std::isfinite(origin_latitude_deg) ||
        !std::isfinite(origin_longitude_deg) ||
        !std::isfinite(meters_per_degree_latitude) ||
        !std::isfinite(meters_per_degree_longitude) ||
        !std::isfinite(hex_side_length_m) ||
        meters_per_degree_latitude <= 0.0 ||
        meters_per_degree_longitude <= 0.0 ||
        hex_side_length_m <= 0.0) {
        throw std::invalid_argument("origin and scale values must be finite and positive where required");
    }

    const double x_m = hex_side_length_m * std::sqrt(3.0) * (
        static_cast<double>(q) + static_cast<double>(r) / 2.0
    );
    const double y_m = hex_side_length_m * 1.5 * static_cast<double>(r);
    const double latitude_deg = origin_latitude_deg + y_m / meters_per_degree_latitude;
    const double longitude_deg = origin_longitude_deg + x_m / meters_per_degree_longitude;

    return {x_m, y_m, latitude_deg, longitude_deg};
}

int main(int argc, char** argv) {
    if (argc != 9) {
        std::cerr
            << "usage: " << argv[0]
            << " <q> <r> <s> <origin_lat_deg> <origin_lon_deg>"
            << " <meters_per_degree_lat> <meters_per_degree_lon> <hex_side_m>\n";
        return 64;
    }

    try {
        const HexCenter result = inverse_transform(
            std::stoll(argv[1]),
            std::stoll(argv[2]),
            std::stoll(argv[3]),
            std::stod(argv[4]),
            std::stod(argv[5]),
            std::stod(argv[6]),
            std::stod(argv[7]),
            std::stod(argv[8])
        );

        std::cout << std::fixed << std::setprecision(10);
        std::cout << "center_x_m=" << result.x_m << '\n';
        std::cout << "center_y_m=" << result.y_m << '\n';
        std::cout << "center_latitude_deg=" << result.latitude_deg << '\n';
        std::cout << "center_longitude_deg=" << result.longitude_deg << '\n';
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "error: " << error.what() << '\n';
        return 65;
    }
}
