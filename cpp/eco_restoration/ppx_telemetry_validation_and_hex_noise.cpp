// File: cpp/eco_restoration/ppx_telemetry_validation_and_hex_noise.cpp
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <random>
#include <string>

namespace ppx::eco_restoration {

struct MachineTelemetry {
    std::string machine_id;
    std::string station_id;
    std::string timestamp_utc;
    double r_hydraulics{};
    double r_energy{};
    double r_uncertainty{};
    double r_reliability{};
    double roh{};
    double ecological_value{};
};

template <typename T, std::size_t N>
struct UnitIntervalValidator {
    using Member = double T::*;
    std::array<Member, N> constraints;

    [[nodiscard]] bool valid(const T& value) const noexcept {
        bool accepted = !value.machine_id.empty();
        accepted &= !value.station_id.empty();
        accepted &= !value.timestamp_utc.empty();
        for (const Member member : constraints) {
            const double coordinate = value.*member;
            accepted &= std::isfinite(coordinate);
            accepted &= coordinate >= 0.0;
            accepted &= coordinate <= 1.0;
        }
        return accepted;
    }
};

constexpr UnitIntervalValidator<MachineTelemetry, 6> kTelemetryValidator{{
    &MachineTelemetry::r_hydraulics,
    &MachineTelemetry::r_energy,
    &MachineTelemetry::r_uncertainty,
    &MachineTelemetry::r_reliability,
    &MachineTelemetry::roh,
    &MachineTelemetry::ecological_value,
}};

struct AxialCell {
    std::int64_t q{};
    std::int64_t r{};
    friend bool operator==(const AxialCell&, const AxialCell&) = default;
};

std::int64_t round_nearest(double value) {
    return static_cast<std::int64_t>(std::floor(value + 0.5));
}

AxialCell point_to_hex(double x_m, double y_m, double edge_m) {
    const double r_fraction = 2.0 * y_m / (3.0 * edge_m);
    const double q_fraction = x_m / (std::sqrt(3.0) * edge_m) - 0.5 * r_fraction;
    const double s_fraction = -q_fraction - r_fraction;

    std::int64_t q = round_nearest(q_fraction);
    std::int64_t r = round_nearest(r_fraction);
    std::int64_t s = round_nearest(s_fraction);

    const double q_error = std::abs(static_cast<double>(q) - q_fraction);
    const double r_error = std::abs(static_cast<double>(r) - r_fraction);
    const double s_error = std::abs(static_cast<double>(s) - s_fraction);

    if (q_error > r_error && q_error > s_error) q = -r - s;
    else if (r_error > s_error) r = -q - s;
    return {q, r};
}

double simulate_same_anchor_probability(
    double gps_sigma_m, double edge_m, std::uint64_t trials, std::uint64_t seed = 20260811) {
    if (gps_sigma_m <= 0.0 || edge_m <= 0.0 || trials == 0) {
        throw std::invalid_argument("sigma, edge, and trial count must be positive");
    }

    std::mt19937_64 generator(seed);
    std::normal_distribution<double> noise(0.0, gps_sigma_m);
    std::uint64_t matches = 0;
    for (std::uint64_t i = 0; i < trials; ++i) {
        const AxialCell first = point_to_hex(noise(generator), noise(generator), edge_m);
        const AxialCell second = point_to_hex(noise(generator), noise(generator), edge_m);
        matches += first == second;
    }
    return static_cast<double>(matches) / static_cast<double>(trials);
}

double minimum_edge_for_consistency(
    double gps_sigma_m, double target_probability, std::uint64_t trials) {
    if (target_probability <= 0.0 || target_probability >= 1.0) {
        throw std::invalid_argument("target probability must be within (0,1)");
    }

    double low = gps_sigma_m;
    double high = gps_sigma_m;
    while (simulate_same_anchor_probability(gps_sigma_m, high, trials) < target_probability) {
        high *= 2.0;
    }
    for (int iteration = 0; iteration < 24; ++iteration) {
        const double middle = 0.5 * (low + high);
        if (simulate_same_anchor_probability(gps_sigma_m, middle, trials) >= target_probability) {
            high = middle;
        } else {
            low = middle;
        }
    }
    return high;
}

void benchmark_validation() {
    const MachineTelemetry frame{
        "pump-ai-01", "phoenix-canal-02", "2026-08-11T23:03:00Z",
        0.10, 0.12, 0.03, 0.05, 0.08, 0.90
    };
    constexpr std::uint64_t iterations = 100'000'000;
    volatile std::uint64_t accepted = 0;

    const auto start = std::chrono::steady_clock::now();
    for (std::uint64_t i = 0; i < iterations; ++i) {
        accepted += kTelemetryValidator.valid(frame);
    }
    const auto elapsed = std::chrono::steady_clock::now() - start;
    const double seconds = std::chrono::duration<double>(elapsed).count();

    std::cout << "accepted=" << accepted
              << "\titerations=" << iterations
              << "\tseconds=" << seconds
              << "\tvalidations_per_second=" << static_cast<double>(iterations) / seconds
              << '\n';
}

}  // namespace ppx::eco_restoration

int main(int argc, char* argv[]) {
    using namespace ppx::eco_restoration;
    try {
        if (argc == 2 && std::string(argv[1]) == "--bench") {
            benchmark_validation();
            return EXIT_SUCCESS;
        }
        if (argc == 4 && std::string(argv[1]) == "--noise") {
            const double sigma_m = std::stod(argv[2]);
            const std::uint64_t trials = std::stoull(argv[3]);
            const double edge_m = minimum_edge_for_consistency(sigma_m, 0.999, trials);
            const double verified = simulate_same_anchor_probability(sigma_m, edge_m, trials);
            std::cout << "sigma_m=" << sigma_m
                      << "\ttarget_probability=0.999"
                      << "\trequired_edge_m=" << edge_m
                      << "\tsimulated_probability=" << verified << '\n';
            return EXIT_SUCCESS;
        }
        std::cerr << "Usage: ppx_kernel --bench | --noise GPS_SIGMA_M TRIALS\n";
        return EXIT_FAILURE;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
