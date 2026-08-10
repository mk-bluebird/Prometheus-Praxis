/* File: cyboquatic-core/include/cyboquatic_core.h */
#ifndef CYBOQUATIC_CORE_H
#define CYBOQUATIC_CORE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct WorkloadInput {
    double flow_m3_s;
    double lift_m;
    double efficiency;
    double runtime_s;
    double voltage_drop_v;
    double renewable_fraction;
    double embodied_carbon_g_per_j;
    double biodiversity_risk;
} WorkloadInput;

typedef struct WorkloadAssessment {
    double energyreq_j;
    double delta_vt;
    double knowledge_factor;
    double eco_impact_value;
    uint8_t accepted;
} WorkloadAssessment;

int32_t assess_workload(
    const WorkloadInput* input,
    WorkloadAssessment* output
);

#ifdef __cplusplus
}
#endif

#endif
