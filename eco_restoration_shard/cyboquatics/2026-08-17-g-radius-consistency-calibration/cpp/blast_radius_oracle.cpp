#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>

struct Result {
    std::string zone;
    long long scaled_radius;
};

static double parse_finite(const char* text, const char* label) {
    const double value = std::stod(text);
    if (!std::isfinite(value)) {
        throw std::invalid_argument(std::string(label) + " must be finite");
    }
    return value;
}

static Result classify(double q, double t, double sensitivity, double distance, double coefficient) {
    if (q <= 0.0 || t <= 0.0 || coefficient <= 0.0 ||
        sensitivity < 0.0 || sensitivity > 1.0 || distance < 0.0) {
        throw std::invalid_argument("input outside shared conformance domain");
    }

    const double radius = coefficient * std::sqrt(q * t) * (1.0 + 1.5 * sensitivity);
    if (!std::isfinite(radius) || radius < 0.0) {
        throw std::invalid_argument("non-finite conservative radius");
    }

    const long long scaled = static_cast<long long>(std::llround(radius * 1000000.0));
    const std::string zone =
        distance > radius ? "SAFE" :
        distance > radius / 2.0 ? "CAUTION" :
        "EXCLUDE";

    return {zone, scaled};
}

int main(int argc, char** argv) {
    if (argc != 6) {
        std::cerr << "usage: " << argv[0] << " <Q_m3_s> <T_s> <S_b_0_to_1> <distance_m> <c>\n";
        return 64;
    }

    try {
        const Result result = classify(
            parse_finite(argv[1], "Q"),
            parse_finite(argv[2], "T"),
            parse_finite(argv[3], "S_b"),
            parse_finite(argv[4], "distance"),
            parse_finite(argv[5], "c")
        );
        std::cout << result.zone << "|" << result.scaled_radius << '\n';
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "error: " << error.what() << '\n';
        return 65;
    }
}
