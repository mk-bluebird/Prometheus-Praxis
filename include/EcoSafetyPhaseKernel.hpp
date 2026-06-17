// filename: include/EcoSafetyPhaseKernel.hpp
// destination: ecosafety-core/include

#pragma once
#include <string>
#include <optional>

namespace EcoNet {

struct EspkParams {
    std::string espkParamsId;
    std::string region;
    std::string contaminant; // empty if generic
    double bminDeploy;
    double rmaxDeploy;
    double dtminDeploy;
    double bminPilot;
    double rmaxPilot;
    double dtminPilot;
};

enum class SafetyPhase {
    FORBID,
    PILOT,
    DEPLOYABLE
};

struct EspkInputRow {
    std::string nodeId;
    std::string region;
    std::string contaminant;
    std::string windowStartIso;
    std::string windowEndIso;
    double badj;   // [0,1]
    double r;      // [0,1]
    double dt;     // [0,1]
    std::string espkParamsId;
};

struct EspkOutputRow {
    EspkInputRow input;
    SafetyPhase phase;
    std::string evidenceHex;
    std::string signingDid;
    std::string createdUtcIso;
};

class EcoSafetyPhaseKernel {
public:
    static SafetyPhase classify(const EspkInputRow& in,
                                const EspkParams& params);

    // convenience: string form for shard writing
    static std::string toPhaseString(SafetyPhase p);
};

} // namespace EcoNet
