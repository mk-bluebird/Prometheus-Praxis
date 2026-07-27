// filename: src/embedded/arm/flatbuffer_surcharge_bridge.c
// target-repo: https://github.com/mk-bluebird/Prometheus-Praxis
// language: C (ARM-friendly, no heap)
// license: MIT OR Apache-2.0

#include <stddef.h>
#include <stdint.h>

// These structs must exactly match the layout used in ecoenginesurcharge.cpp
// and in the LuaJIT ffi cdef snippet for SurchargeEventInput and BlastRadiusOutput.[file:3]

typedef struct {
    double   canallengthm;
    double   canalwidthm;
    double   upstreamflowm3s;
    double   surchargedepthm;
    double   gateopenfraction;   // 0..1
    double   soilceccmolkg;
    double   bodmgl;
    double   tssmgl;
    double   vtbefore;           // Lyapunov residual slice before event
    uint32_t hexid;              // Phoenix hex anchor id
} SurchargeEventInput;

typedef struct {
    double   maxdepthdownstreamm;
    double   maxvelocitymps;
    double   radiusovertopm;
    double   radiusscourm;
    double   pfosriskcoord;      // 0..1, PFAS/FOG risk plane
    double   kfactor;            // Knowledge
    double   efactor;            // Eco-impact
    double   rfactor;            // Risk-of-harm
    uint32_t evidencehex;        // hex-stamp for this event
} BlastRadiusOutput;

// Clamp helper for risk coordinates.[file:3]
static inline double clamp01(double x) {
    if (x < 0.0) return 0.0;
    if (x > 1.0) return 1.0;
    return x;
}

// Core numeric kernel operating on flat buffers.
// - inbuffer: points to a SurchargeEventInput region.
// - outbuffer: points to a BlastRadiusOutput region (may alias inbuffer).
// - insize: must equal sizeof(SurchargeEventInput).
// - outsize: must equal sizeof(BlastRadiusOutput).
//
// Returns 0 on success, nonzero error code otherwise.
// No dynamic memory allocation; only stack locals are used.[file:3]
int computeblastradiusflat(const void* inbuffer,
                           void* outbuffer,
                           size_t insize,
                           size_t outsize)
{
    if (inbuffer == NULL || outbuffer == NULL) {
        return 1;
    }
    if (insize != sizeof(SurchargeEventInput) ||
        outsize != sizeof(BlastRadiusOutput)) {
        return 2;
    }

    const SurchargeEventInput* in  = (const SurchargeEventInput*)inbuffer;
    BlastRadiusOutput*         out = (BlastRadiusOutput*)outbuffer;

    // Simple hydraulics approximations; replace with calibrated kernels.[file:3]
    const double area_m2 = in->canalwidthm * in->surchargedepthm;
    const double velocity_mps =
        (area_m2 > 0.0) ? (in->upstreamflowm3s / area_m2) : 0.0;

    const double depth_decay = 0.35; // corridor-derived factor
    const double scour_decay = 0.25;

    const double maxdepthdownstream =
        in->surchargedepthm * depth_decay * (1.0 + in->gateopenfraction);

    const double radiusovertop =
        in->canalwidthm * 0.5 * (0.5 + in->gateopenfraction);

    const double radiusscour =
        velocity_mps * scour_decay * (in->canallengthm / 10.0);

    // Risk coordinates PFAS/FOG plane, simplified.[file:3]
    double rpfos = 0.0;
    rpfos += 0.4 * clamp01(in->bodmgl / 20.0);
    rpfos += 0.4 * clamp01(in->tssmgl / 200.0);
    rpfos += 0.2 * clamp01(in->soilceccmolkg / 25.0);
    rpfos = clamp01(rpfos);

    // Lyapunov residual update local slice, no global state.[file:3]
    const double vt_before = clamp01(in->vtbefore);
    const double vt_after  = clamp01(0.3 * rpfos * rpfos + vt_before);
    const double delta_vt  = vt_after - vt_before;

    // KER scoring consistent with existing grammar.[file:3]
    double k = 0.9 - 0.3 * rpfos;
    if (delta_vt > 0.0) {
        k -= 0.2; // penalty for residual increase
    }
    if (k < 0.0) k = 0.0;

    double e = 0.9 - vt_after;
    if (delta_vt > 0.0) {
        e -= 0.15; // eco-impact penalty for worsening residual
    }
    if (e < 0.0) e = 0.0;

    double r = vt_after;
    if (delta_vt > 0.0 && delta_vt > r) {
        r = delta_vt; // emphasize positive jumps as risk.[file:3]
    }
    r = clamp01(r);

    // Populate output POD.[file:3]
    out->maxdepthdownstreamm = maxdepthdownstream;
    out->maxvelocitymps      = velocity_mps;
    out->radiusovertopm      = radiusovertop;
    out->radiusscourm        = radiusscour;
    out->pfosriskcoord       = rpfos;
    out->kfactor             = k;
    out->efactor             = e;
    out->rfactor             = r;
    out->evidencehex         = in->hexid;

    return 0;
}
