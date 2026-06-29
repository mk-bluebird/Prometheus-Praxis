#pragma once
#include "eco_state.hpp"

inline bool eco_inside_soft_corridor(const EcoState2& x,
                                     const Corridor2& c,
                                     float margin)
{
    return (x.x1 > (c.x1_min + margin)) &&
           (x.x1 < (c.x1_max - margin)) &&
           (x.x2 > (c.x2_min + margin)) &&
           (x.x2 < (c.x2_max - margin));
}

inline EcoCommand eco_corridor_override(const EcoState2& x,
                                        const Corridor2& c,
                                        const EcoCommand& nominal)
{
    EcoCommand out = nominal;

    if (x.x1 <= c.x1_min || x.x1 >= c.x1_max ||
        x.x2 <= c.x2_min || x.x2 >= c.x2_max)
    {
        out.u = 0.0f;
    }

    if (out.u < c.u_min) out.u = c.u_min;
    if (out.u > c.u_max) out.u = c.u_max;

    return out;
}
