// File: eco_restoration_shard/cyboquatic_progress/20260714/cpp/drainagedecay_bod_tss_cec.cpp
// Domain (e): drainagedecay frames (BOD, TSS, CEC) for cyboquatic machinery.
// Standard C++17 code; no external dependencies. Build with: g++ -std=c++17 -O2 -o drainagedecay_bod_tss_cec drainagedecay_bod_tss_cec.cpp

#include <cmath>
#include <cfloat>
#include <iostream>
#include <stdexcept>
#include <string>

struct DrainageState {
    double bodMgL;
    double tssMgL;
    double cecCmolKg;
    double temperatureC;
    double flowLps;
};

struct DecayParameters {
    double kBodPerDay;
    double kTssPerDay;
    double theta;
    double refTempC;
};

static double firstOrderDecay(double initial, double kPerHour, double dtHours) {
    if (initial <= 0.0) return 0.0;
    if (kPerHour <= 0.0 || dtHours == 0.0) return initial;
    double exponent = -kPerHour * dtHours;
    return initial * std::exp(exponent);
}

static double temperatureFactor(double theta, double refTempC, double currentTempC) {
    double delta = currentTempC - refTempC;
    return std::exp(std::log(theta) * (delta / 10.0));
}

static DrainageState step(const DrainageState &state, const DecayParameters &params, double dtHours) {
    if (dtHours < 0.0) {
        throw std::invalid_argument("dtHours must be non-negative");
    }

    double tempFactor = temperatureFactor(params.theta, params.refTempC, state.temperatureC);

    double kBodPerHour = params.kBodPerDay / 24.0 * tempFactor;
    double kTssPerHour = params.kTssPerDay / 24.0 * tempFactor;

    double bodNext = firstOrderDecay(state.bodMgL, kBodPerHour, dtHours);
    double tssNext = firstOrderDecay(state.tssMgL, kTssPerHour, dtHours);

    DrainageState result = state;
    result.bodMgL = bodNext < 0.0 ? 0.0 : bodNext;
    result.tssMgL = tssNext < 0.0 ? 0.0 : tssNext;

    return result;
}

static double oxygenDemandMgPerSec(const DrainageState &state) {
    double bodNonNegative = state.bodMgL < 0.0 ? 0.0 : state.bodMgL;
    double flowNonNegative = state.flowLps < 0.0 ? 0.0 : state.flowLps;
    return bodNonNegative * flowNonNegative / 1000.0;
}

static std::string toString(const DrainageState &s) {
    std::string out;
    out += "DrainageState{";
    out += "bodMgL=" + std::to_string(s.bodMgL);
    out += ", tssMgL=" + std::to_string(s.tssMgL);
    out += ", cecCmolKg=" + std::to_string(s.cecCmolKg);
    out += ", temperatureC=" + std::to_string(s.temperatureC);
    out += ", flowLps=" + std::to_string(s.flowLps);
    out += "}";
    return out;
}

int main() {
    DrainageState state{45.0, 90.0, 30.0, 23.0, 6.0};
    DecayParameters params{0.22, 0.08, 1.05, 20.0};
    double dtHours = 3.0;

    DrainageState next = step(state, params, dtHours);
    double oxygenDemand = oxygenDemandMgPerSec(next);

    std::cout << "Initial: " << toString(state) << "\n";
    std::cout << "Next after " << dtHours << " h: " << toString(next) << "\n";
    std::cout << "Oxygen demand (mg O2/s): " << oxygenDemand << "\n";

    return 0;
}
