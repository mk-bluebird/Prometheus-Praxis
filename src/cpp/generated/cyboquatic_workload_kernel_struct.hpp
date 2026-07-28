// filename: src/cpp/generated/cyboquatic_workload_kernel_struct.hpp
// license: MIT OR Apache-2.0
// generated from ALN particle cyboquatic.workload.kernel
#ifndef CYBOQUATIC_WORKLOAD_KERNEL_STRUCT_HPP
#define CYBOQUATIC_WORKLOAD_KERNEL_STRUCT_HPP

struct cyboquatic_workload_kernel_struct {
    const char* nodeId;
    const char* dateUtc;
    const char* windowStartUtc;
    const char* windowEndUtc;
    const char* domainCode;
    const char* subtaskId;
    double energyReqJ;
    double throughputM3;
    double headM;
    double dutyCycle;
    double vtCurrent;
    double vtNext;
    double vtDelta;
    double k;
    double e;
    double r;
    double kerScore;
    const char* lane;
    bool safeToPromote;
    const char* evidenceHex;
    const char* signingDid;
};

#endif // CYBOQUATIC_WORKLOAD_KERNEL_STRUCT_HPP
