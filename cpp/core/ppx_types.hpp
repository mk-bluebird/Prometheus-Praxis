// filename: cpp/core/ppx_types.hpp
#pragma once
#include <string>
#include <vector>
#include <chrono>

namespace ppx {

struct EcoNodeId {
    std::string region;     // e.g. "Phoenix-AZ"
    std::string system;     // e.g. "CyboquaticChannel"
    std::string node;       // e.g. "NODE-001"
};

struct TimeStampUtc {
    std::chrono::system_clock::time_point tp;
};

struct RiskVector {
    double r_energy       = 0.0;
    double r_hydraulics   = 0.0;
    double r_carbon       = 0.0;
    double r_materials    = 0.0;
    double r_biology      = 0.0;
    double r_dataquality  = 0.0;
};

struct KerSnapshot {
    EcoNodeId      node_id;
    TimeStampUtc   ts;
    double         vt      = 0.0;   // Lyapunov residual.
    double         k       = 0.0;   // Knowledge factor.
    double         e       = 0.0;   // Eco-impact factor.
    double         r       = 0.0;   // Risk-of-harm factor.
    RiskVector     rv;
};

} // namespace ppx
