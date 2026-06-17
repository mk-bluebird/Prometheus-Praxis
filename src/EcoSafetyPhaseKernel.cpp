// filename: src/EcoSafetyPhaseKernel.cpp
// destination: ecosafety-core/src

#include "EcoSafetyPhaseKernel.hpp"

namespace EcoNet {

SafetyPhase EcoSafetyPhaseKernel::classify(const EspkInputRow& in,
                                           const EspkParams& p) {
    const double B = in.badj;
    const double R = in.r;
    const double Dt = in.dt;

    // 1. FORBID region
    if (Dt < p.dtminPilot) return SafetyPhase::FORBID;
    if (R  > p.rmaxPilot) return SafetyPhase::FORBID;
    if (B  < p.bminPilot) return SafetyPhase::FORBID;

    // 2. DEPLOYABLE region
    if (Dt >= p.dtminDeploy &&
        R  <= p.rmaxDeploy &&
        B  >= p.bminDeploy) {
        return SafetyPhase::DEPLOYABLE;
    }

    // 3. Otherwise PILOT
    return SafetyPhase::PILOT;
}

std::string EcoSafetyPhaseKernel::toPhaseString(SafetyPhase p) {
    switch (p) {
        case SafetyPhase::FORBID:     return "FORBID";
        case SafetyPhase::PILOT:      return "PILOT";
        case SafetyPhase::DEPLOYABLE: return "DEPLOYABLE";
    }
    return "FORBID";
}

} // namespace EcoNet
