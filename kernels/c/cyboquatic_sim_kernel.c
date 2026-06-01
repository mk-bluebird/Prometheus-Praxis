#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

typedef struct {
    double safe;
    double gold;
    double hard;
} CorridorBand;

typedef struct {
    double value;
} RiskCoord;

typedef struct {
    CorridorBand hlr_corridor;
    CorridorBand cec_corridor;
    CorridorBand pfas_corridor;
    CorridorBand t90_corridor;
    double weights[5];
} EcosafetyConfig;

typedef struct {
    double qm3s;
    double area_m2;
    double cin_ngl;
    double hlr_mperh;
    double length_m;
    int has_sat;
} Reach;

typedef struct {
    RiskCoord r_hlr;
    RiskCoord r_cec;
    RiskCoord r_pfas;
    RiskCoord r_t90;
    double vt;
    double k_score;
    double e_score;
    double r_score;
} ReachState;

RiskCoord normalize_corridor(double raw, CorridorBand *band) {
    RiskCoord rc;
    if (raw <= band->safe) {
        rc.value = 0.0;
    } else if (raw >= band->hard) {
        rc.value = 1.0;
    } else if (raw <= band->gold) {
        double t = (raw - band->safe) / (band->gold - band->safe + 1e-12);
        rc.value = 0.5 * t;
    } else {
        double t = (raw - band->gold) / (band->hard - band->gold + 1e-12);
        rc.value = 0.5 + 0.5 * t;
    }
    return rc;
}

double compute_lyapunov(RiskCoord r[], int n, double w[]) {
    double vt = 0.0;
    for (int i = 0; i < n; i++) {
        vt += w[i] * r[i].value * r[i].value;
    }
    return vt;
}

int safestep_ok(double v_prev, double v_next, double epsilon) {
    return v_next <= v_prev + epsilon;
}

void simulate_reach(Reach *reach, EcosafetyConfig *cfg, int steps, double dt_s, FILE *out) {
    double c = reach->cin_ngl;
    double lambda_s = 0.01 / 86400.0;
    double vt_prev = 0.0;
    
    fprintf(out, "step,time_s,c_ngl,hlr_mperh,r_hlr,r_cec,r_pfas,vt,safestep\n");
    
    for (int t = 0; t < steps; t++) {
        double tau_s = (reach->area_m2 / (reach->qm3s + 1e-12));
        double dc_dt = ((reach->cin_ngl - c) / tau_s) - lambda_s * c;
        c += dc_dt * dt_s;
        
        reach->hlr_mperh = (reach->qm3s / (reach->area_m2 + 1e-12)) * 3600.0;
        
        RiskCoord r_hlr = normalize_corridor(reach->hlr_mperh, &cfg->hlr_corridor);
        RiskCoord r_cec = normalize_corridor(c, &cfg->cec_corridor);
        RiskCoord r_pfas = normalize_corridor(c * 0.5, &cfg->pfas_corridor);
        
        RiskCoord r[] = {r_hlr, r_cec, r_pfas};
        double vt = compute_lyapunov(r, 3, cfg->weights);
        
        int safe = safestep_ok(vt_prev, vt, 1e-6);
        
        fprintf(out, "%d,%.2f,%.4f,%.4f,%.4f,%.4f,%.4f,%.6f,%d\n",
                t, t * dt_s, c, reach->hlr_mperh, r_hlr.value, r_cec.value, r_pfas.value, vt, safe);
        
        vt_prev = vt;
    }
}

int main() {
    EcosafetyConfig cfg = {
        .hlr_corridor = {0.0, 0.3, 0.6},
        .cec_corridor = {0.0, 30.0, 100.0},
        .pfas_corridor = {0.0, 20.0, 70.0},
        .t90_corridor = {0.0, 90.0, 180.0},
        .weights = {1.0, 1.0, 1.5, 1.2, 1.0}
    };
    
    Reach reach = {
        .qm3s = 0.05,
        .area_m2 = 100.0,
        .cin_ngl = 50.0,
        .hlr_mperh = 0.0,
        .length_m = 500.0,
        .has_sat = 1
    };
    
    FILE *out = fopen("output/cyboquatic_sim_results.csv", "w");
    if (!out) {
        fprintf(stderr, "Error opening output file\n");
        return 1;
    }
    
    simulate_reach(&reach, &cfg, 1000, 3600.0, out);
    
    fclose(out);
    printf("✓ Simulation complete. Results: output/cyboquatic_sim_results.csv\n");
    return 0;
}
