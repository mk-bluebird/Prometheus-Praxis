#pragma once
#include "eco_state.hpp"

inline float eco_compute_V(const EcoState2& x,
                           const EcoSetpoint2& ref,
                           const LyapunovConfig2& cfg)
{
    const float dx1 = x.x1 - ref.x1_ref;
    const float dx2 = x.x2 - ref.x2_ref;
    return cfg.p1 * dx1 * dx1 + cfg.p2 * dx2 * dx2;
}

inline EcoCommand eco_state_feedback(const EcoState2& x,
                                     const EcoSetpoint2& ref,
                                     float k1, float k2,
                                     const Corridor2& cor)
{
    const float dx1 = x.x1 - ref.x1_ref;
    const float dx2 = x.x2 - ref.x2_ref;

    float u = -k1 * dx1 - k2 * dx2;

    if (u < cor.u_min) u = cor.u_min;
    if (u > cor.u_max) u = cor.u_max;

    EcoCommand cmd;
    cmd.u = u;
    return cmd;
}
