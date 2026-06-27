// aletheion_erm/ecosafety/ALE-ERM-ECOSAFETY-TYPES-CPP-001.hpp
// Ecosafety grammar spine (C++ types) for cyboquatic nodes, corridors, and Lyapunov checks. [file:15]

#ifndef ALE_ERM_ECOSAFETY_TYPES_CPP_001_HPP
#define ALE_ERM_ECOSAFETY_TYPES_CPP_001_HPP

#include <string>
#include <vector>
#include <optional>

namespace aletheion {
namespace ecosafety {

// Normalized risk coordinate (rx in [0,1]). [file:15]
struct RiskCoord {
    std::string name;     // e.g., "rmicroplastics", "rtox", "noise_dBA".
    double value;         // current normalized risk.
    double minsafe;       // lower bound of safe corridor band.
    double maxsafe;       // upper bound of safe corridor band.
};

// Vector of risk coordinates for a node or workflow.
struct RiskVector {
    std::vector<RiskCoord> coords;
};

// Lyapunov residual V_t bound for a corridor. [file:15]
struct LyapunovResidual {
    std::string corridor_id;
    double Vt_current;
    double Vt_max;
    double Vt_margin;
    bool stable;
};

enum class CorridorStatus {
    Ok,
    SoftViolation,  // derate or recycle.
    HardViolation   // stop / no build.
};

struct Corridor {
    std::string corridor_id;
    std::string domain;          // "water", "thermal", "waste", etc.
    std::vector<RiskCoord> bands;
    double Vt_max;
};

struct CorridorEvalResult {
    CorridorStatus status;
    std::optional<LyapunovResidual> vt;
    std::vector<std::string> violations; // names of violated coords or treaties.
};

// C++ façade for Rust ecosafety evaluation functions. [file:15]
CorridorEvalResult eval_corridor(const Corridor& corridor,
                                 const RiskVector& current_rx);

// Check Lyapunov stability (Vt_current <= Vt_max, margin >= 0). [file:15]
bool check_lyapunov(const LyapunovResidual& vt);

} // namespace ecosafety
} // namespace aletheion

#endif // ALE_ERM_ECOSAFETY_TYPES_CPP_001_HPP
