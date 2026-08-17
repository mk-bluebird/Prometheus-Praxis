#include <cmath>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

static double half_life_days(double rate_per_day) {
    return rate_per_day > 0.0 ? std::log(2.0) / rate_per_day : INFINITY;
}

int main(int argc, char** argv) {
    if (argc < 5) {
        std::cerr
            << "usage: " << argv[0]
            << " <target_retained_fraction_0_to_1> <horizon_days> <k0_per_day> <k1_per_day> [<k2_per_day> ...]\n";
        return 64;
    }

    try {
        const double target_fraction = std::stod(argv[1]);
        const double horizon_days = std::stod(argv[2]);

        if (target_fraction <= 0.0 || target_fraction >= 1.0 || horizon_days <= 0.0) {
            throw std::invalid_argument("target retained fraction must be in (0,1); horizon must be positive");
        }

        std::vector<double> rates;
        for (int index = 3; index < argc; ++index) {
            const double rate = std::stod(argv[index]);
            if (rate < 0.0) {
                throw std::invalid_argument("first-order rates must be non-negative");
            }
            rates.push_back(rate);
        }

        const double critical_terminal_rate = -std::log(target_fraction) / horizon_days;
        const double critical_terminal_half_life = half_life_days(critical_terminal_rate);
        bool asymptotic_clearance = true;

        std::cout << std::fixed << std::setprecision(8);
        std::cout << "critical_terminal_rate_per_day=" << critical_terminal_rate << '\n';
        std::cout << "maximum_terminal_half_life_days_for_target=" << critical_terminal_half_life << '\n';

        for (std::size_t index = 0; index < rates.size(); ++index) {
            const double eigenvalue = -rates[index];
            const bool clears = rates[index] > 0.0;
            asymptotic_clearance = asymptotic_clearance && clears;

            std::cout << "state=" << index
                      << " eigenvalue=" << eigenvalue
                      << " half_life_days=" << half_life_days(rates[index])
                      << " clears_asymptotically=" << (clears ? "true" : "false")
                      << '\n';
        }

        const double terminal_rate = rates.back();
        const bool terminal_meets_horizon = terminal_rate >= critical_terminal_rate;

        std::cout << "chain_asymptotic_clearance=" << (asymptotic_clearance ? "true" : "false") << '\n';
        std::cout << "terminal_meets_horizon_target=" << (terminal_meets_horizon ? "true" : "false") << '\n';
        std::cout << "decision=" << (
            asymptotic_clearance && terminal_meets_horizon
                ? "CLEARANCE_SCREEN_PASSED"
                : "ACCUMULATION_OR_PERSISTENCE_RISK_REQUIRES_REVIEW"
        ) << '\n';
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "error: " << error.what() << '\n';
        return 65;
    }
}
