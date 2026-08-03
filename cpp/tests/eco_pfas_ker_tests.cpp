// File: cpp/tests/eco_pfas_ker_tests.cpp
// NOTE: This file assumes an existing C++ test framework is available,
// e.g., Catch2 or GoogleTest integrated into the Prometheus-Praxis repo.
// The example below uses a minimal assertion style that can be adapted
// to the actual framework macros (REQUIRE/EXPECT_*).

#include <vector>
#include <cmath>
#include "eco_restoration.hpp"

static void assert_true(bool cond, const char* msg) {
    if (!cond) {
        std::cerr << "[TEST FAIL] " << msg << "\n";
        std::exit(1);
    }
}

static void test_pfas_mass_non_increase_under_cold_survival() {
    eco_pfas::PFASState s{};
    s.mass_kg = 0.002;
    s.sorbed_fraction = 0.5;
    s.cold_survival_factor = 1.0;

    double base_rate = 0.01;
    double current_temp_C = 10.0; // cold corridor
    double cold_temp_C = 12.0;
    double sorption_increment = 0.001;

    eco_pfas::PFASState next = eco_pfas::step_pfas_corridor(
        s, base_rate, current_temp_C, cold_temp_C, sorption_increment
    );

    assert_true(next.mass_kg <= s.mass_kg + 1e-12,
                "PFAS mass must not increase under cold-survival corridor.");
    assert_true(next.sorbed_fraction >= 0.0 && next.sorbed_fraction <= 1.0,
                "Sorbed fraction must stay within [0,1].");
    assert_true(next.cold_survival_factor >= 0.0,
                "Cold-survival factor must be non-negative.");
}

static void test_ker_scoring_ranges() {
    double k = 0.94;
    double r_max = 0.12;
    double e = 1.0 - r_max;
    if (e < 0.0) e = 0.0;

    double s = eco_tools::ker_score(k, e, r_max);

    assert_true(k >= 0.0 && k <= 1.0, "K must be in [0,1].");
    assert_true(e >= 0.0 && e <= 1.0, "E must be in [0,1].");
    assert_true(r_max >= 0.0 && r_max <= 1.0, "R must be in [0,1].");

    assert_true(std::isfinite(s), "KER score must be finite.");
}

static void test_deltaVt_monotonicity_when_ker_positive() {
    // Synthetic sequence of deltaVt and ker_score; when ker>0, deltaVt must not increase.
    struct Sample {
        double deltaVt;
        double ker;
    };

    std::vector<Sample> seq = {
        {0.40, 0.10},
        {0.38, 0.12},
        {0.35, 0.15},
        {0.35, 0.05}, // ker still positive but deltaVt equal (OK)
        {0.36, 0.00}  // ker non-positive, no constraint
    };

    for (std::size_t i = 1; i < seq.size(); ++i) {
        const auto& prev = seq[i - 1];
        const auto& cur  = seq[i];
        if (prev.ker > 0.0) {
            assert_true(cur.deltaVt <= prev.deltaVt + 1e-9,
                        "deltaVt must be non-increasing when ker_score>0.");
        }
    }
}

int main() {
    test_pfas_mass_non_increase_under_cold_survival();
    test_ker_scoring_ranges();
    test_deltaVt_monotonicity_when_ker_positive();
    std::cout << "[TEST OK] eco_pfas_ker_tests passed.\n";
    return 0;
}
