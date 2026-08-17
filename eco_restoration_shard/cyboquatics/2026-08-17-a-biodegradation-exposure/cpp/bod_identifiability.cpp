#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

struct Observation {
    double time_days;
    double bod_mg_per_l;
};

static double clamp01(double value) {
    return std::clamp(value, 0.0, 1.0);
}

static int numerical_rank_2x2(double a, double b, double c, double d) {
    const double s11 = a * a + c * c;
    const double s12 = a * b + c * d;
    const double s22 = b * b + d * d;
    const double trace = s11 + s22;
    const double determinant = s11 * s22 - s12 * s12;
    const double discriminant = std::max(0.0, trace * trace - 4.0 * determinant);
    const double lambda_max = 0.5 * (trace + std::sqrt(discriminant));
    const double lambda_min = 0.5 * (trace - std::sqrt(discriminant));
    const double tolerance = std::max(1.0e-12, lambda_max * 1.0e-10);

    if (lambda_max <= tolerance) {
        return 0;
    }
    return lambda_min > tolerance ? 2 : 1;
}

static int jacobian_rank(const std::vector<Observation>& observations, double bod_u, double k) {
    if (observations.empty() || bod_u <= 0.0 || k <= 0.0) {
        return 0;
    }

    double j11 = 0.0;
    double j12 = 0.0;
    double j22 = 0.0;

    for (const Observation& observation : observations) {
        if (observation.time_days < 0.0) {
            throw std::invalid_argument("time_days must be non-negative");
        }

        const double exp_term = std::exp(-k * observation.time_days);
        const double d_bodu = 1.0 - exp_term;
        const double d_k = bod_u * observation.time_days * exp_term;

        j11 += d_bodu * d_bodu;
        j12 += d_bodu * d_k;
        j22 += d_k * d_k;
    }

    return numerical_rank_2x2(j11, j12, j12, j22);
}

static double estimate_k_known_bodu(const Observation& observation, double bod_u) {
    if (bod_u <= 0.0 || observation.time_days <= 0.0 ||
        observation.bod_mg_per_l <= 0.0 || observation.bod_mg_per_l >= bod_u) {
        throw std::invalid_argument(
            "require BODu > BOD(t) > 0 and t > 0 for a finite first-order k estimate"
        );
    }
    return -std::log(1.0 - observation.bod_mg_per_l / bod_u) / observation.time_days;
}

int main(int argc, char** argv) {
    if (argc < 6 || ((argc - 3) % 2 != 0)) {
        std::cerr
            << "usage: " << argv[0]
            << " <BODu_mg_L> <candidate_k_per_day> <time_days> <BOD_mg_L> [<time_days> <BOD_mg_L> ...]\n";
        return 64;
    }

    try {
        const double bod_u = std::stod(argv[1]);
        const double candidate_k = std::stod(argv[2]);

        if (bod_u <= 0.0 || candidate_k <= 0.0) {
            throw std::invalid_argument("BODu and candidate k must be positive");
        }

        std::vector<Observation> observations;
        for (int index = 3; index < argc; index += 2) {
            Observation observation{std::stod(argv[index]), std::stod(argv[index + 1])};
            if (observation.time_days < 0.0 || observation.bod_mg_per_l < 0.0) {
                throw std::invalid_argument("time and BOD must be non-negative");
            }
            observations.push_back(observation);
        }

        const int rank = jacobian_rank(observations, bod_u, candidate_k);
        const Observation& first = observations.front();

        std::cout << std::fixed << std::setprecision(8);
        std::cout << "jacobian_rank_for_[BODu,k]=" << rank << '\n';
        std::cout << "local_identifiability_for_[BODu,k]=" << (rank == 2 ? "POSSIBLE" : "NOT_ESTABLISHED") << '\n';

        if (first.time_days > 0.0 && first.bod_mg_per_l > 0.0 && first.bod_mg_per_l < bod_u) {
            const double k_estimate = estimate_k_known_bodu(first, bod_u);
            std::cout << "k_estimate_if_BODu_known_per_day=" << k_estimate << '\n';
        } else {
            std::cout << "k_estimate_if_BODu_known_per_day=UNAVAILABLE\n";
        }

        const double positive_time_fraction = std::count_if(
            observations.begin(), observations.end(),
            [](const Observation& value) { return value.time_days > 0.0; }
        ) / static_cast<double>(observations.size());

        const double knowledge_factor = clamp01(
            0.25 + 0.35 * std::min(1.0, observations.size() / 4.0) +
            0.25 * positive_time_fraction + 0.15 * (rank == 2 ? 1.0 : 0.0)
        );
        const double harm_risk = clamp01(
            0.20 + 0.45 * (rank < 2 ? 1.0 : 0.0) +
            0.20 * (observations.size() < 3 ? 1.0 : 0.0)
        );
        const double eco_impact_value = clamp01(knowledge_factor * (1.0 - harm_risk));

        std::cout << "knowledge_factor=" << knowledge_factor << '\n';
        std::cout << "eco_impact_value=" << eco_impact_value << '\n';
        std::cout << "harm_risk=" << harm_risk << '\n';
        std::cout << "decision="
                  << (rank == 2 && observations.size() >= 3 ? "USE_AS_SCREENING_ESTIMATE" : "COLLECT_MORE_DATA")
                  << '\n';
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "error: " << error.what() << '\n';
        return 65;
    }
}
