// filename: crates/prometheus_praxis_ai/src/engine/cpp/eco_engine_drainage.cpp

#include "eco_engine_drainage.hpp"

namespace {

inline double clamp01(double x) {
    if (x < 0.0) return 0.0;
    if (x > 1.0) return 1.0;
    return x;
}

} // namespace

extern "C" int compute_drainage_decay(
    double bod_mg_l,
    double tss_mg_l,
    double cec_cmol_per_kg,
    double flow_rate_m3s,
    double water_temp_c,
    double elevation_m,
    CDrainageRiskCoords* out_risks,
    CResidualSlice* out_residual
) {
    if (out_risks == nullptr || out_residual == nullptr) {
        return 1;
    }

    if (bod_mg_l < 0.0 || bod_mg_l > 80.0) return 2;
    if (tss_mg_l < 0.0 || tss_mg_l > 500.0) return 3;
    if (cec_cmol_per_kg < 0.0 || cec_cmol_per_kg > 50.0) return 4;
    if (flow_rate_m3s < 0.0) return 5;
    if (water_temp_c < 0.0 || water_temp_c > 45.0) return 6;
    if (elevation_m < -100.0 || elevation_m > 2000.0) return 7;

    double r_bod = bod_mg_l / 80.0;
    double r_tss = tss_mg_l / 500.0;
    double r_cec = cec_cmol_per_kg / 50.0;

    double r_hydraulics = (flow_rate_m3s > 10.0) ? 0.7 : 0.3;
    double r_uncertainty = (water_temp_c > 35.0) ? 0.6 : 0.3;

    r_bod = clamp01(r_bod);
    r_tss = clamp01(r_tss);
    r_cec = clamp01(r_cec);
    r_hydraulics = clamp01(r_hydraulics);
    r_uncertainty = clamp01(r_uncertainty);

    out_risks->r_bod = r_bod;
    out_risks->r_tss = r_tss;
    out_risks->r_cec = r_cec;
    out_risks->r_hydraulics = r_hydraulics;
    out_risks->r_uncertainty = r_uncertainty;

    const double w_bod = 0.9;
    const double w_tss = 0.7;
    const double w_cec = 0.6;
    const double w_hydraulics = 1.0;
    const double w_uncertainty = 0.8;

    double vt_before = 0.0;
    double vt_after =
        w_bod * r_bod * r_bod +
        w_tss * r_tss * r_tss +
        w_cec * r_cec * r_cec +
        w_hydraulics * r_hydraulics * r_hydraulics +
        w_uncertainty * r_uncertainty * r_uncertainty;

    if (vt_after < 0.0) vt_after = 0.0;

    double delta_vt = vt_after - vt_before;

    out_residual->vt_before = vt_before;
    out_residual->vt_after = vt_after;
    out_residual->delta_vt = delta_vt;

    return 0;
}
