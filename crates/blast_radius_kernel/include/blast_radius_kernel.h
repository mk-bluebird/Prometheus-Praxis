/* File: crates/blast_radius_kernel/include/blast_radius_kernel.h */
#ifndef BLAST_RADIUS_KERNEL_H
#define BLAST_RADIUS_KERNEL_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct BlastRadiusInput {
    double energy_j;
    double energy_corridor_j;
    double attenuation_m_inv;
    double base_radius_m;
    double biodiversity_risk;
} BlastRadiusInput;

typedef struct BlastRadiusOutput {
    double radius_m;
    double normalized_energy_risk;
    double eco_impact_value;
    uint8_t within_corridor;
} BlastRadiusOutput;

int32_t compute_ecological_blast_radius(
    const BlastRadiusInput* input,
    BlastRadiusOutput* output
);

#ifdef __cplusplus
}
#endif

#endif
