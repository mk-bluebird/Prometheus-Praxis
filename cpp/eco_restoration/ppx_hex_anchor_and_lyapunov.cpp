// File: cpp/eco_restoration/ppx_hex_anchor_and_lyapunov.cpp
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>

namespace ppx::eco_restoration {

constexpr double kPi = 3.1415926535897932384626433832795;
constexpr double kEarthRadiusM = 6378137.0;
constexpr std::int32_t kCoordinateLimit = (1 << 25) - 1;

struct LatLon {
    double latitude_deg{};
    double longitude_deg{};
};

struct LocalFrame {
    double reference_latitude_deg{33.4484};
    double reference_longitude_deg{-112.0740};
    double hex_edge_m{0.50};
};

struct HexCell {
    std::int32_t q{};
    std::int32_t r{};
    std::uint8_t level{};
};

struct StabilityCalibration {
    std::array<double, 3> p_diagonal{};
    double current_v{};
    double basin_v{};
    double spectral_decay{};
    double max_delta_vt{};
    bool strict_decrease_required{};
};

double radians(double degrees) { return degrees * kPi / 180.0; }
double degrees(double radians_value) { return radians_value * 180.0 / kPi; }

std::uint64_t encode_anchor(const HexCell& cell) {
    if (cell.level > 63 || std::abs(cell.q) > kCoordinateLimit ||
        std::abs(cell.r) > kCoordinateLimit) {
        throw std::out_of_range("hexagonal coordinates exceed 64-bit anchor range");
    }
    const std::uint64_t q = static_cast<std::uint32_t>(cell.q + kCoordinateLimit);
    const std::uint64_t r = static_cast<std::uint32_t>(cell.r + kCoordinateLimit);
    return (static_cast<std::uint64_t>(cell.level) << 52U) | (q << 26U) | r;
}

HexCell decode_anchor(std::uint64_t anchor) {
    const auto level = static_cast<std::uint8_t>((anchor >> 52U) & 0x3FU);
    const auto q = static_cast<std::int32_t>((anchor >> 26U) & 0x3FFFFFFU) - kCoordinateLimit;
    const auto r = static_cast<std::int32_t>(anchor & 0x3FFFFFFU) - kCoordinateLimit;
    return {q, r, level};
}

double edge_length_m(const LocalFrame& frame, std::uint8_t level) {
    return std::ldexp(frame.hex_edge_m, level);
}

std::array<double, 2> anchor_to_local_centroid(std::uint64_t anchor, const LocalFrame& frame) {
    const HexCell cell = decode_anchor(anchor);
    const double edge = edge_length_m(frame, cell.level);
    const double x = edge * std::sqrt(3.0) * (static_cast<double>(cell.q) + 0.5 * cell.r);
    const double y = edge * 1.5 * static_cast<double>(cell.r);
    return {x, y};
}

LatLon local_centroid_to_latlon(const std::array<double, 2>& xy, const LocalFrame& frame) {
    const double reference_lat = radians(frame.reference_latitude_deg);
    const double latitude = reference_lat + xy[1] / kEarthRadiusM;
    const double longitude = radians(frame.reference_longitude_deg) +
                             xy[0] / (kEarthRadiusM * std::cos(reference_lat));
    return {degrees(latitude), degrees(longitude)};
}

LatLon anchor_to_latlon(std::uint64_t anchor, const LocalFrame& frame) {
    return local_centroid_to_latlon(anchor_to_local_centroid(anchor, frame), frame);
}

std::uint64_t ieee754_cell_size_bits(const LocalFrame& frame) {
    return std::bit_cast<std::uint64_t>(frame.hex_edge_m);
}

StabilityCalibration calibrate_max_delta_vt(
    const std::array<double, 3>& state,
    const std::array<double, 3>& continuous_eigenvalues,
    const std::array<double, 3>& q_diagonal,
    double sampling_seconds,
    double basin_v,
    double bounded_disturbance_v) {
    if (sampling_seconds <= 0.0 || basin_v <= 0.0 || bounded_disturbance_v < 0.0) {
        throw std::invalid_argument("sampling, basin, and disturbance bounds are invalid");
    }

    StabilityCalibration result{};
    result.basin_v = basin_v;
    double rho = 0.0;
    for (std::size_t i = 0; i < 3; ++i) {
        if (continuous_eigenvalues[i] >= 0.0 || q_diagonal[i] <= 0.0) {
            throw std::invalid_argument("dissipative modes require negative eigenvalues and positive Q");
        }
        result.p_diagonal[i] = q_diagonal[i] / (-2.0 * continuous_eigenvalues[i]);
        result.current_v += result.p_diagonal[i] * state[i] * state[i];
        rho = std::max(rho, std::exp(continuous_eigenvalues[i] * sampling_seconds));
    }
    if (result.current_v >= basin_v) {
        throw std::range_error("state is outside the certified Lyapunov basin");
    }

    result.spectral_decay = rho * rho;
    const double basin_headroom = basin_v - result.current_v;
    const double dissipative_budget =
        bounded_disturbance_v - (1.0 - result.spectral_decay) * result.current_v;
    result.max_delta_vt = std::min(basin_headroom, dissipative_budget);
    result.strict_decrease_required = result.max_delta_vt < 0.0;
    return result;
}

}  // namespace ppx::eco_restoration

int main(int argc, char* argv[]) {
    using namespace ppx::eco_restoration;
    if (argc != 3 || std::string(argv[1]) != "--anchor") {
        std::cerr << "Usage: ppx_hex_anchor --anchor ANCHOR_ID\n";
        return EXIT_FAILURE;
    }
    try {
        const std::uint64_t anchor = std::stoull(argv[2]);
        const LocalFrame phoenix{};
        const LatLon centroid = anchor_to_latlon(anchor, phoenix);
        std::cout << std::fixed << std::setprecision(9)
                  << "latitude=" << centroid.latitude_deg
                  << "\tlongitude=" << centroid.longitude_deg
                  << "\tcell_edge_m=" << edge_length_m(phoenix, decode_anchor(anchor).level)
                  << "\tcell_size_ieee754=" << ieee754_cell_size_bits(phoenix) << '\n';
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
