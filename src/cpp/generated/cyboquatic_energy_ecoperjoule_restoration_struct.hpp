// filename: src/cpp/generated/cyboquatic_energy_ecoperjoule_restoration_struct.hpp
// license: MIT OR Apache-2.0
// generated from ALN particle cyboquatic.energy.ecoperjoule.restoration
#ifndef CYBOQUATIC_ENERGY_ECOPERJOULE_RESTORATION_STRUCT_HPP
#define CYBOQUATIC_ENERGY_ECOPERJOULE_RESTORATION_STRUCT_HPP

struct cyboquatic_energy_ecoperjoule_restoration_struct {
    const char* frameId;
    const char* nodeId;
    const char* dateUtc;
    const char* windowStartUtc;
    const char* windowEndUtc;
    double energyReqJ;
    double ecoperJoule;
    bool restorationFlag;
    bool carbonNegativeOk;
    double k;
    double e;
    double r;
    double kerScore;
    const char* evidenceHex;
    const char* signingDid;
};

#endif // CYBOQUATIC_ENERGY_ECOPERJOULE_RESTORATION_STRUCT_HPP
