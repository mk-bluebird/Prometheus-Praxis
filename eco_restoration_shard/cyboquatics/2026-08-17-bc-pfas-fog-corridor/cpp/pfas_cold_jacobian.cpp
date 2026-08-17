#include <cmath>
#include <iomanip>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>

struct Eigenvalues {
    double real_1;
    double imag_1;
    double real_2;
    double imag_2;
    double maximum_real;
};

static bool is_finite(double value) {
    return std::isfinite(value);
}

static Eigenvalues eigenvalues_2x2(double a, double b, double c, double d) {
    const double trace = a + d;
    const double determinant = a * d - b * c;
    const double discriminant = trace * trace - 4.0 * determinant;

    if (discriminant >= 0.0) {
        const double root = std::sqrt(discriminant);
        const double lambda_1 = 0.5 * (trace + root);
        const double lambda_2 = 0.5 * (trace - root);
        return {lambda_1, 0.0, lambda_2, 0.0, std::max(lambda_1, lambda_2)};
    }

    const double real_part = 0.5 * trace;
    const double imaginary_part = 0.5 * std::sqrt(-discriminant);
    return {real_part, imaginary_part, real_part, -imaginary_part, real_part};
}

int main(int argc, char** argv) {
    if (argc != 8) {
        std::cerr
            << "usage: " << argv[0]
            << " <C_mg_L> <S_kg_m3> <removalRate_per_day> <partitionRate_m3_kg_day>"
            << " <releaseRate_per_day> <settlingRate_per_day> <temperature_c>\n";
        return 64;
    }

    try {
        const double concentration = std::stod(argv[1]);
        const double sediment_state = std::stod(argv[2]);
        const double removal_rate = std::stod(argv[3]);
        const double partition_rate = std::stod(argv[4]);
        const double release_rate = std::stod(argv[5]);
        const double settling_rate = std::stod(argv[6]);
        const double temperature_c = std::stod(argv[7]);

        const double values[] = {
            concentration, sediment_state, removal_rate, partition_rate,
            release_rate, settling_rate, temperature_c
        };

        for (double value : values) {
            if (!is_finite(value) || value < 0.0) {
                throw std::invalid_argument("all values must be finite and non-negative");
            }
        }

        const double j11 = -removal_rate - partition_rate * sediment_state;
        const double j12 = -partition_rate * concentration;
        const double j21 = release_rate;
        const double j22 = -settling_rate;

        const double trace = j11 + j22;
        const double determinant = j11 * j22 - j12 * j21;
        const Eigenvalues eigen = eigenvalues_2x2(j11, j12, j21, j22);

        const bool contracting = eigen.maximum_real < 0.0;
        const bool boundary = std::abs(eigen.maximum_real) <= 1.0e-8;
        const double knowledge_factor = temperature_c <= 5.0 ? 0.55 : 0.70;
        const double harm_risk = contracting ? 0.25 : 0.80;
        const double eco_impact_value = contracting ? 0.60 * knowledge_factor : 0.15 * knowledge_factor;

        std::cout << std::fixed << std::setprecision(10);
        std::cout << "J11=" << j11 << '\n';
        std::cout << "J12=" << j12 << '\n';
        std::cout << "J21=" << j21 << '\n';
        std::cout << "J22=" << j22 << '\n';
        std::cout << "trace=" << trace << '\n';
        std::cout << "determinant=" << determinant << '\n';
        std::cout << "eigenvalue_1=" << eigen.real_1 << (eigen.imag_1 >= 0.0 ? "+" : "") << eigen.imag_1 << "i\n";
        std::cout << "eigenvalue_2=" << eigen.real_2 << (eigen.imag_2 >= 0.0 ? "+" : "") << eigen.imag_2 << "i\n";
        std::cout << "lambda_max_real=" << eigen.maximum_real << '\n';
        std::cout << "contraction_status="
                  << (contracting ? "CONTRACTING" : (boundary ? "BOUNDARY_REVIEW" : "LOST_CONTRACTION"))
                  << '\n';
        std::cout << "knowledge_factor=" << knowledge_factor << '\n';
        std::cout << "eco_impact_value=" << eco_impact_value << '\n';
        std::cout << "harm_risk=" << harm_risk << '\n';
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "error: " << error.what() << '\n';
        return 65;
    }
}
