// filename: src/cpp/generated/cyboquatic_drainagedecay_kernel_struct.hpp
// license: MIT OR Apache-2.0
// generated from ALN particle cyboquatic.drainagedecay.kernel
#ifndef CYBOQUATIC_DRAINAGEDECAY_KERNEL_STRUCT_HPP
#define CYBOQUATIC_DRAINAGEDECAY_KERNEL_STRUCT_HPP

struct cyboquatic_drainagedecay_kernel_struct {
    const char* frameId;
    const char* canalNodeId;
    const char* kerProfileId;
    const char* timestampUtc;
    double bodMgL;
    double tssMgL;
    double cecCmolPerKg;
    double frameEnergyJ;
    double deltaVtMps;
    double k;
    double e;
    double r;
    double kerScore;
    const char* fogRegionId;
    const char* fogChannelId;
    const char* governanceParticleHex;
    const char* evidenceHex;
    const char* signingDid;
};

#endif // CYBOQUATIC_DRAINAGEDECAY_KERNEL_STRUCT_HPP
