// File: cpp/eco_restoration/hex_spectral_unmixing_consistency.cpp

#include <vector>
#include <string>
#include <cmath>
#include <iostream>

/**
 * Hex-anchor spectral unmixing consistency:
 *
 * Each hex h aggregates mixed pixels of three endmembers:
 *   - vegetation (v)
 *   - impervious (i)
 *   - water (w)
 *
 * Hex-level observed mean indices:
 *   NDVI_h, NDBI_h, NDWI_h.
 *
 * Endmember characteristic indices:
 *   NDVI_v, NDVI_i, NDVI_w;
 *   NDBI_v, NDBI_i, NDBI_w;
 *   NDWI_v, NDWI_i, NDWI_w.[166][172][161][169]
 *
 * We seek fractions f_v, f_i, f_w per hex satisfying:
 *   f_v + f_i + f_w = 1, f_v, f_i, f_w ≥ 0
 *
 * and:
 *   NDVI_h ≈ f_v NDVI_v + f_i NDVI_i + f_w NDVI_w
 *   NDBI_h ≈ f_v NDBI_v + f_i NDBI_i + f_w NDBI_w
 *   NDWI_h ≈ f_v NDWI_v + f_i NDWI_i + f_w NDWI_w
 *
 * This is a constrained linear unmixing problem. We can solve the
 * unconstrained system and then project onto the simplex (non-negative,
 * sum-to-one) to obtain physically plausible fractions.
 *
 * Using these fractions and per-endmember LST contributions, we predict:
 *   LST_pred_h = f_v LST_v + f_i LST_i + f_w LST_w
 *
 * and compare against regression-based LST_h_reg from:
 *   ΔT_h = α V_h + β B_h + γ W_h + δ
 * to validate spectral consistency.
 */

struct EndmemberIndices {
    double ndvi_v;
    double ndvi_i;
    double ndvi_w;
    double ndbi_v;
    double ndbi_i;
    double ndbi_w;
    double ndwi_v;
    double ndwi_i;
    double ndwi_w;
    double lst_v;
    double lst_i;
    double lst_w;
};

struct HexIndices {
    std::string hex_id;
    double ndvi_h;
    double ndbi_h;
    double ndwi_h;
    double lst_h_reg; // from regression model
};

struct Fractions {
    double f_v;
    double f_i;
    double f_w;
};

Fractions solve_unmixing(const HexIndices& hex, const EndmemberIndices& e) {
    // Solve linear system A f ≈ b, then project to simplex.

    // Equations:
    // NDVI_h = f_v NDVI_v + f_i NDVI_i + f_w NDVI_w
    // NDBI_h = f_v NDBI_v + f_i NDBI_i + f_w NDBI_w
    // NDWI_h = f_v NDWI_v + f_i NDWI_i + f_w NDWI_w
    double A[3][3] = {
        {e.ndvi_v, e.ndvi_i, e.ndvi_w},
        {e.ndbi_v, e.ndbi_i, e.ndbi_w},
        {e.ndwi_v, e.ndwi_i, e.ndwi_w}
    };
    double b[3] = {hex.ndvi_h, hex.ndbi_h, hex.ndwi_h};

    // Gaussian elimination.
    for (int k = 0; k < 3; ++k) {
        int pivot = k;
        double max_val = std::fabs(A[k][k]);
        for (int i = k + 1; i < 3; ++i) {
            double val = std::fabs(A[i][k]);
            if (val > max_val) {
                max_val = val;
                pivot = i;
            }
        }
        if (max_val < 1e-10) {
            break;
        }
        if (pivot != k) {
            for (int j = 0; j < 3; ++j) {
                std::swap(A[k][j], A[pivot][j]);
            }
            std::swap(b[k], b[pivot]);
        }
        double diag = A[k][k];
        for (int j = k; j < 3; ++j) {
            A[k][j] /= diag;
        }
        b[k] /= diag;
        for (int i = k + 1; i < 3; ++i) {
            double factor = A[i][k];
            for (int j = k; j < 3; ++j) {
                A[i][j] -= factor * A[k][j];
            }
            b[i] -= factor * b[k];
        }
    }

    double f[3] = {0.0, 0.0, 0.0};
    for (int i = 2; i >= 0; --i) {
        double sum = b[i];
        for (int j = i + 1; j < 3; ++j) {
            sum -= A[i][j] * f[j];
        }
        f[i] = sum;
    }

    // Project to simplex: f_i >= 0, sum f_i = 1.
    for (int i = 0; i < 3; ++i) {
        if (f[i] < 0.0) f[i] = 0.0;
    }
    double sum_f = f[0] + f[1] + f[2];
    if (sum_f <= 0.0) {
        f[0] = 1.0; f[1] = 0.0; f[2] = 0.0;
    } else {
        f[0] /= sum_f;
        f[1] /= sum_f;
        f[2] /= sum_f;
    }

    return {f[0], f[1], f[2]};
}

double predict_lst_from_fractions(const Fractions& f, const EndmemberIndices& e) {
    return f.f_v * e.lst_v + f.f_i * e.lst_i + f.f_w * e.lst_w;
}

int main() {
    EndmemberIndices e{
        /*ndvi_v=*/0.7,  /*ndvi_i=*/0.1,  /*ndvi_w=*/0.2,
        /*ndbi_v=*/-0.3, /*ndbi_i=*/0.6,  /*ndbi_w=*/-0.4,
        /*ndwi_v=*/0.5,  /*ndwi_i=*/-0.2, /*ndwi_w=*/0.8,
        /*lst_v=*/38.0,  /*lst_i=*/45.0,  /*lst_w=*/35.0
    };

    HexIndices hex{
        "hex_10_20",
        /*ndvi_h=*/0.35,
        /*ndbi_h=*/0.25,
        /*ndwi_h=*/0.10,
        /*lst_h_reg=*/42.0
    };

    Fractions f = solve_unmixing(hex, e);
    double lst_pred = predict_lst_from_fractions(f, e);

    std::cout << "Hex " << hex.hex_id << " fractions:\n"
              << "  f_v=" << f.f_v
              << "  f_i=" << f.f_i
              << "  f_w=" << f.f_w << "\n";
    std::cout << "LST_pred_from_unmix=" << lst_pred
              << " | LST_reg=" << hex.lst_h_reg
              << " | diff=" << (lst_pred - hex.lst_h_reg) << "\n";

    return 0;
}
